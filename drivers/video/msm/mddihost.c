/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

#include <linux/clk.h>
#include <mach/clk.h>

struct semaphore mddi_host_mutex;

struct clk *mddi_io_clk;
static boolean mddi_host_powered = FALSE;
static boolean mddi_host_initialized = FALSE;
extern uint32 *mddi_reg_read_value_ptr;

mddi_lcd_func_type mddi_lcd;

extern mddi_client_capability_type mddi_client_capability_pkt;

#ifdef MDDI_HOST_WINDOW_WORKAROUND
/* Tables showing number of rows that would cause a packet length
 * ending in 0x02, for each number of columns. These tables have
 * been generated for MDDI packets that have 16 and 16 bits-per-pixel.
 * This is a work-around for MDDI clients that declare a CRC error
 * on MDDI packets where ((length & 0x00ff) == 0x02).
 */
static uint16 error_vals_16bpp[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 12, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 10, 0, 0, 0, 14, 0, 0, 0, 2, 0, 0, 4, 6, 12, 0,
0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0,
0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 11, 4, 0, 12, 0,
0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0,
0, 10, 0, 1, 0, 14, 0, 0, 0, 2, 0, 3, 4, 6, 12, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 12, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 10, 0, 0, 0, 14, 0, 0, 0, 2, 0, 0, 4, 6, 12, 0,
0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0,
0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 11, 4, 0, 12, 0,
0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0,
};

static uint16 error_vals_18bpp[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 14,
0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 9, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 7,
0, 0, 0, 0, 0, 0, 1, 0, 0, 16, 0, 0, 0, 0, 0, 6,
14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
7, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
0, 7, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 9, 0
};
#endif

#ifdef FEATURE_MDDI_HITACHI
extern void mddi_hitachi_window_adjust(uint16 x1,
				       uint16 x2, uint16 y1, uint16 y2);
#endif

extern void mddi_toshiba_lcd_init(void);

#ifdef FEATURE_MDDI_S6D0142
extern void mddi_s6d0142_lcd_init(void);
extern void mddi_s6d0142_window_adjust(uint16 x1,
				       uint16 x2,
				       uint16 y1,
				       uint16 y2,
				       mddi_llist_done_cb_type done_cb);
#endif

void mddi_init(void)
{
	if (mddi_host_initialized)
		return;

	mddi_host_initialized = TRUE;

	sema_init(&mddi_host_mutex, 1);

	if (!mddi_host_powered) {
		down(&mddi_host_mutex);
		mddi_host_init(MDDI_HOST_PRIM);
		mddi_host_powered = TRUE;
		up(&mddi_host_mutex);
		mdelay(10);
	}
}

int mddi_host_register_read(uint32 reg_addr,
     uint32 *reg_value_ptr, boolean wait, mddi_host_type host) {
	mddi_linked_list_type *curr_llist_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;
	int ret = 0;

	if (in_interrupt())
		MDDI_MSG_CRIT("Called from ISR context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		mddi_init();
	}

	down(&mddi_host_mutex);

	mddi_reg_read_value_ptr = reg_value_ptr;
	curr_llist_idx = mddi_get_reg_read_llist_item(host, TRUE);
	if (curr_llist_idx == UNASSIGNED_INDEX) {
		up(&mddi_host_mutex);

		/* need to change this to some sort of wait */
		MDDI_MSG_ERR("Attempting to queue up more than 1 reg read\n");
		return -EINVAL;
	}

	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_ptr->link_controller_flags = 0x11;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count = 0;

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->packet_data_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = 0x8001;
	regacc_pkt_ptr->register_address = reg_addr;

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, wait,
				   NULL, host);
	/* need to check if we can write the pointer or not */

	up(&mddi_host_mutex);

	if (wait) {
		int wait_ret;

		mddi_linked_list_notify_type *llist_notify_ptr;
		llist_notify_ptr = &llist_extern_notify[host][curr_llist_idx];
		wait_ret = wait_for_completion_timeout(
					&(llist_notify_ptr->done_comp), 5 * HZ);

		if (wait_ret <= 0)
			ret = -EBUSY;

		if (wait_ret < 0)
			printk(KERN_ERR "%s: failed to wait for completion!\n",
				__func__);
		else if (!wait_ret)
			printk(KERN_ERR "%s: Timed out waiting!\n", __func__);

		if (!ret && (mddi_reg_read_value_ptr == reg_value_ptr) &&
			(*reg_value_ptr == -EBUSY)) {
			printk(KERN_ERR "%s - failed to get data from client",
				   __func__);
			mddi_reg_read_value_ptr = NULL;
			ret = -EBUSY;
		}
	}

	MDDI_MSG_DEBUG("Reg Read value=0x%x\n", *reg_value_ptr);

	return ret;
}				/* mddi_host_register_read */

