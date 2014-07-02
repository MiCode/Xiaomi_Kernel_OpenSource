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
 *
 */
/*
 * I2C controller driver for Qualcomm MSM platforms
 */

#ifndef _I2C_MSM_V2_H
#define _I2C_MSM_V2_H

#include <linux/bitops.h>

enum msm_i2_debug_level {
	MSM_ERR,	/* Error messages only. Always on */
	MSM_PROF,	/* High level events. Use for profiling */
	MSM_DBG,	/* Low level details. Use for debugging */
};

#define i2c_msm_dbg(ctrl, dbg_level, fmt, ...) do {\
		if (ctrl->dbgfs.dbg_lvl >= dbg_level)\
			dev_info(ctrl->dev, pr_fmt(fmt), ##__VA_ARGS__);\
	} while (0)

#define BIT_IS_SET(val, idx)        ((val >> idx) & 0x1)
#define BITS_AT(val, idx, n_bits)(((val) & (((1 << n_bits) - 1) << idx)) >> idx)
#define MASK_IS_SET(val, mask)      ((val & mask) == mask)
#define MASK_IS_SET_BOOL(val, mask) (MASK_IS_SET(val, mask) ? 1 : 0)

/* QUP Registers */
enum {
	QUP_CONFIG              = 0x0,
	QUP_STATE               = 0x4,
	QUP_IO_MODES            = 0x8,
	QUP_SW_RESET            = 0xC,
	QUP_OPERATIONAL         = 0x18,
	QUP_ERROR_FLAGS         = 0x1C,
	QUP_ERROR_FLAGS_EN      = 0x20,
	QUP_TEST_CTRL           = 0x24,
	QUP_OPERATIONAL_MASK    = 0x28,
	QUP_HW_VERSION          = 0x30,
	QUP_MX_READ_COUNT       = 0x208,
	QUP_MX_WRITE_COUNT      = 0x150,
	QUP_MX_OUTPUT_COUNT     = 0x100,
	QUP_MX_INPUT_COUNT      = 0x200,
	QUP_MX_WR_CNT           = 0x100,
	QUP_OUT_DEBUG           = 0x108,
	QUP_OUT_FIFO_CNT        = 0x10C,
	QUP_OUT_FIFO_BASE       = 0x110,
	QUP_IN_READ_CUR         = 0x20C,
	QUP_IN_DEBUG            = 0x210,
	QUP_IN_FIFO_CNT         = 0x214,
	QUP_IN_FIFO_BASE        = 0x218,
	QUP_I2C_MASTER_CLK_CTL  = 0x400,
	QUP_I2C_STATUS          = 0x404,
	QUP_I2C_MASTER_CONFIG   = 0x408,
	QUP_I2C_MASTER_BUS_CLR  = 0x40C,
};

/* Register:QUP_STATE state field values */
enum i2c_msm_qup_state {
	QUP_STATE_RESET         = 0,
	QUP_STATE_RUN           = 1U,
	QUP_STATE_PAUSE         = 3U,
};

/* Register:QUP_STATE fields */
enum {
	QUP_STATE_MASK          = 3U,
	QUP_STATE_VALID         = 1U << 2,
	QUP_I2C_MAST_GEN        = 1U << 4,
	QUP_I2C_FLUSH           = 1U << 6,
	QUP_I2C_STATUS_RESET    = 0x42,
};


/* Register:QUP_CONFIG fields */
enum {
	QUP_MINI_CORE_MASK      = 0xF00,
	QUP_MINI_CORE_I2C_VAL   = 0x200,
	QUP_N_MASK              = 0x1F,
	QUP_N_VAL               = 0x7, /* 0xF for A family */
	QUP_NO_OUPUT            = 1U << 6,
	QUP_NO_INPUT            = 1U << 7,
	QUP_APP_CLK_ON_EN       = 1U << 12,
	QUP_CORE_CLK_ON_EN      = 1U << 13,
	QUP_FIFO_CLK_GATE_EN    = 1U << 14,
};

/* Register:QUP_OPERATIONAL fields */
enum {
	QUP_INPUT_FIFO_NOT_EMPTY = 1U << 5,
	QUP_OUTPUT_SERVICE_FLAG  = 1U << 8,
	QUP_INPUT_SERVICE_FLAG   = 1U << 9,
	QUP_MAX_OUTPUT_DONE_FLAG = 1U << 10,
	QUP_MAX_INPUT_DONE_FLAG  = 1U << 11,
	QUP_OUT_BLOCK_WRITE_REQ  = BIT(12),
	QUP_IN_BLOCK_READ_REQ    = BIT(13),
};

