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

// Renames a file in the directory to preserve it as an older version
void rename_in_directory(uint32_t dir_inode_id, const char* old_name, const char* new_name) {
    Inode dir_node;
    load_inode(dir_inode_id, dir_node);
    
    if (dir_node.size == 0 || dir_node.direct_ptrs[0] == 0) return;

    char buffer[BLOCK_SIZE];
    disk->read_block(dir_node.direct_ptrs[0], buffer);
    
    int entries = dir_node.size / sizeof(DirectoryEntry);
    DirectoryEntry* dir_array = reinterpret_cast<DirectoryEntry*>(buffer);
    
    for (int i = 0; i < entries; i++) {
        if (strcmp(dir_array[i].name, old_name) == 0) {
            // Found old file; Rename it
            memset(dir_array[i].name, 0, MAX_FILENAME); // Clear old name
            strncpy(dir_array[i].name, new_name, MAX_FILENAME);
            
            // Persist the directory change to disk
            disk->write_block(dir_node.direct_ptrs[0], buffer);
            std::cout << "[C++ Engine] Preserved old version as: " << new_name << "\n";
            return;
        }
    }
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

    // Lists all files in the root directory
    // Returns a heap-allocated, newline-separated C-string. Go must free it.
    char* fs_list() {
        Inode root_dir;
        load_inode(sb.root_inode_id, root_dir);

        // If the directory has never been allocated or is empty, return empty string
        if (root_dir.size == 0 || root_dir.direct_ptrs[0] == 0) {
            char* empty_result = (char*)malloc(1);
            empty_result[0] = '\0';
            return empty_result;
        }

        char buffer[BLOCK_SIZE];
        disk->read_block(root_dir.direct_ptrs[0], buffer);

        int entries = root_dir.size / sizeof(DirectoryEntry);
        DirectoryEntry* dir_array = reinterpret_cast<DirectoryEntry*>(buffer);

        std::stringstream ss;
        
        // Loop through all slots in the directory data block
        for (int i = 0; i < entries; i++) {
            // Ensure the slot isn't empty (skipped over a deleted file)
            if (dir_array[i].name[0] != '\0') {
                ss << dir_array[i].name << "\n";
            }
        }

        // Convert the C++ string stream into a heap-allocated C-string for Go
        std::string result_str = ss.str();
        char* c_result = (char*)malloc(result_str.length() + 1);
        strcpy(c_result, result_str.c_str());

        return c_result;
    }
    // delete a file, free inode and data bitmap
    int delete_file(const char* path) {
        int target_inode_id = resolve_path(path);
        if (target_inode_id == -1) return  -1; // File doesn't exist

        Inode target;
        load_inode(target_inode_id, target);

        // 1. Free the physical data blocks
        int blocks_used = (target.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < blocks_used && i < 12; i++) {
            if (target.direct_ptrs[i] != 0) {
                data_bm->free_bit(target.direct_ptrs[i]);
            }
        }

        // 2. Free the Inode
        inode_bm->free_bit(target_inode_id);

        // 3. Unlink from Directory
        Inode root_dir;
        load_inode(sb.root_inode_id, root_dir);
        
        if (root_dir.direct_ptrs[0] != 0) {
            char buffer[BLOCK_SIZE];
            disk->read_block(root_dir.direct_ptrs[0], buffer);
            
            int entries = root_dir.size / sizeof(DirectoryEntry);
            DirectoryEntry* dir_array = reinterpret_cast<DirectoryEntry*>(buffer);
            
            for (int i = 0; i < entries; i++) {
                if (strcmp(dir_array[i].name, path) == 0) {
                    // Mark as deleted by clearing the name. 
                    // (In a production OS, we would compact the array to save space)
                    memset(dir_array[i].name, 0, MAX_FILENAME);
                    dir_array[i].inode_id = 0;
                    disk->write_block(root_dir.direct_ptrs[0], buffer);
                    break;
                }
            }
        }
        std::cout << "[C++ Engine] Deleted old version and freed memory: " << path << "\n";
        return 0;
    }



    // Creates a file and writes data to it
    // Fulfills: Indexing structures, Direct Pointer Allocation
    int fs_write(const char* path, const char* data, int size) {
        int existing_inode = resolve_path(path);
        // VERSIONING LOGIC ---
        if (existing_inode != -1) {
            std::string versioned_name = std::string(path) + ".v_old";
            
            // Case 3: If .v_old ALREADY exists, delete it
            if (resolve_path(versioned_name.c_str()) != -1) {
                delete_file(versioned_name.c_str());
            }

            // Case 2: Rename the current file to .v_old
            std::cout << "[C++ Engine] CoW Versioning triggered for '" << path << "'\n";
            rename_in_directory(sb.root_inode_id, path, versioned_name.c_str());
        }
        // Case 1: (Handled automatically) Proceed to allocate new Inode below...
        // ------------------------------------

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
        
        uint32_t bytes_read = 0;
        uint32_t block_idx = 0;

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