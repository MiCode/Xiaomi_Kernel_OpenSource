/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/tsc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>        /* Device drivers need this */
#include <linux/cdev.h>		/* Char device drivers need that */
#include <linux/kernel.h>        /* for KERN_INFO */
#include <linux/fs.h>
#include <linux/completion.h>	/* for completion signaling after interrupts */
#include <linux/uaccess.h>	/* for copy from/to user in the ioctls */
#include <linux/msm_iommu_domains.h>
#include <linux/mutex.h>
#include <linux/of.h>		/* parsing device tree data */
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <mach/gpio.h>		/* gpios definitions */
#include <linux/pinctrl/consumer.h> /* pinctrl API */
#include <linux/clk.h>
#include <linux/wait.h>          /* wait() macros, sleeping */
#include <linux/sched.h>         /* Externally defined globals */
#include <linux/poll.h>          /* poll() file op */
#include <linux/io.h>            /* IO macros */
#include <linux/bitops.h>
#include <linux/msm_ion.h>	/* ion_map_iommu */
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/slab.h>          /* kfree, kzalloc */
#include <linux/debugfs.h>	/* debugfs support */
#include <linux/pm_runtime.h>	/* debugfs support */
#include <linux/pm_wakeup.h>	/* debugfs support */
#include <linux/regulator/consumer.h> /* gdsc */
#include <mach/msm_bus.h>	/* bus client */
#include <linux/delay.h>	/* usleep function */
/* TODO: include <linux/mpq_standby_if.h> after MCU is mainlined */

/*
 * General defines
 */
#define TEST_BIT(pos, number) (number & (1 << pos))
#define CLEAR_BIT(pos, number) (number &= ~(1 << pos))
#define SET_BIT(pos, number) (number |= 1 << pos)

/*
 * extract bits [@b0:@b1] (inclusive) from the value @x
 * it should be @b0 <= @b1, or result is incorrect
 */
static inline u32 GETL_BITS(u32 x, int b0, int b1)
{
	return (x >> b0) & ((1 << (b1 - b0 + 1)) - 1);
}

/* Bypass VBIF/IOMMU for debug and bring-up purposes */
static int tsc_iommu_bypass; /* defualt=0 using iommu */
module_param(tsc_iommu_bypass, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* The rate of the clock that control TS from TSC to the CAM */
#define CICAM_CLK_RATE_12MHZ		12000000
#define CICAM_CLK_RATE_9MHZ		8971962
#define CICAM_CLK_RATE_7MHZ		7218045
/* Rates for TSC serial and parallel clocks */
#define TSC_SER_CLK_RATE		192000000
#define TSC_PAR_CLK_RATE		24000000

/* CICAM address space according to CI specification */
#define CICAM_MAX_ADDRESS		3

/*
 * TSC register offsets
 */
#define TSC_HW_VERSION			(0x0)
#define TSC_MUX_CFG			(0x4)	/* Muxs config */
#define TSC_IN_IFC_EXT			(0x8)	/* External demods tsifs */
#define TSC_IN_IFC_CFG_INT		(0xc)	/* internal demods and
						cicam tsif config */
#define TSC_FSM_STATE			(0x50)	/* Read FSM state */
#define TSC_FSM_STATE_MASK		(0x54)	/* Config FSM state */
#define TSC_CAM_CMD			(0x1000)/* Config cam commands */
#define TSC_CAM_RD_DATA			(0x1004)/* read data for single-mode
							byte */
#define TSC_STAT			(0x1008)/* Interrupts status */
#define TSC_IRQ_ENA			(0x100C)/* Enable interrupts */
#define TSC_IRQ_CLR			(0x1010)/* Clear interrupts */
#define TSC_CIP_CFG			(0x1014)/* Enable HW polling */
#define TSC_CD_STAT			(0x1020)/* Card pins status */
#define TSC_RD_BUFF_ADDR		(0x1024)/* Vbif address for read
						buffer */
#define TSC_WR_BUFF_ADDR		(0x1028)/* Vbif address for write
						buffer */
#define TSC_FALSE_CD			(0x102C)/* Counter of false card
						detection */
#define TSC_FALSE_CD_CLR		(0x1030)/* Clear false cd counter */
#define TSC_RESP_ERR			(0x1034)/* State of read/write buffer
						error */
#define TSC_CICAM_TSIF			(0x1038)/* Enable tsif (tsc->cam) */


/*
 * Registers structure definitions
 */

/* TSC_MUX_CFG */
#define MUX_EXTERNAL_DEMOD_0			0
#define MUX_EXTERNAL_DEMOD_1			1
#define MUX_INTERNAL_DEMOD			2
#define MUX_CICAM				3
#define MUX0_OFFS				0
#define MUX1_OFFS				2
#define MUX_CAM_OFFS				4

/* TSC_IN_IFC_EXT and TSC_IN_IFC_CFG_INT*/
#define TSIF_INPUT_ENABLE			0
#define TSIF_INPUT_DISABLE			1

#define TSIF_CLK_POL_OFFS			0
#define TSIF_DATA_POL_OFFS			1
#define TSIF_START_POL_OFFS			2
#define TSIF_VALID_POL_OFFS			3
#define TSIF_ERROR_POL_OFFS			4
#define TSIF_SER_PAR_OFFS			5
#define TSIF_REC_MODE_OFFS			6
#define TSIF_DATA_SWAP_OFFS			8
#define TSIF_DISABLE_OFFS			9
#define TSIF_ERR_INSERT_OFFS			10

/* TSC_FSM_STATE and TSC_FSM_STATE_MASK*/
#define FSM_STATE_BUFFER_BEG			0
#define FSM_STATE_BUFFER_END			3
#define FSM_STATE_POLL_BEG			8
#define FSM_STATE_POLL_END			10
#define FSM_STATE_BYTE_BEG			12
#define FSM_STATE_BYTE_END			13
#define FSM_STATE_MEM_WR_BEG			16
#define FSM_STATE_MEM_WR_END			17
#define FSM_STATE_MEM_RD_BEG			20
#define FSM_STATE_MEM_RD_END			21
#define FSM_STATE_IO_RD_BEG			24
#define FSM_STATE_IO_RD_END			25
#define FSM_STATE_IO_WR_BEG			28
#define FSM_STATE_IO_WR_END			29

/* TSC_CAM_CMD */
#define MEMORY_TRANSACTION			0
#define IO_TRANSACTION				1
#define WRITE_TRANSACTION			0
#define READ_TRANSACTION			1
#define SINGLE_BYTE_MODE			0
#define BUFFER_MODE				1

#define CAM_CMD_ADDR_SIZE_OFFS			0
#define CAM_CMD_WR_DATA_OFFS			16
#define CAM_CMD_IO_MEM_OFFS			24
#define CAM_CMD_RD_WR_OFFS			25
#define CAM_CMD_BUFF_MODE_OFFS			26
#define CAM_CMD_ABORT				27

/* TSC_STAT, TSC_IRQ_ENA and TSC_IRQ_CLR */
#define CAM_IRQ_EOT_OFFS			0
#define CAM_IRQ_POLL_OFFS			1
#define CAM_IRQ_RATE_MISMATCH_OFFS		2
#define CAM_IRQ_ERR_OFFS			3
#define CAM_IRQ_ABORTED_OFFS			4

/* TSC_CD_STAT */
#define TSC_CD_STAT_INSERT			0x00
#define TSC_CD_STAT_ERROR1			0x01
#define TSC_CD_STAT_ERROR2			0x02
#define TSC_CD_STAT_REMOVE			0x03

#define TSC_CD_BEG				0
#define TSC_CD_END				1

/* TSC_CICAM_TSIF */
#define TSC_CICAM_TSIF_OE_OFFS			0

/* Data structures */

/**
 * enum transaction_state - states for the transacation interrupt reason
 */
enum transaction_state {
	BEFORE_TRANSACTION = 0,
	TRANSACTION_SUCCESS = 1,
	TRANSACTION_ERROR = -1,
	TRANSACTION_CARD_REMOVED = -2
};

/**
* enum pcmcia_state - states for the pcmcia pinctrl states
* Note: the numbers here corresponds to the numbers of enum tsc_cam_personality
* in tsc.h file.
*/
enum pcmcia_state {
	PCMCIA_STATE_DISABLE = 0,
	PCMCIA_STATE_CI_CARD = 1,
	PCMCIA_STATE_CI_PLUS = 2,
	PCMCIA_STATE_PC_CARD = 3
};

/**
 * struct iommu_info - manage all the iommu information
 *
 * @group:		TSC IOMMU group.
 * @domain:		TSC IOMMU domain.
 * @domain_num:		TSC IOMMU domain number.
 * @partition_num:	TSC iommu partition number.
 * @ion_client:		TSC IOMMU client.
 * @iommu_group_name	TSC IOMMU group name.
 */
struct iommu_info {
	struct iommu_group *group;
	struct iommu_domain *domain;
	int domain_num;
	int partition_num;
	struct ion_client *ion_client;
	const char *iommu_group_name;
};

/**
 * struct pinctrl_current_state - represent which TLMM pins currently active
 *
 * @ts0:		true if TS-in 0 is active, false otherwise.
 * @ts1:		true if TS-in 1 is active, false otherwise.
 * @pcmcia_state:	Represent the pcmcia pins state.
 */
struct pinctrl_current_state {
	bool ts0;
	bool ts1;
	enum pcmcia_state pcmcia_state;
};
/**
 * struct pinctrl_info - manage all the pinctrl information
 *
 * @pinctrl:		TSC pinctrl state holder.
 * @disable:		pinctrl state to disable all the pins.
 * @ts0:		pinctrl state to activate TS-in 0 alone.
 * @ts1:		pinctrl state to activate TS-in 1 alone.
 * @dual_ts:		pinctrl state to activate both TS-in.
 * @pc_card:		pinctrl state to activate pcmcia upon card insertion.
 * @ci_card:		pinctrl state to activate pcmcia after personality
 *			change to CI card.
 * @ci_plus:		pinctrl state to activate pcmcia after personality
 *			change to CI+ card.
 * @ts0_pc_card:	pinctrl state to activate TS-in 0 and pcmcia upon card
 *			insertion.
 * @ts0_ci_card:	pinctrl state to activate TS-in 0 and pcmcia after
 *			personality change to CI card.
 * @ts0_ci_plus:	pinctrl state to activate TS-in 0 and pcmcia after
 *			personality change to CI+ card.
 * @ts1_pc_card:	pinctrl state to activate TS-in 1 and pcmcia upon card
 *			insertion.
 * @ts1_ci_card:	pinctrl state to activate TS-in 1 and pcmcia after
 *			personality change to CI card.
 * @ts1_ci_plus:	pinctrl state to activate TS-in 1 and pcmcia after
 *			personality change to CI+ card.
 * @dual_ts_pc_card:	pinctrl state to activate both TS-in and pcmcia upon
 *			card insertion.
 * @dual_ts_ci_card:	pinctrl state to activate both TS-in and pcmcia after
 *			personality change to CI card.
 * @dual_ts_ci_plus:	pinctrl state to activate both TS-in and pcmcia after
 *			personality change to CI+ card.
 * @is_ts0:		true if ts0 pinctrl states exist in device tree, false
 *			otherwise.
 * @is_ts1:		true if ts1 pinctrl states exist in device tree, false
 *			otherwise.
 * @is_pcmcia:		true if pcmcia pinctrl states exist in device tree,
 *			false otherwise.
 * @curr_state:		the current state of the TLMM pins.
 */
struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *disable;
	struct pinctrl_state *ts0;
	struct pinctrl_state *ts1;
	struct pinctrl_state *dual_ts;
	struct pinctrl_state *pc_card;
	struct pinctrl_state *ci_card;
	struct pinctrl_state *ci_plus;
	struct pinctrl_state *ts0_pc_card;
	struct pinctrl_state *ts0_ci_card;
	struct pinctrl_state *ts0_ci_plus;
	struct pinctrl_state *ts1_pc_card;
	struct pinctrl_state *ts1_ci_card;
	struct pinctrl_state *ts1_ci_plus;
	struct pinctrl_state *dual_ts_pc_card;
	struct pinctrl_state *dual_ts_ci_card;
	struct pinctrl_state *dual_ts_ci_plus;
	bool is_ts0;
	bool is_ts1;
	bool is_pcmcia;
	struct pinctrl_current_state curr_state;
};

/**
 * struct tsc_mux_chdev - TSC Mux character device
 *
 * @cdev:		TSC Mux cdev.
 * @mutex:		A mutex for mutual exclusion between Mux API calls.
 * @poll_queue:		Waiting queue for rate mismatch interrupt.
 * @spinlock:	        A spinlock to protect accesses to
 *			data structures that happen from APIs and ISRs.
 * @rate_interrupt:	A flag indicating if rate mismatch interrupt received.
 */
struct tsc_mux_chdev {
	struct cdev cdev;
	struct mutex mutex;
	wait_queue_head_t poll_queue;
	spinlock_t spinlock;
	bool rate_interrupt;
};

/**
 * struct tsc_ci_chdev - TSC CI character device
 *
 * @cdev:		TSC CI cdev.
 * @mutex:		A mutex for mutual exclusion between CI API calls.
 * @poll_queue:		Waiting queue for card detection interrupt.
 * @spinlock:	        A spinlock to protect accesses to data structures that
 *			happen from APIs and ISRs.
 * @transaction_complete: A completion struct indicating end of data
 *			transaction.
 * @transaction_finish: A completion struct indicating data transaction func
 *                      has finished.
 * @transaction_state:	flag indicating the reason for transaction end.
 * @ci_card_status:	The last card status received by the upper layer.
 * @data_busy:          true when the device is in the middle of data
 *                      transaction operation, false otherwise.
 */
struct tsc_ci_chdev {
	struct cdev cdev;
	struct mutex mutex;
	wait_queue_head_t poll_queue;
	spinlock_t spinlock;
	struct completion transaction_complete;
	struct completion transaction_finish;
	enum transaction_state transaction_state;
	enum tsc_card_status card_status;
	bool data_busy;
};

