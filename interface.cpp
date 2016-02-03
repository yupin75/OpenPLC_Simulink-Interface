#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>

using namespace std;

char plcStation1[100], plcStation2[100], plcStation3[100], plcStation4[100], plcStation5[100];
char simulink[100];

pthread_mutex_t bufferLock;

struct plcData
{
	uint16_t pressure = 0;
	bool pumpState = 0;
	bool reliefValve = 0;
};

struct plcData dataStation1;
struct plcData dataStation2;
struct plcData dataStation3;
struct plcData dataStation4;
struct plcData dataStation5;

//-----------------------------------------------------------------------------
// Helper function - Convert a byte array into a double
//-----------------------------------------------------------------------------
double convertBufferToDouble(unsigned char *buff)
{
	double returnVal;
	memcpy(&returnVal, buff, 8);

	return returnVal;
}

//-----------------------------------------------------------------------------
// Helper function - Makes the running thread sleep for the ammount of time
// in milliseconds
//-----------------------------------------------------------------------------
void sleep_ms(int milliseconds)
{
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

//-----------------------------------------------------------------------------
// Finds the IP address on the line provided
//-----------------------------------------------------------------------------
void getIPAddress(char *line, char *buf)
{
	int i=0, j=0;

	while (line[i] != '"' && line[i] != '\0')
	{
		i++;
	}
	i++;

	while (line[i] != '"' && line[i] != '\0')
	{
		buf[j] = line[i];
		i++;
		j++;
		buf[j] = '\0';
	}

}

//-----------------------------------------------------------------------------
// Fills the approriate buffer with the IP address
//-----------------------------------------------------------------------------
void fillBuffer(char *line, int station)
{
	switch(station)
	{
		case 0:
			getIPAddress(line, simulink);
			break;

		case 1:
			getIPAddress(line, plcStation1);
			break;

		case 2:
			getIPAddress(line, plcStation2);
			break;

		case 3:
			getIPAddress(line, plcStation3);
			break;

		case 4:
			getIPAddress(line, plcStation4);
			break;

		case 5:
			getIPAddress(line, plcStation5);
			break;
	}
}

//-----------------------------------------------------------------------------
// Parse the interface.cfg file looking for the IP address of the Simulink app
// and for each OpenPLC station
//-----------------------------------------------------------------------------
void parseConfigFile()
{
	string line;
	char buffer[1024];
	ifstream cfgfile("interface.cfg");

	if (cfgfile.is_open())
	{
		while (getline(cfgfile, line))
		{
			strncpy(buffer, line.c_str(), 1024);
			if (buffer[0] != '#' && strlen(buffer) > 1)
			{
				if (!strncmp(buffer, "Simulink", 8))
				{
					fillBuffer(buffer, 0);
				}

				else if (!strncmp(buffer, "Station", 7))
				{
					char temp[2];
					temp[0] = buffer[7];
					temp[1] = '\0';
					fillBuffer(buffer, atoi(temp));
				}

			}
		}

		cfgfile.close();
	}

	else
	{
		cout << "Error trying to open file!" << endl;
	}
}

//-----------------------------------------------------------------------------
// Create the socket and bind it. Returns the file descriptor for the socket
// created.
//-----------------------------------------------------------------------------
int createUDPServer(int port)
{
	int socket_fd;
	struct sockaddr_in server_addr;
	struct hostent *server;

	//Create TCP Socket
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd<0)
	{
		perror("Server: error creating stream socket");
		exit(1);
	}

	//Initialize Server Struct
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	//Bind socket
	if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Server: error binding socket");
		exit(1);
	}

	printf("Socket %d binded successfully on port %d!\n", socket_fd, port);

	return socket_fd;
}

