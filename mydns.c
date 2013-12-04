#include <stdio.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include "helper.h"
#include "mydns.h"
#include "dns_lib.h"

static const char *dns_ip = NULL;
static unsigned int dns_port = 0;

int init_mydns(const char *ip, unsigned int port) {
  assert(ip != NULL);
  assert(port > 0);

  dns_ip = ip;
  dns_port = port;

  return 0;
}

int resolve(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  assert(node != NULL);
  assert(service != NULL);
  assert(res != NULL);

  struct sockaddr_in addr;
  char *dns_query = NULL;
  char *dns_reply = NULL;
  int sock;
  int ret;  
  int query_len;

  // make query packet
  //dns_query = make_dns_query(node, service);
  dns_query = make_dns_query(node, &query_len);
  struct dns_t *tmp = parse_dns(dns_query);
  print_dns(tmp);
  
  // udp socket to dns server
  printf("resolve: send query of %d bytes to dns_ip:%s, dns_port:%d\n", query_len, dns_ip, dns_port);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  if (inet_aton(dns_ip, &addr.sin_addr) == 0) {
    perror("Error! main, inet_aton");
    exit(-1);
  }
  addr.sin_port = htons(dns_port);
  
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("Error! mydns, socket");
    exit(-1);
  }

  // send
  if (sendto(sock, dns_query, query_len, 0, (struct sockaddr *)&addr, sizeof(addr)) != query_len) {
    perror("Error! mydns, sendto, maybe use while");
    exit(-1);
  }

  // recv
  dns_reply = (char *)calloc(BUF_SIZE, sizeof(char));

  ret = recvfrom(sock, dns_reply, BUF_SIZE, 0, NULL, NULL);
  printf("resolve: recvd %s\n", dns_reply);

  if (ret == -1) {
    perror("Error! mydns, recvfrom");
    exit(-1);
  }
  
  // parse it 
  *res = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
  (*res)->ai_family = AF_INET;
  (*res)->ai_addr = parse_dns_reply(dns_reply);
  (*res)->ai_next = NULL;

  free(dns_reply);
  return 0;
}

#ifdef TEST

int main() {

  int count = get_qdcount("www.google.com");
  assert(count == 3);
  printf("get_qdcount passed test!\n");

  int query_len;
  char *dns_query = make_dns_query("www.google.com", &query_len);
  
  struct dns_t *query = parse_dns(dns_query);
  print_dns(query);

  return 0;
}

#endif