/**
 * struct tsc_device - TSC device
 *
 * @pdev:		TSC platform device.
 * @device_mux:		Mux device for sysfs and /dev entry.
 * @device_ci:		CI device for sysfs and /dev entry.
 * @mux_chdev:		TSC Mux character device instance.
 * @ci_chdev:		TSC CI character device instance.
 * @mux_device_number:	TSC Mux major number.
 * @ci_device_number:	TSC CI major number.
 * @num_mux_opened:	A counter to ensure 1 TSC Mux character device.
 * @num_ci_opened:	A counter to ensure 1 TSC CI character device.
 * @num_device_open:	A counter to synch init of power and bus voting.
 * @mutex:              Global mutex to to synch init of power and bus voting.
 * @base:		Base memory address for the TSC registers.
 * @card_detection_irq:	Interrupt No. of the card detection interrupt.
 * @cam_cmd_irq:	Interrupt No. of the cam cmd interrupt.
 * @iommu_info:		TSC IOMMU parameters.
 * @ahb_clk:		The clock for accessing the TSC registers.
 * @ci_clk:		The clock for TSC internal logic.
 * @ser_clk:		The clock for synchronizing serial TS input.
 * @par_clk:		The clock for synchronizing parallel TS input.
 * @cicam_ts_clk:	The clock for pushing TS data into the cicam.
 * @tspp2_core_clk:	The clock for enabling the TSPP2.
 * @vbif_tspp2_clk:	The clock for accessing the VBIF.
 * @vbif_ahb_clk:	The clock for VBIF AHB.
 * @vbif_axi_clk:	The clock for VBIF AXI.
 * @gdsc:               The Broadcast GDSC.
 * @bus_client:         The TSC bus client.
 * @pinctrl_info:	TSC pinctrl parameters.
 * @reset_cam_gpio:	GPIO No. for CAM HW reset.
 * @hw_card_status:	The card status as reflected by the HW registers.
 * @card_power:		True if the card is powered up, false otherwise.
 * @debugfs_entry:      TSC device debugfs entry.
 */
struct tsc_device {
	struct platform_device *pdev;
	struct device *device_mux;
	struct device *device_ci;
	struct tsc_mux_chdev mux_chdev;
	struct tsc_ci_chdev ci_chdev;
	dev_t mux_device_number;
	dev_t ci_device_number;
	int num_mux_opened;
	int num_ci_opened;
	int num_device_open;
	struct mutex mutex;
	void __iomem *base;
	unsigned int card_detection_irq;
	unsigned int cam_cmd_irq;
	struct iommu_info iommu_info;
	struct clk *ahb_clk;
	struct clk *ci_clk;
	struct clk *ser_clk;
	struct clk *par_clk;
	struct clk *cicam_ts_clk;
	struct clk *tspp2_core_clk;
	struct clk *vbif_tspp2_clk;
	struct clk *vbif_ahb_clk;
	struct clk *vbif_axi_clk;
	struct regulator *gdsc;
	uint32_t bus_client;
	struct pinctrl_info pinctrl_info;
	int reset_cam_gpio;
	enum tsc_card_status hw_card_status;
	bool card_power;
	struct dentry *debugfs_entry;
};

/* Global TSC device class */
static struct class *tsc_class;

/* Global TSC device database */
static struct tsc_device *tsc_device;

/************************** Debugfs Support **************************/
/* debugfs entries */
#define TSC_S_RW	(S_IRUGO | S_IWUSR)

struct debugfs_entry {
	const char *name;
	mode_t mode;
	int offset;
};

static const struct debugfs_entry tsc_regs_32[] = {
		{"tsc_hw_version", S_IRUGO, TSC_HW_VERSION},
		{"tsc_mux", TSC_S_RW, TSC_MUX_CFG},
		{"tsif_external_demods", TSC_S_RW, TSC_IN_IFC_EXT},
		{"tsif_internal_demod_cam", TSC_S_RW, TSC_IN_IFC_CFG_INT},
		{"tsc_fsm_state", S_IRUGO, TSC_FSM_STATE},
		{"tsc_fsm_state_mask", TSC_S_RW, TSC_FSM_STATE_MASK},
		{"tsc_cam_cmd", TSC_S_RW, TSC_CAM_CMD},
		{"tsc_rd_buff_addr", TSC_S_RW, TSC_RD_BUFF_ADDR},
		{"tsc_wr_buff_addr", TSC_S_RW, TSC_WR_BUFF_ADDR},
};

static const struct debugfs_entry tsc_regs_16[] = {
		{"tsc_false_cd_counter", S_IRUGO, TSC_FALSE_CD},
		{"tsc_cicam_tsif", TSC_S_RW, TSC_CICAM_TSIF},
};

static const struct debugfs_entry tsc_regs_8[] = {
		{"tsc_cam_rd_data", S_IRUGO, TSC_CAM_RD_DATA},
		{"tsc_irq_stat", S_IRUGO, TSC_STAT},
		{"tsc_irq_ena", TSC_S_RW, TSC_IRQ_ENA},
		{"tsc_irq_clr", TSC_S_RW, TSC_IRQ_CLR},
		{"tsc_ena_hw_poll", TSC_S_RW, TSC_CIP_CFG},
		{"tsc_card_stat", TSC_S_RW, TSC_CD_STAT},
		{"tsc_false_cd_counter_clr", TSC_S_RW, TSC_FALSE_CD_CLR},
		{"tsc_last_error_resp", S_IRUGO, TSC_RESP_ERR},
};

/* debugfs settings */
static int debugfs_iomem_set(void *data, u64 val)
{
	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (!tsc_device->num_device_open) {
		mutex_unlock(&tsc_device->mutex);
		return -ENXIO;
	}

	mutex_unlock(&tsc_device->mutex);

	writel_relaxed(val, data);
	wmb();

	return 0;
}

static int debugfs_iomem_get(void *data, u64 *val)
{
	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (!tsc_device->num_device_open) {
		mutex_unlock(&tsc_device->mutex);
		return -ENXIO;
	}

	mutex_unlock(&tsc_device->mutex);

	*val = readl_relaxed(data);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_get,
			debugfs_iomem_set, "0x%08llX");
DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x16, debugfs_iomem_get,
			debugfs_iomem_set, "0x%04llX");
DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x8, debugfs_iomem_get,
			debugfs_iomem_set, "0x%02llX");

/**
 * tsc_debugfs_init() - TSC device debugfs initialization.
 */
static void tsc_debugfs_init(void)
{
	int i;
	struct dentry *dentry;
	void __iomem *base = tsc_device->base;

	tsc_device->debugfs_entry = debugfs_create_dir("tsc", NULL);
	if (!tsc_device->debugfs_entry)
			return;
	dentry = debugfs_create_dir("regs", tsc_device->debugfs_entry);
	if (dentry) {
		for (i = 0; i < ARRAY_SIZE(tsc_regs_32); i++) {
			debugfs_create_file(
					tsc_regs_32[i].name,
					tsc_regs_32[i].mode,
					dentry,
					base + tsc_regs_32[i].offset,
					&fops_iomem_x32);
		}
		for (i = 0; i < ARRAY_SIZE(tsc_regs_16); i++) {
			debugfs_create_file(
					tsc_regs_16[i].name,
					tsc_regs_16[i].mode,
					dentry,
					base + tsc_regs_16[i].offset,
					&fops_iomem_x16);
		}
		for (i = 0; i < ARRAY_SIZE(tsc_regs_8); i++) {
			debugfs_create_file(
					tsc_regs_8[i].name,
					tsc_regs_8[i].mode,
					dentry,
					base + tsc_regs_8[i].offset,
					&fops_iomem_x8);
		}
	}
}

/**
 * tsc_debugfs_exit() - TSC device debugfs teardown.
 */
static void tsc_debugfs_exit(void)
{
	debugfs_remove_recursive(tsc_device->debugfs_entry);
	tsc_device->debugfs_entry = NULL;
}

/**
 * tsc_update_hw_card_status() - Update the hw_status according to the HW reg.
 *
 * Read the register indicating the card status (inserted, removed, error) and
 * update the tsc_device->hw_card_status accordingly.
 */
static void tsc_update_hw_card_status(void)
{
	u32 cd_reg, card_status = 0;

	cd_reg = readl_relaxed(tsc_device->base + TSC_CD_STAT);
	card_status = GETL_BITS(cd_reg, TSC_CD_BEG, TSC_CD_END);
	switch (card_status) {
	case TSC_CD_STAT_INSERT:
		tsc_device->hw_card_status = TSC_CARD_STATUS_DETECTED;
		break;
	case TSC_CD_STAT_ERROR1:
	case TSC_CD_STAT_ERROR2:
		tsc_device->hw_card_status = TSC_CARD_STATUS_FAILURE;
		break;
	case TSC_CD_STAT_REMOVE:
		tsc_device->hw_card_status = TSC_CARD_STATUS_NOT_DETECTED;
		break;
	}
}

/**
 * tsc_card_power_down() - power down card interface upon removal.
 *
 * Power down the card by disable VPP, disable pins in the TLMM, assert the
 * reset line and disable the level-shifters. This function assumes the spinlock
 * of ci device is already taken.
 *
 * Return 0 on finish, error value if interrupted while acquiring a mutex.
 */
static int tsc_card_power_down(void)
{
	int ret = 0;
	struct pinctrl_info *ppinctrl = &tsc_device->pinctrl_info;
	struct pinctrl_current_state *pcurr_state = &ppinctrl->curr_state;
	int reset_gpio = tsc_device->reset_cam_gpio;
	u32 reg = 0;

	/* Clearing CAM TSIF OE to disable I/O CAM transactions */
	CLEAR_BIT(TSC_CICAM_TSIF_OE_OFFS, reg);
	writel_relaxed(reg, tsc_device->base + TSC_CICAM_TSIF);

	/* Assert the reset line */
	ret = gpio_direction_output(reset_gpio, 1); /* assert */
	if (ret != 0)
		pr_err("%s: Failed to assert the reset CAM GPIO\n", __func__);

	/* Disable all the level-shifters */
	/* TODO: call mpq_standby_pcmcia_master0_set(0) after MCU mainlined */
	if (ret != 0)
		pr_err("%s: error disable master0 level-shifters. ret value = %d\n",
				__func__, ret);
	/* TODO: call mpq_standby_pcmcia_master1_set(1) after MCU mainlined */
	if (ret != 0)
		pr_err("%s: error disable master1 level-shifters. ret value = %d\n",
				__func__, ret);

	/* Power-down the card */
	/* TODO: call mpq_standby_pcmcia_vpp_set(1) after MCU mainlined */
	if (ret != 0)
		pr_err("%s: error disabling VPP. ret value = %d\n", __func__,
				ret);
	/* Wait 10msec until VPP become stable */
	usleep(10000);

	/* Disable pins in the TLMM */
	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (pcurr_state->ts0 && pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->dual_ts);
	else if (pcurr_state->ts0 && !pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->ts0);
	else if (!pcurr_state->ts0 && pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->ts1);
	else
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->disable);
	if (ret != 0)
		pr_err("%s: error changing PCMCIA pins upon card removal. ret value = %d\n",
					__func__, ret);
	else
		pcurr_state->pcmcia_state = PCMCIA_STATE_DISABLE;

	mutex_unlock(&tsc_device->mutex);

	return 0;
}

/**
 * tsc_card_power_up() - power up card interface upon insertion.
 *
 * Power up the card by open VPP, enable pins in the TLMM, deassert the reset
 * line and enable the level-shifters. This function assumes the spinlock of ci
 * device is already taken.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_card_power_up(void)
{
	int ret = 0;
	struct pinctrl_info *ppinctrl = &tsc_device->pinctrl_info;
	struct pinctrl_current_state *pcurr_state = &ppinctrl->curr_state;
	int reset_gpio = tsc_device->reset_cam_gpio;

	/* Power-up the card */
	/* TODO: call mpq_standby_pcmcia_vpp_set(1) after MCU mainlined */
	if (ret != 0) {
		pr_err("%s: error setting VPP. ret value = %d\n", __func__,
				ret);
		return ret;
	}
	/* Wait 10msec until VPP become stable */
	usleep(10000);

	/* Enable pins in the TLMM */
	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (pcurr_state->ts0 && pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->dual_ts_pc_card);
	else if (pcurr_state->ts0 && !pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->ts0_pc_card);
	else if (!pcurr_state->ts0 && pcurr_state->ts1)
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->ts1_pc_card);
	else
		ret = pinctrl_select_state(ppinctrl->pinctrl,
				ppinctrl->pc_card);
	if (ret != 0) {
		pr_err("%s: error changing PCMCIA pins upon card insertion. ret value = %d\n",
					__func__, ret);
		mutex_unlock(&tsc_device->mutex);
		goto err;
	} else {
		pcurr_state->pcmcia_state = PCMCIA_STATE_PC_CARD;
	}
	mutex_unlock(&tsc_device->mutex);

	/* Release the reset line */
	ret = gpio_direction_output(reset_gpio, 0); /* Deassert */
	if (ret != 0) {
		pr_err("%s: Failed to deassert the reset CAM GPIO\n", __func__);
		goto err;
	}

	/* Enable level-shifters for all pins */
	/* TODO: call mpq_standby_pcmcia_master0_set(0) after MCU mainlined */
	if (ret != 0) {
		pr_err("%s: error setting master0 level-shifters. ret value = %d\n",
				__func__, ret);
		goto err;
	}
	/* TODO: call mpq_standby_pcmcia_master1_set(0) after MCU mainlined */
	if (ret != 0) {
		pr_err("%s: error setting master1 level-shifters. ret value = %d\n",
				__func__, ret);
		goto err;
	}

	/* Wait 20msec at the end of the power-up sequence */
	usleep(20000);

	return ret;

err:
	tsc_card_power_down();
	return ret;
}

/************************** Interrupt handlers **************************/
/**
 * tsc_card_detect_irq_thread_handler() - TSC card detect interrupt handler.
 *
 * @irq:	Interrupt number.
 * @dev:	TSC device.
 *
 * The handler is executed on a thread context, not in the interrupt context
 * (can take a mutex and sleep).
 * Read the card detection status from the register and initiate a power-up/down
 * sequence accordingly. The sequence will occur only if a change is needed in
 * the current power state.
 *
 */
static irqreturn_t tsc_card_detect_irq_thread_handler(int irq, void *dev)
{
	int ret = 0;
	struct tsc_ci_chdev *tsc_ci;
	unsigned long flags = 0;

	tsc_ci = &tsc_device->ci_chdev;

	mutex_lock(&tsc_ci->mutex);

	tsc_update_hw_card_status();

	/* waking-up ci poll queue */
	wake_up_interruptible(&tsc_ci->poll_queue);

	/* If in the middle of a data transaction- aborting the transaction */
	if (tsc_ci->data_busy && tsc_device->hw_card_status ==
			TSC_CARD_STATUS_NOT_DETECTED) {
		spin_lock_irqsave(&tsc_ci->spinlock, flags);
		tsc_ci->transaction_state = TRANSACTION_CARD_REMOVED;
		spin_unlock_irqrestore(&tsc_ci->spinlock, flags);
		complete_all(&tsc_ci->transaction_complete);
	}

	if (tsc_device->hw_card_status == TSC_CARD_STATUS_DETECTED &&
			!tsc_device->card_power) {
		ret = tsc_card_power_up();
		if (ret != 0)
			pr_err("%s: card power-up failed\n", __func__);
		else
			tsc_device->card_power = true;
	} else if (tsc_device->hw_card_status == TSC_CARD_STATUS_NOT_DETECTED &&
			tsc_device->card_power) {
		tsc_card_power_down();
		/*
		 * In case something failed during the power down, the sequence
		 * continue and the status of the card power is considered as
		 * powered down.
		 */
		tsc_device->card_power = false;
	}

	mutex_unlock(&tsc_ci->mutex);

	return IRQ_HANDLED;
}

