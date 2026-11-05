/* 极简测试框架：CHECK 失败时打印位置并计数，main 返回失败数 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <math.h>

extern int g_test_failures;
extern int g_test_checks;

#define CHECK(cond, ...) do {                                              \
        g_test_checks++;                                                   \
        if (!(cond)) {                                                     \
            g_test_failures++;                                             \
            printf("  FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);     \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, tol, what) \
    CHECK(fabs((double)(a) - (double)(b)) <= (tol), "%s: got %.3f, want %.3f +/- %.3f", \
          what, (double)(a), (double)(b), (double)(tol))

#define TEST_MAIN_DEFS int g_test_failures = 0; int g_test_checks = 0;

#define TEST_REPORT(name) do {                                             \
        printf("%s: %d checks, %d failed\n", name, g_test_checks, g_test_failures); \
        return g_test_failures ? 1 : 0;                                    \
    } while (0)

#endif
