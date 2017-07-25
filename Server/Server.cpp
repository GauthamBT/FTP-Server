#include<iostream>
#include<fstream> //To use file stream
#include<stdlib.h> //To use exit(0)
//#include<stdio.h>
//#include<sys/types.h>
//#include<sys/socket.h>
//#include<netinet/in.h>
#include<arpa/inet.h> //To use inet_addr

#include<unistd.h> //To use uSleep(), read, write
#include<netdb.h> //To use Hostent
#include<string.h> //To use strerror()
#include<errno.h> //To get errno

#include<pthread.h>

using namespace std;

int numberOfRequests = 0;
pthread_mutex_t lock;

class Server
{

public:
	struct sockaddr_in serverAddress;

	int ServerSocket;

	//To initiate and start socket
	Server(string IPAddress, int portNumber, string protocol)
	{
		//Assigning IP address
		if(IPAddress == "")
		{
			serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else
		{
			serverAddress.sin_addr.s_addr = inet_addr(IPAddress.c_str());
		}

		//Assigning port number
		serverAddress.sin_port = htons(portNumber);

		if(protocol == "TCP")
		{
			tcpConnection();
		}
		else if(protocol == "UDP")
		{
			udpSocket();
		}
	}

	void tcpConnection()
	{
		//Assigning family
		serverAddress.sin_family = AF_INET;

		//Creating socket
		ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

		if (ServerSocket == 0) {
			cerr << "Error in socket TCP: " << strerror(errno) << endl;
			exit(0);
		}

		cout << "Socket created" << endl;

		//Binding the socket
		if (bind(ServerSocket, (sockaddr *) &serverAddress, sizeof(serverAddress))
				< 0) {
			cerr << "Error in bind: " << strerror(errno) << endl;
			exit(0);
		}

		cout << "Bind successful" << endl;

		//Listening for new connections
		if (listen(ServerSocket, 5) < 0) {
			cerr << "Error in listen: " << strerror(errno) << endl;
			exit(0);
		}

		cout << "After Listen" << endl;
	}

	void udpSocket()
	{
		//Assigning family
		serverAddress.sin_family = AF_INET;

		ServerSocket = socket(AF_INET, SOCK_DGRAM, 0);

		if(ServerSocket < 0)
		{
			cerr << "Error in socket UDP: " << strerror(errno) << endl;
		}
	}
};

class Client
{
public:
	struct sockaddr_in clientAddress;
	int clientSocket;
};

void* servingClientRequest(void * var)
{
	FILE * fp;
	fstream file;
	char buffer[1024];
	char filename[20];
	int fileSize;
	int sentSize;
	Client c;
	char threadName[1024];

	//Synchronizing to perform task. Locking the mutex lock. 
	pthread_mutex_lock(&lock);

	//Performing the task. 
	cout << "Request Number = " << numberOfRequests++ << endl;

	//Task Completed. Unlocking the mutex. 
	pthread_mutex_unlock(&lock);

	//Fetching Thread Name
	pthread_getname_np(pthread_self(), threadName, 1024);

	//Printing thread name and thread ID
	cout << "Inside Serving Client Request" << endl << "Thread = " << pthread_self() << endl
			<< "Thread name = " << threadName << endl;

	//Copying the client information
	memcpy(&c, var, sizeof(Client));

	//Deleting dynamic memory.
	delete((Client *) var);

	//Authenticating Client ID
	if (read(c.clientSocket, buffer, 1024) < 0) {
		cerr << "Error in read: " << strerror(errno) << endl;
		exit(0);
	}

	cout << "Received Message: " << buffer << endl;

	//Executing unix command ls to fetch the files list.
	fp = popen("ls","r");

	//Reading from popen.
	fread(buffer, 1024, 1, fp);

	//Sending files list to client
	if (write(c.clientSocket, buffer, 1024) < 0) {
		cerr << "Error in write: " << strerror(errno) << endl;
		exit(0);
	}

	cout << "Message sent" << endl;

	//Closing the popen 
	pclose(fp); fp = NULL;

	//Reading the file name send by the client
	if (read(c.clientSocket, filename, 1024) < 0) {
		cerr << "Error in read: " << strerror(errno) << endl;
		exit(0);
	}

	//Openning the file requested by client. 
	file.open(filename, ios::out | ios::in | ios::binary);

	//Checking if the file is open or not. 
	if(!file.is_open())
	{
		cerr << "Error in file opening" << endl;
	}

	//Finding file size
	file.seekg(0, file.end);
	fileSize = file.tellg();
	file.seekg(0, file.beg);

	cout << "File size = " << fileSize << endl;

	sprintf(buffer, "%d", fileSize);

	//Sending file size
	if (write(c.clientSocket, buffer, sizeof(buffer)) < 0) {
		cerr << "Error in write: " << strerror(errno) << endl;
		exit(0);
	}

	cout << "File size sent" << endl;

	//Sending file to client
	while(fileSize > 0)
	{
		file.read(buffer, 1);

		sentSize = write(c.clientSocket, buffer, 1);

		fileSize -= sentSize;
	}

	cout << "File sent" << endl;

	//Request handlled. Closing client socket. 
	close(c.clientSocket);
	file.close();

	cout << "Slave socket closed" << endl;
}



int main() {

	//Starting a TCP server.
	Server s("127.0.0.1", 8888, "TCP");
	//Client class variable
	Client c;
	int threadValue = 0;
	int i = 0;

	unsigned int clientLength = sizeof(c.clientAddress);

	//Buffer to received and send messages. 
	char buffer[1024];

	//Running server. 
	while (1) {

		//Once the client has connected.
		c.clientSocket = accept(s.ServerSocket, (struct sockaddr *) &c.clientAddress,
				&clientLength);

		if (c.clientSocket < 0) {
			cerr << "Error in accept: " << strerror(errno) << endl;
			exit(0);
		} else {

			//To know the client address
			hostent * hostTemp;
			char * hostAddressChar;

			hostTemp = gethostbyaddr(
					(const char *) &c.clientAddress.sin_addr.s_addr,
					sizeof(c.clientAddress.sin_addr.s_addr), AF_INET);

			hostAddressChar = inet_ntoa(c.clientAddress.sin_addr);

			cout << "Received connection from: " << hostTemp->h_name
					<< " with IP: " << hostAddressChar << endl;
		}

		cout << "Slave socket created" << endl;

		//To send the client info to thread.
		Client * toSend = new Client;
		memcpy(toSend, &c, sizeof(Client));

		//Call the thread to function serviceClientRequest
		pthread_t t;
		pthread_attr_t ta;

		//Creating the thread to be detached so that the main thread don't have to wait. 
		pthread_attr_init(&ta);
		pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED);

		char threadName[20];

		sprintf(threadName, "Thread%d", i);

		pthread_setname_np(t, threadName);

		//Creating thread
		threadValue = pthread_create(&t, &ta, servingClientRequest, (void *) toSend);

		cout << "Thread created" << endl;
	}

	close(s.ServerSocket);

}

