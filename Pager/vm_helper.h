#ifndef _VM_HELPER_H_
#define _VM_HELPER_H_

#include "vm_pager.h"
#include "vm_arena.h"
#include "structure.h"
#include <queue>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <cassert>

extern pid_t current_pid;                                                // current_pid track the index of the current running process.
extern std::map<pid_t, process_t> process_map;                           // map from process id to the process virtual memory page table
extern std::map<unsigned int, disk_block_t> ppage_to_block_map;          // map from ppage to the disk block using it
extern std::map<disk_block_t, std::map<pid_t, std::vector<unsigned int>>> share_map;  // map from disk block to all the processes and vpn currently using the block 
extern std::map<disk_block_t, disk_block_status_t> block_status_map;     // map from disk block to its status 
extern std::queue<unsigned int> free_swap_blocks;                        // free_swap_blocks contains avaiable swap blocks
extern std::queue<unsigned int> free_physical_pages;                     // free_physical_pages contains avaiable physical pages
extern std::queue<unsigned int> clock_queue;                             // clock_queue contains the index of physical pages



/* Set_state
 * REQUIRES: target states for vpte and pte
 * MODIFIES: vpte, pte
 * EFFECTS:  change the states of vpte and pte
 */
void Set_state(unsigned int read, unsigned int write, unsigned int resident, unsigned int reference, unsigned int dirty, unsigned int ppage, bool isUpdatePPG, page_table_entry_t * pte, virtual_page_entry_t * vpte);

/* Set_page_state
 * REQUIRES: target states for input disk_block_t
 * MODIFIES: processes states in share_map[cur_db]; disk block states in block_status_map
 * EFFECTS:  change the states of all the processes currently using the cur_db to target states
 */
void Set_page_state(unsigned int read, unsigned int write, unsigned int resident, unsigned int reference, unsigned int dirty, disk_block_t & cur_db, unsigned int ppage, bool isUpdatePPG);

/* Find_place_in_PM
 * REQUIRES: disk_block_t that will be put into physical memory
 * MODIFIES: clock_queue; free_physical_pages; ppage_to_block_map; process_map; vm_physmem
 * EFFECTS:  if there are free physical pages, pop one; else, using clock algorithm to evict one physical page 
 */
unsigned int Find_place_in_PM(disk_block_t replace_block);

/* Find_filename
 * REQUIRES: filename_addr in virtual memory
 * MODIFIES: clock_queue; free_physical_pages; ppage_to_block_map; process_map; vm_physmem
 * EFFECTS:  find actual filename using the filename_addr; if not read enable, call vm_fault
 */
std::string Find_filename(const char* filename_addr);

/* RemoveFromClockQueue
 * REQUIRES: dirty_ppage that we want to remove from queue
 * MODIFIES: clock_queue
 * EFFECTS:  if ppage is removed from physical memory not caused by clock algorithm(e.g. process destroy), 
 *           we remove the ppage from clock_queue
 */
void RemoveFromClockQueue(unsigned int dirty_ppage);

/* FilenameToChar
 * REQUIRES: filename of string type
 * MODIFIES: 
 * EFFECTS:  convert string type filename to char*; if filename == "", return nullptr
 */
const char * FilenameToChar(std::string filename);



#endif /* _VM_HELPER_H_ */
