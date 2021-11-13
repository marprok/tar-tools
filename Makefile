CXXFLAGS = -Wall -Wextra -std=c++20 -O3
OUTFILE = main
CXX = g++

main: main.o tarstream.o
	$(CXX) -o $(OUTFILE) main.o tarstream.o

main.o: main.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c main.cc -o main.o

tarstream.o: tarstream.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c tarstream.cc -o tarstream.o

archiver: archiver.o tarstream.o
	$(CXX) -o archiver archiver.o tarstream.o

archiver.o: archiver.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c archiver.cc -o archiver.o

clean:
	rm $(OUTFILE) archiver archiver.o main.o tarstream.o
