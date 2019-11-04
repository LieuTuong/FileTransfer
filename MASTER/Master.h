
#ifndef __MASTER_H__
#define __MASTER_H__
#include<iostream>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>     //socket(), listen(), accept(), setsockopt()
#include<arpa/inet.h>
#include<netinet/in.h>    // IPPROTO_TCP
#include<netinet/tcp.h>   // TCP_KEEPIDLE,...
#include<unistd.h>
#include<string>
#include<sys/stat.h>      //open
#include<fcntl.h>        //open
#include<sys/ioctl.h>
#include<fstream>
#include<netdb.h>
#include<pthread.h>

static pthread_mutex_t m =  PTHREAD_MUTEX_INITIALIZER;

static std::string MasterDatabase = "MASTER_DATABASE.txt";

#define MAXBUFSIZE 1024

#define clientSide 1
#define serverSide 2



 struct session
{
    int sockfd;
    int port;
    int side;
};



std::string deleteWhiteSpace (std::string str);

std::string get_IP_string (int sockfd);

std::string preSaveData_config(int sockfd,char *buffer, int byteRecv);


// create tcp listening socket
int create_tcp_socket(int port);


// Hanlde client connections
int client_handle(int sockfd, char *buffer, int byteRecv);


// Handle server connection
int server_handle(int sockfd, char *buffer, int byteRecv);

int event_server_disconnect_handle ( int sockfd  );


// Handle all connection from client and server
int HandleConnection (int listen_sockfd, int socketType);












#endif