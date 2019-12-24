// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/ipc_logging.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/qcom-geni-se.h>
#include <linux/spinlock.h>
#include <linux/pinctrl/consumer.h>

#define GENI_SE_IOMMU_VA_START	(0x40000000)
#define GENI_SE_IOMMU_VA_SIZE	(0xC0000000)

#ifdef CONFIG_ARM64
#define GENI_SE_DMA_PTR_L(ptr) ((u32)ptr)
#define GENI_SE_DMA_PTR_H(ptr) ((u32)(ptr >> 32))
#else
#define GENI_SE_DMA_PTR_L(ptr) ((u32)ptr)
#define GENI_SE_DMA_PTR_H(ptr) 0
#endif

/* Convert BCM threshold to actual frequency x 4 */
#define CONV_TO_BW(x) (x*20000*4)
#define NUM_LOG_PAGES 2
#define MAX_CLK_PERF_LEVEL 32
static unsigned long default_bus_bw_set[] = {0, 19200000, 50000000,
				100000000, 150000000, 200000000, 236000000};

struct bus_vectors {
	int src;
	int dst;
};

/**
 * @struct geni_se_device - Data structure to represent the QUPv3 Core
 * @dev:		Device pointer of the QUPv3 core.
 * @cb_dev:		Device pointer of the context bank in the IOMMU.
 * @iommu_lock:		Lock to protect IOMMU Mapping & attachment.
 * @iommu_map:		IOMMU map of the memory space supported by this core.
 * @iommu_s1_bypass:	Bypass IOMMU stage 1 translation.
 * @base:		Base address of this instance of QUPv3 core.
 * @bus_bw:		Client handle to the bus bandwidth request.
 * @bus_bw_noc:		Client handle to the QUP clock and DDR path bus
			bandwidth request.
 * @bus_mas_id:		Master Endpoint ID for bus BW request.
 * @bus_slv_id:		Slave Endpoint ID for bus BW request.
 * @geni_dev_lock:		Lock to protect the bus ab & ib values, list.
 * @ab_list_head:	Sorted resource list based on average bus BW.
 * @ib_list_head:	Sorted resource list based on instantaneous bus BW.
 * @ab_list_head_noc:	Sorted resource list based on average DDR path bus BW.
 * @ib_list_head_noc:	Sorted resource list based on instantaneous DDR path
			bus BW.
 * @cur_ab:		Current Bus Average BW request value.
 * @cur_ib:		Current Bus Instantaneous BW request value.
 * @cur_ab_noc:		Current DDR Bus Average BW request value.
 * @cur_ib_noc:		Current DDR Bus Instantaneous BW request value.
 * @bus_bw_set:		Clock plan for the bus driver.
 * @bus_bw_set_noc:	Clock plan for DDR path.
 * @cur_bus_bw_idx:	Current index within the bus clock plan.
 * @cur_bus_bw_idx_noc:	Current index within the DDR path clock plan.
 * @num_clk_levels:	Number of valid clock levels in clk_perf_tbl.
 * @clk_perf_tbl:	Table of clock frequency input to Serial Engine clock.
 * @log_ctx:		Logging context to hold the debug information.
 * @vectors:		Structure to store Master End and Slave End IDs for
			QUPv3 clock and DDR path bus BW request.
 * @num_paths:		Two paths. QUPv3 clock and DDR paths.
 * @num_usecases:	One usecase to vote for both QUPv3 clock and DDR paths.
 * @pdata:		To register our client handle with the ICB driver.
 * @update:		Usecase index for icb voting.
 * @vote_for_bw:	To check if we have to vote for BW or BCM threashold
			in ab/ib ICB voting.
 */
struct geni_se_device {
	struct device *dev;
	struct device *cb_dev;
	struct mutex iommu_lock;
	struct dma_iommu_mapping *iommu_map;
	bool iommu_s1_bypass;
	void __iomem *base;
	struct msm_bus_client_handle *bus_bw;
	uint32_t bus_bw_noc;
	u32 bus_mas_id;
	u32 bus_slv_id;
	struct mutex geni_dev_lock;
	struct list_head ab_list_head;
	struct list_head ib_list_head;
	struct list_head ab_list_head_noc;
	struct list_head ib_list_head_noc;
	unsigned long cur_ab;
	unsigned long cur_ib;
	unsigned long cur_ab_noc;
	unsigned long cur_ib_noc;
	int bus_bw_set_size;
	int bus_bw_set_size_noc;
	unsigned long *bus_bw_set;
	unsigned long *bus_bw_set_noc;
	int cur_bus_bw_idx;
	int cur_bus_bw_idx_noc;
	unsigned int num_clk_levels;
	unsigned long *clk_perf_tbl;
	void *log_ctx;
	struct bus_vectors *vectors;
	int num_paths;
	int num_usecases;
	struct msm_bus_scale_pdata *pdata;
	int update;
	bool vote_for_bw;
};

#define HW_VER_MAJOR_MASK GENMASK(31, 28)
#define HW_VER_MAJOR_SHFT 28
#define HW_VER_MINOR_MASK GENMASK(27, 16)
#define HW_VER_MINOR_SHFT 16
#define HW_VER_STEP_MASK GENMASK(15, 0)

static int geni_se_iommu_map_and_attach(struct geni_se_device *geni_se_dev);

/**
 * geni_read_reg_nolog() - Helper function to read from a GENI register
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 *
 * Return:	Return the contents of the register.
 */
unsigned int geni_read_reg_nolog(void __iomem *base, int offset)
{
	return readl_relaxed_no_log(base + offset);
}
EXPORT_SYMBOL(geni_read_reg_nolog);

/**
 * geni_write_reg_nolog() - Helper function to write into a GENI register
 * @value:	Value to be written into the register.
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 */
void geni_write_reg_nolog(unsigned int value, void __iomem *base, int offset)
{
	return writel_relaxed_no_log(value, (base + offset));
}
EXPORT_SYMBOL(geni_write_reg_nolog);

/**
 * geni_read_reg() - Helper function to read from a GENI register
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 *
 * Return:	Return the contents of the register.
 */
unsigned int geni_read_reg(void __iomem *base, int offset)
{
	return readl_relaxed(base + offset);
}
EXPORT_SYMBOL(geni_read_reg);

/**
 * geni_write_reg() - Helper function to write into a GENI register
 * @value:	Value to be written into the register.
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 */
void geni_write_reg(unsigned int value, void __iomem *base, int offset)
{
	return writel_relaxed(value, (base + offset));
}
EXPORT_SYMBOL(geni_write_reg);

/**
 * get_se_proto() - Read the protocol configured for a serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Protocol value as configured in the serial engine.
 */
int get_se_proto(void __iomem *base)
{
	int proto;

	proto = ((geni_read_reg(base, GENI_FW_REVISION_RO)
			& FW_REV_PROTOCOL_MSK) >> FW_REV_PROTOCOL_SHFT);
	return proto;
}
EXPORT_SYMBOL(get_se_proto);

/**
 * get_se_m_fw() - Read the Firmware ver for the Main sequencer engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Firmware version for the Main sequencer engine
 */
int get_se_m_fw(void __iomem *base)
{
	int fw_ver_m;

	fw_ver_m = ((geni_read_reg(base, GENI_FW_REVISION_RO)
			& FW_REV_VERSION_MSK));
	return fw_ver_m;
}
EXPORT_SYMBOL(get_se_m_fw);

/**
 * get_se_s_fw() - Read the Firmware ver for the Secondry sequencer engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Firmware version for the Secondry sequencer engine
 */
int get_se_s_fw(void __iomem *base)
{
	int fw_ver_s;

	fw_ver_s = ((geni_read_reg(base, GENI_FW_S_REVISION_RO)
			& FW_REV_VERSION_MSK));
	return fw_ver_s;
}
EXPORT_SYMBOL(get_se_s_fw);

static int se_geni_irq_en(void __iomem *base)
{
	unsigned int common_geni_m_irq_en;
	unsigned int common_geni_s_irq_en;

	common_geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	common_geni_s_irq_en = geni_read_reg(base, SE_GENI_S_IRQ_EN);
	/* Common to all modes */
	common_geni_m_irq_en |= M_COMMON_GENI_M_IRQ_EN;
	common_geni_s_irq_en |= S_COMMON_GENI_S_IRQ_EN;

	geni_write_reg(common_geni_m_irq_en, base, SE_GENI_M_IRQ_EN);
	geni_write_reg(common_geni_s_irq_en, base, SE_GENI_S_IRQ_EN);
	return 0;
}


