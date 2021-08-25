// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>        /* Just for modules */
#include <linux/kernel.h>        /* Only for KERN_INFO */
#include <linux/err.h>           /* Error macros */
#include <linux/list.h>          /* Linked list */
#include <linux/cdev.h>
#include <linux/init.h>          /* Needed for the macros */
#include <linux/io.h>            /* IO macros */
#include <linux/device.h>        /* Device drivers need this */
#include <linux/sched.h>         /* Externally defined globals */
#include <linux/pm_runtime.h>    /* Runtime power management */
#include <linux/fs.h>
#include <linux/uaccess.h>       /* copy_to_user */
#include <linux/slab.h>          /* kfree, kzalloc */
#include <linux/ioport.h>        /* XXX_ mem_region */
#include <asm/dma-iommu.h>
#include <linux/dma-mapping.h>   /* dma_XXX */
#include <linux/dmapool.h>       /* DMA pools */
#include <linux/delay.h>         /* msleep */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/poll.h>          /* poll() file op */
#include <linux/wait.h>          /* wait() macros, sleeping */
#include <linux/bitops.h>        /* BIT() macro */
#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/msm-sps.h>       /* BAM stuff */
#include <linux/timer.h>         /* Timer services */
#include <linux/jiffies.h>       /* Jiffies counter */
#include <linux/qcom_tspp.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <linux/msm-bus.h>
#include <linux/interrupt.h>    /* tasklet */
#include <asm/arch_timer.h>     /* Timer */
#include <linux/dma-buf.h>

/*
 * General defines
 */
#define TSPP_TSIF_INSTANCES            2
#define TSPP_GPIOS_PER_TSIF            4
#define TSPP_FILTER_TABLES             3
#define TSPP_MAX_DEVICES               1
#define TSPP_NUM_CHANNELS              16
#define TSPP_NUM_PRIORITIES            16
#define TSPP_NUM_KEYS                  8
#define INVALID_CHANNEL                0xFFFFFFFF
#define TSPP_BAM_DEFAULT_IPC_LOGLVL    2

#define TSPP_SMMU_IOVA_START	(0x10000000)
#define TSPP_SMMU_IOVA_SIZE	(0x40000000)

/*
 * BAM descriptor FIFO size (in number of descriptors).
 * Max number of descriptors allowed by SPS which is 8K-1.
 */
#define TSPP_SPS_DESCRIPTOR_COUNT      (8 * 1024 - 1)
#define TSPP_PACKET_LENGTH             188
#define TSPP_MIN_BUFFER_SIZE           (TSPP_PACKET_LENGTH)

/* Max descriptor buffer size allowed by SPS */
#define TSPP_MAX_BUFFER_SIZE           (32 * 1024 - 1)

/*
 * Returns whether to use DMA pool for TSPP output buffers.
 * For buffers smaller than page size, using DMA pool
 * provides better memory utilization as dma_alloc_coherent
 * allocates minimum of page size.
 */
#define TSPP_USE_DMA_POOL(buff_size)   ((buff_size) < PAGE_SIZE)

/*
 * Max allowed TSPP buffers/descriptors.
 * If SPS desc FIFO holds X descriptors, we can queue up to X-1 descriptors.
 */
#define TSPP_NUM_BUFFERS               (TSPP_SPS_DESCRIPTOR_COUNT - 1)
#define TSPP_TSIF_DEFAULT_TIME_LIMIT   60
#define SPS_DESCRIPTOR_SIZE            8
#define MIN_ACCEPTABLE_BUFFER_COUNT    2
#define TSPP_DEBUG(msg...)

/*
 * TSIF register offsets
 */
#define TSIF_STS_CTL_OFF               (0x0)
#define TSIF_TIME_LIMIT_OFF            (0x4)
#define TSIF_CLK_REF_OFF               (0x8)
#define TSIF_LPBK_FLAGS_OFF            (0xc)
#define TSIF_LPBK_DATA_OFF            (0x10)
#define TSIF_TEST_CTL_OFF             (0x14)
#define TSIF_TEST_MODE_OFF            (0x18)
#define TSIF_TEST_RESET_OFF           (0x1c)
#define TSIF_TEST_EXPORT_OFF          (0x20)
#define TSIF_TEST_CURRENT_OFF         (0x24)
#define TSIF_TTS_CTL_OFF	      (0x38)

#define TSIF_DATA_PORT_OFF            (0x100)

/* bits for TSIF_STS_CTL register */
#define TSIF_STS_CTL_EN_IRQ       BIT(28)
#define TSIF_STS_CTL_PACK_AVAIL   BIT(27)
#define TSIF_STS_CTL_1ST_PACKET   BIT(26)
#define TSIF_STS_CTL_OVERFLOW     BIT(25)
#define TSIF_STS_CTL_LOST_SYNC    BIT(24)
#define TSIF_STS_CTL_TIMEOUT      BIT(23)
#define TSIF_STS_CTL_INV_SYNC     BIT(21)
#define TSIF_STS_CTL_INV_NULL     BIT(20)
#define TSIF_STS_CTL_INV_ERROR    BIT(19)
#define TSIF_STS_CTL_INV_ENABLE   BIT(18)
#define TSIF_STS_CTL_INV_DATA     BIT(17)
#define TSIF_STS_CTL_INV_CLOCK    BIT(16)
#define TSIF_STS_CTL_SPARE        BIT(15)
#define TSIF_STS_CTL_EN_NULL      BIT(11)
#define TSIF_STS_CTL_EN_ERROR     BIT(10)
#define TSIF_STS_CTL_LAST_BIT     BIT(9)
#define TSIF_STS_CTL_EN_TIME_LIM  BIT(8)
#define TSIF_STS_CTL_EN_TCR       BIT(7)
#define TSIF_STS_CTL_TEST_MODE    BIT(6)
#define TSIF_STS_CTL_MODE_2       BIT(5)
#define TSIF_STS_CTL_EN_DM        BIT(4)
#define TSIF_STS_CTL_STOP         BIT(3)
#define TSIF_STS_CTL_START        BIT(0)

/* bits for TSIF_TTS_CTRL register */
#define TSIF_TTS_CTL_TTS_ENDIANNESS	BIT(4)
#define TSIF_TTS_CTL_TTS_SOURCE		BIT(3)
#define TSIF_TTS_CTL_TTS_LENGTH_1	BIT(1)
#define TSIF_TTS_CTL_TTS_LENGTH_0	BIT(0)

/*
 * TSPP register offsets
 */
#define TSPP_RST			0x00
#define TSPP_CLK_CONTROL		0x04
#define TSPP_CONFIG			0x08
#define TSPP_CONTROL			0x0C
#define TSPP_PS_DISABLE			0x10
#define TSPP_MSG_IRQ_STATUS		0x14
#define TSPP_MSG_IRQ_MASK		0x18
#define TSPP_IRQ_STATUS			0x1C
#define TSPP_IRQ_MASK			0x20
#define TSPP_IRQ_CLEAR			0x24
#define TSPP_PIPE_ERROR_STATUS(_n)	(0x28 + (_n << 2))
#define TSPP_STATUS			0x68
#define TSPP_CURR_TSP_HEADER		0x6C
#define TSPP_CURR_PID_FILTER		0x70
#define TSPP_SYSTEM_KEY(_n)		(0x74 + (_n << 2))
#define TSPP_CBC_INIT_VAL(_n)		(0x94 + (_n << 2))
#define TSPP_DATA_KEY_RESET		0x9C
#define TSPP_KEY_VALID			0xA0
#define TSPP_KEY_ERROR			0xA4
#define TSPP_TEST_CTRL			0xA8
#define TSPP_VERSION			0xAC
#define TSPP_GENERICS			0xB0
#define TSPP_NOP			0xB4

/*
 * Register bit definitions
 */
/* TSPP_RST */
#define TSPP_RST_RESET                    BIT(0)

/* TSPP_CLK_CONTROL	*/
#define TSPP_CLK_CONTROL_FORCE_CRYPTO     BIT(9)
#define TSPP_CLK_CONTROL_FORCE_PES_PL     BIT(8)
#define TSPP_CLK_CONTROL_FORCE_PES_AF     BIT(7)
#define TSPP_CLK_CONTROL_FORCE_RAW_CTRL   BIT(6)
#define TSPP_CLK_CONTROL_FORCE_PERF_CNT   BIT(5)
#define TSPP_CLK_CONTROL_FORCE_CTX_SEARCH BIT(4)
#define TSPP_CLK_CONTROL_FORCE_TSP_PROC   BIT(3)
#define TSPP_CLK_CONTROL_FORCE_CONS_AHB2MEM BIT(2)
#define TSPP_CLK_CONTROL_FORCE_TS_AHB2MEM BIT(1)
#define TSPP_CLK_CONTROL_SET_CLKON        BIT(0)

/* TSPP_CONFIG	*/
#define TSPP_CONFIG_SET_PACKET_LENGTH(_a, _b) (_a = (_a & 0xF0) | \
((_b & 0xF) << 8))
#define TSPP_CONFIG_GET_PACKET_LENGTH(_a) ((_a >> 8) & 0xF)
#define TSPP_CONFIG_DUP_WITH_DISC_EN		BIT(7)
#define TSPP_CONFIG_PES_SYNC_ERROR_MASK   BIT(6)
#define TSPP_CONFIG_PS_LEN_ERR_MASK       BIT(5)
#define TSPP_CONFIG_PS_CONT_ERR_UNSP_MASK BIT(4)
#define TSPP_CONFIG_PS_CONT_ERR_MASK      BIT(3)
#define TSPP_CONFIG_PS_DUP_TSP_MASK       BIT(2)
#define TSPP_CONFIG_TSP_ERR_IND_MASK      BIT(1)
#define TSPP_CONFIG_TSP_SYNC_ERR_MASK     BIT(0)

/* TSPP_CONTROL */
#define TSPP_CONTROL_PID_FILTER_LOCK      BIT(5)
#define TSPP_CONTROL_FORCE_KEY_CALC       BIT(4)
#define TSPP_CONTROL_TSP_CONS_SRC_DIS     BIT(3)
#define TSPP_CONTROL_TSP_TSIF1_SRC_DIS    BIT(2)
#define TSPP_CONTROL_TSP_TSIF0_SRC_DIS    BIT(1)
#define TSPP_CONTROL_PERF_COUNT_INIT      BIT(0)

/* TSPP_MSG_IRQ_STATUS + TSPP_MSG_IRQ_MASK */
#define TSPP_MSG_TSPP_IRQ                 BIT(2)
#define TSPP_MSG_TSIF_1_IRQ               BIT(1)
#define TSPP_MSG_TSIF_0_IRQ               BIT(0)

/* TSPP_IRQ_STATUS + TSPP_IRQ_MASK + TSPP_IRQ_CLEAR */
#define TSPP_IRQ_STATUS_TSP_RD_CMPL		BIT(19)
#define TSPP_IRQ_STATUS_KEY_ERROR		BIT(18)
#define TSPP_IRQ_STATUS_KEY_SWITCHED_BAD	BIT(17)
#define TSPP_IRQ_STATUS_KEY_SWITCHED		BIT(16)
#define TSPP_IRQ_STATUS_PS_BROKEN(_n)		BIT((_n))

/* TSPP_PIPE_ERROR_STATUS */
#define TSPP_PIPE_PES_SYNC_ERROR		BIT(3)
#define TSPP_PIPE_PS_LENGTH_ERROR		BIT(2)
#define TSPP_PIPE_PS_CONTINUITY_ERROR		BIT(1)
#define TSPP_PIP_PS_LOST_START			BIT(0)

/* TSPP_STATUS			*/
#define TSPP_STATUS_TSP_PKT_AVAIL		BIT(10)
#define TSPP_STATUS_TSIF1_DM_REQ		BIT(6)
#define TSPP_STATUS_TSIF0_DM_REQ		BIT(2)
#define TSPP_CURR_FILTER_TABLE			BIT(0)

/* TSPP_GENERICS		*/
#define TSPP_GENERICS_CRYPTO_GEN		BIT(12)
#define TSPP_GENERICS_MAX_CONS_PIPES		BIT(7)
#define TSPP_GENERICS_MAX_PIPES			BIT(2)
#define TSPP_GENERICS_TSIF_1_GEN		BIT(1)
#define TSPP_GENERICS_TSIF_0_GEN		BIT(0)

/*
 * TSPP memory regions
 */
#define TSPP_PID_FILTER_TABLE0      0x800
#define TSPP_PID_FILTER_TABLE1      0x880
#define TSPP_PID_FILTER_TABLE2      0x900
#define TSPP_GLOBAL_PERFORMANCE     0x980 /* see tspp_global_performance */
#define TSPP_PIPE_CONTEXT           0x990 /* see tspp_pipe_context */
#define TSPP_PIPE_PERFORMANCE       0x998 /* see tspp_pipe_performance */
#define TSPP_TSP_BUFF_WORD(_n)      (0xC10 + (_n << 2))
#define TSPP_DATA_KEY               0xCD0

