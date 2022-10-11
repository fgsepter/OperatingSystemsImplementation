#include "vm_helper.h"


void Set_state(unsigned int read, unsigned int write, unsigned int resident, unsigned int reference, unsigned int dirty, unsigned int ppage, bool isUpdatePPG, page_table_entry_t * pte, virtual_page_entry_t * vpte){
    if(isUpdatePPG) {
        pte->ppage = ppage;
    }
    pte->read_enable  =read;
    pte->write_enable =write;
    vpte->resident    =resident;
    vpte->reference   =reference;
    vpte->dirty       =dirty;
}


void Set_page_state(unsigned int read, unsigned int write, unsigned int resident, unsigned int reference, unsigned int dirty, disk_block_t & cur_db, unsigned int ppage, bool isUpdatePPG){
    Set_state(read, write, resident, reference, dirty, ppage, isUpdatePPG, &block_status_map[cur_db].ptes, &block_status_map[cur_db].vptes);
    if(share_map.find(cur_db) != share_map.end()){
        for(auto& i : share_map[cur_db]){
            for (auto& j : i.second){
                Set_state(read, write, resident, reference, dirty, ppage, isUpdatePPG, &process_map[i.first].PPT.ptes[j], &process_map[i.first].VPT.vptes[j]);
            }
        }
    } 
}

unsigned int Find_place_in_PM(disk_block_t replace_block){ 
    void * free_addr; 
    unsigned int target_ppage;
    /*** We have free PM ***/
    if (!free_physical_pages.empty()){
        target_ppage = free_physical_pages.front();
        free_physical_pages.pop();
        ppage_to_block_map[target_ppage] = replace_block;
        clock_queue.push(target_ppage);
        return target_ppage;
    }
    /*** We do not have free PM ***/
    /*** Apply Clock Algorithm to evict one from the PM ***/
    assert(!clock_queue.empty());
    unsigned int temp_ppage;
    disk_block_t temp_block;
    while (true) {
        temp_ppage = clock_queue.front();
        clock_queue.pop();
        assert(temp_ppage!=0);
        temp_block = ppage_to_block_map[temp_ppage];
        disk_block_status_t status = block_status_map[temp_block];
        if (status.vptes.reference == 1){ 
            /*** has reference ***/
            Set_page_state(0, 0, 1, 0, status.vptes.dirty, temp_block, 0, false);
            clock_queue.push(temp_ppage);
        }
        else { 
            /*** no reference ***/
            target_ppage = temp_ppage;
            free_addr = (void*) ((uintptr_t)target_ppage * (uintptr_t)VM_PAGESIZE + (uintptr_t)vm_physmem);
            if (status.vptes.dirty == 1) {
                if (file_write(FilenameToChar(temp_block.filename), temp_block.block, free_addr) == -1) {
                    throw -1;
                }
            }
            Set_page_state(0, 0, 0, 0, 0, temp_block, 0, false);
            ppage_to_block_map[target_ppage] = replace_block;
            clock_queue.push(target_ppage);
            return target_ppage;
        }
    }
    assert(0);
}

std::string Find_filename(const char* filename_addr){
    std::string filename = "";
    unsigned int vpn = ((uintptr_t)filename_addr-(uintptr_t)VM_ARENA_BASEADDR) / (uintptr_t)VM_PAGESIZE;
    unsigned int offset = (uintptr_t)filename_addr-(uintptr_t)VM_ARENA_BASEADDR - (uintptr_t)(vpn * VM_PAGESIZE);
    unsigned int temp_physical_address_index = 0;
    char temp_alpha = ' ';
    void * read_addr;
    while(temp_alpha != '\0'){
        if(offset == VM_PAGESIZE){
            vpn++;
            offset = 0;
        }
        read_addr = (void*)((uintptr_t)(vpn * VM_PAGESIZE) + (uintptr_t)offset + (uintptr_t)VM_ARENA_BASEADDR);
        if ((uintptr_t)read_addr >= (uintptr_t)VM_ARENA_BASEADDR + (uintptr_t)process_map[current_pid].current_position * VM_PAGESIZE) {
            throw (-1);
        }
        if(!page_table_base_register->ptes[vpn].read_enable){
            if (vm_fault(read_addr, false) == -1) {
                throw (-1);
            } 
        }
        temp_physical_address_index = page_table_base_register->ptes[vpn].ppage * VM_PAGESIZE + offset;
        temp_alpha = ((char*)vm_physmem)[temp_physical_address_index];
        offset++;
        filename += temp_alpha;
    }
    return filename;
}

void RemoveFromClockQueue(unsigned int dirty_ppage){
    unsigned int top = clock_queue.front();
    unsigned int temp_ppage;
    if (top == dirty_ppage) {
        clock_queue.pop();
    }
    else {
        assert(top != dirty_ppage);
        clock_queue.pop();
        clock_queue.push(top);
        temp_ppage = clock_queue.front();
        while (temp_ppage != top){
            clock_queue.pop();
            if(temp_ppage != dirty_ppage) {
                clock_queue.push(temp_ppage);
            }
            temp_ppage = clock_queue.front();
        }
    }
}

const char * FilenameToChar(std::string filename){
    if (filename == ""){
        return nullptr;
    }
    else {
        assert(filename != "");
        return filename.c_str();
    }
}