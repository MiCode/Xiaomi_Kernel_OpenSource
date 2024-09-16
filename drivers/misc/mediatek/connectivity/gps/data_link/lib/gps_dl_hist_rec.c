/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "gps_dl_config.h"

#include "gps_each_link.h"
#include "gps_dl_osal.h"
#include "gps_dl_hist_rec.h"

enum gps_dl_hist_rec_rw_type {
	GPS_DL_HIST_REC_RW_READ,
	GPS_DL_HIST_REC_RW_WRITE,
	GPS_DL_HIST_REC_RW_TYPE_MAX
};

enum gps_dl_hist_rec_rw_dump_point {
	GPS_DL_HIST_REC_RW_DUMP_ON_REC_FULL,
	GPS_DL_HIST_REC_RW_DUMP_ON_INTERVAL,
	GPS_DL_HIST_REC_RW_DUMP_ON_PID_CHANGED,
	GPS_DL_HIST_REC_RW_DUMP_ON_FORCE_DUMP,
	GPS_DL_HIST_REC_RW_DUMP_ON_ERROR_LEN,
};

struct gps_dl_hist_rec_rw_item {
	int len;
};

#define GPS_DL_HIST_REC_RW_ITEM_MAX (8)
struct gps_dl_hist_rec_rw_list {
	struct gps_dl_hist_rec_rw_item items[GPS_DL_HIST_REC_RW_ITEM_MAX];
	unsigned int n_item;
	unsigned int rec_idx;
	int pid;
	enum gps_dl_hist_rec_rw_rec_point rec_point;
	enum gps_dl_hist_rec_rw_type type;
};

struct gps_dl_hist_rec_rw_list g_gps_dl_hist_rec_rw_list[GPS_DATA_LINK_NUM][GPS_DL_HIST_REC_RW_TYPE_MAX];


static void gps_dl_hist_rec_rw_do_dump(enum gps_dl_link_id_enum link_id,
	struct gps_dl_hist_rec_rw_list *p_list, enum gps_dl_hist_rec_rw_dump_point dump_point)
{
	GDL_LOGXW_DRW(link_id, "%s: dp=%d, pid=%d, i=%d, n=%d(%d), l=%d %d %d %d; %d %d %d %d",
		(p_list->type == GPS_DL_HIST_REC_RW_READ) ? "rd" : "wr",
		dump_point, p_list->pid, p_list->rec_idx, p_list->n_item, p_list->rec_point,
		p_list->items[0].len, p_list->items[1].len, p_list->items[2].len, p_list->items[3].len,
		p_list->items[4].len, p_list->items[5].len, p_list->items[6].len, p_list->items[7].len);
	p_list->rec_idx += p_list->n_item;
	p_list->n_item = 0;
	memset(&p_list->items, 0, sizeof(p_list->items));
}

static void gps_dl_hist_rec_rw_check_dump(enum gps_dl_link_id_enum link_id,
	struct gps_dl_hist_rec_rw_list *p_list, enum gps_dl_hist_rec_rw_rec_point rec_point)
{
	if (p_list->n_item > GPS_DL_HIST_REC_RW_ITEM_MAX) {
		GDL_LOGXE_DRW(link_id, "type=%d, n_rec=%d, rec_point=%d",
			p_list->type, p_list->n_item, rec_point);
		p_list->n_item = GPS_DL_HIST_REC_RW_ITEM_MAX;
	}

	if (p_list->n_item == GPS_DL_HIST_REC_RW_ITEM_MAX)
		gps_dl_hist_rec_rw_do_dump(link_id, p_list, GPS_DL_HIST_REC_RW_DUMP_ON_REC_FULL);
}

