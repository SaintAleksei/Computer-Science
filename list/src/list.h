/*
 * TODO:
 * - Some implementations in list.c
 * - Tests
 * - Makefile for compiling a library 
 * - Description of the interface 
 */

#ifndef SRC_LIST_H
#define SRC_LIST_H

#include <stddef.h>

#define LIST_ERRNO 0
#define LIST_ERRARGS 1
#define LIST_ERRMEM 2
#define LIST_ERRFRCH 3
#define LIST_ERRVRFY 4

extern int list_errno;

struct list *list_create();

void list_destroy(struct list *list);

int list_verify(const struct list *list);

int list_foreach(struct list *list,
                 int (*callback)(void *data, void *context),
                 void *context);

struct list *list_merge(struct list *left,
                        struct list *right);

struct list *list_slice(const struct list *list,
                        size_t begin,
                        size_t size,
                        int step);

struct list *list_copy(const struct list *list);

struct node *list_find_node(const struct list *list,
                            size_t index);

size_t list_find_index(const struct list *list,
                       const struct node *node);

struct node *list_insert(struct list *list,
                         struct node *node,
                         const void *data);

struct node *list_insert_head(struct list *list,
                              const void *data);

struct node *list_insert_tail(struct list *list,
                              const void *data);

int list_delete(struct list *list);

int list_delete_head(struct list *list);

int list_delete_tail(struct list *list);

size_t list_size(const struct list *list);

struct node *list_head(const struct list *list);

struct node *list_tail(const struct list *list);

const char *list_strerror(int errcode);

const void *list_node_data(const struct node *node);

typedef struct list * list_t;
typedef struct node * node_t;

#endif /* SRC_LIST_H */
