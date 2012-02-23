/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef VIDC_INIT_H
#define VIDC_INIT_H
#include <linux/ion.h>
#include <media/msm/vidc_type.h>
#include <media/msm/vcd_property.h>

#define VIDC_MAX_NUM_CLIENTS 4
#define MAX_VIDEO_NUM_OF_BUFF 100

enum buffer_dir {
	BUFFER_TYPE_INPUT,
	BUFFER_TYPE_OUTPUT
};

struct buf_addr_table {
	unsigned long user_vaddr;
	unsigned long kernel_vaddr;
	unsigned long phy_addr;
	unsigned long buff_ion_flag;
	struct ion_handle *buff_ion_handle;
	int pmem_fd;
	struct file *file;
	unsigned long dev_addr;
	void *client_data;
};

struct video_client_ctx {
	void *vcd_handle;
	u32 num_of_input_buffers;
	u32 num_of_output_buffers;
	struct buf_addr_table input_buf_addr_table[MAX_VIDEO_NUM_OF_BUFF];
	struct buf_addr_table output_buf_addr_table[MAX_VIDEO_NUM_OF_BUFF];
	struct list_head msg_queue;
	struct mutex msg_queue_lock;
	struct mutex enrty_queue_lock;
	wait_queue_head_t msg_wait;
	struct completion event;
	struct vcd_property_h264_mv_buffer vcd_h264_mv_buffer;
	struct vcd_property_enc_recon_buffer recon_buffer[4];
	u32 event_status;
	u32 seq_header_set;
	u32 stop_msg;
	u32 stop_called;
	u32 stop_sync_cb;
	struct ion_client *user_ion_client;
	struct ion_handle *seq_hdr_ion_handle;
	struct ion_handle *h264_mv_ion_handle;
	struct ion_handle *recon_buffer_ion_handle[4];
	u32 dmx_disable;
};

void __iomem *vidc_get_ioaddr(void);
int vidc_load_firmware(void);
void vidc_release_firmware(void);
u32 vidc_get_fd_info(struct video_client_ctx *client_ctx,
		enum buffer_dir buffer, int pmem_fd,
		unsigned long kvaddr, int index);
u32 vidc_lookup_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer, u32 search_with_user_vaddr,
	unsigned long *user_vaddr, unsigned long *kernel_vaddr,
	unsigned long *phy_addr, int *pmem_fd, struct file **file,
	s32 *buffer_index);
u32 vidc_insert_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer, unsigned long user_vaddr,
	unsigned long *kernel_vaddr, int pmem_fd,
	unsigned long buffer_addr_offset,
	unsigned int max_num_buffers, unsigned long length);
u32 vidc_delete_addr_table(struct video_client_ctx *client_ctx,
	enum buffer_dir buffer, unsigned long user_vaddr,
	unsigned long *kernel_vaddr);
void vidc_cleanup_addr_table(struct video_client_ctx *client_ctx,
		enum buffer_dir buffer);

u32 vidc_timer_create(void (*timer_handler)(void *),
	void *user_data, void **timer_handle);
void  vidc_timer_release(void *timer_handle);
void  vidc_timer_start(void *timer_handle, u32 time_out);
void  vidc_timer_stop(void *timer_handle);


#endif
