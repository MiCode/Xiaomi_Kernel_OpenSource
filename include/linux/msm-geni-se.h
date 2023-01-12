/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_MSM_GENI_SE
#define _LINUX_MSM_GENI_SE
#include <linux/clk.h>
#include <linux/dma-direction.h>
#include <linux/io.h>
#include <linux/list.h>

/* Transfer mode supported by GENI Serial Engines */
enum se_xfer_mode {
	INVALID,
	FIFO_MODE,
	GSI_DMA,
	SE_DMA,
};

/* Protocols supported by GENI Serial Engines */
enum se_protocol_types {
	NONE,
	SPI,
	UART,
	I2C,
	I3C,
	SPI_SLAVE
};

/**
 * struct geni_se_rsc - GENI Serial Engine Resource
 * @ctrl_dev		Pointer to controller device.
 * @wrapper_dev:	Pointer to the parent QUPv3 core.
 * @se_clk:		Handle to the core serial engine clock.
 * @m_ahb_clk:		Handle to the primary AHB clock.
 * @s_ahb_clk:		Handle to the secondary AHB clock.
 * @ab_list:		List Head of Average bus banwidth list.
 * @ab_list_noc:	List Head of Average DDR path bus
			bandwidth list.
 * @ab:			Average bus bandwidth request value.
 * @ab_noc:		Average DDR path bus bandwidth request value.
 * @ib_list:		List Head of Instantaneous bus banwidth list.
 * @ib_list_noc:	List Head of Instantaneous DDR path bus
			bandwidth list.
 * @ib:			Instantaneous bus bandwidth request value.
 * @ib_noc:		Instantaneous DDR path bus bandwidth
			request value.
 * @geni_pinctrl:	Handle to the pinctrl configuration.
 * @geni_gpio_active:	Handle to the default/active pinctrl state.
 * @geni_gpi_sleep:	Handle to the sleep pinctrl state.
 * @num_clk_levels:	Number of valid clock levels in clk_perf_tbl.
 * @clk_perf_tbl:	Table of clock frequency input to Serial Engine clock.
 * @skip_bw_vote:	Used for PMIC over i2c use case to skip the BW vote.
 */
struct se_geni_rsc {
	struct device *ctrl_dev;
	struct device *wrapper_dev;
	struct clk *se_clk;
	struct clk *m_ahb_clk;
	struct clk *s_ahb_clk;
	struct list_head ab_list;
	struct list_head ab_list_noc;
	unsigned long ab;
	unsigned long ab_noc;
	struct list_head ib_list;
	struct list_head ib_list_noc;
	unsigned long ib;
	unsigned long ib_noc;
	struct pinctrl *geni_pinctrl;
	struct pinctrl_state *geni_gpio_shutdown;
	struct pinctrl_state *geni_gpio_active;
	struct pinctrl_state *geni_gpio_sleep;
	int	clk_freq_out;
	unsigned int num_clk_levels;
	unsigned long *clk_perf_tbl;
	bool skip_bw_vote;
};

#define PINCTRL_DEFAULT	"default"
#define PINCTRL_ACTIVE	"active"
#define PINCTRL_SLEEP	"sleep"
#define PINCTRL_SHUTDOWN	"shutdown"

#define KHz(freq) (1000 * (freq))