/* Register:QUP_OPERATIONAL_MASK fields */
enum {
	QUP_INPUT_SERVICE_MASK  = 1U << 9,
	QUP_OUTPUT_SERVICE_MASK = 1U << 8,
};

/* Register:QUP_IO_MODES fields */
enum {
	QUP_OUTPUT_MODE         = 3U << 10,
	QUP_INPUT_MODE          = 3U << 12,
	QUP_UNPACK_EN           = 1U << 14,
	QUP_PACK_EN             = 1U << 15,
	QUP_OUTPUT_BIT_SHIFT_EN = 1U << 16,
};

/* Register:QUP_I2C_STATUS (a.k.a I2C_MASTER_STATUS) fields */
enum {
	QUP_BUS_ERROR           = 1U << 2,
	QUP_PACKET_NACKED       = 1U << 3,
	QUP_ARB_LOST            = 1U << 4,
	QUP_BUS_ACTIVE          = 1U << 8,
	QUP_BUS_MASTER          = 1U << 9,
	QUP_INVALID_TAG         = 1U << 23,
	QUP_INVALID_READ_ADDR   = 1U << 24,
	QUP_INVALID_READ_SEQ    = 1U << 25,
	QUP_I2C_SDA             = 1U << 26,
	QUP_I2C_SCL             = 1U << 27,
	QUP_MSTR_STTS_ERR_MASK  = 0x380003C,
};

/* Register:QUP_I2C_MASTER_CONFIG fields */
enum {
	QUP_EN_VERSION_TWO_TAG  = 1U,
};

enum {
	I2C_MSM_CLK_FAST_FREQ_HS     =  400000,
	I2C_MSM_CLK_FAST_MAX_FREQ    = 1000000,
	I2C_MSM_CLK_HIGH_MAX_FREQ    = 3400000,
};

/* Register:QUP_I2C_MASTER_CLK_CTL field setters */
#define I2C_MSM_SCL_NOISE_REJECTION(reg_val, noise_rej_val) \
		(((reg_val) & ~(0x3 << 24)) | (((noise_rej_val) & 0x3) << 24))
#define I2C_MSM_SDA_NOISE_REJECTION(reg_val, noise_rej_val) \
		(((reg_val) & ~(0x3 << 26)) | (((noise_rej_val) & 0x3) << 26))
static inline u32 I2C_MSM_CLK_DIV(u32 reg_val, u32 clk_freq_in,
				u32 clk_freq_out, bool is_high_speed)
{
	int fs_div;
	int hs_div;

	if (is_high_speed) {
		fs_div = I2C_MSM_CLK_FAST_FREQ_HS;
		hs_div = (clk_freq_in / (clk_freq_out * 3));
	} else {
		fs_div = (clk_freq_in / (clk_freq_out * 2)) - 3;
		hs_div = 0;
	}
	/* Protect hs_div from overflow (it is represented in HW by 3 bits */
	hs_div = min_t(int, hs_div, 0x7);

	return (reg_val & (~0x7ff)) | ((hs_div & 0x7) << 8) | (fs_div & 0xff);
}

/* Register:QUP_ERROR_FLAGS_EN flags */
enum {
	QUP_OUTPUT_OVER_RUN_ERR_EN  = 1U << 5,
	QUP_INPUT_UNDER_RUN_ERR_EN  = 1U << 4,
	QUP_OUTPUT_UNDER_RUN_ERR_EN = 1U << 3,
	QUP_INPUT_OVER_RUN_ERR_EN   = 1U << 2,
};

/* Status, Error flags */
enum {
	I2C_STATUS_WR_BUFFER_FULL  = 1U << 0,
	I2C_STATUS_BUS_ACTIVE      = 1U << 8,
	I2C_STATUS_BUS_MASTER      = 1U << 9,
	I2C_STATUS_ERROR_MASK      = 0x38000FC,
	QUP_I2C_NACK_FLAG          = 1U << 3,
	QUP_IN_NOT_EMPTY           = 1U << 5,
	QUP_ERR_FLGS_MASK           = 0x3C,
};

