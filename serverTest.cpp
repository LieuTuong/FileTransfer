#include <iostream>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include<sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include<fstream>
//#include<map>
#include <sstream>
#include <sys/time.h> 
#include<cmath>
#include<ctime>
#include<fcntl.h>
#include<pthread.h>

using namespace std;

#define MAX_CONNECTIONS 32
#define MAX_PAYLOAD_LENGTH 1450
typedef void* (*THREADFUNCPTR)(void*);

string masterIP = "127.0.0.1";
int masterPort = 12345;
string SERVER_DATABASE ="serverDatabase.txt";

class RUDPServer
{
	private:
	int server_TCP_sockfd;
	int server_UDP_sockfd;
   	socklen_t clientSockLen;
   	struct sockaddr_in serverAddr;
   	struct sockaddr_in clientAddr;
	struct sockaddr_in masterAddr;   
   	char buff[4096];
	int fileSize;	
	struct RUDP_DATA
	{
		uint32_t sequenceNumber;
		uint32_t ackNumber;
		bool ackFlag;
		char data[MAX_PAYLOAD_LENGTH];
	}segment;
	struct timeval sampleRTT,estimatedRTT,devRTT,timeoutInterval;
	public:
	void TCPsendtoMaster();
	void TCPmasterConnect();
  	void displayError(const char *errorMsg);	
	void createSocket();
	void bindAddress(int portNumber);
	void setClientSockLength();
	int receiveRequest();
	char* getRequestedContent();
	int getFileSize(const char *filename);	
	void createSegments(char *fileContent,int windowSize);
	void slidingWindow(RUDP_DATA *senderBuffer, int senderBufferLen,int windowSize);
	struct timeval calculateTimeout(struct timeval t1, struct timeval t2);	
	RUDPServer::RUDP_DATA setHeader(int seqNo, int ackNo, int flag, char *datagram);
	void sendSegment(RUDP_DATA seg);	
	RUDPServer::RUDP_DATA receiveAck();	
};


void RUDPServer::TCPsendtoMaster()
{
    int read_fd = open (SERVER_DATABASE.c_str(), O_RDONLY);
    if (read_fd < 0)
    {
        std::cerr <<"send_tcp_file open() failed" <<std::endl;
        return ;
    }
    int sendBytes, readBytes;
    char buf[10000];
   
    
        readBytes = read(read_fd, buf,10000);
        if (readBytes < 0)
        {
			displayError("send_tcp_file read() failed");
        }

		
        if( send(server_TCP_sockfd, buf,readBytes,0) < 0)
        {
			displayError("send_tcp_file send() failed");
        }    
}

void RUDPServer::TCPmasterConnect()
{
	server_TCP_sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (server_TCP_sockfd < 0)
    {
        std::cerr<<"master_connect socket() failed !"<<std::endl;
        exit(1);
    }

    masterAddr.sin_family = AF_INET;
    masterAddr.sin_port = htons(masterPort);
    inet_pton(AF_INET,masterIP.c_str(),&masterAddr.sin_addr);

    if (connect(server_TCP_sockfd,(struct sockaddr*)&masterAddr, sizeof(masterAddr)) < 0)
    {
        std::cerr<<"master_connect connect() failed !"<<std::endl;
        exit(1);
    }

    char buff[2048];
    std::string input;
    while(1)
    {
        do
        {
            std::cout<<"\nserver > ";
            std::getline(std::cin, input);
            if (input == "send")
            {
                TCPsendtoMaster();
            }           
        } while (input != "send");  
        
    }
}
void RUDPServer::displayError(const char *errorMsg)
{
	cerr<<"Error: "<<errorMsg<<endl;
	exit(1);
}


void RUDPServer::createSocket()
{
	server_UDP_sockfd=socket(AF_INET, SOCK_DGRAM, 0);
   	if (server_UDP_sockfd < 0) 
	{
		displayError("The server socket could not be opened!");
	}
}


void RUDPServer::bindAddress(int portNumber)
{
	int leng=sizeof(serverAddr);
	bzero(&serverAddr,leng);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(portNumber);     	
	serverAddr.sin_addr.s_addr = INADDR_ANY;
     	
	if (bind(server_UDP_sockfd, (struct sockaddr *) &serverAddr,leng) < 0) 
  	{      
		displayError("There is some problem while binding the server socket to an address!");
	}
}

void RUDPServer::setClientSockLength()
{
	clientSockLen = sizeof(struct sockaddr_in);
}


int RUDPServer::receiveRequest()
{
    int noOfCharacters = recvfrom(server_UDP_sockfd,&segment,sizeof(segment),0,(struct sockaddr *)&clientAddr,&clientSockLen);	       	
	if (noOfCharacters < 0) 
	{
		displayError("There is some problem in receiving the request!");
	}
	return noOfCharacters;
}


