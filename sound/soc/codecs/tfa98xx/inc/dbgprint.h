/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DBGPRINT_H
#   define _DBGPRINT_H

/* Debugging macro's. */
#   ifndef DEBUG
#      define DEBUG
#   endif

#   ifndef ASSERT

#   endif

#   ifndef _ASSERT
		#define _ASSERT(e)
#   endif

#   ifndef PREFIX
#     define PREFIX "tfa98xx: "
#	define DRIVER_NAME "tfa98xx"
#   endif

#ifdef __KERNEL__

#   ifdef DEBUG
#      define _DEBUG(level, fmt, va...) do {\
				if (unlikely(debug >= (level))) \
					    printk(KERN_INFO PREFIX "%s:%d: "fmt, __func__, __LINE__, ##va); \
		} while (0)

#   else
#      define _DEBUG(level, fmt, va...) do {} while(0)
#   endif

#   define MSG(fmt, va...) printk(KERN_INFO PREFIX "%s:%d: "fmt, __func__, __LINE__, ##va)
#   define _ERRORMSG(fmt, va...) printk(KERN_ERR PREFIX "ERROR %s:%d: "fmt, __func__, __LINE__, ##va)


#   define DEBUG0(x...) MSG(x)
#   define DEBUG1(x...) _DEBUG(1, x)
#   define DEBUG2(x...) _DEBUG(2, x)
#   define DEBUG3(x...) _DEBUG(3, x)
#   define ERRORMSG(x...) _ERRORMSG(x)
#	define PRINT(x...)	printk(x)
#   define PRINT_ERROR(x...) printk(KERN_INFO PREFIX " **ERROR** " x)
#   define PRINT_ASSERT(e)if ((e)) printk(KERN_ERR "PrintAssert:%s (%s:%d) error code:%d\n", __FUNCTION__, __FILE__, __LINE__, e)

#   define PRINT_ENTRY DEBUG2("+[%s]\n", __func__)
#   define PRINT_EXIT  DEBUG2("-[%s]\n", __func__)

#   ifdef ASSERT
#      define assert(cond, action) do { if (unlikely(!(cond))) { DEBUG0("Assert: %s\n", #cond); action; }} while(0)
#   else
#      define assert(cond, action) do { } while (0)
#   endif

#else /* __KERNEL__ */
#if defined(WIN32) || defined(_X64)
#include <stdio.h>
/* user mode */
#   ifdef DEBUG
#      define _DEBUGMSG(level, fmt, ...)  printf(PREFIX "%s:%d: "fmt, __FUNCTION__, __LINE__, __VA_ARGS__);
#   else
#      define _DEBUGMSG(level, fmt, ...) do {} while(0)
#   endif

#   define _ERRORMSG(fmt, ...) printf(PREFIX "%s:%s:%d: "fmt, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#   define DEBUG0(...) MSG(__VA_ARGS__)
#   define DEBUG1(...) _DEBUGMSG(1, __VA_ARGS__)
#   define DEBUG2(...) _DEBUGMSG(2, __VA_ARGS__)
#   define DEBUG3(...) _DEBUGMSG(3, __VA_ARGS__)
#   define ERRORMSG(fmt, ...) _ERRORMSG(fmt, __VA_ARGS__)
#	define PRINT(...)	printf(__VA_ARGS__)
/*
#	define PRINT(...) {	FILE *stream;														\
							if((stream = freopen("nxp_tfa.txt", "ab+", stdout)) == NULL) exit(-1);	\
							printf(__VA_ARGS__);												\
							freopen( "CON", "ab+", stdout );										\
						}
*/
#	define PRINT_ERROR(...)	 fprintf(stderr, __VA_ARGS__)
#	define PRINT_FILE(file, ...)	fprintf(file, __VA_ARGS__)
#	define PRINT_ASSERT(e)if ((e)) fprintf(stderr, "PrintAssert:%s (%s:%d) error code:%d\n", __FUNCTION__, __FILE__, __LINE__, e)


#elif defined(__CODE_RED)
#include "app_global.h"
#   ifdef DEBUG
#      define _DEBUG(level, fmt, va...) TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, va))

#   else
#      define _DEBUG(level, fmt, va...) do {} while(0)
#   endif

#   define MSG(fmt, ...) TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, __VA_ARGS__))








#   define ERRORMSG(...) TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, __VA_ARGS__))


#   define DEBUG0(x...) MSG(x)
#   define DEBUG1(x...) _DEBUG(1, x)
#   define DEBUG2(x...) _DEBUG(2, x)
#   define DEBUG3(x...) _DEBUG(3, x)

#	define PRINT(x...)	TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, x))

#	define PRINT_ERROR(x...) TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, x))

#	define PRINT_FILE(file, x...) TB_TRACE_INF(TbTracePfx2("tfa",TB_FUNC, x))

#	define PRINT_ASSERT(e)


#else
#include <stdio.h>
/* user mode */
#   ifdef DEBUG
#      define _DEBUG(level, fmt, va...)  printf(PREFIX "%s:%d: "fmt, __func__, __LINE__, ##va);
#   else
#      define _DEBUG(level, fmt, va...) do {} while(0)
#   endif

#   define MSG(fmt, va...) printf(PREFIX "%s:%s:%d: "fmt, __FILE__, __func__, __LINE__, ##va)
#   define _ERRORMSG(fmt, va...) fprintf(stderr, PREFIX "ERROR %s:%s:%d: "fmt, __FILE__, __func__, __LINE__, ##va)

#   define DEBUG0(x...) MSG(x)
#   define DEBUG1(x...) _DEBUG(1, x)
#   define DEBUG2(x...) _DEBUG(2, x)
#   define DEBUG3(x...) _DEBUG(3, x)
#   define ERRORMSG(x...) _ERRORMSG(x)
#	define PRINT(x...)	printf(x)
#	define PRINT_ERROR(...)	 fprintf(stderr, __VA_ARGS__)
#	define PRINT_FILE(file, ...)	fprintf(file, __VA_ARGS__)
#	define PRINT_ASSERT(e)if ((e)) fprintf(stderr, "PrintAssert:%s (%s:%d) error code:%d\n", __FUNCTION__, __FILE__, __LINE__, e)



#endif	 /* WIN32 */

#endif	 /* user */

#endif				/* _DBGPRINT_H --------------- */
