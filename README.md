# FUSE TAR File System

## Introduction

This project implements a user-space file system using FUSE (File System in User Space) that allows direct access to files within a TAR archive without extracting them. The file system enables users to browse, read, and interact with TAR archive contents as if they were native filesystem directories.

## Table of Contents

1. [Features](#features)
2. [Installation](#installation)
3. [Usage](#usage)
4. [Dependencies](#dependencies)
5. [Project Structure](#project-structure)
6. [Contributors](#contributors)

## Features

- Mount TAR archives as readable file systems
- Support for reading file contents directly from the archive
- Handles multiple versions of files with different modification times
- Read-only access to TAR archive contents
- Compatible with standard Linux file system commands (ls, cat, etc.)

## Installation

### Prerequisites

- Linux environment (tested on Ubuntu 18.04)
- FUSE library

### Steps

1. Install FUSE development libraries:
   ```bash
   sudo apt install libfuse-dev
   ```

2. Compile the project:
   ```bash
   gcc User_File_System.c -o tarfs `pkg-config fuse --cflags --libs`
   ```

## Usage

1. Create a mount point directory:
   ```bash
   mkdir tarfs
   ```

2. Run the FUSE server:
   ```bash
   ./tarfs -f tarfs
   ```

3. Access files:
   ```bash
   cd tarfs
   ls             # List archive contents
   cat filename   # Read file contents
   ```

4. Unmount the file system:
   ```bash
   sudo umount -l tarfs
   ```

## Dependencies

- FUSE (File System in User Space)
- GCC compiler
- Standard C libraries

## Project Structure

```
.
├── User_File_System.c     # Main FUSE implementation
├── test.tar               # TAR archive to be mounted
└── tarfs/                 # Mount point directory
```

## Key Components

- `parse_tar_fs()`: Parses TAR archive metadata
- `my_getattr()`: Retrieves file/directory attributes
- `my_readdir()`: Lists directory contents
- `my_read()`: Reads file contents

## 6. Contributors
- **Name:** Hua-En (Benson) Lee
- **Email:** enen1015@gmail.com
