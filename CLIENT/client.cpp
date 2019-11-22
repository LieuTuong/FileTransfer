#include <iostream>
#include<stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <string.h>
#include<fstream>
#include <sstream>
#include <signal.h>
#include<vector>
#include<pthread.h>
#include"crc.h"
using namespace std;
pthread_mutex_t lock;
std::string masterIP = "127.0.0.1";
int masterPort = 9999;

#define MAX_PAYLOAD_LENGTH 1450
std::string convertToString(char *a, int size)
{
    std::string res;
    for (int i=0;i<size;i++)
    {
        res = res+a[i];
    }
    return res;
}

bool timeout;
class RUDPClient
{
	private:
	int client_TCP_sockfd;
	int client_UDP_sockfd;
   	unsigned int len;
   	struct sockaddr_in serverAddr,clientAddr,masterAddr;
  	//struct hostent *host;
	struct RUDP_DATA
	{
		uint32_t seqNo;
		uint32_t ackNo;
		bool ackFlag;
		unsigned long crc;
		char data[MAX_PAYLOAD_LENGTH];
	}udpSegment;	

	std::vector <std::string> fileNameArr;
	int numOfGetFile;

	public:
	
	// handle master
	std::vector<std::string> master_connect();

	// hamdle server
	int getNumOfGetFile(){return numOfGetFile;}
	string getFileName(int i){return fileNameArr[i];}
	void setServerAddr(std::string masterResponse);
	void displayError(const char *errorMsg);
	void createSocket();
	void getServerInfo(char* hostname);
	void setserverAddr(int portNo);
	void createRequest(string filename);
	int sendRequest();
	void readResponse(int windowSize,char* outputFile);
	void sendAck(RUDP_DATA segment);
	void writeToFile(RUDP_DATA* response,int rcvBuffPos,char* filename);	
};

std::vector<std::string> RUDPClient::master_connect()
{
	std::vector<std::string> response;
    
    client_TCP_sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (client_TCP_sockfd < 0)
    {
        std::cerr<<"master_connect socket() failed !"<<std::endl;
        return response;
    }

    masterAddr.sin_family=AF_INET;
    masterAddr.sin_port=htons(masterPort);
    inet_pton(AF_INET,masterIP.c_str(),&masterAddr.sin_addr);

    if (connect(client_TCP_sockfd,(sockaddr*)&masterAddr,sizeof(masterAddr))<0)
    {
        std::cerr<<"master_connect connect() failed !"<<std::endl;
        return response;
    }

    char buf[2048];
    std::string userInput="";
    
	std::cout<<"Number of file you want: ";
	cin >> numOfGetFile;
	int i = 0;
	cin.ignore();
	while(i<numOfGetFile)
	{
		
		std::cout<<"Name file: ";
        std::getline(std::cin, userInput);

		if (userInput.length()>0)
		{
			int sendBytes = send(client_TCP_sockfd,userInput.c_str(),userInput.size()+1,0);
    		if (sendBytes < 0)
    		{
        		std::cerr<<"master_connect send() failed !"<<std::endl;
        		return response;
    		}
    		memset(buf,0,2048);
    		int recvBytes = recv(client_TCP_sockfd,buf,2048,0);
    		if(recvBytes < 0)
    		{
        		std::cerr<<"master_connect recv() failed !"<<std::endl;
        		return response;
    		}
			if (strcmp(buf,"NO SUCH FILE!!")!=0)
			{
				response.push_back(convertToString(buf, sizeof(buf)/sizeof(char)));
			}
    		
    		std::cout<<"Master > "<<buf<<std::endl;
			userInput.clear();
			i++;
		}
		else
		{
			std::cout<<"Incorrect file name. Please retype !!"<<std::endl;
		}
		
	}
             
    return response;

}

