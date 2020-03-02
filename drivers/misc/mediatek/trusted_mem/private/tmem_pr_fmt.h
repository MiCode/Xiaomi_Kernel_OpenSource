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

#ifndef LOG_TAG
#define LOG_TAG "[TMEM]"
#endif

#if defined(TMEM_PROFILE_FMT)
#define LOG_PROFILE "[PROFILE]"
#define pr_fmt(fmt) LOG_TAG LOG_PROFILE fmt
#elif defined(TMEM_MEMMGR_FMT)
#define LOG_MEMMGR "[MEMMGR]"
#define pr_fmt(fmt) LOG_TAG LOG_MEMMGR fmt
#elif defined(TMEM_UT_TEST_FMT)
#define LOG_UT_TEST "[UT_TEST]"
#define pr_fmt(fmt) LOG_TAG LOG_UT_TEST fmt
#elif defined(TMEM_MOCK_FMT)
#define LOG_MOCK "[MOCK]"
#define pr_fmt(fmt) LOG_TAG LOG_MOCK fmt
#else
#define pr_fmt(fmt) LOG_TAG fmt
#endif

#endif /* end of TMEM_PR_FMT_H */
