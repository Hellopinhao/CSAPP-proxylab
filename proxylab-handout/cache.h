struct object_node{
    char host[20];
    char uri[50];
    char *server_object;
    char *header;
    int content_length;
    struct object_node *next;
    struct object_node *pre;
};

void init_cache(void);
int get_object(char *host, char *uri, char *content, char *header);
void write_object(char *host, char *uri, char *content, char *header, int content_length);