/* Common SE registers */
#define GENI_INIT_CFG_REVISION		(0x0)
#define GENI_S_INIT_CFG_REVISION	(0x4)
#define GENI_FORCE_DEFAULT_REG		(0x20)
#define GENI_OUTPUT_CTRL		(0x24)
#define GENI_CGC_CTRL			(0x28)
#define SE_GENI_STATUS			(0x40)
#define GENI_SER_M_CLK_CFG		(0x48)
#define GENI_SER_S_CLK_CFG		(0x4C)
#define GENI_CLK_CTRL_RO		(0x60)
#define GENI_IF_FIFO_DISABLE_RO		(0x64)
#define GENI_FW_REVISION_RO		(0x68)
#define GENI_FW_S_REVISION_RO		(0x6C)
#define SE_GENI_CLK_SEL			(0x7C)
#define SE_GENI_CFG_SEQ_START		(0x84)
#define SE_GENI_CFG_REG			(0x200)
#define GENI_CFG_REG80			(0x240)
#define SE_GENI_BYTE_GRAN		(0x254)
#define SE_GENI_DMA_MODE_EN		(0x258)
#define SE_GENI_TX_PACKING_CFG0		(0x260)
#define SE_GENI_TX_PACKING_CFG1		(0x264)
#define SE_GENI_RX_PACKING_CFG0		(0x284)
#define SE_GENI_RX_PACKING_CFG1		(0x288)
#define SE_GENI_M_CMD0			(0x600)
#define SE_GENI_M_CMD_CTRL_REG		(0x604)
#define SE_GENI_M_IRQ_STATUS		(0x610)
#define SE_GENI_M_IRQ_EN		(0x614)
#define SE_GENI_M_IRQ_CLEAR		(0x618)
#define SE_GENI_S_CMD0			(0x630)
#define SE_GENI_S_CMD_CTRL_REG		(0x634)
#define SE_GENI_S_IRQ_STATUS		(0x640)
#define SE_GENI_S_IRQ_EN		(0x644)
#define SE_GENI_S_IRQ_CLEAR		(0x648)
#define SE_GENI_TX_FIFOn		(0x700)
#define SE_GENI_RX_FIFOn		(0x780)
#define SE_GENI_TX_FIFO_STATUS		(0x800)
#define SE_GENI_RX_FIFO_STATUS		(0x804)
#define SE_GENI_TX_WATERMARK_REG	(0x80C)
#define SE_GENI_RX_WATERMARK_REG	(0x810)
#define SE_GENI_RX_RFR_WATERMARK_REG	(0x814)
#define SE_GENI_IOS			(0x908)
#define SE_GENI_M_GP_LENGTH		(0x910)
#define SE_GENI_S_GP_LENGTH		(0x914)
#define SE_GSI_EVENT_EN			(0xE18)
#define SE_IRQ_EN			(0xE1C)
#define SE_HW_PARAM_0			(0xE24)
#define SE_HW_PARAM_1			(0xE28)
#define SE_DMA_GENERAL_CFG		(0xE30)
#define SE_DMA_DEBUG_REG0		(0xE40)
#define SLAVE_MODE_EN			(BIT(3))
#define START_TRIGGER			(BIT(0))
#define QUPV3_HW_VER			(0x4)

/* GENI_OUTPUT_CTRL fields */
#define DEFAULT_IO_OUTPUT_CTRL_MSK      (GENMASK(6, 0))
#define GENI_IO_MUX_0_EN		BIT(0)
#define GENI_IO_MUX_1_EN		BIT(1)

/* GENI_CFG_REG80 fields */
#define IO1_SEL_TX			BIT(2)
#define IO2_DATA_IN_SEL_PAD2		(GENMASK(11, 10))
#define IO3_DATA_IN_SEL_PAD2		BIT(15)

/* GENI_FORCE_DEFAULT_REG fields */
#define FORCE_DEFAULT	(BIT(0))

/* GENI_CGC_CTRL fields */
#define CFG_AHB_CLK_CGC_ON		(BIT(0))
#define CFG_AHB_WR_ACLK_CGC_ON		(BIT(1))
#define DATA_AHB_CLK_CGC_ON		(BIT(2))
#define SCLK_CGC_ON			(BIT(3))
#define TX_CLK_CGC_ON			(BIT(4))
#define RX_CLK_CGC_ON			(BIT(5))
#define EXT_CLK_CGC_ON			(BIT(6))
#define PROG_RAM_HCLK_OFF		(BIT(8))
#define PROG_RAM_SCLK_OFF		(BIT(9))
#define DEFAULT_CGC_EN			(GENMASK(6, 0))

/* GENI_STATUS fields */
#define M_GENI_CMD_ACTIVE		(BIT(0))
#define S_GENI_CMD_ACTIVE		(BIT(12))

/* GENI_SER_M_CLK_CFG/GENI_SER_S_CLK_CFG */
#define SER_CLK_EN			(BIT(0))
#define CLK_DIV_MSK			(GENMASK(15, 4))
#define CLK_DIV_SHFT			(4)

/* CLK_CTRL_RO fields */

/* FIFO_IF_DISABLE_RO fields */
#define FIFO_IF_DISABLE			(BIT(0))

