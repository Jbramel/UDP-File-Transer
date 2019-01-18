#include "udpProtocol.h"
#include <stdio.h>


int initializeServer(Tcb *servTcb)
{
  
 
}


int initializeClient(Tcb *clientTcb)
{
   
}



void createPacket(Packet *pkt, uint32_t l, uint32_t seq, uint32_t ack, uint32_t flg)
{

   pkt->length = htonl(l);
   pkt->seqNum = htonl(seq);
   pkt->ackNum = htonl(ack);
   pkt->flag = htonl(flg);
}

void readPacket(Packet *pkt)
{
   pkt->length = ntohl(pkt->length);
   pkt->seqNum = ntohl(pkt->seqNum);
   pkt->ackNum = ntohl(pkt->ackNum);
   pkt->flag = ntohl(pkt->flag);
}

unsigned int checksum(char *addr, unsigned int count )
{

}
