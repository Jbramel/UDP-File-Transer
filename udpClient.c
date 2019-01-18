//================================================== file = udpClient.c =====
//=  A file transfer program using TCP - this is the file send                =
//=============================================================================
//=  Notes:                                                                   =
//=    1) This program  compiles for BSD sockets only.     
//=    2) This program sends a file to udpServer.c using Reliable UDP Protocol=
//=    3) This program take command line input as shown below                 =
//=    4) Ignore build warnings on implicit declarations of functions.        =
//=---------------------------------------------------------------------------=
//=  Example execution: (./udpClient sendFile.dat 127.0.0.1 1050 0)           =
//=    Starting file transfer...                                              =
//=    File transfer is complete                                              =
//=---------------------------------------------------------------------------=
//=  Build: gcc udpClient.c udpProtocol.c -lnsl for BSD                                   
//=---------------------------------------------------------------------------=
//=  Execute: ./udpClient [sendFile] [destIP] [destPort] [packetLoss?0:1]                                                     
//=---------------------------------------------------------------------------=
//=  Author: Justin Bramel                                                    =
//=          University of South Florida                                      =
//=          Email: jbramel@mail.usf.edu                                    =
//=---------------------------------------------------------------------------=
//=  History:  11/27/2017 - Final version                                     =
//=============================================================================
#define  BSD               // WIN for Winsock and BSD for BSD sockets

//----- Include files ---------------------------------------------------------
#include <stdio.h>          // Needed for printf()
#include <string.h>         // Needed for memcpy() and strcpy()
#include <stdlib.h>         // Needed for exit()
#include <fcntl.h>          // Needed for file i/o constants
#include <string.h>         // Needed for strcpy()
#include <math.h>           // Needed for fabs()
#include <ctype.h>
#include "udpProtocol.h"
#ifdef WIN
  #include <windows.h>      // Needed for all Winsock stuff
  #include <io.h>           // Needed for open(), close(), and eof()
  #include <sys/stat.h>     // Needed for file i/o constants
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
  #include <sys/select.h>   //needed for select
#endif

//----- Defines ---------------------------------------------------------------
#define  PORT_NUM    1050   // Port number used at the server
#define  SIZE        496    // Buffer size
#define DISCARD_RATE 0.02   // Discard rate (from 0.0 to 1.0)
#define G            0.125  // Used to sample RTO
#define H            0.25   // Used to sample RTO
#define F            4      // Used to sample RTO
//----- Prototypes ------------------------------------------------------------
int sendFile(char *fileName, char *destIpAddr, int destPortNum, int options);

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
  char                 sendFileName[256];   // Send file name
  char                 recv_ipAddr[16];     // Reciver IP address
  int                  recv_port;           // Receiver port number
  int                  options;             // Options
  int                  retcode;             // Return code

  // Usage and parsing command line arguments
  if (argc != 5)
  {
    printf("usage: 'projectServer sendFile recvIpAddr recvPort emul' where \n");
    printf("       sendFile is the filename of an existing file to be sent \n");
    printf("       to the receiver, recvIpAddr is the IP address of the    \n");
    printf("       receiver, recvPort is the port number for the       \n");
    printf("       receiver where udpServer is running,and emul is whether \n");
    printf("       to emulate or not a packet loss                         \n");
    return(0);
  }
  strcpy(sendFileName, argv[1]);
  strcpy(recv_ipAddr, argv[2]);
  recv_port = atoi(argv[3]);

  // Initialize parameters
  options = atoi(argv[4]);

  // Send the file
  printf("Starting file transfer... \n");
  retcode = sendFile(sendFileName, recv_ipAddr, recv_port, options);
  printf("File transfer is complete \n");

  // Return
  return 0;
}

