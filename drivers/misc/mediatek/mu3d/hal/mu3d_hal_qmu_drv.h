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

#include "mu3d_hal_osal.h"
#include "mu3d_hal_hw.h"
#include "mu3d_hal_comm.h"
#include "mu3d_hal_usb_drv.h"
#include <linux/platform_device.h>

#if defined(CONFIG_USB_MU3D_DRV_36BIT)

struct tx_haddr {
	unsigned char hiaddr;
	unsigned char reserved;
};

struct rx_haddr {
	unsigned char hiaddr;
};

union gpd_b14 {
	unsigned char ExtLength;		/*Tx ExtLength for  TXGPD*/
	struct rx_haddr rx_haddr;	/*Rx hi address for RXGPD*/
};

union gpd_w1 {
	unsigned short DataBufferLen;	/*Rx Allow Length for RXGPD*/
	struct tx_haddr tx_haddr;   /*Tx hi address for TXGPD */
};

#endif

struct TGPD {
	unsigned char flag;
	unsigned char chksum;
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	union gpd_w1 gpd_W1;
#else
	unsigned short DataBufferLen;	/*Rx Allow Length */
#endif
#ifdef CONFIG_ARM64
	unsigned int pNext;
	unsigned int pBuf;
#else
	struct TGPD *pNext;
	unsigned char *pBuf;
#endif
	unsigned short bufLen;
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	union gpd_b14 gpd_B14;
#else
	unsigned char ExtLength;
#endif
	unsigned char ZTepFlag;
/*} __attribute__ ((packed, aligned(4))) TGPD, *PGPD;*/
} __packed __aligned(4);

struct TBD {
	unsigned char flag;
	unsigned char chksum;
	unsigned short DataBufferLen;	/*Rx Allow Length */
#ifdef CONFIG_ARM64
	unsigned int pNext;
	unsigned int pBuf;
#else
	struct TBD *pNext;
	unsigned char *pBuf;
#endif
	unsigned short bufLen;
	unsigned char extLen;
	unsigned char reserved;
/*} __attribute__ ((packed, aligned(4))) TBD, *PBD;*/
} __packed __aligned(4);

struct GPD_R {
	struct TGPD *pNext;
	struct TGPD *pStart;
	struct TGPD *pEnd;
};

struct BD_R {
	struct TBD *pNext;
	struct TBD *pStart;
	struct TBD *pEnd;
};

struct qmu_desc_map {
	void *p_desc;
	dma_addr_t p_desc_dma;
};

/*
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0464d/BABCFDAH.html
 * CA7 , CA15
 * The L1 data memory system has the following features:
 * data side cache line length of 64-bytes
 */
#define CACHE_LINE_SIZE 64
#ifdef CACHE_LINE_SIZE
/*
 * The min size of GPD must align cache line size.
 * So using GPD_EXT_LEN as the dummy space.
*/
#define AT_GPD_EXT_LEN	 (CACHE_LINE_SIZE-16)
#else
#define AT_GPD_EXT_LEN 0
#endif
#define AT_BD_EXT_LEN		0
#define MAX_GPD_NUM		40
#define MAX_BD_NUM		0
/* DVT+ */
#define STRESS_IOC_TH		8
#define STRESS_GPD_TH		24
#define RANDOM_STOP_DELAY	80
#ifdef CONFIG_USBIF_COMPLIANCE
#define STRESS_DATA_LENGTH	1024
#else
#define STRESS_DATA_LENGTH	(1024*64)	/* 1024*16 */
#endif
/* DVT- */
#define GPD_BUF_SIZE 65532
#define BD_BUF_SIZE 32768	/* set to half of 64K of max size */

#define IS_BDP 1

#define MAX_QMU_EP					MAX_EP_NUM	/*The better way is to read U3D_CAP_EPINFO */

