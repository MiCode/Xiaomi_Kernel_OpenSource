/*
 *  vl53l0x_platform_log.h - Linux kernel modules for
 *	STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */




#ifndef _VL_PLATFORM_LOG_H_
#define _VL_PLATFORM_LOG_H_

#include <linux/string.h>
/* LOG Functions */


/**
 * @file vl53l0x_platform_log.h
 *
 * @brief platform log function definition
 */

/* #define VL_LOG_ENABLE */

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


#ifdef VL_LOG_ENABLE

#include <linux/module.h>


extern uint32_t _trace_level;



int32_t VL_trace_config(char *filename, uint32_t modules,
			uint32_t level, uint32_t functions);

#define trace_print_module_function(...)

#define LOG_GET_TIME() (int)0
/*
#define _LOG_FUNCTION_START(module, fmt, ...) \
		dbg(KERN_INFO"beg %s start @%d\t" fmt "\n", \
		__func__, LOG_GET_TIME(), ##__VA_ARGS__)

#define _LOG_FUNCTION_END(module, status, ...)\
		dbg(KERN_INFO"end %s @%d %d\n", \
		 __func__, LOG_GET_TIME(), (int)status)

#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
		dbg(KERN_INFO"End %s @%d %d\t"fmt"\n" , \
		__func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)
*/
#define _LOG_FUNCTION_START(module, fmt, ...) \
		dbg("beg %s start @%d\t" fmt "\n", \
		__func__, LOG_GET_TIME(), ##__VA_ARGS__)

#define _LOG_FUNCTION_END(module, status, ...)\
		dbg("end %s start @%d Status %d\n", \
		 __func__, LOG_GET_TIME(), (int)status)

#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
		dbg("End %s @%d %d\t"fmt"\n" , \
		__func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)


#else /* VL_LOG_ENABLE no logging */
	#define VL_ErrLog(...) (void)0
	#define _LOG_FUNCTION_START(module, fmt, ...) (void)0
	#define _LOG_FUNCTION_END(module, status, ...) (void)0
	#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...) (void)0
#endif /* else */

#define VL_COPYSTRING(str, ...) strlcpy(str, ##__VA_ARGS__, sizeof(str))


#endif  /* _VL_PLATFORM_LOG_H_ */



