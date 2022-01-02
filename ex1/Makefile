all: build

build: mandelbrot_paralelizado.cpp
	g++ -Wall -g -o prog mandelbrot_paralelizado.cpp -lpthread

run: build
	./prog $(ARGS)

clean: 
	/bin/rm -f *.o prog
