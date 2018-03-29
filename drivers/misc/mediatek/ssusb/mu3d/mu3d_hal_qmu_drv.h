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

#ifndef MTK_QMU_H
#define MTK_QMU_H

/* #include <linux/mu3d/hal/mu3d_hal_osal.h> */
/* #include <linux/mu3d/hal/mu3d_hal_comm.h> */
/* #include <linux/mu3d/hal/mu3d_hal_usb_drv.h> */
/* #include "mu3d_hal_osal.h" */
/* #include "mu3d_hal_comm.h" */
#include "mu3d_hal_usb_drv.h"
#include "musb_core.h"

/*
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0464d/BABCFDAH.html
 * CA7 , CA15
 * The L1 data memory system has the following features:
 * data side cache line length of 64-bytes
 */
#define CACHE_LINE_SIZE 64

struct ssusb_gpd {
	u8 flag;
	u8 chksum;
	u16 data_buf_len;	/*Rx Allow Length */
	u32 next_gpd;		/* dma_addr_t */
	u32 buffer;		/* dma_addr_t */
	u16 buf_len;
	u8 ext_len;
	u8 ext_flag;
} /*__attribute__ ((packed), or (aligned(CACHE_LINE_SIZE)))*/;

/**
* dma: physical base address of GPD segment
* start: virtual base address of GPD segment
* end: the last gpd element
* next: the next gpd set to @enqueue.next_gpd
* enqueue: the first empty gpd to use
* dequeue: the first completed gpd serviced by ISR
* the ring elements of the gpd should be >= 2
*/
struct ssusb_gpd_range {
	dma_addr_t dma;
	struct ssusb_gpd *start;
	struct ssusb_gpd *end;
	struct ssusb_gpd *next;
	struct ssusb_gpd *enqueue;
	struct ssusb_gpd *dequeue;
};

#define AT_BD_EXT_LEN		0
#define MAX_GPD_NUM		64
#define MAX_BD_NUM		0
/* DVT+ */
#define STRESS_IOC_TH		8
#define STRESS_GPD_TH		24
#define RANDOM_STOP_DELAY	80
#define STRESS_DATA_LENGTH	(1024*64)	/* 1024*16 */
/* DVT- */
#define GPD_BUF_SIZE 65532
#define BD_BUF_SIZE 32768	/* set to half of 64K of max size */

#define IS_BDP 1

#define MAX_QMU_EP					MAX_EP_NUM	/*The better way is to read U3D_CAP_EPINFO */

#define TGPD_FLAGS_HWO			0x01
#define TGPD_IS_FLAGS_HWO(_pd)		(((struct ssusb_gpd *)_pd)->flag & TGPD_FLAGS_HWO)
#define TGPD_SET_FLAGS_HWO(_pd)		(((struct ssusb_gpd *)_pd)->flag |= TGPD_FLAGS_HWO)
#define TGPD_CLR_FLAGS_HWO(_pd)		(((struct ssusb_gpd *)_pd)->flag &= (~TGPD_FLAGS_HWO))
#define TGPD_FORMAT_BDP			0x02
#define TGPD_IS_FORMAT_BDP(_pd)		(((struct ssusb_gpd *)_pd)->flag & TGPD_FORMAT_BDP)
#define TGPD_SET_FORMAT_BDP(_pd)	(((struct ssusb_gpd *)_pd)->flag |= TGPD_FORMAT_BDP)
#define TGPD_CLR_FORMAT_BDP(_pd)	(((struct ssusb_gpd *)_pd)->flag &= (~TGPD_FORMAT_BDP))
#define TGPD_FORMAT_BPS			0x04
#define TGPD_IS_FORMAT_BPS(_pd)		(((struct ssusb_gpd *)_pd)->flag & TGPD_FORMAT_BPS)
#define TGPD_SET_FORMAT_BPS(_pd)	(((struct ssusb_gpd *)_pd)->flag |= TGPD_FORMAT_BPS)
#define TGPD_CLR_FORMAT_BPS(_pd)	(((struct ssusb_gpd *)_pd)->flag &= (~TGPD_FORMAT_BPS))
#define TGPD_GET_FLAG(_pd)		(((struct ssusb_gpd *)_pd)->flag)
#define TGPD_SET_CHKSUM(_pd, _n)	{ \
		((struct ssusb_gpd *)_pd)->chksum = mu3d_hal_cal_checksum((u8 *)_pd, _n) - 1;\
	}