//-----------------------------------------------------------------------------
// Fill the provided char buffers with the status of the pump and the relief
// valve
//-----------------------------------------------------------------------------
void getValuesFromStation(int stationNumber, char *pumpStatusBuf, char *valveStatusBuf)
{
	int pumpStatus, valveStatus;

	switch (stationNumber)
	{
		case 1:
			pthread_mutex_lock(&bufferLock);
			pumpStatus = dataStation1.pumpState;
			valveStatus = dataStation1.reliefValve;
			pthread_mutex_unlock(&bufferLock);
			break;
		case 2:
			pthread_mutex_lock(&bufferLock);
			pumpStatus = dataStation2.pumpState;
			valveStatus = dataStation2.reliefValve;
			pthread_mutex_unlock(&bufferLock);
			break;
		case 3:
			pthread_mutex_lock(&bufferLock);
			pumpStatus = dataStation3.pumpState;
			valveStatus = dataStation3.reliefValve;
			pthread_mutex_unlock(&bufferLock);
			break;
		case 4:
			pthread_mutex_lock(&bufferLock);
			pumpStatus = dataStation4.pumpState;
			valveStatus = dataStation4.reliefValve;
			pthread_mutex_unlock(&bufferLock);
			break;
		case 5:
			pthread_mutex_lock(&bufferLock);
			pumpStatus = dataStation5.pumpState;
			valveStatus = dataStation5.reliefValve;
			pthread_mutex_unlock(&bufferLock);
			break;
	}

	sprintf(pumpStatusBuf, "%d", pumpStatus);
	sprintf(valveStatusBuf, "%d", valveStatus);
}

//-----------------------------------------------------------------------------
// Thread to send data to Simulink using UDP
//-----------------------------------------------------------------------------
void *sendSimulinkData(void *args)
{
	int stationNumber = *(int *)args;
	int socket_fd, port1, port2;
	struct sockaddr_in server_addr1;
	struct sockaddr_in server_addr2;
	struct hostent *server;
	int send_len;
	char pumpStatus[20];
	char valveStatus[20];

	//Create TCP Socket
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd<0)
	{
		perror("Server: error creating stream socket");
		exit(1);
	}

	//Figure out information about station
	switch (stationNumber)
	{
		case 1:
			port1 = 10001;
			port2 = 10003;
			break;
		case 2:
			port1 = 20001;
			port2 = 20003;
			break;
		case 3:
			port1 = 30001;
			port2 = 30003;
			break;
		case 4:
			port1 = 40001;
			port2 = 40003;
			break;
		case 5:
			port1 = 50001;
			port2 = 50003;
			break;
	}

	//Initialize Server Structures
	server = gethostbyname(simulink);
	if (server == NULL)
	{
		printf("Error locating host %s\n", simulink);
		return 0;
	}

	bzero((char *) &server_addr1, sizeof(server_addr1));
	server_addr1.sin_family = AF_INET;
	server_addr1.sin_port = htons(port1);
	bcopy((char *)server->h_addr, (char *)&server_addr1.sin_addr.s_addr, server->h_length);

	bzero((char *) &server_addr2, sizeof(server_addr2));
	server_addr2.sin_family = AF_INET;
	server_addr2.sin_port = htons(port2);
	bcopy((char *)server->h_addr, (char *)&server_addr2.sin_addr.s_addr, server->h_length);

	while (1)
	{
		getValuesFromStation(stationNumber, pumpStatus, valveStatus);

		//printf("sending pumpStatus: %s to port: %d\n", pumpStatus, port1);
		send_len = sendto(socket_fd, pumpStatus, strlen(pumpStatus), 0, (struct sockaddr *)&server_addr1, sizeof(server_addr1));
		if (send_len < 0)
		{
			printf("Error sending pump status on socket %d\n", socket_fd);
		}

		//printf("sending valveStatus: %s to port: %d\n", valveStatus, port2);
		send_len = sendto(socket_fd, valveStatus, strlen(valveStatus), 0, (struct sockaddr *)&server_addr2, sizeof(server_addr2));
		if (send_len < 0)
		{
			printf("Error sending valve status on socket %d\n", socket_fd);
		}
		sleep_ms(100);
	}
}


