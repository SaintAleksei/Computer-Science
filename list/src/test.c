/*
 * TODO:
 * - TESTS, TESTS, AND AGAIN TESTS
 */

#include "list.h"

#define TESTCOUNT 0x10

void test_list_logic();
void test_list_merge();
void test_list_slice();
void test_list_foreach();
void test_list_errors();

struct list_test
{
    int a;
    int b;
    int c;
};

void (*test[TESTCOUNT])() =
{
    test_logic(),
    test_errors(),
    test_verify(),
};

int main()
{
    for (size_t i = 0; test[i]; i++)
        test[i](); 
}