#define TGPD_FLAGS_HWO		0x01
#define TGPD_IS_FLAGS_HWO(_pd)	(((struct TGPD *)_pd)->flag & TGPD_FLAGS_HWO)
#define TGPD_SET_FLAGS_HWO(_pd)	(((struct TGPD *)_pd)->flag |= TGPD_FLAGS_HWO)
#define TGPD_CLR_FLAGS_HWO(_pd)	(((struct TGPD *)_pd)->flag &= (~TGPD_FLAGS_HWO))
#define TGPD_FORMAT_BDP		0x02
#define TGPD_IS_FORMAT_BDP(_pd)	(((struct TGPD *)_pd)->flag & TGPD_FORMAT_BDP)
#define TGPD_SET_FORMAT_BDP(_pd)	(((struct TGPD *)_pd)->flag |= TGPD_FORMAT_BDP)
#define TGPD_CLR_FORMAT_BDP(_pd)	(((struct TGPD *)_pd)->flag &= (~TGPD_FORMAT_BDP))
#define TGPD_FORMAT_BPS		0x04
#define TGPD_IS_FORMAT_BPS(_pd)	(((struct TGPD *)_pd)->flag & TGPD_FORMAT_BPS)
#define TGPD_SET_FORMAT_BPS(_pd)	(((struct TGPD *)_pd)->flag |= TGPD_FORMAT_BPS)
#define TGPD_CLR_FORMAT_BPS(_pd)	(((struct TGPD *)_pd)->flag &= (~TGPD_FORMAT_BPS))
#define TGPD_SET_FLAG(_pd, _flag)	(((struct TGPD *)_pd)->flag = \
(((struct TGPD *)_pd)->flag&(~TGPD_FLAGS_HWO))|(_flag))
#define TGPD_GET_FLAG(_pd)		((((struct TGPD *)_pd)->flag & TGPD_FLAGS_HWO))
#define TGPD_SET_CHKSUM(_pd, _n)		(((struct TGPD *)_pd)->chksum = \
mu3d_hal_cal_checksum((unsigned char *)_pd, _n)-1)
#define TGPD_SET_CHKSUM_HWO(_pd, _n)    (((struct TGPD *)_pd)->chksum = \
mu3d_hal_cal_checksum((unsigned char *)_pd, _n)-1)
#define TGPD_GET_CHKSUM(_pd)		(((struct TGPD *)_pd)->chksum)
#define TGPD_SET_FORMAT(_pd, _fmt)	(((struct TGPD *)_pd)->flag = \
(((struct TGPD *)_pd)->flag&(~TGPD_FORMAT_BDP))|(_fmt))
#define TGPD_GET_FORMAT(_pd)		((((struct TGPD *)_pd)->flag & TGPD_FORMAT_BDP)>>1)
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
#define TGPD_SET_DataBUF_LEN(_pd, _len) (((struct TGPD *)_pd)->gpd_W1.DataBufferLen = _len)
#define TGPD_ADD_DataBUF_LEN(_pd, _len) (((struct TGPD *)_pd)->gpd_W1.DataBufferLen += _len)
#define TGPD_GET_DataBUF_LEN(_pd)       (((struct TGPD *)_pd)->gpd_W1.DataBufferLen)
#else
#define TGPD_SET_DataBUF_LEN(_pd, _len) (((struct TGPD *)_pd)->DataBufferLen = _len)
#define TGPD_ADD_DataBUF_LEN(_pd, _len) (((struct TGPD *)_pd)->DataBufferLen += _len)
#define TGPD_GET_DataBUF_LEN(_pd)       (((struct TGPD *)_pd)->DataBufferLen)
#endif

#ifdef CONFIG_ARM64
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
#define TGPD_SET_NEXT(_pd, _next)	(((struct TGPD *)_pd)->pNext = (u32)_next)
#define TGPD_SET_NEXT_TXHI(_pd, _next)	\
	do {	\
		((struct TGPD *) _pd)->gpd_W1.tx_haddr.hiaddr &= 0x0F;	\
		((struct TGPD *) _pd)->gpd_W1.tx_haddr.hiaddr |= ((u8)_next << 4);	\
	} while (0)

#define TGPD_SET_NEXT_RXHI(_pd, _next)	\
	do {	\
		((struct TGPD *) _pd)->gpd_B14.rx_haddr.hiaddr &= 0x0F; \
		((struct TGPD *) _pd)->gpd_B14.rx_haddr.hiaddr |= ((u8)_next << 4); \
	} while (0)

