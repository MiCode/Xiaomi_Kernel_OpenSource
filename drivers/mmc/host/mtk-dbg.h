/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DBG_H_
#define _MTK_DBG_H_

#if IS_ENABLED(CONFIG_MMC_DEBUG)

#include <linux/seq_file.h>

#define MSDC_DEBUG_REGISTER_COUNT		0x63

#define PRINTF_REGISTER_BUFFER_SIZE 512
#define ONE_REGISTER_STRING_SIZE    14

#define MSDC_REG_PRINT(OFFSET, VAL, MSG_SZ, MSG_ACCU_SZ, \
	BUF_SZ, BUF, BUF_CUR, SEQ) \
{ \
	if (SEQ) { \
		seq_printf(SEQ, "R[%x]=0x%.8x\n", OFFSET, VAL); \
		continue; \
	} \
	MSG_ACCU_SZ += MSG_SZ; \
	if (MSG_ACCU_SZ >= BUF_SZ) { \
		pr_info("%s", BUF); \
		memset(BUF, 0, BUF_SZ); \
		MSG_ACCU_SZ = MSG_SZ; \
		BUF_CUR = BUF; \
	} \
	snprintf(BUF_CUR, MSG_SZ+1, "[%.3hx:%.8x]", OFFSET, VAL); \
	BUF_CUR += MSG_SZ; \
}

#define MSDC_RST_REG_PRINT_BUF(MSG_ACCU_SZ, BUF_SZ, BUF, BUF_CUR) \
{ \
	MSG_ACCU_SZ = 0; \
	memset(BUF, 0, BUF_SZ); \
	BUF_CUR = BUF; \
}

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
struct msdc_host;
extern void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host);

#else

#define msdc_dump_info(...)

#endif

#endif  /* _MTK_DBG_H_ */
