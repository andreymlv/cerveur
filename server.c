#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "http.h"

#define PORT "3490" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

int main(void) {
  http_server server = {0, "3490", 10};
  int server_socket = init_server(&server);
  printf("HTTP Server Initialized\nPort: %s\n", server.port);

  // registering Routes
  struct route *route = init_route("/", "index.html");
  route = add_route(route, "/about", "about.html");
  printf("all available routes:\n");
  inorder(route);

  printf("server: waiting for connections...\n");
  while (1) { // main accept() loop
    http_client client;
    int client_socket = init_client(&server, &client);
    printf("server: got connection from %s\n", client.ip);
    int pid = fork();
    switch (pid) {
    case -1:
      close(client_socket);
      close(server_socket);
      perror("fork");
      exit(1);
    case 0:
      close(client_socket); // parent doesn't need this
      break;
    default:
      close(server_socket); // child doesn't need the listener
      recv_send_client(&client, route);
      close(client_socket);
    }
  }
  return 0;
}
