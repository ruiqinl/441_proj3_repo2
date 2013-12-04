
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include "http_replyer.h"
#include "helper.h"

/* return the size of buffer inside struct buf, 0 or a positive number  */ 
int create_response(struct buf *bufp) {
    
    int locate_ret;

    if (bufp->is_cgi_req == 0)
	dequeue_request(bufp);
    
    if (bufp->res_fully_created == 1)
	return bufp->buf_size; // no more reply to created, just send what left in buf

    if ((locate_ret = locate_file(bufp)) == -1) {
	dbprintf("create_response: file not located, create msg404\n");

	push_error(bufp, MSG404);
	//return bufp->buf_size; 
    } else if (locate_ret == 0){
	dbprintf("create_response: static file located, path:%s\n", bufp->path);

	if (create_line_header(bufp) == -1) return bufp->buf_size; // msg404
	if (create_res_body(bufp) == -1) return bufp->buf_size; // msg 500, only works if it's GET method

	dbprintf("create_response: bufp->size:%d\n", bufp->buf_size);
	//return bufp->buf_size;
    } 
    return bufp->buf_size;    
}


/* return -1 on file not found, 0 on regular file found*/
int locate_file(struct buf *bufp) {

    dbprintf("locate_file: bufp->res_line_header_create==%d\n", bufp->res_line_header_created);
    // file not located yet
    if (bufp->res_line_header_created == 0) {
	
	memset(bufp->path, 0, PATH_MAX);

	dbprintf("locate_file: uri:%s\n", bufp->http_reply_p->uri);

	// not a file, attach 
	//	p = strrchr(bufp->http_reply_p->uri, '/');
	//	if (p == NULL)
	//	    dbprintf("locate_file: uri contains no /\n");
	//	if (strcmp(p, "/login") == 0) {
	    // no need to memset
	//	    strcpy(p, "/templates/login.html");
	//        }
	//	if (strcmp(p, "/layout") == 0) {
	//	    strcpy(p, "/templates/layout.html");
	//	}
	//	if (strcmp(p, "/show_entries") == 0) {
	//	    strcpy(p, "/templates/show_entries.html");
	//	}
	//..


	strcpy(bufp->path, bufp->www);
	strcat(bufp->path, bufp->http_reply_p->uri);

	
	dbprintf("locate_file: save path, bufp->path:%s\n", bufp->path);

	// check if the path is invalid
	if (check_path(bufp) == -1) {
	    dbprintf("lcoate_file: check_path failed");
	    return -1; 
	} 
    }
    
    return 0;
}

int create_line_header(struct buf *bufp) {

    struct http_req *http_reply_p;

    http_reply_p = bufp->http_reply_p;

    if (bufp->res_line_header_created == 1) 
	return 0; // do nothing
    
    if (strcmp(http_reply_p->version, VERSION) != 0){
	dbprintf("create_line_header: %s not right, send msg505 back\n", http_reply_p->version);
	
	push_error(bufp, MSG505);
	return -1;
    }
    
    if (strcmp(http_reply_p->method, "GET") == 0
	|| strcmp(http_reply_p->method, "HEAD") == 0
	|| strcmp(http_reply_p->method, "POST") == 0) {

	dbprintf("create_line_header: create status line and headers for %s method\n", bufp->http_req_p->method);

	// create status line and headers
	push_str(bufp, MSG200);
	push_header(bufp);
	push_str(bufp, CRLF);

	bufp->res_line_header_created = 1;

    } else {
	// other methods, not implemented
	dbprintf("create_line_header: method not implemented\n");

	push_error(bufp, MSG501);
	return -1;
    }

    return 0;
}

int create_res_body(struct buf *bufp) {

    int push_ret;

    //if (bufp->res_body_created == 1)
    if (bufp->res_fully_created == 1) {
	dbprintf("create_res_body: res_fully_created == 1\n");
	return 0;
    }
    
    if (strcmp(bufp->http_reply_p->method, "GET") == 0){

	if ((push_ret = push_fd(bufp)) == 0) {

	    dbprintf("create_res_body: file %s is read through\n", bufp->path);
	    //bufp->res_body_created = 1; 
	    bufp->res_fully_created = 1;

	} else if (push_ret == -1) {

	    fprintf(stderr, "Error! Failed reading file, handle this later\n");
	    push_error(bufp, MSG500);

	} else if (push_ret == 1) {

	    dbprintf("create_res_body: file %s is not read through yet\n", bufp->path);
	    //bufp->res_body_created = 0;
	    bufp->res_fully_created = 0;
	}

    } else {
	//bufp->res_body_created = 1; // no need to create at all
	bufp->res_fully_created = 1;
    }

    return 0;
}