struct debugfs_entry {
	const char *name;
	mode_t mode;
	int offset;
};

static const struct debugfs_entry debugfs_tsif_regs[] = {
	{"sts_ctl",             0644, TSIF_STS_CTL_OFF},
	{"time_limit",          0644, TSIF_TIME_LIMIT_OFF},
	{"clk_ref",             0644, TSIF_CLK_REF_OFF},
	{"lpbk_flags",          0644, TSIF_LPBK_FLAGS_OFF},
	{"lpbk_data",           0644, TSIF_LPBK_DATA_OFF},
	{"test_ctl",            0644, TSIF_TEST_CTL_OFF},
	{"test_mode",           0644, TSIF_TEST_MODE_OFF},
	{"test_reset",          0200, TSIF_TEST_RESET_OFF},
	{"test_export",         0644, TSIF_TEST_EXPORT_OFF},
	{"test_current",        0444, TSIF_TEST_CURRENT_OFF},
	{"data_port",           0400, TSIF_DATA_PORT_OFF},
	{"tts_source",          0600, TSIF_TTS_CTL_OFF},
};

static const struct debugfs_entry debugfs_tspp_regs[] = {
	{"rst",                 0644, TSPP_RST},
	{"clk_control",         0644, TSPP_CLK_CONTROL},
	{"config",              0644, TSPP_CONFIG},
	{"control",             0644, TSPP_CONTROL},
	{"ps_disable",          0644, TSPP_PS_DISABLE},
	{"msg_irq_status",      0644, TSPP_MSG_IRQ_STATUS},
	{"msg_irq_mask",        0644, TSPP_MSG_IRQ_MASK},
	{"irq_status",          0644, TSPP_IRQ_STATUS},
	{"irq_mask",            0644, TSPP_IRQ_MASK},
	{"irq_clear",           0644, TSPP_IRQ_CLEAR},
	{"status",              0644, TSPP_STATUS},
	{"curr_tsp_header",     0644, TSPP_CURR_TSP_HEADER},
	{"curr_pid_filter",     0644, TSPP_CURR_PID_FILTER},
	{"data_key_reset",      0644, TSPP_DATA_KEY_RESET},
	{"key_valid",           0644, TSPP_KEY_VALID},
	{"key_error",           0644, TSPP_KEY_ERROR},
	{"test_ctrl",           0644, TSPP_TEST_CTRL},
	{"version",             0644, TSPP_VERSION},
	{"generics",            0644, TSPP_GENERICS},
	{"pid_filter_table0",   0644, TSPP_PID_FILTER_TABLE0},
	{"pid_filter_table1",   0644, TSPP_PID_FILTER_TABLE1},
	{"pid_filter_table2",   0644, TSPP_PID_FILTER_TABLE2},
	{"tsp_total_num",       0644, TSPP_GLOBAL_PERFORMANCE},
	{"tsp_ignored_num",     0644, TSPP_GLOBAL_PERFORMANCE + 4},
	{"tsp_err_ind_num",     0644, TSPP_GLOBAL_PERFORMANCE + 8},
	{"tsp_sync_err_num",    0644, TSPP_GLOBAL_PERFORMANCE + 16},
	{"pipe_context",        0644, TSPP_PIPE_CONTEXT},
	{"pipe_performance",    0644, TSPP_PIPE_PERFORMANCE},
	{"data_key",            0644, TSPP_DATA_KEY}
};

struct tspp_pid_filter {
	u32 filter;			/* see FILTER_ macros */
	u32 config;			/* see FILTER_ macros */
};

/* tsp_info */
#define FILTER_HEADER_ERROR_MASK          BIT(7)
#define FILTER_TRANS_END_DISABLE          BIT(6)
#define FILTER_DEC_ON_ERROR_EN            BIT(5)
#define FILTER_DECRYPT                    BIT(4)
#define FILTER_HAS_ENCRYPTION(_p)         (_p->config & FILTER_DECRYPT)
#define FILTER_GET_PIPE_NUMBER0(_p)       (_p->config & 0xF)
#define FILTER_SET_PIPE_NUMBER0(_p, _b)   (_p->config = \
			(_p->config & ~0xF) | (_b & 0xF))
#define FILTER_GET_PIPE_PROCESS0(_p)      ((_p->filter >> 30) & 0x3)
#define FILTER_SET_PIPE_PROCESS0(_p, _b)  (_p->filter = \
			(_p->filter & ~(0x3<<30)) | ((_b & 0x3) << 30))
#define FILTER_GET_PIPE_PID(_p)           ((_p->filter >> 13) & 0x1FFF)
#define FILTER_SET_PIPE_PID(_p, _b)       (_p->filter = \
			(_p->filter & ~(0x1FFF<<13)) | ((_b & 0x1FFF) << 13))
#define FILTER_GET_PID_MASK(_p)           (_p->filter & 0x1FFF)
#define FILTER_SET_PID_MASK(_p, _b)       (_p->filter = \
			(_p->filter & ~0x1FFF) | (_b & 0x1FFF))
#define FILTER_GET_PIPE_PROCESS1(_p)      ((_p->config >> 30) & 0x3)
#define FILTER_SET_PIPE_PROCESS1(_p, _b)  (_p->config = \
			(_p->config & ~(0x3<<30)) | ((_b & 0x3) << 30))
#define FILTER_GET_KEY_NUMBER(_p)         ((_p->config >> 8) & 0x7)
#define FILTER_SET_KEY_NUMBER(_p, _b)     (_p->config = \
			(_p->config & ~(0x7<<8)) | ((_b & 0x7) << 8))

struct tspp_global_performance_regs {
	u32 tsp_total;
	u32 tsp_ignored;
	u32 tsp_error;
	u32 tsp_sync;
};

struct tspp_pipe_context_regs {
	u16 pes_bytes_left;
	u16 count;
	u32 tsif_suffix;
} __packed;
#define CONTEXT_GET_STATE(_a)					(_a & 0x3)
#define CONTEXT_UNSPEC_LENGTH					BIT(11)
#define CONTEXT_GET_CONT_COUNT(_a)			((_a >> 12) & 0xF)

struct tspp_pipe_performance_regs {
	u32 tsp_total;
	u32 ps_duplicate_tsp;
	u32 tsp_no_payload;
	u32 tsp_broken_ps;
	u32 ps_total_num;
	u32 ps_continuity_error;
	u32 ps_length_error;
	u32 pes_sync_error;
};

struct tspp_tsif_device {
	void __iomem *base;
	u32 time_limit;
	u32 ref_count;
	enum tspp_tsif_mode mode;
	int clock_inverse;
	int data_inverse;
	int sync_inverse;
	int enable_inverse;
	u32 tsif_irq;

	/* debugfs */
	struct dentry *dent_tsif;
	struct dentry *debugfs_tsif_regs[ARRAY_SIZE(debugfs_tsif_regs)];
	u32 stat_rx;
	u32 stat_overflow;
	u32 stat_lost_sync;
	u32 stat_timeout;
	enum tsif_tts_source tts_source;
	u32 lpass_timer_enable;
};

enum tspp_buf_state {
	TSPP_BUF_STATE_EMPTY,	/* buffer has been allocated, but not waiting */
	TSPP_BUF_STATE_WAITING, /* buffer is waiting to be filled */
	TSPP_BUF_STATE_DATA,    /* buffer is not empty and can be read */
	TSPP_BUF_STATE_LOCKED   /* buffer is being read by a client */
};

struct tspp_mem_buffer {
	struct tspp_mem_buffer *next;
	struct sps_mem_buffer sps;
	struct tspp_data_descriptor desc; /* buffer descriptor for kernel api */
	enum tspp_buf_state state;
	size_t filled;          /* how much data this buffer is holding */
	int read_index;         /* where to start reading data from */
};

/* this represents each char device 'channel' */
struct tspp_channel {
	struct tspp_device *pdev; /* can use container_of instead? */
	struct sps_pipe *pipe;
	struct sps_connect config;
	struct sps_register_event event;
	struct tspp_mem_buffer *data;    /* list of buffers */
	struct tspp_mem_buffer *read;    /* first buffer ready to be read */
	struct tspp_mem_buffer *waiting; /* first outstanding transfer */
	struct tspp_mem_buffer *locked;  /* buffer currently being read */
	wait_queue_head_t in_queue; /* set when data is received */
	u32 id;           /* channel id (0-15) */
	int used;         /* is this channel in use? */
	int key;          /* which encryption key index is used */
	u32 buffer_size;  /* size of the sps transfer buffers */
	u32 max_buffers;  /* how many buffers should be allocated */
	u32 buffer_count; /* how many buffers are actually allocated */
	u32 filter_count; /* how many filters have been added to this channel */
	u32 int_freq;     /* generate interrupts every x descriptors */
	enum tspp_source src;
	enum tspp_mode mode;
	tspp_notifier *notifier; /* used only with kernel api */
	void *notify_data;       /* data to be passed with the notifier */
	u32 expiration_period_ms; /* notification on partially filled buffers */
	struct timer_list expiration_timer;
	struct dma_pool *dma_pool;
	tspp_memfree *memfree;   /* user defined memory free function */
	void *user_info; /* user cookie passed to memory alloc/free function */
};

struct tspp_pid_filter_table {
	struct tspp_pid_filter filter[TSPP_NUM_PRIORITIES];
};

struct tspp_key_entry {
	u32 even_lsb;
	u32 even_msb;
	u32 odd_lsb;
	u32 odd_msb;
};

struct tspp_key_table {
	struct tspp_key_entry entry[TSPP_NUM_KEYS];
};

struct tspp_pinctrl {
	struct pinctrl *pinctrl;

	struct pinctrl_state *disabled;
	struct pinctrl_state *tsif0_mode1;
	struct pinctrl_state *tsif0_mode2;
	struct pinctrl_state *tsif1_mode1;
	struct pinctrl_state *tsif1_mode2;
	struct pinctrl_state *dual_mode1;
	struct pinctrl_state *dual_mode2;

	bool tsif0_active;
	bool tsif1_active;
};

/* this represents the actual hardware device */
struct tspp_device {
	struct list_head devlist; /* list of all devices */
	struct platform_device *pdev;
	void __iomem *base;
	uint32_t tsif_bus_client;
	unsigned int tspp_irq;
	unsigned int bam_irq;
	unsigned long bam_handle;
	struct sps_bam_props bam_props;
	struct wakeup_source ws;
	spinlock_t spinlock;
	struct tasklet_struct tlet;
	struct tspp_tsif_device tsif[TSPP_TSIF_INSTANCES];
	/* clocks */
	struct clk *tsif_pclk;
	struct clk *tsif_ref_clk;
	/* regulators */
	struct regulator *tsif_vreg;
	/* data */
	struct tspp_pid_filter_table *filters[TSPP_FILTER_TABLES];
	struct tspp_channel channels[TSPP_NUM_CHANNELS];
	struct tspp_key_table *tspp_key_table;
	struct tspp_global_performance_regs *tspp_global_performance;
	struct tspp_pipe_context_regs *tspp_pipe_context;
	struct tspp_pipe_performance_regs *tspp_pipe_performance;
	bool req_irqs;
	/* pinctrl */
	struct mutex mutex;
	struct tspp_pinctrl pinctrl;
	unsigned int tts_source; /* Time stamp source type LPASS timer/TCR */
	struct dma_iommu_mapping *iommu_mapping;

	struct dentry *dent;
	struct dentry *debugfs_regs[ARRAY_SIZE(debugfs_tspp_regs)];
};

static int tspp_key_entry;
static u32 channel_id;  /* next channel id number to assign */

static LIST_HEAD(tspp_devices);