/* FW_REVISION_RO fields */
#define FW_REV_PROTOCOL_MSK	(GENMASK(15, 8))
#define FW_REV_PROTOCOL_SHFT	(8)
#define FW_REV_VERSION_MSK	(GENMASK(7, 0))

/* GENI_CLK_SEL fields */
#define CLK_SEL_MSK		(GENMASK(2, 0))

/* SE_GENI_DMA_MODE_EN */
#define GENI_DMA_MODE_EN	(BIT(0))

/* GENI_M_CMD0 fields */
#define M_OPCODE_MSK		(GENMASK(31, 27))
#define M_OPCODE_SHFT		(27)
#define M_PARAMS_MSK		(GENMASK(26, 0))

/* GENI_M_CMD_CTRL_REG */
#define M_GENI_CMD_CANCEL	BIT(2)
#define M_GENI_CMD_ABORT	BIT(1)
#define M_GENI_DISABLE		BIT(0)

/* GENI_S_CMD0 fields */
#define S_OPCODE_MSK		(GENMASK(31, 27))
#define S_OPCODE_SHFT		(27)
#define S_PARAMS_MSK		(GENMASK(26, 0))

/* GENI_S_CMD_CTRL_REG */
#define S_GENI_CMD_CANCEL	(BIT(2))
#define S_GENI_CMD_ABORT	(BIT(1))
#define S_GENI_DISABLE		(BIT(0))

/* GENI_M_IRQ_EN fields */
#define M_CMD_DONE_EN		(BIT(0))
#define M_CMD_OVERRUN_EN	(BIT(1))
#define M_ILLEGAL_CMD_EN	(BIT(2))
#define M_CMD_FAILURE_EN	(BIT(3))
#define M_CMD_CANCEL_EN		(BIT(4))
#define M_CMD_ABORT_EN		(BIT(5))
#define M_TIMESTAMP_EN		(BIT(6))
#define M_RX_IRQ_EN		(BIT(7))
#define M_GP_SYNC_IRQ_0_EN	(BIT(8))
#define M_GP_IRQ_0_EN		(BIT(9))
#define M_GP_IRQ_1_EN		(BIT(10))
#define M_GP_IRQ_2_EN		(BIT(11))
#define M_GP_IRQ_3_EN		(BIT(12))
#define M_GP_IRQ_4_EN		(BIT(13))
#define M_GP_IRQ_5_EN		(BIT(14))
#define M_IO_DATA_DEASSERT_EN	(BIT(22))
#define M_IO_DATA_ASSERT_EN	(BIT(23))
#define M_RX_FIFO_RD_ERR_EN	(BIT(24))
#define M_RX_FIFO_WR_ERR_EN	(BIT(25))
#define M_RX_FIFO_WATERMARK_EN	(BIT(26))
#define M_RX_FIFO_LAST_EN	(BIT(27))
#define M_TX_FIFO_RD_ERR_EN	(BIT(28))
#define M_TX_FIFO_WR_ERR_EN	(BIT(29))
#define M_TX_FIFO_WATERMARK_EN	(BIT(30))
#define M_SEC_IRQ_EN		(BIT(31))
#define M_COMMON_GENI_M_IRQ_EN	(GENMASK(6, 1) | \
				M_IO_DATA_DEASSERT_EN | \
				M_IO_DATA_ASSERT_EN | M_RX_FIFO_RD_ERR_EN | \
				M_RX_FIFO_WR_ERR_EN | M_TX_FIFO_RD_ERR_EN | \
				M_TX_FIFO_WR_ERR_EN)

