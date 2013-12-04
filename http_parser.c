#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>

#include "helper.h"
#include "http_parser.h"
#include "http_replyer.h"

// send to server/browser
// return 1 if send some bytes, return 0 if finish sending, expect reading or not depends on bufp->status
int general_send(int sock, struct buf *bufp, struct sockaddr_in *server_addr, char *fake_ip) {
    assert(bufp != NULL);
    

    if (bufp->status == TO_SERVER) {
	dbprintf("general_send: TO_SERVER\n");
	send_SERVER(sock, bufp, server_addr, fake_ip);
		
    } else if (bufp->status == TO_BROWSER) {
	dbprintf("general_send: TO_BROWSER, send to sock %d\n", bufp->sock2browser);

	send_BROWSER(sock, bufp, server_addr);
    }

    return 0;
}

// return 1 if send some bytes, return 0 if finish sending, expect reading or not depends on bufp->status
int send_SERVER(int sock, struct buf *bufp, struct sockaddr_in *server_addr, char *fake_ip) {

    assert(server_addr != NULL);
    assert(bufp->http_reply_p != NULL);

    int sock2server;
    size_t numbytes, bytes_left;
    char *p1, *p2;
    struct sockaddr_in fake_addr;

    // get a socket
    if((sock2server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	perror("Error! main, socket, socket2server");
	exit(-1);
    }

    // bind fake ip
    memset(&fake_addr, 0, sizeof(fake_addr));
    fake_addr.sin_family = AF_INET;
    if (inet_aton(fake_ip, &fake_addr.sin_addr) == 0) {
	perror("Error! proxy, fake_addr, inet_aton\n");
	exit(-1);
    }
    fake_addr.sin_port = htons(0);
		    
    if (bind(sock2server, (struct sockaddr *)&fake_addr, sizeof(fake_addr)) == -1) {
	perror("Error! proxy, bind\n");
	exit(-1);
    }
    dbprintf("send_SERVER: bind to fake ip:%s\n", fake_ip);
    
    // connect
    if (connect(sock2server, (struct sockaddr*)server_addr, sizeof(*server_addr)) == -1) {
	perror("Error! general_send, connect, socket2server");
	exit(-1);
    }
	
    bufp->sock2server = sock2server;
	
    // left req to send
    p1 = bufp->http_reply_p->orig_req;
    p2 = bufp->http_reply_p->orig_cur;
    assert(p1 != NULL); 
    assert(p2 != NULL);
    assert(p2 >= p1);
	
    //bytes_sent = p2 - p1;
    bytes_left = strlen(p2);
    //assert(bytes_left == strlen(p2)); // stupid

    if ((numbytes = send(sock2server, p2, bytes_left, 0)) > 0) {
	char tmp[128];
	inet_ntop(AF_INET, &(server_addr->sin_addr), tmp, 128);
	    
	if (numbytes == strlen(p2)) {
	    // finish sending
	    dbprintf("general_send: TO_SERVER, finished sending %ld bytes to server %s:%d:\n%s", numbytes, tmp, ntohs(server_addr->sin_port), p2);

	    return 0;
	} 

	// prepare for next sending
	p2 += numbytes;
	bufp->http_reply_p->orig_cur = p2;
	    
	dbprintf("general_send: TO_SERVER, send %ld bytes to server %s:%d:\n%s", numbytes, tmp, ntohs(server_addr->sin_port), p2-numbytes);

	return 1;

    } else if (numbytes == 0) {
	dbprintf("Error!general_send: TO_SERVER, send 0 bytes, this should not happen\n");
	exit(-1);

    } else if (numbytes == -1) {
	perror("Error! general_send, TO_SERVER, send\n");
	exit(-1);
    }
    
    return 0;

}

// return 1 if send some bytes, return 0 if finish sending, expect reading or not depends on bufp->status
int send_BROWSER(int sock, struct buf *bufp, struct sockaddr_in *server_addr) {
    assert(bufp != NULL);

    int send_ret;
    
    dbprintf("send_BROWSER: len:%ld\n", bufp->buf_tail - bufp->buf_head);
    while ((send_ret = send(bufp->sock2browser, bufp->buf_head, bufp->buf_tail - bufp->buf_head, 0)) > 0) {
	bufp->buf_head += send_ret;
	dbprintf("send_BROWSER: send %d bytes\n", send_ret);
    }
    if (send_ret == -1){
	perror("Error! general_send, send to brow\n"); 
	exit(-1);
    }
    if (send_ret == 0) {
	dbprintf("send_BROWSER: send 0 bytes\n");
	return 0;
    }

    return 0;

}


// return 1 if fully received a request, return 0 if no bytes received, 2 if partially received
int general_recv(int sock, struct buf *bufp) {
    assert(bufp != NULL);

    if (bufp->status == RAW || bufp->status == FROM_BROWSER) {
	dbprintf("general_recv: RAW/FROM_BROWSER, call recv_BROW\n");
	return recv_BROW(sock, bufp);
    } else if (bufp->status == FROM_SERVER) {
	dbprintf("general_recv: FROM_SERVER, call recv_SERVER\n");
	return recv_SERVER(sock, bufp);
    }

    dbprintf("Error! general_recv: wrong status\n");
    exit(-1);

    return 1;
}

int change_rate (struct buf *bufp) {
    assert(bufp != NULL);

    char *p1, *p2;
    char *bitrate = (char *)calloc(128, sizeof(char));
    int new_rate;

    p1 = strstr(bufp->rbuf, "/vod/");
    p2 = strstr(bufp->rbuf, "Seg");

    if (p2 == NULL) {
	dbprintf("change_rate: not for chunk, no need\n");
	return 0;
    }
    assert(p1 != NULL);
    assert(p2 != NULL);

    p1 += strlen("/vod/");
    memcpy(bitrate, p1, p2-p1);
    //bufp->bitrate = atoi(bitrate);

    //printf("change_rate: not changed yet, remain %s\n", bufp->bitrate);
    if (avg_tput == 0.0) {
	printf("change: avg_tput is 0.0, first packet, do not change rate %s\n", bitrate);
	bufp->bitrate = atoi(bitrate);
	return 0;
    }


    int *p = all_rates;

    while ( *p != 0 && (*p) <= (avg_tput/1.5)){
	//printf("change_rate: %d ? %f\n", (*p), (avg_tput/1.5));
	p++;
    }
    
    if (p == all_rates) {
	//printf("change_rate: impossible, avg_tput/1.5/8 is %f\n", avg_tput/1.5/8);
	new_rate = *p;
    } else {
	new_rate = *(p-1);
    }

    //printf("change from rate %s to %d\n", bitrate, new_rate);
    bufp->bitrate = new_rate;

    // modify req
    char *tmp = (char *)calloc(2*strlen(bufp->rbuf), sizeof(char));
    memcpy(tmp, bufp->rbuf, p1 - bufp->rbuf);
	
    char tmp2[128];
    memset(tmp2, 0, 128);
    sprintf(tmp2, "%d", new_rate);

    memcpy(tmp + (p1 - bufp->rbuf), tmp2, strlen(tmp2));
    memcpy(tmp + (p1 - bufp->rbuf) + strlen(tmp2), p2, strlen(p2));
    //    dbprintf("??????\n%s\n???????\n", tmp);

    free(bufp->http_reply_p->orig_req);
    bufp->http_reply_p->orig_req = tmp;
    bufp->http_reply_p->orig_cur = tmp;

    return 0;
}

int log_chunkname(struct buf *bufp) {
    assert(bufp != NULL);

    char *p1, *p2;
    char *tag = "Seg";
    
    p1 = strstr(bufp->http_req_p->orig_req, tag);
    if (p1 == NULL) {
	dbprintf("log_chunkname: not for chunk, no need\n");
	return 0;
    }

    p1 = strstr(bufp->http_req_p->orig_req, "/vod/");
    assert(p1 != NULL);
    p2 = strchr(p1, ' ');
    assert(p2 != NULL);

    bufp->chunk_name = (char *)calloc(256, sizeof(char));
    memcpy(bufp->chunk_name, p1, p2-p1);

    return 0;
}

// return 1 if fully received a request, return 0 if no bytes received, 2 if partially received
int recv_SERVER(int sock, struct buf *bufp) {
    assert(bufp != NULL);

    int recv_ret;
    char *p1 = NULL;
    char *p2 = NULL;
    char tmp[128];
    int cont_len;
    char *con;

    if ((recv_ret = recv(sock, bufp->buf_tail, bufp->buf_free_size, 0)) == -1) {
	perror("Error! recv_SERVER, recv");
	exit(-1);
    }

    if (recv_ret > 0) {
	dbprintf("recv_SERVER: recv %d bytes\n", recv_ret);

	bufp->buf_tail += recv_ret;
	bufp->buf_free_size -= recv_ret;
	if (bufp->buf_free_size <= 0) {
	    dbprintf("Error! recv_SERVER, overflow\n");
	    exit(-1);
	}

	con = "Content-Length: ";
	if((p1 = strstr(bufp->buf, con)) != NULL
	   && (p2 = strstr(p1, "\r\n")) != NULL) {
	    memset(tmp, 0, 128);
	    p1 += strlen(con);
	    memcpy(tmp, p1, p2-p1);
	    cont_len = atoi(tmp);
	    dbprintf("recv_SERVER: Content-Lneght:s:%s, d:%d\n", tmp, cont_len);

	    if ((p1 = strstr(bufp->buf, "\r\n\r\n")) != NULL) {
		dbprintf("recv_SERVER: cont len:%ld\n", bufp->buf_tail - (p1+4));
		if (bufp->buf_tail - (p1+4) == cont_len) {
		    dbprintf("recv_SERVER: fully recvd\n");

		    // time
		    //time(&(bufp->tf));
		    struct timeval tim;
		    gettimeofday(&tim, NULL);
		    bufp->tf = tim.tv_sec + (tim.tv_usec/1000000.0);
		    
		    dbprintf("recv_SERVER: time tf:%f\n", bufp->tf);
		    
		    // size
		    bufp->Bsize = bufp->buf_tail - bufp->buf;
		    dbprintf("recv_SERVER: Bsize:%ld\n", bufp->Bsize);

		    return 1;
		}
	    }
	}
    
	dbprintf("recv_SERVER: not fully recvd, keep recving\n");
	return 2;
    } 

    assert(recv_ret == 0);
    return 0;
}


// return 1 if fully received a request, return 0 if no bytes received, 2 if partially received
int recv_BROW(int sock, struct buf *bufp){

    int recv_ret;
	
    recv_ret = recv_request(sock, bufp); //recv_ret -1: recv error; 0: recv 0; 1: recv some bytes 
    dbprintf("===========================================================\n");
    dbprintf("recv_request: recv from sock %d, recv_ret is %d\n", sock, recv_ret);
    
    if (bufp->recv_time == 0) {
	time(&(bufp->recv_time));
    }

    if (recv_ret == 1){
	parse_request(bufp);
	//dbprint_queue(bufp->req_queue_p);

	if (bufp->req_queue_p->req_count > 0) {
	    dbprintf("recv_request: fully recv, switch to close, change rate, and send to server\n");

	    // time
	    struct timeval tim;
	    gettimeofday(&tim, NULL);
	    bufp->ts = tim.tv_sec + (tim.tv_usec/1000000.0);
	    dbprintf("recv_BROW: time ts:%f\n", bufp->ts);


	    dequeue_request(bufp); 
	    
	    return_nolist(bufp);
	    
	    change_rate(bufp);
	    log_chunkname(bufp);	       	    
	    
	    char *p;
	    char *new_buf = (char *)calloc(2*strlen(bufp->http_req_p->orig_req), sizeof(char));
	    
	    int len, len_left;
	    char *close_str = "Connection: close\r\n";
	    char *alive_str = "Connection: keep-alive\r\n";

	    // switch to close
	    if ((p = strstr(bufp->http_req_p->orig_req, alive_str)) != NULL) {
		len = p - bufp->http_req_p->orig_req;
		memcpy(new_buf, bufp->http_req_p->orig_req, len);
		memcpy(new_buf+len, close_str, strlen(close_str));

		len_left = strlen(p + strlen(alive_str));
		memcpy(new_buf+ len+ strlen(close_str), p + strlen(alive_str), len_left);
		bufp->http_req_p->orig_req = new_buf;
		bufp->http_req_p->orig_cur = new_buf;

	    }
	    /*	    
	    // avoid 303
	    if ((p = strstr(bufp->http_req_p->orig_req, "If-None-Match:")) != NULL) {
	    	memcpy(p, "\r\n\r\n", strlen("\r\n\r\n"));
	    	*(p+4) = '\0';
	    }
	    */

	    return 1;
	} else  {
	    dbprintf("recv_request: partially recv, keep receiving\n");
	    return 2;
	}

    } else {
	dbprintf("recv_request: recv 0 bytes, need to clear sock %d from master_read_fds\n", sock);
	return 0;
    }

}

/* Return errcode, -1:recv error, 0: recv 0, 1:recv some bytes */
int recv_request(int sock, struct buf *bufp) {

    int readbytes;

    if (bufp->req_fully_received == 1) {
	reset_rbuf(bufp);
	dbprintf("recv_request: fully_received==1, reset rbuf\n");
    }
    
    // if free_size == 0, it confuses lisod to think that the client socket is closed
    if (bufp->rbuf_free_size == 0) {
    	dbprintf("Warnning! recv_request, rbuf_free_size == 0, send MSG503 back\n");
	send_error(sock, MSG503); // non_block
	return 0;
    }

    readbytes = recv(sock, bufp->rbuf_tail, bufp->rbuf_free_size, 0);

    // update rbuf
    bufp->rbuf_tail += readbytes; 
    bufp->rbuf_size += readbytes;
    bufp->rbuf_free_size -= readbytes;
    //dbprintf("recv_request:\nwhat recv this time:\n%s\n", bufp->rbuf_tail-readbytes);

    if (readbytes < 0 )
	return -1;
    else if (readbytes == 0)
	return 0;
    return 1;
}


int parse_request(struct buf *bufp) {
    
    char *p;
    char *old_head = NULL;
    size_t size;
    
    p = bufp->parse_p;
    dbprintf("parse_request:\n\trbuf left to parse:\n%s\n", p);

    // count request number based on CRLF2
    while ((p = strstr(p, CRLF2)) != NULL && p < bufp->rbuf_tail) {
	++(bufp->rbuf_req_count);
	p += strlen(CRLF2);
	bufp->parse_p = p;
    }

    if (bufp->rbuf_req_count == 0)
	bufp->req_fully_received = 0;

    // parse every possible request in the rbuf
    while (bufp->rbuf_req_count != 0) {

	dbprintf("parse_request: request count %d\n", bufp->rbuf_req_count);
	
	// proj3
	old_head = bufp->rbuf_head;

	// calloc http_req
	bufp->http_req_p = (struct http_req *)calloc(1, sizeof(struct http_req));
	bufp->req_fully_received = 0;
	bufp->req_line_header_received = 0;

	// parse req
	parse_request_line(bufp);
	parse_request_headers(bufp);

	bufp->req_line_header_received = 1;// update 

	 // set req_fully_received inside
	if (parse_message_body(bufp) == -1) {
	    // POST has no Content-Length header
	    push_error(bufp, MSG411);
	}
	    

	// if fully received, enqueue	
	if (bufp->req_fully_received == 1) {
	    dbprintf("parse_request: req fully received\n");

	    // update req_queue
	    req_enqueue(bufp->req_queue_p, bufp->http_req_p);
	    
	    // update rbuf
	    --(bufp->rbuf_req_count);
	    bufp->rbuf_head = strstr(bufp->rbuf_head, CRLF2) + strlen(CRLF2);

	    // proj3, save origianl req
	    size = bufp->rbuf_head - old_head;
	    bufp->http_req_p->orig_req = (char *)calloc(size + 1, sizeof(char));
	    memcpy(bufp->http_req_p->orig_req, old_head, size);
	    
	    bufp->http_req_p->orig_cur = bufp->http_req_p->orig_req;

	} else {
	    // only POST can reach here
	    dbprintf("parse_request: POST req not fully received yet, continue receiving\n");
	    break; // break out while loop
	}
	
    }

    
    return 0;
}

int parse_request_line(struct buf *bufp){

    char *p1, *p2;
    struct http_req *http_req_p;
    int len;

    if (bufp->req_line_header_received == 1)
	return 0; // received already, just return

    http_req_p = bufp->http_req_p;
    
    bufp->line_head = bufp->rbuf_head; 
    bufp->line_tail = strstr(bufp->line_head, CRLF); 
   
    if (bufp->line_tail >= bufp->rbuf_tail) {
	// this should never happend, since at least a CRLF2 exists
	dbprintf("request line not found\n");
	return -1;
    }
    

    p1 = bufp->line_head;
    while (isspace(*p1))
	p1++;

    if ((p2 = strchr(p1, ' ')) == NULL || p2 > bufp->line_tail) {
	// cannot find method in this request area
	strcpy(http_req_p->method, "method_not_found");
    } else {
	len = p2 - p1;
	if (len > HEADER_LEN-1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, method buffer overflow\n");
	} 
	strncpy(http_req_p->method, p1, len);
	http_req_p->method[len] = '\0';

	p1 += len + 1;
	while (isspace(*p1))
	    p1++;

    }
    dbprintf("http_req->method:%s\n", http_req_p->method);

    
    if ((p2 = strchr(p1, ' ')) == NULL || p2 > bufp->line_tail) {
	// cannot find uri
	strcpy(http_req_p->uri, "uri_not_found");
    } else {
	len = p2 - p1;
	if (len > HEADER_LEN -1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, uri buffer overflow\n");
	}
	strncpy(http_req_p->uri, p1, len);
	http_req_p->uri[len] = '\0';

	p1 += len + 1;
	while (isspace(*p1))
	    p1++;
    }
    dbprintf("http_req->uri:%s\n", http_req_p->uri);
    
    if ((p2 = strstr(p1, CRLF)) == NULL || p2 > bufp->line_tail) {
	// cannot find version
	strcpy(http_req_p->version, "version_not_found");
    } else {

	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len >  HEADER_LEN - 1) {
	    len = HEADER_LEN  - 1;
	    fprintf(stderr, "Warning! parse_request, version buffer overflow\n");
	}
	strncpy(http_req_p->version, p1, len);
	http_req_p->version[len] = '\0';

	p1 += len + strlen(CRLF);
    }
    dbprintf("http_req->version:%s\n", http_req_p->version);

    // update line_head and line_tail, a CRLF always exists since at least a CRLF2 is always there
    // now line_head and line_tail
    bufp->line_head = bufp->line_tail + strlen(CRLF);
    bufp->line_tail = strstr(bufp->line_head, CRLF);
    if (bufp->line_tail == bufp->line_head)
	dbprintf("Warnning! headers do not exist\n");
    else {
	// headers exist, put line_tail to end of headers
	bufp->line_tail = strstr(bufp->line_head, CRLF2);
    }

    return 0;
}


