#include "csapp.h"
#include "cache.h"

static sem_t mutex,w;

static int cache_cnt;
static int read_cnt;

struct object_node *head;


void init_cache(void){
    head=(struct object_node*)malloc(sizeof(struct object_node));
    head->next=head;
    head->pre=head;
    read_cnt=0;
    cache_cnt=0;
    Sem_init(&mutex,0,1);
    Sem_init(&w,0,1);
}

int get_object(char *host, char *uri, char *content, char *header){
    P(&mutex);
    read_cnt++;
    if(read_cnt==1)
        P(&w);
    V(&mutex);

    struct object_node *current=head->next;
    int content_length=0;
    while(current!=head){
        if(strcmp(current->host,host)==0 && strcmp(current->uri,uri)==0){
            is_find=current->content_length;
            memcpy(header,current->header,strlen(current->header)+1);
            memcpy(content,current->server_object,current->content_length);
            break;
        }
        current=current->next;
    }

    P(&mutex);
    readcnt--;
    if(read_cnt==0)
        V(&w);
    V(&mutex);
}

void write_object(char *host, char *uri, char *content, char *header, int content_length){
    struct object_node *c_object;
    int host_len;
    int uri_len;
    int header_len;
    P(&w);
    if(cache_cnt>=10){
        struct object_node *out_node=head->next;
        head->next=out_node->next;
        out_node->next->pre=head;
        free(out_node->header);
        free(out_node->server_object);
        free(out_node);
    }
    c_object=(struct object_node *)malloc(sizeof(struct object_node));
    c_object->content_length=content_length;
    host_len=strlen(host)+1;
    uri_len=strlen(uri)+1;
    header_len=strlen(header)+1;
    c_object->header=malloc(header_len);
    c_object->server_object=malloc(content_length);
    memcpy(c_object->host,host,host_len);
    memcpy(c_object->uri,uri,uri_len);
    memcpy(c_object->header,header,header_len);
    memcpy(c_object->server_object,content,content_length);
    c_object->next=head;
    c_object->pre=head->pre;
    head->pre=c_object;
    c_object->pre->next=c_object;
    V(&w);
}