static void gps_dl_hist_rec_rw_add_rec(enum gps_dl_link_id_enum link_id,
	enum gps_dl_hist_rec_rw_type type,
	enum gps_dl_hist_rec_rw_rec_point rec_point,
	int pid, int len)
{
	struct gps_dl_hist_rec_rw_list *p_list;
	struct gps_dl_hist_rec_rw_item *p_item;
	enum gps_dl_hist_rec_rw_rec_point last_point;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	/* TODO: check type & rec_point */

	/* TODO: protect it by lock */
	p_list = &g_gps_dl_hist_rec_rw_list[link_id][type];
	if (p_list->pid == 0)
		p_list->pid = pid;
	else if (pid != p_list->pid && rec_point == DRW_RETURN) {
		gps_dl_hist_rec_rw_do_dump(link_id, p_list, GPS_DL_HIST_REC_RW_DUMP_ON_PID_CHANGED);
		p_list->pid = pid;
	}
	gps_dl_hist_rec_rw_check_dump(link_id, p_list, rec_point);

	p_item = &p_list->items[p_list->n_item];
	last_point = p_list->rec_point;
	p_list->rec_point = rec_point;

	if (last_point == DRW_RETURN && rec_point == DRW_ENTER) {
		/* TODO: record tiemstamp */
		p_item->len = len;
	} else if (last_point == DRW_ENTER && rec_point == DRW_RETURN) {
		p_item->len = len;
		p_list->n_item++;
		if (len <= 0)
			gps_dl_hist_rec_rw_do_dump(link_id, p_list, GPS_DL_HIST_REC_RW_DUMP_ON_PID_CHANGED);
		else
			gps_dl_hist_rec_rw_check_dump(link_id, p_list, DRW_RETURN);
	} else {
		GDL_LOGXE_DRW(link_id, "type=%d, n_rec=%d, mismatch rec_point=%d/%d, len=%d, pid=%d",
			p_list->type, p_list->n_item, last_point, rec_point, len, pid);
	}
}


void gps_each_link_rec_read(enum gps_dl_link_id_enum link_id, int pid, int len,
	enum gps_dl_hist_rec_rw_rec_point rec_point)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	gps_each_link_mutex_take(link_id, GPS_DL_MTX_BIG_LOCK);
	gps_dl_hist_rec_rw_add_rec(link_id, GPS_DL_HIST_REC_RW_READ, rec_point, pid, len);
	gps_each_link_mutex_give(link_id, GPS_DL_MTX_BIG_LOCK);
}

void gps_each_link_rec_write(enum gps_dl_link_id_enum link_id, int pid, int len,
	enum gps_dl_hist_rec_rw_rec_point rec_point)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	gps_each_link_mutex_take(link_id, GPS_DL_MTX_BIG_LOCK);
	gps_dl_hist_rec_rw_add_rec(link_id, GPS_DL_HIST_REC_RW_WRITE, rec_point, pid, len);
	gps_each_link_mutex_give(link_id, GPS_DL_MTX_BIG_LOCK);
}

void gps_each_link_rec_reset(enum gps_dl_link_id_enum link_id)
{
	enum gps_dl_hist_rec_rw_type type;
	struct gps_dl_hist_rec_rw_list *p_list;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	gps_each_link_mutex_take(link_id, GPS_DL_MTX_BIG_LOCK);
	for (type = 0; type < GPS_DL_HIST_REC_RW_TYPE_MAX; type++) {
		p_list = &g_gps_dl_hist_rec_rw_list[link_id][type];
		memset(p_list, 0, sizeof(*p_list));
		p_list->type = type;
		p_list->rec_point = DRW_RETURN;
	}
	gps_each_link_mutex_give(link_id, GPS_DL_MTX_BIG_LOCK);
}

void gps_each_link_rec_force_dump(enum gps_dl_link_id_enum link_id)
{
	enum gps_dl_hist_rec_rw_type type;
	struct gps_dl_hist_rec_rw_list *p_list;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	gps_each_link_mutex_take(link_id, GPS_DL_MTX_BIG_LOCK);
	for (type = 0; type < GPS_DL_HIST_REC_RW_TYPE_MAX; type++) {
		p_list = &g_gps_dl_hist_rec_rw_list[link_id][type];
		gps_dl_hist_rec_rw_do_dump(link_id, p_list, GPS_DL_HIST_REC_RW_DUMP_ON_FORCE_DUMP);
	}
	gps_each_link_mutex_give(link_id, GPS_DL_MTX_BIG_LOCK);
}