/**
 * tsc_cam_cmd_irq_handler() - TSC CAM interrupt handler.
 *
 * @irq:	Interrupt number.
 * @dev:	TSC device.
 *
 * Handle TSC CAM HW interrupt. Handle the CAM transaction interrupts by waking
 * up the completion sync object, handle rate mismatch interrupt by waking-up
 * the TSC Mux poll wait-queue and clear the interrupts received.
 *
 * Return IRQ_HANDLED.
 */
static irqreturn_t tsc_cam_cmd_irq_handler(int irq, void *dev)
{
	struct tsc_ci_chdev *tsc_ci;
	struct tsc_mux_chdev *tsc_mux;
	unsigned long flags;
	u32 stat_reg, ena_reg;

	tsc_ci = &tsc_device->ci_chdev;
	tsc_mux = &tsc_device->mux_chdev;

	stat_reg = readl_relaxed(tsc_device->base + TSC_STAT);

	/* Handling transaction interrupts */
	if (TEST_BIT(CAM_IRQ_ERR_OFFS, stat_reg) ||
			TEST_BIT(CAM_IRQ_EOT_OFFS, stat_reg)) {
		spin_lock_irqsave(&tsc_ci->spinlock, flags);

		if (TEST_BIT(CAM_IRQ_EOT_OFFS, stat_reg))
			tsc_ci->transaction_state = TRANSACTION_SUCCESS;
		else
			tsc_ci->transaction_state = TRANSACTION_ERROR;

		spin_unlock_irqrestore(&tsc_ci->spinlock, flags);
		complete_all(&tsc_ci->transaction_complete);
	}

	/* Handling rate mismatch interrupt */
	if (TEST_BIT(CAM_IRQ_RATE_MISMATCH_OFFS, stat_reg)) {
		spin_lock_irqsave(&tsc_mux->spinlock, flags);

		/* Disabling rate mismatch interrupt */
		ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
		CLEAR_BIT(CAM_IRQ_RATE_MISMATCH_OFFS, ena_reg);
		writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

		/* Setting internal flag for poll */
		tsc_mux->rate_interrupt = true;

		spin_unlock_irqrestore(&tsc_mux->spinlock, flags);
		/* waking-up mux poll queue */
		wake_up_interruptible(&tsc_mux->poll_queue);
	}

	/* Clearing all the interrupts received */
	writel_relaxed(stat_reg, tsc_device->base + TSC_IRQ_CLR);

	/*
	 * Before returning IRQ_HANDLED to the generic interrupt handling
	 * framework need to make sure all operations including clearing of
	 * interrupt status registers in the hardware is performed.
	 * Thus a barrier after clearing the interrupt status register
	 * is required to guarantee that the interrupt status register has
	 * really been cleared by the time we return from this handler.
	 */
	wmb();

	return IRQ_HANDLED;
}

/************************** Internal functions **************************/

/**
 * tsc_set_cicam_clk() - Setting the rate of the TS from the TSC to the CAM
 *
 * @arg:	The argument received from the user-space via set rate IOCTL.
 *		It is the value of the requested rate in MHz.
 *
 * Setting the rate of the cicam_ts_clk clock, with one of the valid clock
 * frequencies. The arg value given is rounded to the nearest frequency.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_set_cicam_clk(unsigned long arg)
{
	int ret;

	if (arg <= 8)
		ret = clk_set_rate(tsc_device->cicam_ts_clk,
				CICAM_CLK_RATE_7MHZ);
	else if (arg <= 11)
		ret = clk_set_rate(tsc_device->cicam_ts_clk,
				CICAM_CLK_RATE_9MHZ);
	else
		ret = clk_set_rate(tsc_device->cicam_ts_clk,
				CICAM_CLK_RATE_12MHZ);
	return ret;
}

/**
 * tsc_enable_rate_irq() - Enabling the rate mismatch interrupt.
 *
 * @tsc_mux:		TSC Mux device.
 *
 * Setting the bit of this interrupt in the register that controls which
 * interrupts are enabled.
 */
static void tsc_enable_rate_irq(struct tsc_mux_chdev *tsc_mux)
{
	unsigned long flags;
	u32 ena_reg = 0;

	spin_lock_irqsave(&tsc_mux->spinlock, flags);

	/* Setting the bit to start receiving rate mismatch interrupt again */
	ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
	SET_BIT(CAM_IRQ_RATE_MISMATCH_OFFS, ena_reg);
	writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

	spin_unlock_irqrestore(&tsc_mux->spinlock, flags);
}

/**
 * tsc_config_tsif() - Modifying TSIF configuration.
 *
 * @tsc_mux:		TSC Mux device.
 * @tsif_params:	TSIF parameters received from the user-space via IOCTL.
 *
 * Update the specified TSIF parameters according to the values in tsif_params.
 * The update is done by modifying a HW register.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_config_tsif(struct tsc_mux_chdev *tsc_mux,
		struct tsc_tsif_params *tsif_params)
{
	int ret = 0;
	u32 reg;
	int reg_internal_offs;
	u32 reg_addr_offs;

	switch (tsif_params->source) {
	case TSC_SOURCE_EXTERNAL0:
		reg_internal_offs = 0;
		reg_addr_offs = TSC_IN_IFC_EXT;
		break;
	case TSC_SOURCE_EXTERNAL1:
		reg_internal_offs = 16;
		reg_addr_offs = TSC_IN_IFC_EXT;
		break;
	case TSC_SOURCE_INTERNAL:
		reg_internal_offs = 0;
		reg_addr_offs = TSC_IN_IFC_CFG_INT;
		break;
	case TSC_SOURCE_CICAM:
		reg_internal_offs = 16;
		reg_addr_offs = TSC_IN_IFC_CFG_INT;
		break;
	default:
		pr_err("%s: unidentified source parameter\n", __func__);
		ret = -EINVAL;
		goto err;
	}


	reg = readl_relaxed(tsc_device->base + reg_addr_offs);

	/* Modifying TSIF settings in the register value */
	(tsif_params->clock_polarity ?
		SET_BIT((reg_internal_offs + TSIF_CLK_POL_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_CLK_POL_OFFS), reg));
	(tsif_params->data_polarity ?
		SET_BIT(((reg_internal_offs + TSIF_DATA_POL_OFFS)), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_DATA_POL_OFFS), reg));
	(tsif_params->start_polarity ?
		SET_BIT((reg_internal_offs + TSIF_START_POL_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_START_POL_OFFS), reg));
	(tsif_params->valid_polarity ?
		SET_BIT((reg_internal_offs + TSIF_VALID_POL_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_VALID_POL_OFFS), reg));
	(tsif_params->error_polarity ?
		SET_BIT((reg_internal_offs + TSIF_ERROR_POL_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_ERROR_POL_OFFS), reg));
	(tsif_params->data_type ?
		SET_BIT((reg_internal_offs + TSIF_SER_PAR_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_SER_PAR_OFFS), reg));
	reg &= ~(0x3 << TSIF_REC_MODE_OFFS);
	reg |= (tsif_params->receive_mode << TSIF_REC_MODE_OFFS);
	(tsif_params->data_swap ?
		SET_BIT((reg_internal_offs + TSIF_DATA_SWAP_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_DATA_SWAP_OFFS), reg));
	(tsif_params->set_error ?
		SET_BIT((reg_internal_offs + TSIF_ERR_INSERT_OFFS), reg) :
		CLEAR_BIT((reg_internal_offs + TSIF_ERR_INSERT_OFFS), reg));

	/* Writing the new settings to the register */
	writel_relaxed(reg, tsc_device->base + reg_addr_offs);

err:
	return ret;
}

/**
 * tsc_suspend_ts_pins() - Suspend TS-in pins
 *
 * @source:     The TSIF to configure.
 *
 * Config the TLMM pins of a TSIF as TS-in pins in sleep state according to
 * the current pinctrl configuration of the other pins.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_suspend_ts_pins(enum tsc_source source)
{
	int ret = 0;
	struct pinctrl_info *ppinctrl = &tsc_device->pinctrl_info;
	struct pinctrl_current_state *pcurr_state = &ppinctrl->curr_state;

	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (source == TSC_SOURCE_EXTERNAL0) {
		if (!ppinctrl->is_ts0) {
			pr_err("%s: No TS0-in pinctrl definitions were found in the TSC devicetree\n",
					__func__);
			mutex_unlock(&tsc_device->mutex);
			return -EPERM;
		}

		/* Transition from current pinctrl state to curr + ts0 sleep */
		switch (pcurr_state->pcmcia_state) {
		case PCMCIA_STATE_DISABLE:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
						ppinctrl->ts1);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
						ppinctrl->disable);
			break;
		case PCMCIA_STATE_PC_CARD:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_pc_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->pc_card);
			break;
		case PCMCIA_STATE_CI_CARD:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_card);
			break;
		case PCMCIA_STATE_CI_PLUS:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_plus);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_plus);
			break;
		}
	} else  { /* source == TSC_SOURCE_EXTERNAL1 */
		if (!ppinctrl->is_ts1) {
			pr_err("%s: No TS1-in pinctrl definitions were found in the TSC devicetree\n",
					__func__);
			mutex_unlock(&tsc_device->mutex);
			return -EPERM;
		}

		/* Transition from current pinctrl state to curr + ts1 sleep */
		switch (pcurr_state->pcmcia_state) {
		case PCMCIA_STATE_DISABLE:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->disable);
			break;
		case PCMCIA_STATE_PC_CARD:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_pc_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->pc_card);
			break;
		case PCMCIA_STATE_CI_CARD:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_card);
			break;
		case PCMCIA_STATE_CI_PLUS:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_plus);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_plus);
			break;
		}
	}

	if (ret != 0) {
		pr_err("%s: error disabling TS-in pins. ret value = %d\n",
			__func__, ret);
		mutex_unlock(&tsc_device->mutex);
		return -EINVAL;
	}

	/* Update the current pinctrl state in the internal struct */
	if (source == TSC_SOURCE_EXTERNAL0)
		pcurr_state->ts0 = false;
	else
		pcurr_state->ts1 = false;

	mutex_unlock(&tsc_device->mutex);

	return 0;
}

/**
 * tsc_activate_ts_pins() - Activate TS-in pins
 *
 * @source:	The TSIF to configure.
 *
 * Config the TLMM pins of a TSIF as TS-in pins in active state according to
 * the current pinctrl configuration of the other pins
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_activate_ts_pins(enum tsc_source source)
{
	int ret = 0;
	struct pinctrl_info *ppinctrl = &tsc_device->pinctrl_info;
	struct pinctrl_current_state *pcurr_state = &ppinctrl->curr_state;

	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (source == TSC_SOURCE_EXTERNAL0) {
		if (!ppinctrl->is_ts0) {
			pr_err("%s: No TS0-in pinctrl definitions were found in the TSC devicetree\n",
					__func__);
			mutex_unlock(&tsc_device->mutex);
			return -EPERM;
		}

		/* Transition from current pinctrl state to curr + ts0 active */
		switch (pcurr_state->pcmcia_state) {
		case PCMCIA_STATE_DISABLE:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
						ppinctrl->dual_ts);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
						ppinctrl->ts0);
			break;
		case PCMCIA_STATE_PC_CARD:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_pc_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_pc_card);
			break;
		case PCMCIA_STATE_CI_CARD:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_card);
			break;
		case PCMCIA_STATE_CI_PLUS:
			if (pcurr_state->ts1)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_plus);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_plus);
			break;
		}
	} else  { /* source == TSC_SOURCE_EXTERNAL1 */
		if (!ppinctrl->is_ts1) {
			pr_err("%s: No TS1-in pinctrl definitions were found in the TSC devicetree\n",
					__func__);
			mutex_unlock(&tsc_device->mutex);
			return -EPERM;
		}

		/* Transition from current pinctrl state to curr + ts1 active */
		switch (pcurr_state->pcmcia_state) {
		case PCMCIA_STATE_DISABLE:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1);
			break;
		case PCMCIA_STATE_PC_CARD:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_pc_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_pc_card);
			break;
		case PCMCIA_STATE_CI_CARD:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_card);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_card);
			break;
		case PCMCIA_STATE_CI_PLUS:
			if (pcurr_state->ts0)
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_plus);
			else
				ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_plus);
			break;
		}
	}

	if (ret != 0) {
		pr_err("%s: error activating TS-in pins. ret value = %d\n",
			__func__, ret);
		mutex_unlock(&tsc_device->mutex);
		return -EINVAL;
	}

	/* Update the current pinctrl state in the internal struct */
	if (source == TSC_SOURCE_EXTERNAL0)
		pcurr_state->ts0 = true;
	else
		pcurr_state->ts1 = true;

	mutex_unlock(&tsc_device->mutex);

	return 0;
}

