#ifndef _struct_H_
#define _struct_H_

#include "fs_server.h"
#include "fs_client.h"
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <queue>
#include <vector>
#include <cassert>
#include <sstream>
#include <regex>

#define READ   0
#define WRITE  1
#define CREATE 2
#define DELETE 3

extern const int listen_queue_length;
extern const int max_message_length;
extern const int max_send_message_length;
extern bool disk_block[FS_DISKSIZE];
extern std::mutex disk_block_lock[FS_DISKSIZE];
extern std::mutex free_block_lock;
extern const bool test_mode;

struct direntry_node_t {
    fs_direntry directory[FS_DIRENTRIES];
};

struct request_t {
    int request_type;
    std::string username;
    std::string pathname;
    uint32_t block;
    char type;
    char data[FS_BLOCKSIZE];
};

class SysError {
private:
    std::string error;
public:
    SysError(std::string error_name);
};

#endif /* _struct_H_ */