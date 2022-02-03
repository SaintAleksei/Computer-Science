#include "list.h"
#include <stdlib.h>
#include <assert.h>

struct node
{
    const void *data;
    struct node *next;
};

struct list
{
    struct node *ghost;
    struct node *tail;
    size_t size;
};

int list_errno;

struct list* 
list_create()
{
    struct list *list = calloc(1, sizeof(*list));
    list->ghost = calloc(1, sizeof(*list->ghost));

    if (!list || !list->ghost)
    {
        list_errno = LIST_ERRMEM;
        return NULL;
    }

    list->tail = list->ghost;

    return list;
}

void 
list_destroy(struct list *list)
{
    if (!list)
        return;

    while (list->ghost)
    {
        struct node *node = list->ghost->next;
        free(list->ghost);
        list->ghost = node;
    }

    free(list);
}

struct node*
list_insert(struct list *list,
            struct node *node,
            const void *data)
{
    if (!list || !list->ghost || !node)
    {
        list_errno = LIST_ERRARGS;
        return NULL;
    }

    sruct node *new = calloc(1, sizeof(*new));

    if (!new)
    {
        list_errno = LIST_ERRMEM;
        return NULL;
    }

    new->data = data;
    new->next = node->next;
    node->next = new;

    if (node == list->tail)
        tail = new;
    
    list->size++;

    return new;
}

int 
list_foreach(struct list *list,
             int (*callback)(void *data, void *context),
             void *context)
{
    if (!list || !list->ghost || !callback || !context)
    {
        list_errno = LIST_ERRARGS;
        return EXIT_FAILURE;
    }

    struct node *node = list->ghost->next;
    for (size_t i = 0; i < list->size && node; i++)
    {
        int ret = callback( (void *) node->data, context);

        if (ret != EXIT_SUCCESS)
        {
            list_errno = LIST_ERRFRCH;
            return ret;
        }
    }

    return EXIT_SUCCESS;
}

struct list*
list_merge(struct list *left, 
           struct list *right)
{
    if (!left || !left->ghost || !right || !right->ghost)
    {
        list_errno = LIST_ERRARGS;
        return NULL;
    }

    left->tail->next = right->ghost->next;

    free(right->ghost);
    free(right);

    return left;
}

struct list*
list_slice(const struct list *list,
           size_t begin,
           size_t size,
           int step);
{
    if (!list || begin > end)
    {
        list_errno = LIST_ERRARGS;
        return NULL;
    }

    struct list *new = list_create();

    if (!new)
        return NULL;

    size_t j = begin;
    struct node *node = list->ghost->next;
    for (size_t i = 0; i < list->size && node; i++)
    {
        if (i == j)
        {
            new->tail->next = calloc(1, sizeof(*new->tail->next));

            if (!new->tail->next)
            {
                list_destroy(new);
                list_errno = LIST_ERRMEM;
                return NULL;
            }

            new->tail->next->data = node->data;
            new->size++;
        
            j += step;
        }

        if (new->size == size)
            break;

        node = node->next;
    }

    return new;
}

struct list *list_copy(const struct list *list)
{
    return list_slice(list, 0, list->size, 1);
}

struct list *list_insert_head(struct list *list,
                              void *data)
{
    return list_insert(list, list->ghost, data);
}

struct list *list_insert_tail(struct list *list,
                              void *data)
{
    return list_insert(list, list->tail, data);
}

struct node *list_head(const struct list *list)
{
    if (!list || !list->ghost)
    {
        list_errno = LIST_ERRARGS; 
        return NULL;
    }
    else
        return list->ghost->next;
}

struct node *list_tail(const struct list *list)
{
    if (!list || !list->tail)
    {
        list_errno = LIST_ERRARGS;
        return NULL;
    }
    else
        return list->tail;
}

size_t list_size(const struct list *list)
{
    return list->size;
}

const void *node_data(const struct node *node)
{
    if (!node)
    {
        list_errno = LIST_ERRARGS;
        return NULL;
    }
    else
        return node->data;
}

const char *list_strerror(int errcode)
{
    switch (list_errno)
    {
        case LIST_ERRARGS:
            return "Bad arguments"; 
        case LIST_ERRMEM:
            return "Memory allocation error";
        case LIST_ERRNO:
            return "Success";
        default:
            return "Undefined error";
    }
}

int list_verify(const struct list *list)
{
    if (!list || !list->ghost)
    {
        list_errno = LIST_ERRARGS;
        return EXIT_FAILURE;
    }

    struct node *node = list->ghost->next;
    for (size_t i = 0; i < list->size; i++)
    {
        if (!node)
        {
            list_errno = LIST_ERRVRFY;
            return EXIT_FAILURE;
        } 

        node = node->next;
    }

    if (node->next || node != list->tail)
    {
        list_errno = LIST_ERRVRFY;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
