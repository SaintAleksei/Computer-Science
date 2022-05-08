#include "task.h"
#include "log.h"
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
    assert(rangeStart < rangeEnd);

    struct Task *tsk = calloc(1, sizeof(*tsk));
    if (!tsk)
    {
        LOG_ERROR("calloc(1, %lu)", sizeof(*tsk));

        return NULL;
    }

    tsk->rangeStart = rangeStart;
    tsk->rangeEnd = rangeEnd;

    LOG_WRITE("Task [%lg,%lg] is created\n",
              tsk->rangeStart, tsk->rangeEnd);

    return tsk;
}

void task_delete(struct Task *tsk)
{
    if (!tsk)
        return;

    if (tsk->next)
        tsk->next->prev = tsk->prev;

    if (tsk->prev)
        tsk->prev->next = tsk->next;

    LOG_WRITE("Task [%lg,%lg] is deleted\n",
              tsk->rangeStart, tsk->rangeEnd);

    free(tsk);
}

void task_deleteList(struct Task *tsk)
{
    while (tsk)
    {
        struct Task *toDelete = tsk;
        tsk = task_next(tsk);  
        free(toDelete);
    }
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

void task_unlink(struct Task *tsk)
{
    assert(tsk);

    if (tsk->next)
        tsk->next->prev = tsk->prev;
    if (tsk->prev)
        tsk->prev->next = tsk->next;

    tsk->next = NULL;
    tsk->prev = NULL;
}

void task_linkAfter(struct Task *tsk, struct Task *toLink)
{
    assert(tsk);
    if (!toLink)
        return;
    assert(tsk != toLink); 

    task_unlink(toLink);

    toLink->next = tsk->next;
    if (toLink->next)
        toLink->next->prev = toLink;

    tsk->next = toLink;
    toLink->prev = tsk;
}

void task_linkBefore(struct Task *tsk, struct Task *toLink)
{
    assert(tsk);
    if (!toLink)
        return; 
    assert(tsk != toLink); 

    task_unlink(toLink);

    toLink->prev = tsk->prev;
    if (toLink->prev)
        toLink->prev->next = toLink;

    tsk->prev = toLink;
    toLink->next = tsk;
}

int task_split(struct Task *tsk, size_t ntasks)
{
    assert(tsk);
    assert(ntasks);

    LOG_WRITE("Spliting task [%lg,%lg] into %lu parts\n",
              tsk->rangeStart, tsk->rangeEnd, ntasks);

    double start = tsk->rangeStart;
    double end = tsk->rangeEnd; 
    double step = (end - start) / (double) ntasks;

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

        task_linkAfter(iterator, newTsk); 
        iterator = newTsk;
    }

    LOG_WRITE("Task is splited successfully\n");

    return 0;
}

int task_writeStr(struct Task *tsk, char *str, size_t strsize)
{
    int ret = snprintf(str, strsize, "[%lg,%lg]",
                       tsk->rangeStart, tsk->rangeEnd);

    if (ret >= (int) strsize)
        LOG_WRITE("Can't write task [%lg,%lg]\n",
                  tsk->rangeStart, tsk->rangeEnd); 

    return (int) strsize - ret;
}

struct Task *task_readStr(const char *str)
{
    double start;
    double end;
    int ret = sscanf(str, "[%lg,%lg]", &start, &end);
    if (ret != 2)
    {
        LOG_WRITE("Can't write task from \"%s\"\n", str);

        return NULL;
    }

    return task_create(start, end);
}
