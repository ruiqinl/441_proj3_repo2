#ifndef _NAMESERVER_H_
#define _NAMESERVER_H_

#include "list.h"
#include "helper.h"

struct lsa_t {
  char *ip;
  int seq_num;
  struct list_node_t *neighbors;
};


/**
 * Choose cnd based on round_roubin, return reply packet
 *
 * @param query The received query
 * @param server_ip The ip to reply
 * @param reply_len Return the length of the reply via param
 *
 * @return reply packet
 */
char *cnd_rr(struct dns_t *query, uint32_t ip, int *len, char *reply_ip, int reply_ip_len);
char *cnd_geo_dist(struct dns_t *query, int *len, int **graph, int graph_size, int client_id, struct list_node_t *server_ind_list, struct list_node_t *ip_list, char *reply_ip, int reply_ip_len);

struct list_node_t *get_serverlist(char *servers);
void printer_hex(void *data);
uint32_t next_server(struct list_node_t *list);

int **make_graph(char *LSAs, int *graph_size, struct list_node_t **ret_lsa_list, struct list_node_t **ret_ip_list);
struct lsa_t *parse_line(char *line);
void printer_lsa(void *data);
int collect_ip(struct list_node_t **ip_list, struct lsa_t *lsa);
int comparor_lsa(void *lsa1, void *lsa2);
int get_graph_list(struct list_node_t **ret_nei_list, struct list_node_t **ret_ip_list, char *LSAs);
int **get_adj_matrix(struct list_node_t *lsa_list, struct list_node_t *ip_list, int *list_size);
int comparor_lsa_ip(void *lsa_void, void *ip_void);
int set_adj_line(int **matrix, int line_ind, struct lsa_t *lsa, struct list_node_t *ip_list);
int get_client_ind(struct sockaddr_in *client_addr, struct list_node_t *ip_list);
struct list_node_t *get_server_ind(struct list_node_t *serverlist, struct list_node_t *ip_list);
int do_dijkstra(int **graph, int graph_size, int client, int server);

int nameserver_log(const char *log, const struct sockaddr_in *client_addr, const struct dns_t *query, char *reply_ip);

#endif