//=============================================================================
//=  Function to send a file using UDP                                        =
//=============================================================================
//=  Inputs:                                                                  =
//=    fileName ----- Name of file to open, read, and send                    =
//=    destIpAddr --- IP address or receiver                                  =
//=    destPortNum -- Port number receiver is listening on                    =
//=    options ------ Options whether to emulate packet loss or not           =
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
int sendFile(char *fileName, char *destIpAddr, int destPortNum, int options)
{
#ifdef WIN
  WORD wVersionRequested = MAKEWORD(1,1);       // Stuff for WSA functions
  WSADATA wsaData;                              // Stuff for WSA functions
#endif
  int                  client_s;        // Client socket descriptor
  struct sockaddr_in   server_addr;     // Server Internet address
  char                 out_buf[4096];   // Output buffer for data
  int                  fh;              // File handle
  int                  length;          // Length of send buffer
  int                  retcode;         // Return code
  int                  sel;		        // Return code for select
  fd_set	           recvsds;         // Used for time out
  struct timeval       timeout;		    //struct for time interval for select
  struct timeval       te;              //struct for current time
  unsigned int 		   addr_len;        // Length of server address
  double               rtt;             // RTT for a given packet
  double               srtt;            // Used to sample RTO
  double               serr;            // Used to sample RTO
  double               sdev;            // USed to sample RTO
  int                  rto;             // RTO for the timeout
  unsigned long long   startTime;       // UNIX start time (in ms)
  unsigned long long   endTime;         // UNIX end time (in ms)
  Tcb                  tcb;             // Transfer control block
  Packet               pkt;             // Outgoing packet
  Packet               inPkt;           // Incoming packet
  int                  reset;           // Flag to sample RTT
  double               z;               // Random value used for packet loss
  int 		       dup; 		//for regocnizing duplicate packets

#ifdef WIN
  // This stuff initializes winsock
  WSAStartup(wVersionRequested, &wsaData);
#endif

  // Create a client socket
  client_s = socket(AF_INET, SOCK_DGRAM, 0);
  if (client_s < 0)
  {
    printf("*** ERROR - socket() failed \n");
    exit(-1);
  }

  // Fill-in the server's address information and do a connect
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(destPortNum);
  server_addr.sin_addr.s_addr = inet_addr(destIpAddr);
  addr_len = sizeof(server_addr);
  
  // Open file to send
  #ifdef WIN
    fh = open(fileName, O_RDONLY | O_BINARY, S_IREAD | S_IWRITE);
  #endif
  #ifdef BSD
    fh = open(fileName, O_RDONLY, S_IREAD | S_IWRITE);
  #endif
  if (fh == -1)
  {
     printf("  *** ERROR - unable to open '%s' \n", fileName);
     exit(1);
  }

  tcb.nextSeq = 0;   // The first sequence number is 0
  reset =1;          // Set to 1 to start sampling from the beginnign
  srtt = 100;       // 100ms, assume initially it will take one second
  sdev = 0;          // Set to 1 initially to keep RTO unchanged
  rto = 99;         // RTO is initially just below 0
  dup = 0;
  int numDups = 0;

  //Initiate SYN/SYN ACK SEQUENCE
   while(1)
   {
     createPacket(&pkt, 1, 0, 0, SYN);
     //clear and set recv descriptor 
     FD_ZERO(&recvsds);
     FD_SET((unsigned int) client_s, &recvsds);
     //set timeout to 2 seconds if reset == 1 else use remaining time
     timeout.tv_sec = 0;
     timeout.tv_usec = rto*1000;
 
     //SEND SYN AND START TIMER
     sendto(client_s, &pkt, PKT_SIZE, 0, 
         (struct sockaddr *)&server_addr, sizeof(server_addr));
     sel = select(client_s + 1, &recvsds, NULL, NULL, &timeout);
     if (sel == 0) 
     {  
        rto = rto*2;
  	continue;
     }
     else 
     {
       //Receive ACK packet
       recvfrom(client_s, (void *)&inPkt, PKT_SIZE, 0,
           (struct sockaddr *)&server_addr, &addr_len);
       readPacket(&inPkt);
       if (inPkt.flag == SYN_ACK) 
       {  
   
         break;
         
       }
     }
   }


  // Read and send the file to the receiver
  do
  {
    //Read packet data - place into packet payload - fill in packet header
    length = read(fh, pkt.payload, PAYLOAD_SIZE); 
    createPacket(&pkt, length, tcb.nextSeq++, 0, DATA);
    
    if (length > 0)
    {
        
        while(1)
        {
            //clear and set recv descriptor 
            FD_ZERO(&recvsds);
            FD_SET((unsigned int) client_s, &recvsds);
        
	    //set timeout to 2 seconds if reset == 1 else use remaining time
	    timeout.tv_sec = 0;
            timeout.tv_usec = rto*1000;

            gettimeofday(&te,NULL);
            startTime = te.tv_sec*1000LL + te.tv_usec/1000; // transfrom to ms
            
            //packet loss
            if(options){
                z = rand_val();
                if (z > DISCARD_RATE && dup != 1) {
	                sendto(client_s, &pkt, PKT_SIZE, 0, 
                            (struct sockaddr *)&server_addr, sizeof(server_addr));   	
                }
            }
            else if (dup != 1) {
	            sendto(client_s, &pkt, PKT_SIZE, 0, 
	                    (struct sockaddr *)&server_addr, sizeof(server_addr));  
            }
            
	    //call select() (start timeout)
            sel = select(client_s + 1, &recvsds, NULL, NULL, &timeout);
	        
            //Timeout has occurred - GOTO beginning of while(1) to retransmit       
	        if (sel == 0)
            {
                reset = 0; // retransmit packet
	        rto = rto*2;          
            }	
	        
            //Ack packet has been received (could be delayed ack from previous
            //timeout)
            else
	    {   
	            //Receive ACK packet
	            recvfrom(client_s, (void *)&inPkt, PKT_SIZE, 0,
                       (struct sockaddr *)&server_addr, &addr_len);
                    readPacket(&inPkt);
                // Previous packet was recevied, this is not a recvfrom a 
                // retransmission
                if(reset == 1 && inPkt.flag == ACK && inPkt.ackNum == tcb.nextSeq){ 
                    gettimeofday(&te,NULL);
                    endTime = te.tv_sec*1000LL + te.tv_usec/1000;
                    rtt = endTime-startTime; // rtt is in ms
                    if(rtt == 0) rtt = 1;
                    srtt = (1-G)*srtt+G*rtt;
                    serr = rtt-srtt;
                    sdev = (1-H)*sdev+H*fabs(serr);
                    rto = srtt+F*sdev;
                    
                }
	            
                
                //If packet is ACK and contains proper ackNum, send next packet
                //with new timeout
	        if (inPkt.flag == ACK && inPkt.ackNum == tcb.nextSeq)
                {
	            reset = 1;
                    dup = 0; 
	            break;
	        }
                //duplicate packet - do nothing and use remaining timeout   
	        else if (inPkt.flag == ACK && inPkt.ackNum < tcb.nextSeq) 
		{
                  dup = 1;
                  rto = timeout.tv_sec*1000LL + timeout.tv_usec/1000;
                  numDups++;
                  
                }
                else 
                {
                  reset = 0;
                  dup = 0;
                }
	    }
        }
    }
    else //send FIN to terminate connection
    {
      createPacket(&pkt, 0, 0, 0, FIN);
      sendto(client_s, &pkt, PKT_SIZE, 0, 
              (struct sockaddr *)&server_addr, sizeof(server_addr));
    }
  } while ( length > 0);

  printf("numDuplicates: %d\n",numDups);

  // Close the file that was sent to the receiver
  close(fh);

  // Close the client socket
#ifdef WIN
  retcode = closesocket(client_s);
  if (retcode < 0)
  {
    printf("*** ERROR - closesocket() failed \n");
    exit(-1);
  }
#endif
#ifdef BSD
  retcode = close(client_s);
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
