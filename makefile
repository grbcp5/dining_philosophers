#CXX = g++
CXX = mpicxx
CXXFLAGS = -Wall -W -s -O3 -march=native -std=c++11
LIBRARIES = -lpthread
.PHONY: default run
default: build

build:
	${CXX} ${CXXFLAGS} src/*.cpp ${LIBRARIES} -o bin/program 
	
run:
	./bin/program
 
clean:
	rm -f bin/*.o bin/program