static void se_set_rx_rfr_wm(void __iomem *base, unsigned int rx_wm,
						unsigned int rx_rfr)
{
	geni_write_reg(rx_wm, base, SE_GENI_RX_WATERMARK_REG);
	geni_write_reg(rx_rfr, base, SE_GENI_RX_RFR_WATERMARK_REG);
}

static int se_io_set_mode(void __iomem *base)
{
	unsigned int io_mode;
	unsigned int geni_dma_mode;

	io_mode = geni_read_reg(base, SE_IRQ_EN);
	geni_dma_mode = geni_read_reg(base, SE_GENI_DMA_MODE_EN);

	io_mode |= (GENI_M_IRQ_EN | GENI_S_IRQ_EN);
	io_mode |= (DMA_TX_IRQ_EN | DMA_RX_IRQ_EN);
	geni_dma_mode &= ~GENI_DMA_MODE_EN;

	geni_write_reg(io_mode, base, SE_IRQ_EN);
	geni_write_reg(geni_dma_mode, base, SE_GENI_DMA_MODE_EN);
	geni_write_reg(0, base, SE_GSI_EVENT_EN);
	return 0;
}

static void se_io_init(void __iomem *base)
{
	unsigned int io_op_ctrl;
	unsigned int geni_cgc_ctrl;
	unsigned int dma_general_cfg;

	geni_cgc_ctrl = geni_read_reg(base, GENI_CGC_CTRL);
	dma_general_cfg = geni_read_reg(base, SE_DMA_GENERAL_CFG);
	geni_cgc_ctrl |= DEFAULT_CGC_EN;
	dma_general_cfg |= (AHB_SEC_SLV_CLK_CGC_ON | DMA_AHB_SLV_CFG_ON |
			DMA_TX_CLK_CGC_ON | DMA_RX_CLK_CGC_ON);
	io_op_ctrl = DEFAULT_IO_OUTPUT_CTRL_MSK;
	geni_write_reg(geni_cgc_ctrl, base, GENI_CGC_CTRL);
	geni_write_reg(dma_general_cfg, base, SE_DMA_GENERAL_CFG);

	geni_write_reg(io_op_ctrl, base, GENI_OUTPUT_CTRL);
	geni_write_reg(FORCE_DEFAULT, base, GENI_FORCE_DEFAULT_REG);
}

/**
 * geni_se_init() - Initialize the GENI Serial Engine
 * @base:	Base address of the serial engine's register block.
 * @rx_wm:	Receive watermark to be configured.
 * @rx_rfr_wm:	Ready-for-receive watermark to be configured.
 *
 * This function is used to initialize the GENI serial engine, configure
 * receive watermark and ready-for-receive watermarks.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_init(void __iomem *base, unsigned int rx_wm, unsigned int rx_rfr)
{
	int ret;

	se_io_init(base);
	ret = se_io_set_mode(base);
	if (ret)
		return ret;

	se_set_rx_rfr_wm(base, rx_wm, rx_rfr);
	ret = se_geni_irq_en(base);
	return ret;
}
EXPORT_SYMBOL(geni_se_init);

static int geni_se_select_fifo_mode(void __iomem *base)
{
	int proto = get_se_proto(base);
	unsigned int common_geni_m_irq_en;
	unsigned int common_geni_s_irq_en;
	unsigned int geni_dma_mode;

	geni_write_reg(0, base, SE_GSI_EVENT_EN);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_M_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_S_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_TX_IRQ_CLR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_RX_IRQ_CLR);
	geni_write_reg(0xFFFFFFFF, base, SE_IRQ_EN);

	common_geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	common_geni_s_irq_en = geni_read_reg(base, SE_GENI_S_IRQ_EN);
	geni_dma_mode = geni_read_reg(base, SE_GENI_DMA_MODE_EN);
	if (proto != UART) {
		common_geni_m_irq_en |=
			(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN |
			M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
		common_geni_s_irq_en |= S_CMD_DONE_EN;
	}

	if (proto == I3C)
		common_geni_m_irq_en |=  (M_GP_SYNC_IRQ_0_EN | M_SEC_IRQ_EN);

	geni_dma_mode &= ~GENI_DMA_MODE_EN;

	geni_write_reg(common_geni_m_irq_en, base, SE_GENI_M_IRQ_EN);
	geni_write_reg(common_geni_s_irq_en, base, SE_GENI_S_IRQ_EN);
	geni_write_reg(geni_dma_mode, base, SE_GENI_DMA_MODE_EN);

	return 0;
}

static int geni_se_select_dma_mode(void __iomem *base)
{
	int proto = get_se_proto(base);
	unsigned int geni_dma_mode = 0;
	unsigned int common_geni_m_irq_en;

	geni_write_reg(0, base, SE_GSI_EVENT_EN);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_M_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_S_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_TX_IRQ_CLR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_RX_IRQ_CLR);
	geni_write_reg(0xFFFFFFFF, base, SE_IRQ_EN);

	common_geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	if (proto != UART)
		common_geni_m_irq_en &=
			~(M_TX_FIFO_WATERMARK_EN | M_RX_FIFO_WATERMARK_EN);

	geni_write_reg(common_geni_m_irq_en, base, SE_GENI_M_IRQ_EN);
	geni_dma_mode = geni_read_reg(base, SE_GENI_DMA_MODE_EN);
	geni_dma_mode |= GENI_DMA_MODE_EN;
	geni_write_reg(geni_dma_mode, base, SE_GENI_DMA_MODE_EN);
	return 0;
}

static int geni_se_select_gsi_mode(void __iomem *base)
{
	unsigned int geni_dma_mode = 0;
	unsigned int gsi_event_en = 0;
	unsigned int common_geni_m_irq_en = 0;
	unsigned int common_geni_s_irq_en = 0;

	common_geni_m_irq_en = geni_read_reg(base, SE_GENI_M_IRQ_EN);
	common_geni_s_irq_en = geni_read_reg(base, SE_GENI_S_IRQ_EN);
	common_geni_m_irq_en &=
			~(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN |
			M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
	common_geni_s_irq_en &= ~S_CMD_DONE_EN;
	geni_dma_mode = geni_read_reg(base, SE_GENI_DMA_MODE_EN);
	gsi_event_en = geni_read_reg(base, SE_GSI_EVENT_EN);

	geni_dma_mode |= GENI_DMA_MODE_EN;
	gsi_event_en |= (DMA_RX_EVENT_EN | DMA_TX_EVENT_EN |
				GENI_M_EVENT_EN | GENI_S_EVENT_EN);

	geni_write_reg(0, base, SE_IRQ_EN);
	geni_write_reg(common_geni_s_irq_en, base, SE_GENI_S_IRQ_EN);
	geni_write_reg(common_geni_m_irq_en, base, SE_GENI_M_IRQ_EN);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_M_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_GENI_S_IRQ_CLEAR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_TX_IRQ_CLR);
	geni_write_reg(0xFFFFFFFF, base, SE_DMA_RX_IRQ_CLR);
	geni_write_reg(geni_dma_mode, base, SE_GENI_DMA_MODE_EN);
	geni_write_reg(gsi_event_en, base, SE_GSI_EVENT_EN);
	return 0;

}

/**
 * geni_se_select_mode() - Select the serial engine transfer mode
 * @base:	Base address of the serial engine's register block.
 * @mode:	Transfer mode to be selected.
 *
 * Return:	0 on success, standard Linux error codes on failure.
 */
