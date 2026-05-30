#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

const std::string ALLOCA = "alloca";
const std::string LOAD = "load";
const std::string STORE = "store";
const std::string BR = "br";
const std::string CONDBR = "condbr";
const std::string ICMP = "icmp";
const std::string MUL = "mul";
const std::string ADD = "add";
const std::string SUB = "sub";
const std::string RET = "ret";
const std::string PHI = "phi";

struct Variable {
    std::string name;
    int version;
    bool is_temp;

    Variable() : name(""), version(0), is_temp(false) {}

    Variable(std::string name, int version) : name(std::move(name)), version(version), is_temp(false) {}

    std::string str() const {
        if (is_temp) {
            return name;
        }
        return name + "(" + std::to_string(version) + ")";
    }

    bool operator<(const Variable& other) const {
        if (name != other.name) {
            return name < other.name;
        }
        if (version != other.version) {
            return version < other.version;
        }
        return is_temp < other.is_temp;
    }
};

struct Value {
    enum class Kind {
        Empty,
        Int,
        Text,
        Var
    };

    Kind kind;
    int int_value;
    std::string text_value;
    Variable var_value;

    Value() : kind(Kind::Empty), int_value(0) {}

    Value(int value) : kind(Kind::Int), int_value(value) {}

    Value(const char* value) : kind(Kind::Text), int_value(0), text_value(value) {}

    Value(std::string value) : kind(Kind::Text), int_value(0), text_value(std::move(value)) {}

    Value(Variable value) : kind(Kind::Var), int_value(0), var_value(std::move(value)) {}

    bool isVar() const {
        return kind == Kind::Var;
    }

    bool isTempVar() const {
        return isVar() && var_value.is_temp;
    }

    std::string str() const {
        if (kind == Kind::Int) {
            return std::to_string(int_value);
        }
        if (kind == Kind::Text) {
            return text_value;
        }
        if (kind == Kind::Var) {
            return var_value.str();
        }
        return "";
    }
};

struct Instruction {
    std::string typ;
    std::map<std::string, Value> args;
    std::vector<Value> phi_from;

    Instruction() = default;

    Instruction(std::string typ, std::map<std::string, Value> args) : typ(std::move(typ)), args(std::move(args)) {}

    Instruction(std::string typ, std::map<std::string, Value> args, std::vector<Value> phi_from)
        : typ(std::move(typ)), args(std::move(args)), phi_from(std::move(phi_from)) {}

    std::string str() const {
        if (typ == STORE) {
            return args.at("to").str() + " <- " + args.at("from").str();
        }
        if (typ == LOAD) {
            return args.at("to").str() + " <- " + args.at("from").str();
        }
        if (typ == SUB || typ == ADD || typ == MUL) {
            return args.at("to").str() + " <- " + args.at("oper1").str() + " " + typ + " " + args.at("oper2").str();
        }
        if (typ == BR) {
            return "go to BLOCK" + args.at("dest").str();
        }
        if (typ == CONDBR) {
            return "if (" + args.at("cond").str() + ") go to BLOCK" + args.at("dest1").str() +
                   " else go to BLOCK" + args.at("dest2").str();
        }
        if (typ == ICMP) {
            return args.at("to").str() + " <- " + args.at("arg1").str() + " > " + args.at("arg2").str();
        }
        if (typ == PHI) {
            std::ostringstream out;
            out << args.at("to").str() << " = phi(";
            for (std::size_t i = 0; i < phi_from.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << phi_from[i].str();
            }
            out << ")";
            return out.str();
        }
        if (typ == ALLOCA) {
            return "new variable " + args.at("name").str();
        }

        std::ostringstream out;
        out << " " << typ << ": ";
        for (const auto& [key, value] : args) {
            out << key << " " << value.str() << " ";
        }
        return out.str();
    }
};

struct BB {
    int block_num;
    std::vector<Instruction> instructions;
    bool returned;
    std::map<std::string, Variable> variables;
    int varcounter;
    std::set<std::string> changing_variables;
    std::map<std::string, std::set<int>> phi_var_blocks;