/*** IRQ ***/
static irqreturn_t tspp_isr(int irq, void *dev)
{
	struct tspp_device *device = dev;
	u32 status, mask;
	u32 data;

	status = readl_relaxed(device->base + TSPP_IRQ_STATUS);
	mask = readl_relaxed(device->base + TSPP_IRQ_MASK);
	status &= mask;

	if (!status) {
		pr_warn("%s: spurious interrupt\n", __func__);
		return IRQ_NONE;
	}

	if (status & TSPP_IRQ_STATUS_KEY_ERROR) {
		/* read the key error info */
		data = readl_relaxed(device->base + TSPP_KEY_ERROR);
		pr_err("%s: key error 0x%x\n", __func__, data);
	}
	if (status & TSPP_IRQ_STATUS_KEY_SWITCHED_BAD) {
		data = readl_relaxed(device->base + TSPP_KEY_VALID);
		pr_err("%s: key invalidated: 0x%x\n", __func__, data);
	}
	if (status & TSPP_IRQ_STATUS_KEY_SWITCHED)
		pr_debug("%s: key switched\n", __func__);

	if (status & 0xffff)
		pr_debug("%s: broken pipe %d\n", __func__, status & 0xffff);

	writel_relaxed(status, device->base + TSPP_IRQ_CLEAR);

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

static irqreturn_t tsif_isr(int irq, void *dev)
{
	struct tspp_tsif_device *tsif_device = dev;
	u32 sts_ctl = ioread32(tsif_device->base + TSIF_STS_CTL_OFF);

	if (!(sts_ctl & (TSIF_STS_CTL_PACK_AVAIL |
			 TSIF_STS_CTL_OVERFLOW |
			 TSIF_STS_CTL_LOST_SYNC |
			 TSIF_STS_CTL_TIMEOUT)))
		return IRQ_NONE;

	if (sts_ctl & TSIF_STS_CTL_OVERFLOW)
		tsif_device->stat_overflow++;

	if (sts_ctl & TSIF_STS_CTL_LOST_SYNC)
		tsif_device->stat_lost_sync++;

	if (sts_ctl & TSIF_STS_CTL_TIMEOUT)
		tsif_device->stat_timeout++;

	iowrite32(sts_ctl, tsif_device->base + TSIF_STS_CTL_OFF);

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

/*** callbacks ***/
static void tspp_sps_complete_cb(struct sps_event_notify *notify)
{
	struct tspp_device *pdev;

	if (!notify || !notify->user)
		return;

	pdev = notify->user;
	tasklet_schedule(&pdev->tlet);
}

static void tspp_expiration_timer(struct timer_list *t)
{
	struct tspp_channel *channel = from_timer(channel, t, expiration_timer);

	if (channel && channel->pdev)
		tasklet_schedule(&channel->pdev->tlet);
}

/*** tasklet ***/
static void tspp_sps_complete_tlet(unsigned long data)
{
	int i;
	int complete;
	unsigned long flags;
	struct sps_iovec iovec;
	struct tspp_channel *channel;
	struct tspp_device *device = (struct tspp_device *)data;

	spin_lock_irqsave(&device->spinlock, flags);

	for (i = 0; i < TSPP_NUM_CHANNELS; i++) {
		complete = 0;
		channel = &device->channels[i];

		if (!channel->used || !channel->waiting)
			continue;

		/* stop the expiration timer */
		if (channel->expiration_period_ms)
			del_timer(&channel->expiration_timer);

		/* get completions */
		while (channel->waiting->state == TSPP_BUF_STATE_WAITING) {
			if (sps_get_iovec(channel->pipe, &iovec) != 0) {
				pr_err("%s: error in iovec on channel %d\n",
					__func__, channel->id);
				break;
			}
			if (iovec.size == 0)
				break;

			if (DESC_FULL_ADDR(iovec.flags, iovec.addr)
			    != channel->waiting->sps.phys_base)
				pr_err("%s: buffer mismatch\n", __func__);

			complete = 1;
			channel->waiting->state = TSPP_BUF_STATE_DATA;
			channel->waiting->filled = iovec.size;
			channel->waiting->read_index = 0;

			if (channel->src == TSPP_SOURCE_TSIF0)
				device->tsif[0].stat_rx++;
			else if (channel->src == TSPP_SOURCE_TSIF1)
				device->tsif[1].stat_rx++;

			/* update the pointers */
			channel->waiting = channel->waiting->next;
		}

		/* wake any waiting processes */
		if (complete) {
			wake_up_interruptible(&channel->in_queue);

			/* call notifiers */
			if (channel->notifier)
				channel->notifier(channel->id,
					channel->notify_data);
		}

		/* restart expiration timer */
		if (channel->expiration_period_ms)
			mod_timer(&channel->expiration_timer,
				jiffies +
				msecs_to_jiffies(
					channel->expiration_period_ms));
	}

	spin_unlock_irqrestore(&device->spinlock, flags);
}

static int tspp_config_gpios(struct tspp_device *device,
				enum tspp_source source,
				int enable)
{
	int ret;
	struct pinctrl_state *s;
	struct tspp_pinctrl *p = &device->pinctrl;
	bool mode2;

	/*
	 * TSIF devices are handled separately, however changing of the pinctrl
	 * state must be protected from race condition.
	 */
	if (mutex_lock_interruptible(&device->mutex))
		return -ERESTARTSYS;

	switch (source) {
	case TSPP_SOURCE_TSIF0:
		mode2 = device->tsif[0].mode == TSPP_TSIF_MODE_2;
		if (enable == p->tsif1_active) {
			if (enable)
				/* Both tsif enabled */
				s = mode2 ? p->dual_mode2 : p->dual_mode1;
			else
				/* Both tsif disabled */
				s = p->disabled;
		} else if (enable) {
			/* Only tsif0 is enabled */
			s = mode2 ? p->tsif0_mode2 : p->tsif0_mode1;
		} else {
			/* Only tsif1 is enabled */
			s = mode2 ? p->tsif1_mode2 : p->tsif1_mode1;
		}

		ret = pinctrl_select_state(p->pinctrl, s);
		if (!ret)
			p->tsif0_active = enable;
		break;
	case TSPP_SOURCE_TSIF1:
		mode2 = device->tsif[1].mode == TSPP_TSIF_MODE_2;
		if (enable == p->tsif0_active) {
			if (enable)
				/* Both tsif enabled */
				s = mode2 ? p->dual_mode2 : p->dual_mode1;
			else
				/* Both tsif disabled */
				s = p->disabled;
		} else if (enable) {
			/* Only tsif1 is enabled */
			s = mode2 ? p->tsif1_mode2 : p->tsif1_mode1;
		} else {
			/* Only tsif0 is enabled */
			s = mode2 ? p->tsif0_mode2 : p->tsif0_mode1;
		}

		ret = pinctrl_select_state(p->pinctrl, s);
		if (!ret)
			p->tsif1_active = enable;
		break;
	default:
		pr_err("%s: invalid source %d\n", __func__, source);
		mutex_unlock(&device->mutex);
		return -EINVAL;
	}

	if (ret)
		pr_err("%s: failed to change pinctrl state, ret=%d\n",
			__func__, ret);

	mutex_unlock(&device->mutex);
	return ret;
}

static int tspp_get_pinctrl(struct tspp_device *device)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *state;

	pinctrl = devm_pinctrl_get(&device->pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s: unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	device->pinctrl.pinctrl = pinctrl;

	state = pinctrl_lookup_state(pinctrl, "disabled");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n", __func__, "disabled");
		return -EINVAL;
	}
	device->pinctrl.disabled = state;

	state = pinctrl_lookup_state(pinctrl, "tsif0-mode1");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "tsif0-mode1");
		return -EINVAL;
	}
	device->pinctrl.tsif0_mode1 = state;

	state = pinctrl_lookup_state(pinctrl, "tsif0-mode2");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "tsif0-mode2");
		return -EINVAL;
	}
	device->pinctrl.tsif0_mode2 = state;

	state = pinctrl_lookup_state(pinctrl, "tsif1-mode1");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "tsif1-mode1");
		return -EINVAL;
	}
	device->pinctrl.tsif1_mode1 = state;

	state = pinctrl_lookup_state(pinctrl, "tsif1-mode2");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "tsif1-mode2");
		return -EINVAL;
	}
	device->pinctrl.tsif1_mode2 = state;

	state = pinctrl_lookup_state(pinctrl, "dual-tsif-mode1");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "dual-tsif-mode1");
		return -EINVAL;
	}
	device->pinctrl.dual_mode1 = state;

	state = pinctrl_lookup_state(pinctrl, "dual-tsif-mode2");
	if (IS_ERR_OR_NULL(state)) {
		pr_err("%s: unable to find state %s\n",
			__func__, "dual-tsif-mode2");
		return -EINVAL;
	}
	device->pinctrl.dual_mode2 = state;

	device->pinctrl.tsif0_active = false;
	device->pinctrl.tsif1_active = false;

	return 0;
}


/*** Clock functions ***/
static int tspp_clock_start(struct tspp_device *device)
{
	int rc;

	if (device->tsif_bus_client) {
		rc = msm_bus_scale_client_update_request(
					device->tsif_bus_client, 1);
		if (rc) {
			pr_err("%s: can't enable bus\n", __func__);
			return -EBUSY;
		}
	}

	if (device->tsif_vreg) {
		rc = regulator_set_voltage(device->tsif_vreg,
					0,
					RPMH_REGULATOR_LEVEL_MAX);
		if (rc) {
			pr_err("%s: unable to set CX voltage\n", __func__);
			if (device->tsif_bus_client)
				msm_bus_scale_client_update_request(
					device->tsif_bus_client, 0);
			return rc;
		}
	}

	if (device->tsif_pclk && clk_prepare_enable(device->tsif_pclk) != 0) {
		pr_err("%s: can't start %s\n", __func__, "pclk");

		if (device->tsif_vreg) {
			regulator_set_voltage(device->tsif_vreg,
					0,
					RPMH_REGULATOR_LEVEL_MAX);
		}

		if (device->tsif_bus_client)
			msm_bus_scale_client_update_request(
				device->tsif_bus_client, 0);
		return -EBUSY;
	}

	if (device->tsif_ref_clk &&
		clk_prepare_enable(device->tsif_ref_clk) != 0) {
		pr_err("%s: can't start %s\n", __func__, "ref clk");
		clk_disable_unprepare(device->tsif_pclk);
		if (device->tsif_vreg) {
			regulator_set_voltage(device->tsif_vreg,
					0,
					RPMH_REGULATOR_LEVEL_MAX);
		}

		if (device->tsif_bus_client)
			msm_bus_scale_client_update_request(
				device->tsif_bus_client, 0);
		return -EBUSY;
	}

	return 0;
}

static void tspp_clock_stop(struct tspp_device *device)
{
	int rc;

	if (device->tsif_pclk)
		clk_disable_unprepare(device->tsif_pclk);

	if (device->tsif_ref_clk)
		clk_disable_unprepare(device->tsif_ref_clk);

	if (device->tsif_vreg) {
		rc = regulator_set_voltage(device->tsif_vreg,
					0,
					RPMH_REGULATOR_LEVEL_MAX);
		if (rc)
			pr_err("%s: unable to set CX voltage\n", __func__);
	}

	if (device->tsif_bus_client) {
		rc = msm_bus_scale_client_update_request(
					device->tsif_bus_client, 0);
		if (rc)
			pr_err("%s: can't disable bus\n", __func__);
	}
}

/*** TSIF functions ***/
static int tspp_start_tsif(struct tspp_tsif_device *tsif_device)
{
	int start_hardware = 0;
	u32 ctl;

	if (tsif_device->ref_count == 0) {
		start_hardware = 1;
	} else if (tsif_device->ref_count > 0) {
		ctl = readl_relaxed(tsif_device->base + TSIF_STS_CTL_OFF);
		if ((ctl & TSIF_STS_CTL_START) != 1) {
			/* this hardware should already be running */
			pr_err("%s: tsif hw not started but ref count > 0\n",
				__func__);
			start_hardware = 1;
		}
	}

	if (start_hardware) {
		ctl = TSIF_STS_CTL_EN_IRQ |
				TSIF_STS_CTL_EN_DM |
				TSIF_STS_CTL_PACK_AVAIL |
				TSIF_STS_CTL_OVERFLOW |
				TSIF_STS_CTL_LOST_SYNC;

		if (tsif_device->clock_inverse)
			ctl |= TSIF_STS_CTL_INV_CLOCK;

		if (tsif_device->data_inverse)
			ctl |= TSIF_STS_CTL_INV_DATA;

		if (tsif_device->sync_inverse)
			ctl |= TSIF_STS_CTL_INV_SYNC;

		if (tsif_device->enable_inverse)
			ctl |= TSIF_STS_CTL_INV_ENABLE;

		switch (tsif_device->mode) {
		case TSPP_TSIF_MODE_LOOPBACK:
			ctl |= TSIF_STS_CTL_EN_NULL |
					TSIF_STS_CTL_EN_ERROR |
					TSIF_STS_CTL_TEST_MODE;
			break;
		case TSPP_TSIF_MODE_1:
			ctl |= TSIF_STS_CTL_EN_TIME_LIM;
			if (tsif_device->tts_source != TSIF_TTS_LPASS_TIMER)
				ctl |= TSIF_STS_CTL_EN_TCR;
			break;
		case TSPP_TSIF_MODE_2:
			ctl |= TSIF_STS_CTL_EN_TIME_LIM |
					TSIF_STS_CTL_MODE_2;
			if (tsif_device->tts_source != TSIF_TTS_LPASS_TIMER)
				ctl |= TSIF_STS_CTL_EN_TCR;
			break;
		default:
			pr_warn("%s: unknown tsif mode 0x%x\n", __func__,
				tsif_device->mode);
		}

		writel_relaxed(ctl, tsif_device->base + TSIF_STS_CTL_OFF);
		/* write Status control register */
		wmb();
		writel_relaxed(tsif_device->time_limit,
			  tsif_device->base + TSIF_TIME_LIMIT_OFF);
		/* assure register configuration is done before starting TSIF */
		wmb();
		writel_relaxed(ctl | TSIF_STS_CTL_START,
			  tsif_device->base + TSIF_STS_CTL_OFF);
		/* assure TSIF start configuration */
		wmb();
	}

	ctl = readl_relaxed(tsif_device->base + TSIF_STS_CTL_OFF);
	if (!(ctl & TSIF_STS_CTL_START))
		return -EBUSY;

	tsif_device->ref_count++;
	return 0;
}

