#pragma once

#include <lib/types.h>
#include <lib/c/string.h>   /* NULL, memcpy, memset */
#include <lib/c/stdio.h>    /* vbe_console_format */

#define LWIP_NO_STDDEF_H   1
#define LWIP_NO_STDINT_H   1
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H   1
#define LWIP_NO_UNISTD_H   1
#define LWIP_NO_CTYPE_H    1

typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uint64_t  u64_t;
typedef int64_t   s64_t;
typedef uintptr_t mem_ptr_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define LWIP_HAVE_INT64 1

/* lib/c/stdint.h makes uint32_t/size_t 'unsigned long', hence the 'l' */
#define X8_F  "x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"
#define SZT_F "lu"

#define INT_MAX   0x7fffffff
#define SSIZE_MAX INT_MAX    /* stops lwIP from typedef'ing its own ssize_t */

#define LWIP_PLATFORM_DIAG(x)   do { vbe_console_format x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { \
	vbe_console_format("lwIP assert \"%s\" at %s:%d\n", x, __FILE__, __LINE__); \
	for (;;) asm volatile("cli; hlt"); \
} while (0)

u32_t net_rand(void);
#define LWIP_RAND() net_rand()