/* Master status clock states */
enum {
	I2C_CLK_RESET_BUSIDLE_STATE = 0,
	I2C_CLK_FORCED_LOW_STATE    = 5,
};

/* Controller's hardware versions */
enum i2c_msm_ctrl_ver_num {
	/* Values local to this driver */
	I2C_MSM_CTRL_VER_UNKNOWN = 0,
	I2C_MSM_CTRL_VER_A,
	I2C_MSM_CTRL_VER_B,
	/* Values to compare against HW registers */
	I2C_MSM_CTRL_VER_B_MIN   = 0X20010000,
	I2C_MSM_CTRL_VER_B_V1    = 0X20010001,
	I2C_MSM_CTRL_VER_B_V2    = 0X20020000,
	I2C_MSM_CTRL_VER_B_MAX   = 0X30000000,
};

/* Controller's power state */
enum msm_i2c_power_state {
	MSM_I2C_PM_ACTIVE,
	MSM_I2C_PM_SUSPENDED,
	MSM_I2C_PM_SYS_SUSPENDED
};

/*
 * The max buffer size required for tags is for holding the following sequence:
 * [start | hs-addr] + [start | slv-addr] + [ rd/wr | len]
 * which sum up to 6 bytes. However, we use u64 to hold the value, thus we say
 * that max length is 8 bytes.
 */
#define I2C_MSM_TAG2_MAX_LEN            (8)
#define I2C_MSM_BAM_CONS_SZ             (64) /* consumer pipe n entries */
#define I2C_MSM_BAM_PROD_SZ             (32) /* producer pipe n entries */
#define I2C_MSM_BAM_DESC_ARR_SIZ  (I2C_MSM_BAM_CONS_SZ + I2C_MSM_BAM_PROD_SZ)
#define I2C_MSM_REG_2_STR_BUF_SZ        (128)
#define I2C_MSM_BUF_DUMP_MAX_BC         (20)
#define I2C_MSM_MAX_POLL_MSEC           (100)
#define I2C_MSM_TIMEOUT_SAFTY_COEF      (10)
#define I2C_MSM_TIMEOUT_MIN_USEC        (500000)
#define I2C_MSM_HS_ADDR                 (0x0f)

/* QUP v2 tags */
#define QUP_TAG2_DATA_WRITE        (0x82ULL)
#define QUP_TAG2_DATA_WRITE_N_STOP (0x83ULL)
#define QUP_TAG2_DATA_READ         (0x85ULL)
#define QUP_TAG2_DATA_READ_N_STOP  (0x87ULL)
#define QUP_TAG2_START             (0x81ULL)
#define QUP_TAG2_DATA_READ_N_NACK  (0x86ULL)
#define QUP_TAG2_START_STOP        (0x8AULL)
#define QUP_TAG2_INPUT_EOT         (0x93ULL)
#define QUP_TAG2_FLUSH_STOP        (0x96ULL)
/* Aggregate the constatnt values of HS start sequence */
#define QUP_TAG2_START_HS  (QUP_TAG2_START | (I2C_MSM_HS_ADDR << 8) | \
			   (QUP_TAG2_START << 16))

enum msm_spi_clk_path_vec_idx {
	I2C_MSM_CLK_PATH_SUSPEND_VEC,
	I2C_MSM_CLK_PATH_RESUME_VEC,
};
#define I2C_MSM_CLK_PATH_AVRG_BW(ctrl) (0)
#define I2C_MSM_CLK_PATH_BRST_BW(ctrl) (ctrl->rsrcs.clk_freq_in * 8)

static char const * const i2c_msm_gpio_names[] = {"i2c_clk", "i2c_sda"};

enum i2c_msm_gpio_name_idx {
	I2C_MSM_GPIO_SCL,
	I2C_MSM_GPIO_SDA,
};

struct i2c_msm_ctrl;

/*
 * i2c_msm_xfer_mode: transfer modes such as FIFO and BAM define these callbacks
 */
struct i2c_msm_xfer_mode {
	void                     (*teardown)(struct i2c_msm_ctrl *);
	int                      (*xfer)    (struct i2c_msm_ctrl *);
};

/*
 *  i2c_msm_dma_mem: utility struct which holds both physical and virtual addr
 */
struct i2c_msm_dma_mem {
	dma_addr_t               phy_addr;
	void                    *vrtl_addr;
};