static void tspp_stop_tsif(struct tspp_tsif_device *tsif_device)
{
	if (tsif_device->ref_count == 0)
		return;

	tsif_device->ref_count--;

	if (tsif_device->ref_count == 0) {
		writel_relaxed(TSIF_STS_CTL_STOP,
			tsif_device->base + TSIF_STS_CTL_OFF);
		/* assure TSIF stop configuration */
		wmb();
	}
}

/*** local TSPP functions ***/
static int tspp_channels_in_use(struct tspp_device *pdev)
{
	int i;
	int count = 0;

	for (i = 0; i < TSPP_NUM_CHANNELS; i++)
		count += (pdev->channels[i].used ? 1 : 0);

	return count;
}

static struct tspp_device *tspp_find_by_id(int id)
{
	struct tspp_device *dev;

	list_for_each_entry(dev, &tspp_devices, devlist) {
		if (dev->pdev->id == id)
			return dev;
	}
	return NULL;
}

static int tspp_get_key_entry(void)
{
	int i;

	for (i = 0; i < TSPP_NUM_KEYS; i++) {
		if (!(tspp_key_entry & (1 << i))) {
			tspp_key_entry |= (1 << i);
			return i;
		}
	}
	return 1 < TSPP_NUM_KEYS;
}

static void tspp_free_key_entry(int entry)
{
	if (entry > TSPP_NUM_KEYS) {
		pr_err("%s: index out of bounds\n", __func__);
		return;
	}

	tspp_key_entry &= ~(1 << entry);
}


static void tspp_iommu_release_iomapping(struct tspp_device *device)
{
	device->iommu_mapping = NULL;
}

static int tspp_alloc_buffer(u32 channel_id, struct tspp_data_descriptor *desc,
	u32 size, struct dma_pool *dma_pool, tspp_allocator *alloc, void *user)
{
	if (size < TSPP_MIN_BUFFER_SIZE ||
		size > TSPP_MAX_BUFFER_SIZE) {
		pr_err("%s: bad buffer size %d\n", __func__, size);
		return -ENOMEM;
	}

	if (alloc) {
		TSPP_DEBUG("%s: using alloc function\n");
		desc->virt_base = alloc(channel_id, size,
			&desc->phys_base, &desc->dma_base, user);
	} else {
		if (!dma_pool)
			desc->virt_base = dma_alloc_coherent(NULL, size,
				&desc->phys_base, GFP_KERNEL);
		else
			desc->virt_base = dma_pool_alloc(dma_pool, GFP_KERNEL,
				&desc->phys_base);

		if (desc->virt_base == 0) {
			pr_err("%s: dma buffer allocation failed %d\n",
				__func__, size);
			return -ENOMEM;
		}
	}

	desc->size = size;
	return 0;
}

static int tspp_queue_buffer(struct tspp_channel *channel,
	struct tspp_mem_buffer *buffer)
{
	int rc;
	u32 flags = 0;

	/* make sure the interrupt frequency is valid */
	if (channel->int_freq < 1)
		channel->int_freq = 1;

	/* generate interrupt according to requested frequency */
	if (buffer->desc.id % channel->int_freq == channel->int_freq-1)
		flags = SPS_IOVEC_FLAG_INT;

	/* start the transfer */
	rc = sps_transfer_one(channel->pipe,
		buffer->sps.phys_base,
		buffer->sps.size,
		flags ? channel->pdev : NULL,
		flags);
	if (rc < 0)
		return rc;

	buffer->state = TSPP_BUF_STATE_WAITING;

	return 0;
}

static int tspp_global_reset(struct tspp_device *pdev)
{
	u32 i, val;

	/* stop all TSIFs */
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++) {
		pdev->tsif[i].ref_count = 1; /* allows stopping hw */
		tspp_stop_tsif(&pdev->tsif[i]); /* will reset ref_count to 0 */
		pdev->tsif[i].time_limit = TSPP_TSIF_DEFAULT_TIME_LIMIT;
		pdev->tsif[i].clock_inverse = 0;
		pdev->tsif[i].data_inverse = 0;
		pdev->tsif[i].sync_inverse = 0;
		pdev->tsif[i].enable_inverse = 0;
		pdev->tsif[i].lpass_timer_enable = 0;
	}
	writel_relaxed(TSPP_RST_RESET, pdev->base + TSPP_RST);
	/* assure state is reset before continuing with configuration */
	wmb();

	/* TSPP tables */
	for (i = 0; i < TSPP_FILTER_TABLES; i++)
		memset_io(pdev->filters[i],
			0, sizeof(*pdev->filters[i]));

	/* disable all filters */
	val = (2 << TSPP_NUM_CHANNELS) - 1;
	writel_relaxed(val, pdev->base + TSPP_PS_DISABLE);

	/* TSPP registers */
	val = readl_relaxed(pdev->base + TSPP_CONTROL);
	writel_relaxed(val | TSPP_CLK_CONTROL_FORCE_PERF_CNT,
		pdev->base + TSPP_CONTROL);
	/* assure tspp performance count clock is set to 0 */
	wmb();
	memset_io(pdev->tspp_global_performance, 0,
		sizeof(*pdev->tspp_global_performance));
	memset_io(pdev->tspp_pipe_context, 0,
		sizeof(*pdev->tspp_pipe_context));
	memset_io(pdev->tspp_pipe_performance, 0,
		sizeof(*pdev->tspp_pipe_performance));
	/* assure tspp pipe context registers are set to 0 */
	wmb();
	writel_relaxed(val & ~TSPP_CLK_CONTROL_FORCE_PERF_CNT,
		pdev->base + TSPP_CONTROL);
	/* assure tspp performance count clock  is reset */
	wmb();

	val = readl_relaxed(pdev->base + TSPP_CONFIG);
	val &= ~(TSPP_CONFIG_PS_LEN_ERR_MASK |
			TSPP_CONFIG_PS_CONT_ERR_UNSP_MASK |
			TSPP_CONFIG_PS_CONT_ERR_MASK);
	TSPP_CONFIG_SET_PACKET_LENGTH(val, TSPP_PACKET_LENGTH);
	writel_relaxed(val, pdev->base + TSPP_CONFIG);
	writel_relaxed(0x0007ffff, pdev->base + TSPP_IRQ_MASK);
	writel_relaxed(0x000fffff, pdev->base + TSPP_IRQ_CLEAR);
	writel_relaxed(0, pdev->base + TSPP_RST);
	/* assure tspp reset clear */
	wmb();

	tspp_key_entry = 0;

	return 0;
}

static void tspp_channel_init(struct tspp_channel *channel,
	struct tspp_device *pdev)
{
	channel->pdev = pdev;
	channel->data = NULL;
	channel->read = NULL;
	channel->waiting = NULL;
	channel->locked = NULL;
	channel->id = channel_id++;
	channel->used = 0;
	channel->buffer_size = TSPP_MIN_BUFFER_SIZE;
	channel->max_buffers = TSPP_NUM_BUFFERS;
	channel->buffer_count = 0;
	channel->filter_count = 0;
	channel->int_freq = 1;
	channel->src = TSPP_SOURCE_NONE;
	channel->mode = TSPP_MODE_DISABLED;
	channel->notifier = NULL;
	channel->notify_data = NULL;
	channel->expiration_period_ms = 0;
	channel->memfree = NULL;
	channel->user_info = NULL;
	init_waitqueue_head(&channel->in_queue);
}

static void tspp_set_tsif_mode(struct tspp_channel *channel,
	enum tspp_tsif_mode mode)
{
	int index;

	switch (channel->src) {
	case TSPP_SOURCE_TSIF0:
		index = 0;
		break;
	case TSPP_SOURCE_TSIF1:
		index = 1;
		break;
	default:
		pr_warn("%s: can't set mode for non-tsif source %d\n", __func__,
				channel->src);
		return;
	}
	channel->pdev->tsif[index].mode = mode;
}

static void tspp_set_signal_inversion(struct tspp_channel *channel,
					int clock_inverse, int data_inverse,
					int sync_inverse, int enable_inverse)
{
	int index;

	switch (channel->src) {
	case TSPP_SOURCE_TSIF0:
		index = 0;
		break;
	case TSPP_SOURCE_TSIF1:
		index = 1;
		break;
	default:
		return;
	}
	channel->pdev->tsif[index].clock_inverse = clock_inverse;
	channel->pdev->tsif[index].data_inverse = data_inverse;
	channel->pdev->tsif[index].sync_inverse = sync_inverse;
	channel->pdev->tsif[index].enable_inverse = enable_inverse;
}

static int tspp_is_buffer_size_aligned(u32 size, enum tspp_mode mode)
{
	u32 alignment;

	switch (mode) {
	case TSPP_MODE_RAW:
		/* must be a multiple of 192 */
		alignment = (TSPP_PACKET_LENGTH + 4);
		if (size % alignment)
			return 0;
		return 1;

	case TSPP_MODE_RAW_NO_SUFFIX:
		/* must be a multiple of 188 */
		alignment = TSPP_PACKET_LENGTH;
		if (size % alignment)
			return 0;
		return 1;

	case TSPP_MODE_DISABLED:
	case TSPP_MODE_PES:
	default:
		/* no alignment requirement */
		return 1;
	}

}

static u32 tspp_align_buffer_size_by_mode(u32 size, enum tspp_mode mode)
{
	u32 new_size;
	u32 alignment;

	switch (mode) {
	case TSPP_MODE_RAW:
		/* must be a multiple of 192 */
		alignment = (TSPP_PACKET_LENGTH + 4);
		break;

	case TSPP_MODE_RAW_NO_SUFFIX:
		/* must be a multiple of 188 */
		alignment = TSPP_PACKET_LENGTH;
		break;

	case TSPP_MODE_DISABLED:
	case TSPP_MODE_PES:
	default:
		/* no alignment requirement - give the user what he asks for */
		alignment = 1;
		break;
	}
	/* align up */
	new_size = (((size + alignment - 1) / alignment) * alignment);
	return new_size;
}

static void tspp_destroy_buffers(u32 channel_id, struct tspp_channel *channel)
{
	int i;
	struct tspp_mem_buffer *pbuf, *temp;

	pbuf = channel->data;
	for (i = 0; i < channel->buffer_count; i++) {
		if (pbuf->desc.phys_base) {
			if (channel->memfree) {
				channel->memfree(channel_id,
					pbuf->desc.size,
					pbuf->desc.virt_base,
					pbuf->desc.phys_base,
					channel->user_info);
			} else {
				if (!channel->dma_pool)
					dma_free_coherent(
						&channel->pdev->pdev->dev,
						pbuf->desc.size,
						pbuf->desc.virt_base,
						pbuf->desc.phys_base);
				else
					dma_pool_free(channel->dma_pool,
						pbuf->desc.virt_base,
						pbuf->desc.phys_base);
			}
			pbuf->desc.phys_base = 0;
		}
		pbuf->desc.virt_base = 0;
		pbuf->state = TSPP_BUF_STATE_EMPTY;
		temp = pbuf;
		pbuf = pbuf->next;
		kfree(temp);
	}
}

static int msm_tspp_req_irqs(struct tspp_device *device)
{
	int rc;
	int i;
	int j;

	rc = request_irq(device->tspp_irq, tspp_isr, IRQF_SHARED,
		dev_name(&device->pdev->dev), device);
	if (rc) {
		pr_err("%s: failed to request TSPP IRQ %d : %d\n", __func__,
				device->tspp_irq, rc);
		return rc;
	}

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++) {
		rc = request_irq(device->tsif[i].tsif_irq,
			tsif_isr, IRQF_SHARED, dev_name(&device->pdev->dev),
			&device->tsif[i]);
		if (rc) {
			pr_err("%s: failed to request TSIF%d IRQ: %d\n",
				__func__, i, rc);
			goto failed;
		}
	}
	device->req_irqs = true;
	return 0;

failed:
	free_irq(device->tspp_irq, device);
	for (j = 0; j < i; j++)
		free_irq(device->tsif[j].tsif_irq, device);

	return rc;
}

static inline void msm_tspp_free_irqs(struct tspp_device *device)
{
	int i;

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		free_irq(device->tsif[i].tsif_irq,  &device->tsif[i]);

	free_irq(device->tspp_irq, device);
	device->req_irqs = false;
}

/*** TSPP API functions ***/

/**
 * tspp_open_stream - open a TSPP stream for use.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @source: stream source parameters.
 *
 * Return  error status
 *
 */
