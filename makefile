
OS := $(shell uname -s)

core_file = main.cpp

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o openxr_example.exe $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32 #`sdl2-config --cflags --libs`
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o openxr_example.exe $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32 #`sdl2-config --cflags --libs`
endif
ifeq ($(OS),Linux)
	compile_command = g++ -o openxr_example $(core_file) -lGL -lGLEW -lglfw -lopenxr_loader -DX11 -lX11 #`sdl2-config --cflags --libs` 
endif

all: 
	$(compile_command)

clear:
	rm -f openxr_example openxr_example.exe
	