#define TGPD_GET_NEXT(_pd)		((uintptr_t)((struct TGPD *)_pd)->pNext)
#define TGPD_GET_NEXT_TXHI(_pd)		((uintptr_t)((struct TGPD *)_pd)->gpd_W1.tx_haddr.hiaddr >> 4)
#define TGPD_GET_NEXT_RXHI(_pd)		((uintptr_t)((struct TGPD *)_pd)->gpd_B14.rx_haddr.hiaddr >> 4)
#define TGPD_GET_NEXT_TX(_pd)		((struct TGPD *)(TGPD_GET_NEXT(_pd) |  (TGPD_GET_NEXT_TXHI(_pd) << 32)))
#define TGPD_GET_NEXT_RX(_pd)		((struct TGPD *)(TGPD_GET_NEXT(_pd) |  (TGPD_GET_NEXT_RXHI(_pd) << 32)))
#define TGPD_SET_TBD(_pd, _tbd)	(((struct TGPD *)_pd)->pBuf = (u32)_tbd; TGPD_SET_FORMAT_BDP(_pd))
#define TGPD_GET_TBD(_pd)		((struct TBD *)(uintptr_t)((sturct TGPD *)_pd)->pBuf)
#define TGPD_SET_DATA(_pd, _data)	(((struct TGPD *)_pd)->pBuf = (u32)_data)
#define TGPD_SET_DATA_TXHI(_pd, _next)	\
	do {	\
		((struct TGPD *)_pd)->gpd_W1.tx_haddr.hiaddr &= 0xF0; \
		((struct TGPD *)_pd)->gpd_W1.tx_haddr.hiaddr |= ((u8)_next & 0x0F); \
	} while (0)

#define TGPD_SET_DATA_RXHI(_pd, _next)	\
	do {	\
		((struct TGPD *)_pd)->gpd_B14.rx_haddr.hiaddr &= 0xF0; \
		((struct TGPD *)_pd)->gpd_B14.rx_haddr.hiaddr |= ((u8)_next & 0x0F); \
	} while (0)

#define TGPD_GET_DATA(_pd)		((uintptr_t)((struct TGPD *)_pd)->pBuf)
#define TGPD_GET_DATA_TXHI(_pd)		((uintptr_t)((struct TGPD *)_pd)->gpd_W1.tx_haddr.hiaddr & 0x0F)
#define TGPD_GET_DATA_RXHI(_pd)		((uintptr_t)((struct TGPD *)_pd)->gpd_B14.rx_haddr.hiaddr & 0x0F)
#define TGPD_GET_DATA_TX(_pd)		((struct TGPD *)(TGPD_GET_DATA(_pd) |  (TGPD_GET_DATA_TXHI(_pd) << 32)))
#define TGPD_GET_DATA_RX(_pd)		((struct TGPD *)(TGPD_GET_DATA(_pd) |  (TGPD_GET_DATA_RXHI(_pd) << 32)))

#else
#define TGPD_SET_NEXT(_pd, _next)	(((struct TGPD *)_pd)->pNext = (u32)_next)
#define TGPD_GET_NEXT(_pd)		((struct TGPD *)(uintptr_t)((struct TGPD *)_pd)->pNext)

#define TGPD_SET_TBD(_pd, _tbd)	(((struct TGPD *)_pd)->pBuf = (u32)_tbd; TGPD_SET_FORMAT_BDP(_pd))
#define TGPD_GET_TBD(_pd)		((struct TBD *)(uintptr_t)((struct TGPD *)_pd)->pBuf)

#define TGPD_SET_DATA(_pd, _data)	(((struct TGPD *)_pd)->pBuf = (u32)_data)
#define TGPD_GET_DATA(_pd)		((unsigned char *)(uintptr_t)((struct TGPD *)_pd)->pBuf)

#endif

#else

#define TGPD_SET_NEXT(_pd, _next)	(((struct TGPD *)_pd)->pNext = (struct TGPD *)_next)
#define TGPD_GET_NEXT(_pd)		((struct TGPD *)((struct TGPD *)_pd)->pNext)

