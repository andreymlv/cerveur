#include <stdlib.h>
#include <string.h>

struct route {
  char *key;
  char *value;

  struct route *left, *right;
};

struct route *init_route(char *key, char *value);

struct route *add_route(struct route *root, char *key, char *value);

struct route *search(struct route *root, char *key);

void inorder(struct route *root);
