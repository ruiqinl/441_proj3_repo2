#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "helper.h"
#include "dns_lib.h"
#include "nameserver.h"
#include "list.h"
#include "graph.h"

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

  int **graph = NULL;
  int graph_size;
  struct list_node_t *lsa_list = NULL;
  struct list_node_t *ip_list = NULL;
  int client_ind;//, server_ind;
  struct list_node_t *server_ind_list = NULL;
  
  if (strcmp(argv[1], "-r") == 0) {
    round_robin = 1;
    log = argv[2];
    ip = argv[3];
    port = atoi(argv[4]);
    servers = argv[5];
    LSAs = argv[6];

    serverlist = get_serverlist(servers);
    
  } else {
    round_robin = 0;
    log = argv[1];
    ip = argv[2];
    port = atoi(argv[3]);
    servers = argv[4];
    LSAs = argv[5];

    graph = make_graph(LSAs, &graph_size, &lsa_list, &ip_list);
    serverlist = get_serverlist(servers);

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
    client_len = sizeof(struct sockaddr_in);
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
	dbprintf("nameserver: round_robin\n");

	next_ip = next_server(serverlist);
	reply_buf = cnd_rr(query, next_ip, &reply_len);

      } else {
	dbprintf("nameserver: geo_dist\n");
	assert(graph != NULL);
	assert(graph_size > 0);
	assert(lsa_list != NULL);
	assert(ip_list != NULL);
	assert(serverlist != NULL);
	
	client_ind = get_client_ind(&client_addr, ip_list);
	server_ind_list = get_server_ind(serverlist, ip_list);
	printf("client_ind:%d\n", client_ind);
	printf("server_ind_list:\n");
	print_list(server_ind_list, printer_int);
	
	reply_buf = cnd_geo_dist(query, &reply_len, graph, graph_size, client_ind, server_ind_list, serverlist);
      }

      dbprintf("nameserver: send reply back to proxy\n");      
      assert(reply_len != 0);
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


char *cnd_rr(struct dns_t *query, uint32_t ip, int *len) {
  assert(query != NULL);
  assert(ip != 0x00);

  char *reply = NULL;
  
  reply = make_dns_reply(query, ip, len);
  dbprintf("cnd_rr: choose ip %x\n", ip);

  return reply;
}

char *cnd_geo_dist(struct dns_t *query, int *len, int **graph, int graph_size, int client_id, struct list_node_t *server_ind_list, struct list_node_t *server_list) {
  assert(query != NULL);
  assert(len != NULL);
  assert(client_id >= 0);
  assert(server_ind_list != NULL);
  assert(server_list != NULL);
  
  int i;
  int s_num;
  int min_dist;
  int dist;
  int server_id;
  int picked_id;
  struct list_node_t *picked_server = NULL;
  char *reply = NULL;
  uint32_t ip;
  struct list_node_t *server_p;

  min_dist = MAX_DIST;
  s_num = list_size(server_ind_list);
  server_p = server_ind_list;

  for (i = 0; i < s_num; i++) {

    server_id = *(int *)(server_p->data);
    dist = do_dijkstra(graph, graph_size, client_id, server_id);
    printf("cnd_geo_dist: dist to server_%d is %d\n", server_id, dist);
      
    if (dist < min_dist) {
      min_dist = dist;
      picked_id = server_id;
      printf("dist < min_dist: %d < %d, now ", dist, min_dist);
      printf("picked_id:%d\n", picked_id);
    }

    server_p = server_p->next;
  }

  picked_server = list_node(server_list, picked_id);
  ip = *(uint32_t *)(picked_server->data);

  reply = make_dns_reply(query, ip, len);
  printf("cnd_geo_dist: choose ip %x\n", ip);

  return reply;

}

int get_client_ind(struct sockaddr_in *client_addr, struct list_node_t *ip_list) {
  assert(client_addr != NULL);
  assert(ip_list != NULL);

  char *client_ip = NULL;
  int client_ind;

  if ((client_ip = inet_ntoa(client_addr->sin_addr)) == NULL) {
    perror("Error! get_clietn_ind, inet_ntoa");
    exit(-1);
  }
  
  client_ind = list_ind(ip_list, client_ip, comparor_str);
  assert(client_ind != -1);

  printf("get_client_ind:%s, %d\n", client_ip, client_ind);
  return client_ind;
}

