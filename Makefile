# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.


all:
	mkdir -p out
	$(CXX) -shared -fPIC --no-gnu-unique src/*.cpp -Isrc/ -o out/hypr-darkwindow.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b -DWLR_USE_UNSTABLE
clean:
	rm -rf out
load: unload
	hyprctl plugin load $(shell pwd)/out/hypr-darkwindow.so
unload:
	hyprctl plugin unload $(shell pwd)/out/hypr-darkwindow.so
setup-dev:
	git clone --recursive https://github.com/hyprwm/Hyprland	
	cd Hyprland && git checkout tags/v0.28.0 && sudo make config && make protocols && make debug
dev:
	Hyprland/build/Hyprland
