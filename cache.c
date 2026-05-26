#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functions.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if(2 > num_entries || num_entries > 4096)return -1;
  if(cache != NULL)return -1;

  cache = calloc(num_entries, sizeof(cache_entry_t));
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
  if(cache == NULL)return -1;

  free(cache);
  cache = NULL;
  cache_size = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if(buf == NULL)return -1;
  if(cache == NULL)return -1;
  num_queries++;

  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      if (cache[i].valid == false)return -1;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);     
      cache[i].clock_accesses = clock;
      num_hits++;
      clock++;
      return 1; 
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].clock_accesses = clock;
      clock++;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if(cache == NULL)return -1;
  if(buf == NULL)return -1;
  if(disk_num > JBOD_NUM_DISKS || disk_num < 0)return -1;
  if(block_num > JBOD_NUM_BLOCKS_PER_DISK || block_num < 0)return -1;
  
  int most_recent_entry = 0;
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid){
      return -1;
    }
  }
  
  for (int i = 0; i < cache_size; i++){
    if (cache[i].clock_accesses > cache[most_recent_entry].clock_accesses){ most_recent_entry = i;}
  }

  for (int i = 0; i < cache_size; i++){
    if (cache[i].valid == false){
      most_recent_entry = i;
      break;
    }
  }
  
  memcpy(cache[most_recent_entry].block, buf, JBOD_BLOCK_SIZE);
  cache[most_recent_entry].clock_accesses = clock;
  cache[most_recent_entry].disk_num = disk_num;
  cache[most_recent_entry].block_num = block_num;
  cache[most_recent_entry].valid = true;
  clock++;
  return 1;
}

bool cache_enabled(void) {
  if (cache){return true;}
  return false;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  cache = realloc(cache, (new_num_entries*sizeof(cache_entry_t)));
  return 1;
}