    BB() : block_num(0), returned(false), varcounter(0) {}

    std::string str() const {
        std::ostringstream out;
        out << "BLOCK " << block_num << "{\n";
        for (const auto& instruction : instructions) {
            out << instruction.str() << "\n";
        }
        out << "}\n";
        return out.str();
    }

    void print() const {
        std::cout << str();
    }

    void add_instr(const Instruction& instr) {
        if (returned) {
            return;
        }
        instructions.push_back(instr);
    }

    Variable alloca_variable(const std::string& name) {
        Variable newvar(name, 0);
        variables[name] = newvar;
        add_instr(Instruction(ALLOCA, {{"name", Value(name)}}));
        return newvar;
    }

    Variable create_tmp_var() {
        Variable var("tmp_" + std::to_string(block_num) + "_" + std::to_string(varcounter), 0);
        ++varcounter;
        var.is_temp = true;
        return var;
    }

    void new_break(const BB& dest) {
        add_instr(Instruction(BR, {{"dest", Value(dest.block_num)}}));
    }

    void new_ret(const Value& val) {
        if (val.kind == Value::Kind::Int) {
            add_instr(Instruction(RET, {{"value", val}}));
        } else if (val.kind == Value::Kind::Var) {
            Variable tmpvar = create_tmp_var();
            add_instr(Instruction(LOAD, {{"from", val}, {"to", Value(tmpvar)}}));
            add_instr(Instruction(RET, {{"value", Value(tmpvar)}}));
        }
    }

    void new_cond_break(const Variable& cond, const BB& dest1, const BB& dest2) {
        add_instr(Instruction(CONDBR, {{"cond", Value(cond)}, {"dest1", Value(dest1.block_num)}, {"dest2", Value(dest2.block_num)}}));
    }

    Variable new_compare(const Value& arg1, const Value& arg2) {
        Variable tmp = create_tmp_var();
        add_instr(Instruction(ICMP, {{"arg1", arg1}, {"arg2", arg2}, {"to", Value(tmp)}}));
        return tmp;
    }

    bool is_variable_in(const std::string& name) const {
        return variables.find(name) != variables.end();
    }

    void set_variable(const std::string& name, const Value& val) {
        if (val.kind == Value::Kind::Var) {
            Variable tmpvar = create_tmp_var();
            add_instr(Instruction(LOAD, {{"from", val}, {"to", Value(tmpvar)}}));
            add_instr(Instruction(STORE, {{"from", Value(tmpvar)}, {"to", Value(variables[name])}}));
        } else if (val.kind == Value::Kind::Int) {
            add_instr(Instruction(STORE, {{"from", val}, {"to", Value(variables[name])}}));
        }
    }

    void set_map(const BB& parent) {
        for (const auto& [name, variable] : parent.variables) {
            variables[name] = variable;
        }
    }

    std::set<std::pair<int, int>> get_edges() const {
        if (instructions.empty()) {
            return {};
        }

        const Instruction& last = instructions.back();
        if (last.typ == BR) {
            return {{block_num, last.args.at("dest").int_value}};
        }
        if (last.typ == CONDBR) {
            return {{block_num, last.args.at("dest1").int_value}, {block_num, last.args.at("dest2").int_value}};
        }
        return {};
    }

    void build_changing_variables() {
        std::set<std::string> s;
        for (const auto& instruction : instructions) {
            if (instruction.typ == STORE) {
                const Value& to = instruction.args.at("to");
                if (to.kind == Value::Kind::Var) {
                    s.insert(to.var_value.name);
                }
            }
        }
        changing_variables = s;
        phi_var_blocks.clear();
    }
};

struct SsaBuilder {
    std::vector<BB> blocks;
    std::set<int> CFG;
    std::map<int, std::set<int>> succ;
    std::map<int, std::set<int>> pred;
    std::map<int, std::set<int>> dom_of;
    std::map<int, std::set<int>> children;
    std::map<int, std::set<int>> df;
    std::vector<int> stack;
    int counter;

    explicit SsaBuilder(std::vector<BB> blocks) : blocks(std::move(blocks)), counter(0) {
        build_dom();
        build_df();
        build_changed_variables();
    }

