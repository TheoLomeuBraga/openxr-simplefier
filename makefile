
OS := $(shell uname -s)

core_file = main.cpp

xorg_stuf = -lX11 -DX11

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o run.exe vr_manager.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 `sdl2-config --cflags --libs`
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o run.exe vr_manager.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 `sdl2-config --cflags --libs`
endif
ifeq ($(OS),Linux)
	compile_command = g++ -o run vr_manager.cpp $(core_file) -lGL -lGLEW -lopenxr_loader $(xorg_stuf) `sdl2-config --cflags --libs` 


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
	

