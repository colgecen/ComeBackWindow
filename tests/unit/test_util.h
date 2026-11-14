/* Minimal test yardımcıları — ek kütüphane gerektirmez. */
#pragma once

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cbw_test_failures = 0;
static int cbw_test_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        cbw_test_checks++;                                                   \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            cbw_test_failures++;                                             \
        }                                                                    \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                   \
    do {                                                                     \
        cbw_test_checks++;                                                   \
        const char *cbw_a_ = (a);                                            \
        const char *cbw_b_ = (b);                                            \
        if (g_strcmp0(cbw_a_, cbw_b_) != 0) {                                \
            fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__,      \
                    __LINE__, cbw_a_ ? cbw_a_ : "(null)",                    \
                    cbw_b_ ? cbw_b_ : "(null)");                             \
            cbw_test_failures++;                                             \
        }                                                                    \
    } while (0)

#define CHECK_INT_EQ(a, b)                                                   \
    do {                                                                     \
        cbw_test_checks++;                                                   \
        long long cbw_a_ = (long long)(a);                                   \
        long long cbw_b_ = (long long)(b);                                   \
        if (cbw_a_ != cbw_b_) {                                              \
            fprintf(stderr, "FAIL %s:%d: %lld != %lld\n", __FILE__,          \
                    __LINE__, cbw_a_, cbw_b_);                               \
            cbw_test_failures++;                                             \
        }                                                                    \
    } while (0)

#define TEST_MAIN_END()                                                      \
    do {                                                                     \
        if (cbw_test_failures == 0) {                                        \
            printf("OK (%d kontrol)\n", cbw_test_checks);                    \
            return 0;                                                        \
        }                                                                    \
        printf("BAŞARISIZ: %d/%d kontrol\n", cbw_test_failures,              \
               cbw_test_checks);                                             \
        return 1;                                                            \
    } while (0)
