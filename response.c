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
  size_t len = 0;
  while ((n = read(fd, buf, sizeof buf)) > 0) {
    str = realloc(str, len + n + 1);
    memcpy(str + len, buf, n);
    len += n;
    str[len] = '\0';
  }
  return str;
}
