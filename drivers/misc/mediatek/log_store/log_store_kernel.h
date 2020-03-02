/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __LOG_STORE_H__
#define __LOG_STORE_H__

#include <linux/types.h>


#define SRAM_HEADER_SIG (0xabcd1234)
#define DRAM_HEADER_SIG (0x5678ef90)
#define LOG_STORE_SIG (0xcdab3412)
#define LOG_EMMC_SIG (0x785690ef)
#define KEDUMP_ENABLE (0xecab1e)
#define KEDUMP_DISABLE (0xd15ab1e)

#define MAX_DRAM_COUNT	2

#define LOG_STORE_SIZE 0x40000	/*  DRAM buff 256KB*/

/*  log flag */
#define BUFF_VALIE		0x01
#define CAN_FREE		0x02
#define	NEED_SAVE_TO_EMMC	0x04
#define RING_BUFF		0x08
/* ring buf, if buf_full, buf_point is the start of the buf, */
/* else buf_point is the buf end, other buf is not used */
#define BUFF_FULL		0x10	/* buf is full */
/* array buf type, buf_point is the used buf end */
#define ARRAY_BUFF		0X20
#define BUFF_ALLOC_ERROR	0X40
#define BUFF_ERROR		0x80
#define BUFF_NOT_READY		0x100
#define BUFF_READY		0x200
/* pl or lk can printk the early printk information to uart cable */
#define BUFF_EARLY_PRINTK	0x400
#define	LOG_PL_LK  0x0	/* Preloader and lk log buff */

/* total 32 bytes <= u32(4 bytes) * 8 = 32 bytes */
struct pl_lk_log {
	u32 sig;            // default 0xabcd1234
	u32 buff_size;      // total buf size
	u32 off_pl;         // pl offset, sizeof(struct pl_lk_log)
	u32 sz_pl;          // preloader size
	u32 pl_flag;        // pl log flag
	u32 off_lk;         // lk offset, sizeof((struct pl_lk_log) + sz_pl
	u32 sz_lk;          // lk log size
	u32 lk_flag;        // lk log flag
};

/* total 40 bytes <= u32(4 bytes) * 10 = 40 bytes */
struct dram_buf_header {
	u32 sig;
	u32 flag;
	u32 buf_addr;
	u32 buf_size;
	u32 buf_offsize;
	u32 buf_point;
	u32 klog_addr;
	u32 klog_size;
	u32 atf_log_addr;
	u32 atf_log_len;
};

/* total 256 bytes */
struct sram_log_header {
	u32 sig;
	u32 reboot_count;
	u32 save_to_emmc;
	struct dram_buf_header dram_buf;        // 40 bytes
	struct pl_lk_log dram_curlog_header;    // 32 bytes
	u32 gz_log_addr;
	u32 gz_log_len;
	u32 reserve[41];                        // reserve 41 * 4 char size
};

enum EMMC_STORE_TYPE {
	UART_LOG = 0,
	LOG_LEVEL,
	PRINTK_PATELIMIT,
	KEDUMP_CTL,
	EMMC_STORE_TYPE_NR
};

/* emmc last block struct */
struct log_emmc_header {
	u32 sig;
	u32 offset;
	u32 uart_flag;
	u32 reserve[10];
};

#ifdef CONFIG_MTK_DRAM_LOG_STORE
void log_store_bootup(void);
void store_log_to_emmc_enable(bool value);
void disable_early_log(void);
int set_emmc_config(int type, int value);
int read_emmc_config(struct log_emmc_header *log_header);
#else

static inline void  log_store_bootup(void)
{

}

static inline void store_log_to_emmc_enable(bool value)
{

}

static inline void disable_early_log(void)
{
}

static inline int set_emmc_config(int type, int value)
{
	return 0;
}

static inline int read_emmc_config(struct log_emmc_header *log_header)
{
	return 0;
}
#endif
#endif

