#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>


#define DBPRINTF 0
#define dbprintf(...) do{if(DBPRINTF) fprintf(stdout, __VA_ARGS__); }while(0)

#define MAX_SOCK 1024
#define BUF_SIZE 8192
#define PATH_MAX 1024
#define HEADER_LEN 1024
#define ARG_MAX 1024

extern const char CRLF[]; 
extern const char CRLF2[];

extern const char cont_type[];
extern const char accept_range[];
extern const char referer[];
extern const char host[];
extern const char encoding[];
extern const char language[];
extern const char charset[];
extern const char cookie[];
extern const char user_agent[];
extern const char connection[];

extern const char cont_len[];

extern const char VERSION[];
extern const char GET[];
extern const char HEAD[];
extern const char POST[];

extern const char MSG200[];
extern const char MSG404[];
extern const char MSG411[];
extern const char MSG500[];
extern const char MSG501[];
extern const char MSG503[];
extern const char MSG505[];

extern const char server[];
extern const char CGI[];

extern const char TEXT_HTML[];
extern const char TEXT_CSS[];
extern const char IMAGE_PNG[];
extern const char IMAGE_JPEG[];
extern const char IMAGE_GIF[];

extern const char ROOT[];
extern const char CGI_FOLDER[];

extern const int CODE_UNSET;

extern struct buf *pipe_buf_array[];


struct http_req {

    int cont_len;
    char cont_type[HEADER_LEN];
    //    char remote_addr[HEADER_LEN];  in buf
    char method[HEADER_LEN];
    char uri[HEADER_LEN];    
    char http_accept[HEADER_LEN];
    char http_referer[HEADER_LEN];
    char http_accept_encoding[HEADER_LEN];
    char http_accept_language[HEADER_LEN];
    char http_accept_charset[HEADER_LEN];
    char host[HEADER_LEN];
    char cookie[HEADER_LEN];
    char user_agent[HEADER_LEN];
    char connection[HEADER_LEN];
    char https[HEADER_LEN];

    char version[HEADER_LEN];    

    char *contp;

    int use_cgi_folder;
    char *cgi_arg_list[ARG_MAX];
    char *cgi_env_list[ARG_MAX];
    int fds[2];

    // proj3
    char *orig_req;
    char *orig_cur;

    struct http_req *next;

};

struct req_queue {

    struct http_req *req_head;
    struct http_req *req_tail;
    int req_count;    

};

#define RAW 0x00
#define FROM_BROWSER 0x01
#define TO_BROWSER 0x02
#define FROM_SERVER 0x04
#define TO_SERVER 0x08

#define SEG_SIZE 4*1024*1024 // 4m

extern double avg_tput;
extern int *all_rates;
extern char *node; // video.cs.cmu.edu
extern char *service; // 8080, this is the video server port

struct buf {

    int is_cgi_req;

    char https[2];

    char *remote_addr; 

    int buf_sock;

    struct req_queue *req_queue_p;

    // reception part from browser
    struct http_req *http_req_p;
    int req_line_header_received;
    int req_body_received;
    int req_fully_received;
    int rbuf_req_count;

    char *rbuf;
    char *rbuf_head;
    char *rbuf_tail;
    char *line_head;
    char *line_tail;
    char *parse_p;
    int rbuf_free_size;
    int rbuf_size;

    // reply part
    struct http_req *http_reply_p;

    char *buf;
    char *buf_head;
    char *buf_tail;
    int buf_size; // tail - head
    int buf_free_size; // BUF_SIZE - size

    int res_line_header_created;
    int res_body_created;
    int res_fully_created;
    int res_fully_sent;

    const char *cgiscript;
    const char *www; // save www path
    char *path; // GET/HEAD file path
    //    char path[PATH_MAX];
    long offset; // keep track of read offset

    
    int allocated;

    // ssl part
    //SSL *client_context;

    // cgi part
    int server_port;
    
    //int cgi_fully_sent;
    //int cgi_fully_received;

    // proj3
    int status;
    int sock2server;
    int sock2browser;

    //time_t ts; // done in recv_BROW
    //time_t tf; // done in recv_SERVER
    double ts; // in float seconds
    double tf; // in float seconds
    size_t Bsize; // done in recv_SERVER, in bytes
    int bitrate; // done in change_bitrate
    char *client_ip;// done in init_buf
    char *chunk_name; // done in log_chunkname
    time_t recv_time;
};

void init_buf(struct buf *bufp, int buf_sock, const char *www, struct sockaddr_in *cli_addr, int server_port);
void reset_buf(struct buf *bufp);
void reset_rbuf(struct buf *bufp);
int is_2big(int fd);

int push_str(struct buf* bufp, const char *str);
int push_fd(struct buf* bufp);
//void push_error(struct buf *bufp, const char *msg);

void send_error(int sock, const char msg[]);

struct http_req *req_peek(struct req_queue *q);
void req_enqueue(struct req_queue *q, struct http_req *p);
struct http_req *req_dequeue(struct req_queue *q);
void dequeue_request(struct buf *bufp);
void dbprint_queue(struct req_queue *q);

int close_socket(int sock);
int clear_buf(struct buf *bufp);
void clear_buf_array(struct buf *buf_pts[], int maxfd);

int check_path(struct buf *bufp);

void enlist(char *arg_list[], const char *arg);
char *delist(char *arg_list[]);
void dbprintf_arglist(char **list);

void logprint(const char *log_file, const char *s);

// for proj3
int *getf4m(int sock);
int *parsef4m(char *buf);
int logging(struct buf *bufp, double alpha, char *log);
int transfer_info(struct buf *from, struct buf *to);

#endif