int tspp_open_stream(u32 dev, u32 channel_id,
			struct tspp_select_source *source)
{
	u32 val;
	int rc;
	struct tspp_device *pdev;
	struct tspp_channel *channel;
	bool req_irqs = false;

	TSPP_DEBUG("%s: %d %d %d %d\n", __func__, dev, channel_id,
					source->source, source->mode);

	if (dev >= TSPP_MAX_DEVICES) {
		pr_err("%s: device id out of range[0, %d]\n",
				__func__, TSPP_MAX_DEVICES);
		return -ENODEV;
	}

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->src = source->source;
	tspp_set_tsif_mode(channel, source->mode);
	tspp_set_signal_inversion(channel, source->clk_inverse,
			source->data_inverse, source->sync_inverse,
			source->enable_inverse);

	/* Request IRQ resources on first open */
	if (!pdev->req_irqs && (source->source == TSPP_SOURCE_TSIF0 ||
		source->source == TSPP_SOURCE_TSIF1)) {
		rc = msm_tspp_req_irqs(pdev);
		if (rc)
			return rc;
		req_irqs = true;
	}

	switch (source->source) {
	case TSPP_SOURCE_TSIF0:
		if (tspp_config_gpios(pdev, channel->src, 1) != 0) {
			rc = -EBUSY;
			pr_err("%s: error enabling %s\n",
				__func__, "tsif0 GPIOs");
			goto free_irq;
		}
		/* make sure TSIF0 is running & enabled */
		if (tspp_start_tsif(&pdev->tsif[0]) != 0) {
			rc = -EBUSY;
			pr_err("%s: error starting %s\n", __func__, "tsif0");
			goto free_irq;
		}
		if (pdev->tsif[0].ref_count == 1) {
			val = readl_relaxed(pdev->base + TSPP_CONTROL);
			writel_relaxed(val & ~TSPP_CONTROL_TSP_TSIF0_SRC_DIS,
				pdev->base + TSPP_CONTROL);
			/* Assure BAM TS PKT packet processing is enabled */
			wmb();
		}
		break;
	case TSPP_SOURCE_TSIF1:
		if (tspp_config_gpios(pdev, channel->src, 1) != 0) {
			rc = -EBUSY;
			pr_err("%s: error enabling %s\n",
				__func__, "tsif1 GPIOs");
			goto free_irq;
		}
		/* make sure TSIF1 is running & enabled */
		if (tspp_start_tsif(&pdev->tsif[1]) != 0) {
			rc = -EBUSY;
			pr_err("%s: error starting %s\n", __func__, "tsif1");
			goto free_irq;
		}
		if (pdev->tsif[1].ref_count == 1) {
			val = readl_relaxed(pdev->base + TSPP_CONTROL);
			writel_relaxed(val & ~TSPP_CONTROL_TSP_TSIF1_SRC_DIS,
				pdev->base + TSPP_CONTROL);
			/* Assure BAM TS PKT packet processing is enabled */
			wmb();
		}
		break;
	case TSPP_SOURCE_MEM:
		break;
	default:
		pr_err("%s: channel %d invalid source %d\n", __func__,
			channel->id, source->source);
		return -EBUSY;
	}

	return 0;

free_irq:
	/* Free irqs only if were requested during opening of this stream */
	if (req_irqs)
		msm_tspp_free_irqs(pdev);
	return rc;
}
EXPORT_SYMBOL(tspp_open_stream);

/**
 * tspp_close_stream - close a TSPP stream.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 *
 * Return  error status
 *
 */
int tspp_close_stream(u32 dev, u32 channel_id)
{
	u32 val;
	u32 prev_ref_count = 0;
	struct tspp_device *pdev;
	struct tspp_channel *channel;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n", __func__,
				TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -EBUSY;
	}
	channel = &pdev->channels[channel_id];

	switch (channel->src) {
	case TSPP_SOURCE_TSIF0:
		prev_ref_count = pdev->tsif[0].ref_count;
		tspp_stop_tsif(&pdev->tsif[0]);
		if (tspp_config_gpios(pdev, channel->src, 0) != 0)
			pr_err("%s: error disabling %s\n",
				__func__, "tsif0 GPIOs");

		if (prev_ref_count == 1) {
			val = readl_relaxed(pdev->base + TSPP_CONTROL);
			writel_relaxed(val | TSPP_CONTROL_TSP_TSIF0_SRC_DIS,
				pdev->base + TSPP_CONTROL);
			/* Assure BAM TS PKT packet processing is disabled */
			wmb();
		}
		break;
	case TSPP_SOURCE_TSIF1:
		prev_ref_count = pdev->tsif[1].ref_count;
		tspp_stop_tsif(&pdev->tsif[1]);
		if (tspp_config_gpios(pdev, channel->src, 0) != 0)
			pr_err("%s: error disabling %s\n",
				__func__, "tsif1 GPIOs");

		if (prev_ref_count == 1) {
			val = readl_relaxed(pdev->base + TSPP_CONTROL);
			writel_relaxed(val | TSPP_CONTROL_TSP_TSIF1_SRC_DIS,
				pdev->base + TSPP_CONTROL);
			/* Assure BAM TS PKT packet processing is disabled */
			wmb();
		}
		break;
	case TSPP_SOURCE_MEM:
		break;
	case TSPP_SOURCE_NONE:
		break;
	}

	channel->src = TSPP_SOURCE_NONE;

	/* Free requested interrupts to save power */
	if ((pdev->tsif[0].ref_count + pdev->tsif[1].ref_count) == 0 &&
		prev_ref_count)
		msm_tspp_free_irqs(pdev);

	return 0;
}
EXPORT_SYMBOL(tspp_close_stream);

static int tspp_init_sps_device(struct tspp_device *dev)
{
	int ret;

	ret = sps_register_bam_device(&dev->bam_props, &dev->bam_handle);
	if (ret) {
		pr_err("%s: failed to register bam device, err-%d\n",
			__func__, ret);
		return ret;
	}

	ret = sps_device_reset(dev->bam_handle);
	if (ret) {
		sps_deregister_bam_device(dev->bam_handle);
		pr_err("%s: error resetting bam device, err=%d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

/**
 * tspp_open_channel - open a TSPP channel.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 *
 * Return  error status
 *
 */
int tspp_open_channel(u32 dev, u32 channel_id)
{
	int rc = 0;
	struct sps_connect *config;
	struct sps_register_event *event;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	if (channel->used) {
		pr_err("%s: channel already in use\n", __func__);
		return -EBUSY;
	}

	config = &channel->config;
	event = &channel->event;

	/* start the clocks if needed */
	if (tspp_channels_in_use(pdev) == 0) {
		rc = tspp_clock_start(pdev);
		if (rc)
			return rc;

		if (pdev->bam_handle == SPS_DEV_HANDLE_INVALID) {
			rc = tspp_init_sps_device(pdev);
			if (rc) {
				tspp_clock_stop(pdev);
				return rc;
			}
		}

		__pm_stay_awake(&pdev->ws);
	}

	/* mark it as used */
	channel->used = 1;

	/* start the bam  */
	channel->pipe = sps_alloc_endpoint();
	if (channel->pipe == NULL) {
		pr_err("%s: error allocating endpoint\n", __func__);
		rc = -ENOMEM;
		goto err_sps_alloc;
	}

	/* get default configuration */
	sps_get_config(channel->pipe, config);

	config->source = pdev->bam_handle;
	config->destination = SPS_DEV_HANDLE_MEM;
	config->mode = SPS_MODE_SRC;
	config->options =
		SPS_O_AUTO_ENABLE | /* connection is auto-enabled */
		SPS_O_STREAMING | /* streaming mode */
		SPS_O_DESC_DONE | /* interrupt on end of descriptor */
		SPS_O_ACK_TRANSFERS | /* must use sps_get_iovec() */
		SPS_O_HYBRID; /* Read actual descriptors in sps_get_iovec() */
	config->src_pipe_index = channel->id;
	config->desc.size =
		TSPP_SPS_DESCRIPTOR_COUNT * SPS_DESCRIPTOR_SIZE;
	config->desc.base = dma_alloc_coherent(&pdev->pdev->dev,
						config->desc.size,
						&config->desc.phys_base,
						GFP_KERNEL);
	if (config->desc.base == 0) {
		pr_err("%s: error allocating sps descriptors\n", __func__);
		rc = -ENOMEM;
		goto err_desc_alloc;
	}

	memset(config->desc.base, 0, config->desc.size);

	rc = sps_connect(channel->pipe, config);
	if (rc) {
		pr_err("%s: error connecting bam\n", __func__);
		goto err_connect;
	}

	event->mode = SPS_TRIGGER_CALLBACK;
	event->options = SPS_O_DESC_DONE;
	event->callback = tspp_sps_complete_cb;
	event->xfer_done = NULL;
	event->user = pdev;

	rc = sps_register_event(channel->pipe, event);
	if (rc) {
		pr_err("%s: error registering event\n", __func__);
		goto err_event;
	}

	timer_setup(&channel->expiration_timer, tspp_expiration_timer, 0);
	channel->expiration_timer.expires = 0xffffffffL;

	rc = pm_runtime_get(&pdev->pdev->dev);
	if (rc < 0) {
		pr_err(
		  "%s: Runtime PM : Unable to wake up tspp device, rc = %d\n",
		  __func__, rc);
	}
	return 0;

err_event:
	sps_disconnect(channel->pipe);
err_connect:
	dma_free_coherent(&pdev->pdev->dev, config->desc.size,
		config->desc.base, config->desc.phys_base);
err_desc_alloc:
	sps_free_endpoint(channel->pipe);
err_sps_alloc:
	channel->used = 0;
	return rc;
}
EXPORT_SYMBOL(tspp_open_channel);

/**
 * tspp_close_channel - close a TSPP channel.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 *
 * Return  error status
 *
 */
int tspp_close_channel(u32 dev, u32 channel_id)
{
	int i;
	int id;
	int table_idx;
	u32 val;
	unsigned long flags;

	struct sps_connect *config;
	struct tspp_device *pdev;
	struct tspp_channel *channel;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	/* if the channel is not used, we are done */
	if (!channel->used)
		return 0;

	/*
	 * Need to protect access to used and waiting fields, as they are
	 * used by the tasklet which is invoked from interrupt context
	 */
	spin_lock_irqsave(&pdev->spinlock, flags);
	channel->used = 0;
	channel->waiting = NULL;
	spin_unlock_irqrestore(&pdev->spinlock, flags);

	if (channel->expiration_period_ms)
		del_timer(&channel->expiration_timer);

	channel->notifier = NULL;
	channel->notify_data = NULL;
	channel->expiration_period_ms = 0;

	config = &channel->config;
	pdev = channel->pdev;

	/* disable pipe (channel) */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val | channel->id, pdev->base + TSPP_PS_DISABLE);
	/* Assure PS_DISABLE register is set */
	wmb();

	/* unregister all filters for this channel */
	for (table_idx = 0; table_idx < TSPP_FILTER_TABLES; table_idx++) {
		for (i = 0; i < TSPP_NUM_PRIORITIES; i++) {
			struct tspp_pid_filter *filter =
				&pdev->filters[table_idx]->filter[i];
			id = FILTER_GET_PIPE_NUMBER0(filter);
			if (id == channel->id) {
				if (FILTER_HAS_ENCRYPTION(filter))
					tspp_free_key_entry(
						FILTER_GET_KEY_NUMBER(filter));
				filter->config = 0;
				filter->filter = 0;
			}
		}
	}
	channel->filter_count = 0;

	/* disconnect the bam */
	if (sps_disconnect(channel->pipe) != 0)
		pr_warn("%s: Error freeing sps endpoint (%d)\n",
			__func__, channel->id);

	/* destroy the buffers */
	dma_free_coherent(&pdev->pdev->dev, config->desc.size,
		config->desc.base, config->desc.phys_base);

	sps_free_endpoint(channel->pipe);

	tspp_destroy_buffers(channel_id, channel);

	dma_pool_destroy(channel->dma_pool);
	channel->dma_pool = NULL;

	channel->src = TSPP_SOURCE_NONE;
	channel->mode = TSPP_MODE_DISABLED;
	channel->memfree = NULL;
	channel->user_info = NULL;
	channel->buffer_count = 0;
	channel->data = NULL;
	channel->read = NULL;
	channel->locked = NULL;

	if (tspp_channels_in_use(pdev) == 0) {
		sps_deregister_bam_device(pdev->bam_handle);
		pdev->bam_handle = SPS_DEV_HANDLE_INVALID;

		__pm_relax(&pdev->ws);
		tspp_clock_stop(pdev);
	}

	pm_runtime_put(&pdev->pdev->dev);

	return 0;
}
EXPORT_SYMBOL(tspp_close_channel);

/**
 * tspp_get_ref_clk_counter - return the TSIF clock reference (TCR) counter.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @source: The TSIF source from which the counter should be read
 * @tcr_counter: the value of TCR counter
 *
 * Return  error status
 *
 * TCR increments at a rate equal to 27 MHz/256 = 105.47 kHz.
 * If source is neither TSIF 0 or TSIF1 0 is returned.
 */
int tspp_get_ref_clk_counter(u32 dev, enum tspp_source source, u32 *tcr_counter)
{
	struct tspp_device *pdev;
	struct tspp_tsif_device *tsif_device;

	if (!tcr_counter)
		return -EINVAL;

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	switch (source) {
	case TSPP_SOURCE_TSIF0:
		tsif_device = &pdev->tsif[0];
		break;

	case TSPP_SOURCE_TSIF1:
		tsif_device = &pdev->tsif[1];
		break;

	default:
		tsif_device = NULL;
		break;
	}

	if (tsif_device && tsif_device->ref_count)
		*tcr_counter = ioread32(tsif_device->base + TSIF_CLK_REF_OFF);
	else
		*tcr_counter = 0;

	return 0;
}
EXPORT_SYMBOL(tspp_get_ref_clk_counter);

/**
 * tspp_get_lpass_time_counter - return the LPASS  Timer counter value.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @source: The TSIF source from which the counter should be read
 * @tcr_counter: the value of TCR counter
 *
 * Return  error status
 *
 * If source is neither TSIF 0 or TSIF1 0 is returned.
 */
int tspp_get_lpass_time_counter(u32 dev, enum tspp_source source,
			u64 *lpass_time_counter)
{
	return -EPERM;
}
EXPORT_SYMBOL(tspp_get_lpass_time_counter);

/**
 * tspp_get_tts_source - Return the TTS source value.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @tts_source:Updated TTS source type
 *
 * Return  error status
 *
 */
int tspp_get_tts_source(u32 dev, int *tts_source)
{
	struct tspp_device *pdev;

	if (tts_source == NULL)
		return -EINVAL;

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	*tts_source = pdev->tts_source;

	return 0;
}
EXPORT_SYMBOL(tspp_get_tts_source);

/**
 * tspp_add_filter - add a TSPP filter to a channel.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @filter: TSPP filter parameters
 *
 * Return  error status
 *
 */
int tspp_add_filter(u32 dev, u32 channel_id,
	struct tspp_filter *filter)
{
	int i, rc;
	int other_channel;
	int entry;
	u32 val, pid, enabled;
	struct tspp_device *pdev;
	struct tspp_pid_filter p;
	struct tspp_channel *channel;

	TSPP_DEBUG("%s: add filter\n");
	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	channel = &pdev->channels[channel_id];

	if (filter->source > TSPP_SOURCE_MEM) {
		pr_err("%s: invalid source\n", __func__);
		return -ENOSR;
	}

	if (filter->priority >= TSPP_NUM_PRIORITIES) {
		pr_err("%s: invalid filter priority\n", __func__);
		return -ENOSR;
	}

	channel->mode = filter->mode;
	/*
	 * if buffers are already allocated, verify they fulfil
	 * the alignment requirements.
	 */
	if ((channel->buffer_count > 0) &&
	   (!tspp_is_buffer_size_aligned(channel->buffer_size, channel->mode)))
		pr_warn("%s: buffers allocated with incorrect alignment\n",
			__func__);

	if (filter->mode == TSPP_MODE_PES) {
		for (i = 0; i < TSPP_NUM_PRIORITIES; i++) {
			struct tspp_pid_filter *tspp_filter =
				&pdev->filters[channel->src]->filter[i];
			pid = FILTER_GET_PIPE_PID((tspp_filter));
			enabled = FILTER_GET_PIPE_PROCESS0(tspp_filter);
			if (enabled && (pid == filter->pid)) {
				other_channel =
					FILTER_GET_PIPE_NUMBER0(tspp_filter);
				pr_err(
				  "%s: pid 0x%x already in use by channel %d\n",
				  __func__, filter->pid, other_channel);
				return -EBADSLT;
			}
		}
	}

	/* make sure this priority is not already in use */
	enabled = FILTER_GET_PIPE_PROCESS0(
		(&(pdev->filters[channel->src]->filter[filter->priority])));
	if (enabled) {
		pr_err("%s: filter priority %d source %d is already enabled\n",
				__func__, filter->priority, channel->src);
		return -ENOSR;
	}

	if (channel->mode == TSPP_MODE_PES) {
		/*
		 * if we are already processing in PES mode, disable pipe
		 * (channel) and filter to be updated
		 */
		val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
		writel_relaxed(val | (1 << channel->id),
			pdev->base + TSPP_PS_DISABLE);
		/* Assure PS_DISABLE register is set */
		wmb();
	}

	/* update entry */
	p.filter = 0;
	p.config = FILTER_TRANS_END_DISABLE;
	FILTER_SET_PIPE_PROCESS0((&p), filter->mode);
	FILTER_SET_PIPE_PID((&p), filter->pid);
	FILTER_SET_PID_MASK((&p), filter->mask);
	FILTER_SET_PIPE_NUMBER0((&p), channel->id);
	FILTER_SET_PIPE_PROCESS1((&p), TSPP_MODE_DISABLED);
	if (filter->decrypt) {
		entry = tspp_get_key_entry();
		if (entry == -1) {
			pr_err("%s: no more keys available\n", __func__);
		} else {
			p.config |= FILTER_DECRYPT;
			FILTER_SET_KEY_NUMBER((&p), entry);
		}
	}

	pdev->filters[channel->src]->filter[filter->priority].config = p.config;
	pdev->filters[channel->src]->filter[filter->priority].filter = p.filter;

	/*
	 * allocate buffers if needed (i.e. if user did has not already called
	 * tspp_allocate_buffers() explicitly).
	 */
	if (channel->buffer_count == 0) {
		channel->buffer_size =
		tspp_align_buffer_size_by_mode(channel->buffer_size,
							channel->mode);
		rc = tspp_allocate_buffers(dev, channel->id,
					channel->max_buffers,
					channel->buffer_size,
					channel->int_freq, NULL, NULL, NULL);
		if (rc != 0) {
			pr_err("%s: tspp_allocate_buffers failed\n", __func__);
			return rc;
		}
	}

	/* reenable pipe */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val & ~(1 << channel->id), pdev->base + TSPP_PS_DISABLE);
	/* Assure PS_DISABLE register is reset */
	wmb();
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);

	channel->filter_count++;

	return 0;
}
EXPORT_SYMBOL(tspp_add_filter);