//-----------------------------------------------------------------------------
// Thread to receive data from Simulink using UDP
//-----------------------------------------------------------------------------
void *receiveSimulinkData(void *arg)
{
	int *rcv_args = (int *)arg;
	int socket_fd = rcv_args[0];
	int stationNumber = rcv_args[1];
	const int BUFF_SIZE = 1024;
	int rcv_len;
	unsigned char rcv_buffer[BUFF_SIZE];
	socklen_t cli_len;
	struct sockaddr_in client;

	cli_len = sizeof(client);

	while(1)
	{
		rcv_len = recvfrom(socket_fd, rcv_buffer, BUFF_SIZE, 0, (struct sockaddr *) &client, &cli_len);
		if (rcv_len < 0)
		{
			printf("Error receiving data on socket %d\n", socket_fd);
		}

		else
		{
			double valueRcv = convertBufferToDouble(rcv_buffer);
			//printf("Received packet from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
			//printf("Size: %d\nData: %f\n" , rcv_len, valueRcv);
			switch(stationNumber)
			{
				case 1:
					pthread_mutex_lock(&bufferLock);
					dataStation1.pressure = (uint16_t)valueRcv;
					pthread_mutex_unlock(&bufferLock);
					break;
				case 2:
					pthread_mutex_lock(&bufferLock);
					dataStation2.pressure = (uint16_t)valueRcv;
					pthread_mutex_unlock(&bufferLock);
					break;
				case 3:
					pthread_mutex_lock(&bufferLock);
					dataStation3.pressure = (uint16_t)valueRcv;
					pthread_mutex_unlock(&bufferLock);
					break;
				case 4:
					pthread_mutex_lock(&bufferLock);
					dataStation4.pressure = (uint16_t)valueRcv;
					pthread_mutex_unlock(&bufferLock);
					break;
				case 5:
					pthread_mutex_lock(&bufferLock);
					dataStation5.pressure = (uint16_t)valueRcv;
					pthread_mutex_unlock(&bufferLock);
					break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Main function responsible to exchange data with the simulink application
//-----------------------------------------------------------------------------
void exchangeDataWithSimulink()
{
	//creating servers to read pressure on all stations
	int *args0 = new int[2]; int *args1 = new int[2]; int *args2 = new int[2]; int *args3 = new int[2]; int *args4 = new int[2];

	args0[0] = createUDPServer(10002);
	args0[1] = 1; //OpenPLC Station number
	args1[0] = createUDPServer(20002);
	args1[1] = 2; //OpenPLC Station number
	args2[0] = createUDPServer(30002);
	args2[1] = 3; //OpenPLC Station number
	args3[0] = createUDPServer(40002);
	args3[1] = 4; //OpenPLC Station number
	args4[0] = createUDPServer(50002);
	args4[1] = 5; //OpenPLC Station number

	//creating threads to receive simulink data
	pthread_t receivingThreads[5];
	pthread_create(&receivingThreads[0], NULL, receiveSimulinkData, args0);
	pthread_create(&receivingThreads[1], NULL, receiveSimulinkData, args1);
	pthread_create(&receivingThreads[2], NULL, receiveSimulinkData, args2);
	pthread_create(&receivingThreads[3], NULL, receiveSimulinkData, args3);
	pthread_create(&receivingThreads[4], NULL, receiveSimulinkData, args4);

	//creating threads to send data to simulink
	int *stations = new int[5];
	stations[0] = 1; stations[1] = 2; stations[2] = 3; stations[3] = 4; stations[4] = 5;
	pthread_t sendingThreads[5];
	pthread_create(&sendingThreads[0], NULL, sendSimulinkData, (int *)&stations[0]);
	pthread_create(&sendingThreads[1], NULL, sendSimulinkData, (int *)&stations[1]);
	pthread_create(&sendingThreads[2], NULL, sendSimulinkData, (int *)&stations[2]);
	pthread_create(&sendingThreads[3], NULL, sendSimulinkData, (int *)&stations[3]);
	pthread_create(&sendingThreads[4], NULL, sendSimulinkData, (int *)&stations[4]);
}

//-----------------------------------------------------------------------------
// Thread to connect to an OpenPLC station and exchange data with it
//-----------------------------------------------------------------------------
void *connectToStation(void *args)
{
	int stationNumber = *(int *)args;
	int socket_fd, port = 6668;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int data_len;
	socklen_t cli_len;
	struct plcData *plcStation;
	struct plcData *localBuffer = (struct plcData *)malloc(sizeof(struct plcData));
	char *hostaddr;

	//Create TCP Socket
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd<0)
	{
		perror("Server: error creating stream socket");
		exit(1);
	}

	//Figure out information about station
	switch (stationNumber)
	{
		case 1:
			hostaddr = plcStation1;
			plcStation = &dataStation1;
			break;
		case 2:
			hostaddr = plcStation2;
			plcStation = &dataStation2;
			break;
		case 3:
			hostaddr = plcStation3;
			plcStation = &dataStation3;
			break;
		case 4:
			hostaddr = plcStation4;
			plcStation = &dataStation4;
			break;
		case 5:
			hostaddr = plcStation5;
			plcStation = &dataStation5;
			break;
	}

	//Initialize Server Structures
	server = gethostbyname(hostaddr);
	if (server == NULL)
	{
		printf("Error locating host %s\n", hostaddr);
		return 0;
	}

	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

	//set timeout of 100ms on receive
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
		printf("Error setting timeout\n");
	}

	while (1)
	{
		pthread_mutex_lock(&bufferLock);
		localBuffer->pumpState = 0;
		localBuffer->pressure = plcStation->pressure;
		localBuffer->reliefValve = 0;
		pthread_mutex_unlock(&bufferLock);

		//printf("Sending pressure: %d to station: %d\n", localBuffer->pressure, stationNumber);
		data_len = sendto(socket_fd, localBuffer, sizeof(*localBuffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (data_len < 0)
		{
			printf("Error sending data on socket %d\n", socket_fd);
		}
		else
		{
			//printf("Receiving data from station %d\n", stationNumber);
			data_len = recvfrom(socket_fd, localBuffer, sizeof(*localBuffer), 0, (struct sockaddr *)&server_addr, &cli_len);
			if (data_len < 0)
			{
				printf("Error receiving data on socket %d\n", socket_fd);
			}
			else
			{
				pthread_mutex_lock(&bufferLock);
				plcStation->pumpState = localBuffer->pumpState;
				plcStation->reliefValve = localBuffer->reliefValve;
				pthread_mutex_unlock(&bufferLock);
			}
		}

		sleep_ms(100);
	}
}

//-----------------------------------------------------------------------------
// Main function responsible to exchange data with the OpenPLC stations
//-----------------------------------------------------------------------------
void exchangeDataWithOpenPLC()
{
	//creating threads to exchange data with the OpenPLC stations
	int *stations = new int[5];
	stations[0] = 1; stations[1] = 2; stations[2] = 3; stations[3] = 4; stations[4] = 5;
	pthread_t plcThreads[5];
	pthread_create(&plcThreads[0], NULL, connectToStation, (int *)&stations[0]);
	pthread_create(&plcThreads[1], NULL, connectToStation, (int *)&stations[1]);
	pthread_create(&plcThreads[2], NULL, connectToStation, (int *)&stations[2]);
	pthread_create(&plcThreads[3], NULL, connectToStation, (int *)&stations[3]);
	pthread_create(&plcThreads[4], NULL, connectToStation, (int *)&stations[4]);
}

//-----------------------------------------------------------------------------
// Interface main function. Should call the functions to exchange data with
// the simulink application and with the OpenPLC stations. The main loop
// must also display periodically the data exchanged with each OpenPLC station.
//-----------------------------------------------------------------------------
int main ()
{
	printf("Starting Interface program...\n");
	parseConfigFile();
	printf("Simulink IP: %s\n", simulink);
	printf("Station 1 IP: %s\n", plcStation1);
	printf("Station 2 IP: %s\n", plcStation2);
	printf("Station 3 IP: %s\n", plcStation3);
	printf("Station 4 IP: %s\n", plcStation4);
	printf("Station 5 IP: %s\n", plcStation5);

	exchangeDataWithSimulink();
	exchangeDataWithOpenPLC();

	while(1)
	{
		///*
		pthread_mutex_lock(&bufferLock);
		printf("Station 1\nPressure: %d\tPump: %d\t\tValve: %d\n", dataStation1.pressure, dataStation1.pumpState, dataStation1.reliefValve);
		printf("Station 2\nPressure: %d\tPump: %d\t\tValve: %d\n", dataStation2.pressure, dataStation2.pumpState, dataStation2.reliefValve);
		printf("Station 3\nPressure: %d\tPump: %d\t\tValve: %d\n", dataStation3.pressure, dataStation3.pumpState, dataStation3.reliefValve);
		printf("Station 4\nPressure: %d\tPump: %d\t\tValve: %d\n", dataStation4.pressure, dataStation4.pumpState, dataStation4.reliefValve);
		printf("Station 5\nPressure: %d\tPump: %d\t\tValve: %d\n\n", dataStation5.pressure, dataStation5.pumpState, dataStation5.reliefValve);
		pthread_mutex_unlock(&bufferLock);

		sleep_ms(1000);
		//*/
	}

	return 0;
}
