#ifndef __STDBOOL_H_INCLUDED__
#define __STDBOOL_H_INCLUDED__

#ifndef __cplusplus

#if defined(_MSC_VER)
typedef unsigned int bool;

#define true	1
#define false	0

#elif defined(__GNUC__)
#ifndef __KERNEL__
/* Linux kernel driver defines stdbool types in types.h */
typedef unsigned int	bool;

#define true	1
#define false	0

/*
 *Alternatively
 * 
typedef enum {
	false = 0,
	true
} bool;
 */
#endif
#endif

#endif /* __cplusplus */

#endif /* __STDBOOL_H_INCLUDED__ */
