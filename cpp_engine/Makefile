CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fPIC

all: mkfs

mkfs: mkfs.o block_device.o bitmap_manager.o
	$(CXX) $(CXXFLAGS) -o mkfs mkfs.o block_device.o bitmap_manager.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o mkfs libengine.so