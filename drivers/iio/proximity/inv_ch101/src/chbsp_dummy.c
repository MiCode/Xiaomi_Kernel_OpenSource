// SPDX-License-Identifier: GPL-2.0
/*!
 * \file chbsp_dummy.c
 *
 * \brief Dummy implementations of optional board support package IO functions
 * allowing platforms to selectively support only needed functionality.
 * These are placeholder routines that will satisfy references from other code
 * to avoid link errors, but they do not peform any actual operations.
 *
 * See chirp_bsp.h for descriptions of all board support package interfaces,
 * including details on these optional functions.
 */

/*
 * Copyright (c) 2017-2019 Chirp Microsystems.  All rights reserved.
 */

#include "chirp_bsp.h"

/* Functions supporting debugging */

WEAK void chbsp_debug_toggle(u8 dbg_pin_num)
{
}

WEAK void chbsp_debug_on(u8 dbg_pin_num)
{
}

WEAK void chbsp_debug_off(u8 dbg_pin_num)
{
}


WEAK void chbsp_external_i2c_irq_handler(struct chdrv_i2c_transaction_t *trans)
{
	(void)(trans);
}

/* Other BSP functions */

WEAK void chbsp_proc_sleep(u16 ms)
{
	msleep(ms);
}

WEAK void chbsp_critical_section_enter(void)
{
}

WEAK void chbsp_critical_section_leave(void)
{
}