/* #define TGPD_SET_CHKSUM_HWO(_pd, _n)    ((struct ssusb_gpd *)_pd)->chksum = mu3d_hal_cal_checksum((u8 *)_pd, _n)-1 */
#define TGPD_GET_CHKSUM(_pd)		(((struct ssusb_gpd *)_pd)->chksum)
#define TGPD_SET_FORMAT(_pd, _fmt)	{\
		(((struct ssusb_gpd *)_pd)->flag = (((struct ssusb_gpd *)_pd)->flag&(~TGPD_FORMAT_BDP))|(_fmt))\
	}
#define TGPD_GET_FORMAT(_pd)		((((struct ssusb_gpd *)_pd)->flag & TGPD_FORMAT_BDP)>>1)
#define TGPD_SET_DATA_BUF_LEN(_pd, _len) (((struct ssusb_gpd *)_pd)->data_buf_len = _len)
#define TGPD_ADD_DATA_BUF_LEN(_pd, _len) (((struct ssusb_gpd *)_pd)->data_buf_len += _len)
#define TGPD_GET_DATA_BUF_LEN(_pd)       (((struct ssusb_gpd *)_pd)->data_buf_len)
#define TGPD_SET_NEXT(_pd, _next)	{\
		(((struct ssusb_gpd *)_pd)->next_gpd = (u32)_next);\
	}
#define TGPD_GET_NEXT(_pd)		((u32)((struct ssusb_gpd *)_pd)->next_gpd)
#define TGPD_GET_TBD(_pd)		((TBD *)((struct ssusb_gpd *)_pd)->buffer)
#define TGPD_SET_DATA(_pd, _data)	(((struct ssusb_gpd *)_pd)->buffer = (u32)_data)
#define TGPD_GET_DATA(_pd)		(((struct ssusb_gpd *)_pd)->buffer)
#define TGPD_SET_BUF_LEN(_pd, _len)	(((struct ssusb_gpd *)_pd)->buf_len = _len)
#define TGPD_ADD_BUF_LEN(_pd, _len)	(((struct ssusb_gpd *)_pd)->buf_len += _len)
#define TGPD_GET_BUF_LEN(_pd)		(((struct ssusb_gpd *)_pd)->buf_len)
#define TGPD_SET_EXT_LEN(_pd, _len)	(((struct ssusb_gpd *)_pd)->ext_len = _len)
#define TGPD_GET_EXT_LEN(_pd)		(((struct ssusb_gpd *)_pd)->ext_len)
#define TGPD_SET_EPaddr(_pd, _EP)	{\
		(((struct ssusb_gpd *)_pd)->ext_flag = (((struct ssusb_gpd *)_pd)->ext_flag&0xF0)|(_EP))\
	}
