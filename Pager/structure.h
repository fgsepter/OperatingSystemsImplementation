#ifndef _STRUCTURE_H_
#define _STRUCTURE_H_


#include <queue>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include "vm_arena.h"
#include "vm_pager.h"


struct virtual_page_entry_t {
    unsigned int reference = 0;
    unsigned int resident = 0;
    unsigned int dirty = 0;
    unsigned int swap_or_file = 0;          //0 for swap, 1 for file
    std::string filename = "";       //only used for file block
    unsigned int block = 0;         //block index for file, swap index for swap
};

struct virtual_page_table_t {
    virtual_page_entry_t vptes[VM_ARENA_SIZE/VM_PAGESIZE];
};

struct disk_block_status_t {
    page_table_entry_t ptes;
    virtual_page_entry_t vptes;
};

struct process_t {
    virtual_page_table_t VPT;
    page_table_t PPT;
    unsigned int current_position = 0;
};

struct disk_block_t{
    std::string filename;
    unsigned int block;
    bool operator==(const disk_block_t &a) const {
        return (a.filename==filename && a.block == block);
    }
    bool operator<(const disk_block_t &a) const {
        return (filename < a.filename) || ((filename == a.filename) && (block < a.block));
    }
};


#endif /* _VM_STRUCTURE_H_ */
