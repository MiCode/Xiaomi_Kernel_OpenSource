/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#ifndef _ISPV3_DEV_H_
#define _ISPV3_DEV_H_

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/spi/spi.h>
#include <linux/msm_pcie.h>

#define MIPORT_MAX_REG_ADDR			0x1000000

#define ISPV3_SPI_SPEED_HZ		(1 * 1000 * 1000)
#define ISPV3_CLK_NUM			(1)
#define ISPV3_NO_SET_RATE			(-1)

#define ISPV3_PINCTRL_STATE_SLEEP	"ispv3_suspend"
#define ISPV3_PINCTRL_STATE_DEFAULT	"ispv3_default"
#define ISPV3_PCI_VENDOR_ID		0x17cd
#define ISPV3_PCI_DEVICE_ID		0x0100
#define ISPV3_BAR_NUM			3
#define ISPV3_PCI_LINK_DOWN		0
#define ISPV3_PCI_LINK_UP		1
#define LINK_TRAINING_RETRY_MAX_TIMES	3
#define PM_OPTIONS_DEFAULT		0
#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define IRQ_TYPE_UNDEFINED		(-1)
#define IRQ_TYPE_LEGACY			0
#define IRQ_TYPE_MSI			1
#define OCRAM_OFFSET			0x700000
#define RPMSG_SIZE			(1024 * 300)

#define BUG_SOF				1

#define ISPV3_SOC_8450_RGLTR_COUNT 7
#define ISPV3_SOC_8475_RGLTR_COUNT 9

enum ispv3_rgltr_type {
	ISPV3_RGLTR_VDD1,
	ISPV3_RGLTR_L12C,
	ISPV3_RGLTR_VDD2,
	ISPV3_RGLTR_VDD,
	ISPV3_RGLTR_VDDR,
	ISPV3_RGLTR_L10C,
	ISPV3_RGLTR_MCLK,
	ISPV3_RGLTR_S11B,
	ISPV3_RGLTR_S12B,
	ISPV3_RGLTR_MAX,
};

enum pci_barno {
	BAR_0,
	BAR_1,
	BAR_2,
	BAR_3,
	BAR_4,
	BAR_5,
};

enum rpmsg_res {
	RPMSG_RAM,
	RPMSG_IRQ,
	RPMSG_RES_NUM,
};

enum ispv3_power_state_type {
	ISPV3_POWER_ON,
	ISPV3_POWER_OFF,
	ISPV3_POWER_MAX,
};

enum cam_res {
	ISP_RAM,
	ISP_DDR,
	CAM_RES_NUM,
};

enum ispv3_interface_type {
	ISPV3_SPI,
	ISPV3_PCIE,
};

struct ispv3_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	uint8_t pinctrl_status;
};

enum ispv3_soc_id {
	ISPV3_SOC_ID_SM8450 = 0,
	ISPV3_SOC_ID_SM8475,
};

struct ispv3_data {
	struct ispv3_pinctrl_info pinctrl_info;
	struct spi_device	*spi;
	struct pci_dev		*pci;
	struct device		*dev;
	void __iomem		*base;
	void 			*rproc;
	struct resource		bar_res[ISPV3_BAR_NUM];
#ifdef BUG_SOF
	atomic_t		power_state;
#else
	enum ispv3_power_state_type power_state;
#endif
	struct pci_saved_state *saved_state;
	struct pci_saved_state *default_state;
	enum ispv3_interface_type interface_type;
	enum ispv3_soc_id soc_id;
	uint32_t		num_rgltr;
	struct regulator	*rgltr[ISPV3_RGLTR_MAX];
	const char		*rgltr_name[ISPV3_RGLTR_MAX];
	uint32_t		rgltr_min_volt[ISPV3_RGLTR_MAX];
	uint32_t		rgltr_max_volt[ISPV3_RGLTR_MAX];
	uint32_t		rgltr_op_mode[ISPV3_RGLTR_MAX];
	const char		*clk_name;
	struct clk		*clk;
	int32_t			clk_rate;
	int32_t			gpio_sys_reset;
	int32_t			gpio_isolation;
	int32_t			gpio_swcr_reset;
	int32_t			gpio_int0;
	int32_t			gpio_int1;
	int32_t			gpio_fan_en;
	int32_t			irq_num;
	int32_t			rc_index;
	int32_t			pci_irq_type;
	uint32_t		gpio_irq_cam;
	uint32_t		gpio_irq_power;
	atomic_t		pci_link_state;
	struct mutex		ispv3_interf_mutex;
	int                     (*remote_callback)(struct ispv3_data *pdata);
};

void ispv3_gpio_reset_clear(struct ispv3_data *data);
int ispv3_power_on(struct ispv3_data *data);
int ispv3_power_off(struct ispv3_data *data);
void ispv3_write_reset(struct ispv3_data *data, u32 value);
void ispv3_write_reset1(struct ispv3_data *data, u32 value);
void ispv3_write_isolation(struct ispv3_data *data, u32 value);
int ispv3_resume_pci_link(struct ispv3_data *data);
int ispv3_suspend_pci_link(struct ispv3_data *data);

#endif

