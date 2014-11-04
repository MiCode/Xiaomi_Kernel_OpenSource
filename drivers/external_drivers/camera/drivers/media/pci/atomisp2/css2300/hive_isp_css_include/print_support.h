#ifndef __PRINT_SUPPORT_H_INCLUDED__
#define __PRINT_SUPPORT_H_INCLUDED__

#if defined(_MSC_VER)

#include <stdio.h>

#elif defined(__HIVECC)
/*
 * Use OP___dump()
 */

#elif defined(__KERNEL__)
/* printk() */

#elif defined(__FIST__)

#elif defined(__GNUC__)

#include <stdio.h>

#else /* default is for unknown environments */

/* ? */

#endif

#endif /* __PRINT_SUPPORT_H_INCLUDED__ */
