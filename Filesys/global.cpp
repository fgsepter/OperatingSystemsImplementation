#include "global.h"

const int listen_queue_length = 30;         // a queue length of 30 is sufficient
const int max_message_length = 30 + FS_MAXPATHNAME + FS_MAXUSERNAME; // max possible message length received
const int max_send_message_length = max_message_length + 1 + FS_BLOCKSIZE; // max possible message length send
bool disk_block[FS_DISKSIZE];               // true if the disk block is free, false otherwise
std::mutex disk_block_lock[FS_DISKSIZE];    // mutex array for each disk block
std::mutex free_block_lock;                 // mutex for disk_block status
const bool test_mode = false;               // set true to print testing output


SysError::SysError(std::string error_name){
    error = error_name;
}
