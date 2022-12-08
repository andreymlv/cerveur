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

#include "response.h"
#include "routes.h"

#define PORT "3490" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

void sigchld_handler(int s) {
  (void)s; // quiet unused variable warning

  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void) {
  int server_socket,
      client_socket; // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;
  char buf[4096]; // buffer for client data
  int nbytes;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((server_socket =
             socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
      close(server_socket);
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

  if (listen(server_socket, BACKLOG) == -1) {
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

  // registering Routes
  struct route *route = init_route("/", "index.html");
  route = add_route(route, "/about", "about.html");
  printf("all available routes:\n");
  inorder(route);
  printf("server: waiting for connections...\n");
  while (1) { // main accept() loop
    sin_size = sizeof their_addr;
    client_socket =
        accept(server_socket, (struct sockaddr *)&their_addr, &sin_size);
    if (client_socket == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) {          // this is the child process
      close(server_socket); // child doesn't need the listener
      if ((nbytes = recv(client_socket, buf, sizeof buf, 0)) <= 0) {
        // got error or connection closed by client
        if (nbytes == 0) {
          // connection closed
          printf("server: socket %d hung up\n", client_socket);
        } else {
          perror("recv");
        }
        close(client_socket); // bye!
        exit(0);
      } else {
        // we got some data from a client
        printf("client send: %s\n", buf);
        // parsing client socket header to get HTTP method, route
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
          // strcat(template, urlRoute+1);
          strcat(template, "static/index.css");
        } else {
          struct route *destination = search(route, urlRoute);
          strcat(template, "templates/");

          if (destination == NULL) {
            strcat(template, "404.html");
          } else {
            strcat(template, destination->value);
          }
        }
        char *response_data = render_static_file(template);

        const char ok[] = "HTTP/1.1 200 OK";
        const char nl[] = "\r\n\r\n";
        const size_t len = (strlen(ok) + strlen(nl) + strlen(response_data));
        char *http_header = malloc(len * sizeof(char));

        strcat(http_header, ok);
        strcat(http_header, nl);
        strcat(http_header, response_data);
        free(response_data);

        if ((nbytes = send(client_socket, http_header, len, 0)) <= 0) {
          if (nbytes == 0) {
            // connection closed
            printf("server: socket %d hung up\n", client_socket);
          } else {
            perror("send");
          }
          close(client_socket); // bye!
          free(http_header);
          exit(0);
        }
        free(http_header);
        exit(0);
      }
      close(client_socket);
    }
    close(client_socket); // parent doesn't need this
  }

  return 0;
}
