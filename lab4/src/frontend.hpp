#pragma once

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "frontend.hpp"

namespace frontend {

enum Token {
    TOK_EOF = 1,
    TOK_IDENT = 2,
    TOK_NUM = 3,
    TOK_IF = 6,
    TOK_ELSE = 7,
    TOK_THEN = 8,
    TOK_WHILE = 9,
    TOK_DO = 10,
    TOK_END = 11,
    TOK_RETURN = 12
};

class Lexer {
private:
    std::istream& in;
    int last_char;

public:
    std::string identifier;
    int number;

    explicit Lexer(std::istream& input) : in(input), last_char(' '), number(0) {}

    int get_token() {
        while (std::isspace(last_char)) {
            last_char = in.get();
        }

        if (std::isalpha(last_char)) {
            identifier.clear();
            do {
                identifier += static_cast<char>(last_char);
                last_char = in.get();
            } while (std::isalnum(last_char) || last_char == '_');

            if (identifier == "if") {
                return TOK_IF;
            }
            if (identifier == "else") {
                return TOK_ELSE;
            }
            if (identifier == "then") {
                return TOK_THEN;
            }
            if (identifier == "while") {
                return TOK_WHILE;
            }
            if (identifier == "do") {
                return TOK_DO;
            }
            if (identifier == "end") {
                return TOK_END;
            }
            if (identifier == "return") {
                return TOK_RETURN;
            }
            return TOK_IDENT;
        }

        if (std::isdigit(last_char)) {
            std::string num_str;
            do {
                num_str += static_cast<char>(last_char);
                last_char = in.get();
            } while (std::isdigit(last_char));

            number = std::atoi(num_str.c_str());
            return TOK_NUM;
        }

        if (last_char == '#') {
            do {
                last_char = in.get();
            } while (last_char != EOF && last_char != '\n' && last_char != '\r');
            return get_token();
        }

        if (last_char == EOF || last_char == std::char_traits<char>::eof()) {
            return TOK_EOF;
        }

        int this_char = last_char;
        last_char = in.get();
        return this_char;
    }
};

struct ExprAST {
    virtual ~ExprAST() = default;
};

struct NumberExprAST : ExprAST {
    int value;
    explicit NumberExprAST(int value) : value(value) {}
};

struct VariableExprAST : ExprAST {
    std::string name;
    explicit VariableExprAST(std::string name) : name(std::move(name)) {}
};

struct AssignAST : ExprAST {
    std::string name;
    std::unique_ptr<ExprAST> expr;

    AssignAST(std::string name, std::unique_ptr<ExprAST> expr)
        : name(std::move(name)), expr(std::move(expr)) {}
};

struct BinaryExprAST : ExprAST {
    char op;
    std::unique_ptr<ExprAST> lhs;
    std::unique_ptr<ExprAST> rhs;

    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

struct IfExprAST : ExprAST {
    std::unique_ptr<ExprAST> cond;
    std::vector<std::unique_ptr<ExprAST>> then_body;
    std::vector<std::unique_ptr<ExprAST>> else_body;

    IfExprAST(std::unique_ptr<ExprAST> cond,
              std::vector<std::unique_ptr<ExprAST>> then_body,
              std::vector<std::unique_ptr<ExprAST>> else_body)
        : cond(std::move(cond)), then_body(std::move(then_body)), else_body(std::move(else_body)) {}
};

struct WhileExprAST : ExprAST {
    std::unique_ptr<ExprAST> cond;
    std::vector<std::unique_ptr<ExprAST>> body;

    WhileExprAST(std::unique_ptr<ExprAST> cond, std::vector<std::unique_ptr<ExprAST>> body)
        : cond(std::move(cond)), body(std::move(body)) {}
};

struct ReturnExprAST : ExprAST {
    std::unique_ptr<ExprAST> expr;
    explicit ReturnExprAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}
};

class Parser {
private:
    Lexer lexer;
    int current_token;

    void next_token() {
        current_token = lexer.get_token();
    }

    void require(int token, const std::string& message) {
        if (current_token != token) {
            throw std::runtime_error(message);
        }
        next_token();
    }

    int get_precedence() const {
        switch (current_token) {
            case '>':
                return 10;
            case '+':
            case '-':
                return 20;
            case '*':
                return 40;
            default:
                return -1;
        }
    }

    std::unique_ptr<ExprAST> parse_number() {
        auto result = std::make_unique<NumberExprAST>(lexer.number);
        next_token();
        return result;
    }

    std::unique_ptr<ExprAST> parse_paren() {
        next_token();
        auto result = parse_expression();
        require(')', "expected ')' after expression");
        return result;
    }

    std::unique_ptr<ExprAST> parse_variable_or_assign() {
        std::string name = lexer.identifier;
        next_token();

        if (current_token != '=') {
            return std::make_unique<VariableExprAST>(name);
        }

        next_token();
        return std::make_unique<AssignAST>(name, parse_expression());
    }

