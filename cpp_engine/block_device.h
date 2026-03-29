#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include <string>
#include <cstdio>
#include "metadata.h"

class BlockDevice {
private:
    FILE* disk_file;
    std::string disk_name;

public:
    BlockDevice(const std::string& name);
    ~BlockDevice();
    bool read_block(uint32_t block_id, void* buffer);
    bool write_block(uint32_t block_id, const void* buffer);
};
#endif