int mddi_host_register_write(uint32 reg_addr,
     uint32 reg_val, enum mddi_data_packet_size_type packet_size,
     boolean wait, mddi_llist_done_cb_type done_cb, mddi_host_type host) {
	mddi_linked_list_type *curr_llist_ptr;
	mddi_linked_list_type *curr_llist_dma_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;
	int ret = 0;

	if (in_interrupt())
		MDDI_MSG_CRIT("Called from ISR context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		mddi_init();
	}

	down(&mddi_host_mutex);

	curr_llist_idx = mddi_get_next_free_llist_item(host, TRUE);
	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_dma_ptr = &llist_dma_extern[host][curr_llist_idx];

	curr_llist_ptr->link_controller_flags = 1;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count = 4;

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count +
					(uint16)packet_size;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = 0x0001;
	regacc_pkt_ptr->register_address = reg_addr;
	regacc_pkt_ptr->register_data_list[0] = reg_val;

	MDDI_MSG_DEBUG("Reg Access write reg=0x%x, value=0x%x\n",
		       regacc_pkt_ptr->register_address,
		       regacc_pkt_ptr->register_data_list[0]);

	regacc_pkt_ptr = &curr_llist_dma_ptr->packet_header.register_pkt;
	curr_llist_ptr->packet_data_pointer =
	    (void *)(&regacc_pkt_ptr->register_data_list[0]);

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, wait,
				   done_cb, host);

	up(&mddi_host_mutex);

	if (wait) {
		int wait_ret;

		mddi_linked_list_notify_type *llist_notify_ptr;
		llist_notify_ptr = &llist_extern_notify[host][curr_llist_idx];
		wait_ret = wait_for_completion_timeout(
					&(llist_notify_ptr->done_comp), 5 * HZ);

		if (wait_ret <= 0)
			ret = -EBUSY;

		if (wait_ret < 0)
			printk(KERN_ERR "%s: failed to wait for completion!\n",
				__func__);
		else if (!wait_ret)
			printk(KERN_ERR "%s: Timed out waiting!\n", __func__);
	}

	return ret;
}				/* mddi_host_register_write */

boolean mddi_host_register_read_int
    (uint32 reg_addr, uint32 *reg_value_ptr, mddi_host_type host) {
	mddi_linked_list_type *curr_llist_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;

	if (!in_interrupt())
		MDDI_MSG_CRIT("Called from TASK context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		return FALSE;
	}

	if (down_trylock(&mddi_host_mutex) != 0)
		return FALSE;

	mddi_reg_read_value_ptr = reg_value_ptr;
	curr_llist_idx = mddi_get_reg_read_llist_item(host, FALSE);
	if (curr_llist_idx == UNASSIGNED_INDEX) {
		up(&mddi_host_mutex);
		return FALSE;
	}

	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_ptr->link_controller_flags = 0x11;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count = 0;

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->packet_data_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = 0x8001;
	regacc_pkt_ptr->register_address = reg_addr;

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, FALSE,
				   NULL, host);
	/* need to check if we can write the pointer or not */

	up(&mddi_host_mutex);

	return TRUE;

}				/* mddi_host_register_read */

boolean mddi_host_register_write_int
    (uint32 reg_addr,
     uint32 reg_val, mddi_llist_done_cb_type done_cb, mddi_host_type host) {
	mddi_linked_list_type *curr_llist_ptr;
	mddi_linked_list_type *curr_llist_dma_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;

	if (!in_interrupt())
		MDDI_MSG_CRIT("Called from TASK context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		return FALSE;
	}

	if (down_trylock(&mddi_host_mutex) != 0)
		return FALSE;

	curr_llist_idx = mddi_get_next_free_llist_item(host, FALSE);
	if (curr_llist_idx == UNASSIGNED_INDEX) {
		up(&mddi_host_mutex);
		return FALSE;
	}

	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_dma_ptr = &llist_dma_extern[host][curr_llist_idx];

	curr_llist_ptr->link_controller_flags = 1;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count = 4;

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count + 4;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = 0x0001;
	regacc_pkt_ptr->register_address = reg_addr;
	regacc_pkt_ptr->register_data_list[0] = reg_val;

	regacc_pkt_ptr = &curr_llist_dma_ptr->packet_header.register_pkt;
	curr_llist_ptr->packet_data_pointer =
	    (void *)(&(regacc_pkt_ptr->register_data_list[0]));

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, FALSE,
				   done_cb, host);
	up(&mddi_host_mutex);

	return TRUE;

}				/* mddi_host_register_write */