int geni_se_select_mode(void __iomem *base, int mode)
{
	int ret = 0;

	switch (mode) {
	case FIFO_MODE:
		geni_se_select_fifo_mode(base);
		break;
	case SE_DMA:
		geni_se_select_dma_mode(base);
		break;
	case GSI_DMA:
		geni_se_select_gsi_mode(base);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(geni_se_select_mode);

/**
 * geni_setup_m_cmd() - Setup the primary sequencer
 * @base:	Base address of the serial engine's register block.
 * @cmd:	Command/Operation to setup in the primary sequencer.
 * @params:	Parameter for the sequencer command.
 *
 * This function is used to configure the primary sequencer with the
 * command and its assoicated parameters.
 */
void geni_setup_m_cmd(void __iomem *base, u32 cmd, u32 params)
{
	u32 m_cmd = (cmd << M_OPCODE_SHFT);

	m_cmd |= (params & M_PARAMS_MSK);
	geni_write_reg(m_cmd, base, SE_GENI_M_CMD0);
}
EXPORT_SYMBOL(geni_setup_m_cmd);

/**
 * geni_setup_s_cmd() - Setup the secondary sequencer
 * @base:	Base address of the serial engine's register block.
 * @cmd:	Command/Operation to setup in the secondary sequencer.
 * @params:	Parameter for the sequencer command.
 *
 * This function is used to configure the secondary sequencer with the
 * command and its assoicated parameters.
 */
void geni_setup_s_cmd(void __iomem *base, u32 cmd, u32 params)
{
	u32 s_cmd = geni_read_reg(base, SE_GENI_S_CMD0);

	s_cmd &= ~(S_OPCODE_MSK | S_PARAMS_MSK);
	s_cmd |= (cmd << S_OPCODE_SHFT);
	s_cmd |= (params & S_PARAMS_MSK);
	geni_write_reg(s_cmd, base, SE_GENI_S_CMD0);
}
EXPORT_SYMBOL(geni_setup_s_cmd);

/**
 * geni_cancel_m_cmd() - Cancel the command configured in the primary sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to cancel the currently configured command in the
 * primary sequencer.
 */
void geni_cancel_m_cmd(void __iomem *base)
{
	geni_write_reg(M_GENI_CMD_CANCEL, base, SE_GENI_M_CMD_CTRL_REG);
}
EXPORT_SYMBOL(geni_cancel_m_cmd);

/**
 * geni_cancel_s_cmd() - Cancel the command configured in the secondary
 *                       sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to cancel the currently configured command in the
 * secondary sequencer.
 */
void geni_cancel_s_cmd(void __iomem *base)
{
	geni_write_reg(S_GENI_CMD_CANCEL, base, SE_GENI_S_CMD_CTRL_REG);
}
EXPORT_SYMBOL(geni_cancel_s_cmd);

/**
 * geni_abort_m_cmd() - Abort the command configured in the primary sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to force abort the currently configured command in the
 * primary sequencer.
 */
void geni_abort_m_cmd(void __iomem *base)
{
	geni_write_reg(M_GENI_CMD_ABORT, base, SE_GENI_M_CMD_CTRL_REG);
}
EXPORT_SYMBOL(geni_abort_m_cmd);

/**
 * geni_abort_s_cmd() - Abort the command configured in the secondary
 *                       sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to force abort the currently configured command in the
 * secondary sequencer.
 */
void geni_abort_s_cmd(void __iomem *base)
{
	geni_write_reg(S_GENI_CMD_ABORT, base, SE_GENI_S_CMD_CTRL_REG);
}
EXPORT_SYMBOL(geni_abort_s_cmd);

/**
 * get_tx_fifo_depth() - Get the TX fifo depth of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the depth i.e. number of elements in the
 * TX fifo of the serial engine.
 *
 * Return:	TX fifo depth in units of FIFO words.
 */
int get_tx_fifo_depth(void __iomem *base)
{
	int tx_fifo_depth;

	tx_fifo_depth = ((geni_read_reg(base, SE_HW_PARAM_0)
			& TX_FIFO_DEPTH_MSK) >> TX_FIFO_DEPTH_SHFT);
	return tx_fifo_depth;
}
EXPORT_SYMBOL(get_tx_fifo_depth);

/**
 * get_tx_fifo_width() - Get the TX fifo width of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the width i.e. word size per element in the
 * TX fifo of the serial engine.
 *
 * Return:	TX fifo width in bits
 */
int get_tx_fifo_width(void __iomem *base)
{
	int tx_fifo_width;

	tx_fifo_width = ((geni_read_reg(base, SE_HW_PARAM_0)
			& TX_FIFO_WIDTH_MSK) >> TX_FIFO_WIDTH_SHFT);
	return tx_fifo_width;
}
EXPORT_SYMBOL(get_tx_fifo_width);

/**
 * get_rx_fifo_depth() - Get the RX fifo depth of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the depth i.e. number of elements in the
 * RX fifo of the serial engine.
 *
 * Return:	RX fifo depth in units of FIFO words
 */
int get_rx_fifo_depth(void __iomem *base)
{
	int rx_fifo_depth;

	rx_fifo_depth = ((geni_read_reg(base, SE_HW_PARAM_1)
			& RX_FIFO_DEPTH_MSK) >> RX_FIFO_DEPTH_SHFT);
	return rx_fifo_depth;
}
EXPORT_SYMBOL(get_rx_fifo_depth);

/**
 * se_get_packing_config() - Get the packing configuration based on input
 * @bpw:	Bits of data per transfer word.
 * @pack_words:	Number of words per fifo element.
 * @msb_to_lsb:	Transfer from MSB to LSB or vice-versa.
 * @cfg0:	Output buffer to hold the first half of configuration.
 * @cfg1:	Output buffer to hold the second half of configuration.
 *
 * This function is used to calculate the packing configuration based on
 * the input packing requirement and the configuration logic.
 */
void se_get_packing_config(int bpw, int pack_words, bool msb_to_lsb,
			   unsigned long *cfg0, unsigned long *cfg1)
{
	u32 cfg[4] = {0};
	int len;
	int temp_bpw = bpw;
	int idx_start = (msb_to_lsb ? (bpw - 1) : 0);
	int idx = idx_start;
	int idx_delta = (msb_to_lsb ? -BITS_PER_BYTE : BITS_PER_BYTE);
	int ceil_bpw = ((bpw & (BITS_PER_BYTE - 1)) ?
			((bpw & ~(BITS_PER_BYTE - 1)) + BITS_PER_BYTE) : bpw);
	int iter = (ceil_bpw * pack_words) >> 3;
	int i;

	if (unlikely(iter <= 0 || iter > 4)) {
		*cfg0 = 0;
		*cfg1 = 0;
		return;
	}

	for (i = 0; i < iter; i++) {
		len = (temp_bpw < BITS_PER_BYTE) ?
				(temp_bpw - 1) : BITS_PER_BYTE - 1;
		cfg[i] = ((idx << 5) | (msb_to_lsb << 4) | (len << 1));
		idx = ((temp_bpw - BITS_PER_BYTE) <= 0) ?
				((i + 1) * BITS_PER_BYTE) + idx_start :
				idx + idx_delta;
		temp_bpw = ((temp_bpw - BITS_PER_BYTE) <= 0) ?
				bpw : (temp_bpw - BITS_PER_BYTE);
	}
	cfg[iter - 1] |= 1;
	*cfg0 = cfg[0] | (cfg[1] << 10);
	*cfg1 = cfg[2] | (cfg[3] << 10);
}
EXPORT_SYMBOL(se_get_packing_config);

/**
 * se_config_packing() - Packing configuration of the serial engine
 * @base:	Base address of the serial engine's register block.
 * @bpw:	Bits of data per transfer word.
 * @pack_words:	Number of words per fifo element.
 * @msb_to_lsb:	Transfer from MSB to LSB or vice-versa.
 *
 * This function is used to configure the packing rules for the current
 * transfer.
 */
void se_config_packing(void __iomem *base, int bpw,
			int pack_words, bool msb_to_lsb)
{
	unsigned long cfg0, cfg1;

	se_get_packing_config(bpw, pack_words, msb_to_lsb, &cfg0, &cfg1);
	geni_write_reg(cfg0, base, SE_GENI_TX_PACKING_CFG0);
	geni_write_reg(cfg1, base, SE_GENI_TX_PACKING_CFG1);
	geni_write_reg(cfg0, base, SE_GENI_RX_PACKING_CFG0);
	geni_write_reg(cfg1, base, SE_GENI_RX_PACKING_CFG1);
	if (pack_words || bpw == 32)
		geni_write_reg((bpw >> 4), base, SE_GENI_BYTE_GRAN);
}
EXPORT_SYMBOL(se_config_packing);

static bool geni_se_check_bus_bw(struct geni_se_device *geni_se_dev)
{
	int i;
	int new_bus_bw_idx = geni_se_dev->bus_bw_set_size - 1;
	unsigned long new_bus_bw;
	bool bus_bw_update = false;
	/* Convert agg ab into bytes per second */
	unsigned long new_ab_in_hz = DEFAULT_BUS_WIDTH *
					((2*geni_se_dev->cur_ab)*10000);

	new_bus_bw = max(geni_se_dev->cur_ib, new_ab_in_hz) /
							DEFAULT_BUS_WIDTH;
	for (i = 0; i < geni_se_dev->bus_bw_set_size; i++) {
		if (geni_se_dev->bus_bw_set[i] >= new_bus_bw) {
			new_bus_bw_idx = i;
			break;
		}
	}

	if (geni_se_dev->cur_bus_bw_idx != new_bus_bw_idx) {
		geni_se_dev->cur_bus_bw_idx = new_bus_bw_idx;
		bus_bw_update = true;
	}
	return bus_bw_update;
}

static bool geni_se_check_bus_bw_noc(struct geni_se_device *geni_se_dev)
{
	int i;
	int new_bus_bw_idx = geni_se_dev->bus_bw_set_size_noc - 1;
	unsigned long new_bus_bw;
	bool bus_bw_update = false;

	new_bus_bw = max(geni_se_dev->cur_ib_noc, geni_se_dev->cur_ab_noc) /
							DEFAULT_BUS_WIDTH;

	for (i = 0; i < geni_se_dev->bus_bw_set_size_noc; i++) {
		if (geni_se_dev->bus_bw_set_noc[i] >= new_bus_bw) {
			new_bus_bw_idx = i;
			break;
		}
	}

	if (geni_se_dev->cur_bus_bw_idx_noc != new_bus_bw_idx) {
		geni_se_dev->cur_bus_bw_idx_noc = new_bus_bw_idx;
		bus_bw_update = true;
	}

	return bus_bw_update;
}

static int geni_se_rmv_ab_ib(struct geni_se_device *geni_se_dev,
			     struct se_geni_rsc *rsc)
{
	struct se_geni_rsc *tmp;
	bool bus_bw_update = false;
	bool bus_bw_update_noc = false;
	int ret = 0;
	int new_update;

	if (unlikely(list_empty(&rsc->ab_list) || list_empty(&rsc->ib_list)))
		return -EINVAL;

	mutex_lock(&geni_se_dev->geni_dev_lock);

	new_update = geni_se_dev->update ? 0 : 1;
	list_del_init(&rsc->ab_list);
	geni_se_dev->cur_ab -= rsc->ab;

	list_del_init(&rsc->ib_list);
	tmp = list_first_entry_or_null(&geni_se_dev->ib_list_head,
					   struct se_geni_rsc, ib_list);
	if (tmp && tmp->ib != geni_se_dev->cur_ib)
		geni_se_dev->cur_ib = tmp->ib;
	else if (!tmp && geni_se_dev->cur_ib)
		geni_se_dev->cur_ib = 0;

	bus_bw_update = geni_se_check_bus_bw(geni_se_dev);

	if (geni_se_dev->num_paths == 2) {
		geni_se_dev->pdata->usecase[new_update].vectors[0].ab  =
			geni_se_dev->vote_for_bw ?
			CONV_TO_BW(geni_se_dev->cur_ab) : geni_se_dev->cur_ab;
		geni_se_dev->pdata->usecase[new_update].vectors[0].ib  =
			geni_se_dev->vote_for_bw ?
			CONV_TO_BW(geni_se_dev->cur_ib) : geni_se_dev->cur_ib;
	}

	if (bus_bw_update && geni_se_dev->num_paths != 2)
		ret = msm_bus_scale_update_bw(geni_se_dev->bus_bw,
						geni_se_dev->cur_ab,
						geni_se_dev->cur_ib);
	GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
		"%s: %s: cur_ab_ib(%lu:%lu) req_ab_ib(%lu:%lu) %d\n",
		__func__, dev_name(rsc->ctrl_dev), geni_se_dev->cur_ab,
		geni_se_dev->cur_ib, rsc->ab, rsc->ib, bus_bw_update);


	if (geni_se_dev->num_paths == 2) {
		if (unlikely(list_empty(&rsc->ab_list_noc) ||
					list_empty(&rsc->ib_list_noc)))
			return -EINVAL;

		list_del_init(&rsc->ab_list_noc);
		geni_se_dev->cur_ab_noc -= rsc->ab_noc;

		list_del_init(&rsc->ib_list_noc);
		tmp = list_first_entry_or_null(&geni_se_dev->ib_list_head_noc,
					struct se_geni_rsc, ib_list_noc);
		if (tmp && tmp->ib_noc != geni_se_dev->cur_ib_noc)
			geni_se_dev->cur_ib_noc = tmp->ib_noc;
		else if (!tmp && geni_se_dev->cur_ib_noc)
			geni_se_dev->cur_ib_noc = 0;

		bus_bw_update_noc = geni_se_check_bus_bw_noc(geni_se_dev);

		geni_se_dev->pdata->usecase[new_update].vectors[1].ab  =
				geni_se_dev->cur_ab_noc;
		geni_se_dev->pdata->usecase[new_update].vectors[1].ib  =
				geni_se_dev->cur_ib_noc;

		if (bus_bw_update_noc || bus_bw_update) {
			geni_se_dev->update = new_update;
			ret = msm_bus_scale_client_update_request
					(geni_se_dev->bus_bw_noc, new_update);
		}
		GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
			"%s: %s: cur_ab_ib_noc(%lu:%lu) req_ab_ib_noc(%lu:%lu) %d\n",
			__func__, dev_name(rsc->ctrl_dev),
			geni_se_dev->cur_ab_noc, geni_se_dev->cur_ib_noc,
			rsc->ab_noc, rsc->ib_noc, bus_bw_update_noc);
	}
	mutex_unlock(&geni_se_dev->geni_dev_lock);
	return ret;
}

