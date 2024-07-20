CC :=/home/sven/llvm/bin/clang
CXX :=/home/sven/llvm/bin/clang++
CXXC :=/home/sven/acpp/bin/acpp
CXXFLAGS := -O3 -fopenmp=libomp -lSDL2 -v

CPP_SOURCES := $(wildcard src/*.cpp)
HPP_SOURCES := $(wildcard src/*.hpp)
CPP_OBJECTS := $(patsubst src/%.cpp,build/%.o,$(CPP_SOURCES))

.PHONY: build

gameoflife: $(CPP_OBJECTS) | build
	$(CXXC) $(CXXFLAGS) -o $@ $(CPP_OBJECTS) 

build/%.o: src/%.cpp $(HPP_SOURCES) | build
	$(CXXC) $(CXXFLAGS) -c -o $@ $<

build:
	- mkdir build

clean:
	- rm build/*
	- rm gameoflife
