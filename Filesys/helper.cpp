#include "helper.h"

extern const int listen_queue_length;
extern const int max_message_length;
extern bool disk_block[FS_DISKSIZE];
extern std::mutex disk_block_lock[FS_DISKSIZE];
extern std::mutex free_block_lock;
extern const bool test_mode;

const std::regex READ_REG("^(FS_READBLOCK [^ \n\t\v\f\r]+ [^ \n\t\v\f\r]+ [0-9]+)$");
const std::regex WRITE_REG("^(FS_WRITEBLOCK [^ \n\t\v\f\r]+ [^ \n\t\v\f\r]+ [0-9]+)$");
const std::regex CREATE_REG("^(FS_CREATE [^ \n\t\v\f\r]+ [^ \n\t\v\f\r]+ [fd]+)$");
const std::regex DELETE_REG("^(FS_DELETE [^ \n\t\v\f\r]+ [^ \n\t\v\f\r]+)$");

/*
 *  Apply hand-over-hand locking
 *  Find the target disk block id of the inode indicated by pathname
 *  For READ/WRITE, we return the last inode
 *  For CREATE/DELETE, we return the second last inode
 */
uint32_t Find_target_inode(request_t &client_request, std::unique_lock<std::mutex> &curr_mutex){
    std::vector<std::string> filename_set = Pathname_Parsing(client_request.pathname);
    uint32_t curr_disk_block = 0;
    uint32_t next_disk_block = 0;   
    size_t target_depth = ((client_request.request_type == READ) || (client_request.request_type == WRITE))?filename_set.size():(filename_set.size() - 1);
    for (size_t i = 0; i < target_depth; i++) {
        fs_inode curr_inode;
        disk_readblock(curr_disk_block, &curr_inode);
        CheckUserValid(curr_inode, client_request.username);
        CheckInodeType(curr_inode, 'd');

        bool if_found = false;
        for (uint32_t j = 0; j < curr_inode.size; j++) {
            if (if_found) break;
            direntry_node_t dire_node;
            disk_readblock(curr_inode.blocks[j], &dire_node.directory);
            for (unsigned int k = 0; k < FS_DIRENTRIES; k++) {
                if (dire_node.directory[k].inode_block != 0 && std::string(dire_node.directory[k].name) == filename_set[i]) {
                    next_disk_block = dire_node.directory[k].inode_block;
                    if_found = true;
                    break;
                }
            }
        }
        if (next_disk_block == curr_disk_block) throw SysError("no next_disk_block");

        /*** Perform hand-over-hand locking ***/
        std::unique_lock<std::mutex> next_mutex(disk_block_lock[next_disk_block]);
        curr_mutex.swap(next_mutex);
        curr_disk_block = next_disk_block;
    }
    return curr_disk_block;
}

/*
 *  Find a set of filename or directory name of a given pathname
 */
std::vector<std::string> Pathname_Parsing(std::string pathname){
    if (pathname[0] != '/') throw SysError("Invalid Path");
    std::vector<std::string> filename_set;
    pathname += '/';
    std::string temp_filename = "";
    for (size_t i = 1; i < pathname.length(); i++) {
        if (pathname[i] == '/'){
            if (temp_filename == "" || temp_filename.length() > FS_MAXFILENAME) throw SysError("Invalid Path");
            filename_set.push_back(temp_filename);
            temp_filename = "";
        }
        else {
            temp_filename += pathname[i];
        }
    }
    return filename_set;
}

/*
 *  Find the request type of a given client message
 */
int Find_Request_Type(std::string message){
    if (std::regex_match(message, READ_REG))   return READ;
    if (std::regex_match(message, WRITE_REG))  return WRITE;
    if (std::regex_match(message, CREATE_REG)) return CREATE;
    if (std::regex_match(message, DELETE_REG)) return DELETE;
    throw SysError("Unkown Message Type");
}

/*
 *  Convert a string block to an int block
 *  Check if there is leading zeros in the string block
 */
uint32_t String_to_Int(std::string block){
    if (block.length() == 0) throw SysError("Empty block number");
    if (block[0] == '0' && block.length() > 1) throw SysError("Leading Zeros in Block");
    for (size_t i = 0 ; i < block.length(); i++){
        if ((block[i] < '0') || (block[i] > '9')) throw SysError("Invalid block number");
    }
    return atoi(block.c_str());
}

