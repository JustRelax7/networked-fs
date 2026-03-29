#ifndef BITMAP_MANAGER_H
#define BITMAP_MANAGER_H

#include <stdint.h>
#include "block_device.h"

class BitmapManager {
private:
    BlockDevice* disk;
    uint32_t start_block;   //starting address of bitmap block
    uint32_t num_blocks;    //number of bitmap blocks
    uint32_t last_searched_block; // <--- The Improvement

public:
    BitmapManager(BlockDevice* d, uint32_t start, uint32_t count);
    int allocate_bit();
    void free_bit(uint32_t index);
};
#endif