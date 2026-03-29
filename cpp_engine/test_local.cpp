#include <iostream>
#include <cstring>
#include <cstdlib>

// Declare the interface functions exported by engine.cpp
extern "C" {
    int fs_init(const char* disk_path);
    int fs_write(const char* path, const char* data, int size);
    char* fs_read(const char* path, int* out_size);
    void delete_file(const char* path);
    void fs_unmount();
}

int main() {
    std::cout << "\n=== Starting Local File System Test ===\n";

    // 1. Mount the disk
    if (fs_init("virtual_disk.bin") != 0) {
        std::cerr << "Failed to mount! Did you run ./mkfs first?\n";
        return 1;
    }

    // 2. Test Write (New File)
    const char* data_v1 = "Hello, this is my first file!";
    int size_v1 = strlen(data_v1) + 1; // +1 to include the null terminator
    fs_write("notes.txt", data_v1, size_v1);

    // 3. Test Read
    int read_size = 0;
    char* read_buffer = fs_read("notes.txt", &read_size);
    if (read_buffer) {
        std::cout << "[Test] Read notes.txt: " << read_buffer << "\n";
        free(read_buffer); // Free the malloc from fs_read!
    }

    // 4. Test Versioning (Overwrite the file)
    std::cout << "\n--- Simulating User Updating the File ---\n";
    const char* data_v2 = "This is VERSION 2. The old text should be saved.";
    int size_v2 = strlen(data_v2) + 1;
    fs_write("notes.txt", data_v2, size_v2);

    // 5. Read both versions to prove CoW works
    read_buffer = fs_read("notes.txt", &read_size);
    if (read_buffer) {
        std::cout << "[Test] Read notes.txt (Current): " << read_buffer << "\n";
        free(read_buffer);
    }

    read_buffer = fs_read("notes.txt.v_old", &read_size);
    if (read_buffer) {
        std::cout << "[Test] Read notes.txt.v_old (History): " << read_buffer << "\n";
        free(read_buffer);
    }

    // 6. Test Deletion
    std::cout << "\n--- Simulating User Deleting the Old Version ---\n";
    delete_file("notes.txt.v_old");

    // Try to read the deleted file (Should fail/return nothing)
    read_buffer = fs_read("notes.txt.v_old", &read_size);
    if (!read_buffer) {
        std::cout << "[Test] Success: notes.txt.v_old is gone.\n";
    }

    // 7. Clean up
    fs_unmount();
    std::cout << "=== Local Test Complete ===\n\n";
    
    return 0;
}