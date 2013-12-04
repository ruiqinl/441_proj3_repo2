#ifndef HTTP_REPLYER_H
#define HTTP_REPLYER_H

#include "helper.h"

int create_response(struct buf *bufp);
int send_response(int sock, struct buf *bufp);

int create_line_header(struct buf *bufp);
int create_res_body(struct buf *bufp);

void  push_header(struct buf *bufp);
char *date_str();
char *connection_str(struct buf *bufp);
char *cont_len_str(struct buf *bufp);
char *cont_type_str(struct buf *bufp);
char *last_modified_str(struct buf *bufp);

int locate_file(struct buf *bufp);

void push_error(struct buf *bufp, const char *msg);

#endif
