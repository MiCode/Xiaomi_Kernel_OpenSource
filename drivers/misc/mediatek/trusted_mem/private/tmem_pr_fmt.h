/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TMEM_PR_FMT_H
#define TMEM_PR_FMT_H

#if defined(TMEM_PROFILE_FMT)
#define pr_fmt(fmt) "[TMEM][PROFILE]" fmt
#elif defined(TMEM_UT_TEST_FMT)
#define pr_fmt(fmt) "[TMEM][UT_TEST]" fmt
#else
#define pr_fmt(fmt) "[TMEM]" fmt
#endif

#endif /* end of TMEM_PR_FMT_H */
