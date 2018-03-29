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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <ccci.h>

#define CCCI_LOG_MAX_LEN 16

struct ccci_log_t {
	struct timeval tv;
	struct ccci_msg_t msg;
	int droped;
};

struct logic_ch_record_t {
	struct ccci_log_t log[CCCI_LOG_MAX_LEN];
	unsigned long msg_num;
	unsigned long drop_num;
	int log_idx;
	int dir;
	char *name;
};

struct ch_history_t {
	struct logic_ch_record_t all_ch[CCCI_MAX_CH_NUM];
	int md_id;
};

static struct ch_history_t *history_ctlb[MAX_MD_NUM];

void add_logic_layer_record(int md_id, struct ccci_msg_t *data, int drop)
{
	struct logic_ch_record_t *ctlb;
	unsigned int ch = data->channel;
	struct ccci_log_t *record;

	if (ch >= CCCI_MAX_CH_NUM)
		return;

	ctlb = &(history_ctlb[md_id]->all_ch[ch]);
	if (ctlb == NULL)
		return;

	record = &(ctlb->log[ctlb->log_idx]);
	ctlb->log_idx++;
	ctlb->log_idx &= (CCCI_LOG_MAX_LEN - 1);
	do_gettimeofday(&(record->tv));
	record->msg = *data;
	record->droped = drop;
	if (drop)
		ctlb->drop_num++;
	else
		ctlb->msg_num++;
}

static int s_to_date(long seconds, long usec, int *us, int *sec, int *min,
		     int *hour, int *day, int *month, int *year)
{
#define  DAY_PER_LEAP_YEAR        366
#define  DAY_PER_NON_LEAP_YEAR    365

	unsigned int i = 0;
	unsigned long mins, hours, days, month_t, year_t;
	unsigned char m[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (!sec || !min || !hour || !day || !month || !year) {
		CCCI_MSG("<ctl>%s invalid param!\n", __func__);
		return -1;
	}

	*us = usec;
	*sec = seconds % 60;
	mins = seconds / 60;
	*min = mins % 60;
	hours = mins / 60;
	*hour = hours % 24;
	days = hours / 24;

	year_t = 1970;

	while (1) {
		if (!(year_t % 4) && (year_t % 100)) {
			if (days >= DAY_PER_LEAP_YEAR) {
				days -= DAY_PER_LEAP_YEAR;
				year_t++;
			} else
				break;
		} else {
			if (days >= DAY_PER_NON_LEAP_YEAR) {
				days -= DAY_PER_NON_LEAP_YEAR;
				year_t++;
			} else
				break;
		}
	}

	if (!(year_t % 4) && year_t % 100)
		m[1] = 29;

	month_t = 1;
	for (i = 0; i < 12; i++) {
		if (days > m[i]) {
			days -= m[i];
			month_t++;
		} else
			break;
	}

	*day = days;
	*year = year_t;
	*month = month_t;

	return 0;
}

static int ccci_record_dump(struct ccci_log_t *log)
{
	int ms, sec, min, hour, day, month, year;
	long tv_sec, tv_usec;

	tv_sec = (long)log->tv.tv_sec;
	tv_usec = (long)log->tv.tv_usec;

	if ((tv_sec == 0) && (tv_usec == 0))
		return -1;

	s_to_date(tv_sec, tv_usec, &ms, &sec, &min, &hour, &day, &month, &year);

	if (!log->droped) {
		CCCI_DBG_COM_MSG
		    ("%08X %08X %02d %08X   %d-%02d-%02d %02d:%02d:%02d.%06d\n",
		     log->msg.data0, log->msg.data1, log->msg.channel,
		     log->msg.reserved, year, month, day, hour, min, sec, ms);
	} else {
		CCCI_DBG_COM_MSG
		    ("%08X %08X %02d %08X   %d-%02d-%02d %02d:%02d:%02d.%06d -\n",
		     log->msg.data0, log->msg.data1, log->msg.channel,
		     log->msg.reserved, year, month, day, hour, min, sec, ms);
	}
	return 0;
}

void logic_layer_ch_record_dump(int md_id, int ch)
{
	struct ch_history_t *ctlb = history_ctlb[md_id];
	int i, j;
	struct logic_ch_record_t *record;

	if ((ctlb != NULL) && (ch < CCCI_MAX_CH_NUM)) {
		record = &(ctlb->all_ch[ch]);
		CCCI_DBG_COM_MSG("\n");
		if (record->dir == CCCI_LOG_TX)
			CCCI_DBG_COM_MSG
			    ("ch%02d  tx:%ld\t tx_drop:%ld  name: %s\t\n", ch,
			     record->msg_num, record->drop_num, record->name);
		else
			CCCI_DBG_COM_MSG
			    ("ch%02d  rx:%ld\t rx_drop:%ld  name: %s\t\n", ch,
			     record->msg_num, record->drop_num, record->name);
		/*  dump last ten message */
		j = record->log_idx - 1;
		j &= (CCCI_LOG_MAX_LEN - 1);
		for (i = 0; i < 10; i++) {
			if (ccci_record_dump(&(record->log[j])) < 0)
				break;
			j--;
			j &= (CCCI_LOG_MAX_LEN - 1);
		}
	}
}

void dump_logical_layer_tx_rx_histroy(int md_id)
{
	int i = 0;
	struct ch_history_t *ctlb = history_ctlb[md_id];

	if (ctlb != NULL)
		for (i = 0; i < CCCI_MAX_CH_NUM; i++)
			logic_layer_ch_record_dump(md_id, i);
}

int statistics_init_ch_dir(int md_id, int ch, int dir, char *name)
{
	struct ch_history_t *ctlb = history_ctlb[md_id];
	struct logic_ch_record_t *record;
	int ret = 0;

	if ((ctlb != NULL) && (ch < CCCI_MAX_CH_NUM)) {
		record = &(ctlb->all_ch[ch]);
		record->dir = dir;
		record->name = name;
	} else
		ret = -1;

	return ret;
}

int statistics_init(int md_id)
{
	history_ctlb[md_id] = kmalloc(sizeof(struct ch_history_t), GFP_KERNEL);
	if (history_ctlb[md_id] != NULL)
		memset(history_ctlb[md_id], 0, sizeof(struct ch_history_t));

	return 0;
}

void statistics_exit(int md_id)
{
	kfree(history_ctlb[md_id]);
	history_ctlb[md_id] = NULL;
}