struct list_node_t *get_server_ind(struct list_node_t *serverlist, struct list_node_t *ip_list) {
  assert(serverlist != NULL);
  assert(ip_list != NULL);
  dbprintf("ger_server_ind:\n");
  
  struct list_node_t *server = NULL;
  char *ip = NULL;
  struct in_addr addr;  
  struct list_node_t *list = NULL;
  int *ind;
  
  server = serverlist;
  while (server != NULL) {
    memset(&addr, 0, sizeof(addr));
    addr.s_addr = *(uint32_t *)server->data;
    if ((ip = inet_ntoa(addr)) == NULL) {
      perror("Error! get_clietn_ind, inet_ntoa");
      exit(-1);
    }

    ind = (int *)calloc(1, sizeof(int));
    *ind = list_ind(ip_list, ip, comparor_str);
    push(&list, ind);

    server = server->next; 

    //dbprintf("%s_%d, ", ip, *ind);
  }
  printf("\n");
 
  return list;
}

int do_dijkstra(int **graph, int graph_size, int client, int server) {
  assert(graph != NULL);
  assert(graph_size > 0);
  assert(client >= 0 );
  assert(server >= 0);
  assert(client < graph_size);
  assert(server < graph_size);
  
  int i;
  int visited[graph_size];
  int dist[graph_size];
  int distance;
  
  for (i = 0; i < graph_size; i++) {
    visited[i] = 0;
    dist[i] = MAX_DIST;
  }
  dist[client] = 0;

  distance = dijkstra(graph, visited, dist, graph_size, client, server);

  return distance;
}

int **make_graph(char *LSAs, int *graph_size, struct list_node_t **ret_lsa_list, struct list_node_t **ret_ip_list) {
  assert(LSAs != NULL);
  assert(graph_size != NULL);
  assert(ret_lsa_list != NULL);
  assert(ret_ip_list != NULL);
  
  struct list_node_t *lsa_list = NULL; //
  struct list_node_t *ip_list = NULL; //
  int **matrix = NULL;

  // get adj_list and ip_list
  get_graph_list(&lsa_list, &ip_list, LSAs);
  *ret_ip_list = ip_list;
  *ret_lsa_list = lsa_list;

  //print_list(lsa_list, printer_lsa);
  //print_list(ip_list, printer_str);
  
  // get adj_matrix
  matrix = get_adj_matrix(lsa_list, ip_list, graph_size);

  
  return matrix;

}

int **get_adj_matrix(struct list_node_t *lsa_list, struct list_node_t *ip_list, int *matrix_size) {
  assert(lsa_list != NULL);
  assert(ip_list != NULL);
  assert(matrix_size != 0);
  printf("get_adj_matrix:\n");

  int size;
  int **matrix = NULL;
  int i, j;
  struct list_node_t *tmp_node = NULL;
  char *tmp_ip = NULL;
  struct lsa_t *tmp_lsa = NULL;
  int ind;

  // size
  size = list_size(ip_list);
  *matrix_size = size;

  // calloc space, and init to -1 which indicating no edge
  matrix = (int **)calloc(size, sizeof(int *));

  for (i = 0; i < size; i++) {
    matrix[i] = (int *)calloc(size, sizeof(int));
    
    for (j = 0; j < size; j++) {
      if (i != j)
	matrix[i][j] = -1;
      else 
	matrix[i][j] = 0; // diag line is 0 line
    }
  }

  // fill in matrix, each ip is natually assigned index in the ip_list as id
  for (i = 0; i < size; i++) {

    // find lsa with index i
    tmp_node = list_node(ip_list, i);
    tmp_ip = (char *)(tmp_node->data);
    //dbprintf("%s with ind_%d\n", tmp_ip, i);

    ind = list_ind(lsa_list, tmp_ip, comparor_lsa_ip);
    if (ind == -1) {
      printf("Warning! %s_%d does not exist in nei_list\n", tmp_ip, i);
      continue;
    }
    tmp_node = list_node(lsa_list, ind);
    tmp_lsa = (struct lsa_t *)(tmp_node->data);
    //dbprintf("with lsa:");
    //printer_lsa(tmp_lsa);

    // modify adj_matrix
    set_adj_line(matrix, i, tmp_lsa, ip_list);
  }

 
  return matrix;
}