/* GENI_S_IRQ_EN fields */
#define S_CMD_DONE_EN		(BIT(0))
#define S_CMD_OVERRUN_EN	(BIT(1))
#define S_ILLEGAL_CMD_EN	(BIT(2))
#define S_CMD_FAILURE_EN	(BIT(3))
#define S_CMD_CANCEL_EN		(BIT(4))
#define S_CMD_ABORT_EN		(BIT(5))
#define S_GP_SYNC_IRQ_0_EN	(BIT(8))
#define S_GP_IRQ_0_EN		(BIT(9))
#define S_GP_IRQ_1_EN		(BIT(10))
#define S_GP_IRQ_2_EN		(BIT(11))
#define S_GP_IRQ_3_EN		(BIT(12))
#define S_GP_IRQ_4_EN		(BIT(13))
#define S_GP_IRQ_5_EN		(BIT(14))
#define S_IO_DATA_DEASSERT_EN	(BIT(22))
#define S_IO_DATA_ASSERT_EN	(BIT(23))
#define S_RX_FIFO_RD_ERR_EN	(BIT(24))
#define S_RX_FIFO_WR_ERR_EN	(BIT(25))
#define S_RX_FIFO_WATERMARK_EN	(BIT(26))
#define S_RX_FIFO_LAST_EN	(BIT(27))
#define S_COMMON_GENI_S_IRQ_EN	(GENMASK(5, 1) | GENMASK(13, 9) | \
				 S_RX_FIFO_RD_ERR_EN | S_RX_FIFO_WR_ERR_EN)

/*  GENI_/TX/RX/RX_RFR/_WATERMARK_REG fields */
#define WATERMARK_MSK		(GENMASK(5, 0))

/* GENI_TX_FIFO_STATUS fields */
#define TX_FIFO_WC		(GENMASK(27, 0))

/*  GENI_RX_FIFO_STATUS fields */
#define RX_LAST			(BIT(31))
#define RX_LAST_BYTE_VALID_MSK	(GENMASK(30, 28))
#define RX_LAST_BYTE_VALID_SHFT	(28)
#define RX_FIFO_WC_MSK		(GENMASK(24, 0))

/* SE_GSI_EVENT_EN fields */
#define DMA_RX_EVENT_EN		(BIT(0))
#define DMA_TX_EVENT_EN		(BIT(1))
#define GENI_M_EVENT_EN		(BIT(2))
#define GENI_S_EVENT_EN		(BIT(3))

/* SE_GENI_IOS fields */
#define IO2_DATA_IN		(BIT(1))
#define RX_DATA_IN		(BIT(0))

/* SE_IRQ_EN fields */
#define DMA_RX_IRQ_EN		(BIT(0))
#define DMA_TX_IRQ_EN		(BIT(1))
#define GENI_M_IRQ_EN		(BIT(2))
#define GENI_S_IRQ_EN		(BIT(3))

/* SE_HW_PARAM_0 fields */
#define TX_FIFO_WIDTH_MSK	(GENMASK(29, 24))
#define TX_FIFO_WIDTH_SHFT	(24)
#define TX_FIFO_DEPTH_MSK	(GENMASK(21, 16))
#define TX_FIFO_DEPTH_SHFT	(16)
#define GEN_I3C_IBI_CTRL	(BIT(7))

/* SE_HW_PARAM_1 fields */
#define RX_FIFO_WIDTH_MSK	(GENMASK(29, 24))
#define RX_FIFO_WIDTH_SHFT	(24)
#define RX_FIFO_DEPTH_MSK	(GENMASK(21, 16))
#define RX_FIFO_DEPTH_SHFT	(16)

/* SE_DMA_GENERAL_CFG */
#define DMA_RX_CLK_CGC_ON	(BIT(0))
#define DMA_TX_CLK_CGC_ON	(BIT(1))
#define DMA_AHB_SLV_CFG_ON	(BIT(2))
#define AHB_SEC_SLV_CLK_CGC_ON	(BIT(3))
#define DUMMY_RX_NON_BUFFERABLE	(BIT(4))
#define RX_DMA_ZERO_PADDING_EN	(BIT(5))
#define RX_DMA_IRQ_DELAY_MSK	(GENMASK(8, 6))
#define RX_DMA_IRQ_DELAY_SHFT	(6)

#define SE_DMA_TX_PTR_L		(0xC30)
#define SE_DMA_TX_PTR_H		(0xC34)
#define SE_DMA_TX_ATTR		(0xC38)
#define SE_DMA_TX_LEN		(0xC3C)
#define SE_DMA_TX_IRQ_STAT	(0xC40)
#define SE_DMA_TX_IRQ_CLR	(0xC44)
#define SE_DMA_TX_IRQ_EN	(0xC48)
#define SE_DMA_TX_IRQ_EN_SET	(0xC4C)
#define SE_DMA_TX_IRQ_EN_CLR	(0xC50)
#define SE_DMA_TX_LEN_IN	(0xC54)
#define SE_DMA_TX_FSM_RST	(0xC58)
#define SE_DMA_TX_MAX_BURST	(0xC5C)

