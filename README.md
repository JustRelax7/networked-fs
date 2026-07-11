# Networked File System

## Overview
A networked storage system that provides remote, multi-client file operations. Instead of relying on the host operating system's filesystem, this project implements a custom 1 GB virtual filesystem from scratch in C++, which is exposed to network clients via a high-concurrency Go backend. 

## Key Features
* **Custom Virtual Filesystem:** A standalone storage engine built in C++ that manages its own superblocks, inodes, and bitmap allocation on a raw virtual block device.
* **Concurrent Network Backend:** A Go-based network layer that utilizes a pool of 16 worker goroutines to process multi-client requests simultaneously.
* **Per-File Synchronization:** Implements fine-grained locking mechanisms to ensure thread safety and prevent data corruption during simultaneous multi-client read/write operations.
* **Cross-Language Integration:** Utilizes CGO to bridge the high-concurrency Go networking layer directly with the unmanaged C++ storage engine.

## Architecture
The system is designed with a strict three-tier architecture:

1. **Storage Engine (C++):** Handles low-level disk I/O and memory layout. It translates raw binary data into a logical file structure using direct block pointers and byte-level manipulation.
2. **Network Bridge (Go):** Acts as the middleware, receiving HTTP/REST requests, managing concurrent connections, enforcing synchronization locks, and executing calls to the C++ engine via CGO.
3. **Frontend Client (React):** A web-based user interface built with Vite and Tailwind CSS to facilitate file uploads, downloads, and directory management.

## Tech Stack
* **Systems & Backend:** C++, Go, CGO, Gin
* **Frontend:** React, Vite, Tailwind CSS
* **Build & Tools:** Makefile, HTTP/REST, Git

## Repository Structure
* `/cpp_engine/`: Contains the core C++ virtual filesystem implementation, including `engine.cpp`, `block_device.cpp`, and `bitmap_manager.cpp`.
* `/go_network/`: Contains the Go API server and CGO bindings (`main.go`).
* `/networked_fs/`: Contains the React web client and UI configuration files.
* `/test_local.cpp` & `/test.sh`: Scripts and C++ binaries for local storage engine validation.

## Build and Run Instructions

### Prerequisites
* GCC/G++ compiler
* Go (1.x+)
* Node.js & npm

### Setup
1. **Compile the Storage Engine:**
   Navigate to the `cpp_engine` directory and run the Makefile to build the C++ binaries and format the virtual disk.
   ```bash
   cd cpp_engine
   make
   ./mkfs
