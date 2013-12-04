#ifndef _NAMESERVER_H_
#define _NAMESERVER_H_

#include "helper.h"

struct server_t {
  uint32_t server; // ip
  struct server_t *next;
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
char *cnd_rr(struct dns_t *query, uint32_t server_ip, int *reply_len);

char *cnd_geo_dist(struct dns_t *query, int *reply_len);

struct server_t *get_serverlist(char *servers);
int init_serverlist(struct server_t **list);
int push_server(struct server_t *list, uint32_t ip);
int print_serverlist(struct server_t *list);

#endif