/**
 * tspp_remove_filter - remove a TSPP filter from a channel.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @filter: TSPP filter parameters
 *
 * Return  error status
 *
 */
int tspp_remove_filter(u32 dev, u32 channel_id,
	struct tspp_filter *filter)
{
	int entry;
	u32 val;
	struct tspp_device *pdev;
	int src;
	struct tspp_pid_filter *tspp_filter;
	struct tspp_channel *channel;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
			__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	if (!filter) {
		pr_err("%s: NULL filter pointer\n", __func__);
		return -EINVAL;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	if (filter->priority >= TSPP_NUM_PRIORITIES) {
		pr_err("%s: invalid filter priority\n", __func__);
		return -ENOSR;
	}
	channel = &pdev->channels[channel_id];

	src = channel->src;
	if ((src == TSPP_SOURCE_TSIF0) || (src == TSPP_SOURCE_TSIF1))
		tspp_filter = &(pdev->filters[src]->filter[filter->priority]);
	else {
		pr_err("%s: wrong source type %d\n", __func__, src);
		return -EINVAL;
	}


	/* disable pipe (channel) */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val | channel->id, pdev->base + TSPP_PS_DISABLE);
	/* Assure PS_DISABLE register is set */
	wmb();

	/* update data keys */
	if (tspp_filter->config & FILTER_DECRYPT) {
		entry = FILTER_GET_KEY_NUMBER(tspp_filter);
		tspp_free_key_entry(entry);
	}

	/* update pid table */
	tspp_filter->config = 0;
	tspp_filter->filter = 0;

	channel->filter_count--;

	/* reenable pipe */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val & ~(1 << channel->id),
		pdev->base + TSPP_PS_DISABLE);
	/* Assure PS_DISABLE register is reset */
	wmb();
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);

	return 0;
}
EXPORT_SYMBOL(tspp_remove_filter);

/**
 * tspp_set_key - set TSPP key in key table.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @key: TSPP key parameters
 *
 * Return  error status
 *
 */
int tspp_set_key(u32 dev, u32 channel_id, struct tspp_key *key)
{
	int i;
	int id;
	int key_index;
	int data;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	/* read the key index used by this channel */
	for (i = 0; i < TSPP_NUM_PRIORITIES; i++) {
		struct tspp_pid_filter *tspp_filter =
			&(pdev->filters[channel->src]->filter[i]);
		id = FILTER_GET_PIPE_NUMBER0(tspp_filter);
		if (id == channel->id) {
			if (FILTER_HAS_ENCRYPTION(tspp_filter)) {
				key_index = FILTER_GET_KEY_NUMBER(tspp_filter);
				break;
			}
		}
	}
	if (i == TSPP_NUM_PRIORITIES) {
		pr_err("%s: no encryption on this channel\n");
		return -ENOKEY;
	}

	if (key->parity == TSPP_KEY_PARITY_EVEN) {
		pdev->tspp_key_table->entry[key_index].even_lsb = key->lsb;
		pdev->tspp_key_table->entry[key_index].even_msb = key->msb;
	} else {
		pdev->tspp_key_table->entry[key_index].odd_lsb = key->lsb;
		pdev->tspp_key_table->entry[key_index].odd_msb = key->msb;
	}
	data = readl_relaxed(channel->pdev->base + TSPP_KEY_VALID);

	return 0;
}
EXPORT_SYMBOL(tspp_set_key);

/**
 * tspp_register_notification - register TSPP channel notification function.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @notify: notification function
 * @userdata: user data to pass to notification function
 * @timer_ms: notification for partially filled buffers
 *
 * Return  error status
 *
 */
int tspp_register_notification(u32 dev, u32 channel_id,
	tspp_notifier *notify, void *userdata, u32 timer_ms)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->notifier = notify;
	channel->notify_data = userdata;
	channel->expiration_period_ms = timer_ms;

	return 0;
}
EXPORT_SYMBOL(tspp_register_notification);

/**
 * tspp_unregister_notification - unregister TSPP channel notification function.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 *
 * Return  error status
 *
 */
int tspp_unregister_notification(u32 dev, u32 channel_id)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->notifier = NULL;
	channel->notify_data = 0;
	return 0;
}
EXPORT_SYMBOL(tspp_unregister_notification);

/**
 * tspp_get_buffer - get TSPP data buffer.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 *
 * Return  error status
 *
 */
const struct tspp_data_descriptor *tspp_get_buffer(u32 dev, u32 channel_id)
{
	struct tspp_mem_buffer *buffer;
	struct tspp_channel *channel;
	struct tspp_device *pdev;
	unsigned long flags;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return NULL;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return NULL;
	}

	spin_lock_irqsave(&pdev->spinlock, flags);

	channel = &pdev->channels[channel_id];

	if (!channel->read) {
		spin_unlock_irqrestore(&pdev->spinlock, flags);
		pr_warn("%s: no buffer to get on channel %d\n", __func__,
			channel->id);
		return NULL;
	}

	buffer = channel->read;
	/* see if we have any buffers ready to read */
	if (buffer->state != TSPP_BUF_STATE_DATA) {
		spin_unlock_irqrestore(&pdev->spinlock, flags);
		return NULL;
	}

	if (buffer->state == TSPP_BUF_STATE_DATA) {
		/* mark the buffer as busy */
		buffer->state = TSPP_BUF_STATE_LOCKED;

		/* increment the pointer along the list */
		channel->read = channel->read->next;
	}

	spin_unlock_irqrestore(&pdev->spinlock, flags);

	return &buffer->desc;
}
EXPORT_SYMBOL(tspp_get_buffer);

/**
 * tspp_release_buffer - release TSPP data buffer back to TSPP.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @descriptor_id: buffer descriptor ID
 *
 * Return  error status
 *
 */
int tspp_release_buffer(u32 dev, u32 channel_id, u32 descriptor_id)
{
	int i, found = 0;
	struct tspp_mem_buffer *buffer;
	struct tspp_channel *channel;
	struct tspp_device *pdev;
	unsigned long flags;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
				__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	spin_lock_irqsave(&pdev->spinlock, flags);

	channel = &pdev->channels[channel_id];

	if (descriptor_id > channel->buffer_count)
		pr_warn("%s: invalid desc id: 0x%08x\n",
			__func__, descriptor_id);

	/* find the correct descriptor */
	buffer = channel->locked;
	for (i = 0; i < channel->buffer_count; i++) {
		if (buffer->desc.id == descriptor_id) {
			found = 1;
			break;
		}
		buffer = buffer->next;
	}
	channel->locked = channel->locked->next;

	if (!found) {
		spin_unlock_irqrestore(&pdev->spinlock, flags);
		pr_err("%s: cant find desc %d\n", __func__, descriptor_id);
		return -EINVAL;
	}

	/* make sure the buffer is in the expected state */
	if (buffer->state != TSPP_BUF_STATE_LOCKED) {
		spin_unlock_irqrestore(&pdev->spinlock, flags);
		pr_err("%s: buffer %d not locked\n", __func__, descriptor_id);
		return -EINVAL;
	}
	/* unlock the buffer and requeue it */
	buffer->state = TSPP_BUF_STATE_WAITING;

	if (tspp_queue_buffer(channel, buffer))
		pr_warn("%s: can't requeue buffer\n", __func__);

	spin_unlock_irqrestore(&pdev->spinlock, flags);

	return 0;
}
EXPORT_SYMBOL(tspp_release_buffer);

