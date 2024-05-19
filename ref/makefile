
OS := $(shell uname -s)

core_file = main.cpp

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o run.exe glimpl.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32 -lopenvr_api `sdl2-config --cflags --libs`
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o run.exe glimpl.cpp $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32 -lopenvr_api `sdl2-config --cflags --libs`
endif
ifeq ($(OS),Linux)
	#compile_command = g++ -o run glimpl.cpp $(core_file) -lGL -lGLEW -lglfw -lopenxr_loader  -lopenvr_api -lX11 `sdl2-config --cflags --libs` -DX11
	compile_command = g++ -o run glimpl.cpp $(core_file) -lGL -lGLEW -lglfw -lopenxr_loader  -lopenvr_api -lwayland-client -lwayland-egl -lEGL `sdl2-config --cflags --libs` -DWAYLAND
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
	

