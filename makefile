
OS := $(shell uname -s)

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o openxr_example.exe main.cpp -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o openxr_example.exe main.cpp -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lglfw3 -lopenxr_loader -lopengl32 -lgdi32
endif
ifeq ($(OS),Linux)
	compile_command = g++ -o openxr_example main.cpp -lGL -lGLEW -lglfw -lopenxr_loader -lX11
endif

all: 
	$(compile_command)

clear:
	rm -f openxr_example openxr_example.exe
	