/**
 * tspp_allocate_buffers - allocate TSPP data buffers.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @channel_id: Channel ID number (up to TSPP_NUM_CHANNELS)
 * @count: number of buffers to allocate
 * @size: size of each buffer to allocate
 * @int_freq: interrupt frequency
 * @alloc: user defined memory allocator function. Pass NULL for default.
 * @memfree: user defined memory free function. Pass NULL for default.
 * @user: user data to pass to the memory allocator/free function
 *
 * Return  error status
 *
 * The user can optionally call this function explicitly to allocate the TSPP
 * data buffers. Alternatively, if the user did not call this function, it
 * is called implicitly by tspp_add_filter().
 */
int tspp_allocate_buffers(u32 dev, u32 channel_id, u32 count, u32 size,
			u32 int_freq, tspp_allocator *alloc,
			tspp_memfree *memfree, void *user)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;
	struct tspp_mem_buffer *last = NULL;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("%s: channel id out of range[0, %d]\n",
			__func__, TSPP_NUM_CHANNELS);
		return -ECHRNG;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	if (count < MIN_ACCEPTABLE_BUFFER_COUNT) {
		pr_err("%s: tspp requires a minimum of %d buffers\n", __func__,
				MIN_ACCEPTABLE_BUFFER_COUNT);
		return -EINVAL;
	}

	if (count > TSPP_NUM_BUFFERS) {
		pr_err("%s: tspp requires a maximum of %d buffers\n", __func__,
				TSPP_NUM_BUFFERS);
		return -EINVAL;
	}

	channel = &pdev->channels[channel_id];

	/* allow buffer allocation only if there was no previous buffer
	 * allocation for this channel.
	 */
	if (channel->buffer_count > 0) {
		pr_err("%s: buffers already allocated for channel %u\n",
			__func__, channel_id);
		return -EINVAL;
	}

	channel->max_buffers = count;

	/* set up interrupt frequency */
	if (int_freq > channel->max_buffers) {
		int_freq = channel->max_buffers;
		pr_debug("%s: setting interrupt frequency to %u\n", __func__,
				int_freq);
	}
	channel->int_freq = int_freq;
	/*
	 * it is the responsibility of the caller to tspp_allocate_buffers(),
	 * whether it's the user or the driver, to make sure the size parameter
	 * is compatible to the channel mode.
	 */
	channel->buffer_size = size;

	/* save user defined memory free function for later use */
	channel->memfree = memfree;
	channel->user_info = user;

	/*
	 * For small buffers, create a DMA pool so that memory
	 * is not wasted through dma_alloc_coherent.
	 */
	if (TSPP_USE_DMA_POOL(channel->buffer_size)) {
		channel->dma_pool = dma_pool_create("tspp",
			&pdev->pdev->dev, channel->buffer_size, 0, 0);
		if (!channel->dma_pool) {
			pr_err("%s: Can't allocate memory pool\n", __func__);
			return -ENOMEM;
		}
	} else {
		channel->dma_pool = NULL;
	}


	for (channel->buffer_count = 0;
		channel->buffer_count < channel->max_buffers;
		channel->buffer_count++) {

		/* allocate the descriptor */
		struct tspp_mem_buffer *desc =
			kmalloc(sizeof(struct tspp_mem_buffer), GFP_KERNEL);
		if (!desc) {
			pr_warn("%s: Can't allocate desc %d\n",
				__func__, channel->buffer_count);
			break;
		}

		desc->desc.id = channel->buffer_count;
		/* allocate the buffer */
		if (tspp_alloc_buffer(channel_id, &desc->desc,
			channel->buffer_size, channel->dma_pool,
			alloc, user) != 0) {
			kfree(desc);
			pr_warn("%s: Can't allocate buffer %d\n",
				__func__, channel->buffer_count);
			break;
		}

		/* add the descriptor to the list */
		desc->filled = 0;
		desc->read_index = 0;
		if (!channel->data) {
			channel->data = desc;
			desc->next = channel->data;
		} else {
			if (last != NULL)
				last->next = desc;
		}
		last = desc;
		desc->next = channel->data;

		/* prepare the sps descriptor */
		desc->sps.phys_base = desc->desc.phys_base;
		desc->sps.base = desc->desc.virt_base;
		desc->sps.size = desc->desc.size;

		/* start the transfer */
		if (tspp_queue_buffer(channel, desc))
			pr_err("%s: can't queue buffer %d\n", __func__,
					desc->desc.id);
	}

	if (channel->buffer_count < channel->max_buffers) {
		/*
		 * we failed to allocate the requested number of buffers.
		 * we don't allow a partial success, so need to clean up here.
		 */
		tspp_destroy_buffers(channel_id, channel);
		channel->buffer_count = 0;

		dma_pool_destroy(channel->dma_pool);
		channel->dma_pool = NULL;
		return -ENOMEM;
	}

	channel->waiting = channel->data;
	channel->read = channel->data;
	channel->locked = channel->data;

	/* Now that buffers are scheduled to HW, kick data expiration timer */
	if (channel->expiration_period_ms)
		mod_timer(&channel->expiration_timer,
			jiffies +
			msecs_to_jiffies(
				channel->expiration_period_ms));

	return 0;
}
EXPORT_SYMBOL(tspp_allocate_buffers);

/**
 * tspp_detach_ion_dma_buff - detach the mapped ion dma buffer from TSPP device
 * It will detach previously mapped DMA buffer from TSPP device.
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @ion_dma_buf: It contains required members for ION buffer dma mapping.
 *
 * Return  error status
 *
 */
int tspp_detach_ion_dma_buff(u32 dev, struct tspp_ion_dma_buf_info *ion_dma_buf)
{
	struct tspp_device *pdev;
	int dir = DMA_FROM_DEVICE;

	if (ion_dma_buf == NULL || ion_dma_buf->dbuf == NULL ||
	    ion_dma_buf->table == NULL || ion_dma_buf->table->sgl == NULL ||
	    ion_dma_buf->smmu_map == false) {
		pr_err("%s: invalid input argument\n", __func__);
		return -EINVAL;
	}

	if (dev >= TSPP_MAX_DEVICES) {
		pr_err("%s: device id out of range[0, %d]\n",
				__func__, TSPP_MAX_DEVICES);
		return -ENODEV;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}


	dma_unmap_sg(&pdev->pdev->dev, ion_dma_buf->table->sgl,
			ion_dma_buf->table->nents, dir);
	dma_buf_unmap_attachment(ion_dma_buf->attach, ion_dma_buf->table, dir);
	dma_buf_detach(ion_dma_buf->dbuf, ion_dma_buf->attach);
	dma_buf_put(ion_dma_buf->dbuf);

	ion_dma_buf->smmu_map = false;
	return 0;
}
EXPORT_SYMBOL(tspp_detach_ion_dma_buff);

/**
 * tspp_allocate_dma_buffer - Allocates DMA buffer using dma_alloc_coherent
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @size: Size of memory to be allocated
 * @paddr: Returns the physical address of the allocated memory
 *
 * Return  error status
 *
 */
void *tspp_allocate_dma_buffer(u32 dev, int size, phys_addr_t *paddr)
{

	struct tspp_device *pdev;

	if (dev >= TSPP_MAX_DEVICES) {
		pr_err("%s: device id out of range[0, %d]\n",
				__func__, TSPP_MAX_DEVICES);
		return NULL;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return NULL;
	}

	return  dma_alloc_coherent(&pdev->pdev->dev,
			size, paddr, GFP_KERNEL);
}
EXPORT_SYMBOL(tspp_allocate_dma_buffer);

/**
 * tspp_free_dma_buffer - Free the allocated memmory
 *
 * @dev: TSPP device (up to TSPP_MAX_DEVICES)
 * @size: Size of allocated memory
 * @vaddr: Virtual address of the allocated memory
 * @paddr: Physical address of the allocated memory
 *
 * Return  error status
 *
 */
int tspp_free_dma_buffer(u32 dev, int size, void *vaddr, dma_addr_t paddr)
{

	struct tspp_device *pdev;

	if (dev >= TSPP_MAX_DEVICES) {
		pr_err("%s: device id out of range[0, %d]\n",
				__func__, TSPP_MAX_DEVICES);
		return -ENODEV;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, dev);
		return -ENODEV;
	}

	dma_free_coherent(&pdev->pdev->dev,
			size, vaddr, paddr);

	return 0;
}
EXPORT_SYMBOL(tspp_free_dma_buffer);


/*** debugfs ***/
static int debugfs_iomem_x32_set(void *data, u64 val)
{
	int rc;
	int clock_started = 0;
	struct tspp_device *pdev;

	pdev = tspp_find_by_id(0);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, 0);
		return 0;
	}

	if (tspp_channels_in_use(pdev) == 0) {
		rc = tspp_clock_start(pdev);
		if (rc) {
			pr_err("%s: tspp_clock_start failed %d\n",
					__func__, rc);
			return 0;
		}
		clock_started = 1;
	}

	writel_relaxed(val, data);
	/* Assure register write */
	wmb();

	if (clock_started)
		tspp_clock_stop(pdev);
	return 0;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	int rc;
	int clock_started = 0;
	struct tspp_device *pdev;

	pdev = tspp_find_by_id(0);
	if (!pdev) {
		pr_err("%s: can't find device %d\n", __func__, 0);
		*val = 0;
		return 0;
	}

	if (tspp_channels_in_use(pdev) == 0) {
		rc = tspp_clock_start(pdev);
		if (rc) {
			pr_err("%s: tspp_clock_start failed %d\n",
					__func__, rc);
			*val = 0;
			return 0;
		}
		clock_started = 1;
	}

	*val = readl_relaxed(data);

	if (clock_started)
		tspp_clock_stop(pdev);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llx");

static void tsif_debugfs_init(struct tspp_tsif_device *tsif_device,
	int instance)
{
	char name[10];

	snprintf(name, sizeof(name), "tsif%d", instance);
	tsif_device->dent_tsif = debugfs_create_dir(
	      name, NULL);
	if (tsif_device->dent_tsif) {
		int i;
		void __iomem *base = tsif_device->base;

		for (i = 0; i < ARRAY_SIZE(debugfs_tsif_regs); i++) {
			tsif_device->debugfs_tsif_regs[i] =
			   debugfs_create_file(
				debugfs_tsif_regs[i].name,
				debugfs_tsif_regs[i].mode,
				tsif_device->dent_tsif,
				base + debugfs_tsif_regs[i].offset,
				&fops_iomem_x32);
		}

		debugfs_create_u32(
			"stat_rx_chunks", 0664,
			tsif_device->dent_tsif,
			&tsif_device->stat_rx);

		debugfs_create_u32(
			"stat_overflow", 0664,
			tsif_device->dent_tsif,
			&tsif_device->stat_overflow);

		debugfs_create_u32(
			"stat_lost_sync", 0664,
			tsif_device->dent_tsif,
			&tsif_device->stat_lost_sync);

		debugfs_create_u32(
			"stat_timeout", 0664,
			tsif_device->dent_tsif,
			&tsif_device->stat_timeout);
	}
}

static void tsif_debugfs_exit(struct tspp_tsif_device *tsif_device)
{
	int i;

	debugfs_remove_recursive(tsif_device->dent_tsif);
	tsif_device->dent_tsif = NULL;
	for (i = 0; i < ARRAY_SIZE(debugfs_tsif_regs); i++)
		tsif_device->debugfs_tsif_regs[i] = NULL;
}

static void tspp_debugfs_init(struct tspp_device *device, int instance)
{
	char name[10];

	snprintf(name, sizeof(name), "tspp%d", instance);
	device->dent = debugfs_create_dir(
	      name, NULL);
	if (device->dent) {
		int i;
		void __iomem *base = device->base;

		for (i = 0; i < ARRAY_SIZE(debugfs_tspp_regs); i++)
			device->debugfs_regs[i] =
			   debugfs_create_file(
				debugfs_tspp_regs[i].name,
				debugfs_tspp_regs[i].mode,
				device->dent,
				base + debugfs_tspp_regs[i].offset,
				&fops_iomem_x32);
	}
}

