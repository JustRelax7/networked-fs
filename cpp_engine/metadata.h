#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

const uint32_t BLOCK_SIZE = 4096;               
const uint64_t DISK_SIZE = 1073741824;          // 1GB Virtual Disk
const uint32_t TOTAL_BLOCKS = DISK_SIZE / BLOCK_SIZE; // 262,144 blocks
const uint32_t MAX_FILENAME = 28;

#pragma pack(push, 1) // Prevents compiler from adding padding bytes

struct Superblock {
    uint32_t magic_number;       // 0x05F5E111
    uint32_t total_blocks;
    uint32_t inode_bitmap_start; // Block 1
    uint32_t data_bitmap_start;  // Block 2
    uint32_t inode_table_start;  // Block 10
    uint32_t data_blocks_start;  // Block 522
    uint32_t root_inode_id;      // 0
};

// 
struct Inode {
    uint32_t id;
    uint32_t size;               
    uint8_t  is_dir;             // 1 for dir, 0 for file
    uint8_t  padding[3];         
    uint32_t direct_ptrs[12];    // Direct block pointers
    uint32_t single_indirect;    // Points to a block of pointers
};

struct DirectoryEntry {
    char name[MAX_FILENAME];
    uint32_t inode_id;
};

#pragma pack(pop)
#endif