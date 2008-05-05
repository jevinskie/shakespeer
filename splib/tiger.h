#ifndef _tiger_h_
#define _tiger_h_

#include <sys/types.h>

typedef u_int64_t word64;
typedef u_int32_t word32;
typedef u_int8_t byte;

void tiger(word64 *str, word64 length, word64 res[3]);

#endif