/**
 * tsc_enable_disable_tsif() - Enable/disable a TSIF.
 *
 * @tsc_mux:	TSC Mux device.
 * @source:	The TSIF to enable or disable.
 * @operation:	The operation to perform: 0- enable, 1- disable.
 *
 * Enable or disable the specified TSIF, which consequently will block the TS
 * flowing through this TSIF. The update is done by modifying a HW register.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_enable_disable_tsif(struct tsc_mux_chdev *tsc_mux,
		enum tsc_source source, int operation)
{
	int ret = 0;
	u32 reg;
	u32 addr_offs;
	int reg_offs;
	int curr_disable_state;

	switch (source) {
	case TSC_SOURCE_EXTERNAL0:
		reg_offs = 0;
		addr_offs = TSC_IN_IFC_EXT;
		break;
	case TSC_SOURCE_EXTERNAL1:
		reg_offs = 16;
		addr_offs = TSC_IN_IFC_EXT;
		break;
	case TSC_SOURCE_INTERNAL:
		reg_offs = 0;
		addr_offs = TSC_IN_IFC_CFG_INT;
		break;
	case TSC_SOURCE_CICAM:
		reg_offs = 16;
		addr_offs = TSC_IN_IFC_CFG_INT;
		break;
	default:
		pr_err("%s: unidentified source parameter\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	/* Reading the current enable/disable state from the register */
	reg = readl_relaxed(tsc_device->base + addr_offs);
	curr_disable_state = GETL_BITS(reg, TSIF_DISABLE_OFFS + reg_offs,
			TSIF_DISABLE_OFFS + reg_offs);
	/* If the current state equals the new state- return success */
	if (curr_disable_state == operation)
		return ret;

	if (operation == TSIF_INPUT_DISABLE) {
		if (source == TSC_SOURCE_EXTERNAL0 ||
				source == TSC_SOURCE_EXTERNAL1) {
			/* Disabling the TS-in pins in the TLMM */
			ret = tsc_suspend_ts_pins(source);
			if (ret != 0) {
				pr_err("%s: Error suspending TS-in pins",
						__func__);
				return ret;
			}
		}
		SET_BIT((reg_offs + TSIF_DISABLE_OFFS), reg);
	} else {
		if (source == TSC_SOURCE_EXTERNAL0 ||
				source == TSC_SOURCE_EXTERNAL1) {
			/* Enabling the TS-in pins in the TLMM */
			ret = tsc_activate_ts_pins(source);
			if (ret != 0) {
				pr_err("%s: Error activating TS-in pins",
						__func__);
				return ret;
			}
		}
		CLEAR_BIT((reg_offs + TSIF_DISABLE_OFFS), reg);
	}

	/* Writing back to the reg the enable/disable of the TSIF */
	writel_relaxed(reg, tsc_device->base + addr_offs);

	return ret;
}

/**
 * tsc_route_mux() - Configuring one of the TSC muxes.
 *
 * @tsc_mux:	TSC Mux device.
 * @source:	The requested TS source to be selected by the mux.
 * @dest:	The requested mux.
 *
 * Configuring the specified mux to pass the TS indicated by the src parameter.
 * The update is done by modifying a HW register.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_route_mux(struct tsc_mux_chdev *tsc_mux, enum tsc_source source,
		enum tsc_dest dest)
{
	int ret = 0;
	u32 mux_cfg_reg;
	int src_val;

	switch (source) {
	case TSC_SOURCE_EXTERNAL0:
		src_val = MUX_EXTERNAL_DEMOD_0;
		break;
	case TSC_SOURCE_EXTERNAL1:
		src_val = MUX_EXTERNAL_DEMOD_1;
		break;
	case TSC_SOURCE_INTERNAL:
		src_val = MUX_INTERNAL_DEMOD;
		break;
	case TSC_SOURCE_CICAM:
		src_val = MUX_CICAM;
		break;
	default:
		pr_err("%s: unidentified source parameter\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	/* Reading the current muxes state, to change only the requested mux */
	mux_cfg_reg = readl_relaxed(tsc_device->base + TSC_MUX_CFG);

	switch (dest) {
	case TSC_DEST_TSPP0:
		mux_cfg_reg &= ~(0x3 << MUX0_OFFS);
		mux_cfg_reg |= (src_val << MUX0_OFFS);
		break;
	case TSC_DEST_TSPP1:
		mux_cfg_reg &= ~(0x3 << MUX1_OFFS);
		mux_cfg_reg |= (src_val << MUX1_OFFS);
		break;
	case TSC_DEST_CICAM:
		if (src_val == TSC_SOURCE_CICAM) {
			pr_err("%s: Error: CICAM cannot be source and dest\n",
					__func__);
			ret = -EINVAL;
			goto err;
		}
		mux_cfg_reg &= ~(0x3 << MUX_CAM_OFFS);
		mux_cfg_reg |= (src_val << MUX_CAM_OFFS);
		break;
	default:
		pr_err("%s: unidentified dest parameter\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	writel_relaxed(mux_cfg_reg, tsc_device->base + TSC_MUX_CFG);

err:
	return ret;
}

/**
 * is_tsc_idle() - Checking if TSC is idle.
 *
 * @tsc_ci:		TSC CI device.
 *
 * Reading the TSC state-machine register and checking if the TSC is busy in
 * one of the operations reflected by this register.
 *
 * Return true if the TSC is idle and false if it's busy.
 */
static bool is_tsc_idle(struct tsc_ci_chdev *tsc_ci)
{
	u32 fsm_reg;

	fsm_reg = readl_relaxed(tsc_device->base + TSC_FSM_STATE);
	if (GETL_BITS(fsm_reg, FSM_STATE_BUFFER_BEG, FSM_STATE_BUFFER_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_POLL_BEG, FSM_STATE_POLL_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_BYTE_BEG, FSM_STATE_BYTE_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_MEM_WR_BEG,
				FSM_STATE_MEM_WR_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_MEM_RD_BEG,
				FSM_STATE_MEM_RD_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_IO_RD_BEG, FSM_STATE_IO_RD_END) ||
		GETL_BITS(fsm_reg, FSM_STATE_IO_WR_BEG, FSM_STATE_IO_WR_END) ||
			tsc_ci->data_busy)
		return false;

	tsc_ci->data_busy = true;

	return true;
}


/**
 * tsc_power_on_buff_mode_clocks() - power-on the TSPP2 and VBIF clocks.
 *
 * Power-on the TSPP2 and the VBIF clocks required for buffer mode transaction.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_power_on_buff_mode_clocks(void)
{
	int ret = 0;

	ret = clk_prepare_enable(tsc_device->tspp2_core_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tspp2_core_clk", __func__);
		goto err_tspp2;
	}
	ret = clk_prepare_enable(tsc_device->vbif_tspp2_clk);
	if (ret != 0) {
		pr_err("%s: Can't start vbif_tspp2_clk", __func__);
		goto err_vbif_tspp2;
	}
	ret = clk_prepare_enable(tsc_device->vbif_ahb_clk);
	if (ret != 0) {
		pr_err("%s: Can't start vbif_ahb_clk", __func__);
		goto err_vbif_ahb;
	}
	ret = clk_prepare_enable(tsc_device->vbif_axi_clk);
	if (ret != 0) {
		pr_err("%s: Can't start vbif_axi_clk", __func__);
		goto err_vbif_axi;
	}

	return ret;

err_vbif_axi:
	clk_disable_unprepare(tsc_device->vbif_ahb_clk);
err_vbif_ahb:
	clk_disable_unprepare(tsc_device->vbif_tspp2_clk);
err_vbif_tspp2:
	clk_disable_unprepare(tsc_device->tspp2_core_clk);
err_tspp2:
	return ret;
}

/**
 * tsc_power_off_buff_mode_clocks() - power-off the SPP2 and VBIF clocks.
 *
 * Power-off the TSPP2 and the VBIF clocks required for buffer mode transaction.
 */
static void tsc_power_off_buff_mode_clocks(void)
{
	clk_disable_unprepare(tsc_device->vbif_axi_clk);
	clk_disable_unprepare(tsc_device->vbif_ahb_clk);
	clk_disable_unprepare(tsc_device->tspp2_core_clk);
	clk_disable_unprepare(tsc_device->vbif_tspp2_clk);
}

/**
 * tsc_config_cam_data_transaction() - Configuring a new data transaction.
 *
 * @addr_size:	The value for the address_size register field- address when
 *		using single byte-mode, and size when using buffer mode.
 * @wr_data:	the value for the wr_data register field- data to write to the
 *		cam when using single byte mode.
 * @io_mem:	The value for the io_mem register field- 1 for IO transaction,
 *		0 for memory transaction.
 * @read_write:	The value for the read_write register field- 1 for read
 *		transaction, 0 for write transaction.
 * @buff_mode:	The value for the buff_mode register field- 1 for buffer mode,
 *		0 for single byte mode.
 *
 * Configuring the cam cmd register with the specified parameters, to initiate
 * data transaction with the cam.
 */
static void tsc_config_cam_data_transaction(u16 addr_size,
		u8 wr_data,
		uint io_mem,
		uint read_write,
		uint buff_mode)
{
	u32 cam_cmd_reg = 0;

	cam_cmd_reg |= (addr_size << CAM_CMD_ADDR_SIZE_OFFS);
	cam_cmd_reg |= (wr_data << CAM_CMD_WR_DATA_OFFS);
	cam_cmd_reg |= (io_mem << CAM_CMD_IO_MEM_OFFS);
	cam_cmd_reg |= (read_write << CAM_CMD_RD_WR_OFFS);
	cam_cmd_reg |= (buff_mode << CAM_CMD_BUFF_MODE_OFFS);
	writel_relaxed(cam_cmd_reg, tsc_device->base + TSC_CAM_CMD);
}

/**
 * tsc_data_transaction() - Blocking function that manage the data transactions.
 *
 * @tsc_ci:	TSC CI device.
 * @io_mem:	The value for the io_mem register field- 1 for IO transaction,
 *		0 for memory transaction.
 * @read_write:	The value for the read_write register field- 1 for read
 *		transaction, 0 for write transaction.
 * @buff_mode:	The value for the buff_mode register field- 1 for buffer mode,
 *		0 for single byte mode.
 * @arg:	The argument received from the user-space via a data transaction
 *		IOCTL. It is from one of the two following types:
 *		"struct tsc_single_byte_mode" and "struct tsc_buffer_mode".
 *
 * Receiving the transaction paramters from the user-space. Configure the HW
 * registers to initiate a data transaction with the cam. Wait for an
 * interrupt indicating the transaction is over and return the the data read
 * from the cam in case of single-byte read transaction.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_data_transaction(struct tsc_ci_chdev *tsc_ci, uint io_mem,
		uint read_write, uint buff_mode, unsigned long arg)
{
	struct tsc_single_byte_mode arg_byte;
	struct tsc_buffer_mode arg_buff;
	u16 addr_size;
	u8 wr_data;
	uint timeout;
	u32 cam_cmd_reg;
	struct ion_handle *ion_handle = NULL;
	ion_phys_addr_t iova = 0;
	unsigned long buffer_size = 0;
	unsigned long flags = 0;
	int ret = 0;

	if (!arg)
		return -EINVAL;

	/* make sure the tsc is in idle state before configuring the cam */
	if (!is_tsc_idle(tsc_ci)) {
		ret =  -EBUSY;
		goto finish;
	}

	INIT_COMPLETION(tsc_ci->transaction_finish);

	/* copying data from the ioctl parameter */
	if (buff_mode == SINGLE_BYTE_MODE) {
		if (copy_from_user(&arg_byte, (void *)arg,
				sizeof(struct tsc_single_byte_mode))) {
			ret = -EFAULT;
			goto err_copy_arg;
		}
		addr_size = arg_byte.address;
		if (IO_TRANSACTION == io_mem &&
				addr_size > CICAM_MAX_ADDRESS) {
			pr_err("%s: wrong address parameter: %d\n", __func__,
					addr_size);
			ret = -EFAULT;
			goto err_copy_arg;
		}
		wr_data = arg_byte.data;
		timeout = arg_byte.timeout;
	} else {
		if (copy_from_user(&arg_buff, (void *)arg,
				sizeof(struct tsc_buffer_mode))) {
			ret =  -EFAULT;
			goto err_copy_arg;
		}
		addr_size = arg_buff.buffer_size;
		if (!addr_size) {
			pr_err("%s: size parameter is 0\n", __func__);
			ret =  -EFAULT;
			goto err_copy_arg;
		}
		wr_data = 0;
		timeout = arg_buff.timeout;

		/* import ion handle from the ion fd passed from user-space */
		ion_handle = ion_import_dma_buf
			(tsc_device->iommu_info.ion_client, arg_buff.buffer_fd);
		if (IS_ERR_OR_NULL(ion_handle)) {
			pr_err("%s: get_ION_handle failed\n", __func__);
			ret =  -EIO;
			goto err_ion_handle;
		}

		/*
		 * mapping the ion handle to the VBIF and get the virtual
		 * address
		 */
		ret = ion_map_iommu(tsc_device->iommu_info.ion_client,
				ion_handle, tsc_device->iommu_info.domain_num,
				tsc_device->iommu_info.partition_num, SZ_4K,
				0, &iova, &buffer_size, 0, 0);

		if (ret != 0) {
			pr_err("%s: get_ION_kernel physical addr fail\n",
					__func__);
			goto err_ion_map;
		}

		/*
		 * writing the buffer virtual address to the register for buffer
		 * address of buffer mode
		 */
		if (read_write == READ_TRANSACTION)
			writel_relaxed(iova,
					tsc_device->base + TSC_RD_BUFF_ADDR);
		else   /* write transaction */
			writel_relaxed(iova,
					tsc_device->base + TSC_WR_BUFF_ADDR);
	}

	/* configuring the cam command register */
	tsc_config_cam_data_transaction(addr_size, wr_data, io_mem, read_write,
			buff_mode);

	/*
	 * This function assume the mutex is locked before calling the function,
	 * so mutex has to be unlocked before going to sleep when waiting for
	 * the transaction.
	 */
	mutex_unlock(&tsc_ci->mutex);
	/* waiting for EOT interrupt or timeout */
	if (!wait_for_completion_timeout(&tsc_ci->transaction_complete,
			msecs_to_jiffies(timeout))) {
		pr_err("%s: Error: wait for transaction timed-out\n", __func__);
		ret =  -ETIMEDOUT;
		mutex_lock(&tsc_ci->mutex);
		/* Aborting the transaction if it's buffer mode */
		if (buff_mode) {
			cam_cmd_reg = readl_relaxed(tsc_device->base +
					TSC_CAM_CMD);
			SET_BIT(CAM_CMD_ABORT, cam_cmd_reg);
			writel_relaxed(cam_cmd_reg, tsc_device->base +
					TSC_CAM_CMD);
		}
		goto finish;
	}
	mutex_lock(&tsc_ci->mutex);

	/* Checking if transaction ended with error */
	spin_lock_irqsave(&tsc_ci->spinlock, flags);
	if (tsc_ci->transaction_state == TRANSACTION_ERROR) {
		tsc_ci->transaction_state = BEFORE_TRANSACTION;
		spin_unlock_irqrestore(&tsc_ci->spinlock, flags);
		pr_err("%s: Transaction error\n", __func__);
		ret =  -EBADE; /* Invalid exchange error code */
		goto finish;
	} else if (tsc_ci->transaction_state == TRANSACTION_CARD_REMOVED) {
		tsc_ci->transaction_state = BEFORE_TRANSACTION;
		spin_unlock_irqrestore(&tsc_ci->spinlock, flags);
		pr_err("%s: Card was removed during the transaction. Aborting\n",
				__func__);
		ret = -ECONNABORTED;
		/* Aborting the transaction if it's buffer mode */
		if (buff_mode) {
			cam_cmd_reg = readl_relaxed(tsc_device->base +
					TSC_CAM_CMD);
			SET_BIT(CAM_CMD_ABORT, cam_cmd_reg);
			writel_relaxed(cam_cmd_reg, tsc_device->base +
					TSC_CAM_CMD);
		}
		goto finish;
	}

	/* reseting the argument after reading the interrupt type */
	tsc_ci->transaction_state = BEFORE_TRANSACTION;
	spin_unlock_irqrestore(&tsc_ci->spinlock, flags);

	/*
	 * Only on case of read single byte operation, we need to copy the data
	 * to the arg data field
	 */
	if (buff_mode == SINGLE_BYTE_MODE && read_write == READ_TRANSACTION)
		ret = put_user(readl_relaxed(tsc_device->base +
				TSC_CAM_RD_DATA),
				&((struct tsc_single_byte_mode *)arg)->data);

finish:
	if (iova != 0)
		ion_unmap_iommu(tsc_device->iommu_info.ion_client, ion_handle,
			tsc_device->iommu_info.domain_num,
			tsc_device->iommu_info.partition_num);
err_ion_map:
	if (!IS_ERR_OR_NULL(ion_handle))
		ion_free(tsc_device->iommu_info.ion_client, ion_handle);
err_ion_handle:
err_copy_arg:
	tsc_ci->data_busy = false;
	INIT_COMPLETION(tsc_ci->transaction_complete);
	complete_all(&tsc_ci->transaction_finish);
	return ret;
}

