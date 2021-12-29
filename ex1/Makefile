all: build

build: mandel-tiles-graphic.cpp
	g++ -Wall -g -o prog mandel-tiles-graphic.cpp -lgraph

run: build
	./prog $(ARGS)

clean: 
	/bin/rm -f *.o prog
