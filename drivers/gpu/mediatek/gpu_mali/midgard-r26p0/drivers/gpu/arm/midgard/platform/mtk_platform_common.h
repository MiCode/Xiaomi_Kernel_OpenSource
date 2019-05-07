/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MTK_PLATFORM_COMMON_H__
#define __MTK_PLATFORM_COMMON_H__

/*
* Add by mediatek, Hook the memory query function pointer to (*mtk_get_gpu_memory_usage_fp) in order to
* provide the gpu total memory usage to mlogger module
*/
extern unsigned int (*mtk_get_gpu_memory_usage_fp)(void);

/*
* Add by mediatek, Hook the gpu loading function pointer to (*mtk_get_gpu_loading_fp) in order to
* provide the gpu loading to mlogger module
*/
extern unsigned int (*mtk_get_gpu_loading_fp)(void);

/*
* Add by mediatek, Hook the gpu freq function pointer to (*mtk_get_gpu_freq_fp) in order to
* provide the gpu freq to mlogger module
*/
extern unsigned int (*mtk_get_gpu_freq_fp)(void);

void mtk_kbase_gpu_debug_init(void);

#endif /* __MTK_PLATFORM_COMMON_H__ */