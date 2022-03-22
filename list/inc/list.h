#ifndef INC_LIST
#define INC_LIST

#include <stddef.h>

typedef struct list * list_t;
typedef struct node * node_t;

node_t node_create(const void *data);

void node_destroy(node_t node);

void *node_data(const node_t node);

list_t list_create();

void list_destroy(list_t list);

size_t list_size(list_t list);

node_t list_index(const list_t list, size_t index);

node_t list_front(const list_t list);

node_t list_back(const list_t list);

int list_link(list_t list, node_t node, node_t next);

int list_link_front(list_t list, node_t next);

int list_link_back(list_t list, node_t next);

node_t list_unlink_front(list_t list);

int list_foreach(const list_t list, int (*callback)(void *data, void *context), void *context);

#endif /* INC_LIST */