    std::unique_ptr<ExprAST> parse_if() {
        next_token();
        require('(', "expected '(' after if");
        auto cond = parse_expression();
        require(')', "expected ')' after if condition");
        require(TOK_THEN, "expected then after if condition");

        auto then_body = parse_statement_list({TOK_ELSE});
        require(TOK_ELSE, "expected else after then branch");
        auto else_body = parse_statement_list({TOK_END});
        require(TOK_END, "expected end after else branch");

        return std::make_unique<IfExprAST>(std::move(cond), std::move(then_body), std::move(else_body));
    }

    std::unique_ptr<ExprAST> parse_while() {
        next_token();
        require('(', "expected '(' after while");
        auto cond = parse_expression();
        require(')', "expected ')' after while condition");
        require(TOK_DO, "expected do after while condition");

        auto body = parse_statement_list({TOK_END});
        require(TOK_END, "expected end after while body");

        return std::make_unique<WhileExprAST>(std::move(cond), std::move(body));
    }

    std::unique_ptr<ExprAST> parse_return() {
        next_token();
        return std::make_unique<ReturnExprAST>(parse_expression());
    }

    std::unique_ptr<ExprAST> parse_primary() {
        switch (current_token) {
            case TOK_IDENT:
                return parse_variable_or_assign();
            case TOK_NUM:
                return parse_number();
            case '(':
                return parse_paren();
            case '-': {
                next_token();
                auto rhs = parse_primary();
                return std::make_unique<BinaryExprAST>('-', std::make_unique<NumberExprAST>(0), std::move(rhs));
            }
            case TOK_IF:
                return parse_if();
            case TOK_WHILE:
                return parse_while();
            case TOK_RETURN:
                return parse_return();
            default:
                throw std::runtime_error("unknown token when expecting an expression: " + std::to_string(current_token));
        }
    }

    std::unique_ptr<ExprAST> parse_bin_rhs(int expr_prec, std::unique_ptr<ExprAST> lhs) {
        while (true) {
            int tok_prec = get_precedence();
            if (tok_prec < expr_prec) {
                return lhs;
            }

            char bin_op = static_cast<char>(current_token);
            next_token();

            auto rhs = parse_primary();
            int next_prec = get_precedence();
            if (tok_prec < next_prec) {
                rhs = parse_bin_rhs(tok_prec + 1, std::move(rhs));
            }

            lhs = std::make_unique<BinaryExprAST>(bin_op, std::move(lhs), std::move(rhs));
        }
    }

    std::unique_ptr<ExprAST> parse_expression() {
        auto lhs = parse_primary();
        return parse_bin_rhs(0, std::move(lhs));
    }

    bool is_stop_token(const std::set<int>& stop_tokens) const {
        return stop_tokens.find(current_token) != stop_tokens.end();
    }

    std::vector<std::unique_ptr<ExprAST>> parse_statement_list(const std::set<int>& stop_tokens) {
        std::vector<std::unique_ptr<ExprAST>> statements;

        while (current_token != TOK_EOF && !is_stop_token(stop_tokens)) {
            if (current_token == ';') {
                next_token();
                continue;
            }

            statements.push_back(parse_expression());

            if (current_token == ';') {
                next_token();
            }
        }

        return statements;
    }

public:
    explicit Parser(std::istream& input) : lexer(input), current_token(TOK_EOF) {}

    std::vector<std::unique_ptr<ExprAST>> parse_program() {
        next_token();
        return parse_statement_list({TOK_EOF});
    }
};

class IRGenerator {
private:
    std::vector<BB> blocks;
    int current_block;
    int next_block_num;

    BB& block() {
        return blocks[current_block];
    }

    BB& block(int id) {
        for (BB& bb : blocks) {
            if (bb.block_num == id) {
                return bb;
            }
        }
        throw std::runtime_error("block not found");
    }

    int new_block(const std::map<std::string, Variable>& variables) {
        BB bb;
        bb.block_num = next_block_num++;
        bb.variables = variables;
        blocks.push_back(bb);
        return static_cast<int>(blocks.size()) - 1;
    }

    void ensure_variable(const std::string& name) {
        if (!block().is_variable_in(name)) {
            block().variables[name] = Variable(name, 0);
        }
    }

