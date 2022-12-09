#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <netinet/in.h>

#include "routes.h"

typedef struct http_server {
  int socket;
  const char *port;
  const int backlog;
} http_server;

typedef struct http_client {
  int socket;
  struct sockaddr_storage their_addr; // connector's address information
  char ip[INET6_ADDRSTRLEN];
} http_client;

int init_server(http_server *http_server);
int init_client(http_server *http_server, http_client *http_client);
int recv_send_client(http_client *http_client, struct route *root);

#endif