/* bufp->line_head and bufp->line_tail have already been updated in method parse_request_line  */
int parse_request_headers(struct buf *bufp) {

    char *p1, *p2;
    struct http_req *http_req_p;
    int tmp_size = 128;
    char tmp[tmp_size];
    int len;

    if (bufp->req_line_header_received == 1)
	return 0; // received already, just return

    http_req_p = bufp->http_req_p;

    if ((p1 = strstr(bufp->line_head, cont_type)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cont_type);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req_p->cont_type, p1, len);
	http_req_p->cont_type[len] = '\0';
    } else 
	strcat(http_req_p->cont_type, "");
    dbprintf("http_req->cont_type:%s\n", http_req_p->cont_type);

    if ((p1 = strstr(bufp->line_head, accept_range)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(accept_range);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, accept buffer overflow\n");
	}
	strncpy(http_req_p->http_accept, p1, len);
	http_req_p->http_accept[len] = '\0';
    } else 
	strcat(http_req_p->http_accept, "");
    dbprintf("http_req->http_accept:%s\n", http_req_p->http_accept);

     if ((p1 = strstr(bufp->line_head, referer)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(referer);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, referer buffer overflow\n");
	}
	strncpy(http_req_p->http_referer, p1, len);
	http_req_p->http_referer[len] = '\0';
    } else 
	strcat(http_req_p->http_referer, "");
    dbprintf("http_req->http_referer:%s\n", http_req_p->http_referer);


    if ((p1 = strstr(bufp->line_head, host)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(host);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->host, p1, len);
	http_req_p->host[len] = '\0';
	
    } else 
	strcat(http_req_p->host, "");
    dbprintf("http_req->host:%s\n", http_req_p->host);

    if ((p1 = strstr(bufp->line_head, encoding)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(encoding);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_encoding, p1, len);
	http_req_p->http_accept_encoding[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_encoding, "");
    dbprintf("http_req->http_accept_encoding:%s\n", http_req_p->http_accept_encoding);

    if ((p1 = strstr(bufp->line_head, language)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(language);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_language, p1, len);
	http_req_p->http_accept_language[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_language, "");
    dbprintf("http_req->http_accept_language:%s\n", http_req_p->http_accept_language);

    if ((p1 = strstr(bufp->line_head, charset)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(charset);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_charset, p1, len);
	http_req_p->http_accept_charset[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_charset, "");
    dbprintf("http_req->http_accept_charset:%s\n", http_req_p->http_accept_charset);

    if ((p1 = strstr(bufp->line_head, cookie)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cookie);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cookie buffer overflow\n");
	}
	strncpy(http_req_p->cookie, p1, len);
	http_req_p->cookie[len] = '\0';
    } else 
	strcat(http_req_p->cookie, "");
    dbprintf("http_req->cookie:%s\n", http_req_p->cookie);

    if ((p1 = strstr(bufp->line_head, user_agent)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(user_agent);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_reaquest, user_agent buffer overflow\n");
	}
	strncpy(http_req_p->user_agent, p1, len);
	http_req_p->user_agent[len] = '\0';
	
    } else 
	strcat(http_req_p->user_agent, "");
    dbprintf("http_req->user_agent:%s\n", http_req_p->user_agent);

    if ((p1 = strstr(bufp->line_head, connection)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(connection);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req_p->connection, p1, len);
	http_req_p->connection[len] = '\0';
    } else 
	strcat(http_req_p->connection, "");
    dbprintf("http_req->connection:%s\n", http_req_p->connection);


    if ((p1 = strstr(bufp->line_head, cont_len)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cont_len);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;


	//len = bufp->line_tail - p1;
	if (len > tmp_size-1) {
	    len = tmp_size- 1;
	    fprintf(stderr, "Warning! parse_request, cont_len buffer overflow\n");
	}
	strncpy(tmp, p1, len);
	tmp[len] = '\0';
	http_req_p->cont_len = atoi(tmp);
	

    }
    dbprintf("http_req->cont_len:%d\n", http_req_p->cont_len);

    

    return 0;
}

int parse_message_body(struct buf *bufp) {

    char *p;
    int non_body_size, body_size;
    
    // not POST, receiving request finished
    if (strcmp(bufp->http_req_p->method, POST) != 0) {
	bufp->req_fully_received = 1;
	return 0;
    }

    /* POST does not appear in piped request, so all left bytes are message body */
    if (bufp->http_req_p->cont_len == 0) {
	fprintf(stderr, "Error! POST method does not has Content-Length header\n");
	bufp->req_fully_received = 1;
	return -1;
    }

    if ((p = strstr(bufp->rbuf_head, CRLF2)) == NULL) {
	dbprintf("Warnning! parse_message_body, strstr, this line should never be reached\n");
	dbprintf("Remember to send error msg back to client\n");
    }
    p += strlen(CRLF2);
    
    bufp->http_req_p->contp = (char *)calloc(bufp->http_req_p->cont_len+1, sizeof(char));
    strcpy(bufp->http_req_p->contp, p);

    dbprintf("parse_message_body: body part:%s\n", bufp->http_req_p->contp);

    non_body_size = p - bufp->rbuf;
    body_size = bufp->rbuf_size - non_body_size;
    if (body_size >= bufp->http_req_p->cont_len)
	bufp->req_fully_received = 1; 
    else 
	bufp->req_fully_received = 0;
	
    return 0;
}

int return_nolist(struct buf *bufp) {
    assert(bufp != NULL);

    char *p1, *p2;
    char *orig = bufp->rbuf;
    char *tag = "/big_buck_bunny.f4m";
    char *newtag = "/big_buck_bunny_nolist.f4m";
    
    if ((p1 = strstr(orig, tag)) == NULL) {
	return 0;
    }

    char *tmp = (char *)calloc(2*strlen(orig), sizeof(char));
    
    memcpy(tmp, orig, p1 - orig);
    memcpy(tmp + (p1 - orig), newtag, strlen(newtag));
    
    p2 = p1 + strlen(tag);
    memcpy(tmp + (p1 - orig) + strlen(newtag), p2, strlen(p2));

    //printf("?????orig_req:\n%s\n???????\n", bufp->http_reply_p->orig_req);
    //printf("?????new_req:\n%s\n???????\n", tmp);

    free(bufp->http_reply_p->orig_req);
    bufp->http_reply_p->orig_req = tmp;
    bufp->http_reply_p->orig_cur = tmp;

    return 0;
}
