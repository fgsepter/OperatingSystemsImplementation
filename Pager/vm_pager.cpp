#include "vm_pager.h"
#include "vm_arena.h"
#include "vm_helper.h"
#include "structure.h"
#include <queue>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <cassert>

pid_t current_pid;                                                             // current_pid track the index of the current running process.
std::map<pid_t, process_t> process_map;                                        // map from process id to the process virtual memory page table
std::map<unsigned int, disk_block_t> ppage_to_block_map;                       // map from ppage to the disk block using it
std::map<disk_block_t, std::map<pid_t, std::vector<unsigned int>>> share_map;  // map from disk block to a map which contains pid and virtual page num
std::map<disk_block_t, disk_block_status_t> block_status_map;                  // map from disk block to its status 
std::queue<unsigned int> free_swap_blocks;                                     // available swap blocks
std::queue<unsigned int> free_physical_pages;                                  // available physical pages
std::queue<unsigned int> clock_queue;                                          // unsigned int is the index of ppage

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    /*** Initialize all the queues ***/
    for (unsigned int i = 1; i < memory_pages; i++){ 
        free_physical_pages.push(i);
    }
    for (unsigned int i = 0; i < swap_blocks; i++){
        free_swap_blocks.push(i);
    }
    assert(clock_queue.empty());
    std::memset(vm_physmem, 0, VM_PAGESIZE);
}

int vm_create(pid_t parent_pid, pid_t child_pid){
    /*** Create a new process_t and add it to the process_map ***/
    process_t VM_table;
    page_table_t new_PPT;
    for (long unsigned int i =0 ; i < VM_ARENA_SIZE/VM_PAGESIZE; i++){
        new_PPT.ptes[i].ppage = 0;
        new_PPT.ptes[i].read_enable = 0;
        new_PPT.ptes[i].write_enable = 0;
    }
    virtual_page_table_t new_VPT;
    VM_table.PPT = new_PPT;
    VM_table.VPT = new_VPT;
    VM_table.current_position = 0;
    process_map[child_pid] = VM_table;
    return 0;
}

void vm_switch(pid_t pid){
    /*** Switch to pid ***/
    if (pid == current_pid) return;
    assert(process_map.find(pid) != process_map.end());
    current_pid = pid;
    page_table_base_register = & process_map[pid].PPT;
}

void vm_destroy(){
    /*** Destroy the current process and free swap blocks used by the process ***/
    unsigned int ppage_num = process_map[current_pid].current_position;
    virtual_page_table_t *VPT = & process_map[current_pid].VPT;
    disk_block_t db;
    for (unsigned int i = 0; i < ppage_num; i++){
        db.filename = VPT->vptes[i].filename;
        db.block = VPT->vptes[i].block;
        if(share_map[db].find(current_pid) != share_map[db].end()){
            share_map[db].erase(share_map[db].find(current_pid));
        }
        if (VPT->vptes[i].swap_or_file == 0){ 
            /*** swap backs ***/
            free_swap_blocks.push(db.block);
            unsigned int dirty_ppage = page_table_base_register->ptes[i].ppage;
            if (VPT->vptes[i].resident == 1 && dirty_ppage != 0){
                free_physical_pages.push(dirty_ppage);
                RemoveFromClockQueue(dirty_ppage);
            }
        }
    }
    if(process_map.find(current_pid) != process_map.end()){
        /*** Clear VM page table ***/
        process_map.erase(process_map.find(current_pid)); 
    }
}

