CXXFLAGS = -Wall -Wextra -std=c++20 -O0
OUTFILE = main
CXX = g++

main: main.o tarstream.o
	$(CXX) -o $(OUTFILE) main.o tarstream.o

main.o: main.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c main.cc -o main.o

tarstream.o: tarstream.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c tarstream.cc -o tarstream.o

clean:
	rm $(OUTFILE) main.o tarstream.o
