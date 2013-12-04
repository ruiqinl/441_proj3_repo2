
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>


#include "helper.h"
#include "http_replyer.h"
//#include "cgi_parser.h"

double avg_tput = 0.0;
int *all_rates = NULL;
char *node = "video.cs.cmu.edu";
char *service = "8080";


const char CRLF[] = "\r\n";
const char CRLF2[] = "\r\n\r\n";

const char cont_type[] = "Content-Type:";
const char accept_range[] = "Accept:";
const char referer[] = "Referer:";//????
const char host[] = "Host:";
const char encoding[] = "Accept-Encoding:";
const char language[] = "Accept-Language:";
const char charset[] = "Accept-Charset:";
const char cookie[] = "Cookie:";
const char user_agent[] = "User-Agent:";
const char connection[] = "Connection:";

const char cont_len[] = "Content-Length:";

const char GET[] = "GET";
const char HEAD[] = "HEAD";
const char POST[] = "POST";
const char VERSION[] = "HTTP/1.1";

const char MSG200[] = "HTTP/1.1 200 OK\r\n";
const char MSG404[] = "HTTP/1.1 404 NOT FOUND\r\n";
const char MSG411[] = "HTTP/1.1 411 LENGTH REQUIRED\r\n";
const char MSG500[] = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\n";
const char MSG501[] = "HTTP/1.1 501 NOT IMPLEMENTED\r\n";
const char MSG503[] = "HTTP/1.1 503 SERCIVE UNAVAILABLE\r\n";
const char MSG505[] = "HTTP/1.1 505 HTTP VERSION NOT SUPPORTED\r\n";

const char TEXT_HTML[] = "text/html";
const char TEXT_CSS[] = "text/css";
const char IMAGE_PNG[] = "image/png";
const char IMAGE_JPEG[] = "image/jpeg";
const char IMAGE_GIF[] = "image/gif";

const char server[] = "Server: Liso/1.0\r\n";
const char CGI[] = "/cgi";


struct buf *pipe_buf_array[MAX_SOCK];


void init_req_queue(struct req_queue *p) {
    p->req_head = NULL;
    p->req_tail = NULL;
    p->req_count = 0;
}

//void init_buf(struct buf* bufp, const char *cgiscript, int buf_sock, const char *www, struct sockaddr_in *cli_addr, int port){
void init_buf(struct buf* bufp, int buf_sock, const char *www, struct sockaddr_in *cli_addr, int port){

    assert(bufp != NULL);

    char clientIP[INET6_ADDRSTRLEN];
    const char *p = inet_ntop(AF_INET, &(cli_addr->sin_addr), clientIP, INET6_ADDRSTRLEN);
    bufp->remote_addr = (char *)calloc(1, strlen(p)+1);
    strcpy(bufp->remote_addr, p);

    bufp->server_port = port;

    bufp->buf_sock = buf_sock;

    bufp->req_queue_p = (struct req_queue *)calloc(1, sizeof(struct req_queue));
    
    // reception part
    bufp->http_req_p = (struct http_req *)calloc(1, sizeof(struct http_req));
    init_req_queue(bufp->req_queue_p);
    bufp->req_line_header_received = 0;
    bufp->req_fully_received = 1; // see parse_request for reason
    
    bufp->http_req_p = bufp->req_queue_p->req_head;

    bufp->rbuf = (char *) calloc(BUF_SIZE, sizeof(char));
    bufp->rbuf_head = bufp->rbuf;
    bufp->rbuf_tail = bufp->rbuf;
    bufp->line_head = bufp->rbuf;
    bufp->line_tail = bufp->rbuf;
    bufp->parse_p = bufp->rbuf;
    bufp->rbuf_free_size = BUF_SIZE;
    bufp->rbuf_size = 0;

    // reply part
    bufp->http_reply_p = NULL;

    bufp->buf = (char *) calloc(SEG_SIZE, sizeof(char));    
    //memset(bufp->buf, 0, BUF_SIZE);
    bufp->buf_head = bufp->buf; // p is not used yet in checkpoint-1
    bufp->buf_tail = bufp->buf_head; // empty buffer, off-1 sentinal

    bufp->buf_free_size = SEG_SIZE;
    bufp->buf_size = 0;

    bufp->res_line_header_created = 0;
    bufp->res_body_created = 0;
    bufp->res_fully_created = 0; 
    bufp->res_fully_sent = 1; // see create_response for reason
 
    bufp->www = www;
    bufp->path = (char *)calloc(PATH_MAX, sizeof(char)); // file path
    bufp->offset = 0; // file offest 

    bufp->allocated = 1;

    // ssl part
    //bufp->client_context = NULL;

    // cgi part
    //bufp->cgiscript = cgiscript;
    //bufp->cgi_fully_sent = 0;
    //bufp->cgi_fully_received = 0;

    //proj3
    bufp->status = RAW;
    bufp->sock2server = -1;
    bufp->sock2browser = -1;

    bufp->ts = 0.0;
    bufp->tf = 0.0;
    bufp->Bsize = 0;
    bufp->bitrate = 0;
    bufp->client_ip = (char *)calloc(128, sizeof(char));
    bufp->chunk_name = NULL;
    bufp->recv_time = 0;

    // clietn_ip
    inet_ntop(AF_INET, &(cli_addr->sin_addr), bufp->client_ip, INET_ADDRSTRLEN);
}


