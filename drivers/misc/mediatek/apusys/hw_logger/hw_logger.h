/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __HW_LOGGER_H__
#define __HW_LOGGER_H__

#include "apu.h"
#include "apu_config.h"


#define APU_LOG_DATA         (apu_logd)

#define APU_LOGTOP_BASE      (apu_logtop)
#define APU_LOGTOP_CON       (APU_LOGTOP_BASE + 0x0)
#define APU_LOG_MST_UPP_ID0  (APU_LOGTOP_BASE + 0x10)
#define APU_LOG_MST_UPP_ID1  (APU_LOGTOP_BASE + 0x14)
#define APU_LOG_MST_UPP_ID2  (APU_LOGTOP_BASE + 0x18)
#define APU_LOG_MST_UPP_ID3  (APU_LOGTOP_BASE + 0x1C)
#define APU_LOG_MST_LOW_ID0  (APU_LOGTOP_BASE + 0x20)
#define APU_LOG_MST_LOW_ID1  (APU_LOGTOP_BASE + 0x24)
#define APU_LOG_MST_LOW_ID2  (APU_LOGTOP_BASE + 0x28)
#define APU_LOG_MST_LOW_ID3  (APU_LOGTOP_BASE + 0x2C)
#define APU_LOG_MST_ENA      (APU_LOGTOP_BASE + 0x40)
#define APU_LOG_MST_ENA_HW   (APU_LOGTOP_BASE + 0x44)
#define APU_LOG_MST_SEL      (APU_LOGTOP_BASE + 0x48)
#define APU_LOG_INBUF_AW_ST  (APU_LOGTOP_BASE + 0x50)
#define APU_LOG_INBUF_W_ST   (APU_LOGTOP_BASE + 0x54)
#define APU_LOG_INBUF_B_ST   (APU_LOGTOP_BASE + 0x58)
#define APU_LOG_BST_FIFO_ST  (APU_LOGTOP_BASE + 0x5C)
#define APU_LOG_IDC_ST       (APU_LOGTOP_BASE + 0x60)
#define APU_LOG_BUF_ADDR_INI (APU_LOGTOP_BASE + 0x70)
#define APU_LOG_BUF_ST_ADDR  (APU_LOGTOP_BASE + 0x74)
#define APU_LOG_BUF_T_SIZE   (APU_LOGTOP_BASE + 0x78)
#define APU_LOG_AXI_SIDEBAND (APU_LOGTOP_BASE + 0x7C)
#define APU_LOG_BUF_W_PTR    (APU_LOGTOP_BASE + 0x80)
#define APU_LOG_BUF_R_PTR    (APU_LOGTOP_BASE + 0x84)
#define APU_LOG_LBC_SIZE     (APU_LOGTOP_BASE + 0x90)
#define APU_LOG_LBC_STATUS   (APU_LOGTOP_BASE + 0x94)
#define APU_LOG_WB_FIFO_ST0  (APU_LOGTOP_BASE + 0xC0)
#define APU_LOG_WB_FIFO_ST1  (APU_LOGTOP_BASE + 0xC4)
#define APU_LOG_WB_FIFO_ST2  (APU_LOGTOP_BASE + 0xC8)
#define APU_LOG_WB_FIFO_ST3  (APU_LOGTOP_BASE + 0xCC)
#define APU_LOG_WB_FIFO_ST4  (APU_LOGTOP_BASE + 0xD0)
#define APU_LOG_WB_FIFO_ST5  (APU_LOGTOP_BASE + 0xD4)
#define APU_LOG_WB_FIFO_ST6  (APU_LOGTOP_BASE + 0xD8)
#define APU_LOG_WB_FIFO_ST7  (APU_LOGTOP_BASE + 0xDC)



#define APU_LOGTOP_CON_FLAG_ADDR             (APU_LOGTOP_CON)
#define APU_LOGTOP_CON_FLAG_SHIFT            (8)
#define APU_LOGTOP_CON_FLAG_MASK             (0xF << \
	APU_LOGTOP_CON_FLAG_SHIFT)

/* bit[11:8] */
#define LBC_FULL_FLAG (0x1 << 0)
#define LBC_ERR_FLAG  (0x1 << 1)
#define OVWRITE_FLAG  (0x1 << 2)
#define LOCKBUS_FLAG  (0x1 << 3)

#define APU_LOGTOP_CON_ST_ADDR_HI_ADDR       (APU_LOGTOP_CON)
#define APU_LOGTOP_CON_ST_ADDR_HI_SHIFT      (4)
#define APU_LOGTOP_CON_ST_ADDR_HI_MASK       (0x3 << \
	APU_LOGTOP_CON_ST_ADDR_HI_SHIFT)

