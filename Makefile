00CXX = g++
CXXFLAGS = -std=c++17



all: mygit

mygit: mygit.o utils.o
	$(CXX) $(CXXFLAGS) -o mygit mygit.o utils.o -lssl -lcrypto -lz

mygit.o: mygit.cpp mygit.h
	$(CXX) $(CXXFLAGS) -c mygit.cpp

utils.o: utils.cpp mygit.h
	$(CXX) $(CXXFLAGS) -c utils.cpp

clean:
	rm -f *.o mygit