/**
 * tsc_personality_change() - change the PCMCIA pins state.
 *
 * @pcmcia_state:	The new state of the PCMCIA pins.
 *
 * Configure the TLMM pins of the PCMCIA according to received state and
 * the current pinctrl configuration of the other pins. This function assums the
 * PCMCIA pinctrl definitions were successfully parsed from the devicetree (this
 * check is done at open device).
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_personality_change(enum tsc_cam_personality pcmcia_state)
{
	int ret = 0;
	struct pinctrl_info *ppinctrl = &tsc_device->pinctrl_info;
	struct pinctrl_current_state *pcurr_state = &ppinctrl->curr_state;
	u32 reg = 0;

	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (pcmcia_state == (enum tsc_cam_personality)pcurr_state->pcmcia_state)
		goto exit;

	/* Transition from current pinctrl state to curr + new pcmcia state */
	switch (pcmcia_state) {
	case TSC_CICAM_PERSONALITY_CI:
		if (pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_card);
		else if (pcurr_state->ts0 && !pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_card);
		else if (!pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_card);
		else
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_card);
		break;
	case TSC_CICAM_PERSONALITY_CIPLUS:
		if (pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts_ci_plus);
		else if (pcurr_state->ts0 && !pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0_ci_plus);
		else if (!pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1_ci_plus);
		else
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ci_plus);
		break;
	case TSC_CICAM_PERSONALITY_DISABLE:
		if (pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->dual_ts);
		else if (pcurr_state->ts0 && !pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts0);
		else if (!pcurr_state->ts0 && pcurr_state->ts1)
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->ts1);
		else
			ret = pinctrl_select_state(ppinctrl->pinctrl,
					ppinctrl->disable);
		break;
	default:
		pr_err("%s: Wrong personality parameter\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (ret != 0) {
		pr_err("%s: error changing PCMCIA pins. ret value = %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto exit;
	}

	/* Update the current pcmcia state in the internal struct */
	pcurr_state->pcmcia_state = (enum pcmcia_state)pcmcia_state;

	/*
	 * Setting CAM TSIF OE to enable I/O transactions for CI/+ cards
	 * or clearing it when moving to disable state
	 */
	if (TSC_CICAM_PERSONALITY_CI == pcmcia_state ||
			TSC_CICAM_PERSONALITY_CIPLUS == pcmcia_state) {
		SET_BIT(TSC_CICAM_TSIF_OE_OFFS, reg);
		writel_relaxed(reg, tsc_device->base + TSC_CICAM_TSIF);
	} else {
		CLEAR_BIT(TSC_CICAM_TSIF_OE_OFFS, reg);
		writel_relaxed(reg, tsc_device->base + TSC_CICAM_TSIF);
	}

exit:
	mutex_unlock(&tsc_device->mutex);
	return ret;
}

/**
 * tsc_reset_cam() - HW reset to the CAM.
 *
 * Toggle the reset pin of the pcmcia to make a HW reset.
 * This function assumes that pinctrl_select_state was already called on the
 * reset pin with its active state (happens during personality change).
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_reset_cam(void)
{
	int ret;
	int reset_gpio = tsc_device->reset_cam_gpio;

	/* Toggle the GPIO to create a reset pulse */
	ret = gpio_direction_output(reset_gpio, 0); /* Make sure it's 0 */
	if (ret != 0)
		goto err;

	ret = gpio_direction_output(reset_gpio, 1); /* Assert */
	if (ret != 0)
		goto err;

	/*
	 * Waiting to enable the CAM to process the assertion before the
	 * deassertion. 1ms is needed for this processing.
	 */
	usleep(1000);

	ret = gpio_direction_output(reset_gpio, 0); /* Deassert */
	if (ret != 0)
		goto err;

	return 0;
err:
	pr_err("%s: Failed writing to reset cam GPIO\n", __func__);
	return ret;
}

/**
 * tsc_reset_registers() - Reset the TSC registers.
 *
 * Write specific reset values to the TSC registers, managed by the driver.
 */
static void tsc_reset_registers(void)
{
	/* Reset state - all mux transfer ext. demod 0 */
	writel_relaxed(0x00000000, tsc_device->base + TSC_MUX_CFG);

	/* Disabling TSIFs inputs, putting polarity to normal, data as serial */
	writel_relaxed(0x02000200, tsc_device->base + TSC_IN_IFC_EXT);
	writel_relaxed(0x02000200, tsc_device->base + TSC_IN_IFC_CFG_INT);

	/* Reseting TSC_FSM_STATE_MASK to represent all the states but poll */
	writel_relaxed(0x3333300F, tsc_device->base + TSC_FSM_STATE_MASK);

	/* Clearing all the CAM interrupt */
	writel_relaxed(0x1F, tsc_device->base + TSC_IRQ_CLR);

	/* Disabling all cam interrupts (enable is done at - open) */
	writel_relaxed(0x00, tsc_device->base + TSC_IRQ_ENA);

	/* Disabling HW polling */
	writel_relaxed(0x00, tsc_device->base + TSC_CIP_CFG);

	/* Reset state - address for read/write buffer */
	writel_relaxed(0x00000000, tsc_device->base + TSC_RD_BUFF_ADDR);
	writel_relaxed(0x00000000, tsc_device->base + TSC_WR_BUFF_ADDR);

	/* Clearing false cd counter */
	writel_relaxed(0x01, tsc_device->base + TSC_FALSE_CD_CLR);
	writel_relaxed(0x00, tsc_device->base + TSC_FALSE_CD_CLR);

	/* Disabling TSIF out to cicam and IO read/write with the CAM */
	writel_relaxed(0x00000000, tsc_device->base + TSC_CICAM_TSIF);
}

/**
 * tsc_disable_tsifs() - Disable all the TSC Tsifs.
 *
 * Disable the TSIFs of the ext. demods, the int. demod and the cam on both
 * directions.
 */
static void tsc_disable_tsifs(void)
{
	u32 reg;

	/* Ext. TSIFs */
	reg = readl_relaxed(tsc_device->base + TSC_IN_IFC_EXT);
	SET_BIT(TSIF_DISABLE_OFFS, reg);
	SET_BIT((TSIF_DISABLE_OFFS + 16), reg);
	writel_relaxed(reg, tsc_device->base + TSC_IN_IFC_EXT);

	/* Int. TSIF and TSIF-in from the CAM */
	reg = readl_relaxed(tsc_device->base + TSC_IN_IFC_CFG_INT);
	SET_BIT(TSIF_DISABLE_OFFS, reg);
	SET_BIT((TSIF_DISABLE_OFFS + 16), reg);
	writel_relaxed(reg, tsc_device->base + TSC_IN_IFC_CFG_INT);
}

/**
 * tsc_power_on_clocks() - power-on the TSC clocks.
 *
 * Power-on the TSC clocks required for Mux and/or CI operations.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_power_on_clocks(void)
{
	int ret = 0;
	unsigned long rate_in_hz = 0;

	/* Enabling the clocks */
	ret = clk_prepare_enable(tsc_device->ahb_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tsc_ahb_clk", __func__);
		return ret;
	}

	/* We need to set the rate of ci clock before enabling it */
	rate_in_hz = clk_round_rate(tsc_device->ci_clk, 1);
	if (clk_set_rate(tsc_device->ci_clk, rate_in_hz)) {
		pr_err("%s: Failed to set rate to tsc_ci clock\n", __func__);
		goto err;
	}

	ret = clk_prepare_enable(tsc_device->ci_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tsc_ci_clk", __func__);
		goto err;
	}

	return ret;
err:
	clk_disable_unprepare(tsc_device->ahb_clk);
	return ret;
}

/**
 * tsc_power_off_clocks() - power-off the TSC clocks.
 *
 * Power-off the TSC clocks required for Mux and/or CI operations.
 */
static void tsc_power_off_clocks(void)
{
	clk_disable_unprepare(tsc_device->ahb_clk);
	clk_disable_unprepare(tsc_device->ci_clk);
}

/**
 * tsc_mux_power_on_clocks() - power-on the TSC Mux clocks.
 *
 * Power-on the TSC clocks required only for Mux operations, and not for CI.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_mux_power_on_clocks(void)
{
	int ret = 0;

	/* Setting the cicam clock rate */
	ret = clk_set_rate(tsc_device->cicam_ts_clk, CICAM_CLK_RATE_7MHZ);
	if (ret != 0) {
		pr_err("%s: Can't set rate for tsc_cicam_ts_clk", __func__);
		goto err_set_rate;
	}

	/* Setting the TSC serial clock rate */
	ret = clk_set_rate(tsc_device->ser_clk, TSC_SER_CLK_RATE);
	if (ret != 0) {
		pr_err("%s: Can't set rate for tsc serial clock", __func__);
		goto err_set_rate;
	}

	/* Setting the TSC parallel clock rate */
	ret = clk_set_rate(tsc_device->par_clk, TSC_PAR_CLK_RATE);
	if (ret != 0) {
		pr_err("%s: Can't set rate for tsc parallel clock", __func__);
		goto err_set_rate;
	}

	/* Enabling the clocks */
	ret = clk_prepare_enable(tsc_device->ser_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tsc_ser_clk", __func__);
		goto err_ser_clk;
	}
	ret = clk_prepare_enable(tsc_device->par_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tsc_par_clk", __func__);
		goto err_par_clk;
	}
	ret = clk_prepare_enable(tsc_device->cicam_ts_clk);
	if (ret != 0) {
		pr_err("%s: Can't start tsc_cicam_ts_clk", __func__);
		goto err_cicam_ts_clk;
	}

	return ret;

err_cicam_ts_clk:
	clk_disable_unprepare(tsc_device->par_clk);
err_par_clk:
	clk_disable_unprepare(tsc_device->ser_clk);
err_ser_clk:
err_set_rate:
	return ret;
}

/**
 * tsc_mux_power_off_clocks() - power-off the TSC Mux clocks.
 *
 * Power-off the TSC clocks required only for Mux operations, and not for CI.
 */
static void tsc_mux_power_off_clocks(void)
{
	clk_disable_unprepare(tsc_device->ser_clk);
	clk_disable_unprepare(tsc_device->par_clk);
	clk_disable_unprepare(tsc_device->cicam_ts_clk);
}

/**
 * tsc_device_power_up() - Power init done by the first device opened.
 *
 * Check if it's the first device and enable the GDSC,power-on the TSC clocks
 * required for both Mux and CI, Vote for the bus and reset the registers to a
 * known default values.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_device_power_up(void)
{
	int ret = 0;

	if (mutex_lock_interruptible(&tsc_device->mutex))
		return -ERESTARTSYS;

	if (tsc_device->num_device_open > 0)
		goto not_first_device;

	/* Enable the GDSC */
	ret = regulator_enable(tsc_device->gdsc);
	if (ret != 0) {
		pr_err("%s: Failed to enable regulator\n", __func__);
		goto err_regulator;
	}

	/* Power-on the clocks needed by Mux and CI */
	ret = tsc_power_on_clocks();
	if (ret != 0)
		goto err_power_clocks;

	/* Voting for bus bandwidth */
	if (tsc_device->bus_client) {
		ret = msm_bus_scale_client_update_request
				(tsc_device->bus_client, 1);
		if (ret) {
			pr_err("%s: Can't enable bus\n", __func__);
			goto err_bus;
		}
	}

	/* Reset the TSC TLMM pins to a default state */
	ret = pinctrl_select_state(tsc_device->pinctrl_info.pinctrl,
			tsc_device->pinctrl_info.disable);
	if (ret != 0) {
		pr_err("%s: Failed to disable the TLMM pins\n", __func__);
		goto err_pinctrl;
	}
	/* Update the current pinctrl state in the internal struct */
	tsc_device->pinctrl_info.curr_state.ts0 = false;
	tsc_device->pinctrl_info.curr_state.ts1 = false;
	tsc_device->pinctrl_info.curr_state.pcmcia_state =
			TSC_CICAM_PERSONALITY_DISABLE;

	/* Reset TSC registers to a default known state */
	tsc_reset_registers();

not_first_device:
	tsc_device->num_device_open++;
	mutex_unlock(&tsc_device->mutex);
	return ret;

err_pinctrl:
	if (tsc_device->bus_client)
		msm_bus_scale_client_update_request(tsc_device->bus_client, 0);
err_bus:
	tsc_power_off_clocks();
err_power_clocks:
	regulator_disable(tsc_device->gdsc);
err_regulator:
	mutex_unlock(&tsc_device->mutex);
	return ret;
}

/**
 * tsc_device_power_off() - Power off done by the last device closed.
 *
 * Check if it's the last device and unvote the bus, power-off the TSC clocks
 * required for both Mux and CI, disable the TLMM pins and disable the GDSC.
 */
