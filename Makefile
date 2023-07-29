# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp -o hypr-darkwindow.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./hypr-darkwindow.so
load:
	hyprctl plugin load $(shell pwd)/hypr-darkwindow.so
unload:
	hyprctl plugin unload $(shell pwd)/hypr-darkwindow.so
dev:
	Hyprland/build/Hyprland --config $(shell pwd)/hyprlandd.conf

