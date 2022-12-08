#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

typedef struct http_server {
  int socket;
  int port;
} http_server;

void init_server(http_server *http_server, int port);

#endif
