/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MTK_PP_H__
#define __MTK_PP_H__

#include <linux/mutex.h>

#if defined(MTK_DEBUG_PROC_PRINT)

#if defined(__GNUC__)
#define MTK_PP_FORMAT_PRINTF(x, y)	__printf(x, y)
#else
#define MTK_PP_FORMAT_PRINTF(x, y)
#endif

enum MTKPP_ID {
	MTKPP_ID_FW,
	MTKPP_ID_SYNC,
	MTKPP_ID_SHOT_FW,
	MTKPP_ID_SIZE
};

extern int g_use_id;

enum MTKPP_BUFFERTYPE {
	MTKPP_QUEUEBUFFER,
	MTKPP_RINGBUFFER
};

struct MTK_PROC_PRINT_DATA {
	enum MTKPP_BUFFERTYPE type;

	char *data;
	char **line;
	int data_array_size;
	int line_array_size;
	int current_data;
	int current_line;

	spinlock_t lock;
	unsigned long irqflags;

	void (*pfn_print)(struct MTK_PROC_PRINT_DATA *data,
		const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2, 3);
};

void MTKPP_Init(void);
void MTKPP_Deinit(void);

struct MTK_PROC_PRINT_DATA *MTKPP_GetData(enum MTKPP_ID id);

#define MTKPP_LOG(id, ...) \
	do { \
		struct MTK_PROC_PRINT_DATA *mtkpp_data = MTKPP_GetData(id); \
		if (mtkpp_data != NULL) \
			mtkpp_data->pfn_print(mtkpp_data, __VA_ARGS__); \
	} while (0)

/* print log into both kerne log and gpulog for time sync */
void MTKPP_LOGTIME(enum MTKPP_ID id, const char *);

/* trigger AEE to generate a DB */
void MTKPP_TriggerAEE(int bug_on);

#else

#define MTKPP_LOG(...)
#define MTKPP_LOGTIME(...)
#define MTKPP_TriggerAEE(...)

#endif

#endif	/* __MTK_PP_H__ */

/******************************************************************************
 * End of file (mtk_pp.h)
 *****************************************************************************/

