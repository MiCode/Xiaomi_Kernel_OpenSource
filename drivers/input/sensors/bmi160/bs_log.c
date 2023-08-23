/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bs_log.c
 * @date     "Wed Sep 24 15:27:12 2014 +0800"
 * @id       "e416c14"
 *
 * @brief
 * The source file of BOSCH SENSOR LOG
*/


#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#include <linux/time.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#ifdef BOSCH_DRIVER_LOG_FUNC
#define BSLOG_VAR_DEF
#include "bs_log.h"

void set_debug_log_level(uint8_t level)
{
	debug_log_level = level;
}

uint8_t get_debug_log_level(void)
{
	return debug_log_level;
}

EXPORT_SYMBOL(set_debug_log_level);
EXPORT_SYMBOL(get_debug_log_level);

#endif/*BOSCH_DRIVER_LOG_FUNC*/
/*@}*/