static void tsc_device_power_off(void)
{
	mutex_lock(&tsc_device->mutex);

	if (tsc_device->num_device_open > 1)
		goto not_last_device;

	pinctrl_select_state(tsc_device->pinctrl_info.pinctrl,
			tsc_device->pinctrl_info.disable);
	if (tsc_device->bus_client)
		msm_bus_scale_client_update_request(tsc_device->bus_client, 0);

	tsc_power_off_clocks();
	regulator_disable(tsc_device->gdsc);

not_last_device:
	tsc_device->num_device_open--;
	mutex_unlock(&tsc_device->mutex);
}


/************************** TSC file operations **************************/
/**
 * tsc_mux_open() - init the TSC Mux char device.
 *
 * @inode:	The inode associated with the TSC Mux device.
 * @flip:	The file pointer associated with the TSC Mux device.
 *
 * Enables only one open Mux device.
 * Init all the data structures and vote for all the power resources needed.
 * Manage reference counters for initiating resources upon first open.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_mux_open(struct inode *inode, struct file *filp)
{
	struct tsc_mux_chdev *tsc_mux;
	int ret = 0;
	u32 ena_reg;

	if (mutex_lock_interruptible(&tsc_device->mux_chdev.mutex))
		return -ERESTARTSYS;

	if (tsc_device->num_mux_opened > 0) {
		pr_err("%s: Too many devices open\n", __func__);
		mutex_unlock(&tsc_device->mux_chdev.mutex);
		return -EMFILE;
	}
	tsc_device->num_mux_opened++;

	tsc_mux = container_of(inode->i_cdev, struct tsc_mux_chdev, cdev);
	filp->private_data = tsc_mux;

	/* Init all resources if it's the first device (checked inside) */
	ret = tsc_device_power_up();
	if (ret != 0)
		goto err_first_device;

	/* Power-on the Mux clocks */
	ret = tsc_mux_power_on_clocks();
	if (ret != 0)
		goto err_mux_clocks;

	/* Init TSC Mux args */
	spin_lock_init(&tsc_mux->spinlock);
	init_waitqueue_head(&tsc_mux->poll_queue);
	tsc_mux->rate_interrupt = false;

	/* Enabling TSC Mux cam interrupt of rate mismatch */
	ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
	SET_BIT(CAM_IRQ_RATE_MISMATCH_OFFS, ena_reg);
	writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

	mutex_unlock(&tsc_device->mux_chdev.mutex);

	return ret;

err_mux_clocks:
	/* De-init all resources if it's the only device (checked inside) */
	tsc_device_power_off();
err_first_device:
	tsc_device->num_mux_opened--;
	mutex_unlock(&tsc_device->mux_chdev.mutex);
	return ret;
}

/**
 * tsc_ci_open() - init the TSC CI char device.
 *
 * @inode:	The inode associated with the TSC Mux device.
 * @flip:	The file pointer associated with the TSC Mux device.
 *
 * Enables only one open CI device.
 * Init all the data structures and vote for all the power resources needed.
 * Manage reference counters for initiating resources upon first open.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_ci_open(struct inode *inode, struct file *filp)
{
	struct tsc_ci_chdev *tsc_ci;
	int ret = 0;
	u32 ena_reg;

	if (mutex_lock_interruptible(&tsc_device->ci_chdev.mutex))
		return -ERESTARTSYS;

	if (tsc_device->num_ci_opened > 0) {
		pr_err("%s: Too many devices open\n", __func__);
		mutex_unlock(&tsc_device->ci_chdev.mutex);
		return -EMFILE;
	}

	if (!tsc_device->pinctrl_info.is_pcmcia) {
		pr_err("%s: No pcmcia pinctrl definitions were found in the TSC devicetree\n",
				__func__);
		mutex_unlock(&tsc_device->ci_chdev.mutex);
		return -EPERM;
	}

	tsc_device->num_ci_opened++;

	tsc_ci = container_of(inode->i_cdev, struct tsc_ci_chdev, cdev);
	filp->private_data = tsc_ci;

	/* Init all resources if it's the first device (checked inside) */
	ret = tsc_device_power_up();
	if (ret != 0)
		goto err_first_device;

	/* powering-up the tspp2 and VBIF clocks */
	ret = tsc_power_on_buff_mode_clocks();
	if (ret != 0)
		goto err_buff_clocks;

	/* Request reset CAM GPIO */
	ret = gpio_request(tsc_device->reset_cam_gpio, "tsc_ci_reset");
	if (ret != 0) {
		pr_err("%s: Failed to request reset CAM GPIO\n", __func__);
		goto err_gpio_req;
	}

	/* Set the reset line to default "no card" state */
	ret = gpio_direction_output(tsc_device->reset_cam_gpio, 1);
	if (ret != 0) {
		pr_err("%s: Failed to assert the reset CAM GPIO\n", __func__);
		goto err_assert;
	}

	/* Attach the iommu group to support the required memory mapping */
	if (!tsc_iommu_bypass) {
		ret = iommu_attach_group(tsc_device->iommu_info.domain,
				tsc_device->iommu_info.group);
		if (ret != 0) {
			pr_err("%s: iommu_attach_group failed\n", __func__);
			goto err_iommu_attach;
		}
	}

	/* Init TSC CI args */
	spin_lock_init(&tsc_ci->spinlock);
	init_waitqueue_head(&tsc_ci->poll_queue);
	tsc_ci->transaction_state = BEFORE_TRANSACTION;
	tsc_ci->data_busy = false;
	tsc_device->card_power = false;

	/*
	 * Init hw card status flag according to the pins' state.
	 * No need to protect from interrupt because the handler is not
	 * registred yet.
	 */
	tsc_update_hw_card_status();
	tsc_ci->card_status = tsc_device->hw_card_status;

	/* If a card is already inserted - need to power up the card */
	if (tsc_device->hw_card_status == TSC_CARD_STATUS_DETECTED) {
		ret = tsc_card_power_up();
		if (ret != 0)
			pr_err("%s: card power-up failed\n", __func__);
		else
			tsc_device->card_power = true;
	}

	/* Enabling the TSC CI cam interrupts: EOT and Err */
	ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
	SET_BIT(CAM_IRQ_EOT_OFFS, ena_reg);
	SET_BIT(CAM_IRQ_ERR_OFFS, ena_reg);
	writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

	/* Registering the CAM cmd interrupt handler */
	ret = request_irq(tsc_device->cam_cmd_irq, tsc_cam_cmd_irq_handler,
			IRQF_SHARED, dev_name(&tsc_device->pdev->dev),
			tsc_device);
	if (ret) {
		pr_err("%s: failed to request TSC IRQ %d : %d",
				__func__, tsc_device->cam_cmd_irq, ret);
		goto err_cam_irq;
	}

	/*
	 * Registering the card detect interrupt handler (this interrupt is
	 * enabled by default, right after this registration)
	 */
	ret = request_threaded_irq(tsc_device->card_detection_irq,
			NULL, tsc_card_detect_irq_thread_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_RISING,
			dev_name(&tsc_device->pdev->dev), tsc_device);
	if (ret) {
		pr_err("%s: failed to request TSC IRQ %d : %d",
				__func__, tsc_device->card_detection_irq, ret);
		goto err_card_irq;
	}

	mutex_unlock(&tsc_device->ci_chdev.mutex);

	return ret;

err_card_irq:
	free_irq(tsc_device->cam_cmd_irq, tsc_device);
err_cam_irq:
	if (!tsc_iommu_bypass)
		iommu_detach_group(tsc_device->iommu_info.domain,
				tsc_device->iommu_info.group);
err_iommu_attach:
	gpio_free(tsc_device->reset_cam_gpio);
err_assert:
err_gpio_req:
	tsc_power_off_buff_mode_clocks();
err_buff_clocks:
	/* De-init all resources if it's the only device (checked inside) */
	tsc_device_power_off();
err_first_device:
	tsc_device->num_ci_opened--;
	mutex_unlock(&tsc_device->ci_chdev.mutex);
	return ret;
}

/**
 * tsc_mux_release() - Release and close the TSC Mux char device.
 *
 * @inode:	The inode associated with the TSC Mux device.
 * @flip:	The file pointer associated with the TSC Mux device.
 *
 * Release all the resources allocated for the Mux device and unvote power
 * resources.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_mux_release(struct inode *inode, struct file *filp)
{
	struct tsc_mux_chdev *tsc_mux;
	u32 ena_reg;

	tsc_mux = filp->private_data;
	if (!tsc_mux)
		return -EINVAL;

	mutex_lock(&tsc_mux->mutex);

	tsc_mux_power_off_clocks();

	/* Disable the TSIFs */
	tsc_disable_tsifs();
	/* Disabling rate mismatch interrupt */
	ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
	CLEAR_BIT(CAM_IRQ_RATE_MISMATCH_OFFS, ena_reg);
	writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

	tsc_device_power_off();

	tsc_device->num_mux_opened--;
	mutex_unlock(&tsc_mux->mutex);

	return 0;
}

/**
 * tsc_ci_release() - Release and close the TSC CI char device.
 *
 * @inode:	The inode associated with the TSC CI device.
 * @flip:	The file pointer associated with the TSC CI device.
 *
 * Release all the resources allocated for the CI device and unvote power
 * resources.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_ci_release(struct inode *inode, struct file *filp)
{
	struct tsc_ci_chdev *tsc_ci;
	u32 ena_reg;
	int ret;

	tsc_ci = filp->private_data;
	if (!tsc_ci)
		return -EINVAL;

	mutex_lock(&tsc_ci->mutex);

	/* If in the middle of a data transaction- wake-up completion */
	if (tsc_ci->data_busy) {
		/* Closing the device is similar in behavior to card removal */
		tsc_ci->transaction_state = TRANSACTION_CARD_REMOVED;
		mutex_unlock(&tsc_ci->mutex);
		complete_all(&tsc_ci->transaction_complete);
		wait_for_completion(&tsc_ci->transaction_finish);
		mutex_lock(&tsc_ci->mutex);
	}

	/* clearing EOT and ERR interrupts */
	ena_reg = readl_relaxed(tsc_device->base + TSC_IRQ_ENA);
	CLEAR_BIT(CAM_IRQ_EOT_OFFS, ena_reg);
	CLEAR_BIT(CAM_IRQ_ERR_OFFS, ena_reg);
	writel_relaxed(ena_reg, tsc_device->base + TSC_IRQ_ENA);

	/* Cancel the  interrupt handlers registration */
	free_irq(tsc_device->card_detection_irq, tsc_device);
	free_irq(tsc_device->cam_cmd_irq, tsc_device);

	/* power down the card interface if it's currently powered up */
	if (tsc_device->hw_card_status == TSC_CARD_STATUS_DETECTED &&
			tsc_device->card_power) {
		ret = tsc_card_power_down();
		if (ret != 0)
			pr_err("%s: card power-down failed\n", __func__);
	}

	if (!tsc_iommu_bypass)
		iommu_detach_group(tsc_device->iommu_info.domain,
				tsc_device->iommu_info.group);

	gpio_free(tsc_device->reset_cam_gpio);

	tsc_power_off_buff_mode_clocks();
	tsc_device_power_off();

	tsc_device->num_ci_opened--;
	mutex_unlock(&tsc_ci->mutex);

	return 0;
}

/**
 * tsc_mux_poll() - Perform polling on a designated wait-queue.
 *
 * @flip:	The file pointer associated with the TSC Mux device.
 * @p:		The poll-table struct of the kernel.
 *
 * Add the TSC Mux wait-queue to the poll-table. Poll until a rate mismatch
 * interrupt is received.
 *
 * Return 0 on success, error value otherwise.
 */
static unsigned int tsc_mux_poll(struct file *filp, struct poll_table_struct *p)
{
	unsigned long flags;
	unsigned int mask = 0;
	struct tsc_mux_chdev *tsc_mux;

	tsc_mux = filp->private_data;
	if (!tsc_mux)
		return -EINVAL;

	/* register the wait queue for rate mismatch interrupt */
	poll_wait(filp, &tsc_mux->poll_queue, p);

	/* Setting the mask upon rate mismatch irq and clearing the flag */
	spin_lock_irqsave(&tsc_mux->spinlock, flags);
	if (tsc_mux->rate_interrupt) {
		mask = POLLPRI;
		tsc_mux->rate_interrupt = false;
	}
	spin_unlock_irqrestore(&tsc_mux->spinlock, flags);

	return mask;
}

/**
 * tsc_ci_poll() - Perform polling on a designated wait-queue.
 *
 * @flip:	The file pointer associated with the TSC CI device.
 * @p:		The poll-table struct of the kernel.
 *
 * Add the TSC Mux wait-queue to the poll-table. Poll until a card detection
 * interrupt is received.
 *
 * Return 0 on success, error value otherwise.
 */
static unsigned int tsc_ci_poll(struct file *filp, struct poll_table_struct *p)
{
	unsigned int mask = 0;

	struct tsc_ci_chdev *tsc_ci = filp->private_data;
	if (!tsc_ci)
		return -EINVAL;

	/* Register the wait queue for card detection interrupt */
	poll_wait(filp, &tsc_ci->poll_queue, p);

	/* Setting the mask upon card detect irq and update ci card state */
	if (mutex_lock_interruptible(&tsc_ci->mutex))
		return -ERESTARTSYS;
	if (tsc_ci->card_status != tsc_device->hw_card_status) {
		mask = POLLPRI;
		tsc_ci->card_status = tsc_device->hw_card_status;
	}
	mutex_unlock(&tsc_ci->mutex);

	return mask;
}

/**
 * tsc_mux_ioctl() - Handle IOCTLs sent from user-space application.
 *
 * @flip:	The file pointer associated with the TSC Mux device.
 * @cmd:	The IOCTL code sent
 * @arg:	The IOCTL argument (if the IOCTL receives an argument)
 *
 * Verify the validity of the IOCTL sent and handle it by updating the
 * appropriate register or calling a function that handle the IOCTL operation.
 *
 * Return 0 on success, error value otherwise.
 */
