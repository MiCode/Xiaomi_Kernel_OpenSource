/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISPV3_DEV_H_
#define _CAM_ISPV3_DEV_H_

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <linux/spinlock.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/ispv3_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_subdev.h>
#include "cam_debug_util.h"
#include "cam_context.h"
#include "cam_soc_util.h"
#include <linux/mfd/ispv3_dev.h>

#define CAMX_ISPV3_DEV_NAME		"cam-ispv3-driver"
#define DRV_NAME			"ispv3-v4l2-subdev"

#define FRENQUENCY_400MHZ        10
#define FRENQUENCY_533MHZ        11
#define FRENQUENCY_600MHZ        12
#define FRENQUENCY_667MHZ        13
#define FRENQUENCY_733MHZ        14
#define FRENQUENCY_800MHZ        15
#define FRENQUENCY_933MHZ        16
#define FRENQUENCY_1066MHZ       17

#define ISPV3_FW_ADDR_START		0x7f700000
#define ISPV3_FW_ADDR_END		0x7f74b000

#define ISPV3_SYS_CONFIG_ADDR		0xffef0118

/* ISPV3 AIO timing registers */
#define ISPV3_GEN_CLK_ADDR		0xffe80120
#define ISPV3_AIO_TIMING0_ADDR		0xffe40004
#define ISPV3_AIO_TIMING1_ADDR		0xffe40008
#define ISPV3_AIO_TIMING2_ADDR		0xffe4000c
#define ISPV3_AIO_TIMING3_ADDR		0xffe40010
#define ISPV3_AIO_ENABLE_ADDR		0xffe40050

#define ISPV3_AIO_BOOT_TIMING0_ADDR	0xffe40054
#define ISPV3_AIO_BOOT_TIMING1_ADDR	0xffe40058
#define ISPV3_AIO_BOOT_TIMING2_ADDR	0xffe4005c
#define ISPV3_AIO_BOOT_TIMING3_ADDR	0xffe40060
#define ISPV3_AIO_BOOT_TIMING4_ADDR	0xffe40064

#define ISPV3_SRAM_SLEEP_ADDR		0xffee0014
#define ISPV3_SRAM_DISABLE_ADDR		0xffee0018

/* ISPV3 gpio function select, gpio ap_int0 and gpio ap_int1 */
#define ISPV3_GPIO_AP_INT0_ADDR		0xffea0700
#define ISPV3_GPIO_AP_INT0_VAL		0x00100a01
#define ISPV3_GPIO_AP_INT1_ADDR		0xffea0704
#define ISPV3_GPIO_AP_INT1_VAL		0x00100a01

/* ISPV3 internal interrupt source assign */
#define ISPV3_INT_SOURCE_ASSIGN_ADDR    0xffef0604
#define ISPV3_INT_SW_SOURCE_ASSIGN(val)	(val << 19)
/* Setting SWINT to AP_int_1 gpio, pipe INT chip default assign to AP_int_0 */
#define ISPV3_INT_SW_SOURCE1		(1)
/* Setting SWINT to AP_int_0 gpio, pipe INT chip default assign to AP_int_1 */
#define ISPV3_INT_SW_SOURCE0		(0)

/* SW INT ENABLE register bank */
#define ISPV3_SWINT_ENABLE_ADDR		0xffef0024
#define ISPV3_SWINT_ENABLE_VALUE(val)	(val | 0x1)

/* PCIe DMA trigger register bank */
#define ISPV3_SW_TRIGGER_CPU_ADDR	0xffef030c
#define ISPV3_SW_TRIGGER_CPU_VAL	BIT(0)
#define ISPV3_SW_DATA_CPU_ADDR		0xffef0308
#define ISPV3_SW_DATA_CPU_INIT_OFF	BIT(0)
#define ISPV3_SW_DATA_CPU_TURN_ON	BIT(1)
#define ISPV3_SW_DATA_CPU_TURN_OFF	BIT(2)
#define ISPV3_SW_DATA_CPU_MISN_CFG	BIT(4)
#define ISPV3_SW_DATA_CP_MODE_TEST	BIT(27)
#define ISPV3_SW_DATA_WDT_TEST		BIT(28)
#define ISPV3_SW_DATA_DDR_FULL_RUN	BIT(29)
#define ISPV3_SW_DATA_DDR_QUICK_SCAN	BIT(30)
#define ISPV3_SW_DATA_DO_DDR_INIT	BIT(31)

#define ISPV3_SW_DATA_AP_ADDR		0xffef0310
#define ISPV3_SW_DATA_TURN_ON_CPU	BIT(0)
#define ISPV3_SW_DATA_TURN_OFF_CPU	BIT(1)
#define ISPV3_SW_DATA_MISN_END		BIT(2)

