#include "block_device.h"
#include <iostream>

BlockDevice::BlockDevice(const std::string& name) : disk_name(name) {
    disk_file = fopen(disk_name.c_str(), "rb+");
    if (!disk_file) {
        std::cerr << "Error: Could not open " << disk_name << "\n";
    }
}

BlockDevice::~BlockDevice() {
    if (disk_file) fclose(disk_file);
}

bool BlockDevice::read_block(uint32_t block_id, void* buffer) {
    if (!disk_file || block_id >= TOTAL_BLOCKS) return false;
    fseek(disk_file, block_id * BLOCK_SIZE, SEEK_SET);
    return fread(buffer, 1, BLOCK_SIZE, disk_file) == BLOCK_SIZE;
}

bool BlockDevice::write_block(uint32_t block_id, const void* buffer) {
    if (!disk_file || block_id >= TOTAL_BLOCKS) return false;
    fseek(disk_file, block_id * BLOCK_SIZE, SEEK_SET);
    size_t written = fwrite(buffer, 1, BLOCK_SIZE, disk_file);
    fflush(disk_file);
    return written == BLOCK_SIZE;
}