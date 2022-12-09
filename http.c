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
#include "response.h"

static void sigchld_handler(int s) {
  (void)s; // quiet unused variable warning

  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int init_server(http_server *http_server) {
  struct addrinfo hints, *servinfo, *p;
  struct sigaction sa;
  int yes = 1;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, http_server->port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((http_server->socket =
             socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(http_server->socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(http_server->socket, p->ai_addr, p->ai_addrlen) == -1) {
      close(http_server->socket);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(http_server->socket, http_server->backlog) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  return http_server->socket;
}

int init_client(http_server *http_server, http_client *http_client) {
  socklen_t sin_size;
  sin_size = sizeof http_client->their_addr;
  http_client->socket =
      accept(http_server->socket, (struct sockaddr *)&http_client->their_addr,
             &sin_size);
  if (http_client->socket == -1) {
    perror("accept");
    return -1;
  }

  if (inet_ntop(http_client->their_addr.ss_family,
                get_in_addr((struct sockaddr *)&http_client->their_addr),
                http_client->ip, sizeof http_client->ip) == NULL) {
    perror("inet_ntop");
    close(http_client->socket);
    exit(0);
  }
  return http_client->socket;
}

int recv_send_client(http_client *http_client, struct route *root) {
  char buf[4096]; // buffer for client data
  int nbytes;
  if ((nbytes = recv(http_client->socket, buf, sizeof buf, 0)) <= 0) {
    // got error or connection closed by client
    if (nbytes == 0) {
      // connection closed
      printf("server: socket %d hung up\n", http_client->socket);
    } else {
      perror("recv");
    }
    close(http_client->socket); // bye!
    exit(0);
  } // parsing client socket header to get HTTP method, route
  char *method = "";
  char *urlRoute = "";

  char *client_http_header = strtok(buf, "\n");
  printf("\n\nclient http header: %s\n\n", client_http_header);

  char *header_token = strtok(client_http_header, " ");

  int header_parse_counter = 0;
  while (header_token != NULL) {
    switch (header_parse_counter) {
    case 0:
      method = header_token;
      break;
    case 1:
      urlRoute = header_token;
      break;
    }
    header_token = strtok(NULL, " ");
    header_parse_counter++;
  }

  printf("The method is %s\n", method);
  printf("The route is %s\n", urlRoute);

  char template[256];
  if (strstr(urlRoute, "/static/") != NULL) {
    strcat(template, "static/index.css");
  } else {
    struct route *destination = search(root, urlRoute);
    strcat(template, "templates/");

    if (destination == NULL) {
      strcat(template, "404.html");
    } else {
      strcat(template, destination->value);
    }
  }
  char *response_data = render_static_file(template);
  if (response_data == NULL) {
    perror("render_static_file");
    close(http_client->socket); // bye!
    exit(0);
  }

  const char ok[] = "HTTP/1.1 200 OK";
  const char nl[] = "\r\n\r\n";
  const size_t len = strlen(ok) + strlen(nl) + strlen(response_data);
  char *http_header = malloc(len * sizeof(char));

  strcat(http_header, ok);
  strcat(http_header, nl);
  strcat(http_header, response_data);
  free(response_data);

  if ((nbytes = send(http_client->socket, http_header, len, 0)) <= 0) {
    if (nbytes == 0) {
      // connection closed
      printf("server: socket %d hung up\n", http_client->socket);
    } else {
      perror("send");
    }
    close(http_client->socket); // bye!
    free(http_header);
    exit(0);
  }
  free(http_header);
  exit(0);
}
