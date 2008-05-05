#ifndef _tiger_h_
#define _tiger_h_

#include <sys/types.h>

typedef u_int64_t word64;
typedef u_int32_t word32;
typedef u_int8_t byte;

void tiger(word64 *str, word64 length, word64 res[3]);

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# if linux
#  include <endian.h>
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#   define __LITTLE_ENDIAN__ 1
#  else
#   define __BIG_ENDIAN__ 1
#  endif
# else
#  include <sys/endian.h>
#  if _BYTE_ORDER == _LITTLE_ENDIAN
#   define __LITTLE_ENDIAN__ 1
#  else
#   define __BIG_ENDIAN__ 1
#  endif
# endif
#endif

#endif