/*
 * i2c_msm_tag: tag's data and its length.
 *
 * @len tag len can be two, four or six bytes.
 */
struct i2c_msm_tag {
	u64                    val;
	int                    len;
};

/*
 * i2c_msm_bam_tag: similar to struct i2c_msm_tag but holds physical address.
 *
 * @buf physical address of entry in the tag_arr of
 *          struct i2c_msm_xfer_mode_bam
 * @len tag len.
 *
 * Hold the information from i2c_msm_bam_xfer_prepare() which is used by
 * i2c_msm_bam_xfer_process() and freed by i2c_msm_bam_xfer_unprepare()
 */
struct i2c_msm_bam_tag {
	dma_addr_t             buf;
	size_t                 len;
};

/*
 * i2c_msm_bam_buf: dma mapped pointer to i2c_msg data buffer and related tag
 * @vir_addr ptr to i2c_msg buf beginning or with offset (when buf len > 256)
 */
struct i2c_msm_bam_buf {
	struct i2c_msm_dma_mem   ptr;
	enum dma_data_direction  dma_dir;
	size_t                   len;
	bool                     is_rx;
	bool                     is_last;
	struct i2c_msm_bam_tag   tag;
};

/*
 * i2c_msm_bam_pipe: per pipe info
 *
 * @is_init true when the pipe is initialized and requires eventual teardown.
 * @name pipe name (consumer/producer) for debugging.
 * @desc_cnt_max size of descriptors space
 * @desc_cnt_cur number of occupied descriptors
 */
struct i2c_msm_bam_pipe {
	bool                     is_init;
	struct sps_pipe         *handle;
	struct sps_connect       config;
	const char              *name;
	size_t                   desc_cnt_max;
	size_t                   desc_cnt_cur;
};

enum i2c_msm_bam_pipe_dir {
	I2C_MSM_BAM_CONS = 0,
	I2C_MSM_BAM_PROD = 1,
};

static const char * const i2c_msm_bam_pipe_name[] = {"consumer", "producer"};

/*
 * struct i2c_msm_xfer_mode_bam: bam mode configuration and work space
 *
 * @is_init true when BAM and its pipes are fully initialized.
 * @is_core_init true when BAM core is initialised.
 * @ops     "base class" of i2c_msm_xfer_mode_bam. Contains the operations while
 *          the rest of the fields contain the data.
 * @deregister_required deregister is required when this driver has registerd
 *          the BAM device. When another kernel module has registered BAM
 *          prior to this driver, then deregister is not required.
 * @buf_arr_cnt current number of vaid buffers in buf_arr. The valid buffers
 *          are at index 0..buf_arr_cnt excluding buf_arr_cnt.
 * @buf_arr array of descriptors which point to the user's buffer
 *     virtual and physical address, and hold meta data about the buffer
 *     and respective tag.
 * @tag_arr array of tags in DMAable memory. Holds a tag per buffer of the same
 *          index, that is tag_arr[i] is related to buf_arr[i]. Also, tag_arr[i]
 *          is queued in the consumer pipe just befor buf_arr[i] is queued in
 *          the consumer (output buf) or producer pipe (input buffer).
 * @eot_n_flush_stop_tags EOT and flush-stop tags to be queued to the consumer
 *          bam pipe after the last transfer when it is a read.
 * @input_tag hw is placing input tags in the producer pipe on read operations.
 *          The value of these tags is "don't care" from bam transfer
 *          perspective. Thus, this single buffer is used for all the input
 *          tags. The field is used as write only.
 * @mem pointer to platform data describing the BAM's register space.
 */
struct i2c_msm_xfer_mode_bam {
	struct i2c_msm_xfer_mode ops;
	bool                     is_init;
	bool                     is_core_init;
	bool                     deregister_required;

	size_t                   buf_arr_cnt;
	struct i2c_msm_bam_buf   buf_arr[I2C_MSM_BAM_DESC_ARR_SIZ];
	struct i2c_msm_dma_mem   tag_arr;
	struct i2c_msm_dma_mem   eot_n_flush_stop_tags;
	struct i2c_msm_dma_mem   input_tag;

	struct resource         *mem;
	void __iomem            *base;
	ulong                    handle;
	u32                      irq;
	struct i2c_msm_bam_pipe  pipe[2];
};

