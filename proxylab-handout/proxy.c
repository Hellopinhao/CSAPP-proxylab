#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define BUF_SIZE 16

/* You won't lose style points for including this long line in your code */

char  *user_agent_hdr= "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void parse_uri(char *uri, char *hostname, char *proxy_uri, char* port);
void handle_header(char *current_line, char *header, int *is_existed);
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) ;
void get_object_from_server(char *header, int *content_length, int *is_get, char * header_from_server, char *content_from_server, char *host, char *port);
void *thread(void *vargp);

sbuf_t sbuf;

int main(int argc, char **argv)
{
    int listen_fd, conn_fd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    int i;
    pthread_t tid;
    struct sockaddr_storage clientaddr;

    if(argc!=2){
        fprintf(stderr,"usage: %s <port>\n",argv[0]);
        exit(1);
    }

    listen_fd=Open_listenfd(argv[1]);

    sbuf_init(&sbuf,BUF_SIZE);

    for(i=0;i<BUF_SIZE;i++){
        Pthread_create(&tid,NULL,thread,NULL);
    }

    while(1){
        clientlen=sizeof(clientaddr);
        conn_fd=Accept(listen_fd,(SA *)&clientaddr,&clientlen);
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accept connection from (%s %s)\n",hostname,port);

        sbuf_insert(&sbuf,conn_fd);
    }
    return 0;
}

void *thread(void *vargp){
    Pthread_detach(Pthread_self());
    int conn_fd;
    static pthread_once_t once =PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_cache);
    while(1){
        conn_fd=sbuf_remove(&sbuf);
        doit(conn_fd);
        Close(conn_fd);
    }
}

void doit(int fd){
    char method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char buf[MAXBUF];
    char header[MAXBUF];
    char header_from_server[MAXLINE];
    char content_from_server[MAX_OBJECT_SIZE];
    char server_host[50];
    char server_port[10]="80";
    char proxy_uri[50];
    char proxy_version[10];
    //char connection[10]="close";
    //char proxy_connection[10]="close";
    int is_existed[4]={0,0,0,0};
    int i;
    int content_length;
    int is_get=0;
    int is_have=0;
    
    rio_t rio;
    
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,buf,MAXLINE);
    sscanf(buf,"%s %s %s",method,uri,version);

    if(strcasecmp(method,"GET")){
        clienterror(fd,method,"501","Not implemented","Tiny does not implement this method");
        return;
    }

    strcpy(proxy_version,"HTTP/1.0");

    parse_uri(uri,server_host,proxy_uri,server_port);

    sprintf(header,"%s %s %s\r\n",method,proxy_uri,proxy_version);

    do{
        Rio_readlineb(&rio,buf,MAXLINE);
        if(!strcmp(buf,"\r\n")){
            break;
        }
        handle_header(buf,header,is_existed);
    }while(1);
    for(i=0;i<4;i++){
        if(is_existed[i]==0){
            switch (i)
            {
            case 0:
                sprintf(header,"%sHost: %s\r\n",header,server_host);
                break;
            case 1:
                sprintf(header,"%s%s",header,user_agent_hdr);
                break;
            case 2:
                sprintf(header,"%sConnection: close\r\n",header);
                break;
            case 3:
                sprintf(header,"%sProxy-Connection: close\r\n",header);
                break;
            default:
                break;
            }
        }
    }
    sprintf(header,"%s\r\n",header);

    content_length=get_object(server_host,proxy_uri,content_from_server,header_from_server);
    if(content_length==0){
        get_object_from_server(header,&content_length,&is_get,header_from_server,content_from_server,server_host,server_port);
    }
    else{
        is_get=1;
        is_have=1;
    }
    Rio_writen(fd,header_from_server,strlen(header_from_server));
    if(is_get==0)
        content_length=strlen(content_from_server)+1;
    Rio_writen(fd,content_from_server,content_length);
    if(is_have==0){
        if(content_length<=MAX_OBJECT_SIZE)
            write_object(server_host,proxy_uri,content_from_server,header_from_server,content_length);
    }
}

void parse_uri(char *uri, char *hostname, char *proxy_uri, char *port){
    *proxy_uri='/';
    *(proxy_uri+1)='\0';
    if(*uri=='h'||*uri=='H'){
        uri=uri+7;
        char *p;
        if((p=strstr(uri,":"))==NULL){
            p=strstr(uri,"/");
            if(p==NULL){
                strcpy(hostname,uri);
                return;
            }
            *p='\0';
            strcpy(hostname,uri);
            uri=p+1;
            strcpy(proxy_uri+1,uri);
        }
        else{
            *p='\0';
            strcpy(hostname,uri);
            uri=p+1;
            p=strstr(uri,"/");
            if(p==NULL){
                strcpy(port,uri);
                return;
            }
            *p='\0';
            strcpy(port,uri);
            uri=p+1;
            strcpy(proxy_uri+1,uri);
        }
    }
    else{
        strcpy(proxy_uri,uri);
    }
}

void handle_header(char *current_line, char *header, int *is_existed) {
	char *p = strstr(current_line, ":");
	*p = '\0';
	if (strcmp(current_line, "Host") == 0) {
		*is_existed = 1;
		*p = ':';
		sprintf(header, "%s%s", header, current_line);
	}
	else if (strcmp(current_line, "User-Agent") == 0) {
		*(is_existed + 1) = 1;
		sprintf(header, "%s%s", header, user_agent_hdr);
	}
	else if (strcmp(current_line, "Connection") == 0) {
		*(is_existed + 2) = 1;
		sprintf(header, "%sConnection: close\r\n", header);
	}
	else if (strcmp(current_line, "Proxy-Connection") == 0) {
		*(is_existed + 3) = 1;
		sprintf(header, "%sProxy-Connection: close\r\n", header);
	}
	else {
		*p = ':';
		sprintf(header, "%s%s", header, current_line);
	}
}


void get_object_from_server(char *header, int *content_length, int *is_get, char * header_from_server, char *content_from_server, char *host, char *port){
    int fd;
    char buf[MAXLINE];
    rio_t rio;
    char *p=NULL;
    fd=Open_clientfd(host,port);
    Rio_writen(fd,header,strlen(header));
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,header_from_server,MAXLINE);
    Rio_readlineb(&rio,buf,MAXLINE);
    while(strcmp(buf,"\r\n")){
        if(*is_get){
            sprintf(header_from_server,"%s%s",header_from_server,buf);
        }
        else{
            p=strstr(buf,":");
            *p='\0';
            if(strcmp(buf,"Content-Length")==0||strcmp(buf,"Content-length")==0){
                *content_length=atoi(p+2);
                *is_get=1;
            }
            *p=':';
            sprintf(header_from_server,"%s%s",header_from_server,buf);
        }
        Rio_readlineb(&rio,buf,MAXLINE);
    }
    sprintf(header_from_server,"%s\r\n",header_from_server);
    Rio_readnb(&rio,content_from_server,MAX_CACHE_SIZE);
    Close(fd);
}

void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}