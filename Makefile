CXX = g++
CXXFLAGS = -fPIC --no-gnu-unique -Isrc/ -g `pkg-config --cflags pixman-1 libdrm hyprland hyprlang` -std=c++2b -DWLR_USE_UNSTABLE


SRC_DIR = src
OBJ_DIR = out/obj
OUT_DIR = out

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = $(OUT_DIR)/hypr-darkwindow.so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -shared $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf out

load: unload
	hyprctl plugin load $(shell pwd)/$(TARGET)

unload:
	hyprctl plugin unload $(shell pwd)/$(TARGET)

