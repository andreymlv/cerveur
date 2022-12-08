#include "routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct route *init_route(char *key, char *value) {
  struct route *temp = (struct route *)malloc(sizeof(struct route));

  temp->key = key;
  temp->value = value;

  temp->left = temp->right = NULL;
  return temp;
}

void inorder(struct route *root) {

  if (root != NULL) {
    inorder(root->left);
    printf("%s -> %s \n", root->key, root->value);
    inorder(root->right);
  }
}

struct route *add_route(struct route *root, char *key, char *value) {
  if (root == NULL) {
    return init_route(key, value);
  }

  if (strcmp(key, root->key) == 0) {
    printf("============ WARNING ============\n");
    printf("A Route For \"%s\" Already Exists\n", key);
  } else if (strcmp(key, root->key) > 0) {
    root->right = add_route(root->right, key, value);
  } else {
    root->left = add_route(root->left, key, value);
  }

  return root;
}

struct route *search(struct route *root, char *key) {
  if (root == NULL) {
    return NULL;
  }

  if (strcmp(key, root->key) == 0) {
    return root;
  } else if (strcmp(key, root->key) > 0) {
    return search(root->right, key);
  } else if (strcmp(key, root->key) < 0) {
    return search(root->left, key);
  }

  return root;
}