void mddi_wait(uint16 time_ms)
{
	mdelay(time_ms);
}

void mddi_client_lcd_vsync_detected(boolean detected)
{
	if (mddi_lcd.vsync_detected)
		(*mddi_lcd.vsync_detected) (detected);
}

/* extended version of function includes done callback */
void mddi_window_adjust_ext(struct msm_fb_data_type *mfd,
			    uint16 x1,
			    uint16 x2,
			    uint16 y1,
			    uint16 y2, mddi_llist_done_cb_type done_cb)
{
#ifdef FEATURE_MDDI_HITACHI
	if (mfd->panel.id == HITACHI)
		mddi_hitachi_window_adjust(x1, x2, y1, y2);
#elif defined(FEATURE_MDDI_S6D0142)
	if (mfd->panel.id == MDDI_LCD_S6D0142)
		mddi_s6d0142_window_adjust(x1, x2, y1, y2, done_cb);
#else
	/* Do nothing then... except avoid lint/compiler warnings */
	(void)x1;
	(void)x2;
	(void)y1;
	(void)y2;
	(void)done_cb;
#endif
}

void mddi_window_adjust(struct msm_fb_data_type *mfd,
			uint16 x1, uint16 x2, uint16 y1, uint16 y2)
{
	mddi_window_adjust_ext(mfd, x1, x2, y1, y2, NULL);
}

#ifdef MDDI_HOST_WINDOW_WORKAROUND
uint16 mddi_assign_pkt_height(uint16 pkt_width,
	uint16 pkt_height, uint16 bpp)
{
	uint16 new_pkt_height;
	uint16 problem_height = 0;

	if (pkt_width <= 240) {
		if (bpp == 16)
			problem_height = error_vals_16bpp[pkt_width-1];
		else if (bpp == 18)
			problem_height = error_vals_18bpp[pkt_width-1];
		else {
			printk(KERN_ERR"Invalid bpp value");
			return -EINVAL;
		}
	}
	if (problem_height == pkt_height)
		new_pkt_height = problem_height - 1;
	else
		new_pkt_height = pkt_height;

	return new_pkt_height;
}
#endif

#ifdef ENABLE_MDDI_MULTI_READ_WRITE
int mddi_host_register_multiwrite(uint32 reg_addr,
	uint32 *value_list_ptr,
	uint32 value_count, boolean wait, mddi_llist_done_cb_type done_cb,
	mddi_host_type host)
{
	mddi_linked_list_type *curr_llist_ptr;
	mddi_linked_list_type *curr_llist_dma_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;
	int ret = 0;

	if (!value_list_ptr || !value_count ||
		value_count > MDDI_HOST_MAX_CLIENT_REG_IN_SAME_ADDR) {
		MDDI_MSG_ERR("\n Invalid value_list or value_count");
		return -EINVAL;
	}

	if (in_interrupt())
		MDDI_MSG_CRIT("Called from ISR context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		mddi_init();
	}

	down(&mddi_host_mutex);

	curr_llist_idx = mddi_get_next_free_llist_item(host, TRUE);
	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_dma_ptr = &llist_dma_extern[host][curr_llist_idx];

	curr_llist_ptr->link_controller_flags = 1;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count =
		(uint16)(value_count * 4);

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count
		+ curr_llist_ptr->packet_data_count;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = value_count;
	regacc_pkt_ptr->register_address = reg_addr;
	memcpy((void *)&regacc_pkt_ptr->register_data_list[0], value_list_ptr,
		   curr_llist_ptr->packet_data_count);

	regacc_pkt_ptr = &curr_llist_dma_ptr->packet_header.register_pkt;
	curr_llist_ptr->packet_data_pointer =
		(void *)(&regacc_pkt_ptr->register_data_list[0]);
	MDDI_MSG_DEBUG("MultiReg Access write reg=0x%x, value[0]=0x%x\n",
		       regacc_pkt_ptr->register_address,
		       regacc_pkt_ptr->register_data_list[0]);

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, wait,
				   done_cb, host);

	up(&mddi_host_mutex);

	if (wait) {
		int wait_ret;

		mddi_linked_list_notify_type *llist_notify_ptr;
		llist_notify_ptr = &llist_extern_notify[host][curr_llist_idx];
		wait_ret = wait_for_completion_timeout(
					&(llist_notify_ptr->done_comp), 5 * HZ);

		if (wait_ret <= 0)
			ret = -EBUSY;

		if (wait_ret < 0)
			printk(KERN_ERR "%s: failed to wait for completion!\n",
				__func__);
		else if (!wait_ret)
			printk(KERN_ERR "%s: Timed out waiting!\n", __func__);
	}

	return ret;
}