/*
 * I2C_MSM_BAM_TAG_MEM_SZ includes the following fields of
 * struct i2c_msm_xfer_mode_bam (in order):
 *
 * Buffer of DMA memory:
 * +-----------+---------+-----------+-----------+----+-----------+
 * | input_tag | eot_... | tag_arr 0 | tag_arr 1 | .. | tag_arr n |
 * +-----------+---------+-----------+-----------+----+-----------+
 *
 * I2C_MSM_TAG2_MAX_LEN bytes for input_tag
 * I2C_MSM_TAG2_MAX_LEN bytes for eot_n_flush_stop_tags
 * I2C_MSM_BAM_DESC_ARR_SIZ * I2C_MSM_TAG2_MAX_LEN bytes for tag_arr
 */
#define I2C_MSM_BAM_TAG_MEM_SZ  \
	((I2C_MSM_BAM_DESC_ARR_SIZ + 2) * I2C_MSM_TAG2_MAX_LEN)

/*
 * i2c_msm_xfer_mode_fifo: operations and state of FIFO mode
 *
 * @ops     "base class" of i2c_msm_xfer_mode_bam. Contains the operations while
 *          the rest of the fields contain the data.
 * @input_fifo_sz input fifo size in bytes
 * @output_fifo_sz output fifo size in bytes
 * @tx_bc   keeps track of number of bytes written to fifo per client request
 * @rx_bc   keeps track of number of bytes read from fifo per client request
 * @in_rem  remaining u32 entries in input FIFO before empty
 * @out_rem remaining u32 entries in output FIFO before full
 * @out_buf buffer for collecting bytes to four bytes groups (u32) before
 *          writing them to the output fifo.
 * @out_buf_idx next free index in out_buf. 0..3
 */
struct i2c_msm_xfer_mode_fifo {
	struct i2c_msm_xfer_mode ops;
	size_t                   input_fifo_sz;
	size_t                   output_fifo_sz;
	size_t                   tx_bc;
	size_t                   rx_bc;
	size_t                   in_rem;
	size_t                   out_rem;
	u8                       out_buf[4];
	int                      out_buf_idx;
};

/* i2c_msm_xfer_mode_blk: operations and state of Block mode
 *
 * @in_blk_sz size of input/rx block
 * @out_blk_sz size of output/tx block
 * @tx_cache internal buffer to store tx data
 * @rx_cache internal buffer to store rx data
 * @rx_cache_idx points to the next unread index in rx cache
 * @tx_cache_idx points to the next unwritten index in tx cache
 * @wait_rx_blk completion object to wait on for end of blk rx transfer.
 * @wait_tx_blk completion object to wait on for end of blk tx transfer.
 * @complete_mask applied to QUP_OPERATIONAL to determine when blk
 *  xfer is complete.
 */
struct i2c_msm_xfer_mode_blk {
	struct i2c_msm_xfer_mode ops;
	size_t                   in_blk_sz;
	size_t                   out_blk_sz;
	u8                       *tx_cache;
	u8                       *rx_cache;
	int                      rx_cache_idx;
	int                      tx_cache_idx;
	struct completion        wait_rx_blk;
	struct completion        wait_tx_blk;
	u32                      complete_mask;
};

/* INPUT_MODE and OUTPUT_MODE filds of QUP_IO_MODES register */
enum i2c_msm_xfer_mode_id {
	I2C_MSM_XFER_MODE_FIFO,
	I2C_MSM_XFER_MODE_BLOCK,
	I2C_MSM_XFER_MODE_BAM,
	I2C_MSM_XFER_MODE_NONE, /* keep last as a counter */
};

/*
 * i2c_msm_ctrl_ver: info that is different between i2c controller versions
 *
 * @destroy  Called once on exit.  Deallocate transfer modes
 * @init     Initialises the controller.
 * @teardown Teardown the controller and the transfer modes.
 * @reset    Reset the controller (SW reset)
 * @choose_mode    Chooses a transfer mode of the xfer_mode[].
 * @post_xfer      Steps to do after data transfer is done. It updates the error
 *                 value if needed, and waits until the HW is truly done.
 * @max_rx_cnt  Max bytes per transfer.
 * @max_tx_cnt Max bytes per transfer.
 * @max_buf_size   Number of bytes max between tags.
 * @msg_ovrhd_bc   Message overhead byte cnt = 4.
 * @buf_ovrhd_bc   Buffer  overhead byte cnt = 2.
 * @xfer_mode      Array of available transfer modes. struct i2c_msm_xfer_mode
 *                 is a "base class" to the particular transfer mode.
 */
