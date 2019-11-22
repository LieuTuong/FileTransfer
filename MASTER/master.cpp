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
#include<sstream>
#include<fstream>
#include<netdb.h>
#include<pthread.h>

#define MAXBUFSIZE 1024

pthread_mutex_t m =  PTHREAD_MUTEX_INITIALIZER;
#define clientSide 1
#define serverSide 2
std::string MasterDatabase = "MASTER_DATABASE.txt";


std::string deleteWhiteSpace (std::string str)
{
    int i;
    int begin=0, end=str.length()-1;
    
    while((str[begin]==' '||str[begin]=='\n'||str[begin]=='\t'||str[begin]=='\a'||str[begin]=='\r')&& (begin<end))
    {
        begin++;
    }
    while((end>=begin) && (str[end]==' '||str[end]=='\n'||str[end]=='\t'||str[end]=='\a'||str[end]=='\r'))
    {
        end--;
    }
    std::string ret = str.erase(0,begin);
    ret.erase(end+1,str.length()-end)   ;
    return ret;
}

std::string preSaveData_config(int sockfd,char *buffer, int byteRecv)
{
    
    std::string tmp;
    std::string delimStart("###S ");
    std::string delimEnd("###E ");
    
    // get the address 
    sockaddr_in remoteAddr;
    socklen_t remoteAddrSize = sizeof(remoteAddr);
    getpeername(sockfd, (sockaddr*)&remoteAddr, &remoteAddrSize);
    
    // get string(IP)
    char remote_ip [NI_MAXHOST];
    inet_ntop(AF_INET,&remoteAddr.sin_addr,remote_ip,NI_MAXHOST);

    tmp.append(delimStart.append(std::string(remote_ip).append("\n")));   // ###S
    tmp.append(std::string(buffer,byteRecv));
    tmp.erase(tmp.length()-1);  // erase "\n" in char*
    tmp.append("\n");
    tmp.append(delimEnd.append(std::string(remote_ip).append("\n")));     // ###E
    return tmp;
}


int create_tcp_socket(int port)
{
    int enable =1;
    //create socket
    int listening_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    //make the socket reusable
    setsockopt(listening_sock, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT,&enable,sizeof(enable));
    

    /*************************************************************/
   /* Set socket to be nonblocking. All of the sockets for      */
   /* the incoming connections will also be nonblocking since   */
   /* they will inherit that state from the listening socket.   */
   /*************************************************************/
    if (ioctl(listening_sock, FIONBIO, (char *)&enable) < 0)
    {
      std::cerr<<"ioctl() failed"<<std::endl;
      close(listening_sock);
      return EXIT_FAILURE;
   }

    //bind
    sockaddr_in hint;
    bzero((char*)&hint, sizeof(hint));
    hint.sin_family=AF_INET;
    hint.sin_port=htons(port);
    inet_pton(AF_INET,"0.0.0.0",&hint.sin_addr);
    bind(listening_sock, (sockaddr*)&hint, sizeof(hint));
    //listen
    if (listen(listening_sock, SOMAXCONN)< 0)
    {
        std::cerr<<"create_tcp_socket failed: can't bind socket"<<std::endl;
        return EXIT_FAILURE;
    }

    return listening_sock; 
    
}

// find in MASTER database whether the file that clients want store in buffer exists or not
// if it's does, send the IP and port of the server that hold that file to client
// if no, send announcement message to client
int client_handle(int sockfd, char *buffer, int byteRecv)
{
    bool fileFound = false;
    std::string buf="";
    std::string clientFileName = std::string(buffer, byteRecv - 1);
    
    std::ifstream readFile(MasterDatabase);

    if (readFile.fail())
    {
        std::cerr << "open() in client_handle failed" << std::endl;
        return EXIT_FAILURE;
    }   
    
    while (std::getline(readFile, buf)) 
    {
        if (buf.substr(0, buf.find(",")) == clientFileName) 
        {
            fileFound = true;
            break;    
        }             
    }   
    
    // Neu khong tim thay file
    if ( !fileFound)
    {
        buf = "NO SUCH FILE!!";
    }
    if (send (sockfd, buf.c_str(), buf.length()+1, 0) < 0)
    {
        std::cerr<< "client_handle send() failed" <<std::endl;
        return EXIT_FAILURE;
    }  
   
    readFile.close();
    return EXIT_SUCCESS;  

}


// store the file that server send(fileName, IP, port) to MASTER database
int server_handle(int sockfd, char *buffer, int byteRecv)
{
  
    int write_fd = open(MasterDatabase.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0775);
    if (write_fd < 0)
    {
        std::cerr << " server_handle open() failes" <<std::endl;
        return EXIT_FAILURE;
    }
    std::string tmp = preSaveData_config (sockfd, buffer, byteRecv);
    if (write(write_fd, tmp.c_str(), tmp.length()) < 0)
    {
        std::cerr<< "server_handle write() failed" <<std::endl;
        return EXIT_FAILURE;
    }
    close(write_fd);
    return EXIT_SUCCESS;
}

std::string get_IP_string (int sockfd)
{
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    getpeername(sockfd,(sockaddr*)&addr,&addrLen );
    char remote_ip [NI_MAXSERV];
    inet_ntop(AF_INET,&addr.sin_addr,remote_ip,NI_MAXHOST);
    return std::string(remote_ip);
}