void RUDPClient::setServerAddr(std::string masterResponse)
{
    std::string serverIP, serverPort,fileName;
    std::stringstream ssInput(masterResponse);
    std::getline(ssInput,fileName,',');
    std::getline(ssInput,serverIP,',');
    std::getline(ssInput,serverPort);
    //serverIP=deleteWhiteSpace(serverIP);
    fileNameArr.push_back(fileName);
    int port =std::atoi(serverPort.c_str());
   
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET,serverIP.c_str(),&serverAddr.sin_addr);
	  
}

void RUDPClient::displayError(const char *errorMsg)
{
	cerr<<"Error: "<<errorMsg<<endl;
	exit(1);
}

/**
 * This method is used to create the client socket and sets the 
 * client_UDP_sockfd class variable.
 */
void RUDPClient::createSocket()
{
	client_UDP_sockfd=socket(AF_INET,SOCK_DGRAM,0);
	if (client_UDP_sockfd < 0)
  	{
		displayError("The server socket could not be opened!");
	}
}


/* void RUDPClient::getServerInfo(char* hostname)
{
	host=gethostbyname(hostname);
	if(host==NULL)
	{
		displayError("There is no such host!");
	}
} */


/* void RUDPClient::setserverAddr(int portNo)
{
	bzero((char *) &serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	bcopy((char *)host->h_addr,(char *)&serverAddr.sin_addr.s_addr,host->h_length);
	serverAddr.sin_port = htons(portNo);
} */


void RUDPClient::createRequest(string filename)
{
	udpSegment.seqNo=0;
	udpSegment.ackNo=0;
	udpSegment.ackFlag=0;
	udpSegment.crc=0;
	string fileStr=filename;
	strcpy(udpSegment.data,fileStr.c_str());

}


int RUDPClient::sendRequest()
{
	len=sizeof(struct sockaddr_in);
	int no=sendto(client_UDP_sockfd,&udpSegment,sizeof(udpSegment),0,(const struct sockaddr *)&serverAddr,len);	
	if (no<0)
   	{
		displayError("There is problem while sending request!");
	}
	return no;
}

void RUDPClient::readResponse(int windowSize, char *outFile)
{
	RUDP_DATA *receiveBuffer=new RUDP_DATA[3000];
	int nxtSeqNo = 0;
	int rcvBuffPos=0;
	int winCount = 0;
	int advertiseWin = windowSize/sizeof(udpSegment);
	RUDP_DATA ackPacket;
	ackPacket.ackFlag=0;
	struct timeval tv;
	fd_set fds;
	int val =1;
	while(ackPacket.ackFlag == 0)
	{
		bool rcvError = 0;
		while(winCount < advertiseWin && ackPacket.ackFlag == 0)
		{
			FD_ZERO(&fds);
			FD_CLR(client_UDP_sockfd,&fds);
			FD_SET(client_UDP_sockfd,&fds);
			tv.tv_sec = 0;
			tv.tv_usec =100000;
			int no;
			val=select(client_UDP_sockfd+1,&fds,NULL,NULL,&tv);
			if(val==0)
			{
				break;
			}
			if(val==-1)
			{
				displayError("There is some problem in receiving the segment!");
			}
			if(FD_ISSET(client_UDP_sockfd,&fds) && val==1)
			{
				no=recvfrom(client_UDP_sockfd,&ackPacket,sizeof(ackPacket),0,(struct sockaddr *)&clientAddr, &len);
				int seqNo=ackPacket.seqNo;
				unsigned long crc = compute_crc((const unsigned char*)ackPacket.data,strlen(ackPacket.data));
				std::cout<<"Received Segment for sequence number "<<seqNo<<endl;					
				if(seqNo==nxtSeqNo/* && crc==ackPacket.crc*/ && rcvBuffPos<1000)
				{
					receiveBuffer[rcvBuffPos]=ackPacket;
					nxtSeqNo=ackPacket.seqNo+1;
					rcvBuffPos++;
					winCount++;
				}
				else
				{	
					rcvError=true;
				}
			}
		}
		if(rcvError)
		{
			RUDP_DATA ack;
			if(rcvBuffPos==0)
			{
				std::cout<<" after 3 hour\n";
							
					ack.seqNo=0;
					ack.ackNo=0;
					ack.ackFlag=1;
					std::cout<<"Sending Acknowledgement Number "<<ack.ackNo<<" for sequence number "<<ack.seqNo<<endl;
					int no=sendto(client_UDP_sockfd,&ack,sizeof(ack),0,(const struct sockaddr *)&serverAddr,len);	
					if (no<0)
   					{
						displayError("There is problem while sending acknowledgement!");
					}
					
				
			}
			else
			{
				ack.seqNo=receiveBuffer[rcvBuffPos-1].seqNo;
				ack.ackNo=receiveBuffer[rcvBuffPos-1].ackNo;
				sendAck(ack);
			}
			

		}
		else
		{
			sendAck(receiveBuffer[rcvBuffPos-1]);
		}
		winCount = 0;
		
	}
	
	writeToFile(receiveBuffer,rcvBuffPos,outFile);
	delete[] receiveBuffer;


}




			
			
		


