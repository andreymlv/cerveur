#include "response.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

char *render_static_file(char *path) {
  char buf[4096];
  ssize_t n;
  char *str = NULL;
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    perror("open");
    return str;
  }
  size_t len = 0;
  while ((n = read(fd, buf, sizeof buf)) > 0) {
    if ((str = realloc(str, len + n + 1)) == NULL) {
      perror("realloc");
      break;
    }
    memcpy(str + len, buf, n);
    len += n;
    str[len] = '\0';
  }
  return str;
}
