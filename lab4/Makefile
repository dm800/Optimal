CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
SRC_DIR = src
RESULTS_DIR = results
TARGET = lab4.exe
SRC = $(SRC_DIR)/lab4.cpp

.PHONY: all run graph clean

all: $(TARGET)

$(TARGET): $(SRC) $(SRC_DIR)/frontend.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

graph: run
	dot -Tpng $(RESULTS_DIR)/prog1.dot -o $(RESULTS_DIR)/prog1.png

clean:
	rm -f $(TARGET)
	rm -f $(RESULTS_DIR)/prog1.dot
	rm -f $(RESULTS_DIR)/prog1.png