int RUDPServer::getFileSize(const char *filename)
{
    ifstream file;
    file.open(filename, ios_base::binary);
    file.seekg(0,ios_base::end);
    int size = file.tellg();
    file.close();
    return size;
}


char* RUDPServer::getRequestedContent()
{
	fileSize=getFileSize(segment.data);
	char *fileContent;
	fileContent=new char[fileSize];	
	ifstream readFile;
	readFile.open(segment.data);		
	if(readFile.is_open())
	{
		readFile.read(fileContent, fileSize);
	}
	else
	{
		displayError("File Not Found");
	}
	return fileContent; 	
}


void RUDPServer::createSegments(char *fileContent,int windowSize)
{
	int noOfSegments=fileSize/MAX_PAYLOAD_LENGTH;
	char *seg=new char[MAX_PAYLOAD_LENGTH];	
	int seqNo=0;
	int ackNo=segment.sequenceNumber+1;
	int ackFlag=0;
	int i,j,k;
	int senderBufferLen=noOfSegments+1;
	RUDP_DATA *senderBuffer=new RUDP_DATA[senderBufferLen];
	for(j=0;j<noOfSegments;j++)
	{
		for(i=j*MAX_PAYLOAD_LENGTH, k=0;i<(j+1)*MAX_PAYLOAD_LENGTH && k<MAX_PAYLOAD_LENGTH;i++,k++)
		{
			seg[k]=fileContent[i];
		}
		senderBuffer[j]=setHeader(seqNo,ackNo,ackFlag,seg);
		seqNo++;
	}
	int rem=fileSize%MAX_PAYLOAD_LENGTH;
	for( int s=0;s<rem;s++)
	{
		seg[s]=fileContent[++i];
	}
	senderBuffer[j]=setHeader(seqNo,ackNo,ackFlag,seg);
	slidingWindow(senderBuffer,senderBufferLen,windowSize);	
}


void RUDPServer::slidingWindow(RUDP_DATA *senderBuffer, int senderBufferLen,int windowSize)
{
	int firstUnAck=0,nxtSeqNo=0,dupAckCnt;
	RUDP_DATA ack;
	cout<<"No of segments to be sent: "<<senderBufferLen<<endl;
	int cwnd=1;
	int ssthresh=64000;
	int segmentSize=sizeof(senderBuffer[0]);
	int noOfSegmentsInWin=windowSize/segmentSize;
	struct timeval t1, t2;
	cout<<"No of segments in window "<<noOfSegmentsInWin<<endl;
	int dropPercent=60;
	int noOfPacketsToDrop=(dropPercent*noOfSegmentsInWin)/100;
	cout<<"No of packets to drop "<<noOfPacketsToDrop<<endl;
	int *packetsToDrop=new int[noOfPacketsToDrop];
	estimatedRTT.tv_sec=0;
	estimatedRTT.tv_usec=0;
	devRTT.tv_sec=0;
	devRTT.tv_usec=0;
	timeoutInterval.tv_sec=2;
	timeoutInterval.tv_usec=0;
	fd_set fds;
	int val=1;
	while(nxtSeqNo<senderBufferLen)
	{
		
		if(firstUnAck==0 && nxtSeqNo==0)
		{
			srand(time(NULL));
			for(int i=0;i<noOfPacketsToDrop;i++)
			{
				
				packetsToDrop[i]=rand()%(noOfSegmentsInWin-1)+1;
				cout<<"Sequence numbers to drop "<<packetsToDrop[i]<<endl;
			}
		}
		int minimumSize=(cwnd<noOfSegmentsInWin)?cwnd:noOfSegmentsInWin;		
		gettimeofday(&t1, NULL);
		while(nxtSeqNo<firstUnAck+minimumSize && nxtSeqNo<senderBufferLen)
		{	
			if(nxtSeqNo==senderBufferLen-1)
			{
				senderBuffer[nxtSeqNo].ackFlag=1;
			}
			bool flag=true;
			for(int i=0;i<noOfPacketsToDrop;i++)
			{
				if(nxtSeqNo==packetsToDrop[i])
				{
					flag=false;
				}
			}
			if(flag)
			{
				
				sendSegment(senderBuffer[nxtSeqNo]);
			}	
			nxtSeqNo++;
		}
		dupAckCnt=0;
		FD_ZERO(&fds);
		FD_CLR(server_UDP_sockfd,&fds);
		FD_SET(server_UDP_sockfd,&fds);
		val=select(server_UDP_sockfd+1,&fds,NULL,NULL,&timeoutInterval);
		if(val==0)
		{
			ssthresh=(cwnd*segmentSize)/2;			
			cwnd=1;
			timeoutInterval.tv_sec=2*timeoutInterval.tv_sec;
			timeoutInterval.tv_usec=2*timeoutInterval.tv_usec;
			continue;
		}
		if(val==-1)
		{
			displayError("There is some problem in receiving the segment!");
		}
		if(FD_ISSET(server_UDP_sockfd,&fds) && val==1)
		{
			ack=receiveAck();
			gettimeofday(&t2, NULL);
			if(ack.ackNumber<nxtSeqNo)
			{
				dupAckCnt++;
				while(dupAckCnt<3)
				{
					ack=receiveAck();
					dupAckCnt++;
				}
				for(int i=0;i<noOfPacketsToDrop;i++)
				{
					if(ack.ackNumber==packetsToDrop[i])
					{
						packetsToDrop[i]=-1;
					}
				}
			}
			if(dupAckCnt<3 && minimumSize==cwnd)
			{
				if((cwnd*segmentSize)>=ssthresh)
				{
					cout<<"Congestion Avoidance "<<endl;
					cwnd=cwnd+1;
					cout<<"Next Sequence Number "<<ack.ackNumber<<endl;
				}
				else
				{
					cout<<"Slow Start "<<endl;
					cwnd=cwnd*2;
				}
				timeoutInterval=calculateTimeout(t1,t2);
			}
			firstUnAck=ack.ackNumber;
			nxtSeqNo=ack.ackNumber;			
		}
			
	}
	cout<<"File Sent Successfully "<<endl;
}


