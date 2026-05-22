BUILD_DIR=build
TARGET=lab3
INPUT=input.txt

all:
        mkdir -p $(BUILD_DIR)
        cd $(BUILD_DIR) && cmake .. && make

run: all
        ./$(BUILD_DIR)/$(TARGET) < $(INPUT)

clean:
        rm -rf $(BUILD_DIR)