static long tsc_mux_ioctl(struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct tsc_mux_chdev *tsc_mux;
	struct tsc_route tsc_route;
	struct tsc_tsif_params tsif_params;

	tsc_mux = filp->private_data;
	if (!tsc_mux)
		return -EINVAL;

	if (mutex_lock_interruptible(&tsc_mux->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case TSC_CONFIG_ROUTE:
		if (!arg || copy_from_user(&tsc_route, (void *)arg,
				sizeof(struct tsc_route))) {
			ret = -EFAULT;
			goto err;
		}
		ret = tsc_route_mux(tsc_mux, tsc_route.source, tsc_route.dest);
		break;
	case TSC_ENABLE_INPUT:
		ret = tsc_enable_disable_tsif(tsc_mux, arg, TSIF_INPUT_ENABLE);
		break;
	case TSC_DISABLE_INPUT:
		ret = tsc_enable_disable_tsif(tsc_mux, arg, TSIF_INPUT_DISABLE);
		break;
	case TSC_SET_TSIF_CONFIG:
		if (!arg || copy_from_user(&tsif_params, (void *)arg,
				sizeof(struct tsc_tsif_params))) {
			ret = -EFAULT;
			goto err;
		}
		ret = tsc_config_tsif(tsc_mux, &tsif_params);
		break;
	case TSC_CLEAR_RATE_MISMATCH_IRQ:
		tsc_enable_rate_irq(tsc_mux);
		break;
	case TSC_CICAM_SET_CLOCK:
		ret = tsc_set_cicam_clk(arg);
		break;
	default:
		ret = -EINVAL;
		pr_err("%s: Unknown ioctl %i", __func__, cmd);
	}

err:
	mutex_unlock(&tsc_mux->mutex);
	return ret;
}

/**
 * tsc_ci_ioctl() - Handle IOCTLs sent from user-space application.
 *
 * @flip:	The file pointer associated with the TSC CI device.
 * @cmd:	The IOCTL code sent
 * @arg:	The IOCTL argument (if the IOCTL receives an argument)
 *
 * Verify the validity of the IOCTL sent and handle it by updating the
 * appropriate register or calling a function that handle the IOCTL operation.
 *
 * Return 0 on success, error value otherwise.
 */
static long tsc_ci_ioctl(struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct tsc_ci_chdev *tsc_ci;
	unsigned long flags;

	tsc_ci = filp->private_data;
	if (!tsc_ci)
		return -EINVAL;

	if (mutex_lock_interruptible(&tsc_ci->mutex))
		return -ERESTARTSYS;

	switch (cmd) {

	case TSC_CAM_RESET:
		ret = tsc_reset_cam();
		break;
	case TSC_CICAM_PERSONALITY_CHANGE:
		ret = tsc_personality_change(arg);
		break;
	case TSC_GET_CARD_STATUS:
		spin_lock_irqsave(&tsc_ci->spinlock, flags);
		tsc_ci->card_status = tsc_device->hw_card_status;
		ret = __put_user(tsc_ci->card_status,
				(enum tsc_card_status __user *)arg);
		spin_unlock_irqrestore(&tsc_ci->spinlock, flags);
		break;
	case TSC_READ_CAM_MEMORY:
		ret = tsc_data_transaction(tsc_ci, MEMORY_TRANSACTION,
				READ_TRANSACTION, SINGLE_BYTE_MODE, arg);
		break;
	case TSC_WRITE_CAM_MEMORY:
		ret = tsc_data_transaction(tsc_ci, MEMORY_TRANSACTION,
				WRITE_TRANSACTION, SINGLE_BYTE_MODE, arg);
		break;
	case TSC_READ_CAM_IO:
		ret = tsc_data_transaction(tsc_ci, IO_TRANSACTION,
				READ_TRANSACTION, SINGLE_BYTE_MODE, arg);
		break;
	case TSC_WRITE_CAM_IO:
		ret = tsc_data_transaction(tsc_ci, IO_TRANSACTION,
				WRITE_TRANSACTION, SINGLE_BYTE_MODE, arg);
		break;
	case TSC_READ_CAM_BUFFER:
		ret = tsc_data_transaction(tsc_ci, IO_TRANSACTION,
				READ_TRANSACTION, BUFFER_MODE, arg);
		break;
	case TSC_WRITE_CAM_BUFFER:
		ret = tsc_data_transaction(tsc_ci, IO_TRANSACTION,
				WRITE_TRANSACTION, BUFFER_MODE, arg);
		break;
	default:
		ret = -EINVAL;
		pr_err("%s: Unknown ioctl %i\n", __func__, cmd);
	}

	mutex_unlock(&tsc_ci->mutex);
	return ret;
}

/************************** Probe helper-functions **************************/
/**
 * tsc_init_char_driver() - Initialize a character driver.
 *
 * @pcdev:		A pointer to the cdev structure to initialize.
 * @pfops:		A pointer to the file_operations for this device.
 * @device_number:	A pointer that will store the device number.
 * @device:		A pointer that will store the new device upon success.
 * @name:		A string for the device's name.
 *
 * Create a new character device driver inside the TSC class. The new device
 * is created under "/dev/<name>0".
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_init_char_driver(struct cdev *pcdev,
		const struct file_operations *pfops,
		dev_t *pdevice_number,
		struct device *pdevice,
		const char *name)
{
	int ret = 0;

	/* Allocate device number for the char device driver */
	ret = alloc_chrdev_region(pdevice_number, 0, 1, name);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n",  name, ret);
		goto err_devrgn;
	}

	/* initializing the char device structures with file operations */
	cdev_init(pcdev, pfops);
	pcdev->owner = THIS_MODULE;

	/* adding the char device structures to the VFS */
	ret = cdev_add(pcdev, *pdevice_number, 1);
	if (ret != 0) {
		pr_err("%s%d: cdev_add failed\n", name, MINOR(*pdevice_number));
		goto err_cdev_add;
	}

	/* create the char devices under "/dev/" and register them to sysfs */
	pdevice = device_create(tsc_class, NULL, pcdev->dev, NULL, "%s%d", name,
			MINOR(*pdevice_number));
	if (IS_ERR(pdevice)) {
		pr_err("%s%d device_create failed\n", name,
				MINOR(*pdevice_number));
		ret = PTR_ERR(pdevice); /* PTR_ERR return -ENOMEM */
		goto err_device_create;
	}

	return  ret;

err_device_create:
	cdev_del(pcdev);
err_cdev_add:
	unregister_chrdev_region(*pdevice_number, 1);
err_devrgn:
	return ret;
}

/**
 * tsc_get_pinctrl() - Get the TSC pinctrl definitions.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Get the pinctrl states' handles from the device tree. The function doesn't
 * enforce wrong pinctrl definitions, i.e. it's the client's responsibility to
 * define all the necessary states for the board being used.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_get_pinctrl(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	tsc_device->pinctrl_info.pinctrl = pinctrl;

	/* get all the states handles */
	tsc_device->pinctrl_info.disable =
			pinctrl_lookup_state(pinctrl, "disable");
	tsc_device->pinctrl_info.ts0 =
			pinctrl_lookup_state(pinctrl, "ts-in-0");
	tsc_device->pinctrl_info.ts1 =
			pinctrl_lookup_state(pinctrl, "ts-in-1");
	tsc_device->pinctrl_info.dual_ts =
			pinctrl_lookup_state(pinctrl, "dual-ts");
	tsc_device->pinctrl_info.pc_card =
			pinctrl_lookup_state(pinctrl, "pc-card");
	tsc_device->pinctrl_info.ci_card =
			pinctrl_lookup_state(pinctrl, "ci-card");
	tsc_device->pinctrl_info.ci_plus =
			pinctrl_lookup_state(pinctrl, "ci-plus");
	tsc_device->pinctrl_info.ts0_pc_card =
			pinctrl_lookup_state(pinctrl, "ts-in-0-pc-card");
	tsc_device->pinctrl_info.ts0_ci_card =
			pinctrl_lookup_state(pinctrl, "ts-in-0-ci-card");
	tsc_device->pinctrl_info.ts0_ci_plus =
			pinctrl_lookup_state(pinctrl, "ts-in-0-ci-plus");
	tsc_device->pinctrl_info.ts1_pc_card =
			pinctrl_lookup_state(pinctrl, "ts-in-1-pc-card");
	tsc_device->pinctrl_info.ts1_ci_card =
			pinctrl_lookup_state(pinctrl, "ts-in-1-ci-card");
	tsc_device->pinctrl_info.ts1_ci_plus =
			pinctrl_lookup_state(pinctrl, "ts-in-1-ci-plus");
	tsc_device->pinctrl_info.dual_ts_pc_card =
			pinctrl_lookup_state(pinctrl, "dual-ts-pc-card");
	tsc_device->pinctrl_info.dual_ts_ci_card =
			pinctrl_lookup_state(pinctrl, "dual-ts-ci-card");
	tsc_device->pinctrl_info.dual_ts_ci_plus =
			pinctrl_lookup_state(pinctrl, "dual-ts-ci-plus");

	if (IS_ERR(tsc_device->pinctrl_info.disable)) {
		pr_err("%s: Unable to get pinctrl disable state handle\n",
				__func__);
		return -EINVAL;
	}

	/* Basic checks to inquire what pinctrl states are available */
	if (IS_ERR(tsc_device->pinctrl_info.ts0))
		tsc_device->pinctrl_info.is_ts0 = false;
	else
		tsc_device->pinctrl_info.is_ts0 = true;

	if (IS_ERR(tsc_device->pinctrl_info.ts1))
		tsc_device->pinctrl_info.is_ts1 = false;
	else
		tsc_device->pinctrl_info.is_ts1 = true;

	if (IS_ERR(tsc_device->pinctrl_info.pc_card) ||
			IS_ERR(tsc_device->pinctrl_info.ci_card) ||
			IS_ERR(tsc_device->pinctrl_info.ci_plus))
		tsc_device->pinctrl_info.is_pcmcia = false;
	else
		tsc_device->pinctrl_info.is_pcmcia = true;

	return 0;
}

/**
 * tsc_get_regulator_bus() - Get the TSC regulator and register the bus client.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_get_regulator_bus(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *tsc_bus_pdata = NULL;

	/* Reading the GDSC info */
	tsc_device->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(tsc_device->gdsc)) {
		dev_err(&pdev->dev, "%s: Failed to get vdd power regulator\n",
				__func__);
		return PTR_ERR(tsc_device->gdsc);
	}

	/* Reading the bus platform data */
	tsc_bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (tsc_bus_pdata == NULL) {
		dev_err(&pdev->dev, "%s: Could not find the bus property. Continue anyway...\n",
				__func__);
	}

	/* Register the bus client */
	if (tsc_bus_pdata) {
		tsc_device->bus_client =
				msm_bus_scale_register_client(tsc_bus_pdata);
		if (!tsc_device->bus_client) {
			dev_err(&pdev->dev, "%s: Unable to register bus client\n",
					__func__);
			goto err;
		}
	}

	return 0;
err:
	devm_regulator_put(tsc_device->gdsc);
	return -EINVAL;
}

/**
 * tsc_get_irqs() - Get the TSC IRQ numbers and map the cam irq.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Read the irq numbers from the platform device information.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_get_irqs(struct platform_device *pdev)
{
	int irq;

	irq = platform_get_irq_byname(pdev, "cam-cmd");
	if (irq > 0) {
		tsc_device->cam_cmd_irq = irq;
	} else {
		dev_err(&pdev->dev, "%s: Failed to get CAM_CMD IRQ = %d",
				__func__, irq);
		goto err;
	}

	irq = platform_get_irq_byname(pdev, "card-detect");
	if (irq > 0) {
		tsc_device->card_detection_irq = irq;
	} else {
		dev_err(&pdev->dev, "%s: Failed to get CARD_DETECT IRQ = %d",
				__func__, irq);
		goto err;
	}

	return 0;
err:
	tsc_device->cam_cmd_irq = 0;
	tsc_device->card_detection_irq = 0;

	return -EINVAL;
}

/**
 * tsc_map_io_memory() - Map memory resources to kernel space.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_map_io_memory(struct platform_device *pdev)
{
	struct resource *registers_mem;

	/* Reading memory resources */
	registers_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"tsc-base");
	if (!registers_mem) {
		dev_err(&pdev->dev, "%s: Missing tsc-base MEM resource",
				__func__);
		return -EINVAL;
	}

	tsc_device->base = ioremap(registers_mem->start,
			resource_size(registers_mem));
	if (!tsc_device->base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		return -ENXIO;
	}

	return 0;
}

/**
 * tsc_clocks_put() - Put the clocks
 */
static void tsc_clocks_put(void)
{
	if (tsc_device->ahb_clk)
		clk_put(tsc_device->ahb_clk);
	if (tsc_device->ci_clk)
		clk_put(tsc_device->ci_clk);
	if (tsc_device->ser_clk)
		clk_put(tsc_device->ser_clk);
	if (tsc_device->par_clk)
		clk_put(tsc_device->par_clk);
	if (tsc_device->cicam_ts_clk)
		clk_put(tsc_device->cicam_ts_clk);
	if (tsc_device->tspp2_core_clk)
		clk_put(tsc_device->tspp2_core_clk);
	if (tsc_device->vbif_tspp2_clk)
		clk_put(tsc_device->vbif_tspp2_clk);
	if (tsc_device->vbif_ahb_clk)
		clk_put(tsc_device->vbif_ahb_clk);
	if (tsc_device->vbif_axi_clk)
		clk_put(tsc_device->vbif_axi_clk);

	tsc_device->ahb_clk = NULL;
	tsc_device->ci_clk = NULL;
	tsc_device->ser_clk = NULL;
	tsc_device->par_clk = NULL;
	tsc_device->cicam_ts_clk = NULL;
	tsc_device->tspp2_core_clk = NULL;
	tsc_device->vbif_tspp2_clk = NULL;
	tsc_device->vbif_ahb_clk = NULL;
	tsc_device->vbif_axi_clk = NULL;
}

