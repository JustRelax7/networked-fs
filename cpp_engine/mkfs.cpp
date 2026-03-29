#include <iostream>
#include <cstring>
#include "metadata.h"
#include "block_device.h"

int main() {
    BlockDevice disk("virtual_disk.bin");

    Superblock sb;
    memset(&sb, 0, sizeof(Superblock));
    sb.magic_number = 0x05F5E111;
    sb.total_blocks = TOTAL_BLOCKS;
    sb.inode_bitmap_start = 1;     
    sb.data_bitmap_start = 2;      
    sb.inode_table_start = 10;     
    sb.data_blocks_start = 522;    
    sb.root_inode_id = 0;

    char block_buffer[BLOCK_SIZE];

    //Write Superblock
    memset(block_buffer, 0, BLOCK_SIZE);
    memcpy(block_buffer, &sb, sizeof(Superblock));
    disk.write_block(0, block_buffer);

    //Clear Bitmaps
    memset(block_buffer, 0, BLOCK_SIZE);
    for (uint32_t i = sb.inode_bitmap_start; i < sb.inode_table_start; i++) {
        disk.write_block(i, block_buffer);
    }

    //Init Root Directory Inode
    Inode root_dir;
    memset(&root_dir, 0, sizeof(Inode));
    root_dir.id = 0;
    root_dir.is_dir = 1;
    root_dir.size = 0; 

    memset(block_buffer, 0, BLOCK_SIZE);
    memcpy(block_buffer, &root_dir, sizeof(Inode));
    disk.write_block(sb.inode_table_start, block_buffer);


    //Mark Root Inode as 'Used' in the Inode Bitmap
    memset(block_buffer, 0, BLOCK_SIZE);
    block_buffer[0] |= (1 << 0); // Set bit 0 to 1
    disk.write_block(sb.inode_bitmap_start, block_buffer);

    //Reserve Metadata Blocks in the Data Bitmap
    memset(block_buffer, 0, BLOCK_SIZE);
    
    for (uint32_t i = 0; i < sb.data_blocks_start; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        block_buffer[byte_idx] |= (1 << bit_idx); // Flip bit to 1 (Used)
    }
    
    disk.write_block(sb.data_bitmap_start, block_buffer);

    std::cout << "File System Formatted Successfully!\n";
    std::cout << "- Superblock initialized.\n";
    std::cout << "- Metadata blocks (0-" << (sb.data_blocks_start - 1) << ") reserved.\n";
    std::cout << "- Root Directory (/) created at Inode 0.\n";

    return 0;
}

   