/* 
 *  This function process the string message and parse it into a request_t struct that contains all the useful information
 */
request_t Message_Parsing(std::string message){
    request_t request;
    request.block = 0;
    std::istringstream m_stream(message);
    std::string protocal_type;
    int message_type = Find_Request_Type(message);
    std::string block_string;
    std::string type_string;
    request.request_type = message_type;
    if ((message_type == READ) || (message_type == WRITE)){
        m_stream >> protocal_type >> request.username >> request.pathname >> block_string;
        request.block = String_to_Int(block_string);
    }
    else if (message_type == CREATE){
        m_stream >> protocal_type >> request.username >> request.pathname >> type_string;
        if (type_string == "d")      request.type = 'd';
        else if (type_string == "f") request.type = 'f';
        else throw SysError("Invalid type");
    }
    else if (message_type == DELETE){
        m_stream >> protocal_type >> request.username >> request.pathname;
    }
    else throw SysError("Unknow Request Type");
    Check_Valid_Message(message, request);
    Check_Valid_Request(request);
    return request;
}

uint32_t Find_free_disk_block(){
    std::unique_lock<std::mutex> m(free_block_lock); 
    for (uint32_t i = 0; i < FS_DISKSIZE; i++) {
        if (disk_block[i]){
            disk_block[i] = false;
            return i;
        }
    }
    throw SysError("No free disk blocks");
}

/* 
 *  This function set the diskblock status
 */
void Set_disk_block_status(uint32_t index, bool if_free){
    std::unique_lock<std::mutex> m(free_block_lock); 
    disk_block[index] = if_free;
}

/* 
 *  This function is a helper function to print the output for testing if needed
 */
void TestPrint(std::string test_output, size_t index){
    if (test_mode){
        cout_lock.lock();
        std::cout << test_output << index << std::endl;
        cout_lock.unlock();
    }
}

/* 
 *  This function checks whether the received message is valid
 */
void Check_Valid_Message(std::string message, request_t client_request){
    std::string message_str = "";
    std::string block = "";
    if (client_request.request_type == READ){
        block = std::to_string(client_request.block);
        message_str = "FS_READBLOCK " + client_request.username + " " + client_request.pathname + " " + block;
    }
    else if (client_request.request_type == WRITE){
        block = std::to_string(client_request.block);
        message_str = "FS_WRITEBLOCK " + client_request.username + " " + client_request.pathname + " " + block;
    }
    else if (client_request.request_type == CREATE){
        message_str = "FS_CREATE " + client_request.username + " " + client_request.pathname + " " + client_request.type;
    }
    else if (client_request.request_type == DELETE){
        message_str = "FS_DELETE " + client_request.username + " " + client_request.pathname;
    }
    if (message_str != message) throw SysError("Invalid Input Request Message!");

}

/* 
 *  This function checks whether the user request is valid
 */
void Check_Valid_Request(request_t request){
    if (request.block >= FS_MAXFILEBLOCKS) throw SysError("Block Overflow");
    if ((request.username.length() > FS_MAXUSERNAME) || (request.username.length() == 0)) throw SysError("Username Length Overflow");
    if ((request.pathname.length() > FS_MAXPATHNAME) || (request.username.length() == 0)) throw SysError("Pathname Length Overflow");
    if ((request.pathname[0] != '/') || (request.pathname[request.pathname.length()-1] == '/')) throw SysError("Pathname Not Valid");
}

/* 
 *  This function checks whether the user can access the path
 */
void CheckUserValid(fs_inode target_inode, std::string username){
    if (std::string(target_inode.owner) == ""){
        return;
    }
    if (std::string(target_inode.owner) != username){
        throw SysError("User has no permission");
    }
}

/* 
 *  This function checks whether the block is overflow
 */
void CheckBlockOverflow(fs_inode target_inode, uint32_t block){
    if (block >= target_inode.size){
        throw SysError("Block index overflow");
    }
}

/* 
 *  This function checks whether the inode is the correct type
 */
void CheckInodeType(fs_inode target_inode, char type){
    if (target_inode.type != type){
        throw SysError("Invalid inode type");
    }
}