/*******************************************************************************
 * Copyright © 2015, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/


#ifndef _VL53L0_PLATFORM_LOG_H_
#define _VL53L0_PLATFORM_LOG_H_

#include <linux/string.h>
/* LOG Functions */


/**
 * @file vl53l0_platform_log.h
 *
 * @brief platform log function definition
 */

/* #define VL53L0_LOG_ENABLE */

enum {
	TRACE_LEVEL_NONE,
	TRACE_LEVEL_ERRORS,
	TRACE_LEVEL_WARNING,
	TRACE_LEVEL_INFO,
	TRACE_LEVEL_DEBUG,
	TRACE_LEVEL_ALL,
	TRACE_LEVEL_IGNORE
};

enum {
	TRACE_FUNCTION_NONE = 0,
	TRACE_FUNCTION_I2C  = 1,
	TRACE_FUNCTION_ALL  = 0x7fffffff /* all bits except sign */
};

enum {
	TRACE_MODULE_NONE              = 0x0,
	TRACE_MODULE_API               = 0x1,
	TRACE_MODULE_PLATFORM          = 0x2,
	TRACE_MODULE_ALL               = 0x7fffffff /* all bits except sign */
};


#ifdef VL53L0_LOG_ENABLE

#include <linux/module.h>


extern uint32_t _trace_level;



int32_t VL53L0_trace_config(char *filename, uint32_t modules,
			uint32_t level, uint32_t functions);

#if 0
void trace_print_module_function(uint32_t module, uint32_t level,
			uint32_t function, const char *format, ...);
#else
#define trace_print_module_function(...)
#endif

#define LOG_GET_TIME() (int)0
/*
 * #define _LOG_FUNCTION_START(module, fmt, ...) \
		printk(KERN_INFO"beg %s start @%d\t" fmt "\n", \
		__func__, LOG_GET_TIME(), ##__VA_ARGS__)

 * #define _LOG_FUNCTION_END(module, status, ...)\
		printk(KERN_INFO"end %s @%d %d\n", \
		 __func__, LOG_GET_TIME(), (int)status)

 * #define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
		printk(KERN_INFO"End %s @%d %d\t"fmt"\n" , \
		__func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)
*/
#define _LOG_FUNCTION_START(module, fmt, ...) \
		pr_err("beg %s start @%d\t" fmt "\n", \
		__func__, LOG_GET_TIME(), ##__VA_ARGS__)

#define _LOG_FUNCTION_END(module, status, ...)\
		pr_err("end %s start @%d Status %d\n", \
		 __func__, LOG_GET_TIME(), (int)status)

#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
		pr_err("End %s @%d %d\t"fmt"\n", \
		__func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)


#else /* VL53L0_LOG_ENABLE no logging */
	#define VL53L0_ErrLog(...) (void)0
	#define _LOG_FUNCTION_START(module, fmt, ...) (void)0
	#define _LOG_FUNCTION_END(module, status, ...) (void)0
	#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...) (void)0
#endif /* else */

#define VL53L0_COPYSTRING(str, ...) strlcpy(str, ##__VA_ARGS__, sizeof(str))


#endif  /* _VL53L0_PLATFORM_LOG_H_ */



