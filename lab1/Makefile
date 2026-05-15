PLUGIN = lab1.so
SRC = src/main.cpp
TEST = test/main.c

build:
        g++ -std=c++14 \
        -fPIC \
        -fno-rtti \
        -shared \
        -I$(shell gcc -print-file-name=plugin)/include \
        $(SRC) \
        -o $(PLUGIN)

test: build
        gcc -fplugin=./$(PLUGIN) $(TEST)