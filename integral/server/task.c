#include "task.h"
#include <stdlib.h>
#include <assert.h>

struct Task
{
    struct Task *next;
    struct Task *prev;
    double rangeStart;
    double rangeEnd;
};

struct Task *task_create(double rangeStart, double rangeEnd)
{
    struct Task *tsk = calloc(1, sizeof(*tsk));
    if (!tsk)
        return NULL;

    tsk->rangeStart = rangeStart;
    tsk->rangeEnd = rangeEnd;

    return tsk;
}

void task_delete(struct Task *tsk)
{
    assert(tsk);

    if (tsk->next)
        tsk->next->prev = tsk->prev;

    if (tsk->prev)
        tsk->prev->next = tsk->next;

    free(tsk);
}

struct Task *task_next(const struct Task *tsk)
{
    assert(tsk);

    return tsk->next;
} 

struct Task *task_prev(const struct Task *tsk)
{
    assert(tsk);

    return tsk->prev;
}

double task_rangeStart(const struct Task *tsk)
{
    assert(tsk);

    return tsk->rangeStart;
}

double task_rangeEnd(const struct Task *tsk)
{
    assert(tsk);

    return tsk->rangeEnd;
}

int task_unlink(struct Task *tsk)
{
    assert(tsk);

    if (tsk->next)
        tsk->next->prev = tsk->prev;
    if (tsk->prev)
        tsk->prev->next = tsk->next;

    tsk->next = NULL;
    tsk->prev = NULL;

    return 0;
}

int task_link(struct Task *left, struct Task *right)
{
    assert(left);
    assert(right);
    assert(left != right); 

    right->next = left->next;
    if (right->next)
        right->next->prev = right;

    left->next = right;
    right->prev = left;

    return 0;
}

int task_split(struct Task *tsk, size_t ntasks)
{
    assert(tsk);

    double start = tsk->rangeStart;
    double end = tsk->rangeEnd; 
    double step = (start - end) / (double) ntasks;

    tsk->rangeEnd = tsk->rangeStart + step; 

    struct Task *iterator = tsk;
    for (size_t i = 1; i < ntasks; i++)
    {
        struct Task *newTsk = task_create(iterator->rangeEnd,
                                          iterator->rangeEnd + step);
        if (!newTsk)
        {
            iterator->rangeEnd = end;
            return -1;
        } 

        task_link(iterator, newTsk); 
        iterator = newTsk;
    }

    return 0;
}
