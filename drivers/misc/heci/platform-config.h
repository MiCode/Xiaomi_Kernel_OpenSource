/*
 * ISS platform-specific definitions
 *
 * Copyright (c) 2012-2015, Intel Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef PLATFORM_CONFIG__H
#define PLATFORM_CONFIG__H

/* Build ID string */
#define	BUILD_ID	"0195-fix-recv-hid-hw-reset"

#define	ISH_DEBUG	0
#if ISH_DEBUG
#define	ISH_DBG_PRINT	printk
#else
#define	ISH_DBG_PRINT	no_printk
#endif

#define	ISH_INFO	1
#if ISH_INFO
#define	ISH_INFO_PRINT	printk
#else
#define	ISH_INFO_PRINT	no_printk
#endif

#define ISH_LOG		0

#if 0
/*
 * Define if running on VirtualBox -
 * may solve imprecise timer emulation problems
 */
#define	HOST_VIRTUALBOX	1
#endif

#if 0
/* Timer-polling workaround for DUTs with non-functional interrupts reporting */
#define	TIMER_POLLING	1
#endif

#define	REVISION_ID_CHT_A0	0x6
#define	REVISION_ID_CHT_A0_SI	0x0
#define	REVISION_ID_CHT_Bx_SI	0x10
#define	REVISION_ID_CHT_Kx_SI	0x20
#define	REVISION_ID_CHT_Dx_SI	0x30
#define	REVISION_ID_CHT_B0	0xB0

#define	REVISION_ID_SI_MASK	0x70

/* For buggy (pre-)silicon, select model rather than retrieve it */
#if 0
/* If defined, will support A0 only, will not check revision ID */
#define	SUPPORT_Ax_ONLY	1

#else

#if  0
/* If defined, will support B0 only, will not check revision ID */
#define	SUPPORT_B0_ONLY	1
#endif
#endif

#if defined(SUPPORT_A0_ONLY) && defined(SUPPORT_B0_ONLY)
#error Only one of SUPPORT_A0_ONLY and SUPPORT_B0_ONLY may be defined
#endif

/* D3 RCR */
#define	D3_RCR	1

/* Define in order to force FW-initated reset */
#define	FORCE_FW_INIT_RESET	1

/* Include ISH register debugger */
#define	ISH_DEBUGGER	1

/* Debug mutex locking/unlocking */
#define	DEBUG_LOCK	0

#if DEBUG_LOCK

static void	do_mutex_lock(void *m)
{
	mutex_lock(m);
}

static void	do_mutex_unlock(void *m)
{
	mutex_unlock(m);
}

#ifdef mutex_lock
#undef mutex_lock
#endif
#ifdef mutex_unlock
#undef mutex_unlock
#endif

#define mutex_lock(a) \
	do {\
		dev_warn(NULL, "%s:%d[%s] -- mutex_lock(%p)\n",	\
			__FILE__, __LINE__, __func__, a);	\
		do_mutex_lock(a);	\
	} while (0)

#define mutex_unlock(a) \
	do {\
		dev_warn(NULL, "%s:%d[%s] -- mutex_unlock(%p)\n",	\
			__FILE__, __LINE__, __func__, a);	\
		do_mutex_unlock(a);	\
	} while (0)
#endif /* DEBUG_LOCK */
/*************************************/

#endif /* PLATFORM_CONFIG__H*/

