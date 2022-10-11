#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "global.h"
#include "helper.h"
#include "filesys.h"

void Create_server(int port_number);

request_t Receive_message(int ClientFD);

void Send_message(int ClientFD, request_t client_request);

void Thread_running(int ClientFD);



#endif /* _SOCKET_H_ */