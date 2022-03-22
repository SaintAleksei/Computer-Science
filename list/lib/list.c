#include "list.h"
#include <errno.h>
#include <stdlib.h>

struct node 
{
    struct node *next;
    const void *data;
};

struct list
{
    struct node *head;  
    struct node *tail;
    size_t size;
};

node_t node_create(const void *data)
{
    node_t node = calloc(1, sizeof(*node));

    if (node)
        node->data = data;

    return node;
}

void node_destroy(node_t node)
{
    free(node);
}

void *node_data(const node_t node)
{
    if (!node)
        return NULL;

    return (void *) node->data;
}

list_t list_create()
{
    return calloc(1, sizeof(struct list));
}

void list_destroy(list_t list)
{
    free(list);
}

size_t list_size(list_t list)
{
    if (!list)
        return (size_t) -1;

    return list->size;
}

node_t list_index(list_t list, size_t index)
{
    if (!list || index >= list->size)
        return (node_t) -1;

    node_t node = list->head;
    for (size_t i = 0; i < index && node; i++, node = node->next);

    return node;
}

node_t list_front(list_t list)
{
    if (!list)
        return (node_t) -1;

    return list->head;
}

node_t list_back(list_t list)
{
    if (!list)
        return (node_t) -1;

    return list->tail;
}

int list_link(list_t list, node_t node, node_t insert)
{
    if (!list || !node || !insert || node == insert || !list->size)
        return -1;

    insert->next = node->next;
    node->next = insert;

    list->size++;
    
    return 0;
}

int list_link_front(list_t list, node_t node)
{
    if (!list || !node)
        return -1;

    node->next = list->head;

    if (!node->next)
        list->tail = node;

    list->head = node;

    list->size++;

    return 0;
}

int list_link_back(list_t list, node_t node)
{
    if (!list || !node)
        return -1;

    if (list->tail)
    {
        list->tail->next = node;
        list->tail = node;
    }
    else
    {
        list->head = node;
        list->tail = node;
    }

    list->size++;
    
    return 0;
}

node_t list_unlink_front(list_t list)
{
    if (!list || !list->head)
        return (node_t) -1;

    node_t unlink = list->head;
    list->head = list->head->next;
    unlink->next = NULL;

    if (list->tail == unlink)
        list->tail = NULL;

    list->size--;

    return unlink;
}

int list_foreach(list_t list, int (*callback)(void *data, void *context), void *context)
{
    if (!list || !callback)
        return -1;
    
    for (node_t node = list->head; node; node = node->next)
    {
        int ret = callback( (void *) node->data, context);
    
        if (ret)
            return ret;
    }

    return 0;
}
