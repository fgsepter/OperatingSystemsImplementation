#ifndef _HELPER_H_
#define _HELPER_H_

#include "global.h"

uint32_t Find_target_inode(request_t &client_request, std::unique_lock<std::mutex> &curr_mutex);

std::vector<std::string> Pathname_Parsing(std::string pathname);

int Find_Request_Type(std::string message);

uint32_t String_to_Int(std::string block);

request_t Message_Parsing(std::string message);

uint32_t Find_free_disk_block();

void Set_disk_block_status(uint32_t index, bool if_free);

void TestPrint(std::string test_output, size_t index);

void Check_Valid_Message(std::string message, request_t client_request);

void Check_Valid_Request(request_t request);

void CheckUserValid(fs_inode target_inode, std::string username);

void CheckBlockOverflow(fs_inode target_inode, uint32_t block);

void CheckInodeType(fs_inode target_inode, char type);


#endif /* _HELPER_H_ */