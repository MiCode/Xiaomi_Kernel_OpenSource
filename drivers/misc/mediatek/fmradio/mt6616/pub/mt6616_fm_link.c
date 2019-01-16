/* mt6626_fm_link.c
 *
 * (C) Copyright 2009
 * MediaTek <www.MediaTek.com>
 * Hongcheng <hongcheng.xia@MediaTek.com>
 *
 * MT6626 FM Radio Driver -- setup data link
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/fm.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"

#include "mt6626_fm.h"
#include "mt6626_fm_link.h"
#include "mt6626_fm_reg.h"

/* these functions are defined after Linux2.6.32 */
static int fm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int fm_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int fm_i2c_remove(struct i2c_client *client);


static const struct i2c_device_id fm_i2c_id = { MT6626_DEV, 0 };
static unsigned short force[] =
    { MT6626_I2C_PORT, MT6626_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

static struct i2c_client_address_data addr_data = {
	.forces = forces
};

struct i2c_driver MT6626_driver = {
	.probe = fm_i2c_probe,
	.remove = fm_i2c_remove,
	.detect = fm_i2c_detect,
	.driver.name = MT6626_DEV,
	.id_table = &fm_i2c_id,
	.address_data = &addr_data,
};

static struct i2c_client *g_client;

static int fm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	WCN_DBG(FM_NTC | LINK, "%s\n", __func__);
	g_client = client;

	return ret;
}

static int fm_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	WCN_DBG(FM_NTC | LINK, "%s\n", __func__);
	strcpy(info->type, MT6626_DEV);
	return 0;
}

static int fm_i2c_remove(struct i2c_client *client)
{
	WCN_DBG(FM_NTC | LINK, "%s\n", __func__);
	return 0;
}

static fm_u32 fm_event_set(struct fm_link_event *thiz, fm_u32 mask)
{
	thiz->flag |= mask;
	wake_up(&thiz->wq);

	return thiz->flag;
}

static fm_u32 fm_event_clr(struct fm_link_event *thiz, fm_u32 mask)
{
	return thiz->flag &= ~mask;
}

/**
 * fm_event_check - sleep until a condition gets true or a timeout elapses
 * @thiz: the pointer of current object
 * @mask: bitmap in fm_u32
 * @timeout: timeout, in jiffies
 *
 * fm_event_set() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function returns 0 if the @timeout elapsed, and the remaining
 * jiffies if the condition evaluated to true before the timeout elapsed.
 */
static long fm_event_check(struct fm_link_event *thiz, fm_u32 mask, long timeout)
{
	return wait_event_timeout(thiz->wq, ((thiz->flag & mask) == mask), timeout * HZ);
}

static fm_u32 fm_event_get(struct fm_link_event *thiz)
{
	return thiz->flag;

}

static fm_u32 fm_event_rst(struct fm_link_event *thiz)
{
	return thiz->flag = 0;
}

static struct fm_link_event *stcdone_event;

fm_s32 fm_link_setup(void *data)
{
	if (!(stcdone_event = kzalloc(sizeof(struct fm_link_event), GFP_KERNEL))) {
		WCN_DBG(FM_ALT | LINK, "kzalloc(fm_link_event) -ENOMEM\n");
		return -1;
	}

	stcdone_event->rst = fm_event_rst;
	stcdone_event->get = fm_event_get;
	stcdone_event->clr = fm_event_clr;
	stcdone_event->check = fm_event_check;
	stcdone_event->set = fm_event_set;
	init_waitqueue_head(&stcdone_event->wq);
	stcdone_event->rst(stcdone_event);	/* set flag to 0x00000000 */

	WCN_DBG(FM_NTC | LINK, "fm link setup\n");
	return i2c_add_driver(&MT6626_driver);
}

fm_s32 fm_link_release(void)
{
	if (stcdone_event) {
		kfree(stcdone_event);
	}

	WCN_DBG(FM_NTC | LINK, "fm link release\n");
	i2c_del_driver(&MT6626_driver);
	return 0;
}