#define ISPV3_AP_INT_ADDR		0xffef0030
#define ISPV3_AP_IDD_INT_EN		BIT(0)
#define ISPV3_AP_RLB_INT_EN		BIT(1)
#define ISPV3_AP_TXLM_MUX_INT_EN	BIT(5)
#define ISPV3_AP_TXLM_A0_INT_EN		BIT(7)
#define ISPV3_AP_TXLM_B0_INT_EN		BIT(8)
#define ISPV3_AP_TXLM_D0_INT_EN		BIT(10)
#define ISPV3_AP_SOF_INT_EN		BIT(18)

#define ISPV3_INT_MISN_EN_AP_ADDR	0xffef00d8
#define ISPV3_INT_MISN_EN_TO_AP		BIT(0)
#define ISPV3_INT_MISN_STATUS_ADDR	0xffef00dc

#define ISPV3_INT_ENABLE_AP_ADDR	0xffef0104
#define ISPV3_INT_ENABLE_SW_TO_AP	BIT(0)

#define ISPV3_INT_STATUS_ADDR		0xffef0108
#define ISPV3_INT_STATUS_SW_INT		BIT(0)

#define ISPV3_TXLM_MUX_INT_ST_ADDR	0xffecb304
#define ISPV3_TXLM_MUX_INT_ST_SOF	BIT(20)

#define ISPV3_TXLM_A0_INT_ST_ADDR	0xffec1200
#define ISPV3_TXLM_B0_INT_ST_ADDR	0xffec2200
#define ISPV3_TXLM_INT_ST_SOF		BIT(7)
#define ISPV3_TXLM_INT_ST_EOF_ALL	BIT(18)

#define ISPV3_IDD_INT_ST_ADDR		0xfff00180
#define ISPV3_IDD_INT_ST_SOF		BIT(2)
#define ISPV3_IDD_INT_ST_EOF		BIT(4)

#define ISPV3_RLB_A0_INT_ST_ADDR	0xfff0206c
#define ISPV3_RLB_A0_INT_ST_EOF		BIT(1)
#define ISPV3_RLB_A0_INT_ST_SOF		BIT(16)

#define ISPV3_FRAME_INT_ST_ADDR		0xfff99568
#define ISPV3_FRAME_INT_ST_CH1_SOF	BIT(3)

#define ISPV3_MISN_MOD_STATUS_ADDR	0xff080010
#define ISPV3_MISN_MOD_INT_LONG_EXP	BIT(0)
#define ISPV3_MISN_MOD_INT_MIDD_EXP	BIT(3)
#define ISPV3_MISN_MOD_INT_LONG_LSC	BIT(28)
#define ISPV3_MITOP_MOD_STATUS_ADDR	0xff1c0040
#define ISPV3_MITOP_MOD_INT_MISN	BIT(30)

#define ISPV3_CPU_PD_ADDR0		0xffef0304
#define ISPV3_CPU_PD_ADDR1		0xffef0300
#define ISPV3_CPU_PD_CTL_ADDR		0xffe82000
#define ISPV3_INTERNAL_SRAM_CTL_ADDR	0xffef010c
#define ISPV3_CPU_CLK_CTL_ADDR		0xffe80000
#define ISPV3_CPU_RESET_CTL_ADDR	0xffe80200
#define ISPV3_ISO_RELEASE_ADDR		0xffef0000

#define ISPV3_MISN_PR_PARA_LONG_ISO	0xFF080014
#define ISPV3_MISN_PR_PARA_LONG_ET	0xFF080018
#define ISPV3_MISN_PR_PARA_LONG_RGAIN   0xFF08001c
#define ISPV3_MISN_PR_PARA_LONG_BGAIN	0xFF080020
#define ISPV3_MISN_PR_PARA_LONG_G0GAIN	0xFF080024
#define ISPV3_MISN_PR_PARA_LONG_G1GAIN  0xFF080028

#define ISPV3_MISN_PR_PARA_SAFE_ISO    0xFF08003c
#define ISPV3_MISN_PR_PARA_SAFE_ET	0xFF080040
#define ISPV3_MISN_PR_PARA_SAFE_RGAIN   0xFF080044
#define ISPV3_MISN_PR_PARA_SAFE_BGAIN   0xFF080048
#define ISPV3_MISN_PR_PARA_SAFE_G0GAIN  0xFF08004c
#define ISPV3_MISN_PR_PARA_SAFE_G1GAIN  0xFF080050

#define ISPV3_MISN_GROUP_EN 0xFF080098

#define ISPV3_MISN_PR_PARA_LONG_RBBLACKPOINT       0xFF080034
#define ISPV3_MISN_PR_PARA_LONG_G0G1BLACKPOINT     0xFF080038

