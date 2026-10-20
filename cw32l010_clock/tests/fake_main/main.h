#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
void test_wait(void);
#define __WFI() test_wait()
#endif