/**
 * tsc_clocks_get() - Get the TSC clocks
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_clocks_get(struct platform_device *pdev)
{
	int ret = 0;

	tsc_device->ahb_clk = clk_get(&pdev->dev, "bcc_tsc_ahb_clk");
	if (IS_ERR(tsc_device->ahb_clk)) {
		pr_err("%s: Failed to get bcc_tsc_ahb_clk\n", __func__);
		ret = PTR_ERR(tsc_device->ahb_clk);
		goto ahb_err;
	}

	tsc_device->ci_clk = clk_get(&pdev->dev, "bcc_tsc_ci_clk");
	if (IS_ERR(tsc_device->ci_clk)) {
		pr_err("%s: Failed to get bcc_tsc_ci_clk\n", __func__);
		ret = PTR_ERR(tsc_device->ci_clk);
		goto ci_err;
	}

	tsc_device->ser_clk = clk_get(&pdev->dev, "bcc_tsc_ser_clk");
	if (IS_ERR(tsc_device->ser_clk)) {
		pr_err("%s: Failed to get bcc_tsc_ser_clk\n", __func__);
		ret = PTR_ERR(tsc_device->ser_clk);
		goto ser_err;
	}

	tsc_device->par_clk = clk_get(&pdev->dev, "bcc_tsc_par_clk");
	if (IS_ERR(tsc_device->par_clk)) {
		pr_err("%s: Failed to get bcc_tsc_par_clk", __func__);
		ret = PTR_ERR(tsc_device->par_clk);
		goto par_err;
	}

	tsc_device->cicam_ts_clk = clk_get(&pdev->dev, "bcc_tsc_cicam_ts_clk");
	if (IS_ERR(tsc_device->cicam_ts_clk)) {
		pr_err("%s: Failed to get bcc_tsc_cicam_ts_clk", __func__);
		ret = PTR_ERR(tsc_device->cicam_ts_clk);
		goto cicam_err;
	}

	tsc_device->tspp2_core_clk = clk_get(&pdev->dev, "bcc_tspp2_core_clk");
	if (IS_ERR(tsc_device->tspp2_core_clk)) {
		pr_err("%s: Failed to get bcc_tspp2_core_clk", __func__);
		ret = PTR_ERR(tsc_device->tspp2_core_clk);
		goto tspp2_err;
	}

	tsc_device->vbif_tspp2_clk = clk_get(&pdev->dev, "bcc_vbif_tspp2_clk");
	if (IS_ERR(tsc_device->vbif_tspp2_clk)) {
		pr_err("%s: Failed to get bcc_vbif_tspp2_clk", __func__);
		ret = PTR_ERR(tsc_device->vbif_tspp2_clk);
		goto vbif_tspp2_err;
	}

	tsc_device->vbif_ahb_clk = clk_get(&pdev->dev, "iface_vbif_clk");
	if (IS_ERR(tsc_device->vbif_ahb_clk)) {
		pr_err("%s: Failed to get bcc_vbif_ahb_clk", __func__);
		ret = PTR_ERR(tsc_device->vbif_ahb_clk);
		goto vbif_ahb_err;
	}

	tsc_device->vbif_axi_clk = clk_get(&pdev->dev, "vbif_core_clk");
	if (IS_ERR(tsc_device->vbif_axi_clk)) {
		pr_err("%s: Failed to get bcc_vbif_axi_clk", __func__);
		ret = PTR_ERR(tsc_device->vbif_axi_clk);
		goto vbif_axi_err;
	}

	return ret;

vbif_axi_err:
	tsc_device->vbif_axi_clk = NULL;
	clk_put(tsc_device->vbif_ahb_clk);
vbif_ahb_err:
	tsc_device->vbif_ahb_clk = NULL;
	clk_put(tsc_device->vbif_tspp2_clk);
vbif_tspp2_err:
	tsc_device->vbif_tspp2_clk = NULL;
	clk_put(tsc_device->tspp2_core_clk);
tspp2_err:
	tsc_device->tspp2_core_clk = NULL;
	clk_put(tsc_device->cicam_ts_clk);
cicam_err:
	tsc_device->cicam_ts_clk = NULL;
	clk_put(tsc_device->par_clk);
par_err:
	tsc_device->par_clk = NULL;
	clk_put(tsc_device->ser_clk);
ser_err:
	tsc_device->ser_clk = NULL;
	clk_put(tsc_device->ci_clk);
ci_err:
	tsc_device->ci_clk = NULL;
	clk_put(tsc_device->ahb_clk);
ahb_err:
	tsc_device->ahb_clk = NULL;
	return ret;
}

/**
 * tsc_free_iommu_info() - Free IOMMU information.
 */
static void tsc_free_iommu_info(void)
{
	if (tsc_device->iommu_info.group)  {
		iommu_group_put(tsc_device->iommu_info.group);
		tsc_device->iommu_info.group = NULL;
	}

	if (tsc_device->iommu_info.ion_client) {
		ion_client_destroy(tsc_device->iommu_info.ion_client);
		tsc_device->iommu_info.ion_client = NULL;
	}

	tsc_device->iommu_info.domain = NULL;
	tsc_device->iommu_info.domain_num = -1;
	tsc_device->iommu_info.partition_num = -1;
}

/**
 * tsc_get_iommu_info() - Get IOMMU information.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_get_iommu_info(struct platform_device *pdev)
{
	int ret = 0;

	/* Create a new ION client used by tsc ci to allocate memory */
	tsc_device->iommu_info.ion_client = msm_ion_client_create(UINT_MAX,
			"tsc_client");
	if (IS_ERR_OR_NULL(tsc_device->iommu_info.ion_client)) {
		pr_err("%s: error in ion_client_create", __func__);
		ret = PTR_ERR(tsc_device->iommu_info.ion_client);
		if (!ret)
			ret = -ENOMEM;
		tsc_device->iommu_info.ion_client = NULL;
		goto err_client;
	}

	/* Find the iommu group by the name obtained from the device tree */
	tsc_device->iommu_info.group =
		iommu_group_find(tsc_device->iommu_info.iommu_group_name);
	if (!tsc_device->iommu_info.group) {
		pr_err("%s: error in iommu_group_find", __func__);
		ret = -EINVAL;
		goto err_group;
	}

	/* Get the domain associated with the iommu group */
	tsc_device->iommu_info.domain =
			iommu_group_get_iommudata(tsc_device->iommu_info.group);
	if (IS_ERR_OR_NULL(tsc_device->iommu_info.domain)) {
		pr_err("%s: iommu_group_get_iommudata failed", __func__);
		ret = -EINVAL;
		goto err_domain;
	}

	/* Get the domain number */
	tsc_device->iommu_info.domain_num =
			msm_find_domain_no(tsc_device->iommu_info.domain);

	return ret;

err_domain:
	iommu_group_put(tsc_device->iommu_info.group);
	tsc_device->iommu_info.group = NULL;
err_group:
	ion_client_destroy(tsc_device->iommu_info.ion_client);
	tsc_device->iommu_info.ion_client = NULL;
err_client:
	return ret;
}

/**
 * tsc_parse_dt() - Parse device-tree data and save it.
 *
 * @pdev:	A pointer to the TSC platform device.
 *
 * Return 0 on success, error value otherwise.
 */
static int tsc_parse_dt(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *iommu_pnode;
	int ret;

	/* Check that power regulator property exist  */
	if (!of_get_property(node, "vdd-supply", NULL)) {
		dev_err(&pdev->dev, "%s: Could not find vdd-supply property\n",
				__func__);
		return -EINVAL;
	}

	/* Reading IOMMU group label by obtaining the group's phandle */
	iommu_pnode = of_parse_phandle(node, "qcom,iommu-group", 0);
	if (!iommu_pnode) {
		dev_err(&pdev->dev, "%s: Couldn't find iommu-group property\n",
				__func__);
		return -EINVAL;
	}
	ret = of_property_read_string(iommu_pnode, "label",
			&tsc_device->iommu_info.iommu_group_name);
	of_node_put(iommu_pnode);
	if (ret) {
		dev_err(&pdev->dev, "%s: Couldn't find label property of the IOMMU group, err=%d\n",
				__func__, ret);
		return -EINVAL;
	}

	/* Reading IOMMU partition */
	ret = of_property_read_u32(node, "qcom,iommu-partition",
			&tsc_device->iommu_info.partition_num);
	if (ret) {
		dev_err(&pdev->dev, "%s: Couldn't find iommu-partition property, err=%d\n",
				__func__, ret);
		return -EINVAL;
	}

	/* Reading reset cam gpio */
	tsc_device->reset_cam_gpio = of_get_named_gpio(node,
			"qcom,tsc-reset-cam-gpio", 0);
	if (tsc_device->reset_cam_gpio < 0) {
		dev_err(&pdev->dev, "%s: Couldn't find qcom,tsc-reset-cam-gpio property\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

/* TSC Mux file operations */
static const struct file_operations tsc_mux_fops = {
		.owner   = THIS_MODULE,
		.open    = tsc_mux_open,
		.poll    = tsc_mux_poll,
		.release = tsc_mux_release,
		.unlocked_ioctl   = tsc_mux_ioctl,
};

/* TSC CI file operations */
static const struct file_operations tsc_ci_fops = {
		.owner   = THIS_MODULE,
		.open    = tsc_ci_open,
		.poll    = tsc_ci_poll,
		.release = tsc_ci_release,
		.unlocked_ioctl   = tsc_ci_ioctl,
};


/************************ Device driver probe function ************************/
static int msm_tsc_probe(struct platform_device *pdev)
{
	int ret;

	tsc_device = kzalloc(sizeof(struct tsc_device), GFP_KERNEL);
	if (!tsc_device) {
		pr_err("%s: Unable to allocate memory for struct\n", __func__);
		return -ENOMEM;
	}

	/* get information from device tree */
	if (pdev->dev.of_node) {
		ret = tsc_parse_dt(pdev);
		if (ret != 0) {
			pr_err("%s: devicetree data not available", __func__);
			ret =  -EINVAL;
			goto err_dt;
		}
	} else { /* else - devicetree is not found */
		pr_err("%s: devicetree data is missing", __func__);
		ret =  -EINVAL;
		goto err_dt;
	}

	/* set up references */
	tsc_device->pdev = pdev;
	platform_set_drvdata(pdev, tsc_device);

	/* init iommu client, group and domain */
	if (!tsc_iommu_bypass) {
		ret = tsc_get_iommu_info(pdev);
		if (ret != 0)
			return ret;
	}

	/* Map clocks */
	ret = tsc_clocks_get(pdev);
	if (ret != 0)
		goto err_clocks_get;

	/* map registers memory */
	ret = tsc_map_io_memory(pdev);
	if (ret != 0)
		goto err_map_io;

	/* map irqs */
	ret = tsc_get_irqs(pdev);
	if (ret != 0)
		goto err_map_irqs;

	/* get regulators and bus */
	ret = tsc_get_regulator_bus(pdev);
	if (ret != 0)
		goto err_get_regulator_bus;

	/* get pinctrl */
	ret = tsc_get_pinctrl(pdev);
	if (ret != 0)
		goto err_pinctrl;

	/* creating the tsc device's class */
	tsc_class = class_create(THIS_MODULE, "tsc");
	if (IS_ERR(tsc_class)) {
		ret = PTR_ERR(tsc_class);
		pr_err("%s: Error creating class: %d\n", __func__, ret);
		goto err_class;
	}

	/* Initialize and register mux char device driver */
	ret = tsc_init_char_driver(&tsc_device->mux_chdev.cdev, &tsc_mux_fops,
			&tsc_device->mux_device_number, tsc_device->device_mux,
			"tsc_mux");
	if (ret != 0)
		goto err_chdev_mux;

	/* Initialize and register ci char device drivers */
	ret = tsc_init_char_driver(&tsc_device->ci_chdev.cdev, &tsc_ci_fops,
			&tsc_device->ci_device_number, tsc_device->device_ci,
			"tsc_ci");
	if (ret != 0)
		goto err_chdev_ci;

	/* Init char device counters */
	tsc_device->num_device_open = 0;
	tsc_device->num_mux_opened = 0;
	tsc_device->num_ci_opened = 0;

	/* Init char device mutexes and completion structs */
	mutex_init(&tsc_device->mux_chdev.mutex);
	mutex_init(&tsc_device->ci_chdev.mutex);
	mutex_init(&tsc_device->mutex);
	init_completion(&tsc_device->ci_chdev.transaction_complete);
	init_completion(&tsc_device->ci_chdev.transaction_finish);

	/* Init debugfs support */
	tsc_debugfs_init();

	return ret;

err_chdev_ci:
	device_destroy(tsc_class, tsc_device->mux_chdev.cdev.dev);
	cdev_del(&tsc_device->mux_chdev.cdev);
err_chdev_mux:
	class_destroy(tsc_class);
err_class:
err_pinctrl:
	if (tsc_device->bus_client)
		msm_bus_scale_unregister_client(tsc_device->bus_client);

	devm_regulator_put(tsc_device->gdsc);
err_get_regulator_bus:
err_map_irqs:
	iounmap(tsc_device->base);
err_map_io:
	tsc_clocks_put();
err_clocks_get:
	tsc_free_iommu_info();
err_dt:
	kfree(tsc_device);

	return ret;
}

/*********************** Device driver remove function ***********************/
static int msm_tsc_remove(struct platform_device *pdev)
{
	/* Removing debugfs support */
	tsc_debugfs_exit();

	/* Destroying the char device mutexes */
	mutex_destroy(&tsc_device->mux_chdev.mutex);
	mutex_destroy(&tsc_device->ci_chdev.mutex);

	/* unregistering and deleting the tsc-ci char device driver*/
	device_destroy(tsc_class, tsc_device->ci_chdev.cdev.dev);
	cdev_del(&tsc_device->ci_chdev.cdev);

	/* unregistering and deleting the tsc-mux char device driver*/
	device_destroy(tsc_class, tsc_device->mux_chdev.cdev.dev);
	cdev_del(&tsc_device->mux_chdev.cdev);

	/* Unregistering the char devices */
	unregister_chrdev_region(tsc_device->ci_device_number, 1);
	unregister_chrdev_region(tsc_device->mux_device_number, 1);

	/* Removing the tsc class*/
	class_destroy(tsc_class);

	/* Unregister the bus client and the regulator */
	if (tsc_device->bus_client)
		msm_bus_scale_unregister_client(tsc_device->bus_client);

	devm_regulator_put(tsc_device->gdsc);

	/* Unmapping the io memory */
	iounmap(tsc_device->base);

	/* Releasing the clocks */
	tsc_clocks_put();

	/* Releasing the iommu info */
	if (!tsc_iommu_bypass)
		tsc_free_iommu_info();

	/* Releasing the memory allocated for the TSC device struct */
	kfree(tsc_device);

	return 0;
}

/*********************** Platform driver information ***********************/
static struct of_device_id msm_match_table[] = {
		{.compatible = "qcom,msm-tsc"},
		{}
};

static struct platform_driver msm_tsc_driver = {
		.probe          = msm_tsc_probe,
		.remove         = msm_tsc_remove,
		.driver         = {
		.name   = "msm_tsc",
		.of_match_table = msm_match_table,
	},
};

/**
 * tsc_init() - TSC driver module init function.
 *
 * Return 0 on success, error value otherwise.
 */
static int __init tsc_init(void)
{
	int ret = 0;

	/* register the driver, and check hardware */
	ret = platform_driver_register(&msm_tsc_driver);
	if (ret) {
		pr_err("%s: platform_driver_register failed: %d\n", __func__,
				ret);
		return ret;
	}

	return ret;
}

/**
 * tsc_exit() - TSC driver module exit function.
 */
static void __exit tsc_exit(void)
{
	platform_driver_unregister(&msm_tsc_driver);
}

module_init(tsc_init);
module_exit(tsc_exit);

MODULE_DESCRIPTION("TSC platform device and two char devs: mux and ci");
MODULE_LICENSE("GPL v2");