#define SE_DMA_RX_PTR_L		(0xD30)
#define SE_DMA_RX_PTR_H		(0xD34)
#define SE_DMA_RX_ATTR		(0xD38)
#define SE_DMA_RX_LEN		(0xD3C)
#define SE_DMA_RX_IRQ_STAT	(0xD40)
#define SE_DMA_RX_IRQ_CLR	(0xD44)
#define SE_DMA_RX_IRQ_EN	(0xD48)
#define SE_DMA_RX_IRQ_EN_SET	(0xD4C)
#define SE_DMA_RX_IRQ_EN_CLR	(0xD50)
#define SE_DMA_RX_LEN_IN	(0xD54)
#define SE_DMA_RX_FSM_RST	(0xD58)
#define SE_DMA_RX_MAX_BURST	(0xD5C)
#define SE_DMA_RX_FLUSH		(0xD60)

/* SE_DMA_TX_IRQ_STAT Register fields */
#define TX_DMA_DONE		(BIT(0))
#define TX_EOT			(BIT(1))
#define TX_SBE			(BIT(2))
#define TX_RESET_DONE		(BIT(3))
#define TX_GENI_CANCEL_IRQ	(BIT(14))

/* SE_DMA_RX_IRQ_STAT Register fields */
#define RX_DMA_DONE		(BIT(0))
#define RX_EOT			(BIT(1))
#define RX_SBE			(BIT(2))
#define RX_RESET_DONE		(BIT(3))
#define RX_FLUSH_DONE		(BIT(4))
#define RX_GENI_GP_IRQ		(GENMASK(10, 5))
#define RX_GENI_CANCEL_IRQ	(BIT(14))
#define RX_GENI_GP_IRQ_EXT	(GENMASK(13, 12))

/* DMA DEBUG Register fields */
#define DMA_TX_ACTIVE		(BIT(0))
#define DMA_RX_ACTIVE		(BIT(1))
#define DMA_TX_STATE		(GENMASK(7, 4))
#define DMA_RX_STATE		(GENMASK(11, 8))

#define DEFAULT_BUS_WIDTH	(4)

/* GSI TRE fields */
/* Packing fields */
#define GSI_TX_PACK_EN          (BIT(0))
#define GSI_RX_PACK_EN          (BIT(1))
#define GSI_PRESERVE_PACK       (BIT(2))

#define GENI_SE_ERR(log_ctx, print, dev, x...) do { \
if (log_ctx) \
	ipc_log_string(log_ctx, x); \
if (print) { \
	if (dev) \
		dev_err((dev), x); \
	else \
		pr_err(x); \
} \
} while (0)

#define GENI_SE_DBG(log_ctx, print, dev, x...) do { \
if (log_ctx) \
	ipc_log_string(log_ctx, x); \
if (print) { \
	if (dev) \
		dev_dbg((dev), x); \
	else \
		pr_debug(x); \
} \
} while (0)

/* In KHz */
#define DEFAULT_SE_CLK  19200
#define I2C_CORE2X_VOTE	19200
#define I3C_CORE2X_VOTE	19200
#define SPI_CORE2X_VOTE	100000
#define UART_CORE2X_VOTE	100000
#define UART_CONSOLE_CORE2X_VOTE	19200

#if IS_ENABLED(CONFIG_MSM_GENI_SE)
/**
 * geni_read_reg_nolog() - Helper function to read from a GENI register
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 *
 * Return:	Return the contents of the register.
 */
unsigned int geni_read_reg_nolog(void __iomem *base, int offset);

/**
 * geni_write_reg_nolog() - Helper function to write into a GENI register
 * @value:	Value to be written into the register.
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 */
void geni_write_reg_nolog(unsigned int value, void __iomem *base, int offset);

/**
 * geni_read_reg() - Helper function to read from a GENI register
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 *
 * Return:	Return the contents of the register.
 */
unsigned int geni_read_reg(void __iomem *base, int offset);

/**
 * geni_write_reg() - Helper function to write into a GENI register
 * @value:	Value to be written into the register.
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 */
void geni_write_reg(unsigned int value, void __iomem *base, int offset);

/**
 * get_se_proto() - Read the protocol configured for a serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Protocol value as configured in the serial engine.
 */
