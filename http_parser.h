#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "helper.h"

int recv_request(int sock, struct buf *bufp);
int parse_request(struct buf *bufp);

int parse_request_line(struct buf *bufp);
int parse_request_headers(struct buf *bufp);
int parse_message_body(struct buf *bufp);

// proj3
int general_recv(int sock, struct buf *bufp);
int recv_BROW(int sock, struct buf *bufp);
int change_rate (struct buf *bufp);
int general_send(int sock, struct buf *bufp, struct sockaddr_in *server_addr, char *fake_ip);

int recv_SERVER(int sock, struct buf *bufp);
int send_BROWSER(int sock, struct buf *bufp, struct sockaddr_in *server_addr);
int send_SERVER(int sock, struct buf *bufp, struct sockaddr_in *server_addr, char *fake_ip);
int return_nolist(struct buf *bufp);

#endif