#define APU_LOGTOP_CON_LOCKBUS_IRQ_EN_SHIFT  (3)
#define APU_LOGTOP_CON_LOCKBUS_IRQ_EN_MASK   (0x1 << \
	APU_LOGTOP_CON_LOCKBUS_IRQ_EN_SHIFT)

#define APU_LOGTOP_CON_OVRWR_IRQ_EN_SHIFT    (2)
#define APU_LOGTOP_CON_OVRWR_IRQ_EN_MASK     (0x1 << \
	APU_LOGTOP_CON_OVRWR_IRQ_EN_SHIFT)

#define APU_LOG_MST_ID_MASK                  (0x1FF)
#define APU_LOG_MST_ID_DEFAULT               (0x1FF)
#define APU_LOG_MST_ID_RCX_UP                (0x0)

#define APU_LOG_MST_ENA_HW_ST_EN_ALL_SHIFT   (0)
#define APU_LOG_MST_ENA_HW_ST_EN_ALL_MASK    (0x7F << \
	APU_LOG_MST_ENA_HW_ST_EN_ALL_SHIFT)

#define APU_LOG_IDC_ST_NAXIS_SHIFT           (31)
#define APU_LOG_IDC_ST_NAXIS_MASK            (0x1 << \
	APU_LOG_IDC_ST_NAXIS_SHIFT)

#define APU_LOG_IDC_ST_AXIS_SHIFT            (29)
#define APU_LOG_IDC_ST_AXIS_MASK             (0x1 << \
	APU_LOG_IDC_ST_AXIS_SHIFT)

#define APU_LOG_BUF_ADDR_INI_RST_ALL_SHIFT   (4)
#define APU_LOG_BUF_ADDR_INI_RST_ALL_MASK    (0x7 << \
	APU_LOG_BUF_ADDR_INI_RST_ALL_SHIFT)

#define APU_LOG_WB_FIFO_STX_WR_PTR_SHIFT     (16)
#define APU_LOG_WB_FIFO_STX_WR_PTR_MASK      (0x3F << \
	APU_LOG_WB_FIFO_STX_WR_PTR_SHIFT)

#define GET_MASK_BITS(x) ((ioread32(x##_ADDR) & \
	x##_MASK) >> x##_SHIFT)

#define SET_MASK_BITS(x, y) iowrite32(((ioread32(y##_ADDR) & \
	~y##_MASK) | ((x << y##_SHIFT) & y##_MASK)), y##_ADDR)

#define HWLOGR_PREFIX "[apusys_hwlogger]"

#define HWLOG_LINE_MAX_LENS 128

enum {
	DBG_LOG_WARN,
	DBG_LOG_INFO,
	DBG_LOG_DEBUG,
};

#define HWLOGR_ERR(x, args...) \
	pr_info(HWLOGR_PREFIX "[error] %s " x, __func__, ##args)
#define HWLOGR_WARN(x, args...) \
	pr_info(HWLOGR_PREFIX "[warn] %s " x, __func__, ##args)
#define HWLOGR_INFO(x, args...) \
	{ \
		if (g_hw_logger_log_lv >= DBG_LOG_INFO) \
			pr_info(HWLOGR_PREFIX "%s " \
			x, __func__, ##args); \
	}
#define HWLOGR_DBG(x, args...) \
	{ \
		if (g_hw_logger_log_lv >= DBG_LOG_DEBUG) \
			pr_info(HWLOGR_PREFIX "[debug] %s/%d " \
			x, __func__, __LINE__, ##args); \
	}

/* print to console via seq file */
#define DBG_HWLOG_CON(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			HWLOGR_INFO(x, ##args); \
	}

#define DBG_ON() (g_hw_logger_log_lv >= DBG_LOG_DEBUG)

#define hwlogr_hex_dump(prefix_str, buf, len) \
	print_hex_dump(KERN_ERR, prefix_str, DUMP_PREFIX_OFFSET, 16, 1, buf, len, false)

#define APUSYS_HWLOGR_DIR      "apusys_logger"
#define APUSYS_HWLOGR_AEE_DIR  "apusys_debug"
#define HWLOGR_LOG_SIZE        (1024 * 1024)
#define LOCAL_LOG_SIZE         (1024 * 1024)

#define IPI_DEBUG_LEVEL 4

/* #define hw_logger_DEBUG */

int hw_logger_config_init(struct mtk_apu *apu);
int hw_logger_ipi_init(struct mtk_apu *apu);
void hw_logger_ipi_remove(struct mtk_apu *apu);

int hw_logger_copy_buf(void);
int hw_logger_deep_idle_enter_pre(void);
int hw_logger_deep_idle_enter_post(void);
int hw_logger_deep_idle_leave(void);

#endif /* __HW_LOGGER_H__ */
