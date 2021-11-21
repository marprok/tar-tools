CXXFLAGS = -Wall -Wextra -std=c++20 -O3
CXX = g++

parser: parser.o tarstream.o
	$(CXX) -o parser parser.o tarstream.o

parser.o: parser.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c parser.cc -o parser.o

tarstream.o: tarstream.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c tarstream.cc -o tarstream.o

archiver: archiver.o tarstream.o
	$(CXX) -o archiver archiver.o tarstream.o

archiver.o: archiver.cc tarstream.hh
	$(CXX) $(CXXFLAGS) -c archiver.cc -o archiver.o

clean:
	rm archiver archiver.o parser.o tarstream.o
