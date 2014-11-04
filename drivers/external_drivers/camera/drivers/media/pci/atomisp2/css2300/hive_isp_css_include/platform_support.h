#ifndef __PLATFORM_SUPPORT_H_INCLUDED__
#define __PLATFORM_SUPPORT_H_INCLUDED__

#if defined(_MSC_VER)
/*
 * Put here everything _MSC_VER specific not covered in
 * "assert_support.h", "math_support.h", etc
 */

#elif defined(__HIVECC)
/*
 * Put here everything __HIVECC specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include "hrt/host.h"

#elif defined(__KERNEL__)
/*
 * Put here everything __KERNEL__ specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#elif defined(__FIST__)
/*
 * Put here everything __FIST__ specific not covered in
 * "assert_support.h", "math_support.h", etc
 */

#elif defined(__GNUC__)
/*
 * Put here everything __GNUC__ specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include "hrt/host.h"


#else /* default is for unknwn environments */
/*
 * Put here everything specific not covered in
 * "assert_support.h", "math_support.h", etc
 */

#endif

#endif /* __PLATFORM_SUPPORT_H_INCLUDED__ */