int set_adj_line(int **matrix, int line_ind, struct lsa_t *lsa, struct list_node_t *ip_list) {
  assert(matrix != NULL);
  assert(lsa != NULL);
  assert(ip_list != NULL);

  struct list_node_t *nei = NULL;
  char *nei_ip = NULL;
  int nei_ind;
  
  nei = lsa->neighbors;

  while (nei != NULL) {
    nei_ip = (char *)nei->data;
    nei_ind = list_ind(ip_list, nei_ip, comparor_str);
    assert(nei_ind != -1);

    matrix[line_ind][nei_ind] = 1;
    matrix[nei_ind][line_ind] = 1; // undirected graph
    //dbprintf("matrix[%d][%d] = 1, ", line_ind, nei_ind);
    
    nei = nei->next;
  }
  //dbprintf("\n");

  return 0;
}


int comparor_lsa_ip(void *lsa_void, void *ip_void) {
  assert(lsa_void != NULL);
  assert(ip_void != NULL);

  struct lsa_t *lsa = (struct lsa_t *)lsa_void;
  char *ip = (char *)ip_void;

  return strcmp(lsa->ip, ip);

}


int get_graph_list(struct list_node_t **ret_nei_list, struct list_node_t **ret_ip_list, char *LSAs) {

  assert(LSAs != NULL);
  assert(ret_nei_list != NULL);
  assert(ret_ip_list != NULL);

  FILE *fp = NULL;
  int line_size = 1024;
  char line[line_size];
  struct lsa_t *lsa = NULL;
  struct list_node_t *lsa_list = NULL; //
  struct list_node_t *ip_list = NULL; //
  int ind;
  struct list_node_t *tmp_node = NULL;
  struct lsa_t *tmp_lsa = NULL;

  //init_list(&lsa_list);


  // get neighbor list and ip list first
  if((fp = fopen(LSAs, "r")) == NULL) {
    perror("Error! make_graph, fopen");
    exit(-1);
  }

  memset(line, 0, line_size);
  while (fgets(line, line_size, fp) != NULL) {
    lsa = parse_line(line);
    //dbprinter_lsa(lsa);//

    // ip_list
    collect_ip(&ip_list, lsa);

    // lsa_list
    if ((ind = list_ind(lsa_list, lsa, comparor_lsa)) != -1) {
      //dbprintf("already exist at %d", ind);// 
      tmp_node = list_node(lsa_list, ind);
      tmp_lsa = (struct lsa_t *)(tmp_node->data);
      
      //dbprintf("compare with old lsa\n");//
      //printer_lsa(tmp_lsa);
      
      //dbprintf("new %d ? old %d\n", tmp_lsa->seq_num, lsa->seq_num);
      if (tmp_lsa->seq_num < lsa->seq_num)
	tmp_node->data = lsa;

    } else {
      push(&lsa_list, lsa);
    }     
    
    memset(line, 0, line_size);
  }

  //print_list(lsa_list, printer_lsa);

  // ret lists
  *ret_nei_list = lsa_list;
  *ret_ip_list = ip_list;
 
  return 0;
}

int comparor_lsa(void *lsa1, void *lsa2) {
  assert(lsa1 != NULL);
  assert(lsa2 != NULL);
  
  struct lsa_t *a = (struct lsa_t *)lsa1;
  struct lsa_t *b = (struct lsa_t *)lsa2;

  return strcmp(a->ip, b->ip);
}

char *make_server_list(char *servers) {
  assert(servers != NULL);

  FILE *fp = NULL;
  char *line = NULL;

  if ((fp = fopen(servers, "r")) == NULL) {
    perror("Error! make_server_array, fopen\n");
    exit(-1);
  }
  
  line = (char *)calloc(128, sizeof(char));
  while(fgets(line, 128, fp) != NULL) {
    
  }
  
  return NULL;
}

struct lsa_t *parse_line(char *line) {
  assert(line != NULL);

  char *p1 = NULL;
  char *p2 = NULL;
  struct lsa_t *lsa = NULL;
  struct list_node_t *neighbors = NULL;
  int tmp_size = 128;
  char tmp[tmp_size];
  char *n_ip;