#define ISPV3_MISN_PR_PARA_SAFE_RBBLACKPOINT      0xFF08005c
#define ISPV3_MISN_PR_PARA_SAFE_G0G1BLACKPOINT    0xFF080060

#define ISPV3_MISN_PR_PARA_TARGET_OPT_K0          0xFF0B000C
#define ISPV3_MISN_PR_PARA_TARGET_OPT_K1          0xFF0B0010
#define ISPV3_MISN_PR_PARA_TARGET_OPT_K2          0xFF0B0014

#define ISPV3_PCIE_L1_MODE_ADDR		0xFFFB00D0

#define ISPV3_SPI_OP_WRITE_SIZE		5
#define ISPV3_SPI_OP_READ_SIZE		21
#define ISPV3_SPI_RAM_WRITE_OP		0x05
#define ISPV3_SPI_RAM_READ_OP		0x15
#define ISPV3_SPI_REG_WRITE_OP		0x09
#define ISPV3_SPI_REG_READ_OP		0x19

#define EXP_INDEX_SHORT			0
#define EXP_INDEX_LONG			1
#define EXP_INDEX_SAFE			2
#define EXP_INDEX_COUNT			3

enum cam_ispv3_irq_type {
	ISPV3_SW_IRQ,
	ISPV3_HW_IRQ,
};

struct ispv3_image_device {
	struct video_device *video;
	struct ispv3_data *pdata;
	void __iomem *base[CAM_RES_NUM];
	struct device *dev;
	struct completion comp_off;
	struct completion comp_on;
	struct completion comp_setup;
	struct work_struct work;
	enum cam_ispv3_irq_type irq_type;
	uint32_t boot_addr;
};

enum cam_ispv3_state_t {
	CAM_ISPV3_INIT,
	CAM_ISPV3_ACQUIRE,
	CAM_ISPV3_CONFIG,
	CAM_ISPV3_START,
};

/**
 * struct intf_params
 * @device_hdl: Device Handle
 * @session_hdl: Session Handle
 * @link_hdl: Link Handle
 * @ops: KMD operations
 * @crm_cb: Callback API pointers
 */
struct intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

struct ispv3_cmd_set {
	uint32_t awb_ctl_r_gain;
	uint32_t awb_ctl_g_gain;
	uint32_t awb_ctl_b_gain;
	uint32_t aec_linear_gain[EXP_INDEX_COUNT];
	uint32_t aec_exposure_time[EXP_INDEX_COUNT];
	uint32_t aec_r_black_point[EXP_INDEX_COUNT];
	uint32_t aec_b_black_point[EXP_INDEX_COUNT];
	uint32_t aec_g0_black_point[EXP_INDEX_COUNT];
	uint32_t aec_g1_black_point[EXP_INDEX_COUNT];
	uint32_t aec_targetopt_kx[EXP_INDEX_COUNT];
	uint32_t aec_iso[EXP_INDEX_COUNT];
	uint32_t aec_settled;
};

struct cam_ispv3_ctrl_t {
	char device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct platform_device *pdev;
	struct cam_hw_soc_info soc_info;
	struct mutex cam_ispv3_mutex;
	enum cam_ispv3_state_t ispv3_state;
	struct device_node *of_node;
	struct cam_subdev v4l2_dev_str;
	struct intf_params bridge_intf;
	uint16_t pipeline_delay;
	struct ispv3_cmd_set per_frame[MAX_PER_FRAME_ARRAY];
	struct ispv3_image_device *priv;
	uint32_t id;
	enum cam_req_mgr_trigger_source     trigger_source;
	uint64_t frame_count;
	uint64_t req_id;
	uint64_t frame_id;
	bool is4K;
	bool stop_notify_crm;
};

inline int cam_ispv3_reg_read(struct ispv3_image_device *priv,
			      uint32_t reg_addr, uint32_t *reg_data);
inline int cam_ispv3_reg_write(struct ispv3_image_device *priv,
			       uint32_t reg_addr, uint32_t reg_data);
int cam_ispv3_read(struct ispv3_image_device *priv,
		   struct cam_control *cmd);
int cam_ispv3_write(struct ispv3_image_device *priv,
		    struct cam_control *cmd);
int cam_ispv3_spi_change_speed(struct ispv3_image_device *priv,
			       struct cam_control *cmd);
int cam_ispv3_turn_on(struct cam_ispv3_ctrl_t *s_ctrl, struct ispv3_image_device *priv, uint64_t DFScmd_value);
int cam_ispv3_turn_off(struct ispv3_image_device *priv);
int cam_ispv3_driver_init(void);
void cam_ispv3_driver_exit(void);
void cam_ispv3_notify_message(struct cam_ispv3_ctrl_t *s_ctrl,
			      uint32_t id, uint32_t type);
#endif /* _CAM_ISPV3_DEV_H_ */
