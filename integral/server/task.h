#ifndef SERVER_TASK_H_INCLUDED
#define SERVER_TASK_H_INCLUDED

#include <stddef.h>

struct Task* task_create(double rangeStart, double rangeEnd);
void task_delete(struct Task *tsk);

struct Task* task_next(const struct Task *tsk);
struct Task* task_prev(const struct Task *prev);
double task_rangeStart(const struct Task *tsk);
double task_rangeEnd(const struct Task *tsk);

int task_split(struct Task *tsk, size_t ntasks);

void task_link_after(struct Task *tsk, struct Task *toLink);
void task_link_before(struct Task *tsk, struct Task *toLink);
void task_unlink(struct Task *tsk);

#endif
