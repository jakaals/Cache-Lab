#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int lastInsert = -1;
static int num_queries = 0;
static int num_hits = 0;
//Clock replaced with accesstime variable of same meaning
int accesstime = 0;
bool cacheActive = false;


int cache_create(int num_entries) {
  //creates an array of cache_entry_t and sets it's location to cache
  if(cacheActive == true){return -1;}
  if(num_entries < 2 || num_entries > 4096){return -1;}
  cacheActive = true;
  cache_size = num_entries;
  num_hits = 0;
  num_queries = 0;

  lastInsert = 0;
  accesstime = 0;
  //code below initializes cache data
  cache = calloc(num_entries,sizeof(cache_entry_t));

  return 1;
}

int cache_destroy(void) {
  //deletes the cache and resets important static variables
  if(cacheActive == false){return -1;}
  cacheActive = false;
  free(cache);
  cache = NULL;
  cache_size = 0;
  lastInsert = -1;
  accesstime = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //Fail cases
  if(cacheActive == false || lastInsert == -1 || accesstime == 0 || buf == NULL){return -1;}
    num_queries +=1;
    int i = 0;
    
    //Searches entire cache for a valid item matching the disk and block
    //If successful : copies block to buf and adds 1 to num_hits, if fails returns -1
    for(i=0;i<cache_size;i++){
    if(cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true){
      num_hits +=1;
      memcpy(buf,cache[i].block,256);
      accesstime += 1;
      cache[i].access_time = accesstime;
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  //updates an item in the cache to a new buffer value and updates its access time
  int i = 0;
  for(i=0;i<cache_size;i++){
    if(cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true){
      accesstime += 1;
      cache[i].access_time = accesstime;
      memcpy(cache[i].block,buf,256);

    }

  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //returns -1 if any fail cases are triggered
  if(cacheActive == false || buf == NULL || 15 < disk_num || disk_num < 0 || 255 < block_num || block_num < 0){return -1;}
  
  int i = 0;
  for(i=0;i<cache_size;i++){
    //Breaks loop early if a cache item with false is detected - meaning there is no more saved data past that point
    //Returns -1 if a duplicate disk/block is detected
    if(cache[i].valid == false){break;}
    if(cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true){return -1;}
  }
  //Stores intial data in cache while empty
  if (lastInsert != cache_size - 1 ){
    accesstime += 1;
    cache[lastInsert].valid = true;
    cache[lastInsert].disk_num = disk_num;
    cache[lastInsert].block_num = block_num;
    memcpy(cache[lastInsert].block,buf,256);
    cache[lastInsert].access_time = accesstime;
    lastInsert +=1;
    return 1;
  }
  //Follows LRU system once full and overwrites older cache items
  //Smaller access_time means older
  else{
    i = 0;
    int temp = accesstime + 1;
    int location = 0;
    for(i=0;i<cache_size;i++){
      if (temp >= cache[i].access_time){
        temp = cache[i].access_time;
        location = i;}
    }
    //Overwriting code
    accesstime += 1;
    cache[location].valid = true;
    cache[location].disk_num = disk_num;
    cache[location].block_num = block_num;
    memcpy(&cache[location].block[0],buf,256);
    cache[location].access_time = accesstime;
    return 1;
  }
  return -1;
}


bool cache_enabled(void) {
  //returns cacheActive state, which is either True or false
  return cacheActive;
}

void cache_print_hit_rate(void) {
  //prints hit rate from the hit rate formula
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
