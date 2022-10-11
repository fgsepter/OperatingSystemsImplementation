#include "global.h"
#include "helper.h"
#include "socket.h"
#include "filesys.h"
  
/*
 *  This is the main function of the server
 *  We first init the file system, and then create the server to accept client
 */
int main(int argc, char *argv[])
{
    Filesystem_init();
    int port_number;
    port_number = (argc == 2)?atoi(argv[1]):0;
    try{
        Create_server(port_number);
    }
    catch(...){
        TestPrint("Error Catched", 0);
    }
    return 0;  
}