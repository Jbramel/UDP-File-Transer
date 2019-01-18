//================================================== file = udpServer.c =====
//=  A file transfer program using TCP - this is the file send                =
//=============================================================================
//=  Notes:                                                                   =
//=    1) This program  compiles for BSD sockets only.     
//=    2) This program receives a file from udpClient.c using Reliable UDP Protocol=
//=    3) This program take command line input as shown below                 =
//=    4) Ignore build warnings on implicit declarations of functions.        =
//=---------------------------------------------------------------------------=
//=  Example execution: (./udpServer 0)           =
//=    Starting file receive...                                              =
//=    File receive is complete                                              =
//=---------------------------------------------------------------------------=
//=  Build: gcc udpServer.c udpProtocol.c -lnsl for BSD                                   
//=---------------------------------------------------------------------------=
//=  Execute: ./udpServer [packetLoss?0:1]                                                     
//=---------------------------------------------------------------------------=
//=  Author: Justin Bramel                                                    =
//=          University of South Florida                                      =
//=          Email: jbramel@mail.usf.edu                                    =
//=---------------------------------------------------------------------------=
//=  History:  11/27/2017 - Final version                                     =
//=============================================================================
#define  BSD              // WIN for Winsock and BSD for BSD sockets

//----- Include files ---------------------------------------------------------
#include <stdio.h>          // Needed for printf()
#include <string.h>         // Needed for memcpy() and strcpy()
#include <stdlib.h>         // Needed for exit()
#include <fcntl.h>          // Needed for file i/o constants
#include <string.h>         // Used for strcpy()
#include <ctype.h>
#include "udpProtocol.h"
#ifdef WIN
  #include <windows.h>      // Needed for all Winsock stuff
  #include <io.h>           // Needed for open(), close(), and eof()
  #include <sys\stat.h>     // Needed for file i/o constants
#endif
#ifdef BSD
  #include <sys/types.h>    // Needed for sockets stuff
  #include <netinet/in.h>   // Needed for sockets stuff
  #include <sys/socket.h>   // Needed for sockets stuff
  #include <arpa/inet.h>    // Needed for sockets stuff
  #include <fcntl.h>        // Needed for sockets stuff
  #include <netdb.h>        // Needed for sockets stuff
  #include <sys/uio.h>      // Needed for open(), close(), and eof()
  #include <sys/stat.h>     // Needed for file i/o constants
#endif

//----- Defines ---------------------------------------------------------------
#define  PORT_NUM   1050            // Arbitrary port number for the server
#define  SIZE        512            // Buffer size
#define  RECV_FILE  "recvFile.dat"  // File name of received file
#define DISCARD_RATE 0.02           // Discard rate (from 0.0 to 1.0)

//----- Prototypes ------------------------------------------------------------
int recvFile(char *fileName, int portNum, int maxSize, int options);

double rand_val(void)
{
  const long  a =      16807;  // Multiplier
  const long  m = 2147483647;  // Modulus
  const long  q =     127773;  // m div a
  const long  r =       2836;  // m mod a
  static long x = 1;           // Random int value (seed is set to 1)
  long        x_div_q;         // x divided by q
  long        x_mod_q;         // x modulo q
  long        x_new;           // New x value

  // RNG using integer arithmetic
  x_div_q = x / q;
  x_mod_q = x % q;
  x_new = (a * x_mod_q) - (r * x_div_q);
  if (x_new > 0)
    x = x_new;
  else
    x = x_new + m;

  // Return a random value between 0.0 and 1.0
  return((double) x / m);
}

//===== Main program ==========================================================
int main(int argc, char *argv[])
{
  int                  portNum;         // Port number to receive on
  int                  maxSize;         // Maximum allowed size of file
  int                  timeOut;         // Timeout in seconds
  int                  options;         // Options
  int                  retcode;         // Return code
  
  if(argc != 2){
    printf("Usage: 'projectServer emul' where emul is whether to emulate or\n");
    printf("        not a packet loss                                      \n");
    return (0);
  }

  // Initialize parameters
  portNum = PORT_NUM;
  maxSize = 0;           // This parameter is unused in this implementation
  options = atoi(argv[1]);     

  // Receive the file
  printf("Starting file receive... \n");
  retcode = recvFile(RECV_FILE, portNum, maxSize, options);
  printf("File receive is complete \n");

  // Return
  return(0);
}

