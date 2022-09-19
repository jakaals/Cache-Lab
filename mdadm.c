#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

//char *stringify(const uint8_t *buf, int length);

//Global variable mounted keeps track of whether or not the disks are currently mounted
int mounted = 0;

//Enum stores bit values of basical JDOB commands
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


int mdadm_mount(void) {
  //mounts the disks
  int x;
  if (mounted == 0){
  x = jbod_client_operation(MOUNT, NULL);
  mounted = 1;
  if (x==0){return 1;}
  }
  else if (mounted == 1){
    return -1;
    }
  return -1;
}

int mdadm_unmount(void) {
  //unmounts the disks
  int x;
  if (mounted == 1){
  x = jbod_client_operation(UNMOUNT, NULL);
  mounted = 0;
  if (x==0){
    return 1;}
  }
  else if (mounted == 0){
    return -1;
    }
  return -1;
}
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  //initializes *tempbuf value with maximum size to account for random trace.
  uint8_t tempbuf[256];
  uint32_t temp = addr;
  int counter = 0;
  //Generates original diskID and blockID numbers, code below makes sure nothing is out of bounds
  if (mounted == 0){return -1;}  
  if (len > 1024){return -1;}
  if (addr + len > 1048576){return -1;}  
  if (buf == NULL){
    if (len == 0){return 0;}
    else{return -1;}
  }
  //loops while temp doesn't surpass the total of address and length to ensure no over-reading
  while (temp < (addr + len)){
    int diskID = temp / JBOD_DISK_SIZE;
    int blockID = (temp % JBOD_DISK_SIZE) / 256;
    int remaining = temp % 256;
    int cached = 0;

    //Checks if cache is enabled, if so checks for disk and block combination in cache
    if(cache_enabled()){
      cached = cache_lookup(diskID,blockID,tempbuf);

      //If not in cache, runs default JBOD operations and saves to cache
      if (cached == -1){
        jbod_client_operation(SEEKDISK | (diskID<<22),NULL);
        jbod_client_operation(SEEKBLOCK | (blockID),NULL);
        jbod_client_operation(READBLOCK,tempbuf);
        cache_insert(diskID,blockID,tempbuf);
      }
    }

    //Runs default JBOD operations if cache not enabled
    if (cache_enabled() == false){
    jbod_client_operation(SEEKDISK | (diskID<<22),NULL);
    jbod_client_operation(SEEKBLOCK | (blockID),NULL);
    jbod_client_operation(READBLOCK,tempbuf);
    }
    int size = 0;
    //first block
    if (temp == addr){
      if (len < 256 - remaining){
        size = len;
      }
      else{
        size = 256 - remaining;
      }
    }
    else if ((len - counter) < 256){
      size = len - counter;
    }
    else {
      size = 256;
    }
    //copies read item to buf at location counter
    memcpy(buf+counter,tempbuf + remaining,size);
    counter += size;
    temp += size;

  }
  
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  //initializes *tempbuf value with maximum size to account for random trace.
  uint8_t tempbuf[JBOD_BLOCK_SIZE - 1];
  uint32_t temp = addr;
  int counter = 0;
  //Generates original diskID and blockID numbers, code below makes sure nothing is out of bounds
  if (mounted == 0){return -1;}  
  if (len > 1024){return -1;}
  if (addr + len > 1048576){return -1;}  
  if (buf == NULL){
    if (len == 0){return 0;}
    else{return -1;}
  }
  //Same loop as read; ensures no overwriting
  while (temp < (addr + len)){
    int diskID = temp / JBOD_DISK_SIZE;
    int blockID = (temp % JBOD_DISK_SIZE) / 256;
    int remaining = temp % 256;
    int cached = 0;

    //Checks if cache is enabled, if so checks for the disk and block combination inside of the cache
    if(cache_enabled()){
      cached = cache_lookup(diskID,blockID,tempbuf);
      if(cached == 1){
      jbod_client_operation(SEEKDISK | (diskID<<22),NULL);
      jbod_client_operation(SEEKBLOCK | (blockID),NULL);
      }

      //If the item is not in the cache, run default JBOD method and save it to cache
      if (cached == -1){
        if(jbod_client_operation(SEEKDISK | (diskID<<22),NULL)!=0){return -1;}
        if(jbod_client_operation(SEEKBLOCK | (blockID),NULL)!=0){return -1;}
        jbod_client_operation(READBLOCK,tempbuf);
        cache_insert(diskID,blockID,tempbuf);
        if(jbod_client_operation(SEEKDISK | (diskID<<22),NULL)!=0){return -1;}
        if(jbod_client_operation(SEEKBLOCK | (blockID),NULL)!=0){return -1;}
      }
    }

    //Default jbod operations, only runs when cache is not enabled
    if (cache_enabled() == false){
    jbod_client_operation(SEEKDISK | (diskID<<22),NULL);
    jbod_client_operation(SEEKBLOCK | (blockID),NULL);
    jbod_client_operation(READBLOCK,tempbuf);
    jbod_client_operation(SEEKDISK | (diskID<<22),NULL);
    jbod_client_operation(SEEKBLOCK | (blockID),NULL);
    }
    //first block
    int size = 0;
    if (temp == addr){
      if (len < 256 - remaining){
        size = len;
      }
      else{
        size = 256 - remaining;
      }
    }
    else if ((len - counter) < 256){
      size = len - counter;
    }
    else {
      size = 256;
    }
    //copies found data from entered buf to tempbuf then writes into JBOD
    memcpy(tempbuf + remaining,buf + counter,size);
    counter += size;
    temp += size;
    jbod_client_operation(WRITEBLOCK,tempbuf);
    cache_update(diskID,blockID,tempbuf);
    
  
  }


  
  return len;
}