int mddi_host_register_multiread(uint32 reg_addr,
	uint32 *value_list_ptr, uint32 value_count,
	boolean wait, mddi_host_type host) {
	mddi_linked_list_type *curr_llist_ptr;
	mddi_register_access_packet_type *regacc_pkt_ptr;
	uint16 curr_llist_idx;
	int ret = 0;

	if (!value_list_ptr || !value_count ||
		value_count >= MDDI_HOST_MAX_CLIENT_REG_IN_SAME_ADDR) {
		MDDI_MSG_ERR("\n Invalid value_list or value_count");
		return -EINVAL;
	}

	if (in_interrupt())
		MDDI_MSG_CRIT("Called from ISR context\n");

	if (!mddi_host_powered) {
		MDDI_MSG_ERR("MDDI powered down!\n");
		mddi_init();
	}

	down(&mddi_host_mutex);

	mddi_reg_read_value_ptr = value_list_ptr;
	curr_llist_idx = mddi_get_reg_read_llist_item(host, TRUE);
	if (curr_llist_idx == UNASSIGNED_INDEX) {
		up(&mddi_host_mutex);

		/* need to change this to some sort of wait */
		MDDI_MSG_ERR("Attempting to queue up more than 1 reg read\n");
		return -EINVAL;
	}

	curr_llist_ptr = &llist_extern[host][curr_llist_idx];
	curr_llist_ptr->link_controller_flags = 0x11;
	curr_llist_ptr->packet_header_count = 14;
	curr_llist_ptr->packet_data_count = 0;

	curr_llist_ptr->next_packet_pointer = NULL;
	curr_llist_ptr->packet_data_pointer = NULL;
	curr_llist_ptr->reserved = 0;

	regacc_pkt_ptr = &curr_llist_ptr->packet_header.register_pkt;

	regacc_pkt_ptr->packet_length = curr_llist_ptr->packet_header_count;
	regacc_pkt_ptr->packet_type = 146;	/* register access packet */
	regacc_pkt_ptr->bClient_ID = 0;
	regacc_pkt_ptr->read_write_info = 0x8000 | value_count;
	regacc_pkt_ptr->register_address = reg_addr;

	/* now adjust pointers */
	mddi_queue_forward_packets(curr_llist_idx, curr_llist_idx, wait,
				   NULL, host);
	/* need to check if we can write the pointer or not */

	up(&mddi_host_mutex);

	if (wait) {
		int wait_ret;

		mddi_linked_list_notify_type *llist_notify_ptr;
		llist_notify_ptr = &llist_extern_notify[host][curr_llist_idx];
		wait_ret = wait_for_completion_timeout(
					&(llist_notify_ptr->done_comp), 5 * HZ);

		if (wait_ret <= 0)
			ret = -EBUSY;

		if (wait_ret < 0)
			printk(KERN_ERR "%s: failed to wait for completion!\n",
				__func__);
		else if (!wait_ret)
			printk(KERN_ERR "%s: Timed out waiting!\n", __func__);

		if (!ret && (mddi_reg_read_value_ptr == value_list_ptr) &&
			(*value_list_ptr == -EBUSY)) {
			printk(KERN_ERR "%s - failed to get data from client",
				   __func__);
			mddi_reg_read_value_ptr = NULL;
			ret = -EBUSY;
		}
	}

	MDDI_MSG_DEBUG("MultiReg Read value[0]=0x%x\n", *value_list_ptr);

	return ret;
}
#endif