#define TGPD_GET_EPaddr(_pd)		(((struct ssusb_gpd *)_pd)->ext_flag & 0x0F)
#define TGPD_FORMAT_TGL			0x10
#define TGPD_IS_FORMAT_TGL(_pd)		(((struct ssusb_gpd *)_pd)->ext_flag & TGPD_FORMAT_TGL)
#define TGPD_SET_FORMAT_TGL(_pd)	(((struct ssusb_gpd *)_pd)->ext_flag |= TGPD_FORMAT_TGL)
#define TGPD_CLR_FORMAT_TGL(_pd)	(((struct ssusb_gpd *)_pd)->ext_flag &= (~TGPD_FORMAT_TGL))
#define TGPD_FORMAT_ZLP			0x20
#define TGPD_IS_FORMAT_ZLP(_pd)		(((struct ssusb_gpd *)_pd)->ext_flag & TGPD_FORMAT_ZLP)
#define TGPD_SET_FORMAT_ZLP(_pd)	(((struct ssusb_gpd *)_pd)->ext_flag |= TGPD_FORMAT_ZLP)
#define TGPD_CLR_FORMAT_ZLP(_pd)	(((struct ssusb_gpd *)_pd)->ext_flag &= (~TGPD_FORMAT_ZLP))
#define TGPD_FORMAT_IOC			0x80
#define TGPD_IS_FORMAT_IOC(_pd)		(((struct ssusb_gpd *)_pd)->flag & TGPD_FORMAT_IOC)
#define TGPD_SET_FORMAT_IOC(_pd)	(((struct ssusb_gpd *)_pd)->flag |= TGPD_FORMAT_IOC)
#define TGPD_CLR_FORMAT_IOC(_pd)	(((struct ssusb_gpd *)_pd)->flag &= (~TGPD_FORMAT_IOC))
#define TGPD_SET_TGL(_pd, _TGL)			(((struct ssusb_gpd *)_pd)->ext_flag |= ((_TGL) ? 0x10 : 0x00))
#define TGPD_GET_TGL(_pd)			((((struct ssusb_gpd *)_pd)->ext_flag & TGPD_FORMAT_TGL) ? 1 : 0)
#define TGPD_SET_ZLP(_pd, _ZLP)		{\
		(((struct ssusb_gpd *)_pd)->ext_flag |= ((_ZLP) ? TGPD_FORMAT_ZLP : 0x00))\
	}
#define TGPD_GET_ZLP(_pd)			((((struct ssusb_gpd *)_pd)->ext_flag & TGPD_FORMAT_ZLP) ? 1 : 0)
/* #define TGPD_GET_EXT(_pd)             ((u8 *)_pd + sizeof(struct ssusb_gpd)) */


void mu3d_hal_resume_qmu(struct musb *musb, int q_num, USB_DIR dir);
void mu3d_hal_stop_qmu(struct musb *musb, int q_num, USB_DIR dir);
#if 0
struct ssusb_gpd *mu3d_hal_prepare_tx_gpd(struct ssusb_gpd *gpd, dma_addr_t pBuf, u32 data_length,
					  u8 ep_num, u8 _is_bdp, u8 isHWO, u8 ioc, u8 bps, u8 zlp);
struct ssusb_gpd *mu3d_hal_prepare_rx_gpd(struct ssusb_gpd *gpd, dma_addr_t pBuf, u32 data_len,
					  u8 ep_num, u8 _is_bdp, u8 isHWO,
					  u8 ioc, u8 bps, u32 cMaxPacketSize);
#endif
void mu3d_hal_insert_transfer_gpd(int ep_num, USB_DIR dir, dma_addr_t buf,
				  u32 count, u8 isHWO, u8 ioc, u8 bps, u8 zlp, u32 cMaxPacketSize);
void mu3d_hal_alloc_qmu_mem(struct musb *musb);
void mu3d_hal_free_qmu_mem(struct musb *musb);

void mu3d_hal_init_qmu(struct musb *musb);
void mu3d_hal_start_qmu(struct musb *musb, int q_num, USB_DIR dir);
void mu3d_hal_flush_qmu(struct musb *musb, int q_num, USB_DIR dir);
void mu3d_hal_restart_qmu(struct musb *musb, int q_num, USB_DIR dir);
void mu3d_hal_send_stall(struct musb *musb, int q_num, USB_DIR dir);
/* u8 mu3d_hal_cal_checksum(u8 *data, int len); */
/* struct ssusb_gpd *get_gpd(USB_DIR dir, u32 num); */
/* void gpd_ptr_align(USB_DIR dir, u32 num, struct ssusb_gpd *ptr); */

void qmu_done_tasklet(unsigned long data);
void qmu_exception_interrupt(struct musb *musb, u32 wQmuVal);

#endif