#define TGPD_SET_TBD(_pd, _tbd)	(((struct TGPD *)_pd)->pBuf = (unsigned char *)_tbd; TGPD_SET_FORMAT_BDP(_pd))
#define TGPD_GET_TBD(_pd)		((struct TBD *)((struct TGPD *)_pd)->pBuf)

#define TGPD_SET_DATA(_pd, _data)	(((struct TGPD *)_pd)->pBuf = (unsigned char *)_data)
#define TGPD_GET_DATA(_pd)		((unsigned char *)((struct TGPD *)_pd)->pBuf)

#endif

#define TGPD_SET_BUF_LEN(_pd, _len)	(((struct TGPD *)_pd)->bufLen = _len)
#define TGPD_ADD_BUF_LEN(_pd, _len)	(((struct TGPD *)_pd)->bufLen += _len)
#define TGPD_GET_BUF_LEN(_pd)	(((struct TGPD *)_pd)->bufLen)
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
#define TGPD_SET_EXT_LEN(_pd, _len)	(((struct TGPD *)_pd)->gpd_B14.ExtLength = _len)
#define TGPD_GET_EXT_LEN(_pd)		(((struct TGPD *)_pd)->gpd_B14.ExtLength)
#else
#define TGPD_SET_EXT_LEN(_pd, _len)	(((struct TGPD *)_pd)->ExtLength = _len)
#define TGPD_GET_EXT_LEN(_pd)		(((struct TGPD *)_pd)->ExtLength)
#endif
#define TGPD_SET_EPaddr(_pd, _EP)		(((struct TGPD *)_pd)->ZTepFlag = \
(((struct TGPD *)_pd)->ZTepFlag&0xF0)|(_EP))
#define TGPD_GET_EPaddr(_pd)		(((struct TGPD *)_pd)->ZTepFlag & 0x0F)
#define TGPD_FORMAT_TGL		0x10
#define TGPD_IS_FORMAT_TGL(_pd)	(((struct TGPD *)_pd)->ZTepFlag & TGPD_FORMAT_TGL)
#define TGPD_SET_FORMAT_TGL(_pd)	(((struct TGPD *)_pd)->ZTepFlag |= TGPD_FORMAT_TGL)
#define TGPD_CLR_FORMAT_TGL(_pd)	(((struct TGPD *)_pd)->ZTepFlag &= (~TGPD_FORMAT_TGL))
#define TGPD_FORMAT_ZLP		0x20
#define TGPD_IS_FORMAT_ZLP(_pd)	(((struct TGPD *)_pd)->ZTepFlag & TGPD_FORMAT_ZLP)
#define TGPD_SET_FORMAT_ZLP(_pd)	(((struct TGPD *)_pd)->ZTepFlag |= TGPD_FORMAT_ZLP)
#define TGPD_CLR_FORMAT_ZLP(_pd)	(((struct TGPD *)_pd)->ZTepFlag &= (~TGPD_FORMAT_ZLP))
#define TGPD_FORMAT_IOC		0x80
#define TGPD_IS_FORMAT_IOC(_pd)	(((struct TGPD *)_pd)->flag & TGPD_FORMAT_IOC)
#define TGPD_SET_FORMAT_IOC(_pd)	(((struct TGPD *)_pd)->flag |= TGPD_FORMAT_IOC)
#define TGPD_CLR_FORMAT_IOC(_pd)	(((struct TGPD *)_pd)->flag &= (~TGPD_FORMAT_IOC))
#define TGPD_SET_TGL(_pd, _TGL)		(((struct TGPD *)_pd)->ZTepFlag |= ((_TGL) ? 0x10 : 0x00))
#define TGPD_GET_TGL(_pd)		(((struct TGPD *)_pd)->ZTepFlag & 0x10 ? 1:0)
#define TGPD_SET_ZLP(_pd, _ZLP)		(((struct TGPD *)_pd)->ZTepFlag |= ((_ZLP) ? 0x20 : 0x00))
#define TGPD_GET_ZLP(_pd)		(((struct TGPD *)_pd)->ZTepFlag & 0x20 ? 1:0)
#define TGPD_GET_EXT(_pd)		((unsigned char *)_pd + sizeof(struct TGPD))


