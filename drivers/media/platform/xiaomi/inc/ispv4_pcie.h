/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#ifndef _ISPV4_DEV_H_
#define _ISPV4_DEV_H_

#include "linux/spinlock_types.h"
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
#define PCIE_I_ATU_NUM_REGIONS		8

#define ISPV4_SPI_SPEED_HZ		(1 * 1000 * 1000)
#define ISPV4_CLK_NUM			(1)
#define ISPV4_NO_SET_RATE		(-1)

#define ISPV4_PINCTRL_STATE_SLEEP	"ispv4_suspend"
#define ISPV4_PINCTRL_STATE_DEFAULT	"ispv4_default"
#define ISPV4_PCI_VENDOR_ID		0x16c3
#define ISPV4_PCI_DEVICE_ID		0xabcd
#define ISPV4_BAR_NUM			6
#define ISPV4_PCI_LINK_DOWN		0
#define ISPV4_PCI_LINK_UP		1
#define PM_OPTIONS_DEFAULT		0
#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define IRQ_TYPE_UNDEFINED		(-1)
#define IRQ_TYPE_LEGACY			0
#define IRQ_TYPE_MSI			1
#define OCRAM_OFFSET			0x0
#define RPROC_SIZE			(4 * 1024 * 1024)
#define RPROC_ATTACH_OFFSET		0xD400000
#define RPROC_ATTACH_SIZE		(1 * 1024)
#define MAILBOX_OFFSET			0x2150000
#define MAILBOX_SIZE			0x4000
#define TIMER_OFFSET            0x2168000
#define TIMER_SIZE              0x100

#define BUG_SOF				1


#define TIMER64_LDCNT_LO    0x00
#define TIMER64_LDCNT_HI    0x04
#define TIMER64_LDCNT_EN    0x18

#define LDREG_TIM64_ENABLE  0x01
#define TIMER64_RDCNT_EN    0x1c

#define TIMER64_EN          0x10
#define TIMER64_ENABLE      0x01

#define RDREG_TIM64_ENABLE  0x01
#define TIMER64_CURVAL_LO   0x08
#define TIMER64_CURVAL_HI   0x0c

#define ISPV4_TIMER64_FREQ  133312496
#define NSEC_PER_SEC_S      1000000000L



enum ispv4_rgltr_type {
	ISPV4_RGLTR_VDD1,
	ISPV4_RGLTR_L12C,
	ISPV4_RGLTR_VDD2,
	ISPV4_RGLTR_VDD,
	ISPV4_RGLTR_VDDR,
	ISPV4_RGLTR_L10C,
	ISPV4_RGLTR_MCLK,
	ISPV4_RGLTR_MAX,
};

enum pci_barno {
	BAR_0,
	BAR_1,
	BAR_2,
	BAR_3,
	BAR_4,
	BAR_5,
};

enum mailbox_res {
	MAILBOX_REG,
	MAILBOX_IRQ,
	MAILBOX_INTC,
	MAILBOX_RES_NUM,
};

enum rpmsg_res {
	RPROC_RAM,
	RPROC_DDR,
	RPROC_ATTACH,
	RPROC_IPC_IRQ,
	RPROC_CRASH_IRQ,
	RPROC_RES_NUM,
};

enum ispv4_power_state_type {
	ISPV4_POWER_ON,
	ISPV4_POWER_OFF,
	ISPV4_POWER_MAX,
};

enum cam_res {
	ISP_RAM,
	ISP_DDR,
	CAM_RES_NUM,
};

enum memdump_res {
	ISPV4_DUMP_MEM,
	MEMDUMP_RES_NUM,
};

enum timealign_res{
    ISPV4_TIMEALIGN_REG,
	TIMEALIGN_RES_NUM,
};
enum ispv4_interface_type {
	ISPV4_SPI,
	ISPV4_PCIE,
};

struct ispv4_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	uint8_t pinctrl_status;
};

enum register_type {
	REG_MATRIX1,
	REG_APB,
	REG_AHB,
	REG_MATRIX3,
	REG_NPU_SMEM,
	REG_NPU_CFG,
	REG_ISP_CFG,
	REG_MATRIX4,
	REG_MATRIX5,
	REG_MATRIX2,
	REG_PCIE_CFG,
	REG_MAX,
};

enum iatu_dir {
	IATU_OUTBOUND = 0,
	IATU_INBOUND,
	IATU_NUM_DIR,
};

struct iatu_region_ctrl {
	spinlock_t region_lock;
	uint8_t region_id;
	bool inuse;
	enum iatu_dir dir;
	const char *client;
};

struct iatu {
	struct iatu_region_ctrl		region_ctrls[IATU_NUM_DIR][PCIE_I_ATU_NUM_REGIONS];
	struct pci_dev			*pci_dev;
	void __iomem			*base;
	enum register_type		reg_type;
	enum register_type		last_reg_type;
	spinlock_t			lock;
};

struct ispv4_data {
	struct iatu *iatu;
	struct iatu_region_ctrl *region_ctrl;
	struct ispv4_pinctrl_info pinctrl_info;
	struct spi_device	*spi;
	struct pci_dev		*pci;
	struct device		*dev;
	void __iomem		*base;
	void __iomem		*base_bar[4];
	void __iomem    *debug_bar;
	void __iomem		*timer_reg_base;
	struct spinlock		timer_lock;
	void			*rproc;
	struct resource bar_res[ISPV4_BAR_NUM];
	struct pci_saved_state *saved_state;
	struct pci_saved_state *default_state;
	int32_t			rc_index;
	int32_t pci_irq_type;
	uint8_t pci_link_state;

	struct msm_pcie_register_event msm_pci_event;
	struct work_struct linkdown_work;
	struct pcie_hdma *pdma;
	struct device comp_dev;
};

void ispv4_gpio_reset_clear(struct ispv4_data *data);
int ispv4_power_on(struct ispv4_data *data);
int ispv4_power_off(struct ispv4_data *data);
void ispv4_write_reset(struct ispv4_data *data, u32 value);
void ispv4_write_reset1(struct ispv4_data *data, u32 value);
void ispv4_write_isolation(struct ispv4_data *data, u32 value);
int ispv4_resume_pci_link(struct ispv4_data *data);
int ispv4_suspend_pci_link(struct ispv4_data *data);
inline uint32_t ispv4_reg_read_by_pcie(struct ispv4_data *priv,
				       uint32_t reg_addr);
inline void ispv4_reg_write_by_pcie(struct ispv4_data *priv,
				    uint32_t reg_addr, uint32_t val);

inline void _pci_reset(void);
#endif
