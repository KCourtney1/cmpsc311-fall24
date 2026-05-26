#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int mounted = 0;
int canWrite = 0;

int mdadm_mount(void) {
  uint32_t op = (JBOD_MOUNT << 12);
  if (jbod_client_operation(op, NULL) == 0 && mounted != 1){
    mounted = 1;
    return 1;
  }
  return -1;
}

int mdadm_unmount(void) {
  uint32_t op = (JBOD_UNMOUNT << 12);
  if (jbod_client_operation(op, NULL) == 0 && mounted != 0){
    mounted = 0;
    return 1;
  }
  return -1;
}

int mdadm_write_permission(void){
  uint32_t op = (JBOD_WRITE_PERMISSION << 12);
  if (jbod_client_operation(op, NULL) == 0 && canWrite != 1){
    canWrite = 1;
    return 1;
  }
  return -5;
}


int mdadm_revoke_write_permission(void){
  uint32_t op = (JBOD_REVOKE_WRITE_PERMISSION << 12);
    if (jbod_client_operation(op, NULL) == 0 && canWrite != 0){
      canWrite = 0;
      return 1;
    }
    return -1;
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  if (!mounted) {return -3; }// System is unmounted
  if (read_len > 1024) {return -2; }// Read length exceeds limit
  if (start_addr + read_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS){return -1; }
  if (read_buf == NULL && read_len != 0){return -4; }

  uint32_t bytes_read = 0;
  uint8_t  temp_buf[256] = {0};
  uint32_t current_addr = start_addr;
  uint32_t bytes_to_read = read_len;
  
  while (current_addr < (start_addr + read_len)){
    int disk_ID = current_addr / JBOD_DISK_SIZE;
    int block_ID = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int offset = current_addr % JBOD_BLOCK_SIZE; 

    uint32_t seek_disk_op = (JBOD_SEEK_TO_DISK << 12)   | (disk_ID << 0) | (block_ID << 4);
    uint32_t seek_block_op = (JBOD_SEEK_TO_BLOCK << 12) | (disk_ID << 0) | (block_ID << 4);
    uint32_t read_block_op = (JBOD_READ_BLOCK << 12)    | (disk_ID << 0) | (block_ID << 4);
    
    if (cache_lookup(disk_ID, block_ID, temp_buf) == -1){ 
      if (jbod_client_operation(seek_disk_op, NULL) != 0) {return -4; }// Seek to disk failed
      if (jbod_client_operation(seek_block_op, NULL) != 0) {return -4; }// Seek to block failed
      if (jbod_client_operation(read_block_op, temp_buf) != 0) {return -4; }// Reading the block failed
      memcpy(read_buf + bytes_read, temp_buf + offset, bytes_to_read);  
      cache_insert(disk_ID, block_ID, temp_buf);
    }else{
      memcpy (read_buf + bytes_read, temp_buf + offset, bytes_to_read);
    }
 
    current_addr += JBOD_BLOCK_SIZE - offset;
    bytes_read += JBOD_BLOCK_SIZE - offset;
    bytes_to_read -= JBOD_BLOCK_SIZE - offset;
  }
  return read_len;
}



int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
  if (start_addr + write_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS){return -1; }
  if (write_len > 1024) {return -2; }// write length exceeds limit
  if (!mounted) {return -3; }
  if (write_buf == NULL && write_len != 0){return -4; }
  if (canWrite == 0) {return -5;}

  uint32_t bytes_wrote = 0;
  uint32_t bytes_to_write = write_len;
  uint32_t current_addr = start_addr;
  uint8_t  temp_buf[256] = {0};
  while (bytes_to_write > 0){
    int disk_ID = current_addr / JBOD_DISK_SIZE;
    int block_ID = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int offset = current_addr % JBOD_BLOCK_SIZE;

    uint32_t seek_disk_op = (JBOD_SEEK_TO_DISK << 12)   | (disk_ID << 0) | (block_ID << 4);
    uint32_t seek_block_op = (JBOD_SEEK_TO_BLOCK << 12) | (disk_ID << 0) | (block_ID << 4);
    uint32_t write_block_op = (JBOD_WRITE_BLOCK << 12)  | (disk_ID << 0) | (block_ID << 4);
    uint32_t read_block_op = (JBOD_READ_BLOCK << 12)    | (disk_ID << 0) | (block_ID << 4);
    int write_size = (bytes_to_write < JBOD_BLOCK_SIZE - offset) ? bytes_to_write : JBOD_BLOCK_SIZE - offset;
  
    if (jbod_client_operation(seek_disk_op, NULL) != 0) {return -4; }// Seek to disk failed
    if (jbod_client_operation(seek_block_op, NULL) != 0) {return -4; }// Seek to block failed
    if (jbod_client_operation(read_block_op, temp_buf) != 0) {return -4; }// Reading the block failed  
 
    memcpy(temp_buf + offset, write_buf + bytes_wrote, write_size);
    if (jbod_client_operation(seek_disk_op, NULL) != 0) {return -4; }// Seek to disk failed
    if (jbod_client_operation(seek_block_op, NULL) != 0) {return -4; }// Seek to block failed
    if (jbod_client_operation(write_block_op, temp_buf) != 0) {return -4; }// writing the block failed

    int lookupRes = cache_lookup(disk_ID, block_ID, temp_buf);
    if (lookupRes == -1){
      cache_update(disk_ID, block_ID, temp_buf);
    }else{
      cache_insert(disk_ID, block_ID, temp_buf);
    }

    current_addr += write_size;
    bytes_wrote += write_size;
    bytes_to_write -= write_size;
  }
  return write_len;
}