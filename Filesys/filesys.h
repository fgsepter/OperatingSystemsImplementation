#ifndef _FILESYS_H_
#define _FILESYS_H_

#include "global.h"
#include "helper.h"

void Filesystem_init();

void ReadBlock_helper(request_t &client_request);

void WriteBlock_helper(request_t &client_request);

void Create_attempt(request_t &client_request, uint32_t free_inode);

void Create_helper(request_t &client_request);

bool Delete_attempt(request_t &client_request, uint32_t i, int target_inode_id, fs_inode target_inode, std::string filename);

void Delete_helper(request_t &client_request);



#endif /* _FILESYS_H_ */