/* returnt the # of bytes pushed into the buffer  */
int push_str(struct buf* bufp, const char *str) {
    int str_size;
    str_size = strlen(str);
    
    if (str_size <= bufp->buf_free_size) {

	strncpy(bufp->buf_tail, str, str_size);

	bufp->buf_tail += str_size;
	bufp->buf_size += str_size;
	bufp->buf_free_size -= str_size;
	
	return str_size;

    } else {
	// deal with it later
	fprintf(stderr, "Warnning! push_str: no enough space in buffer\n");
	return 0;
    }
    
}

/* read from file and push to buffer
 * return 0 when bufp->free_size == 0 or EOF
 * return 1 when reading not finished
 * return -1 on error
 */
int push_fd(struct buf* bufp) {
    int fd;
    long readret;
    
    if (bufp->buf_free_size == 0) {
	dbprintf("Warnning! push_fd: bufp->free_size == 0, wait for sending bytes to free some space\n");
	return 1;
    }


    if ((fd = open(bufp->path, O_RDONLY)) == -1) {
	dbprintf("bufp->path:%s\n", bufp->path);
	perror("Error! push_fd, open");
	return -1;
    }
    
    dbprintf("push_fd: seek to previous position %ld and start reading\n", bufp->offset);
    lseek(fd, bufp->offset, SEEK_SET);

    if ((readret = read(fd, bufp->buf_tail, bufp->buf_free_size)) == -1) {
	perror("Error! push_fd, read");
	close(fd);
	return -1;
    } 

    fprintf(stderr, "push_fd: %ld bytes read this time\n", readret);
    
    bufp->buf_tail += readret;
    bufp->buf_size += readret;
    bufp->buf_free_size -= readret;
    bufp->offset += readret;

    if (readret < bufp->buf_free_size){
	// EOF
	close(fd);
	return 0;

    } else if (readret == bufp->buf_free_size) {
	// send the buffer out, and when the buffer is clear, come back and read again
	close(fd);
	return 1;
    }

    return 1; // this line is not reachable 
}


void reset_buf(struct buf* bufp) {
    if (bufp->allocated == 1) {

	//memset(bufp->buf, 0, BUF_SIZE); // for buf array
	memset(&(bufp->buf[0]), 0, BUF_SIZE); // for buf pointer

	bufp->buf_head = bufp->buf;
	bufp->buf_tail = bufp->buf;

	bufp->buf_size = 0;
	bufp->buf_free_size = BUF_SIZE;

    } else 
	fprintf(stderr, "Warning: reset_buf, buf is not allocated yet\n");
    
}

void reset_rbuf(struct buf *bufp) {
    
    if (bufp->allocated == 1) {
	//memset(bufp->rbuf, 0, BUF_SIZE);
	memset(&(bufp->rbuf[0]), 0, BUF_SIZE);
    
	bufp->rbuf_head = bufp->rbuf;
	bufp->rbuf_tail = bufp->rbuf;
	bufp->line_head = bufp->rbuf;
	bufp->line_tail = bufp->rbuf;
	bufp->parse_p = bufp->rbuf;
    
	bufp->rbuf_size = 0;
	bufp->rbuf_free_size = BUF_SIZE;

	bufp->status = RAW;
	bufp->sock2browser = -1;
	bufp->sock2server = -1;

    } else 
	fprintf(stderr, "Warnning: reset_rbuf, buf is not allocated yet\n");
    
}



int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's closed, the client might receive connection reset error\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}

struct http_req *req_peek(struct req_queue *q) {
    return q->req_head;
}

void req_enqueue(struct req_queue *q, struct http_req *p) {
    struct http_req *r;

    if (q->req_head != NULL) {
	r = q->req_head;
	while (r->next != NULL)
	    r = r->next;
	r->next = p;
    } else {
	q->req_head = p;
    }
    
    q->req_count += 1;
}

struct http_req *req_dequeue(struct req_queue *q) {
    struct http_req *r;

