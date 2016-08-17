/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * Copyright (C) 2011-2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __S_VERSION_H__
#define __S_VERSION_H__

#include "linux/stringify.h"

/*
 * Usage: define S_VERSION_BUILD on the compiler's command line.
 *
 * Then set:
 * - S_VERSION_OS
 * - S_VERSION_PLATFORM
 * - S_VERSION_MAIN
 * - S_VERSION_ENG is optional
 * - S_VERSION_PATCH is optional
 * - S_VERSION_BUILD = 0 if S_VERSION_BUILD not defined or empty
 */



/*
 * This version number must be updated for each new release.
 *
 * If this is a patch or engineering version use the following
 * defines to set the version number. Else set these values to 0.
 */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define S_VERSION_OS "A"          /* "A" for all Android */
#define S_VERSION_PLATFORM "A"
#define S_VERSION_MAIN  "01.11"
#define S_VERSION_ENG 0
#define S_VERSION_PATCH 0

#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define S_VERSION_OS "A"          /* "A" for all Android */
#define S_VERSION_PLATFORM "B"
#define S_VERSION_MAIN  "02.02"
#define S_VERSION_ENG 0
#define S_VERSION_PATCH 0

#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define S_VERSION_OS "A"          /* "A" for all Android */
#define S_VERSION_PLATFORM "C"
#define S_VERSION_MAIN  "02.08"
#define S_VERSION_ENG 0
#define S_VERSION_PATCH 0

#else
#define S_VERSION_OS "Z"          /* Unknown platform */
#define S_VERSION_PLATFORM "Z"    /* Unknown platform */
#define S_VERSION_MAIN  "00.00"
#define S_VERSION_ENG 0
#define S_VERSION_PATCH 0
#endif


#ifdef S_VERSION_BUILD
/* TRICK: detect if S_VERSION is defined but empty */
#if 0 == S_VERSION_BUILD-0
#undef  S_VERSION_BUILD
#define S_VERSION_BUILD 0
#endif
#else
/* S_VERSION_BUILD is not defined */
#define S_VERSION_BUILD 0
#endif


#if S_VERSION_ENG != 0
#define _S_VERSION_ENG "e" __stringify(S_VERSION_ENG)
#else
#define _S_VERSION_ENG ""
#endif

#if S_VERSION_PATCH != 0
#define _S_VERSION_PATCH "p" __stringify(S_VERSION_PATCH)
#else
#define _S_VERSION_PATCH ""
#endif

#if !defined(NDEBUG) || defined(_DEBUG)
#define S_VERSION_VARIANT "D   "
#else
#define S_VERSION_VARIANT "    "
#endif

#define S_VERSION_STRING (\
	"TFN" \
	S_VERSION_OS \
	S_VERSION_PLATFORM \
	S_VERSION_MAIN \
	_S_VERSION_ENG \
	_S_VERSION_PATCH \
	"."  __stringify(S_VERSION_BUILD) " " \
	S_VERSION_VARIANT)

#endif /* __S_VERSION_H__ */