/**
 * se_geni_clks_off() - Turn off clocks associated with the serial
 *                      engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_clks_off(struct se_geni_rsc *rsc)
{
	int ret = 0;
	struct geni_se_device *geni_se_dev;

	if (unlikely(!rsc || !rsc->wrapper_dev))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -ENODEV;

	clk_disable_unprepare(rsc->se_clk);
	clk_disable_unprepare(rsc->s_ahb_clk);
	clk_disable_unprepare(rsc->m_ahb_clk);

	ret = geni_se_rmv_ab_ib(geni_se_dev, rsc);
	if (ret)
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d during bus_bw_update\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(se_geni_clks_off);

/**
 * se_geni_resources_off() - Turn off resources associated with the serial
 *                           engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_resources_off(struct se_geni_rsc *rsc)
{
	int ret = 0;
	struct geni_se_device *geni_se_dev;

	if (unlikely(!rsc || !rsc->wrapper_dev))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -ENODEV;

	ret = se_geni_clks_off(rsc);
	if (ret)
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d turning off clocks\n", __func__, ret);
	ret = pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_sleep);
	if (ret)
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d pinctrl_select_state\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(se_geni_resources_off);

static int geni_se_add_ab_ib(struct geni_se_device *geni_se_dev,
			     struct se_geni_rsc *rsc)
{
	struct se_geni_rsc *tmp = NULL;
	struct list_head *ins_list_head;
	struct list_head *ins_list_head_noc;
	bool bus_bw_update = false;
	bool bus_bw_update_noc = false;
	int ret = 0;
	int new_update;

	mutex_lock(&geni_se_dev->geni_dev_lock);

	new_update = geni_se_dev->update ? 0 : 1;
	list_add(&rsc->ab_list, &geni_se_dev->ab_list_head);
	geni_se_dev->cur_ab += rsc->ab;

	ins_list_head = &geni_se_dev->ib_list_head;
	list_for_each_entry(tmp, &geni_se_dev->ib_list_head, ib_list) {
		if (tmp->ib < rsc->ib)
			break;
		ins_list_head = &tmp->ib_list;
	}
	list_add(&rsc->ib_list, ins_list_head);
	/* Currently inserted node has greater average BW value */
	if (ins_list_head == &geni_se_dev->ib_list_head)
		geni_se_dev->cur_ib = rsc->ib;

	bus_bw_update = geni_se_check_bus_bw(geni_se_dev);

	if (geni_se_dev->num_paths == 2) {
		geni_se_dev->pdata->usecase[new_update].vectors[0].ab  =
			geni_se_dev->vote_for_bw ?
			CONV_TO_BW(geni_se_dev->cur_ab) : geni_se_dev->cur_ab;
		geni_se_dev->pdata->usecase[new_update].vectors[0].ib  =
			geni_se_dev->vote_for_bw ?
			CONV_TO_BW(geni_se_dev->cur_ib) : geni_se_dev->cur_ib;
	}

	if (bus_bw_update && geni_se_dev->num_paths != 2)
		ret = msm_bus_scale_update_bw(geni_se_dev->bus_bw,
						geni_se_dev->cur_ab,
						geni_se_dev->cur_ib);
	GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
		"%s: %s: cur_ab_ib(%lu:%lu) req_ab_ib(%lu:%lu) %d\n",
		__func__, dev_name(rsc->ctrl_dev),
		geni_se_dev->cur_ab, geni_se_dev->cur_ib,
		rsc->ab, rsc->ib, bus_bw_update);


	if (geni_se_dev->num_paths == 2) {

		list_add(&rsc->ab_list_noc, &geni_se_dev->ab_list_head_noc);
		geni_se_dev->cur_ab_noc += rsc->ab_noc;
		ins_list_head_noc = &geni_se_dev->ib_list_head_noc;

		list_for_each_entry(tmp, &geni_se_dev->ib_list_head_noc,
					ib_list_noc) {
			if (tmp->ib < rsc->ib)
				break;
			ins_list_head_noc = &tmp->ib_list_noc;
		}
		list_add(&rsc->ib_list_noc, ins_list_head_noc);

		if (ins_list_head_noc == &geni_se_dev->ib_list_head_noc)
			geni_se_dev->cur_ib_noc = rsc->ib_noc;

		bus_bw_update_noc = geni_se_check_bus_bw_noc(geni_se_dev);

		geni_se_dev->pdata->usecase[new_update].vectors[1].ab  =
				geni_se_dev->cur_ab_noc;
		geni_se_dev->pdata->usecase[new_update].vectors[1].ib  =
				geni_se_dev->cur_ib_noc;
		if (bus_bw_update_noc || bus_bw_update) {
			geni_se_dev->update = new_update;
			ret = msm_bus_scale_client_update_request
					(geni_se_dev->bus_bw_noc, new_update);
		}
		GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
			"%s: %s: cur_ab_ib_noc(%lu:%lu) req_ab_ib_noc(%lu:%lu) %d\n",
			__func__, dev_name(rsc->ctrl_dev),
			geni_se_dev->cur_ab_noc, geni_se_dev->cur_ib_noc,
			rsc->ab_noc, rsc->ib_noc, bus_bw_update_noc);
	}
	mutex_unlock(&geni_se_dev->geni_dev_lock);
	return ret;
}

