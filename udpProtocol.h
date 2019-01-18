//udpProtocol Data structures and function declarations

#ifndef UDPPROTOCOL_H
#define UDPPROTOCOL_H

#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>

#define PKT_SIZE 	512
#define PAYLOAD_SIZE 	496
#define HEADER_SIZE 	16

#define SYN 		1
#define SYN_ACK 	2
#define DATA		3
#define ACK		4
#define FIN		5
#define FIN_ACK		6

/*
	DATA STRUCTURES
*/


//packet data structure
typedef struct {
   uint32_t length;		//checksum value for integrity check
   uint32_t seqNum;		//number for packet sequencing
   uint32_t ackNum;		//number for ack sequencing
   uint32_t flag;		//determines type of message being sent (seq/ack/fin/etc)
   char payload[PAYLOAD_SIZE];
} Packet;

//Transfer control block for stop and wait protocol
typedef struct {
   uint32_t nextSeq;		//nextsequence to be sent by client
   uint32_t expectedSeq;	//expected seqNum for server 

   Packet currentPkt;		//client packet awaiting acknowledgement or last packet server has acknowledged
   
} Tcb;




/*
	FUNCTIONS
*/

//initializes server state and handles initial connection with client 
int initializeServer(Tcb *servTcb);

// intializes client state and handles connection with server
int initializeClient(Tcb *clientTcb);

//waits for SYN and sends SYNACK
int serverConnect(int server_socket);

//sends SYN and waits for SYN ACK
int clientConnect(int client_socket, struct sockaddr* server_addr);


//creates a packet with length l (size of payload), seqNum seq, ackNum ack, and flag flg 
void createPacket(Packet *pkt, uint32_t l, uint32_t seq, uint32_t ack, uint32_t flg);

//converts packet header fields to host format
void readPacket(Packet *pkt);

//Compute 32 bit checksum form "count" bytes beginning at location addr 
unsigned int checksum(char *addr, unsigned int count );

#endif