/*
 * fm_ctrl_rx
 * the low level func to read a rigister
 * @addr - rigister address
 * @val - the pointer of target buf
 * If success, return 0; else error code
 */
fm_s32 fm_ctrl_rx(fm_u8 addr, fm_u16 *val)
{
	fm_s32 n;
	fm_s8 b[2] = { 0 };

	/* first, send addr to MT6626 */
	n = i2c_master_send(g_client, (char *)&addr, 1);

	if (n < 0) {
		WCN_DBG(FM_ALT | LINK, "rx 1, [addr=0x%02X] [err=%d]\n", addr, n);
		return -1;
	}
	/* second, receive two byte from MT6626 */
	n = i2c_master_recv(g_client, b, 2);

	if (n < 0) {
		WCN_DBG(FM_ALT | LINK, "rx 2, [addr=0x%02X] [err=%d]\n", addr, n);
		return -2;
	}

	*val = ((uint16_t) b[0] << 8 | (uint16_t) b[1]);

	return 0;
}

/*
 * fm_ctrl_tx
 * the low level func to write a rigister
 * @addr - rigister address
 * @val - value will be writed in the rigister
 * If success, return 0; else error code
 */
fm_s32 fm_ctrl_tx(fm_u8 addr, fm_u16 val)
{
	fm_s32 n;
	fm_s8 b[3];

	b[0] = addr;
	b[1] = (char)(val >> 8);
	b[2] = (char)(val & 0xFF);

	n = i2c_master_send(g_client, b, 3);

	if (n < 0) {
		WCN_DBG(FM_ALT | LINK, "tx, [addr=0x%02X] [err=%d]\n", addr, n);
		return -1;
	}

	return 0;
}

/*
 * fm_cmd_tx() - send cmd to FM firmware and wait event
 * @buf - send buffer
 * @len - the length of cmd
 * @mask - the event flag mask
 * @	cnt - the retry conter
 * @timeout - timeout per cmd
 * Return 0, if success; error code, if failed
 */
fm_s32 fm_cmd_tx(fm_u8 *buf, fm_u16 len, fm_s32 mask, fm_s32 cnt, fm_s32 timeout,
		 fm_s32(*callback) (struct fm_res_ctx *result))
{
	return 0;
}

fm_bool fm_wait_stc_done(fm_u32 sec)
{
	fm_s32 ret_time = 0;

	ret_time = stcdone_event->check(stcdone_event, FLAG_TEST, sec);

	if (!ret_time) {
		WCN_DBG(FM_WAR | LINK, "wait stc done fail\n");
		return fm_false;
	} else {
		WCN_DBG(FM_DBG | LINK, "wait stc done ok\n");
	}

	stcdone_event->clr(stcdone_event, FLAG_TEST);
	return fm_true;
}

fm_s32 fm_event_parser(fm_s32(*rds_parser) (struct rds_rx_t *, fm_s32))
{
	fm_u16 tmp_reg;

	fm_ctrl_rx(FM_MAIN_INTR, &tmp_reg);

	if (tmp_reg & 0x0001) {
		/* clear status flag */
		fm_ctrl_tx(FM_MAIN_INTR, (tmp_reg & 0xFFFF) | 0x0001);
		stcdone_event->set(stcdone_event, FLAG_TEST);
	}

	if (tmp_reg & 0x0020) {
		/* clear status flag */
		fm_ctrl_tx(FM_MAIN_INTR, (tmp_reg & 0xFFFF) | 0x0020);

		/*Handle the RDS data that we get */
		if (rds_parser) {
			rds_parser(NULL, 0);	/* mt6626 rds lib will get rds raw data by itself */
		} else {
			WCN_DBG(FM_WAR | LINK, "no method to parse RDS data\n");
		}
	}

	return 0;
}

fm_s32 fm_force_active_event(fm_u32 mask)
{
	return 0;
}
