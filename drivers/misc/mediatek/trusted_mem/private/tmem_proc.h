/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_PROC_H
#define TMEM_PROC_H

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
int get_multithread_test_wait_completion_time(void);
int get_saturation_stress_pmem_min_chunk_size(void);
#endif

#endif /* end of TMEM_PROC_H */
