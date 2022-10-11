#include "filesys.h"

extern const int listen_queue_length;
extern const int max_message_length;
extern const int max_send_message_length;
extern bool disk_block[FS_DISKSIZE];
extern std::mutex disk_block_lock[FS_DISKSIZE];
extern std::mutex free_block_lock;


/*
 * Filesystem_init() preprocess the existing file system, and set all the currently used disk blocks not free
 */
void Filesystem_init(){
    Set_disk_block_status(0, false); /*** Disk block 0 is the root_inode and it is never free ***/
    for (uint32_t i = 1 ; i < FS_DISKSIZE; i++) {
        Set_disk_block_status(i, true);
    }
    std::queue<uint32_t> used_direntry;
    struct fs_inode root_inode;
    disk_readblock(0, &root_inode);
    for (uint32_t i = 0; i < root_inode.size; i++) {
        used_direntry.push(root_inode.blocks[i]);
        Set_disk_block_status(root_inode.blocks[i], false);
    }
    while (!used_direntry.empty()){
        uint32_t curr_direntry = used_direntry.front();
        used_direntry.pop();
        direntry_node_t dire_node;
        disk_readblock(curr_direntry, &dire_node.directory);
        for (uint32_t i = 0 ; i < FS_DIRENTRIES; i++) {
            uint32_t curr_inode_id = dire_node.directory[i].inode_block;
            if (curr_inode_id == 0){
                /*** The direntory is not used ***/
                continue;
            }
            Set_disk_block_status(curr_inode_id, false);
            struct fs_inode curr_inode;
            disk_readblock(curr_inode_id, &curr_inode);

            if (curr_inode.type == 'f'){
                for (uint32_t i = 0; i < curr_inode.size; i++) {
                    Set_disk_block_status(curr_inode.blocks[i], false);
                }
            }
            else if (curr_inode.type == 'd'){
                for (uint32_t i = 0; i < curr_inode.size; i++) {
                    Set_disk_block_status(curr_inode.blocks[i], false);
                    used_direntry.push(curr_inode.blocks[i]);
                }
            }
        }
    }
}

/* 
 *  This function will serve the client request type READBLOCK
 */
void ReadBlock_helper(request_t &client_request){
    TestPrint("---------- Read Begin ---------- ", client_request.block);
    std::unique_lock<std::mutex> curr_mutex(disk_block_lock[0]);
    uint32_t target_inode_id = Find_target_inode(client_request, curr_mutex);
    fs_inode target_inode;
    disk_readblock(target_inode_id, &target_inode);
    CheckUserValid(target_inode, client_request.username);
    CheckInodeType(target_inode, 'f');
    CheckBlockOverflow(target_inode, client_request.block);
    disk_readblock(target_inode.blocks[client_request.block], client_request.data);
    TestPrint("---------- Read End ---------- ", client_request.block);
}

/* 
 *  This function will serve the client request type WRITEBLOCK
 */
void WriteBlock_helper(request_t &client_request){
    TestPrint("---------- Write Begin ---------- ", client_request.block);
    std::unique_lock<std::mutex> curr_mutex(disk_block_lock[0]);
    uint32_t target_inode_id = Find_target_inode(client_request, curr_mutex);
    fs_inode target_inode;
    disk_readblock(target_inode_id, &target_inode);
    uint32_t write_disk_block;
    char data[FS_BLOCKSIZE];
    CheckUserValid(target_inode, client_request.username);
    CheckInodeType(target_inode, 'f');
    if (client_request.block < target_inode.size){ 
        /*** We write to an existing block ***/
        write_disk_block = target_inode.blocks[client_request.block];
        memcpy(data, client_request.data, FS_BLOCKSIZE);
        CheckBlockOverflow(target_inode, client_request.block);
        disk_writeblock(write_disk_block, data);
    }
    else { 
        /*** We create a block immediately after the current end of the file ***/
        if (target_inode.size == FS_MAXFILEBLOCKS) throw SysError("File blocks are maximal");
        target_inode.size ++;
        CheckBlockOverflow(target_inode, client_request.block);
        write_disk_block = Find_free_disk_block();
        target_inode.blocks[client_request.block] = write_disk_block;
        memcpy(data, client_request.data, FS_BLOCKSIZE);
        disk_writeblock(write_disk_block, data);
        disk_writeblock(target_inode_id, &target_inode);
    }
    TestPrint("---------- Write End ---------- ", client_request.block);
}

/* 
 *  This function will serve the client request type CREATE. 
 *  If CREATE fails, the original occupied disk block would be set free
 */
