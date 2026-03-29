#include "bitmap_manager.h"

BitmapManager::BitmapManager(BlockDevice* d, uint32_t start, uint32_t count)
    : disk(d), start_block(start), num_blocks(count) {}

int BitmapManager::allocate_bit() {
    uint8_t buffer[BLOCK_SIZE];
    
    // Start searching from where last left off
    for (uint32_t i = 0; i < num_blocks; ++i) {
        // Using modulo to wrap around
        uint32_t current_block = (last_searched_block + i) % num_blocks;
        
        disk->read_block(start_block + current_block, buffer);
        
        for (uint32_t byte_idx = 0; byte_idx < BLOCK_SIZE; ++byte_idx) {
            if (buffer[byte_idx] != 0xFF) { 
                for (int bit = 0; bit < 8; ++bit) {
                    if (!(buffer[byte_idx] & (1 << bit))) {
                        buffer[byte_idx] |= (1 << bit);
                        disk->write_block(start_block + current_block, buffer);
                        
                        // Update for next call
                        last_searched_block = current_block;
                        
                        return (current_block * BLOCK_SIZE * 8) + (byte_idx * 8) + bit;
                    }
                }
            }
        }
    }
    return -1; // Disk full
}

void BitmapManager::free_bit(uint32_t index) {
    uint32_t block_offset = index / (BLOCK_SIZE * 8);
    uint32_t byte_offset = (index % (BLOCK_SIZE * 8)) / 8;
    uint32_t bit_offset = index % 8;

    uint8_t buffer[BLOCK_SIZE];
    disk->read_block(start_block + block_offset, buffer);
    buffer[byte_offset] &= ~(1 << bit_offset); 
    disk->write_block(start_block + block_offset, buffer);
}