/**
 * se_geni_clks_on() - Turn on clocks associated with the serial
 *                     engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_clks_on(struct se_geni_rsc *rsc)
{
	int ret = 0;
	struct geni_se_device *geni_se_dev;

	if (unlikely(!rsc || !rsc->wrapper_dev))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -EPROBE_DEFER;

	ret = geni_se_add_ab_ib(geni_se_dev, rsc);
	if (ret) {
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d during bus_bw_update\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(rsc->m_ahb_clk);
	if (ret)
		goto clks_on_err1;

	ret = clk_prepare_enable(rsc->s_ahb_clk);
	if (ret)
		goto clks_on_err2;

	ret = clk_prepare_enable(rsc->se_clk);
	if (ret)
		goto clks_on_err3;
	return 0;

clks_on_err3:
	clk_disable_unprepare(rsc->s_ahb_clk);
clks_on_err2:
	clk_disable_unprepare(rsc->m_ahb_clk);
clks_on_err1:
	geni_se_rmv_ab_ib(geni_se_dev, rsc);
	return ret;
}
EXPORT_SYMBOL(se_geni_clks_on);

/**
 * se_geni_resources_on() - Turn on resources associated with the serial
 *                          engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_resources_on(struct se_geni_rsc *rsc)
{
	int ret = 0;
	struct geni_se_device *geni_se_dev;

	if (unlikely(!rsc || !rsc->wrapper_dev))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -EPROBE_DEFER;

	ret = pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_active);
	if (ret) {
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d pinctrl_select_state\n", __func__, ret);
		return ret;
	}

	ret = se_geni_clks_on(rsc);
	if (ret) {
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			"%s: Error %d during clks_on\n", __func__, ret);
		pinctrl_select_state(rsc->geni_pinctrl, rsc->geni_gpio_sleep);
	}

	return ret;
}
EXPORT_SYMBOL(se_geni_resources_on);

/**
 * geni_se_resources_init() - Init the SE resource structure
 * @rsc:	SE resource structure to be initialized.
 * @ab:		Initial Average bus bandwidth request value.
 * @ib:		Initial Instantaneous bus bandwidth request value.
 *
 * Return:	0 on success, standard Linux error codes on failure.
 */