int event_server_disconnect_handle ( int sockfd  )
{
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    getpeername(sockfd,(sockaddr*)&addr,&addrLen );
    std::string remote_ip = get_IP_string (sockfd);
    std::string start_ip ("###S ");
    std::string end_ip("###E ");
    start_ip.append(remote_ip);       // start_ip = ###S "IP"
    end_ip.append(remote_ip);         //  end_ip = ###E "IP" 


    std::ifstream old_file(MasterDatabase);
    if (old_file.fail())
    {
        std::cerr << "server_Disconnect_Handle open old file failed" << std::endl;
        return -1;
    }

    std::ofstream new_file("tmp.txt");
    if (new_file.fail())
    {
         std::cerr << "server_Disconnect_Handle open new file failed" << std::endl;
        return -1;
    } 

    std::string lineS, lineE;
    
    while(std::getline(old_file, lineS))
    {
        lineS=deleteWhiteSpace(lineS);
        if (lineS == start_ip)
        {          
            while(getline(old_file,lineE))
            {
                lineE=deleteWhiteSpace(lineE);              
                if (lineE == end_ip)break ;              
            }           
        }
        else
        {
            new_file << lineS.append("\n");   
        }         
    }
   
    old_file.close();
    new_file.close();
    remove(MasterDatabase.c_str());
    rename("tmp.txt",MasterDatabase.c_str());
    return 0;
}

int HandleConnection (int listen_sockfd, int socketSide)
{
   int    i, len, rc;
   int    max_fd, new_fd;
   int    desc_ready;
   int    close_conn;
   char   buffer[MAXBUFSIZE];
   fd_set    master_set, working_set;
   
   
   FD_ZERO(&master_set);
   max_fd = listen_sockfd;
   FD_SET(listen_sockfd, &master_set);

   while(1)
   {
       memcpy(&working_set, &master_set, sizeof(master_set));

       std::cout<<"\nWaiting on select()..."<<std::endl;
       rc = select(max_fd + 1, &working_set, nullptr, nullptr, nullptr);
       if (rc < 0)
       {
           std::cerr<<"select() failed"<<std::endl;
           continue;
       }

        /* One or more descriptors are readable.  Need to         */
        /* determine which ones they are.                         */

        desc_ready = rc ; 
        for (i=0; i<=max_fd && desc_ready > 0; ++i)
        {
            //check to see if this descriptor is ready
            if (FD_ISSET(i, &working_set))
            {
             /* A descriptor was found that was readable - one   
                less has to be looked for.  This is being done   
                so that we can stop looking at the working set   
                once we have found all of the descriptors that   
                were ready.                                      */
                desc_ready -= 1;

                //check to see if this is the listening socket
                if (i == listen_sockfd)
                {                    
                    do
                    {
                        if (socketSide == clientSide)
                        {
                            std::cout<<"New connection from client side" <<std::endl;
                        }
                        else if (socketSide == serverSide)
                        {
                            std::cout<<"New connection from server side" << std::endl;
                        }
                        
                        new_fd = accept(listen_sockfd, nullptr, nullptr);
                        if (new_fd < 0)
                        {
                            if (errno != EWOULDBLOCK)
                            {
                                std::cerr << "accept() failed" <<std::endl;
                                break;
                            }
                        }
            
                        // add the new incoming connection to the master read set
                        FD_SET(new_fd, &master_set);
                        max_fd = new_fd > max_fd ? new_fd : max_fd ;

                        // loop back up and accept another incoming connection
                    } while (new_fd != -1); 
                
                } 

                // not the listening socket, therefore an existing connection must be readble
                else
                {
                    close_conn = false;
                    do
                    {
                        rc = recv(i, buffer, sizeof(buffer), 0);
                        if (rc < 0)
                        {
                            if ( errno != EWOULDBLOCK)
                            {
                                std::cerr<< "revc() failed";
                                close_conn = true;
                            }
                            break;
                        }

                        if (rc == 0)  //if the connection has been closed by the client
                        {
                            // when a server disconnected, delete the 
                            // part of that server file in DATABASE
                            if (socketSide == serverSide) 
                            {
                                std::cout<<"Server "<<get_IP_string(i)<<" is down"<< std::endl;
                                if(event_server_disconnect_handle(i)==0)
                                {
                                    std::cout<<"Deleted all file related to server "
                                             <<get_IP_string(i)<<std::endl;
                                }
                                else
                                {
                                    std::cout<<"Failed in delete down server file" << std::endl;
                                }
                       
                            }
                            // when a client close a connection
                            else
                            {
                                std::cout<<"Connection of client " 
                                         << get_IP_string(i) << " is closed" <<std::endl;
                            }
                            close_conn = true;
                            break;
                        }

                        //data was received
                        else
                        {
                            if (socketSide == clientSide)
                            {
                                client_handle(i, buffer, rc);
                            }
                            else // socketType == serverSide
                            {
                                // lock thread to adjust the DATABASE
                                pthread_mutex_lock(&m);      
                                server_handle(i,buffer,rc);
                                pthread_mutex_unlock(&m);
                            }
                            
                        }
                        
                    } while (1);
                
                    if (close_conn) // delete a close connection from fd_set 
                    {
                        close(i);
                        FD_CLR(i, &master_set);
                        if ( i == max_fd)
                        {
                            while ( FD_ISSET(max_fd, &master_set)== false)
                            {
                                max_fd -= 1;
                            }
                        }
                    }
               
                }
            
            }
        }
    }
    return 0;
}

 struct session
{
    int sockfd;
    int port;
    int side;
};

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