#define TBD_FLAGS_EOL		0x01
#define TBD_IS_FLAGS_EOL(_bd)	(((struct TBD *)_bd)->flag & TBD_FLAGS_EOL)
#define TBD_SET_FLAGS_EOL(_bd)	(((struct TBD *)_bd)->flag |= TBD_FLAGS_EOL)
#define TBD_CLR_FLAGS_EOL(_bd)	(((struct TBD *)_bd)->flag &= (~TBD_FLAGS_EOL))
#define TBD_SET_FLAG(_bd, _flag)	(((struct TBD *)_bd)->flag = (unsigned char)_flag)
#define TBD_GET_FLAG(_bd)		(((struct TBD *)_bd)->flag)
#define TBD_SET_CHKSUM(_pd, _n)	(((struct TBD *)_pd)->chksum = mu3d_hal_cal_checksum((unsigned char *)_pd, _n))
#define TBD_GET_CHKSUM(_pd)		(((struct TBD *)_pd)->chksum)
#define TBD_SET_DataBUF_LEN(_pd, _len)	(((struct TBD *)_pd)->DataBufferLen = _len)
#define TBD_GET_DataBUF_LEN(_pd)	(((struct TBD *)_pd)->DataBufferLen)

#ifdef CONFIG_ARM64

#define TBD_SET_NEXT(_bd, _next)	(((struct TBD *)_bd)->pNext = (u32)_next)
#define TBD_GET_NEXT(_bd)		((struct TBD *)(uintptr_t)((struct TBD *)_bd)->pNext)
#define TBD_SET_DATA(_bd, _data)	(((struct TBD *)_bd)->pBuf = (u32)_data)
#define TBD_GET_DATA(_bd)		((unsigned char *)(uintptr_t)((struct TBD *)_bd)->pBuf)

#else

#define TBD_SET_NEXT(_bd, _next)	(((struct TBD *)_bd)->pNext = (struct TBD *)_next)
#define TBD_GET_NEXT(_bd)		((struct TBD *)((struct TBD *)_bd)->pNext)
#define TBD_SET_DATA(_bd, _data)	(((struct TBD *)_bd)->pBuf = (unsigned char *)_data)
#define TBD_GET_DATA(_bd)		((DEV_UINT8 *)((struct TBD *)_bd)->pBuf)

#endif

#define TBD_SET_BUF_LEN(_bd, _len)	(((struct TBD *)_bd)->bufLen = _len)
#define TBD_ADD_BUF_LEN(_bd, _len)	(((struct TBD *)_bd)->bufLen += _len)
#define TBD_GET_BUF_LEN(_bd)		(((struct TBD *)_bd)->bufLen)
#define TBD_SET_EXT_LEN(_bd, _len)	(((struct TBD *)_bd)->extLen = _len)
#define TBD_ADD_EXT_LEN(_bd, _len)	(((struct TBD *)_bd)->extLen += _len)
#define TBD_GET_EXT_LEN(_bd)		(((struct TBD *)_bd)->extLen)
#define TBD_GET_EXT(_bd)		(((unsigned char *)_bd + sizeof(struct TBD)))



#undef EXTERN

#ifdef _MTK_QMU_DRV_EXT_
#define EXTERN
#else
#define EXTERN \
extern
#endif


EXTERN unsigned char is_bdp;
/* DVT+ */
EXTERN unsigned int gpd_buf_size;
EXTERN unsigned short bd_buf_size;
EXTERN unsigned char bBD_Extension;
EXTERN unsigned char bGPD_Extension;
EXTERN unsigned int g_dma_buffer_size;
/* DVT+ */
EXTERN struct TGPD *Rx_gpd_head[15];
EXTERN struct TGPD *Tx_gpd_head[15];
EXTERN struct TGPD *Rx_gpd_end[15];
EXTERN struct TGPD *Tx_gpd_end[15];
EXTERN struct TGPD *Rx_gpd_last[15];
EXTERN struct TGPD *Tx_gpd_last[15];
EXTERN struct GPD_R Rx_gpd_List[15];
EXTERN struct GPD_R Tx_gpd_List[15];
EXTERN struct BD_R Rx_bd_List[15];
EXTERN struct BD_R Tx_bd_List[15];
EXTERN struct qmu_desc_map rx_gpd_map[15];
EXTERN struct qmu_desc_map tx_gpd_map[15];
EXTERN struct qmu_desc_map rx_bd_map[15];
EXTERN struct qmu_desc_map tx_bd_map[15];