static void tspp_debugfs_exit(struct tspp_device *device)
{
	int i;

	debugfs_remove_recursive(device->dent);
	for (i = 0; i < ARRAY_SIZE(debugfs_tspp_regs); i++)
		device->debugfs_regs[i] = NULL;
}

static int msm_tspp_map_irqs(struct platform_device *pdev,
				struct tspp_device *device)
{
	int rc;

	/* get IRQ numbers from platform information */

	/* map TSPP IRQ */
	rc = platform_get_irq_byname(pdev, "TSIF_TSPP_IRQ");
	if (rc > 0) {
		device->tspp_irq = rc;
	} else {
		pr_err("%s: failed to get %s\n", __func__, "TSPP IRQ");
		return -EINVAL;
	}

	/* map TSIF IRQs */
	rc = platform_get_irq_byname(pdev, "TSIF0_IRQ");
	if (rc > 0) {
		device->tsif[0].tsif_irq = rc;
	} else {
		pr_err("%s: failed to get %s\n", __func__, "TSIF0 IRQ");
		return -EINVAL;
	}

	rc = platform_get_irq_byname(pdev, "TSIF1_IRQ");
	if (rc > 0) {
		device->tsif[1].tsif_irq = rc;
	} else {
		pr_err("%s: failed to get %s\n", __func__, "TSIF1 IRQ");
		return -EINVAL;
	}

	/* map BAM IRQ */
	rc = platform_get_irq_byname(pdev, "TSIF_BAM_IRQ");
	if (rc > 0) {
		device->bam_irq = rc;
	} else {
		pr_err("%s: failed to get %s\n", __func__, "TSPP BAM IRQ");
		return -EINVAL;
	}

	return 0;
}

static int msm_tspp_probe(struct platform_device *pdev)
{
	int rc = -ENODEV;
	u32 version;
	u32 i;
	struct tspp_device *device;
	struct resource *mem_tsif0;
	struct resource *mem_tsif1;
	struct resource *mem_tspp;
	struct resource *mem_bam;
	struct msm_bus_scale_pdata *tspp_bus_pdata = NULL;
	unsigned long rate;

	if (pdev->dev.of_node) {
		/* ID is always 0 since there is only 1 instance of TSPP */
		pdev->id = 0;
		tspp_bus_pdata = msm_bus_cl_get_pdata(pdev);
	} else {
		/* must have device tree data */
		pr_err("%s: Device tree data not available\n", __func__);
		rc = -EINVAL;
		goto out;
	}

	/* OK, we will use this device */
	device = kzalloc(sizeof(*device), GFP_KERNEL);
	if (!device) {
		rc = -ENOMEM;
		goto out;
	}

	/* set up references */
	device->pdev = pdev;
	platform_set_drvdata(pdev, device);

	/* setup pin control */
	rc = tspp_get_pinctrl(device);
	if (rc) {
		pr_err("%s: failed to get pin control data, rc=%d\n",
			__func__, rc);
		goto err_pinctrl;
	}

	/* register bus client */
	if (tspp_bus_pdata) {
		device->tsif_bus_client =
			msm_bus_scale_register_client(tspp_bus_pdata);
		if (!device->tsif_bus_client)
			pr_err("%s: Unable to register bus client\n", __func__);
	} else {
		device->tsif_bus_client = 0;
	}

	/* map regulators */
	device->tsif_vreg = devm_regulator_get_optional(&pdev->dev, "vdd_cx");
	if (IS_ERR_OR_NULL(device->tsif_vreg)) {
		rc = PTR_ERR(device->tsif_vreg);
		device->tsif_vreg = NULL;
		if (rc == -ENODEV) {
			pr_notice("%s: vdd_cx regulator will not be used\n",
				__func__);
		} else {
			pr_err("%s: failed to get CX regulator, err=%d\n",
				__func__, rc);
			goto err_regulator;
		}
	} else {
		/* Set an initial voltage and enable the regulator */
		rc = regulator_set_voltage(device->tsif_vreg,
					0,
					RPMH_REGULATOR_LEVEL_MAX);
		if (rc) {
			pr_err("%s: Unable to set CX voltage\n", __func__);
			goto err_regulator;
		}

		rc = regulator_enable(device->tsif_vreg);
		if (rc) {
			pr_err("%s: Unable to enable CX regulator\n", __func__);
			goto err_regulator;
		}
	}

	/* map clocks */
	device->tsif_pclk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR_OR_NULL(device->tsif_pclk)) {
		rc = PTR_ERR(device->tsif_pclk);
		device->tsif_pclk = NULL;
		goto err_pclock;
	}

	device->tsif_ref_clk = clk_get(&pdev->dev, "ref_clk");
	if (IS_ERR_OR_NULL(device->tsif_ref_clk)) {
		rc = PTR_ERR(device->tsif_ref_clk);
		device->tsif_ref_clk = NULL;
		goto err_refclock;
	}
	rate = clk_round_rate(device->tsif_ref_clk, 1);
	rc = clk_set_rate(device->tsif_ref_clk, rate);
	if (rc)
		goto err_res_tsif0;

	/* map I/O memory */
	mem_tsif0 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSIF0_PHYS");
	if (!mem_tsif0) {
		pr_err("%s: Missing tsif0 MEM resource\n", __func__);
		rc = -ENXIO;
		goto err_res_tsif0;
	}
	device->tsif[0].base = ioremap(mem_tsif0->start,
		resource_size(mem_tsif0));
	if (!device->tsif[0].base) {
		pr_err("%s: ioremap failed\n", __func__);
		goto err_map_tsif0;
	}

	mem_tsif1 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSIF1_PHYS");
	if (!mem_tsif1) {
		pr_err("%s: missing tsif1 MEM resource\n", __func__);
		rc = -ENXIO;
		goto err_res_tsif1;
	}
	device->tsif[1].base = ioremap(mem_tsif1->start,
		resource_size(mem_tsif1));
	if (!device->tsif[1].base) {
		pr_err("%s: ioremap failed\n", __func__);
		goto err_map_tsif1;
	}

	mem_tspp = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSPP_PHYS");
	if (!mem_tspp) {
		pr_err("%s: missing MEM resource\n", __func__);
		rc = -ENXIO;
		goto err_res_dev;
	}
	device->base = ioremap(mem_tspp->start, resource_size(mem_tspp));
	if (!device->base) {
		pr_err("%s: ioremap failed\n", __func__);
		goto err_map_dev;
	}

	mem_bam = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_TSPP_BAM_PHYS");
	if (!mem_bam) {
		pr_err("%s: Missing bam MEM resource\n", __func__);
		rc = -ENXIO;
		goto err_res_bam;
	}
	memset(&device->bam_props, 0, sizeof(device->bam_props));
	device->bam_props.phys_addr = mem_bam->start;
	device->bam_props.virt_addr = ioremap(mem_bam->start,
		resource_size(mem_bam));
	if (!device->bam_props.virt_addr) {
		pr_err("%s: ioremap failed\n", __func__);
		goto err_map_bam;
	}

	if (msm_tspp_map_irqs(pdev, device))
		goto err_irq;
	device->req_irqs = false;

	device->tts_source = TSIF_TTS_TCR;
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		device->tsif[i].tts_source = device->tts_source;

	/* power management */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	tspp_debugfs_init(device, 0);

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_init(&device->tsif[i], i);

	//wakeup_source_init(&device->ws, dev_name(&pdev->dev));

	/* set up pointers to ram-based 'registers' */
	device->filters[0] = device->base + TSPP_PID_FILTER_TABLE0;
	device->filters[1] = device->base + TSPP_PID_FILTER_TABLE1;
	device->filters[2] = device->base + TSPP_PID_FILTER_TABLE2;
	device->tspp_key_table = device->base + TSPP_DATA_KEY;
	device->tspp_global_performance =
		device->base + TSPP_GLOBAL_PERFORMANCE;
	device->tspp_pipe_context =
		device->base + TSPP_PIPE_CONTEXT;
	device->tspp_pipe_performance =
		device->base + TSPP_PIPE_PERFORMANCE;

	device->bam_props.summing_threshold = 0x10;
	device->bam_props.irq = device->bam_irq;
	device->bam_props.manage = SPS_BAM_MGR_LOCAL;
	/*add SPS BAM log level*/
	device->bam_props.ipc_loglevel = TSPP_BAM_DEFAULT_IPC_LOGLVL;

	if (tspp_clock_start(device) != 0) {
		pr_err("%s: Can't start clocks\n", __func__);
		goto err_clock;
	}

	device->bam_handle = SPS_DEV_HANDLE_INVALID;

	spin_lock_init(&device->spinlock);
	mutex_init(&device->mutex);
	tasklet_init(&device->tlet, tspp_sps_complete_tlet,
			(unsigned long)device);

	/* initialize everything to a known state */
	tspp_global_reset(device);

	version = readl_relaxed(device->base + TSPP_VERSION);
	/*
	 * TSPP version can be bits [7:0] or alternatively,
	 * TSPP major version is bits [31:28].
	 */
	if ((version != 0x1) && (((version >> 28) & 0xF) != 0x1))
		pr_err("%s: unrecognized hw version=%d\n", __func__, version);

	/* initialize the channels */
	for (i = 0; i < TSPP_NUM_CHANNELS; i++)
		tspp_channel_init(&(device->channels[i]), device);

	/* stop the clocks for power savings */
	tspp_clock_stop(device);

	/* everything is ok, so add the device to the list */
	list_add_tail(&(device->devlist), &tspp_devices);
	return 0;

err_clock:
	tspp_debugfs_exit(device);
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_exit(&device->tsif[i]);

	tspp_iommu_release_iomapping(device);
err_irq:
	iounmap(device->bam_props.virt_addr);
err_map_bam:
err_res_bam:
	iounmap(device->base);
err_map_dev:
err_res_dev:
	iounmap(device->tsif[1].base);
err_map_tsif1:
err_res_tsif1:
	iounmap(device->tsif[0].base);
err_map_tsif0:
err_res_tsif0:
	if (device->tsif_ref_clk)
		clk_put(device->tsif_ref_clk);
err_refclock:
	if (device->tsif_pclk)
		clk_put(device->tsif_pclk);
err_pclock:
	if (device->tsif_vreg)
		regulator_disable(device->tsif_vreg);
err_regulator:
	if (device->tsif_bus_client)
		msm_bus_scale_unregister_client(device->tsif_bus_client);
err_pinctrl:
	kfree(device);

out:
	return rc;
}

static int msm_tspp_remove(struct platform_device *pdev)
{
	struct tspp_channel *channel;
	u32 i;

	struct tspp_device *device = platform_get_drvdata(pdev);

	/* free the buffers, and delete the channels */
	for (i = 0; i < TSPP_NUM_CHANNELS; i++) {
		channel = &device->channels[i];
		tspp_close_channel(device->pdev->id, i);
	}

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_exit(&device->tsif[i]);

	mutex_destroy(&device->mutex);

	if (device->tsif_bus_client)
		msm_bus_scale_unregister_client(device->tsif_bus_client);

	//wakeup_source_trash(&device->ws);
	if (device->req_irqs)
		msm_tspp_free_irqs(device);

	iounmap(device->bam_props.virt_addr);
	iounmap(device->base);
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		iounmap(device->tsif[i].base);

	if (device->tsif_ref_clk)
		clk_put(device->tsif_ref_clk);

	if (device->tsif_pclk)
		clk_put(device->tsif_pclk);

	if (device->tsif_vreg)
		regulator_disable(device->tsif_vreg);

	tspp_iommu_release_iomapping(device);

	pm_runtime_disable(&pdev->dev);

	kfree(device);

	return 0;
}

/*** power management ***/

static int tspp_runtime_suspend(struct device *dev)
{
	pr_debug("%s: pm_runtime: suspending\n", __func__);
	return 0;
}

static int tspp_runtime_resume(struct device *dev)
{
	pr_debug("%s: pm_runtime: resuming\n", __func__);
	return 0;
}

static const struct dev_pm_ops tspp_dev_pm_ops = {
	.runtime_suspend = tspp_runtime_suspend,
	.runtime_resume = tspp_runtime_resume,
};

static const struct of_device_id msm_match_table[] = {
	{.compatible = "qcom,msm_tspp"},
	{},
};

static struct platform_driver msm_tspp_driver = {
	.probe          = msm_tspp_probe,
	.remove         = msm_tspp_remove,
	.driver         = {
		.name   = "msm_tspp",
		.pm     = &tspp_dev_pm_ops,
		.of_match_table = msm_match_table,
	},
};


static int __init mod_init(void)
{
	int rc;

	/* register the driver, and check hardware */
	rc = platform_driver_register(&msm_tspp_driver);
	if (rc)
		pr_err("%s: platform_driver_register failed: %d\n",
			__func__, rc);

	return rc;
}

static void __exit mod_exit(void)
{
	/* delete low level driver */
	platform_driver_unregister(&msm_tspp_driver);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("TSPP platform device");
MODULE_LICENSE("GPL v2");