    if (q->req_head == NULL) {
	fprintf(stderr, "Error! req_dequeue: req_queue is empty\n");
	return NULL;
    }
    r = q->req_head;
    q->req_head = r->next;

    q->req_count -= 1;
    
    return r;
}

void dbprint_queue(struct req_queue *q) {
    struct http_req *p = q->req_head;

    if (p == NULL)
	dbprintf("print_queue:%d requests\nnull\n", q->req_count);

    dbprintf("print_queue:%d requests\n", q->req_count);
    while (p != NULL) {
	dbprintf("request: %s %s %s ...\n", p->method, p->uri, p->version);
	p = p->next;
    }
    

}


void send_error(int sock, const char msg[]) {
    int flag;

    if ((flag = fcntl(sock, F_GETFL, 0)) == -1)
	perror("Error! send_error fcntl F_GETFL");

    flag |= O_NONBLOCK;

    if (fcntl(sock, F_SETFL, flag) == -1)
	perror("Error! send_error fcntl F_SETFL");

    if (send(sock, msg, strlen(msg), 0) == -1)
	perror("Error! send_error send");
        
}

int close_socket(int sock) {
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int clear_buf(struct buf *bufp){
    
    if (bufp->allocated == 0) {
	fprintf(stderr, "Warnning! clear_buf a buf which is not allocated\n");
	return -1;
    }

    free(bufp->http_req_p);
    free(bufp->req_queue_p);
    free(bufp->rbuf);
    free(bufp->buf);
    free(bufp->path);
    //free(bufp->client_context);
    
    bufp->allocated = 0;
    //bufp->client_context = NULL;

    return 0;
}

void clear_buf_array(struct buf *buf_pts[], int maxfd){

    int i;

    for (i = 0; i <= maxfd; i++ ) {
	if (buf_pts[i]->allocated == 1) {
	    free(buf_pts[i]);
	    //clear_buf(buf_pts[i]);
	}
    }
}


void dequeue_request(struct buf *bufp) {

    // dequeue a request if previous response if fully sent
    if (bufp->res_fully_sent == 1) {

	// reset part of buf
    	memset(bufp->path, 0, PATH_MAX);
    	bufp->offset = 0;

    	bufp->http_reply_p = req_dequeue(bufp->req_queue_p);

    	bufp->res_line_header_created = 0;
    	bufp->res_body_created = 0;
	bufp->res_fully_created = 0;
	bufp->res_fully_sent = 0; 

	//bufp->cgi_fully_sent = 0;
	//bufp->cgi_fully_received = 0;

    } else {

	dbprintf("dequeue_request: fails, since fully_sent==0, current req is not fully sent yet\n");
	dbprintf("dequeue_request: current req:%s %s %s\n", bufp->http_reply_p->method,bufp->http_reply_p->uri, bufp->http_reply_p->version);
    }

}



/* return 0 on valid path, -1 on invalid path  */
int check_path(struct buf *bufp) {

    int len;
    struct stat status;

    if (stat(bufp->path, &status) == -1) {
	perror("Error! check_path stat");		
	return -1;
    }

    if (S_ISDIR(status.st_mode)) {
	len = strlen(bufp->path);
	if (bufp->path[len-1] == '/')
	    bufp->path[len-1] = '\0';
	strcat(bufp->path, "/index.html");
    }

    // check if path is valid file path
    if (stat(bufp->path, &status) == -1) {
	perror("Error! check_path stat");	
	return -1;
		
    } else if (S_ISREG(status.st_mode)) {
	dbprintf("check_path: locate file %s\n", bufp->path);
    }
    
    return 0;
}


void enlist(char *arg_list[], const char *arg) {
    char **p;

    p = arg_list;

    while (*p != NULL)
	p++;

    //*p = arg;
    *p = (char *)calloc(1, strlen(arg)+1);
    strcpy(*p, arg);

    *(p+1) = NULL;
}

char *delist(char *arg_list[]) {
    char **p;

    p = arg_list;
    arg_list++;
    
    return *p;
}

void dbprintf_arglist(char **list) {
    char *p;
    int count;

    count = 0;
    while ((p = *(list++)) != NULL)
	dbprintf("list[%d]:%s\n", count++, p);

}


void logprint(const char *log_file, const char *s) {

     FILE *fp;

     if (DEBUG) {
	 fputs(s, stderr);
     } else {
	 fp = fopen(log_file, "a");
	 
	 if (fp) {

	     fputs(s, fp);
	     fclose(fp);
	 } 
     }
     
	      
}

// for proj3

/* return a int* array of max size 16, end with NULL  */
int *getf4m(int sock) {
    
    char buf[BUF_SIZE];
    char *str = NULL;
    int ret;
    char *p = NULL;

    // send req
    memset(buf, 0, BUF_SIZE);
    
    str = "GET /vod/big_buck_bunny.f4m HTTP/1.0\r\n\r\n";
    strcpy(buf, str);

    p = buf;
    while ((ret = send(sock, p, strlen(p), 0)) >0) 
	p += ret;

    if (ret == -1) {
	perror("Error! getf4m, send");
	exit(-1);
    }

    // recv reply
    memset(buf, 0, BUF_SIZE);

    p = buf;
    while ((ret = recv(sock, p, BUF_SIZE, 0)) > 0) 
	p += ret;

    if (ret == -1){
	printf("Error! getf4m, recv\n");
	exit(-1);
    }
    
    int *res =  parsef4m(buf);
        
    return res;
}


int *parsef4m(char *buf) {
    assert(buf != NULL);

    int max_num = 16;
    int *rate = (int *)calloc(max_num, sizeof(int));
    char *p = NULL;
    char *p1, *p2;
    int count = 0;

    while ((p = strstr(buf, "bitrate")) != NULL) {
	if ((p1 = strchr(p, '"')) == NULL || (p2 = strchr(p1+1, '"')) == NULL) {
	    printf("Error! f4m is in wrong format\n");
	    exit(-1);
	}
	
	p1 += 1;
	*p2 = '\0';
	rate[count++] = atoi(p1);

	if (count >= max_num-1) {
	    printf("Warning! parsef4m, too many bitrate\n");
	    break;
	}

	buf = p2 + 1;
    }
    
    return rate;
}



int logging(struct buf *bufp, double alpha, char *log) {
    assert(bufp != NULL);
    assert(log != NULL);

    //assert(bufp->ts != 0);
    //assert(bufp->tf != 0);
    assert(bufp->tf >= bufp->ts);
    
    char line[1024];
    time_t cur_time;
    double duration;
    double tput;
    //static double avg_tput = 0.0;

    FILE *fp = NULL;
    
    if (bufp->chunk_name == NULL) {
	printf("logging: not chunk data, no need to log\n");
	return 0;
    }

    fp = fopen(log, "a");
    assert(fp != NULL);

    memset(line, 0, 1024);

    // time
    time(&cur_time);
    
    // duration
    assert(bufp->tf > bufp->ts);
    duration = bufp->tf - bufp->ts;
    //dbprintf("logging: duration %f = %f - %f\n", duration, bufp->tf, bufp->ts);

    // tput
    tput = bufp->Bsize / duration; 
    tput = (tput) * 8 / 1000;
    //printf("proxy logging: tput %f Kbps\n", tput);
    if (avg_tput == 0.0) 
    	avg_tput = tput;
    else 
	avg_tput = alpha * tput + (1 - alpha) * avg_tput;
    
    // bitrate, just bufp->bitrate
    //printf("proxy logging: bitrate %d \n", bufp->bitrate);
    //cprintf("proxy logging: tput:%f, new avg_tput:%f\n", tput, avg_tput);
    // client_ip, just bufp->client_ip
    //dbprintf("proxy logging: clent_ip %s\n", bufp->client_ip);
    // chunk_name, just bufp->chunkname
    //printf("proxy logging: chunk_name %s\n", bufp->chunk_name);

    // log
    //sprintf(line, "%ld %f %f %f %d %s %s\n", bufp->recv_time, duration, tput, avg_tput, bufp->bitrate, bufp->client_ip, bufp->chunk_name);
    sprintf(line, "%ld %f %f %f %d %s %s\n", cur_time, duration, tput, avg_tput, bufp->bitrate, bufp->client_ip, bufp->chunk_name);

    fputs(line, fp);

    fclose(fp);

    return 0;
    
}

int transfer_info(struct buf *from, struct buf *to) {
  /*
  buf_pts[buf_pts[i]->sock2server]->sock2browser = buf_pts[i]->sock2browser;
  buf_pts[buf_pts[i]->sock2server]->ts = buf_pts[i]->ts;
  buf_pts[buf_pts[i]->sock2server]->client_ip = buf_pts[i]->client_ip;
  buf_pts[buf_pts[i]->sock2server]->bitrate = buf_pts[i]->bitrate;
  buf_pts[buf_pts[i]->sock2server]->chunk_name = buf_pts[i]->chunk_name;			
  */

  to->sock2browser = from->sock2browser;
  to->ts = from->ts;
  to->client_ip = from->client_ip;
  to->bitrate = from->bitrate;
  to->chunk_name = from->chunk_name;			
  
  return 0;
}

