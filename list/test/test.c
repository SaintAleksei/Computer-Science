#include "list.h"
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define TEST_PRINT(...)\
    {\
        fprintf(stderr, "%s:%d:%s: ", __FILE__, __LINE__, __PRETTY_FUNCTION__);\
        fprintf(stderr, __VA_ARGS__);\
    }

#define TEST_ASSERT(condition)\
    {\
        if (!(condition))\
        {\
            TEST_PRINT("TEST FAILED: %s\n", #condition);\
            return;\
        }\
    }

#define TEST_SUCCESS TEST_PRINT("TEST SUCCEED\n");

void test_list_link_unlink()
{
    list_t list;
    node_t node[3];

    TEST_ASSERT(node_data(NULL) == NULL);
    TEST_ASSERT(list_front(NULL) == (node_t) -1);
    TEST_ASSERT(list_back(NULL) == (node_t) -1);
    TEST_ASSERT(list_size(NULL) == (size_t) -1);
    TEST_ASSERT(list_link_front(NULL, (node_t) 0xBAD) == -1);
    TEST_ASSERT(list_link_front((list_t) 0xBAD, NULL) == -1);
    TEST_ASSERT(list_link_back(NULL, (node_t) 0xBAD) == -1);
    TEST_ASSERT(list_link_back((list_t) 0xBAD, NULL) == -1);
    TEST_ASSERT(list_link((list_t) 0xBAD, (node_t) 0xBAD, NULL) == -1);
    TEST_ASSERT(list_link((list_t) 0xBAD, NULL, (node_t) 0xBAD) == -1);
    TEST_ASSERT(list_link(NULL, (node_t) 0xBAD, (node_t) 0xBAD) == -1);

    TEST_ASSERT((node[0] = node_create( (void *) 0xAA)) != NULL);
    TEST_ASSERT((node[1] = node_create( (void *) 0xBB)) != NULL);
    TEST_ASSERT((node[2] = node_create( (void *) 0xCC)) != NULL);
    TEST_ASSERT(node_data(node[0]) == (void *) 0xAA);
    TEST_ASSERT(node_data(node[1]) == (void *) 0xBB);
    TEST_ASSERT(node_data(node[2]) == (void *) 0xCC);

    TEST_ASSERT((list = list_create()) != NULL);
    TEST_ASSERT(list_front(list) == NULL);
    TEST_ASSERT(list_back(list) == NULL);
    TEST_ASSERT(list_size(list) == 0);

    TEST_ASSERT(list_link_front(list, node[0]) == 0);
    TEST_ASSERT(list_front(list) == node[0]);
    TEST_ASSERT(list_back(list) == node[0]);
    TEST_ASSERT(list_size(list) == 1);

    TEST_ASSERT(list_link_front(list, node[1]) == 0);
    TEST_ASSERT(list_front(list) == node[1]);
    TEST_ASSERT(list_back(list) == node[0]);
    TEST_ASSERT(list_size(list) == 2);

    TEST_ASSERT(list_unlink_front(list) == node[1]);
    TEST_ASSERT(list_front(list) == node[0]);
    TEST_ASSERT(list_back(list) == node[0]);
    TEST_ASSERT(list_size(list) == 1);

    TEST_ASSERT(list_unlink_front(list) == node[0]);
    TEST_ASSERT(list_front(list) == NULL);
    TEST_ASSERT(list_back(list) == NULL);
    TEST_ASSERT(list_size(list) == 0);

    TEST_ASSERT(list_unlink_front(list) == (node_t) -1);
    
    TEST_ASSERT(list_link_back(list, node[0]) == 0);
    TEST_ASSERT(list_front(list) == node[0])
    TEST_ASSERT(list_back(list) == node[0])
    TEST_ASSERT(list_size(list) == 1);

    TEST_ASSERT(list_link_back(list, node[1]) == 0);
    TEST_ASSERT(list_front(list) == node[0])
    TEST_ASSERT(list_back(list) == node[1])
    TEST_ASSERT(list_size(list) == 2);

    TEST_ASSERT(list_link(list, node[0], node[2]) == 0);
    TEST_ASSERT(list_front(list) == node[0]);   
    TEST_ASSERT(list_back(list) == node[1]);
    TEST_ASSERT(list_size(list) == 3);

    TEST_ASSERT(list_link(list, node[0], node[0]) == -1);

    TEST_ASSERT(list_unlink_front(list) == node[0])
    TEST_ASSERT(list_front(list) == node[2]);
    TEST_ASSERT(list_back(list) == node[1]);
    TEST_ASSERT(list_size(list) == 2);

    TEST_ASSERT(list_unlink_front(list) == node[2]);
    TEST_ASSERT(list_front(list) == node[1]);
    TEST_ASSERT(list_back(list) == node[1]);
    TEST_ASSERT(list_size(list) == 1);

    TEST_ASSERT(list_unlink_front(list) == node[1]); 
    TEST_ASSERT(list_front(list) == NULL);
    TEST_ASSERT(list_back(list) == NULL);
    TEST_ASSERT(list_size(list) == 0);

    TEST_ASSERT(node_data(node[0]) == (void *) 0xAA);
    TEST_ASSERT(node_data(node[1]) == (void *) 0xBB);
    TEST_ASSERT(node_data(node[2]) == (void *) 0xCC);

    node_destroy(node[0]);
    node_destroy(node[1]);
    node_destroy(node[2]);
    list_destroy(list);

    TEST_SUCCESS;
}

void test_list_index()
{
    list_t list;
    node_t node[3];

    TEST_ASSERT((list = list_create()) != NULL);
    TEST_ASSERT((node[0] = node_create( (void *) 0x11)) != NULL);
    TEST_ASSERT((node[1] = node_create( (void *) 0x22)) != NULL);
    TEST_ASSERT((node[2] = node_create( (void *) 0x33)) != NULL);

    TEST_ASSERT(list_index(NULL, 1) == (node_t) -1);
    TEST_ASSERT(list_index(list, 1) == (node_t) -1);

    TEST_ASSERT(list_link_front(list, node[2]) == 0);
    TEST_ASSERT(list_link_front(list, node[1]) == 0);
    TEST_ASSERT(list_link_front(list, node[0]) == 0);

    TEST_ASSERT(list_index(list, 0) == node[0]);
    TEST_ASSERT(list_index(list, 1) == node[1]);
    TEST_ASSERT(list_index(list, 2) == node[2]);
        
    TEST_ASSERT(list_index(list, 3) == (node_t) -1);

    TEST_ASSERT(node_data(node[0]) == (void *) 0x11);
    TEST_ASSERT(node_data(node[1]) == (void *) 0x22);
    TEST_ASSERT(node_data(node[2]) == (void *) 0x33);

    node_destroy(node[0]);  
    node_destroy(node[1]);
    node_destroy(node[2]);
    list_destroy(list);

    TEST_SUCCESS;
}

static int test_list_foreach_summator(void *data, void *context)
{
    *( (int *) context) += (uint64_t) data;

    return 0;
}

void test_list_foreach()
{
    list_t list;
    node_t node[3];
    int sum = 0;

    TEST_ASSERT(list_foreach(NULL, (void *) 0xBAD, (void *) 0xBAD) == -1);
    TEST_ASSERT(list_foreach( (void *) 0xBAD, NULL, (void *) 0xBAD) == -1);

    TEST_ASSERT((list = list_create()) != NULL);
    TEST_ASSERT((node[0] = node_create( (void *) 0x11)) != NULL);
    TEST_ASSERT((node[1] = node_create( (void *) 0x22)) != NULL);
    TEST_ASSERT((node[2] = node_create( (void *) 0x33)) != NULL);

    TEST_ASSERT(list_index(NULL, 1) == (node_t) -1);
    TEST_ASSERT(list_index(list, 1) == (node_t) -1);

    TEST_ASSERT(list_link_front(list, node[2]) == 0);
    TEST_ASSERT(list_link_front(list, node[1]) == 0);
    TEST_ASSERT(list_link_front(list, node[0]) == 0);

    TEST_ASSERT(list_foreach(list, test_list_foreach_summator, &sum) == 0);

    TEST_ASSERT(sum == 0x66);

    node_destroy(node[0]);  
    node_destroy(node[1]);
    node_destroy(node[2]);
    list_destroy(list);

    TEST_SUCCESS;
}