void Create_attempt(request_t &client_request, uint32_t free_inode){
    std::vector<std::string> filename_set = Pathname_Parsing(client_request.pathname);
    std::string filename = filename_set[filename_set.size()-1];
    std::unique_lock<std::mutex> curr_mutex(disk_block_lock[0]);
    uint32_t target_inode_id = Find_target_inode(client_request, curr_mutex);
    fs_inode target_inode;
    disk_readblock(target_inode_id, &target_inode);
    CheckUserValid(target_inode, client_request.username);
    CheckInodeType(target_inode, 'd');
    direntry_node_t dire_node;
    direntry_node_t target_dire_node;
    bool if_created = false;
    uint32_t dire_block_node = 0;   // disk block id for the fs_dire
    unsigned int dire_index = 0;    // index for the position in fs_dire
    TestPrint("---------- Target Inode Size ---------- ", target_inode.size);
    for (uint32_t i = 0; i < target_inode.size; i++) {
        disk_readblock(target_inode.blocks[i], &dire_node.directory);
        for (unsigned int j = 0; j < FS_DIRENTRIES; j++) {
            if (dire_node.directory[j].inode_block != 0 && std::string(dire_node.directory[j].name) == filename) throw SysError("Cannot create since filename already exist in the path");
            if (!if_created && dire_node.directory[j].inode_block == 0) {
                dire_block_node = target_inode.blocks[i];
                dire_index = j;
                if_created = true;
                for (unsigned int k = 0; k < FS_DIRENTRIES; k++) {
                    target_dire_node.directory[k] = dire_node.directory[k];
                }
            }
        }
    }
    if (!if_created){
        if (target_inode.size == FS_MAXFILEBLOCKS) throw SysError("No more file blocks for the directory");
        uint32_t free_direntry = Find_free_disk_block();
        for (unsigned int i=0; i<FS_DIRENTRIES; i++) {
            target_dire_node.directory[i].inode_block = 0;
        }
        target_inode.blocks[target_inode.size] = free_direntry;
        target_inode.size++;
        dire_block_node = free_direntry;
        dire_index = 0;
    }
    /*** Create a new inode ***/
    fs_inode new_inode;
    new_inode.type = client_request.type;
    strcpy(new_inode.owner, client_request.username.c_str());
    new_inode.size = 0;
    /*** Create a new direntory node ***/
    target_dire_node.directory[dire_index].inode_block = free_inode;
    strcpy(target_dire_node.directory[dire_index].name, filename.c_str());
    std::unique_lock<std::mutex> create_mutex(disk_block_lock[free_inode]);
    disk_writeblock(free_inode, &new_inode);
    disk_writeblock(dire_block_node, &target_dire_node.directory);
    if (!if_created) {
        disk_writeblock(target_inode_id, &target_inode);
    }
}

/* 
 *  This function will serve the client request type CREATE. 
 *  We first set one free disk block and call Create_attempt.
 *  If attempt fails, we free the occupied disk block
 */
void Create_helper(request_t &client_request){
    TestPrint("---------- Create Begin ---------- ", 0);
    uint32_t free_inode = Find_free_disk_block();
    try{
        Create_attempt(client_request, free_inode);
    }
    catch(SysError e){
        Set_disk_block_status(free_inode, true);
        throw e;
    }
    TestPrint("---------- Create End ---------- ", 0);
}

/* 
 *  This function will serve the client request type DELETE
 *  If we can find a file to delete, the function will return true
 *  Otherwise, return false
 */
bool Delete_attempt(request_t &client_request, uint32_t i, int target_inode_id, fs_inode target_inode, std::string filename){
    direntry_node_t dire_node;
    disk_readblock(target_inode.blocks[i], &dire_node.directory);
    for (unsigned int j = 0; j < FS_DIRENTRIES; j++) {
        if (dire_node.directory[j].inode_block != 0 && std::string(dire_node.directory[j].name) == filename) {
            uint32_t delete_inode_id = dire_node.directory[j].inode_block;
            fs_inode delete_inode;
            std::unique_lock<std::mutex> delete_mutex(disk_block_lock[delete_inode_id]);
            disk_readblock(delete_inode_id, &delete_inode);
            CheckUserValid(delete_inode, client_request.username);

            if (delete_inode.type == 'd' && delete_inode.size > 0) throw SysError("Cannot delete non-empty directory");
            if (delete_inode.type == 'f') {
                for (uint32_t k = 0; k < delete_inode.size; k++) {
                    Set_disk_block_status(delete_inode.blocks[k], true);
                }
            }
            dire_node.directory[j].inode_block = 0;   
            Set_disk_block_status(delete_inode_id, true);
            /*** Determine whether the current direntry is empty ***/
            for (unsigned int k = 0; k < FS_DIRENTRIES; k++) {
                if (dire_node.directory[k].inode_block != 0) {
                    /*** The direntry is not empty, we write the direntry back to disk and return ***/
                    disk_writeblock(target_inode.blocks[i], &dire_node.directory);
                    return true;
                }
            }
            /*** The direntry is empty, we set the disk block to free, and move the later direntries forward ***/
            Set_disk_block_status(target_inode.blocks[i], true);
            for (uint32_t k = i + 1; k < target_inode.size; k++){
                target_inode.blocks[k - 1] = target_inode.blocks[k];
            }
            target_inode.size--;
            disk_writeblock(target_inode_id, &target_inode);
            return true;
        }
    }
    return false;
}


/* 
 *  This function will serve the client request type DELETE
 *  We call Delete_attempt() to determine whether we can delete the file
 */
void Delete_helper(request_t &client_request){
    TestPrint("---------- Delete Begin ---------- ", 0);
    if (client_request.pathname == "/") throw SysError("Cannot delete root node");
    std::vector<std::string> filename_set = Pathname_Parsing(client_request.pathname);
    std::string filename = filename_set[filename_set.size()-1];
    std::unique_lock<std::mutex> curr_mutex(disk_block_lock[0]);
    uint32_t target_inode_id = Find_target_inode(client_request, curr_mutex);
    fs_inode target_inode; // target inode is the dir for the to be deleted dir/file
    disk_readblock(target_inode_id, &target_inode);
    CheckUserValid(target_inode, client_request.username);
    CheckInodeType(target_inode, 'd');
    TestPrint("---------- Target Inode Size ---------- ", target_inode.size);
    for (uint32_t i = 0; i < target_inode.size; i++) {
        if (Delete_attempt(client_request, i, target_inode_id, target_inode, filename)) {
            TestPrint("---------- Delete End ---------- ", 0);
            return;
        }
    }

    throw SysError("Not find delete file path!");
}