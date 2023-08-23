/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bs_log.h
 * @date     "Sat Oct 11 16:12:16 2014 +0800"
 * @id       "762cc9e"
 *
 * @brief
 * The head file of BOSCH SENSOR LOG
*/

#ifndef __BS_LOG_H
#define __BS_LOG_H

#include <linux/kernel.h>

/*! @ trace functions
 @{*/
/*! ERROR LOG LEVEL */
#define LOG_LEVEL_E 3
/*! NOTICE LOG LEVEL */
#define LOG_LEVEL_N 5
/*! INFORMATION LOG LEVEL */
#define LOG_LEVEL_I 6
/*! DEBUG LOG LEVEL */
#define LOG_LEVEL_D 7
/*! DEBUG_FWDL LOG LEVEL */
#define LOG_LEVEL_DF 10
/*! DEBUG_DATA LOG LEVEL */
#define LOG_LEVEL_DA 15
/*! ALL LOG LEVEL */
#define LOG_LEVEL_A 20

#ifndef MODULE_TAG
/*! MODULE TAG DEFINATION */
#define MODULE_TAG "<BS_LOG>"
#endif

#ifndef LOG_LEVEL
/*! LOG LEVEL DEFINATION */
#define LOG_LEVEL LOG_LEVEL_I
#endif

#ifdef BOSCH_DRIVER_LOG_FUNC
	#ifdef BSLOG_VAR_DEF
		uint8_t debug_log_level = LOG_LEVEL;
	#else
		extern uint8_t debug_log_level;
	#endif

	/*! print error message */
	#define PERR(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_E)\
			printk(KERN_INFO "\n" "[E]" KERN_ERR MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print notice message */
	#define PNOTICE(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_N)\
			printk(KERN_INFO "\n" "[N]" KERN_NOTICE MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print information message */
	#define PINFO(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_I)\
			printk(KERN_INFO "\n" "[I]" KERN_INFO MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug message */
	#define PDEBUG(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_D)\
			printk(KERN_INFO "\n" "[D]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug fw download message */
	#define PDEBUG_FWDL(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_DF)\
			printk(KERN_INFO "\n" "[DF]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug data log message */
	#define PDEBUG_DLOG(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_DA)\
			printk(KERN_INFO "\n" "[DA]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	void set_debug_log_level(uint8_t level);
	uint8_t get_debug_log_level(void);

#else

	#if (LOG_LEVEL >= LOG_LEVEL_E)
	/*! print error message */
	#define PERR(fmt, args...) \
		printk(KERN_INFO "\n" "[E]" KERN_ERR MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PERR(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_N)
	/*! print notice message */
	#define PNOTICE(fmt, args...) \
		printk(KERN_INFO "\n" "[N]" KERN_NOTICE MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PNOTICE(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_I)
	/*! print information message */
	#define PINFO(fmt, args...) printk(KERN_INFO "\n" "[I]" KERN_INFO MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PINFO(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_D)
	/*! print debug message */
	#define PDEBUG(fmt, args...) printk(KERN_INFO "\n" "[D]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_DF)
	/*! print debug fw download message */
	#define PDEBUG_FWDL(fmt, args...) printk(KERN_INFO "\n" "[DF]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG_FWDL(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_DA)
	/*! print debug data log message */
	#define PDEBUG_DLOG(fmt, args...) printk(KERN_INFO "\n" "[DA]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG_DLOG(fmt, args...)
	#endif

	#define set_debug_log_level(level) {}
	#define get_debug_log_level() (LOG_LEVEL)

#endif

#endif/*__BS_LOG_H*/
/*@}*/