struct timeval RUDPServer::calculateTimeout(struct timeval t1, struct timeval t2)
{
	
	double alpha=0.125,beta=0.25;
	sampleRTT.tv_sec=t2.tv_sec-t1.tv_sec;
	sampleRTT.tv_usec=t2.tv_usec-t1.tv_usec;
       	estimatedRTT.tv_sec=((1-alpha)*estimatedRTT.tv_sec + (alpha*sampleRTT.tv_sec));
	estimatedRTT.tv_usec=((1-alpha)*estimatedRTT.tv_usec + (alpha*sampleRTT.tv_usec));
       	devRTT.tv_sec=((1-beta)*devRTT.tv_sec + beta*(abs(sampleRTT.tv_sec-estimatedRTT.tv_sec)));
	devRTT.tv_usec=((1-beta)*devRTT.tv_usec + beta*(abs(sampleRTT.tv_usec-estimatedRTT.tv_usec)));
       	timeoutInterval.tv_sec=estimatedRTT.tv_sec+4*devRTT.tv_sec;
	timeoutInterval.tv_usec=estimatedRTT.tv_usec+4*devRTT.tv_usec;
	return timeoutInterval;
}


RUDPServer::RUDP_DATA RUDPServer::setHeader(int seqNo, int ackNo, int flag, char *datagram)
{
	RUDP_DATA udpData;
	udpData.sequenceNumber=seqNo;
	udpData.ackNumber=ackNo;
	udpData.ackFlag=flag;
	strcpy(udpData.data,datagram);
	return udpData;
}


void RUDPServer::sendSegment(RUDP_DATA seg)
{
	cout<<"Sending packet with sequence number: "<<seg.sequenceNumber<<endl;
	int no=sendto(server_UDP_sockfd,&seg,sizeof(seg),0,(struct sockaddr *)&clientAddr,clientSockLen);
	if(no<0)
	{
		displayError("There is some problem in sending the segment!");
	}
}


RUDPServer::RUDP_DATA RUDPServer::receiveAck()
{
	RUDP_DATA ack;
	int no=recvfrom(server_UDP_sockfd,&ack,sizeof(ack),0,(struct sockaddr *)&clientAddr,&clientSockLen);
	if(no<0)
	{
		displayError("There is some problem in receiving the segment!");
	}
	cout<<"Received Acknowledgement "<<ack.ackNumber<<" for sequence number "<<ack.sequenceNumber<<endl;	
	return ack; 
}


template<class T, void(T::*mem_fn)()>
void* thunk(void* p)
{
(static_cast<T*>(p)->*mem_fn)();
return 0;
}

int main(/* int noOfArguments,char *argumentList[]*/)
{
	RUDPServer server ;
	int port = 6969;
	/*
         * It checks if all the command-line arguments are provided.
         */	
	/* if(noOfArguments<3)
	{
		server.displayError("The client must provide a port number!");
	} */
	/* server.createSocket();
	//int portNo=atoi(argumentList[1]);	
	server.bindAddress(port);
	server.setClientSockLength();	
	while(1)
	{
		server.receiveRequest();
		char* fileContent=server.getRequestedContent();
		server.createSegments(fileContent,4350);		
	} */

	// master handle
	pthread_t threadID;
	pthread_create(&threadID, nullptr,thunk<RUDPServer,&RUDPServer::TCPmasterConnect>,&server);


	//client handle
	server.createSocket();
	server.bindAddress(port);
	server.setClientSockLength();	
	while(1)
	{
		server.receiveRequest();
		char* fileContent=server.getRequestedContent();
		server.createSegments(fileContent,4350);		
	} 


	return 0;	
}

