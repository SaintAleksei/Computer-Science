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

struct node *list_insert(struct list *list,
                         struct node *node,
                         const void *data);

struct node *list_insert_head(struct list *list,
                              const void *data);

struct node *list_insert_tail(struct list *list,
                              const void *data);

size_t list_size(const struct list *list);

struct node *list_head(const struct list *list);

struct node *list_tail(const struct list *list);

const char *list_strerror(int errcode);

const void *node_data(const struct node *node);

typedef struct list * list_t;
typedef struct node * node_t;

#endif /* SRC_LIST_H */