    Value emit_expr(const ExprAST& expr) {
        if (const auto* number = dynamic_cast<const NumberExprAST*>(&expr)) {
            return Value(number->value);
        }

        if (const auto* variable = dynamic_cast<const VariableExprAST*>(&expr)) {
            ensure_variable(variable->name);
            return Value(block().variables[variable->name]);
        }

        if (const auto* assign = dynamic_cast<const AssignAST*>(&expr)) {
            ensure_variable(assign->name);
            Value rhs = emit_expr(*assign->expr);
            block().set_variable(assign->name, rhs);
            return Value(block().variables[assign->name]);
        }

        if (const auto* binary = dynamic_cast<const BinaryExprAST*>(&expr)) {
            Value lhs = emit_expr(*binary->lhs);
            Value rhs = emit_expr(*binary->rhs);

            if (binary->op == '>') {
                return Value(block().new_compare(lhs, rhs));
            }

            Variable tmp = block().create_tmp_var();
            std::string op;
            if (binary->op == '+') {
                op = ADD;
            } else if (binary->op == '-') {
                op = SUB;
            } else if (binary->op == '*') {
                op = MUL;
            } else {
                throw std::runtime_error("unsupported binary operator");
            }

            block().add_instr(Instruction(op, {{"oper1", lhs}, {"oper2", rhs}, {"to", Value(tmp)}}));
            return Value(tmp);
        }

        if (const auto* if_expr = dynamic_cast<const IfExprAST*>(&expr)) {
            emit_if(*if_expr);
            return Value(0);
        }

        if (const auto* while_expr = dynamic_cast<const WhileExprAST*>(&expr)) {
            emit_while(*while_expr);
            return Value(0);
        }

        if (const auto* ret = dynamic_cast<const ReturnExprAST*>(&expr)) {
            Value value = emit_expr(*ret->expr);
            block().new_ret(value);
            block().returned = true;
            return value;
        }

        throw std::runtime_error("unknown AST node");
    }

    void emit_statement_list(const std::vector<std::unique_ptr<ExprAST>>& statements) {
        for (const auto& statement : statements) {
            if (!block().returned) {
                emit_expr(*statement);
            }
        }
    }

    void merge_variable_maps(int target_idx, const std::map<std::string, Variable>& left, const std::map<std::string, Variable>& right) {
        std::map<std::string, Variable> merged = left;
        for (const auto& [name, variable] : right) {
            merged[name] = variable;
        }
        blocks[target_idx].variables = merged;
    }

    void emit_if(const IfExprAST& expr) {
        Value cond_value = emit_expr(*expr.cond);
        if (cond_value.kind != Value::Kind::Var) {
            Variable tmp = block().create_tmp_var();
            block().add_instr(Instruction(ICMP, {{"arg1", cond_value}, {"arg2", Value(0)}, {"to", Value(tmp)}}));
            cond_value = Value(tmp);
        }

        std::map<std::string, Variable> parent_vars = block().variables;
        int then_idx = new_block(parent_vars);
        int else_idx = new_block(parent_vars);
        int merge_idx = new_block(parent_vars);

        block().new_cond_break(cond_value.var_value, blocks[then_idx], blocks[else_idx]);

        current_block = then_idx;
        emit_statement_list(expr.then_body);
        std::map<std::string, Variable> then_vars = block().variables;
        if (!block().returned) {
            block().new_break(blocks[merge_idx]);
        }

        current_block = else_idx;
        emit_statement_list(expr.else_body);
        std::map<std::string, Variable> else_vars = block().variables;
        if (!block().returned) {
            block().new_break(blocks[merge_idx]);
        }

        merge_variable_maps(merge_idx, then_vars, else_vars);
        current_block = merge_idx;
    }

    void emit_while(const WhileExprAST& expr) {
        std::map<std::string, Variable> parent_vars = block().variables;
        int cond_idx = new_block(parent_vars);
        int body_idx = new_block(parent_vars);
        int after_idx = new_block(parent_vars);

        block().new_break(blocks[cond_idx]);

        current_block = cond_idx;
        Value cond_value = emit_expr(*expr.cond);
        if (cond_value.kind != Value::Kind::Var) {
            Variable tmp = block().create_tmp_var();
            block().add_instr(Instruction(ICMP, {{"arg1", cond_value}, {"arg2", Value(0)}, {"to", Value(tmp)}}));
            cond_value = Value(tmp);
        }
        block().new_cond_break(cond_value.var_value, blocks[body_idx], blocks[after_idx]);

        current_block = body_idx;
        emit_statement_list(expr.body);
        std::map<std::string, Variable> body_vars = block().variables;
        if (!block().returned) {
            block().new_break(blocks[cond_idx]);
        }

        merge_variable_maps(cond_idx, parent_vars, body_vars);
        merge_variable_maps(after_idx, parent_vars, body_vars);
        current_block = after_idx;
    }

public:
    IRGenerator() : current_block(0), next_block_num(0) {
        new_block({});
    }

    std::vector<BB> generate(const std::vector<std::unique_ptr<ExprAST>>& statements) {
        emit_statement_list(statements);
        return blocks;
    }
};

inline std::vector<BB> parse_to_blocks(std::istream& input) {
    Parser parser(input);
    auto program = parser.parse_program();
    IRGenerator generator;
    return generator.generate(program);
}

inline std::vector<BB> parse_to_blocks(const std::string& source) {
    std::istringstream input(source);
    return parse_to_blocks(input);
}

} // namespace frontend
