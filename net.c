#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

//Describes commands for use in detecting READ or WRITE
//Technically only read and write is needed.
enum {
  MOUNT = 0,
  UNMOUNT = 1<<26,
  SEEKDISK = 2<<26,
  SEEKBLOCK = 3<<26,
  READBLOCK = 4<<26,
  WRITEBLOCK = 5<<26,
  SIGNBLOCK = 6<<26,
  NUMCMD = 7<<26,
} commands;

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int readSoFar = 0;
  int remaining = len;
  while (readSoFar < len) {
    //Keeps track of how many bytes are read to ensure none are missed
    readSoFar += read(fd,buf + readSoFar,remaining);
    remaining = remaining - readSoFar;
    if (readSoFar == -1) {
      //returns false if a server error occurs
      return false;
    }
  }
  return true;
}


/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int writtenSoFar = 0;
  int remaining = len;
  while (writtenSoFar < len) {
    //Keeps track of how many bytes are written to ensure none are missed
    writtenSoFar += write(fd,buf + writtenSoFar,remaining);
    remaining = remaining - writtenSoFar;
    if (writtenSoFar == -1) {
      //Returns false if a server error occurs
      return false;
    }
  }
  return true;
}


//Attempts to receive a packet from sd
/*
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 
op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  if(sd == -1){return false;}

  //initializing variables
  uint8_t header[HEADER_LEN];
  uint16_t lenfound[2];
  uint16_t len = 0;
  uint32_t opfound[4];
  uint16_t retfound[2];
  
  //reads received header into header buffer
  if (nread(sd,HEADER_LEN,header) == false) {
    return false;}
  else{
    //break header into components (len,op,ret) (first 2 bytes, next 4 bytes, and next 2 bytes)
    memcpy(&lenfound,&header[0],2);
    memcpy(&opfound,&header[2],4);
    memcpy(&retfound,&header[6],2);
    len = ntohs(*lenfound);
    *ret = ntohs(*retfound);
    *op = ntohl(*opfound);
    //Checks if received packet command is READ
    if(len > HEADER_LEN){
      nread(sd,256,block);
    }
    return true;
  }

  return false;
}


//Attempts to send a JBOD packet to the server socket
/*
op - Operation code
block - If command is WRITE, block contains data to write to server. Otherwise NULL
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  if(sd == -1){return false;}
  uint32_t command = op >> 26;
  uint16_t len;

  //Checks if command is WRITE command
  if (command == JBOD_WRITE_BLOCK){
    len = 264;
  }
  else{
    len = 8;
  }

  //Creates header
  uint8_t *header = (uint8_t *) malloc(len);
  uint16_t outlen = htons(len);
  command = htonl(op);

  //Copies len and command data to header
  //Return code is set to blank data to prevent issues with packet
  memcpy(&header[0],(uint8_t *)&outlen,2);
  memcpy(&header[2],(uint8_t *)&command,4);
  memset(&header[6],0,2);

  //Checks if operation is a WRITE, and if so adds the block to the header
  if (len > 8){
    memcpy(header+8,block,JBOD_BLOCK_SIZE);
  }

  //nwrites data to server
  return nwrite(sd,len,header);
}



//Attempts to connect to server and set cli_sd variable to socket
//returns true on success
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;

  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }

  //Creates socket for connecting to server with
  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1){
    return false;
  }

  //Attempts to connect socket to server
  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1 ){
    return false;
  }
  
  return true;
}




// disconnects from the server and resets cli_sd to -1
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}




//Sends JBOD operation to server using send_packet function, then receives via recv_packet function and processes the response
// 0 = Success, -1 = Success
int jbod_client_operation(uint32_t op, uint8_t *block) {
  if (cli_sd == -1){return -1;}
  
  //Sends first command out to Server, returns false if it fails
  if (send_packet(cli_sd,op,block) == false) {return -1;}
  
  //receives response from server and saves return value from the server
  uint16_t ret;
  if (recv_packet(cli_sd,&op,&ret,block) == false) {return -1;}
  if (ret==-1){return -1;}
  return 0;
}
