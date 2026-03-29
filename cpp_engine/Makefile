CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fPIC

# Added test_fs to the default build list
all: mkfs libengine.so test_fs

mkfs: mkfs.o block_device.o bitmap_manager.o
	$(CXX) $(CXXFLAGS) -o mkfs mkfs.o block_device.o bitmap_manager.o

libengine.so: engine.o block_device.o bitmap_manager.o
	$(CXX) -shared -o libengine.so engine.o block_device.o bitmap_manager.o

# New rule to build the local test executable
test_fs: test_local.o engine.o block_device.o bitmap_manager.o
	$(CXX) $(CXXFLAGS) -o test_fs test_local.o engine.o block_device.o bitmap_manager.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o mkfs libengine.so test_fs