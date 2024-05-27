
OS := $(shell uname -s)

core_file = main.cpp

xorg_stuf = -lX11 -DX11
wayland_stuf = -lwayland-egl -lwayland-client  -lEGL -lGLESv2 -DWAYLAND

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o run.exe glimpl.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 `sdl2-config --cflags --libs`
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o run.exe glimpl.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 `sdl2-config --cflags --libs`
endif
ifeq ($(OS),Linux)
	compile_command = g++ -o run glimpl.cpp $(core_file) -lGL -lGLEW -lopenxr_loader $(xorg_stuf) -lX11 `sdl2-config --cflags --libs` 
	#compile_command = g++ -g -o run glimpl.cpp $(core_file) -lGL -lGLEW -lopenxr_loader  $(wayland_stuf)  -lEGL `sdl2-config --cflags --libs` 

	#SDL_VIDEODRIVER=wayland ./run

endif

all: 
	$(compile_command)

make_command_name = 
ifeq ($(OS),Windows_NT)
	make_command_name = mingw32-make
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	make_command_name = mingw32-make
endif
ifeq ($(OS),Linux)
	make_command_name = make
endif

clear:
	rm -f run run.exe
	

