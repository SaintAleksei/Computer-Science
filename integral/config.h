#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <math.h>

#define INTEGRAL_RANGE_START 0.0
#define INTEGRAL_RANGE_END 5e2
#define INTEGRAL_RANGE_SPLIT_FACTOR 100U
#define INTEGRAL_DX 1e-6
#define INTEGRAL_FUNC(x) ((x) * cos(x) * cos(x))


#define INTEGRAL_BROADCAST_PORT 23971U
#define INTEGRAL_BROADCAST_MSG "integral"
#define INTEGRAL_COMMUNICATION_PORT 23972U
#define INTEGRAL_MAX_CLIENTS_COUNT 0x100U


#define INTEGRAL_STRSIZE 0x10U

struct MsgTask
{
    char rangeStart[INTEGRAL_STRSIZE];
    char rangeEnd[INTEGRAL_STRSIZE];
};

struct MsgResult
{
    char result[INTEGRAL_STRSIZE];
};

#endif /* CONFIG_H_INCLUDED */
