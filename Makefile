CXX=g++
CXXFLAGS=-std=c++17 -O2 -Wall -g
SRCS=$(wildcard src/*.cpp)
TARGET=build/riscv_sim

all:
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -rf build