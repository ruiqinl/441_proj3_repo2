#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "helper.h"
#include "dns_lib.h"
#include "nameserver.h"
#include "list.h"

#ifndef TEST

int main(int argc, char *argv[]) {
  if (argc < 6) {
    printf("Usage: ./nameserver [-r] <log> <ip> <port> <servers> <LSAs>\n");
    exit(-1);
  }

  int round_robin;
  char *log;
  char *ip;
  short port;
  char *servers;
  char *LSAs;

  int sock;
  char *query_buf, *reply_buf;
  struct sockaddr_in client_addr, addr;
  socklen_t client_len;
  int recv_ret, send_ret;
  struct dns_t *query = NULL;
  //struct server_t *serverlist = NULL;
  struct list_node_t *serverlist = NULL;
  //static struct server_t *picked_server = NULL;
  int reply_len = 0;
  uint32_t next_ip;
  
  if (strcmp(argv[1], "-r") == 0) {
    round_robin = 1;
    log = argv[2];
    ip = argv[3];
    port = atoi(argv[4]);
    servers = argv[5];
    LSAs = argv[6];

    serverlist = get_serverlist(servers);
    //picked_server = serverlist;
    
  } else {
    round_robin = 0;
    log = argv[1];
    ip = argv[2];
    port = atoi(argv[3]);
    servers = argv[4];
    LSAs = argv[5];
  }


  // sock
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("Error! nameserver, socket");
    exit(-1);
  }
  
  addr.sin_family = AF_INET;
  if (inet_aton(ip, &addr.sin_addr) == 0) {
    perror("Error! nameserver, inet_aton\n");
    exit(-1);
  }
  //addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("nameserver: bind\n");
    exit(-1);
  }

  // recvfrom
  query_buf = (char *)calloc(BUF_SIZE, sizeof(char));
  while (1) {
    memset(&client_addr, 0, sizeof(client_addr));
    memset(query_buf, 0, BUF_SIZE);

    printf("nameserver: ready to recvfrom\n");
    recv_ret = recvfrom(sock, query_buf, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

    
    if (recv_ret == -1) {
      perror("Error! nameserve, recvfrom\n");
      exit(-1);
    }
    
    // udp, a packet is recvd as a whole
    if (recv_ret > 0) {
      dbprintf("nameserver: recved %d bytes\n", recv_ret);

      query = parse_dns(query_buf);
      print_dns(query);
      
      if (round_robin) {

	next_ip = next_server(serverlist);
	reply_buf = cnd_rr(query, next_ip, &reply_len);

      } else {

	reply_buf = cnd_geo_dist(query, &reply_len);
      }

      printf("nameserver: send reply back to proxy\n");
      assert(reply_len != 0);
      printf("??sock:%d\n", sock);
      printf("??reply_buf addr:%p\n", reply_buf);
      printf("??client ip:%s\n", inet_ntoa(client_addr.sin_addr));
      printf("??clietn len:%d\n", client_len);
      if ((send_ret = sendto(sock, reply_buf, reply_len, 0, (struct sockaddr *)&client_addr, client_len)) != reply_len) {
	perror("Error! nameserver, sendto\n");
	exit(-1);
      }
      
      free(reply_buf);
    }

  }
  
  free(query_buf);
}

#endif //TEST

struct list_node_t *get_serverlist(char *servers) {
  assert(servers != NULL);
  dbprintf("get_server_list:\n");

  //static struct server_t *server_list = NULL;
  FILE *fp = NULL;
  struct list_node_t *serverlist = NULL;
  int size = 128;
  char line[size];
  struct in_addr tmp;
  uint32_t *ip = NULL;

  //init_serverlist(&serverlist);

  if ((fp = fopen(servers, "r")) == NULL) {
    perror("Error! cnd_rr, fopen\n");
    exit(-1);
  }

  memset(line, 0, size);
  while (fgets(line, size, fp) != NULL){
    line[strlen(line)-1] = '\0';

    // inet_aton
    memset(&tmp, 0, sizeof(tmp));
    if (inet_aton(line, &tmp) == 0) {
      perror("Error! cnd_rr, inet_aton\n");
      exit(-1);
    }
    
    // save s_addr in allocated mem
    ip = (uint32_t *)calloc(1, sizeof(uint32_t));
    memcpy(ip, &tmp.s_addr, sizeof(uint32_t));
    
    push(&serverlist, ip); // uint32_t
    memset(line, 0, size);
  }
  
  //print_list(serverlist, printer_hex);

  return serverlist;
}

uint32_t next_server(struct list_node_t *list) {
  assert(list != NULL);

  static int ind = 0;
  int list_len;
  int count = 0;

  list_len = list_size(list);
  assert(ind <= list_len);
    
  if (ind == list_len) 
    ind = 0;

  count = 0;
  while (count < ind) {
    assert(list != NULL);
    list = list->next;
    ++count;
  }

  ++ind;

  return *(uint32_t *)list->data;
}


void printer_hex(void *data) {
  assert(data != NULL);

  uint32_t ip = *(uint32_t *)data;
  printf("%x, ", ip);
}


char *cnd_rr(struct dns_t *query, uint32_t ip, int *len) {
  assert(query != NULL);
  assert(ip != 0x00);

  char *reply = NULL;
  
  reply = make_dns_reply(query, ip, len);
  dbprintf("cnd_rr: choose ip %x\n", ip);

  return reply;
}

char *cnd_geo_dist(struct dns_t *query, int *len) {
  assert(query != NULL);
  
  printf("cnd_geo_dist: not imp yet\n");

  //
  printf("Now, just return 15441\n");
  char * ret = (char *)calloc(1024, sizeof(char));
  
  memcpy(ret, "15441", strlen("15441"));

  return ret;
}


#ifdef TEST

int main(){

  struct list_node_t *servers = NULL;

  printf("test get_serverlist:\n");
  servers = get_serverlist("./topos/topo2/topo2.servers");
  print_list(servers, printer_hex);

  int i;
  uint32_t ip;
  for (i = 0; i < 20; i++) {
    ip = next_server(servers);
    printf("%x, ", ip);
  }
  printf("\n");

  return 0;
}

#endif 