int get_se_proto(void __iomem *base);

/**
 * get_se_m_fw() - Read the Firmware ver for the Main sequencer engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Firmware version for the Main sequencer engine
 */
int get_se_m_fw(void __iomem *base);

/**
 * get_se_s_fw() - Read the Firmware ver for the Secondry sequencer engine
 * @base:	Base address of the serial engine's register block.
 *
 * Return:	Firmware version for the Secondry sequencer engine
 */
int get_se_s_fw(void __iomem *base);

/**
 * geni_se_init() - Initialize the GENI Serial Engine
 * @base:	Base address of the serial engine's register block.
 * @rx_wm:	Receive watermark to be configured.
 * @rx_rfr_wm:	Ready-for-receive watermark to be configured.
 *
 * This function is used to initialize the GENI serial engine, configure
 * the transfer mode, receive watermark and ready-for-receive watermarks.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_init(void __iomem *base, unsigned int rx_wm, unsigned int rx_rfr);

/**
 * geni_se_select_mode() - Select the serial engine transfer mode
 * @base:	Base address of the serial engine's register block.
 * @mode:	Transfer mode to be selected.
 *
 * Return:	0 on success, standard Linux error codes on failure.
 */
int geni_se_select_mode(void __iomem *base, int mode);

/**
 * geni_setup_m_cmd() - Setup the primary sequencer
 * @base:	Base address of the serial engine's register block.
 * @cmd:	Command/Operation to setup in the primary sequencer.
 * @params:	Parameter for the sequencer command.
 *
 * This function is used to configure the primary sequencer with the
 * command and its assoicated parameters.
 */
void geni_setup_m_cmd(void __iomem *base, u32 cmd, u32 params);

/**
 * geni_setup_s_cmd() - Setup the secondary sequencer
 * @base:	Base address of the serial engine's register block.
 * @cmd:	Command/Operation to setup in the secondary sequencer.
 * @params:	Parameter for the sequencer command.
 *
 * This function is used to configure the secondary sequencer with the
 * command and its assoicated parameters.
 */
void geni_setup_s_cmd(void __iomem *base, u32 cmd, u32 params);

/**
 * geni_cancel_m_cmd() - Cancel the command configured in the primary sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to cancel the currently configured command in the
 * primary sequencer.
 */
void geni_cancel_m_cmd(void __iomem *base);

/**
 * geni_cancel_s_cmd() - Cancel the command configured in the secondary
 *			 sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to cancel the currently configured command in the
 * secondary sequencer.
 */
void geni_cancel_s_cmd(void __iomem *base);

/**
 * geni_abort_m_cmd() - Abort the command configured in the primary sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to force abort the currently configured command in the
 * primary sequencer.
 */
void geni_abort_m_cmd(void __iomem *base);

/**
 * geni_abort_s_cmd() - Abort the command configured in the secondary
 *			 sequencer
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to force abort the currently configured command in the
 * secondary sequencer.
 */
void geni_abort_s_cmd(void __iomem *base);

/**
 * get_tx_fifo_depth() - Get the TX fifo depth of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the depth i.e. number of elements in the
 * TX fifo of the serial engine.
 *
 * Return:	TX fifo depth in units of FIFO words.
 */
int get_tx_fifo_depth(void __iomem *base);

/**
 * get_tx_fifo_width() - Get the TX fifo width of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the width i.e. word size per element in the
 * TX fifo of the serial engine.
 *
 * Return:	TX fifo width in bits.
 */
int get_tx_fifo_width(void __iomem *base);

/**
 * get_rx_fifo_depth() - Get the RX fifo depth of the serial engine
 * @base:	Base address of the serial engine's register block.
 *
 * This function is used to get the depth i.e. number of elements in the
 * RX fifo of the serial engine.
 *
 * Return:	RX fifo depth in units of FIFO words.
 */
int get_rx_fifo_depth(void __iomem *base);

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
			   unsigned long *cfg0, unsigned long *cfg1);

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
void se_config_packing(void __iomem *base, int bpw, int pack_words,
		       bool msb_to_lsb);