//=============================================================================
//=  Function to receive a file using UDP                                     =
//=============================================================================
//=  Inputs:                                                                  =
//=    fileName -- Name of file to create and write                           =
//=    portNum --- Port number to listen and receive on                       =
//=    maxSize --- Maximum size in bytes for written file (not implemented)   =
//=    options --- Options whether to emulate packet loss or not              =
//=---------------------------------------------------------------------------=
//=  Outputs:                                                                 =
//=    Returns -1 for fail and 0 for success                                  =
//=---------------------------------------------------------------------------=
//=  Side effects:                                                            =
//=    None known                                                             =
//=---------------------------------------------------------------------------=
//=  Bugs:                                                                    =
//=    None known                                                             =
//=---------------------------------------------------------------------------=
int recvFile(char *fileName, int portNum, int maxSize, int options)
{
#ifdef WIN
  WORD wVersionRequested = MAKEWORD(1,1);       // Stuff for WSA functions
  WSADATA wsaData;                              // Stuff for WSA functions
#endif
  int                  server_s;        // server socket descriptor
  struct sockaddr_in   server_addr;     // Server Internet address
  int                  connect_s;       // Connection socket descriptor
  struct sockaddr_in   client_addr;     // Client Internet address
  struct in_addr       client_ip_addr;  // Client IP address
  int                  addr_len;        // Internet address length
  char                 in_buf[4096];    // Input buffer for data
  int                  fh;              // File handle
  int                  length;          // Length in received buffer
  int                  retcode;         // Return code
  Packet               pkt;             // Outgoing packet
  Packet               inPkt;           // Incoming packet
  Tcb                  tcb;             // Transfer control block
  double               z;               // Random value to generate packet loss

#ifdef WIN
  // This stuff initializes winsock
  WSAStartup(wVersionRequested, &wsaData);
#endif
  
  // Create a welcome socket
  server_s = socket(AF_INET, SOCK_DGRAM, 0);
  if (server_s < 0)
  {
    printf("*** ERROR - socket() failed \n");
    exit(-1);
  }
  
  // Fill-in server (my) address information and bind the welcome socket
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(portNum);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  retcode = bind(server_s, (struct sockaddr *)&server_addr,
    sizeof(server_addr));
  if (retcode < 0)
  {
    printf("*** ERROR - bind() failed \n");
    exit(-1);
  }
  addr_len = sizeof(client_addr);
  

  // Open IN_FILE for file to write
  #ifdef WIN
    fh = open(fileName, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD |
            S_IWRITE);
  #endif
  #ifdef BSD
    fh = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  #endif
  if (fh == -1)
  {
     printf("  *** ERROR - unable to create '%s' \n", RECV_FILE);
     exit(1);
  }
  
  tcb.expectedSeq = 0;     // First sequence number will be 0
  
  // Receive and write file from udpClient
  do
  {
    //Read incoming packet
    length = recvfrom(server_s, (void *)&inPkt, PKT_SIZE, 0, 
      (struct sockaddr *)&client_addr, &addr_len);
    readPacket(&inPkt);
    
    //IF SYN send SYN ACK
    if (inPkt.flag == SYN)
    {
      printf("Sending SYNACK\n");
      createPacket(&pkt, 0, 0, 0, SYN_ACK);
      sendto(server_s, &pkt, PKT_SIZE, 0, 
        (struct sockaddr *)&client_addr, sizeof(client_addr));
    }
    
    //Packet loss
    if(options == 1){
        z = rand_val();
        if (z <= DISCARD_RATE)
            continue;
    }
    if (inPkt.flag == FIN)
    {
      createPacket(&pkt, 0, 0, 0, FIN_ACK);
      sendto(server_s, &pkt, PKT_SIZE, 0, 
        (struct sockaddr *)&client_addr, sizeof(client_addr));
    }
    	    

    //Packet is correct packet
    if (inPkt.flag == DATA && inPkt.seqNum == tcb.expectedSeq)
    {
      write(fh, inPkt.payload, inPkt.length);
      createPacket(&pkt, 0, 0, ++(tcb.expectedSeq), ACK); 
      sendto(server_s, &pkt, PKT_SIZE, 0, 
	        (struct sockaddr *)&client_addr, sizeof(client_addr));
    }
      
    //Packet contains incorrect seqNum - retransmitted packet after lost ACK
    else if (inPkt.flag == DATA && inPkt.seqNum != tcb.expectedSeq)
    {
        createPacket(&pkt, 0, 0, tcb.expectedSeq, ACK);
        sendto(server_s, &pkt, PKT_SIZE, 0, 
            (struct sockaddr *)&client_addr, sizeof(client_addr));
    }
        
  } while (inPkt.length > 0);

  // Close the received file
  close(fh);

  // Close the welcome and connect sockets
#ifdef WIN
  retcode = closesocket(server_s);
  if (retcode < 0)
  {
    printf("*** ERROR - closesocket() failed \n");
    exit(-1);
  }
  
  
#endif
#ifdef BSD
  retcode = close(server_s);
  if (retcode < 0)
  {
    printf("*** ERROR - close() failed \n");
    exit(-1);
  }
  
#endif

#ifdef WIN
  // Clean-up winsock
  WSACleanup();
#endif

  // Return zero
  return(0);
}