void RUDPClient::sendAck(RUDP_DATA segment)
{
	RUDP_DATA acknowledge;
	acknowledge.ackNo=segment.seqNo+1;
	acknowledge.seqNo=segment.seqNo;
	acknowledge.ackFlag=1;
	std::cout<<"Sending Acknowledgement Number "<<acknowledge.ackNo<<" for sequence number "<<segment.seqNo<<endl;
	int no=sendto(client_UDP_sockfd,&acknowledge,sizeof(acknowledge),0,(const struct sockaddr *)&serverAddr,len);	
	if (no<0)
   	{
		displayError("There is problem while sending acknowledgement!");
	}
}


void RUDPClient::writeToFile(RUDP_DATA* response,int rcvBuffPos,char* filename)
{
	ofstream writeFile;
  	writeFile.open(filename);
	for(int i=0;i<rcvBuffPos;i++)
	{
		writeFile<<response[i].data;
	}
	std::cout<<"File Transfered Successfully "<<endl;
	writeFile.close();
}

struct threadArg
{
	RUDPClient client;
	std::string response="";

	int windowSize=4350;
};

void* threadFunc(void* argv)
{
	threadArg *arg=(threadArg*)argv;
	pthread_mutex_lock(&lock);
	arg->client.createSocket();
	arg->client.setServerAddr(arg->response);
	std::string file = arg->client.getFileName(arg->windowSize);
	arg->client.createRequest(file);
	arg->client.sendRequest();
	arg->client.readResponse(arg->windowSize, (char*)file.c_str());
	pthread_mutex_unlock(&lock);
	pthread_exit(nullptr);
}
int main()
{
	RUDPClient client;
	vector<string> masterResponse;
	
	
	masterResponse = client.master_connect();

	int thread_no = masterResponse.size();
	 if (thread_no == 1)
	{
		client.createSocket();
		std::cout<<"after createSocket"<<endl;
		client.setServerAddr(masterResponse[0]);
		std::cout<<"after setServerAddr"<<endl;
		std::string file=client.getFileName(0);
		client.createRequest(file);
		std::cout<<"after createRequest"<<endl;
		client.sendRequest();
		std::cout<<"after sendRequest"<<endl;
		client.readResponse(4350,(char*)file.c_str()); 

	} 
	else
	{
		struct threadArg arg[thread_no];
		pthread_t threadID[thread_no];
		for (int i=0;i<thread_no;i++)
		{
			arg[i].client=client;
			arg[i].response=masterResponse[i];
			
			pthread_create(&threadID[i],nullptr,threadFunc, &arg);
		}
		 for (int i =0;i<thread_no;++i)
		{
			pthread_join(threadID[i],nullptr);
		} 
	}

	return 0;
}