struct i2c_msm_ctrl_ver {
	void			  (*destroy)    (struct i2c_msm_ctrl *);
	int			  (*init)       (struct i2c_msm_ctrl *);
	void			  (*teardown)   (struct i2c_msm_ctrl *);
	int			  (*reset)      (struct i2c_msm_ctrl *);
	int			  (*init_rsrcs) (struct platform_device *,
						 struct i2c_msm_ctrl *);
	enum i2c_msm_xfer_mode_id (*choose_mode)(struct i2c_msm_ctrl *);
	int			  (*post_xfer)  (struct i2c_msm_ctrl *,
								int err);

	int			  max_rx_cnt;
	int			  max_tx_cnt;
	int			  max_buf_size;
	int			  msg_ovrhd_bc;
	int			  buf_ovrhd_bc;

	struct i2c_msm_xfer_mode *xfer_mode[I2C_MSM_XFER_MODE_NONE];
};

struct i2c_msm_dbgfs {
	struct dentry             *root;
	enum msm_i2_debug_level    dbg_lvl;
	enum i2c_msm_xfer_mode_id  force_xfer_mode;
};

/*
 * qup_i2c_clk_path_vote: data to use bus scaling driver for clock path vote
 *
 * @mstr_id master id number of the i2c core or its wrapper (BLSP/GSBI).
 *       When zero, clock path voting is disabled.
 * @client_hdl when zero, client is not registered with the bus scaling driver,
 *      and bus scaling functionality should not be used. When non zero, it
 *      is a bus scaling client id and may be used to vote for clock path.
 * @reg_err when true, registration error was detected and an error message was
 *      logged. i2c will attempt to re-register but will log error only once.
 *      once registration succeed, the flag is set to false.
 * @actv_only when set, votes when system active and removes the vote when
 *       system goes idle (optimises for performance). When unset, voting using
 *       runtime pm (optimizes for power).
 */
struct qup_i2c_clk_path_vote {
	u32                         mstr_id;
	u32                         client_hdl;
	struct msm_bus_scale_pdata *pdata;
	bool                        reg_err;
	bool                        actv_only;
};

/*
 * i2c_msm_resources: OS resources
 *
 * @mem  I2C controller memory resource from platform data.
 * @base I2C controller virtual base address
 * @clk_freq_in core clock frequency in Hz
 * @clk_freq_out bus clock frequency in Hz
 * @bam_pipe_idx_cons index of BAM's consumer pipe
 * @bam_pipe_idx_prod index of BAM's producer pipe
 */
struct i2c_msm_resources {
	struct resource             *mem;
	void __iomem                *base; /* virtual */
	struct clk                  *core_clk;
	struct clk                  *iface_clk;
	int                          clk_freq_in;
	int                          clk_freq_out;
	struct qup_i2c_clk_path_vote clk_path_vote;
	int                          irq;
	bool                         disable_dma;
	u32                          bam_pipe_idx_cons;
	u32                          bam_pipe_idx_prod;
	struct pinctrl              *pinctrl;
	struct pinctrl_state        *gpio_state_active;
	struct pinctrl_state        *gpio_state_suspend;
};

#define I2C_MSM_PINCTRL_ACTIVE       "i2c_active"
#define I2C_MSM_PINCTRL_SUSPEND        "i2c_sleep"

/*
 * i2c_msm_xfer_buf: current xfer position and preprocessed tags
 *
 * @is_init the buf is marked initialized by the first call to
 *          i2c_msm_xfer_next_buf()
 * @msg_idx   index of the message that the buffer is pointing to
 * @byte_idx  index of first byte in the current buffer
 * @prcsed_bc count of bytes processed from the current message. This value
 *            is compared against len to find out if buffer is done processing.
 * @len       number of bytes in current buffer.
 * @is_rx when true, current buffer is pointing to a i2c read operation.
 * @slv_addr 8 bit address. This is the i2c_msg->addr + rd/wr bit.
 *
 * Keep track of current position in the client's transfer request and
 * pre-process a transfer's buffer and tags.
 */
