# use with make target VERSION=tags/0.31.0
VERSION = HEAD


all:
	mkdir -p out
	$(CXX) -shared -fPIC --no-gnu-unique src/*.cpp -Isrc/ -o out/hypr-darkwindow.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b -DWLR_USE_UNSTABLE

build-version:
	mkdir -p "out/$(VERSION)" 
	$(CXX) -shared -fPIC --no-gnu-unique src/*.cpp -Isrc/ -I"hyprland/$(VERSION)/include/hyprland/protocols" -I"hyprland/$(VERSION)/include/hyprland/wlroots" -I"hyprland/$(VERSION)/include/" -o "out/$(VERSION)/hypr-darkwindow.so" -g `pkg-config --cflags pixman-1 libdrm` -std=c++2b -DWLR_USE_UNSTABLE

clean:
	rm -rf out
	rm -rf hyprland

load: unload
	hyprctl plugin load $(shell pwd)/out/$(VERSION)/hypr-darkwindow.so

unload:
	hyprctl plugin unload $(shell pwd)/out/$(VERSION)/hypr-darkwindow.so

setup-dev:
ifeq ("$(wildcard hyprland/$(VERSION))","")
	mkdir -p "hyprland/$(VERSION)"
	git clone https://github.com/hyprwm/Hyprland "hyprland/$(VERSION)"
	cd "hyprland/$(VERSION)" && git checkout "$(VERSION)" && git submodule update --init
endif
	cd "hyprland/$(VERSION)" && make debug && make installheaders PREFIX="."

setup-headers:
ifeq ("$(wildcard hyprland/$(VERSION))","")
	mkdir -p "hyprland/$(VERSION)"
	git clone https://github.com/hyprwm/Hyprland "hyprland/$(VERSION)"
	cd "hyprland/$(VERSION)" && git checkout "$(VERSION)" && git submodule update --init
endif
	cd "hyprland/$(VERSION)" && make all && make installheaders PREFIX="."

dev:
	hyprland/$(VERSION)/build/Hyprland
