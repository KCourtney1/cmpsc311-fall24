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

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) {
  if (fd<0){
    return false;
  }

  int bytes_read = 0;
  while (bytes_read < len){
    int read_op = read(fd, buf + bytes_read, len - bytes_read);
    if (read_op == -1){
      printf( "Error reading network data [%s]\n", strerror(errno) );
      return false;
    }
    bytes_read += read_op;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
  if (fd<0){
    return false;
  }

  int bytes_wrote = 0;
  while (bytes_wrote < len){
    int write_op = write(fd, buf + bytes_wrote, len - bytes_wrote);
    if (write_op == -1){
      printf( "Error reading network data [%s]\n", strerror(errno) );
      return false;
    }
    bytes_wrote += write_op;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN + JBOD_BLOCK_SIZE];
  if (!nread(fd, HEADER_LEN, header)) {
    return false;
  }

  memcpy(op, header, sizeof(uint32_t)); // First 4 bytes are the opcode
  *op = ntohl(*op);
  *ret = header[4]; // Info code is the 5th byte

  bool has_block = (*ret & 0x02) != 0;
  if (has_block && block != NULL) {
    if (!nread(fd, JBOD_BLOCK_SIZE, block)) {
      return false;
    }
  }
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int fd, uint32_t op, uint8_t *block) {
  uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE] = {0};
  int len = HEADER_LEN;

  uint32_t net_op =  htonl(op);
  memcpy(packet, &net_op, sizeof(net_op));
  if (((op>>12)&0xFF) == JBOD_WRITE_BLOCK){
    packet[4] |=  0x02;
    memcpy(&packet[HEADER_LEN], block, JBOD_BLOCK_SIZE);
    len += JBOD_BLOCK_SIZE;
  }
  return nwrite(fd, len, packet);
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(JBOD_PORT);

  if(inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }

  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1){
    printf("Error on socket creation [%s]\n", strerror(errno));
    return false;
  }
  if (connect(cli_sd, (const struct sockaddr *)&caddr,  sizeof(caddr)) != 0){
    printf( "Error on socket connect [%s]\n", strerror(errno) );
    return false;
  }
  return true;
}

void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  if (cli_sd == -1){return -1;}

  uint32_t response_op;
  uint8_t info_code;

  if (!send_packet(cli_sd, op, block)) {
    printf("Failed to send packet for operation: 0x%x\n", op);
    return -1;
  }

  if (!recv_packet(cli_sd, &response_op, &info_code, block)) {
    printf("Failed to receive response for operation: 0x%x\n", op);
    return -1;
  }
  if (response_op != op) {return -1;}
  return 0;
}