  lsa = (struct lsa_t *)calloc(1, sizeof(struct lsa_t));

  // ip
  p1 = line;
  if ((p2 = strchr(p1, ' ')) == NULL) {
    printf("Error! parse_line, ip, wrong format\n");
    exit(-1);
  }
  
  lsa->ip = (char *)calloc(p2 - p1 + 1, sizeof(char));
  memcpy(lsa->ip, p1, p2-p1);
  //dbprintf("parse_line: ip:%s, ", lsa->ip);

  // seq_num
  p1 = p2 + 1;
  if ((p2 = strchr(p1, ' ')) == NULL) {
    printf("Error! parse_line, seq_num, wrong format\n");
    exit(-1);
  }
  
  memset(tmp, 0, tmp_size);
  memcpy(tmp, p1, p2-p1);
  lsa->seq_num = atoi(tmp);
  //dbprintf("seq_num:%d, ", lsa->seq_num);

  // neighbors  
  //init_list(&neighbors);
  
  //dbprintf("neighbors:");
  p1 = p2 + 1;
  while ((p2 = strchr(p1, ',')) != NULL || (p2 = strchr(p1, '\n')) != NULL) {
    n_ip = (char *)calloc(p2-p1+1, sizeof(char));
    memcpy(n_ip, p1, p2-p1);
    push(&neighbors, n_ip);
    //dbprintf("%s, ", n_ip);
    
    p1 = p2 + 1;
  }

  lsa->neighbors = neighbors;

  // done
  return lsa;
}

void printer_lsa(void *data) {
  assert(data != NULL);
  struct lsa_t *lsa = (struct lsa_t *)data;

  printf("ip:%s, seq_num:%d, neighbors:", lsa->ip, lsa->seq_num);
  print_list(lsa->neighbors, printer_str);

}

int collect_ip(struct list_node_t **ip_list, struct lsa_t *lsa) {
  assert(ip_list != NULL);
  assert(lsa != NULL);
  //dbprintf("collect_ip:\n");
  
  char *ip = NULL;
  struct list_node_t *neighbor = NULL;
  char *str = NULL;
  

  // ip part
  if (list_ind(*ip_list, lsa->ip, comparor_str) == -1) {
    ip = (char *)calloc(strlen(lsa->ip)+1, sizeof(char));
    memcpy(ip, lsa->ip, strlen(lsa->ip));
    push(ip_list, ip);

    //print_list(*ip_list, printer_str);//
  }

  // neighbor list part
  neighbor = lsa->neighbors;
  while (neighbor != NULL) {

    str = (char *)neighbor->data;

    if (list_ind(*ip_list, str, comparor_str) != -1) {
      neighbor = neighbor->next;
      continue;
    }

    ip = (char *)calloc(strlen(str)+1, sizeof(char));
    memcpy(ip, str, strlen(str));

    push(ip_list, ip);    

    neighbor = neighbor->next;

    //print_list(*ip_list, printer_str);//
  }
  
  return 0;
}


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



#ifdef TEST

int main(){
  // test make_graph
  int size;
  struct list_node_t *lsa_list = NULL;
  struct list_node_t *ip_list = NULL;
  int **matrix = NULL;

  matrix = make_graph("./topos/topo2/topo2.lsa", &size, &lsa_list, &ip_list);

  printf("print lsa_list:\n");
  print_list(lsa_list, printer_lsa);
  printf("print ip_list:\n");
  print_list(ip_list, printer_str);

  printf("print adj_matrix:\n");
  int i, j;
  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++)
      printf("%2d, ", matrix[i][j]);
    printf("\n");
  }

  //
  printf("test dijkstra:\n");
  printf("1 to 2:%d, 1 to 6:%d, 1 to 9:%d\n", do_dijkstra(matrix, size, 1, 2), do_dijkstra(matrix, size, 1, 6), do_dijkstra(matrix, size, 1, 9));
  printf("5 to 2:%d, 5 to 6:%d, 5 to 9:%d\n", do_dijkstra(matrix, size, 5, 2), do_dijkstra(matrix, size, 5, 6), do_dijkstra(matrix, size, 5, 9));


  return 0;
}

#endif 