struct i2c_msm_xfer_buf {
	bool                       is_init;
	int                        msg_idx;
	size_t                     byte_idx;
	size_t                     prcsed_bc;
	size_t                     len;
	bool                       is_rx;
	bool                       is_last;
	u16                        slv_addr;
	struct i2c_msm_tag         in_tag;
	struct i2c_msm_tag         out_tag;
};

#ifdef DEBUG
#define I2C_MSM_PROF_MAX_EVNTS   (64)
#else
#define I2C_MSM_PROF_MAX_EVNTS   (16)
#endif

struct i2c_msm_prof_event;
typedef void (*i2c_msm_prof_dump_func_func_t)(struct i2c_msm_ctrl *,
			struct i2c_msm_prof_event *, size_t msec, size_t usec);
/*
 * i2c_msm_prof_event: profiling event
 *
 * @data Additional data about the event. The interpretation of the data is
 *       dependant on the type field.
 * @type event type (see enum i2c_msm_prof_event_type)
 */
struct i2c_msm_prof_event {
	i2c_msm_prof_dump_func_func_t dump_func;
	struct timespec time;
	u64             data0;
	u32             data1;
	u32             data2;
	u8              type;
};

enum i2c_msm_err_bit_field {
	I2C_MSM_ERR_NONE     = 0,
	I2C_MSM_ERR_NACK     = 1U << 0,
	I2C_MSM_ERR_ARB_LOST = 1U << 1,
	I2C_MSM_ERR_BUS_ERR  = 1U << 2,
	I2C_MSM_ERR_TIMEOUT  = 1U << 3,
};

/*
 * i2c_msm_xfer: A client transfer request. A list of one or more i2c messages
 *
 * @msgs         NULL when no active xfer. Points to array of i2c_msgs
 *               given by the client.
 * @msg_cnt      number of messages in msgs array.
 * @complete     completion object to wait on for end of transfer.
 * @rx_cnt       number of input  bytes in the client's request.
 * @tx_cnt       number of output bytes in the client's request.
 * @rx_ovrhd_cnt number of input  bytes due to tags.
 * @tx_ovrhd_cnt number of output bytes due to tags.
 * @event        profiling data. An array of timestamps of transfer events
 * @event_cnt    number of items in event array.
 * @is_active    true during xfer process and false after xfer end
 * @mtx          mutex to solve multithreaded problem in xfer
 */
struct i2c_msm_xfer {
	struct i2c_msg            *msgs;
	int                        msg_cnt;
	enum i2c_msm_xfer_mode_id  mode_id;
	struct completion          complete;
	size_t                     rx_cnt;
	size_t                     tx_cnt;
	size_t                     rx_ovrhd_cnt;
	size_t                     tx_ovrhd_cnt;
	struct i2c_msm_xfer_buf    cur_buf;
	u32                        timeout;
	bool                       last_is_rx;
	enum i2c_msm_err_bit_field err;
	struct i2c_msm_prof_event  event[I2C_MSM_PROF_MAX_EVNTS];
	atomic_t                   event_cnt;
	atomic_t                   is_active;
	struct mutex               mtx;
};

/*
 * i2c_msm_ctrl: the driver's main struct
 *
 * @is_init true when
 * @ver info that is different between i2c controller versions
 * @ver_num  ha
 * @xfer     state of the currently processed transfer.
 * @dbgfs    debug-fs root and values that may be set via debug-fs.
 * @rsrcs    resources from platform data including clocks, gpios, irqs, and
 *           memory regions.
 * @noise_rjct_scl noise rejection value for the scl line (a field of
 *           I2C_MASTER_CLK_CTL).
 * @noise_rjct_sda noise rejection value for the sda line (a field of
 *           I2C_MASTER_CLK_CTL).
 * @pdata    the platform data (values from board-file or from device-tree)
 */
struct i2c_msm_ctrl {
	struct device             *dev;
	struct i2c_adapter         adapter;
	struct i2c_msm_ctrl_ver    ver;
	struct i2c_msm_xfer        xfer;
	struct i2c_msm_dbgfs       dbgfs;
	struct i2c_msm_resources   rsrcs;
	int                        noise_rjct_scl;
	int                        noise_rjct_sda;
	struct i2c_msm_v2_platform_data *pdata;
	enum msm_i2c_power_state   pwr_state;
};

#endif  /* _I2C_MSM_V2_H */
