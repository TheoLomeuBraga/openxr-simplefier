
OS := $(shell uname -s)

core_file = main.cpp

compile_command = 
ifeq ($(OS),Windows_NT)
	compile_command = g++ -o run.exe $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 -lopenvr_api
endif
ifeq ($(findstring MINGW,$(OS)),MINGW)
	compile_command = g++ -o run.exe $(core_file) -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib -lglew32 -lopenxr_loader -lopengl32 -lgdi32 -lopenvr_api
endif
ifeq ($(OS),Linux)
	compile_command = g++ -o run $(core_file) -lGL -lGLEW -lopenxr_loader -lX11 -lopenvr_api
	#compile_command = g++ -o run $(core_file) -lGL -lGLEW -lopenxr_loader -DWAYLAND -lX11 -lopenvr_api
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
	