int vm_fault(const void* addr, bool write_flag){
    /*** Handle faults ***/
    if (((uintptr_t)addr < (uintptr_t)VM_ARENA_BASEADDR) || 
        ((uintptr_t)addr >= (uintptr_t)VM_ARENA_BASEADDR + (uintptr_t)process_map[current_pid].current_position * (uintptr_t)VM_PAGESIZE)) {
        return -1;
    }
    unsigned int vpn = ((uintptr_t)addr-(uintptr_t)VM_ARENA_BASEADDR) / (uintptr_t)VM_PAGESIZE;
    virtual_page_entry_t cur_virtual_page = process_map[current_pid].VPT.vptes[vpn];
    page_table_entry_t cur_phyical_page = page_table_base_register->ptes[vpn];
    disk_block_t cur_db = {.filename=cur_virtual_page.filename,.block=cur_virtual_page.block};

    if ((cur_phyical_page.read_enable && !cur_phyical_page.write_enable  && !cur_virtual_page.reference ) || !cur_virtual_page.resident){
        /*** If we want an empty physical memory page ***/
        unsigned int free_ppage; 
        try { 
            free_ppage = Find_place_in_PM(cur_db); 
        }
        catch(int e) { 
            return -1; 
        }
        if (cur_virtual_page.resident){
            /*** Situation 1 ***/
            /*** The target address is in pinning memory, and we want to write ***/
            /*** Copy on write ***/
            assert(write_flag);
            void * free_addr = (void*) ((uintptr_t)free_ppage * (uintptr_t)VM_PAGESIZE + (uintptr_t)vm_physmem);
            std::memset(free_addr, 0, VM_PAGESIZE);
        }
        else {
            /*** Situation 2 ***/
            /*** We require read from the disk ***/
            if (file_read(FilenameToChar(cur_db.filename),cur_db.block,(void*)((uintptr_t)free_ppage * (uintptr_t)VM_PAGESIZE + (uintptr_t)vm_physmem)) == -1) {
                free_physical_pages.push(free_ppage);
                RemoveFromClockQueue(free_ppage);
                return -1;
            }
        }
        cur_phyical_page.ppage = free_ppage;
        Set_page_state(1, write_flag, 1, 1, write_flag, cur_db, free_ppage, true);
    }
    else{
        /*** Situation 3 ***/
        /*** The target virtual address is in physical memory but not pinning memory ***/
        unsigned int if_dirty = (cur_virtual_page.dirty || write_flag);
        Set_page_state(1, if_dirty, 1, 1, if_dirty, cur_db, 0, false);
    }
    return 0;
}

void *vm_map(const char *filename, unsigned int block) {
    /*** create a new virtual memory for the block ***/
    if (process_map[current_pid].current_position >= VM_ARENA_SIZE/VM_PAGESIZE) { 
        /*** No avaiable virtual memory page ***/
        return nullptr; 
    }
    assert(process_map[current_pid].current_position < VM_ARENA_SIZE/VM_PAGESIZE);
    unsigned int vm_index = process_map[current_pid].current_position;
    if (filename == nullptr) {
        /*** Swap Backs Map ***/
        if (free_swap_blocks.empty()) { 
            /*** No avaiable swap back blocks ***/
            return nullptr; 
        }
        assert(!free_swap_blocks.empty());
        unsigned int swap_block_id = free_swap_blocks.front();
        free_swap_blocks.pop();
        disk_block_t swap_block;
        swap_block.filename = "";
        swap_block.block = swap_block_id;
        share_map[swap_block][current_pid].push_back(vm_index);
        Set_page_state(1, 0, 1, 0, 0, swap_block, 0, true); /*** Set Status ***/
        process_map[current_pid].VPT.vptes[vm_index].swap_or_file = 0;
        process_map[current_pid].VPT.vptes[vm_index].filename = "";
        process_map[current_pid].VPT.vptes[vm_index].block = swap_block_id;
        void * return_addr = (void *) ((uintptr_t)vm_index * (uintptr_t)VM_PAGESIZE + (uintptr_t)(VM_ARENA_BASEADDR));
        process_map[current_pid].current_position++;
        return return_addr;
    } 
    else {
        /*** File Backs Map ***/
        assert(filename != nullptr);
        if (((uintptr_t)filename < (uintptr_t)VM_ARENA_BASEADDR) || 
            ((uintptr_t)filename >= (uintptr_t)VM_ARENA_BASEADDR + (uintptr_t)vm_index * (uintptr_t)VM_PAGESIZE)) {
            /*** Address out of range ***/
            return nullptr;
        }
        std::string actual_filename;
        try          { 
            actual_filename = Find_filename(filename); 
        }
        catch (int e){ 
            /*** We cannot find the filename ***/
            return nullptr; 
        }
        disk_block_t file_block;
        file_block.filename = actual_filename;
        file_block.block = block;
        share_map[file_block][current_pid].push_back(vm_index);
        if (block_status_map.find(file_block) != block_status_map.end()){
            /*** previous process has already used this block ***/
            disk_block_status_t status = block_status_map[file_block];
            Set_page_state(status.ptes.read_enable, status.ptes.write_enable, status.vptes.resident, status.vptes.reference, status.vptes.dirty, file_block, status.ptes.ppage, true); 
        }
        else {
            /*** this file block is firstly used, and hence must not in PM ***/
            Set_page_state(0, 0, 0, 0, 0, file_block, 0, false);
        }
        process_map[current_pid].VPT.vptes[vm_index].swap_or_file = 1;
        process_map[current_pid].VPT.vptes[vm_index].filename = actual_filename;
        process_map[current_pid].VPT.vptes[vm_index].block = block;
        void * return_addr = (void*)((uintptr_t)vm_index * (uintptr_t)VM_PAGESIZE + (uintptr_t)VM_ARENA_BASEADDR);
        process_map[current_pid].current_position++;
        return return_addr;
    }
}