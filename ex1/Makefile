all: build

build: paralelo.cpp
	g++ -Wall -g -o prog paralelo.cpp -lpthread

run: build
	./prog $(ARGS)

clean: 
	/bin/rm -f *.o prog
