
#include"Master.h"


void *event_loop(void* argv)
{
    struct session* tmp = (session*)argv;
    int listen = create_tcp_socket(tmp->port);
    HandleConnection(listen, tmp->side) ;
    pthread_exit(nullptr);
} 

int main()
{
   pthread_t threadID;
  
    session client, server;
    client.port=9999;
    client.side=clientSide;
    pthread_create(&threadID, nullptr,event_loop, &client ); 

    server.port=12345;
    server.side = serverSide;
    event_loop(&server); 

   return 0;
}