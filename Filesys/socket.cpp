#include "socket.h"

extern const int listen_queue_length;
extern const int max_message_length;
extern const int max_send_message_length;
extern bool disk_block[FS_DISKSIZE];
extern std::mutex disk_block_lock[FS_DISKSIZE];
extern std::mutex free_block_lock;


/*  
 *  Create a server socket specificed by the port_number
 *  If fails, throw a SysError
 */
void Create_server(int port_number){
    struct sockaddr_in server;
    int SocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (SocketFD == -1) {
        throw SysError("cannot create socket");
    }
    int val = 1;
	if (setsockopt(SocketFD, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
		throw SysError("cannot set socket options");
	}
    server.sin_family = AF_INET;
    server.sin_port = htons(port_number);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(SocketFD,(sockaddr *) &server, sizeof(server)) == -1) {
        throw SysError("bind failed");
    }
    socklen_t length = sizeof(server);
    if (getsockname(SocketFD, (struct sockaddr *) &server, &length) == -1) {
        throw SysError("error getting socket name");
    }
    port_number = ntohs(server.sin_port);
    if (listen(SocketFD, listen_queue_length) == -1) {
        throw SysError("listen failed");
    }
    cout_lock.lock();
    std::cout << "\n@@@ port " << port_number << std::endl;
    cout_lock.unlock();
    while (true) {
        int ConnectFD = accept(SocketFD, 0, 0);
        if (ConnectFD == -1) {
            throw SysError("accept failed");
        }
        std::thread client_thread(Thread_running, ConnectFD);
        client_thread.detach();   
    }
}

/*  
 *  Receive message from client, and parse the message into a request_t type 
 *  which contains all the information about the request
 */
request_t Receive_message(int ClientFD){
    std::string message;
    /*** Receive message and data from client ***/
    while(true){
        char buf[1];
        buf[0] = '1';
        int n = recv(ClientFD, buf, 1, 0); 
        if (buf[0] == '\0') break;
        message += buf[0];
        if (n <= 0) throw SysError("Receive Fails! Return value <= 0");
        if ((int)message.length() > max_message_length) throw SysError("Message too long");
    } 
    request_t client_request = Message_Parsing(message);
    /*** If request type is WRITE, we also need to receive the data ***/
    if (client_request.request_type == WRITE) {
        memset(client_request.data, 0, FS_BLOCKSIZE);
        int n = recv(ClientFD, client_request.data, FS_BLOCKSIZE, MSG_WAITALL);
        if (n <= 0) throw SysError("Receive Fails! Return value <= 0");
        else if (n < (int)FS_BLOCKSIZE) throw SysError("Receive Fails! Less than BLOCKSIZE");
        else if (n > (int)FS_BLOCKSIZE) throw SysError("Receive Fails! More than BLOCKSIZE");
    }
    return client_request;
}

/*  
 *  Send back message from the server to the client
 */
void Send_message(int ClientFD, request_t client_request){
    TestPrint("---------- Begin Sending Message ---------- ", ClientFD);
    char message[max_send_message_length];
    std::string message_str;
    size_t length;

    if (client_request.request_type == READ){
        std::string block = std::to_string(client_request.block);
        message_str = "FS_READBLOCK " + client_request.username + " " + client_request.pathname + " " + block;
        length =  message_str.length() + 1;
        strcpy(message, message_str.c_str());
        message[length - 1] = '\0';
        memcpy(message + length, client_request.data, FS_BLOCKSIZE);
        length += FS_BLOCKSIZE;
    }
    else if (client_request.request_type == WRITE){
        std::string block = std::to_string(client_request.block);
        message_str = "FS_WRITEBLOCK " + client_request.username + " " + client_request.pathname + " " + block;
        length =  message_str.length() + 1;
        strcpy(message, message_str.c_str());
        message[length - 1] = '\0';
    }
    else if (client_request.request_type == CREATE){
        message_str = "FS_CREATE " + client_request.username + " " + client_request.pathname + " " + client_request.type;
        length =  message_str.length() + 1;
        strcpy(message, message_str.c_str());
        message[length - 1] = '\0';
    }
    else if (client_request.request_type == DELETE){
        message_str = "FS_DELETE " + client_request.username + " " + client_request.pathname;
        length =  message_str.length() + 1;
        strcpy(message, message_str.c_str());
        message[length - 1] = '\0';
    }
    else {
        TestPrint("---------- Invalid Request Type ---------- ", 0);
        throw SysError("Invalid Request Type");
    }

    send(ClientFD, message, length, MSG_NOSIGNAL);
    TestPrint("---------- Stop Sending Message ---------- ", ClientFD);
}

/*  
 *  Main thread that will handle the request from the server. 
 *  If any error is catched, we close the client socket.
 */
void Thread_running(int ClientFD){
    TestPrint("---------- Thread Begin Running ---------- ", ClientFD);
    try{
        request_t client_request = Receive_message(ClientFD);
        if (client_request.request_type == READ) {
            ReadBlock_helper(client_request);
        }
        else if (client_request.request_type == WRITE) {
            WriteBlock_helper(client_request);
        }
        else if (client_request.request_type == CREATE) {
            Create_helper(client_request);
        }
        else if (client_request.request_type == DELETE) {
            Delete_helper(client_request);
        }
        Send_message(ClientFD, client_request);
    }
    catch (...){
        TestPrint("Error Catched", 0);
    }
    close(ClientFD);
    TestPrint("---------- Thread Stop Running ---------- ", ClientFD);
}