    BB& get_block(int n) {
        for (BB& bb : blocks) {
            if (bb.block_num == n) {
                return bb;
            }
        }
        throw std::runtime_error("block not found");
    }

    const BB& get_block(int n) const {
        for (const BB& bb : blocks) {
            if (bb.block_num == n) {
                return bb;
            }
        }
        throw std::runtime_error("block not found");
    }

    std::set<int> blocks_to_nums(const std::vector<BB>& source) const {
        std::set<int> result;
        for (const BB& bb : source) {
            result.insert(bb.block_num);
        }
        return result;
    }

    std::vector<BB*> nums_to_blocks(const std::set<int>& nums) {
        std::vector<BB*> result;
        for (BB& bb : blocks) {
            if (nums.find(bb.block_num) != nums.end()) {
                result.push_back(&bb);
            }
        }
        return result;
    }

    std::set<int> get_succ(int node) const {
        auto it = succ.find(node);
        if (it == succ.end()) {
            return {};
        }
        return it->second;
    }

    std::set<int> get_preds(int node) const {
        auto it = pred.find(node);
        if (it == pred.end()) {
            return {};
        }
        return it->second;
    }

    static std::set<int> set_intersection_copy(std::set<int> a, const std::set<int>& b) {
        for (auto it = a.begin(); it != a.end();) {
            if (b.find(*it) == b.end()) {
                it = a.erase(it);
            } else {
                ++it;
            }
        }
        return a;
    }

    static bool strict_dominates(int x, int y, const std::map<int, std::set<int>>& doms) {
        auto it = doms.find(y);
        if (it == doms.end()) {
            return false;
        }
        return x != y && it->second.find(x) != it->second.end();
    }

    void build_dom() {
        std::map<int, std::set<int>> undirected;

        for (const BB& bb : blocks) {
            CFG.insert(bb.block_num);
            succ[bb.block_num];
            pred[bb.block_num];
            for (const auto& edge : bb.get_edges()) {
                succ[edge.first].insert(edge.second);
                pred[edge.second].insert(edge.first);
                undirected[edge.first].insert(edge.second);
                undirected[edge.second].insert(edge.first);
                CFG.insert(edge.first);
                CFG.insert(edge.second);
            }
        }

        std::set<int> connected;
        std::queue<int> q;
        q.push(0);
        connected.insert(0);
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            for (int to : undirected[v]) {
                if (connected.insert(to).second) {
                    q.push(to);
                }
            }
        }
        CFG = connected;

        blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [&](const BB& bb) {
            return CFG.find(bb.block_num) == CFG.end();
        }), blocks.end());

        for (auto it = succ.begin(); it != succ.end();) {
            if (CFG.find(it->first) == CFG.end()) {
                it = succ.erase(it);
            } else {
                for (auto jt = it->second.begin(); jt != it->second.end();) {
                    if (CFG.find(*jt) == CFG.end()) {
                        jt = it->second.erase(jt);
                    } else {
                        ++jt;
                    }
                }
                ++it;
            }
        }

        pred.clear();
        for (int node : CFG) {
            pred[node];
        }
        for (const auto& [from, tos] : succ) {
            for (int to : tos) {
                pred[to].insert(from);
            }
        }

        std::map<int, std::set<int>> doms;
        for (int node : CFG) {
            if (node == 0) {
                doms[node] = {0};
            } else {
                doms[node] = CFG;
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (int node : CFG) {
                if (node == 0) {
                    continue;
                }

                std::set<int> new_dom = CFG;
                if (pred[node].empty()) {
                    new_dom.clear();
                }
                for (int p : pred[node]) {
                    new_dom = set_intersection_copy(new_dom, doms[p]);
                }
                new_dom.insert(node);

                if (new_dom != doms[node]) {
                    doms[node] = new_dom;
                    changed = true;
                }
            }
        }

        std::map<int, int> imm_dom;
        imm_dom[0] = 0;
        for (int node : CFG) {
            if (node == 0) {
                continue;
            }

            std::set<int> candidates = doms[node];
            candidates.erase(node);
            int idom = -1;
            for (int candidate : candidates) {
                bool is_immediate = true;
                for (int other : candidates) {
                    if (other == candidate) {
                        continue;
                    }
                    if (strict_dominates(candidate, other, doms)) {
                        is_immediate = false;
                        break;
                    }
                }
                if (is_immediate) {
                    idom = candidate;
                    break;
                }
            }
            imm_dom[node] = idom;
        }

        dom_of.clear();
        children.clear();
        for (int node : CFG) {
            children[node] = {};
            dom_of[node] = {imm_dom[node]};
        }
        for (const auto& [node, ys] : dom_of) {
            for (int y : ys) {
                if (y != node) {
                    children[y].insert(node);
                }
            }
        }
    }

    void build_df() {
        df.clear();
        for (int bb : CFG) {
            df[bb] = {};
        }

        std::map<int, std::set<int>> old;
        while (old != df) {
            old = df;
            for (int x : blocks_to_nums(blocks)) {
                for (int y : get_succ(x)) {
                    if (dom_of[y].find(x) == dom_of[y].end()) {
                        df[x].insert(y);
                    }
                }

                for (int z : children[x]) {
                    for (int y : df[z]) {
                        if (children[x].find(y) == children[x].end()) {
                            df[x].insert(y);
                        }
                    }
                }
            }
        }
    }

    void build_changed_variables() {
        for (BB& bb : blocks) {
            bb.build_changing_variables();
        }
    }

    void print_blocks() const {
        for (const BB& bb : blocks) {
            bb.print();
        }
    }

    std::string to_graph() const {
        std::ostringstream ret;
        ret << "digraph G{\nnode [shape=box nojustify=false]\n";

        for (const BB& x : blocks) {
            std::string s = x.str();
            s.erase(std::remove(s.begin(), s.end(), '{'), s.end());
            s.erase(std::remove(s.begin(), s.end(), '}'), s.end());

            std::string replaced;
            for (char ch : s) {
                if (ch == '\n') {
                    replaced += "\\l ";
                } else {
                    replaced += ch;
                }
            }

            while (!replaced.empty() && std::isspace(static_cast<unsigned char>(replaced.back()))) {
                replaced.pop_back();
            }
            while (replaced.size() >= 2 && replaced.substr(replaced.size() - 2) == "\\l") {
                replaced.erase(replaced.size() - 2);
                while (!replaced.empty() && std::isspace(static_cast<unsigned char>(replaced.back()))) {
                    replaced.pop_back();
                }
            }

            ret << x.block_num << " [label=\"" << replaced << "\"]\n";
            const Instruction& last = x.instructions.back();
            if (last.typ == BR) {
                ret << x.block_num << " -> " << last.args.at("dest").int_value << "\n";
            } else if (last.typ == CONDBR) {
                ret << x.block_num << " -> " << last.args.at("dest1").int_value << " [label=true]\n";
                ret << x.block_num << " -> " << last.args.at("dest2").int_value << " [label=false]\n";
            }
        }

        ret << "}\n";
        return ret.str();
    }

    std::set<std::string> get_all_vars_names() const {
        std::set<std::string> vars;
        for (const BB& bb : blocks) {
            for (const auto& [name, variable] : bb.variables) {
                vars.insert(name);
            }
        }
        return vars;
    }

    std::set<int> find_blocks_that_redefine_var(const std::string& varname) const {
        std::set<int> s;
        for (const BB& bb : blocks) {
            if (bb.changing_variables.find(varname) != bb.changing_variables.end()) {
                s.insert(bb.block_num);
            }
        }
        return s;
    }

    std::set<int> find_df(const std::set<int>& s) const {
        std::set<int> ret;
        for (int x : s) {
            auto it = df.find(x);
            if (it != df.end()) {
                ret.insert(it->second.begin(), it->second.end());
            }
        }
        return ret;
    }

    std::set<int> find_df_post_order(const std::set<int>& s) const {
        std::set<int> old;
        std::set<int> current = find_df(s);
        while (true) {
            old.insert(current.begin(), current.end());
            current = find_df(current);

            std::set<int> difference;
            std::set_difference(current.begin(), current.end(), old.begin(), old.end(), std::inserter(difference, difference.begin()));
            if (difference.empty()) {
                return old;
            }
        }
    }

    std::set<int> find_post_order(const std::set<int>& s) const {
        return find_df_post_order(s);
    }

    void insert_phi(const std::string& varname) {
        std::set<int> stored_in_blocks_num = find_blocks_that_redefine_var(varname);
        std::set<int> post_order_blocks_num = find_post_order(stored_in_blocks_num);

        for (int bb_num : post_order_blocks_num) {
            BB& bb = get_block(bb_num);
            bb.phi_var_blocks[varname] = {};
            for (int p : get_preds(bb.block_num)) {
                bb.phi_var_blocks[varname].insert(p);
            }
        }
    }

    void insert_all_phi() {
        std::set<std::string> vars = get_all_vars_names();
        for (const std::string& varname : vars) {
            insert_phi(varname);
        }

        for (BB& bb : blocks) {
            for (const auto& [varname, phiblocks] : bb.phi_var_blocks) {
                std::vector<Value> from;
                for (int block_num : phiblocks) {
                    from.emplace_back(block_num);
                }
                Instruction instr(PHI, {{"to", Value(Variable(varname, 0))}}, from);
                bb.instructions.insert(bb.instructions.begin(), instr);
            }
        }
    }

    void update_variable_versions() {
        traverse();
    }

    void traverse() {
        std::set<std::string> vars = get_all_vars_names();
        for (const std::string& target_var : vars) {
            stack.clear();
            counter = 0;
            traverse_rec(0, target_var);
        }
    }

    int which_pred(int v, int v1) const {
        std::vector<int> preds;
        std::set<int> pred_set = get_preds(v1);
        preds.insert(preds.end(), pred_set.begin(), pred_set.end());
        std::sort(preds.begin(), preds.end());
        auto it = std::find(preds.begin(), preds.end(), v);
        return static_cast<int>(std::distance(preds.begin(), it));
    }

    void traverse_rec(int bb, const std::string& target_var) {

        BB& block = get_block(bb);
        for (Instruction& instr : block.instructions) {
            for (auto& [key, val] : instr.args) {
                if (!val.isVar() || val.isTempVar() || val.var_value.name != target_var) {
                    continue;
                }

                std::string name = val.var_value.name;

                if (instr.typ == STORE || instr.typ == PHI) {
                    int new_ver = counter;
                    stack.push_back(counter);
                    ++counter;
                    instr.args["to"] = Value(Variable(name, new_ver));
                }

                if (instr.typ != PHI) {
                    instr.args[key] = Value(Variable(name, stack.back()));
                }
            }
        }

        std::set<int> successors = get_succ(bb);
        for (int v1 : successors) {
            int j = which_pred(bb, v1);
            BB& succ_block = get_block(v1);
            for (Instruction& instr : succ_block.instructions) {
                if (instr.typ != PHI || instr.args["to"].var_value.name != target_var) {
                    continue;
                }
                if (j >= 0 && j < static_cast<int>(instr.phi_from.size())) {
                    instr.phi_from[j] = Value(Variable(target_var, stack.back()));
                }
            }
        }

        for (int v1 : children[bb]) {
            traverse_rec(v1, target_var);
        }

        for (const Instruction& instr : get_block(bb).instructions) {
            if ((instr.typ == STORE || instr.typ == PHI) && instr.args.at("to").var_value.name == target_var) {
                stack.pop_back();
            }
        }
    }
};

#include "frontend.hpp"


int main() {
    std::ifstream input("program.txt");

    std::vector<BB> blocks = frontend::parse_to_blocks(input);

    SsaBuilder ssab(blocks);
    ssab.insert_all_phi();
    ssab.update_variable_versions();
    ssab.print_blocks();

    std::filesystem::create_directories("results");
    std::ofstream f("results/prog1.dot");
    f << ssab.to_graph();

    return 0;
}
