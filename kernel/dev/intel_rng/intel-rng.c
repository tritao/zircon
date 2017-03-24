#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

// TODO(mcgrathr): As of GCC 6.2.0, these other files included by
// <x86intrin.h> are incompatible with -mno-sse.
#ifndef __clang__
#define _IMMINTRIN_H_INCLUDED
#define _MM3DNOW_H_INCLUDED
#define _FMA4INTRIN_H_INCLUDED
#define _XOPMMINTRIN_H_INCLUDED
#endif
#include <x86intrin.h>

#include <arch/x86/feature.h>
#include <dev/hw_rng.h>

/* @brief Get entropy from the CPU using RDSEED.
 *
 * len must be at most SSIZE_MAX
 *
 * If |block|=true, it will retry the RDSEED instruction until |len| bytes are
 * written to |buf|.  Otherwise, it will fetch data from RDSEED until either
 * |len| bytes are written to |buf| or RDSEED is unable to return entropy.
 *
 * Returns the number of bytes written to the buffer on success (potentially 0),
 * and a negative value on error.
 */
static ssize_t get_entropy_from_cpu(void* buf, size_t len, bool block) {
    /* TODO(security): Move this to a shared kernel/user lib, so we can write usermode
     * tests against this code */

    if (len >= SSIZE_MAX) {
        static_assert(ERR_INVALID_ARGS < 0, "");
        return ERR_INVALID_ARGS;
    }

    if (!x86_feature_test(X86_FEATURE_RDSEED)) {
        /* We don't have an entropy source */
        static_assert(ERR_NOT_SUPPORTED < 0, "");
        return ERR_NOT_SUPPORTED;
    }

    size_t written = 0;
    while (written < len) {
        unsigned long long int val = 0;
        if (!_rdseed64_step(&val)) {
            if (!block) {
                break;
            }
            continue;
        }
        const size_t to_copy = (len < sizeof(val)) ? len : sizeof(val);
        memcpy(buf + written, &val, to_copy);
        written += to_copy;
    }
    if (block) {
        DEBUG_ASSERT(written == len);
    }
    return (ssize_t)written;
}

size_t hw_rng_get_entropy(void* buf, size_t len, bool block)
{
    if (!len) {
        return 0;
    }

    ssize_t res = get_entropy_from_cpu(buf, len, block);
    if (res < 0) {
        return 0;
    }
    return (size_t)res;
}
