CXX = g++
CXXFLAGS = -fPIC --no-gnu-unique -Isrc/ -g `pkg-config --cflags pixman-1 libdrm hyprland hyprlang` -std=c++2b -DWLR_USE_UNSTABLE


SRC_DIR = src
OBJ_DIR = out/obj
OUT_DIR = out

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.d, $(SRCS))

TARGET = $(OUT_DIR)/hypr-darkwindow.so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -shared $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR)/%.d | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MT $@ -MMD -MP -MF $(OBJ_DIR)/$*.d -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

$(DEPS):

include $(wildcard $(DEPS))

clean:
	rm -rf $(OUT_DIR)

load: unload
	hyprctl plugin load $(shell pwd)/$(TARGET)

unload:
	hyprctl plugin unload $(shell pwd)/$(TARGET)

