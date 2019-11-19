#include <iostream>
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
using namespace std;

string masterIP = "127.0.0.1";
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
  	struct hostent *host;
	struct RUDP_DATA
	{
		uint32_t seqNo;
		uint32_t ackNo;
		bool ackFlag;
		char data[MAX_PAYLOAD_LENGTH];
	}udpSegment;	

	string fileName;
	public:
	
	// handle master
	std::string master_connect();

	// hamdle server
	string getFileName(){return fileName;}
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

std::string RUDPClient::master_connect()
{
    std::string response="";
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
    do
    {
        std::cout<<"Name file: ";
        std::getline(std::cin, userInput);
        if (userInput.length()==0)
        {
            std::cout<<"Incorrect file name !! Please retype."<<std::endl;
        }
    }while(userInput.length() == 0);
    
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
        return nullptr;
    }
    response = convertToString(buf, sizeof(buf)/sizeof(char));
    std::cout<<"Master > "<<response<<std::endl;
    return response;

}

void RUDPClient::setServerAddr(std::string masterResponse)
{
    std::string serverIP, serverPort;
    std::stringstream ssInput(masterResponse);
    std::getline(ssInput,fileName,',');
    std::getline(ssInput,serverIP,',');
    std::getline(ssInput,serverPort);
    //serverIP=deleteWhiteSpace(serverIP);
    
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


void RUDPClient::getServerInfo(char* hostname)
{
	host=gethostbyname(hostname);
	if(host==NULL)
	{
		displayError("There is no such host!");
	}
}


void RUDPClient::setserverAddr(int portNo)
{
	bzero((char *) &serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	bcopy((char *)host->h_addr,(char *)&serverAddr.sin_addr.s_addr,host->h_length);
	serverAddr.sin_port = htons(portNo);
}


void RUDPClient::createRequest(string filename)
{
	udpSegment.seqNo=0;
	udpSegment.ackNo=0;
	udpSegment.ackFlag=0;
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


void RUDPClient::readResponse(int windowSize,char* outputFile)
{
	RUDP_DATA *receiveBuffer=new RUDP_DATA[3000];
	int nxtExpectedSeqNum=0;
	int rcvBuffPos=0;	
	int winCount=0;
	int noOfSegInWin=windowSize/sizeof(udpSegment);
	int no=1;
	RUDP_DATA segment;
	segment.ackFlag=0;
	struct timeval tv;
	fd_set fds;
	int val=1;
	while(segment.ackFlag==0)
	{
		no=1;
		bool outOfOrderFlag=false;
		while(winCount<noOfSegInWin && segment.ackFlag==0)
		{
			FD_ZERO(&fds);
			FD_CLR(client_UDP_sockfd,&fds);
			FD_SET(client_UDP_sockfd,&fds);
			tv.tv_sec = 0;
			tv.tv_usec =100000;
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
				no=recvfrom(client_UDP_sockfd,&segment,sizeof(segment),0,(struct sockaddr *)&clientAddr, &len);
				int seqNo=segment.seqNo;
				cout<<"Received Segment for sequence number "<<seqNo<<endl;					
				if(seqNo==nxtExpectedSeqNum && rcvBuffPos<1000)
				{
					receiveBuffer[rcvBuffPos]=segment;
					nxtExpectedSeqNum=segment.seqNo+1;
					rcvBuffPos++;
					winCount++;
				}
				else
				{	
					outOfOrderFlag=true;
				}
			}
		}
		if(outOfOrderFlag)
		{
			RUDP_DATA ack;
			if(rcvBuffPos==0)
			{
				for(int i=0;i<3;i++)
				{			
					ack.seqNo=0;
					ack.ackNo=0;
					ack.ackFlag=1;
					cout<<"Sending Acknowledgement Number "<<ack.ackNo<<" for sequence number "<<ack.seqNo<<endl;
					int no=sendto(client_UDP_sockfd,&ack,sizeof(ack),0,(const struct sockaddr *)&serverAddr,len);	
					if (no<0)
   					{
						displayError("There is problem while sending acknowledgement!");
					}
				}	
				
			}
			else
			{
				for(int i=0;i<3;i++)
				{			
					ack.seqNo=receiveBuffer[rcvBuffPos-1].seqNo;
					ack.ackNo=receiveBuffer[rcvBuffPos-1].seqNo;					
					sendAck(ack);
				}
			}			
		}
		else
		{
			//sleep(1);
			sendAck(receiveBuffer[rcvBuffPos-1]);
		}
		winCount=0;		
	}
	writeToFile(receiveBuffer,rcvBuffPos,outputFile);
}


void RUDPClient::sendAck(RUDP_DATA segment)
{
	RUDP_DATA acknowledge;
	acknowledge.ackNo=segment.seqNo+1;
	acknowledge.seqNo=segment.seqNo;
	acknowledge.ackFlag=1;
	cout<<"Sending Acknowledgement Number "<<acknowledge.ackNo<<" for sequence number "<<segment.seqNo<<endl;
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
	for(int i=0;i<rcvBuffPos-1;i++)
	{
		writeFile<<response[i].data;
	}
	cout<<"File Transfered Successfully "<<endl;
	writeFile.close();
}

int main(/* int noOfArguments,char *argumentList[]*/)
{
	RUDPClient client;

	/* if(noOfArguments<5)
	{
		client.displayError("Invalid arguments!");
	}  
	client.createSocket();
	int portNo=atoi(argumentList[2]);
	client.getServerInfo(argumentList[1]);
	client.setserverAddr(portNo);
	client.createRequest(argumentList[3]);	
	int size=client.sendRequest();
	client.readResponse(atoi(argumentList[4]),argumentList[3]);	*/
	std::string masterResponse;
	masterResponse = client.master_connect();
	client.createSocket();
	client.setServerAddr(masterResponse);
	std::string file = client.getFileName();
	client.createRequest(file);
	client.sendRequest();
	client.readResponse(4350,(char*)"COLEN.txt");



	return 0;
}