void push_header(struct buf *bufp){
    char *p;

    p = date_str();
    dbprintf("date_str:%s", p);
    push_str(bufp, p); 
    free(p);
	    
    p = connection_str(bufp);
    dbprintf("connection_str:%s", p);
    push_str(bufp, p);
    free(p);
	    
    dbprintf("server:%s", server);
    push_str(bufp, server);

    p = cont_len_str(bufp);
    dbprintf("cont_len_str:%s", p);
    push_str(bufp, p);
    free(p);
	    
    p = cont_type_str(bufp);
    dbprintf("cont_type_str:%s", p);
    push_str(bufp, p);
    free(p);
    
    p = last_modified_str(bufp);
    dbprintf("last_modified_str:%s", p);
    push_str(bufp, p);
    free(p);

}

void push_error(struct buf *bufp, const char *msg) {

    reset_buf(bufp);
    push_str(bufp, msg);
    push_header(bufp);
    push_str(bufp, CRLF);

    bufp->res_line_header_created = 1;
    bufp->res_body_created = 1;
    bufp->res_fully_created = 1;

}


/* return 0 if buf->size == 0  */
int send_response(int sock, struct buf *bufp) {
    int sendret;

    if (bufp->buf_size == 0) {
	dbprintf("send_response: buffer is empty, sending finished\n");
	return 0;
    }

    sendret = send(sock, bufp->buf_head, bufp->buf_size, 0);

    if (sendret == -1) {
	perror("Error! send_response: send");
	return -1;
    }


    bufp->buf_head += sendret;
    bufp->buf_size -= sendret;
    //    bufp->buf_free_size += sendret; ???????? what !!!!!!!!
    
    dbprintf("send_response: %d bytes are sent\n", sendret);

    // whole buf is sent, reset it for possible more respose
    if (bufp->buf_size == 0) {
	reset_buf(bufp);
	dbprintf("send_response: whole buf is sent, reset it\n");
    }


    return sendret;
}


char *date_str() {

    time_t rawtime;
    struct tm *timeinfo;
    char *time_str = (char *)calloc(128, sizeof(char));
    
    rawtime = time(NULL);
    timeinfo = gmtime(&rawtime);

    strcpy(time_str, "Date: ");
    strcat(time_str, asctime(timeinfo));
    time_str[strlen(time_str)-1] = '\0';
    strcat(time_str, "\r\n");
    //strcpy(time_str + strlen("Date: ") + strlen(time_str) - 1, "GMT\r\n");
    //???unable to add GMT???

    return time_str;

}

char *connection_str(struct buf *bufp) {
    
    char *str = (char *)calloc(128, sizeof(char));

    if (strcmp(bufp->http_req_p->connection, "keep-alive") == 0)
	strcpy(str, "Connection: keep-alive\r\n");
    else
	strcpy(str, "Connection: closed\r\n");
    
    return str;
}

char *cont_len_str(struct buf *bufp) {

    struct stat status;
    char *str = (char *)calloc(128, sizeof(char));
    
    if (stat(bufp->path, &status) == -1) {
	perror("Error! cont_len_str, stat");
	strcpy(str, "Content-Length:\r\n");
	return str;
    }
    
    sprintf(str, "Content-Length: %ld\r\n", status.st_size);

    return str;
}

char *cont_type_str(struct buf *bufp) {

    char *str = (char *)calloc(128, sizeof(char));
    char *p;

    if ((p = strstr(bufp->path, ".html")) != NULL 
	&& *(p + strlen(".html")) == '\0') {
	sprintf(str, "Content-Type: %s\r\n", TEXT_HTML);
    }

    if ((p = strstr(bufp->path, ".css")) != NULL 
	&& *(p + strlen(".css")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", TEXT_CSS);

    if ((p = strstr(bufp->path, ".png")) != NULL 
	&& *(p + strlen(".png")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_PNG);

    if ((p = strstr(bufp->path, ".jpeg")) != NULL 
	&& *(p + strlen(".jpeg")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_JPEG);

    if ((p = strstr(bufp->path, ".gif")) != NULL 
	&& *(p + strlen(".gif")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_GIF);

    return str;
}


char *last_modified_str(struct buf *bufp) {
    
    struct stat status;
    struct tm *timeinfo;
    char *str = (char *)calloc(128, sizeof(char));
    
    if (stat(bufp->path, &status) == -1) {
	perror("Error! last_modified_str, stat");
	strcpy(str, "Last-Modified:\r\n");
	return str;
    }
    
    timeinfo = gmtime(&(status.st_mtime));

    strcpy(str, "Last-Modified: ");
    strcat(str, asctime(timeinfo));

    return str;
}