int geni_se_resources_init(struct se_geni_rsc *rsc,
			   unsigned long ab, unsigned long ib)
{
	struct geni_se_device *geni_se_dev;
	int ret = 0;
	const char *mode = NULL;

	if (unlikely(!rsc || !rsc->wrapper_dev))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -EPROBE_DEFER;

	if (geni_se_dev->num_paths == 2) {
		if (unlikely(!(geni_se_dev->bus_bw_noc))) {
			geni_se_dev->bus_bw_noc =
			msm_bus_scale_register_client(geni_se_dev->pdata);
			if (!(geni_se_dev->bus_bw_noc)) {
				GENI_SE_ERR(geni_se_dev->log_ctx,
					false, NULL,
				"%s: Error creating bus client\n",  __func__);
				return -EFAULT;
			}
		}

		/* To reduce the higher ab values from individual drivers */
		rsc->ab = ab/2;
		rsc->ib = ab;
		rsc->ab_noc = 0;
		rsc->ib_noc = ib;
		INIT_LIST_HEAD(&rsc->ab_list_noc);
		INIT_LIST_HEAD(&rsc->ib_list_noc);
	} else {
		if (unlikely(IS_ERR_OR_NULL(geni_se_dev->bus_bw))) {
			geni_se_dev->bus_bw = msm_bus_scale_register(
						geni_se_dev->bus_mas_id,
						geni_se_dev->bus_slv_id,
					(char *)dev_name(geni_se_dev->dev),
						false);
			if (IS_ERR_OR_NULL(geni_se_dev->bus_bw)) {
				GENI_SE_ERR(geni_se_dev->log_ctx,
					false, NULL,
				"%s: Error creating bus client\n", __func__);
				return (int)PTR_ERR(geni_se_dev->bus_bw);
			}
		}
		rsc->ab = ab;
		rsc->ib = ib;
	}

	INIT_LIST_HEAD(&rsc->ab_list);
	INIT_LIST_HEAD(&rsc->ib_list);

	ret = of_property_read_string(geni_se_dev->dev->of_node,
					"qcom,iommu-dma", &mode);

	if ((ret == 0) && (strcmp(mode, "disabled") == 0)) {
		ret = geni_se_iommu_map_and_attach(geni_se_dev);
		if (ret)
			GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
				"%s: Error %d iommu_map_and_attach\n",
					 __func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL(geni_se_resources_init);

/**
 * geni_se_clk_tbl_get() - Get the clock table to program DFS
 * @rsc:	Resource for which the clock table is requested.
 * @tbl:	Table in which the output is returned.
 *
 * This function is called by the protocol drivers to determine the different
 * clock frequencies supported by Serail Engine Core Clock. The protocol
 * drivers use the output to determine the clock frequency index to be
 * programmed into DFS.
 *
 * Return:	number of valid performance levels in the table on success,
 *		standard Linux error codes on failure.
 */
int geni_se_clk_tbl_get(struct se_geni_rsc *rsc, unsigned long **tbl)
{
	struct geni_se_device *geni_se_dev;
	int i;
	unsigned long prev_freq = 0;
	int ret = 0;

	if (unlikely(!rsc || !rsc->wrapper_dev || !rsc->se_clk || !tbl))
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -EPROBE_DEFER;
	mutex_lock(&geni_se_dev->geni_dev_lock);
	*tbl = NULL;

	if (geni_se_dev->clk_perf_tbl) {
		*tbl = geni_se_dev->clk_perf_tbl;
		ret = geni_se_dev->num_clk_levels;
		goto exit_se_clk_tbl_get;
	}

	geni_se_dev->clk_perf_tbl = kzalloc(sizeof(*geni_se_dev->clk_perf_tbl) *
						MAX_CLK_PERF_LEVEL, GFP_KERNEL);
	if (!geni_se_dev->clk_perf_tbl) {
		ret = -ENOMEM;
		goto exit_se_clk_tbl_get;
	}

	for (i = 0; i < MAX_CLK_PERF_LEVEL; i++) {
		geni_se_dev->clk_perf_tbl[i] = clk_round_rate(rsc->se_clk,
								prev_freq + 1);
		if (geni_se_dev->clk_perf_tbl[i] == prev_freq) {
			geni_se_dev->clk_perf_tbl[i] = 0;
			break;
		}
		prev_freq = geni_se_dev->clk_perf_tbl[i];
	}
	geni_se_dev->num_clk_levels = i;
	*tbl = geni_se_dev->clk_perf_tbl;
	ret = geni_se_dev->num_clk_levels;
exit_se_clk_tbl_get:
	mutex_unlock(&geni_se_dev->geni_dev_lock);
	return ret;
}
EXPORT_SYMBOL(geni_se_clk_tbl_get);

/**
 * geni_se_clk_freq_match() - Get the matching or closest SE clock frequency
 * @rsc:	Resource for which the clock frequency is requested.
 * @req_freq:	Requested clock frequency.
 * @index:	Index of the resultant frequency in the table.
 * @res_freq:	Resultant frequency which matches or is closer to the
 *		requested frequency.
 * @exact:	Flag to indicate exact multiple requirement of the requested
 *		frequency .
 *
 * This function is called by the protocol drivers to determine the matching
 * or closest frequency of the Serial Engine clock to be selected in order
 * to meet the performance requirements.
 *
 * Return:	0 on success, standard Linux error codes on failure.
 */
int geni_se_clk_freq_match(struct se_geni_rsc *rsc, unsigned long req_freq,
			   unsigned int *index, unsigned long *res_freq,
			   bool exact)
{
	unsigned long *tbl;
	int num_clk_levels;
	int i;
	unsigned long best_delta = 0;
	unsigned long new_delta;
	unsigned int divider;

	num_clk_levels = geni_se_clk_tbl_get(rsc, &tbl);
	if (num_clk_levels < 0)
		return num_clk_levels;

	if (num_clk_levels == 0)
		return -EFAULT;

	*res_freq = 0;

	for (i = 0; i < num_clk_levels; i++) {
		divider = DIV_ROUND_UP(tbl[i], req_freq);
		new_delta = req_freq - (tbl[i] / divider);

		if (!best_delta || new_delta < best_delta) {
			/* We have a new best! */
			*index = i;
			*res_freq = tbl[i];

			/*If the new best is exact then we're done*/
			if (new_delta == 0)
				return 0;

			best_delta = new_delta;
		}
	}

	if (exact || !(*res_freq))
		return -ENOKEY;

	return 0;
}
EXPORT_SYMBOL(geni_se_clk_freq_match);

/**
 * geni_se_tx_dma_prep() - Prepare the Serial Engine for TX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the TX buffer is mapped.
 * @base:		Base address of the SE register block.
 * @tx_buf:		Pointer to the TX buffer.
 * @tx_len:		Length of the TX buffer.
 * @tx_dma:		Pointer to store the mapped DMA address.
 *
 * This function is used to prepare the buffers for DMA TX.
 *
 * Return:	0 on success, standard Linux error codes on error/failure.
 */
int geni_se_tx_dma_prep(struct device *wrapper_dev, void __iomem *base,
			void *tx_buf, int tx_len, dma_addr_t *tx_dma)
{
	int ret;

	if (unlikely(!wrapper_dev || !base || !tx_buf || !tx_len || !tx_dma))
		return -EINVAL;

	ret = geni_se_iommu_map_buf(wrapper_dev, tx_dma, tx_buf, tx_len,
				    DMA_TO_DEVICE);
	if (ret)
		return ret;

	geni_write_reg(7, base, SE_DMA_TX_IRQ_EN_SET);
	geni_write_reg(GENI_SE_DMA_PTR_L(*tx_dma), base, SE_DMA_TX_PTR_L);
	geni_write_reg(GENI_SE_DMA_PTR_H(*tx_dma), base, SE_DMA_TX_PTR_H);
	geni_write_reg(1, base, SE_DMA_TX_ATTR);
	geni_write_reg(tx_len, base, SE_DMA_TX_LEN);
	return 0;
}
EXPORT_SYMBOL(geni_se_tx_dma_prep);

/**
 * geni_se_rx_dma_prep() - Prepare the Serial Engine for RX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the RX buffer is mapped.
 * @base:		Base address of the SE register block.
 * @rx_buf:		Pointer to the RX buffer.
 * @rx_len:		Length of the RX buffer.
 * @rx_dma:		Pointer to store the mapped DMA address.
 *
 * This function is used to prepare the buffers for DMA RX.
 *
 * Return:	0 on success, standard Linux error codes on error/failure.
 */
int geni_se_rx_dma_prep(struct device *wrapper_dev, void __iomem *base,
			void *rx_buf, int rx_len, dma_addr_t *rx_dma)
{
	int ret;

	if (unlikely(!wrapper_dev || !base || !rx_buf || !rx_len || !rx_dma))
		return -EINVAL;

	ret = geni_se_iommu_map_buf(wrapper_dev, rx_dma, rx_buf, rx_len,
				    DMA_FROM_DEVICE);
	if (ret)
		return ret;

	geni_write_reg(7, base, SE_DMA_RX_IRQ_EN_SET);
	geni_write_reg(GENI_SE_DMA_PTR_L(*rx_dma), base, SE_DMA_RX_PTR_L);
	geni_write_reg(GENI_SE_DMA_PTR_H(*rx_dma), base, SE_DMA_RX_PTR_H);
	/* RX does not have EOT bit */
	geni_write_reg(0, base, SE_DMA_RX_ATTR);
	geni_write_reg(rx_len, base, SE_DMA_RX_LEN);
	return 0;
}
EXPORT_SYMBOL(geni_se_rx_dma_prep);

/**
 * geni_se_tx_dma_unprep() - Unprepare the Serial Engine after TX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the RX buffer is mapped.
 * @tx_dma:		DMA address of the TX buffer.
 * @tx_len:		Length of the TX buffer.
 *
 * This function is used to unprepare the DMA buffers after DMA TX.
 */
void geni_se_tx_dma_unprep(struct device *wrapper_dev,
			   dma_addr_t tx_dma, int tx_len)
{
	if (tx_dma)
		geni_se_iommu_unmap_buf(wrapper_dev, &tx_dma, tx_len,
					DMA_TO_DEVICE);
}
EXPORT_SYMBOL(geni_se_tx_dma_unprep);

/**
 * geni_se_rx_dma_unprep() - Unprepare the Serial Engine after RX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the RX buffer is mapped.
 * @rx_dma:		DMA address of the RX buffer.
 * @rx_len:		Length of the RX buffer.
 *
 * This function is used to unprepare the DMA buffers after DMA RX.
 */
void geni_se_rx_dma_unprep(struct device *wrapper_dev,
			   dma_addr_t rx_dma, int rx_len)
{
	if (rx_dma)
		geni_se_iommu_unmap_buf(wrapper_dev, &rx_dma, rx_len,
					DMA_FROM_DEVICE);
}
EXPORT_SYMBOL(geni_se_rx_dma_unprep);

/**
 * geni_se_qupv3_hw_version() - Read the QUPv3 Hardware version
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @major:		Buffer for Major Version field.
 * @minor:		Buffer for Minor Version field.
 * @step:		Buffer for Step Version field.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_qupv3_hw_version(struct device *wrapper_dev, unsigned int *major,
			     unsigned int *minor, unsigned int *step)
{
	unsigned int version;
	struct geni_se_device *geni_se_dev;

	if (!wrapper_dev || !major || !minor || !step)
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(wrapper_dev);
	if (unlikely(!geni_se_dev))
		return -ENODEV;

	version = geni_read_reg(geni_se_dev->base, QUPV3_HW_VER);
	*major = (version & HW_VER_MAJOR_MASK) >> HW_VER_MAJOR_SHFT;
	*minor = (version & HW_VER_MINOR_MASK) >> HW_VER_MINOR_SHFT;
	*step = version & HW_VER_STEP_MASK;
	return 0;
}
EXPORT_SYMBOL(geni_se_qupv3_hw_version);

static int geni_se_iommu_map_and_attach(struct geni_se_device *geni_se_dev)
{
	return 0;
}

/**
 * geni_se_iommu_map_buf() - Map a single buffer into QUPv3 context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @buf:		Address of the buffer that needs to be mapped.
 * @size:		Size of the buffer.
 * @dir:		Direction of the DMA transfer.
 *
 * This function is used to map an already allocated buffer into the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_iommu_map_buf(struct device *wrapper_dev, dma_addr_t *iova,
			  void *buf, size_t size, enum dma_data_direction dir)
{
	struct device *cb_dev;
	struct geni_se_device *geni_se_dev;

	if (!wrapper_dev || !iova || !buf || !size)
		return -EINVAL;

	*iova = DMA_ERROR_CODE;
	geni_se_dev = dev_get_drvdata(wrapper_dev);
	if (!geni_se_dev || !geni_se_dev->cb_dev)
		return -ENODEV;

	cb_dev = geni_se_dev->cb_dev;

	*iova = dma_map_single(cb_dev, buf, size, dir);
	if (dma_mapping_error(cb_dev, *iova))
		return -EIO;
	return 0;
}
EXPORT_SYMBOL(geni_se_iommu_map_buf);

/**
 * geni_se_iommu_alloc_buf() - Allocate & map a single buffer into QUPv3
 *			       context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @size:		Size of the buffer.
 *
 * This function is used to allocate a buffer and map it into the
 * QUPv3 context bank device space.
 *
 * Return:	address of the buffer on success, NULL or ERR_PTR on
 *		failure/error.
 */
void *geni_se_iommu_alloc_buf(struct device *wrapper_dev, dma_addr_t *iova,
			      size_t size)
{
	struct device *cb_dev;
	struct geni_se_device *geni_se_dev;
	void *buf = NULL;

	if (!wrapper_dev || !iova || !size)
		return ERR_PTR(-EINVAL);

	*iova = DMA_ERROR_CODE;
	geni_se_dev = dev_get_drvdata(wrapper_dev);
	if (!geni_se_dev || !geni_se_dev->cb_dev)
		return ERR_PTR(-ENODEV);

	cb_dev = geni_se_dev->cb_dev;

	buf = dma_alloc_coherent(cb_dev, size, iova, GFP_KERNEL);
	if (!buf)
		GENI_SE_ERR(geni_se_dev->log_ctx, false, NULL,
			    "%s: Failed dma_alloc_coherent\n", __func__);
	return buf;
}
EXPORT_SYMBOL(geni_se_iommu_alloc_buf);

/**
 * geni_se_iommu_unmap_buf() - Unmap a single buffer from QUPv3 context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @size:		Size of the buffer.
 * @dir:		Direction of the DMA transfer.
 *
 * This function is used to unmap an already mapped buffer from the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_iommu_unmap_buf(struct device *wrapper_dev, dma_addr_t *iova,
			    size_t size, enum dma_data_direction dir)
{
	struct device *cb_dev;
	struct geni_se_device *geni_se_dev;

	if (!wrapper_dev || !iova || !size)
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(wrapper_dev);
	if (!geni_se_dev || !geni_se_dev->cb_dev)
		return -ENODEV;

	cb_dev = geni_se_dev->cb_dev;

	dma_unmap_single(cb_dev, *iova, size, dir);
	return 0;
}
EXPORT_SYMBOL(geni_se_iommu_unmap_buf);

/**
 * geni_se_iommu_free_buf() - Unmap & free a single buffer from QUPv3
 *			      context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @buf:		Address of the buffer.
 * @size:		Size of the buffer.
 *
 * This function is used to unmap and free a buffer from the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_iommu_free_buf(struct device *wrapper_dev, dma_addr_t *iova,
			   void *buf, size_t size)
{
	struct device *cb_dev;
	struct geni_se_device *geni_se_dev;

	if (!wrapper_dev || !iova || !buf || !size)
		return -EINVAL;

	geni_se_dev = dev_get_drvdata(wrapper_dev);
	if (!geni_se_dev || !geni_se_dev->cb_dev)
		return -ENODEV;

	cb_dev = geni_se_dev->cb_dev;

	dma_free_coherent(cb_dev, size, buf, *iova);
	return 0;
}
EXPORT_SYMBOL(geni_se_iommu_free_buf);

/**
 * geni_se_dump_dbg_regs() - Print relevant registers that capture most
 *			accurately the state of an SE.
 * @_dev:		Pointer to the SE's device.
 * @iomem:		Base address of the SE's register space.
 * @ipc:		IPC log context handle.
 *
 * This function is used to print out all the registers that capture the state
 * of an SE to help debug any errors.
 *
 * Return:	None
 */
void geni_se_dump_dbg_regs(struct se_geni_rsc *rsc, void __iomem *base,
				void *ipc)
{
	u32 m_cmd0 = 0;
	u32 m_irq_status = 0;
	u32 s_cmd0 = 0;
	u32 s_irq_status = 0;
	u32 geni_status = 0;
	u32 geni_ios = 0;
	u32 dma_rx_irq = 0;
	u32 dma_tx_irq = 0;
	u32 rx_fifo_status = 0;
	u32 tx_fifo_status = 0;
	u32 se_dma_dbg = 0;
	u32 m_cmd_ctrl = 0;
	u32 se_dma_rx_len = 0;
	u32 se_dma_rx_len_in = 0;
	u32 se_dma_tx_len = 0;
	u32 se_dma_tx_len_in = 0;
	struct geni_se_device *geni_se_dev;

	if (!ipc)
		return;

	geni_se_dev = dev_get_drvdata(rsc->wrapper_dev);
	if (unlikely(!geni_se_dev || !geni_se_dev->bus_bw))
		return;
	if (unlikely(list_empty(&rsc->ab_list) || list_empty(&rsc->ib_list))) {
		GENI_SE_DBG(ipc, false, NULL, "%s: Clocks not on\n", __func__);
		return;
	}
	m_cmd0 = geni_read_reg(base, SE_GENI_M_CMD0);
	m_irq_status = geni_read_reg(base, SE_GENI_M_IRQ_STATUS);
	s_cmd0 = geni_read_reg(base, SE_GENI_S_CMD0);
	s_irq_status = geni_read_reg(base, SE_GENI_S_IRQ_STATUS);
	geni_status = geni_read_reg(base, SE_GENI_STATUS);
	geni_ios = geni_read_reg(base, SE_GENI_IOS);
	dma_tx_irq = geni_read_reg(base, SE_DMA_TX_IRQ_STAT);
	dma_rx_irq = geni_read_reg(base, SE_DMA_RX_IRQ_STAT);
	rx_fifo_status = geni_read_reg(base, SE_GENI_RX_FIFO_STATUS);
	tx_fifo_status = geni_read_reg(base, SE_GENI_TX_FIFO_STATUS);
	se_dma_dbg = geni_read_reg(base, SE_DMA_DEBUG_REG0);
	m_cmd_ctrl = geni_read_reg(base, SE_GENI_M_CMD_CTRL_REG);
	se_dma_rx_len = geni_read_reg(base, SE_DMA_RX_LEN);
	se_dma_rx_len_in = geni_read_reg(base, SE_DMA_RX_LEN_IN);
	se_dma_tx_len = geni_read_reg(base, SE_DMA_TX_LEN);
	se_dma_tx_len_in = geni_read_reg(base, SE_DMA_TX_LEN_IN);

	GENI_SE_DBG(ipc, false, NULL,
	"%s: m_cmd0:0x%x, m_irq_status:0x%x, geni_status:0x%x, geni_ios:0x%x\n",
	__func__, m_cmd0, m_irq_status, geni_status, geni_ios);
	GENI_SE_DBG(ipc, false, NULL,
	"dma_rx_irq:0x%x, dma_tx_irq:0x%x, rx_fifo_sts:0x%x, tx_fifo_sts:0x%x\n"
	, dma_rx_irq, dma_tx_irq, rx_fifo_status, tx_fifo_status);
	GENI_SE_DBG(ipc, false, NULL,
	"se_dma_dbg:0x%x, m_cmd_ctrl:0x%x, dma_rxlen:0x%x, dma_rxlen_in:0x%x\n",
	se_dma_dbg, m_cmd_ctrl, se_dma_rx_len, se_dma_rx_len_in);
	GENI_SE_DBG(ipc, false, NULL,
	"dma_txlen:0x%x, dma_txlen_in:0x%x\n", se_dma_tx_len, se_dma_tx_len_in);
}
EXPORT_SYMBOL(geni_se_dump_dbg_regs);

static const struct of_device_id geni_se_dt_match[] = {
	{ .compatible = "qcom,qupv3-geni-se", },
	{ .compatible = "qcom,qupv3-geni-se-cb", },
	{}
};

static struct msm_bus_scale_pdata *ab_ib_register(struct platform_device *pdev,
				struct geni_se_device *host)
{
	int rc = 0;
	struct device *dev = &pdev->dev;
	int i = 0, j, len;
	bool mem_err = false;
	const uint32_t *vec_arr = NULL;
	struct msm_bus_scale_pdata *pdata = NULL;
	struct msm_bus_paths *usecase = NULL;

	vec_arr = of_get_property(dev->of_node,
			"qcom,msm-bus,vectors-bus-ids", &len);
	if (vec_arr == NULL) {
		pr_err("Error: Vector array not found\n");
		rc = 1;
		goto out;
	}

	if (len != host->num_paths * sizeof(uint32_t) * 2) {
		pr_err("Error: Length-error on getting vectors\n");
		rc = 1;
		goto out;
	}


	pdata = devm_kzalloc(dev, sizeof(struct msm_bus_scale_pdata),
							GFP_KERNEL);
	if (!pdata) {
		mem_err = true;
		goto out;
	}

	pdata->name = (char *)dev_name(host->dev);

	pdata->num_usecases = 2;

	pdata->active_only = 0;

	usecase = devm_kzalloc(dev, (sizeof(struct msm_bus_paths) *
		pdata->num_usecases), GFP_KERNEL);
	if (!usecase) {
		mem_err = true;
		goto out;
	}

	for (i = 0; i < pdata->num_usecases; i++) {
		usecase[i].num_paths = host->num_paths;
		usecase[i].vectors = devm_kzalloc(dev, host->num_paths *
			sizeof(struct msm_bus_vectors), GFP_KERNEL);
		if (!usecase[i].vectors) {
			mem_err = true;
			pr_err("Error: Mem alloc failure in vectors\n");
			goto out;
		}

		for (j = 0; j < host->num_paths; j++) {
			int index = (j * 2);

			usecase[i].vectors[j].src =
					be32_to_cpu(vec_arr[index]);
			usecase[i].vectors[j].dst =
					be32_to_cpu(vec_arr[index + 1]);
			usecase[i].vectors[j].ab = 0;
			usecase[i].vectors[j].ib = 0;
		}
	}

	pdata->usecase = usecase;

	return pdata;
out:
	if (mem_err) {
		for ( ; i > 0; i--)
			devm_kfree(dev, usecase[i-1].vectors);
		devm_kfree(dev, usecase);
		devm_kfree(dev, pdata);
	}
	return NULL;
}

static int geni_se_iommu_probe(struct device *dev)
{
	struct geni_se_device *geni_se_dev;

	if (unlikely(!dev->parent)) {
		dev_err(dev, "%s no parent for this device\n", __func__);
		return -EINVAL;
	}

	geni_se_dev = dev_get_drvdata(dev->parent);
	if (unlikely(!geni_se_dev)) {
		dev_err(dev, "%s geni_se_dev not found\n", __func__);
		return -EINVAL;
	}
	geni_se_dev->cb_dev = dev;

	GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
		    "%s: Probe successful\n", __func__);
	return 0;
}

static int geni_se_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct geni_se_device *geni_se_dev;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(&pdev->dev, "could not set DMA mask\n");
			return ret;
		}
	}

	if (of_device_is_compatible(dev->of_node, "qcom,qupv3-geni-se-cb"))
		return geni_se_iommu_probe(dev);

	geni_se_dev = devm_kzalloc(dev, sizeof(*geni_se_dev), GFP_KERNEL);
	if (!geni_se_dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "%s: Mandatory resource info not found\n",
			__func__);
		devm_kfree(dev, geni_se_dev);
		return -EINVAL;
	}

	geni_se_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(geni_se_dev->base)) {
		dev_err(dev, "%s: Error mapping the resource\n", __func__);
		devm_kfree(dev, geni_se_dev);
		return -EFAULT;
	}

	geni_se_dev->dev = dev;
	geni_se_dev->cb_dev = dev;
	ret = of_property_read_u32(dev->of_node, "qcom,msm-bus,num-paths",
					&geni_se_dev->num_paths);
	if (!ret) {
		geni_se_dev->update = 0;
		geni_se_dev->pdata = ab_ib_register(pdev, geni_se_dev);
		if (geni_se_dev->pdata == NULL) {
			dev_err(dev,
			"%s: Error missing bus master and slave id\n",
								__func__);
			devm_iounmap(dev, geni_se_dev->base);
			devm_kfree(dev, geni_se_dev);
		}
	}

	else {
		geni_se_dev->num_paths = 1;
		ret = of_property_read_u32(dev->of_node, "qcom,bus-mas-id",
				   &geni_se_dev->bus_mas_id);
		if (ret) {
			dev_err(dev, "%s: Error missing bus master id\n",
								__func__);
			devm_iounmap(dev, geni_se_dev->base);
			devm_kfree(dev, geni_se_dev);
		}
		ret = of_property_read_u32(dev->of_node, "qcom,bus-slv-id",
				   &geni_se_dev->bus_slv_id);
		if (ret) {
			dev_err(dev, "%s: Error missing bus slave id\n",
								 __func__);
			devm_iounmap(dev, geni_se_dev->base);
			devm_kfree(dev, geni_se_dev);
		}
	}

	geni_se_dev->vote_for_bw = of_property_read_bool(dev->of_node,
							"qcom,vote-for-bw");
	geni_se_dev->iommu_s1_bypass = of_property_read_bool(dev->of_node,
							"qcom,iommu-s1-bypass");
	geni_se_dev->bus_bw_set = default_bus_bw_set;
	geni_se_dev->bus_bw_set_size =
				ARRAY_SIZE(default_bus_bw_set);
	if (geni_se_dev->num_paths == 2) {
		geni_se_dev->bus_bw_set_noc = default_bus_bw_set;
		geni_se_dev->bus_bw_set_size_noc =
				ARRAY_SIZE(default_bus_bw_set);
	}
	mutex_init(&geni_se_dev->iommu_lock);
	INIT_LIST_HEAD(&geni_se_dev->ab_list_head);
	INIT_LIST_HEAD(&geni_se_dev->ib_list_head);
	if (geni_se_dev->num_paths == 2) {
		INIT_LIST_HEAD(&geni_se_dev->ab_list_head_noc);
		INIT_LIST_HEAD(&geni_se_dev->ib_list_head_noc);
	}
	mutex_init(&geni_se_dev->geni_dev_lock);
	geni_se_dev->log_ctx = ipc_log_context_create(NUM_LOG_PAGES,
						dev_name(geni_se_dev->dev), 0);
	if (!geni_se_dev->log_ctx)
		dev_err(dev, "%s Failed to allocate log context\n", __func__);
	dev_set_drvdata(dev, geni_se_dev);

	ret = of_platform_populate(dev->of_node, geni_se_dt_match, NULL, dev);
	if (ret) {
		dev_err(dev, "%s: Error populating children\n", __func__);
		devm_iounmap(dev, geni_se_dev->base);
		devm_kfree(dev, geni_se_dev);
	}

	GENI_SE_DBG(geni_se_dev->log_ctx, false, NULL,
		    "%s: Probe successful\n", __func__);
	return ret;
}

static int geni_se_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct geni_se_device *geni_se_dev = dev_get_drvdata(dev);

	ipc_log_context_destroy(geni_se_dev->log_ctx);
	devm_iounmap(dev, geni_se_dev->base);
	devm_kfree(dev, geni_se_dev);
	return 0;
}

static struct platform_driver geni_se_driver = {
	.driver = {
		.name = "qupv3_geni_se",
		.of_match_table = geni_se_dt_match,
	},
	.probe = geni_se_probe,
	.remove = geni_se_remove,
};

static int __init geni_se_driver_init(void)
{
	return platform_driver_register(&geni_se_driver);
}
arch_initcall(geni_se_driver_init);

static void __exit geni_se_driver_exit(void)
{
	platform_driver_unregister(&geni_se_driver);
}
module_exit(geni_se_driver_exit);

MODULE_DESCRIPTION("GENI Serial Engine Driver");
MODULE_LICENSE("GPL v2");
