LIB = -lrt

all: $(wildcard *.cpp)
	mkdir -p obj
	g++ -o obj/main $(wildcard *.cpp) -Wall -pedantic -g -O3 -lm -ffast-math -pthread