EXTERN void mu3d_hal_resume_qmu(int Q_num, enum USB_DIR dir);
EXTERN void mu3d_hal_stop_qmu(int Q_num, enum USB_DIR dir);
EXTERN struct TGPD *_ex_mu3d_hal_prepare_tx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_length,
					 unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
					 unsigned char ioc, unsigned char bps, unsigned char zlp);
EXTERN struct TGPD *mu3d_hal_prepare_tx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_length,
				     unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
				     unsigned char ioc, unsigned char bps, unsigned char zlp);
EXTERN struct TGPD *_ex_mu3d_hal_prepare_rx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
					 unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
					 unsigned char ioc, unsigned char bps, unsigned int cMaxPacketSize);
EXTERN struct TGPD *mu3d_hal_prepare_rx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
				     unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
				     unsigned char ioc, unsigned char bps, unsigned int cMaxPacketSize);
EXTERN void _ex_mu3d_hal_insert_transfer_gpd(int ep_num, enum USB_DIR dir, dma_addr_t buf,
					     unsigned int count, unsigned char isHWO, unsigned char ioc,
					     unsigned char bps, unsigned char zlp,
					     unsigned int cMaxPacketSize);
EXTERN void mu3d_hal_insert_transfer_gpd(int ep_num, enum USB_DIR dir, dma_addr_t buf,
					 unsigned int count, unsigned char isHWO, unsigned char ioc,
					 unsigned char bps, unsigned char zlp, unsigned int cMaxPacketSize);
EXTERN void _ex_mu3d_hal_alloc_qmu_mem(struct device *dev);
EXTERN void _ex_mu3d_hal_free_qmu_mem(struct device *dev);
EXTERN void mu3d_hal_alloc_qmu_mem(void);
EXTERN void mu3d_hal_free_qmu_mem(void);
EXTERN void _ex_mu3d_hal_init_qmu(void);
EXTERN void mu3d_hal_init_qmu(void);
EXTERN void mu3d_hal_start_qmu(int Q_num, enum USB_DIR dir);
EXTERN void _ex_mu3d_hal_flush_qmu(int Q_num, enum USB_DIR dir);
EXTERN void mu3d_hal_flush_qmu(int Q_num, enum USB_DIR dir);
EXTERN void mu3d_hal_restart_qmu(int Q_num, enum USB_DIR dir);
EXTERN void mu3d_hal_send_stall(int Q_num, enum USB_DIR dir);
EXTERN DEV_UINT8 mu3d_hal_cal_checksum(unsigned char *data, int len);

EXTERN dma_addr_t _ex_mu3d_hal_gpd_virt_to_phys(void *vaddr, enum USB_DIR dir, unsigned int num);
EXTERN dma_addr_t mu3d_hal_gpd_virt_to_phys(void *vaddr, enum USB_DIR dir, unsigned int num);

EXTERN struct TBD *_ex_get_bd(enum USB_DIR dir, unsigned int num);
EXTERN struct TBD *get_bd(enum USB_DIR dir, unsigned int num);

EXTERN dma_addr_t bd_virt_to_phys(void *vaddr, enum USB_DIR dir, unsigned int num);
EXTERN void *bd_phys_to_virt(void *paddr, enum USB_DIR dir, unsigned int num);

EXTERN struct TGPD *get_gpd(enum USB_DIR dir, unsigned int num);

EXTERN void *gpd_phys_to_virt(void *paddr, enum USB_DIR dir, unsigned int num);
EXTERN void gpd_ptr_align(enum USB_DIR dir, unsigned int num, struct TGPD *ptr);

#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
EXTERN bool _ex_mu3d_hal_qmu_status_done(int Q_num, enum USB_DIR dir);
#endif
EXTERN void mu3d_reset_gpd_resource(void);

#undef EXTERN

#endif