/**
 * se_geni_clks_off() - Turn off clocks associated with the serial
 *                      engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_clks_off(struct se_geni_rsc *rsc);

/**
 * se_geni_clks_on() - Turn on clocks associated with the serial
 *                     engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_clks_on(struct se_geni_rsc *rsc);

/**
 * se_geni_resources_off() - Turn off resources associated with the serial
 *                           engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_resources_off(struct se_geni_rsc *rsc);

/**
 * se_geni_resources_on() - Turn on resources associated with the serial
 *                           engine
 * @rsc:	Handle to resources associated with the serial engine.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int se_geni_resources_on(struct se_geni_rsc *rsc);

/**
 * geni_se_resources_init() - Init the SE resource structure
 * @rsc:	SE resource structure to be initialized.
 * @ab:		Initial Average bus bandwidth request value.
 * @ib:		Initial Instantaneous bus bandwidth request value.
 *
 * Return:	0 on success, standard Linux error codes on failure.
 */
int geni_se_resources_init(struct se_geni_rsc *rsc,
			   unsigned long ab, unsigned long ib);

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
int geni_se_clk_tbl_get(struct se_geni_rsc *rsc, unsigned long **tbl);

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
			   bool exact);

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
			void *tx_buf, int tx_len, dma_addr_t *tx_dma);

/**
 * geni_se_rx_dma_start() - Prepare the Serial Engine registers for RX DMA
				transfers.
 * @base:		Base address of the SE register block.
 * @rx_len:		Length of the RX buffer.
 * @rx_dma:		Pointer to store the mapped DMA address.
 *
 * This function is used to prepare the Serial Engine registers for DMA RX.
 *
 * Return:	None.
 */
void geni_se_rx_dma_start(void __iomem *base, int rx_len, dma_addr_t *rx_dma);

/**
 * geni_se_rx_dma_prep() - Prepare the Serial Engine for RX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the TX buffer is mapped.
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
			void *rx_buf, int rx_len, dma_addr_t *rx_dma);

/**
 * geni_se_tx_dma_unprep() - Unprepare the Serial Engine after TX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the TX buffer is mapped.
 * @tx_dma:		DMA address of the TX buffer.
 * @tx_len:		Length of the TX buffer.
 *
 * This function is used to unprepare the DMA buffers after DMA TX.
 */
void geni_se_tx_dma_unprep(struct device *wrapper_dev,
			   dma_addr_t tx_dma, int tx_len);

/**
 * geni_se_rx_dma_unprep() - Unprepare the Serial Engine after RX DMA transfer
 * @wrapper_dev:	QUPv3 Wrapper Device to which the TX buffer is mapped.
 * @rx_dma:		DMA address of the RX buffer.
 * @rx_len:		Length of the RX buffer.
 *
 * This function is used to unprepare the DMA buffers after DMA RX.
 */
void geni_se_rx_dma_unprep(struct device *wrapper_dev,
			   dma_addr_t rx_dma, int rx_len);

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
			     unsigned int *minor, unsigned int *step);

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
			  void *buf, size_t size, enum dma_data_direction dir);

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
			      size_t size);

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
			    size_t size, enum dma_data_direction dir);

/**
 * geni_se_iommu_free_buf() - Unmap & free a single buffer from QUPv3
 *			      context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:	Pointer in which the mapped virtual address is stored.
 * @buf:	Address of the buffer.
 * @size:	Size of the buffer.
 *
 * This function is used to unmap and free a buffer from the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
int geni_se_iommu_free_buf(struct device *wrapper_dev, dma_addr_t *iova,
			   void *buf, size_t size);


/**
 * geni_se_dump_dbg_regs() - Print relevant registers that capture most
 *			accurately the state of an SE; meant to be called
 *			in case of errors to help debug.
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
				void *ipc);

/*
 * This function is used to remove proxy ICC BW vote put from common driver
 * probe on behalf of earlycon usecase.
 */
void geni_se_remove_earlycon_icc_vote(struct device *dev);
#else
static inline unsigned int geni_read_reg_nolog(void __iomem *base, int offset)
{
	return 0;
}

static inline void geni_write_reg_nolog(unsigned int value,
					void __iomem *base, int offset)
{
}

static inline unsigned int geni_read_reg(void __iomem *base, int offset)
{
	return 0;
}

static inline void geni_write_reg(unsigned int value, void __iomem *base,
				int offset)
{
}

