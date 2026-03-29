#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include "metadata.h"
#include "block_device.h"
#include "bitmap_manager.h"

// --- Global State for the C-Bridge ---
BlockDevice* disk = nullptr;
BitmapManager* inode_bm = nullptr;
BitmapManager* data_bm = nullptr;
Superblock sb;

// --- Helper Functions (Kernel Space Logic) ---

// Reads an Inode from the Inode Table on disk
bool load_inode(uint32_t inode_id, Inode& inode) {
    if (inode_id >= (sb.data_blocks_start - sb.inode_table_start) * (BLOCK_SIZE / sizeof(Inode))) return false;
    
    uint32_t block_offset = sb.inode_table_start + (inode_id * sizeof(Inode)) / BLOCK_SIZE;
    uint32_t byte_offset = (inode_id * sizeof(Inode)) % BLOCK_SIZE;
    
    char buffer[BLOCK_SIZE];
    disk->read_block(block_offset, buffer);
    memcpy(&inode, buffer + byte_offset, sizeof(Inode));
    return true;
}

// Writes an Inode back to the Inode Table on disk
bool save_inode(uint32_t inode_id, const Inode& inode) {
    uint32_t block_offset = sb.inode_table_start + (inode_id * sizeof(Inode)) / BLOCK_SIZE;
    uint32_t byte_offset = (inode_id * sizeof(Inode)) % BLOCK_SIZE;
    
    char buffer[BLOCK_SIZE];
    disk->read_block(block_offset, buffer); // Read existing block
    memcpy(buffer + byte_offset, &inode, sizeof(Inode)); // modify Inode
    return disk->write_block(block_offset, buffer); // write 
}

// Resolves a path (e.g., "file.txt") in the Root Directory (Inode 0)
// Fulfills: Name resolution in directory tree
int resolve_path(const char* path) {
    Inode root;
    load_inode(sb.root_inode_id, root);
    
    //search the first direct block of the root directory
    if (root.size == 0 || root.direct_ptrs[0] == 0) return -1;

    char buffer[BLOCK_SIZE];
    disk->read_block(root.direct_ptrs[0], buffer);
    
    int entries = root.size / sizeof(DirectoryEntry);
    DirectoryEntry* dir_array = reinterpret_cast<DirectoryEntry*>(buffer);
    
    for (int i = 0; i < entries; i++) {
        if (strcmp(dir_array[i].name, path) == 0) {
            return dir_array[i].inode_id;
        }
    }
    return -1; // !found
}

// Adds a new entry to the Root Directory
void add_to_directory(uint32_t dir_inode_id, const char* name, uint32_t target_inode_id) {
    Inode dir_node;
    load_inode(dir_inode_id, dir_node);
    
    // Allocate a block for the directory if it doesn't have one
    if (dir_node.size == 0) {
        dir_node.direct_ptrs[0] = data_bm->allocate_bit();
    }

    char buffer[BLOCK_SIZE];
    disk->read_block(dir_node.direct_ptrs[0], buffer);
    
    int entries = dir_node.size / sizeof(DirectoryEntry);
    DirectoryEntry* dir_array = reinterpret_cast<DirectoryEntry*>(buffer);
    
    // add new entry
    strncpy(dir_array[entries].name, name, MAX_FILENAME);
    dir_array[entries].inode_id = target_inode_id;
    
    dir_node.size += sizeof(DirectoryEntry); // Update directory
    // Add new entryctory size
    
    disk->write_block(dir_node.direct_ptrs[0], buffer);
    save_inode(dir_inode_id, dir_node);
}


// --- EXTERN C INTERFACE (The Bridge for GoLang) ---
extern "C" {

    // Mounts the File System
    int fs_init(const char* disk_path) {
        disk = new BlockDevice(disk_path);
        
        char buffer[BLOCK_SIZE];
        if (!disk->read_block(0, buffer)) return -1;
        
        memcpy(&sb, buffer, sizeof(Superblock));
        
        // Verify Magic Number
        if (sb.magic_number != 0x05F5E111) return -2;

        inode_bm = new BitmapManager(disk, sb.inode_bitmap_start, 1);
        data_bm = new BitmapManager(disk, sb.data_bitmap_start, 8);
        
        std::cout << "[C++ Engine] File System Mounted Successfully.\n";
        return 0; 
    }

    // Creates a file and writes data to it
    // Fulfills: Indexing structures, Direct Pointer Allocation
    int fs_write(const char* path, const char* data, int size) {
        int existing_inode = resolve_path(path);
        if (existing_inode != -1) {
            std::cerr << "[C++ Engine] File already exists. Versioning not yet triggered.\n";
            return -1; 
        }

        // 1. Allocate Inode
        int new_inode_id = inode_bm->allocate_bit();
        if (new_inode_id == -1) return -1; // out of inodes; MLE

        Inode new_file;
        memset(&new_file, 0, sizeof(Inode));
        new_file.id = new_inode_id;
        new_file.size = size;
        new_file.is_dir = 0;

        // 2. Allocate Data Blocks and Write Data
        int bytes_written = 0;
        int block_idx = 0;

        while (bytes_written < size && block_idx < 12) {
            int new_block = data_bm->allocate_bit();
            if (new_block == -1) break; // Out of space

            new_file.direct_ptrs[block_idx] = new_block;

            char buffer[BLOCK_SIZE] = {0};
            int chunk_size = std::min(BLOCK_SIZE, (uint32_t)(size - bytes_written));
            memcpy(buffer, data + bytes_written, chunk_size);
            
            disk->write_block(new_block, buffer);
            
            bytes_written += chunk_size;
            block_idx++;
        }

        // 3. Save Metadata
        save_inode(new_inode_id, new_file);
        add_to_directory(sb.root_inode_id, path, new_inode_id);

        std::cout << "[C++ Engine] Written " << bytes_written << " bytes to " << path << " (Inode: " << new_inode_id << ")\n";
        return new_inode_id;
    }

    // Reads a file and returns a heap-allocated buffer
    char* fs_read(const char* path, int* out_size) {
        int target_inode_id = resolve_path(path);
        if (target_inode_id == -1) {
            *out_size = 0;
            return nullptr;
        }

        Inode target;
        load_inode(target_inode_id, target);
        
        *out_size = target.size;
        char* result_buffer = (char*)malloc(target.size); // G0 responsible for freeing this
        
        int bytes_read = 0;
        int block_idx = 0;

        while (bytes_read < target.size && block_idx < 12) {
            char block_buffer[BLOCK_SIZE];
            disk->read_block(target.direct_ptrs[block_idx], block_buffer);
            
            int chunk_size = std::min(BLOCK_SIZE, (uint32_t)(target.size - bytes_read));
            memcpy(result_buffer + bytes_read, block_buffer, chunk_size);
            
            bytes_read += chunk_size;
            block_idx++;
        }

        return result_buffer;
    }

    // Cleanup memory when shutting down
    void fs_unmount() {
        delete inode_bm;
        delete data_bm;
        delete disk;
    }
}