static inline int get_se_proto(void __iomem *base)
{
	return -ENXIO;
}

static inline int geni_se_init(void __iomem *base,
		unsigned int rx_wm, unsigned int rx_rfr)
{
	return -ENXIO;
}

static inline int geni_se_select_mode(void __iomem *base, int mode)
{
	return -ENXIO;
}

static inline void geni_setup_m_cmd(void __iomem *base, u32 cmd,
								u32 params)
{
}

static inline void geni_setup_s_cmd(void __iomem *base, u32 cmd,
								u32 params)
{
}

static inline void geni_cancel_m_cmd(void __iomem *base)
{
}

static inline void geni_cancel_s_cmd(void __iomem *base)
{
}

static inline void geni_abort_m_cmd(void __iomem *base)
{
}

static inline void geni_abort_s_cmd(void __iomem *base)
{
}

static inline int get_tx_fifo_depth(void __iomem *base)
{
	return -ENXIO;
}

static inline int get_tx_fifo_width(void __iomem *base)
{
	return -ENXIO;
}

static inline int get_rx_fifo_depth(void __iomem *base)
{
	return -ENXIO;
}

static inline void se_get_packing_config(int bpw, int pack_words,
					bool msb_to_lsb, unsigned long *cfg0,
					unsigned long *cfg1)
{
}

static inline void se_config_packing(void __iomem *base, int bpw,
				int pack_words, bool msb_to_lsb)
{
}

static inline int se_geni_clks_on(struct se_geni_rsc *rsc)
{
	return -ENXIO;
}

static inline int se_geni_clks_off(struct se_geni_rsc *rsc)
{
	return -ENXIO;
}

static inline int se_geni_resources_on(struct se_geni_rsc *rsc)
{
	return -ENXIO;
}

static inline int se_geni_resources_off(struct se_geni_rsc *rsc)
{
	return -ENXIO;
}

static inline int geni_se_resources_init(struct se_geni_rsc *rsc,
					 unsigned long ab, unsigned long ib)
{
	return -ENXIO;
}

static inline int geni_se_clk_tbl_get(struct se_geni_rsc *rsc,
					unsigned long **tbl)
{
	return -ENXIO;
}

static inline int geni_se_clk_freq_match(struct se_geni_rsc *rsc,
			unsigned long req_freq, unsigned int *index,
			unsigned long *res_freq, bool exact)
{
	return -ENXIO;
}

static inline int geni_se_tx_dma_prep(struct device *wrapper_dev,
	void __iomem *base, void *tx_buf, int tx_len, dma_addr_t *tx_dma)
{
	return -ENXIO;
}

static inline int geni_se_rx_dma_prep(struct device *wrapper_dev,
	void __iomem *base, void *rx_buf, int rx_len, dma_addr_t *rx_dma)
{
	return -ENXIO;
}

static inline void geni_se_tx_dma_unprep(struct device *wrapper_dev,
					dma_addr_t tx_dma, int tx_len)
{
}

static inline void geni_se_rx_dma_unprep(struct device *wrapper_dev,
					dma_addr_t rx_dma, int rx_len)
{
}

static inline int geni_se_qupv3_hw_version(struct device *wrapper_dev,
		unsigned int *major, unsigned int *minor, unsigned int *step)
{
	return -ENXIO;
}

static inline int geni_se_iommu_map_buf(struct device *wrapper_dev,
	dma_addr_t *iova, void *buf, size_t size, enum dma_data_direction dir)
{
	return -ENXIO;
}

static inline void *geni_se_iommu_alloc_buf(struct device *wrapper_dev,
					dma_addr_t *iova, size_t size)
{
	return NULL;
}

static inline int geni_se_iommu_unmap_buf(struct device *wrapper_dev,
		dma_addr_t *iova, size_t size, enum dma_data_direction dir)
{
	return -ENXIO;

}

static inline int geni_se_iommu_free_buf(struct device *wrapper_dev,
				dma_addr_t *iova, void *buf, size_t size)
{
	return -ENXIO;
}

static void geni_se_dump_dbg_regs(struct se_geni_rsc *rsc, void __iomem *base,
				void *ipc)
{
}

static void geni_se_rx_dma_start(void __iomem *base, int rx_len,
						dma_addr_t *rx_dma)
{
}

#endif
#endif
