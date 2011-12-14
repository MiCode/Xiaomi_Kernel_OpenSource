/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/dma-mapping.h>   /* dma_XXX */
#include <linux/delay.h>         /* msleep */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/poll.h>          /* poll() file op */
#include <linux/wait.h>          /* wait() macros, sleeping */
#include <linux/tspp.h>          /* tspp functions */
#include <linux/bitops.h>        /* BIT() macro */
#include <mach/sps.h>            /* BAM stuff */
#include <mach/gpio.h>
#include <linux/wakelock.h>      /* Locking functions */
#include <mach/dma.h>
#include <mach/msm_tspp.h>
#include <linux/debugfs.h>

/*
 * General defines
 */
#define TSPP_TSIF_INSTANCES            2
#define TSPP_FILTER_TABLES             3
#define TSPP_MAX_DEVICES               1
#define TSPP_NUM_CHANNELS              16
#define TSPP_NUM_PRIORITIES            16
#define TSPP_NUM_KEYS                  8
#define INVALID_CHANNEL                0xFFFFFFFF
#define TSPP_SPS_DESCRIPTOR_COUNT      128
#define TSPP_PACKET_LENGTH             188
#define TSPP_MIN_BUFFER_SIZE           (TSPP_PACKET_LENGTH)
#define TSPP_MAX_BUFFER_SIZE           (32 * 1024)
#define TSPP_NUM_BUFFERS               64
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

/*
 * TSPP register offsets
 */
#define TSPP_RST					0x00
#define TSPP_CLK_CONTROL		0x04
#define TSPP_CONFIG				0x08
#define TSPP_CONTROL				0x0C
#define TSPP_PS_DISABLE			0x10
#define TSPP_MSG_IRQ_STATUS	0x14
#define TSPP_MSG_IRQ_MASK		0x18
#define TSPP_IRQ_STATUS			0x1C
#define TSPP_IRQ_MASK			0x20
#define TSPP_IRQ_CLEAR			0x24
#define TSPP_PIPE_ERROR_STATUS(_n)	(0x28 + (_n << 2))
#define TSPP_STATUS				0x68
#define TSPP_CURR_TSP_HEADER	0x6C
#define TSPP_CURR_PID_FILTER	0x70
#define TSPP_SYSTEM_KEY(_n)	(0x74 + (_n << 2))
#define TSPP_CBC_INIT_VAL(_n)	(0x94 + (_n << 2))
#define TSPP_DATA_KEY_RESET	0x9C
#define TSPP_KEY_VALID			0xA0
#define TSPP_KEY_ERROR			0xA4
#define TSPP_TEST_CTRL			0xA8
#define TSPP_VERSION				0xAC
#define TSPP_GENERICS			0xB0
#define TSPP_NOP					0xB4

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
#define TSPP_IRQ_STATUS_TSP_RD_CMPL			BIT(19)
#define TSPP_IRQ_STATUS_KEY_ERROR			BIT(18)
#define TSPP_IRQ_STATUS_KEY_SWITCHED_BAD	BIT(17)
#define TSPP_IRQ_STATUS_KEY_SWITCHED		BIT(16)
#define TSPP_IRQ_STATUS_PS_BROKEN(_n)		BIT((_n))

/* TSPP_PIPE_ERROR_STATUS */
#define TSPP_PIPE_PES_SYNC_ERROR				BIT(3)
#define TSPP_PIPE_PS_LENGTH_ERROR			BIT(2)
#define TSPP_PIPE_PS_CONTINUITY_ERROR		BIT(1)
#define TSPP_PIP_PS_LOST_START				BIT(0)

/* TSPP_STATUS			*/
#define TSPP_STATUS_TSP_PKT_AVAIL			BIT(10)
#define TSPP_STATUS_TSIF1_DM_REQ				BIT(6)
#define TSPP_STATUS_TSIF0_DM_REQ				BIT(2)
#define TSPP_CURR_FILTER_TABLE				BIT(0)

/* TSPP_GENERICS		*/
#define TSPP_GENERICS_CRYPTO_GEN				BIT(12)
#define TSPP_GENERICS_MAX_CONS_PIPES		BIT(7)
#define TSPP_GENERICS_MAX_PIPES				BIT(2)
#define TSPP_GENERICS_TSIF_1_GEN				BIT(1)
#define TSPP_GENERICS_TSIF_0_GEN				BIT(0)

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
	{"sts_ctl",             S_IRUGO | S_IWUSR, TSIF_STS_CTL_OFF},
	{"time_limit",          S_IRUGO | S_IWUSR, TSIF_TIME_LIMIT_OFF},
	{"clk_ref",             S_IRUGO | S_IWUSR, TSIF_CLK_REF_OFF},
	{"lpbk_flags",          S_IRUGO | S_IWUSR, TSIF_LPBK_FLAGS_OFF},
	{"lpbk_data",           S_IRUGO | S_IWUSR, TSIF_LPBK_DATA_OFF},
	{"test_ctl",            S_IRUGO | S_IWUSR, TSIF_TEST_CTL_OFF},
	{"test_mode",           S_IRUGO | S_IWUSR, TSIF_TEST_MODE_OFF},
	{"test_reset",                    S_IWUSR, TSIF_TEST_RESET_OFF},
	{"test_export",         S_IRUGO | S_IWUSR, TSIF_TEST_EXPORT_OFF},
	{"test_current",        S_IRUGO,           TSIF_TEST_CURRENT_OFF},
	{"data_port",           S_IRUSR,           TSIF_DATA_PORT_OFF},
};

static const struct debugfs_entry debugfs_tspp_regs[] = {
	{"rst",                 S_IRUGO | S_IWUSR, TSPP_RST},
	{"clk_control",         S_IRUGO | S_IWUSR, TSPP_CLK_CONTROL},
	{"config",              S_IRUGO | S_IWUSR, TSPP_CONFIG},
	{"control",             S_IRUGO | S_IWUSR, TSPP_CONTROL},
	{"ps_disable",          S_IRUGO | S_IWUSR, TSPP_PS_DISABLE},
	{"msg_irq_status",      S_IRUGO | S_IWUSR, TSPP_MSG_IRQ_STATUS},
	{"msg_irq_mask",        S_IRUGO | S_IWUSR, TSPP_MSG_IRQ_MASK},
	{"irq_status",          S_IRUGO | S_IWUSR, TSPP_IRQ_STATUS},
	{"irq_mask",            S_IRUGO | S_IWUSR, TSPP_IRQ_MASK},
	{"irq_clear",           S_IRUGO | S_IWUSR, TSPP_IRQ_CLEAR},
	/* {"pipe_error_status",S_IRUGO | S_IWUSR, TSPP_PIPE_ERROR_STATUS}, */
	{"status",              S_IRUGO | S_IWUSR, TSPP_STATUS},
	{"curr_tsp_header",     S_IRUGO | S_IWUSR, TSPP_CURR_TSP_HEADER},
	{"curr_pid_filter",     S_IRUGO | S_IWUSR, TSPP_CURR_PID_FILTER},
	/* {"system_key",       S_IRUGO | S_IWUSR, TSPP_SYSTEM_KEY}, */
	/* {"cbc_init_val",     S_IRUGO | S_IWUSR, TSPP_CBC_INIT_VAL}, */
	{"data_key_reset",      S_IRUGO | S_IWUSR, TSPP_DATA_KEY_RESET},
	{"key_valid",           S_IRUGO | S_IWUSR, TSPP_KEY_VALID},
	{"key_error",           S_IRUGO | S_IWUSR, TSPP_KEY_ERROR},
	{"test_ctrl",           S_IRUGO | S_IWUSR, TSPP_TEST_CTRL},
	{"version",             S_IRUGO | S_IWUSR, TSPP_VERSION},
	{"generics",            S_IRUGO | S_IWUSR, TSPP_GENERICS},
	{"pid_filter_table0",   S_IRUGO | S_IWUSR, TSPP_PID_FILTER_TABLE0},
	{"pid_filter_table1",   S_IRUGO | S_IWUSR, TSPP_PID_FILTER_TABLE1},
	{"pid_filter_table2",   S_IRUGO | S_IWUSR, TSPP_PID_FILTER_TABLE2},
	{"global_performance",  S_IRUGO | S_IWUSR, TSPP_GLOBAL_PERFORMANCE},
	{"pipe_context",        S_IRUGO | S_IWUSR, TSPP_PIPE_CONTEXT},
	{"pipe_performance",    S_IRUGO | S_IWUSR, TSPP_PIPE_PERFORMANCE},
	{"data_key",            S_IRUGO | S_IWUSR, TSPP_DATA_KEY}
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

	/* debugfs */
	struct dentry *dent_tsif;
	struct dentry *debugfs_tsif_regs[ARRAY_SIZE(debugfs_tsif_regs)];
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
	struct cdev cdev;
	struct device *dd;
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
	u32 notify_timer;        /* notification for partially filled buffers */
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

/* this represents the actual hardware device */
struct tspp_device {
	struct list_head devlist; /* list of all devices */
	struct platform_device *pdev;
	void __iomem *base;
	unsigned int tspp_irq;
	unsigned int bam_irq;
	u32 bam_handle;
	struct sps_bam_props bam_props;
	struct wake_lock wake_lock;
	spinlock_t spinlock;
	struct tasklet_struct tlet;
	struct tspp_tsif_device tsif[TSPP_TSIF_INSTANCES];
	/* clocks */
	struct clk *tsif_pclk;
	struct clk *tsif_ref_clk;
	/* data */
	struct tspp_pid_filter_table *filters[TSPP_FILTER_TABLES];
	struct tspp_channel channels[TSPP_NUM_CHANNELS];
	struct tspp_key_table *tspp_key_table;
	struct tspp_global_performance_regs *tspp_global_performance;
	struct tspp_pipe_context_regs *tspp_pipe_context;
	struct tspp_pipe_performance_regs *tspp_pipe_performance;

	struct dentry *dent;
	struct dentry *debugfs_regs[ARRAY_SIZE(debugfs_tspp_regs)];
};


static struct class *tspp_class;
static int tspp_key_entry;
static dev_t tspp_minor;  /* next minor number to assign */

static LIST_HEAD(tspp_devices);

/* forward declarations */
static ssize_t tspp_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t tspp_open(struct inode *inode, struct file *filp);
static unsigned int tspp_poll(struct file *filp, struct poll_table_struct *p);
static ssize_t tspp_release(struct inode *inode, struct file *filp);
static long tspp_ioctl(struct file *, unsigned int, unsigned long);

/* file operations */
static const struct file_operations tspp_fops = {
	.owner   = THIS_MODULE,
	.read    = tspp_read,
	.open    = tspp_open,
	.poll    = tspp_poll,
	.release = tspp_release,
	.unlocked_ioctl   = tspp_ioctl,
};

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
		dev_warn(&device->pdev->dev, "Spurious interrupt");
		return IRQ_NONE;
	}

	/* if (status & TSPP_IRQ_STATUS_TSP_RD_CMPL) */

	if (status & TSPP_IRQ_STATUS_KEY_ERROR) {
		/* read the key error info */
		data = readl_relaxed(device->base + TSPP_KEY_ERROR);
		dev_info(&device->pdev->dev, "key error 0x%x", data);
	}
	if (status & TSPP_IRQ_STATUS_KEY_SWITCHED_BAD) {
		data = readl_relaxed(device->base + TSPP_KEY_VALID);
		dev_info(&device->pdev->dev, "key invalidated: 0x%x", data);
	}
	if (status & TSPP_IRQ_STATUS_KEY_SWITCHED)
		dev_info(&device->pdev->dev, "key switched");

	if (status & 0xffff)
		dev_info(&device->pdev->dev, "broken pipe %i", status & 0xffff);

	writel_relaxed(status, device->base + TSPP_IRQ_CLEAR);
	wmb();
	return IRQ_HANDLED;
}

/*** callbacks ***/
static void tspp_sps_complete_cb(struct sps_event_notify *notify)
{
	struct tspp_device *pdev = notify->user;
	tasklet_schedule(&pdev->tlet);
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

		/* get completions */
		while (channel->waiting->state == TSPP_BUF_STATE_WAITING) {
			if (sps_get_iovec(channel->pipe, &iovec) != 0) {
				pr_err("tspp: Error in iovec on channel %i",
					channel->id);
				break;
			}
			if (iovec.size == 0)
				break;

			if (iovec.addr != channel->waiting->sps.phys_base)
				pr_err("tspp: buffer mismatch 0x%08x",
					channel->waiting->sps.phys_base);

			complete = 1;
			channel->waiting->state = TSPP_BUF_STATE_DATA;
			channel->waiting->filled = iovec.size;
			channel->waiting->read_index = 0;

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
	}

	spin_unlock_irqrestore(&device->spinlock, flags);
}

/*** GPIO functions ***/
static void tspp_gpios_free(const struct msm_gpio *table, int size)
{
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		g = table + i;
		gpio_free(GPIO_PIN(g->gpio_cfg));
	}
}

static int tspp_gpios_request(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_request(GPIO_PIN(g->gpio_cfg), g->label);
		if (rc) {
			pr_err("tspp: gpio_request(%d) <%s> failed: %d\n",
			       GPIO_PIN(g->gpio_cfg), g->label ?: "?", rc);
			goto err;
		}
	}
	return 0;
err:
	tspp_gpios_free(table, i);
	return rc;
}

static int tspp_gpios_disable(const struct msm_gpio *table, int size)
{
	int rc = 0;
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		int tmp;
		g = table + i;
		tmp = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_DISABLE);
		if (tmp) {
			pr_err("tspp_gpios_disable(0x%08x, GPIO_CFG_DISABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("tspp: pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			if (!rc)
				rc = tmp;
		}
	}

	return rc;
}

static int tspp_gpios_enable(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("tspp: gpio_tlmm_config(0x%08x, GPIO_CFG_ENABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("tspp: pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			goto err;
		}
	}
	return 0;
err:
	tspp_gpios_disable(table, i);
	return rc;
}

static int tspp_gpios_request_enable(const struct msm_gpio *table, int size)
{
	int rc = tspp_gpios_request(table, size);
	if (rc)
		return rc;
	rc = tspp_gpios_enable(table, size);
	if (rc)
		tspp_gpios_free(table, size);
	return rc;
}

static void tspp_gpios_disable_free(const struct msm_gpio *table, int size)
{
	tspp_gpios_disable(table, size);
	tspp_gpios_free(table, size);
}

static int tspp_start_gpios(struct tspp_device *device)
{
	struct msm_tspp_platform_data *pdata =
		device->pdev->dev.platform_data;
	return tspp_gpios_request_enable(pdata->gpios, pdata->num_gpios);
}

static void tspp_stop_gpios(struct tspp_device *device)
{
	struct msm_tspp_platform_data *pdata =
		device->pdev->dev.platform_data;
	tspp_gpios_disable_free(pdata->gpios, pdata->num_gpios);
}

/*** Clock functions ***/
static int tspp_clock_start(struct tspp_device *device)
{
	if (device->tsif_pclk && clk_prepare_enable(device->tsif_pclk) != 0) {
		pr_err("tspp: Can't start pclk");
		return -EBUSY;
	}

	if (device->tsif_ref_clk &&
		clk_prepare_enable(device->tsif_ref_clk) != 0) {
		pr_err("tspp: Can't start ref clk");
		clk_disable_unprepare(device->tsif_pclk);
		return -EBUSY;
	}

	return 0;
}

static void tspp_clock_stop(struct tspp_device *device)
{
	if (device->tsif_pclk)
		clk_disable(device->tsif_pclk);

	if (device->tsif_ref_clk)
		clk_disable(device->tsif_ref_clk);
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
			pr_warn("tspp: tsif hw not started but ref count > 0");
			start_hardware = 1;
		}
	}

	if (start_hardware) {
		ctl = TSIF_STS_CTL_EN_IRQ |
				TSIF_STS_CTL_EN_DM;
		switch (tsif_device->mode) {
		case TSPP_TSIF_MODE_LOOPBACK:
			ctl |= TSIF_STS_CTL_EN_NULL |
					TSIF_STS_CTL_EN_ERROR |
					TSIF_STS_CTL_TEST_MODE;
			break;
		case TSPP_TSIF_MODE_1:
			ctl |= TSIF_STS_CTL_EN_TIME_LIM |
					TSIF_STS_CTL_EN_TCR;
			break;
		case TSPP_TSIF_MODE_2:
			ctl |= TSIF_STS_CTL_EN_TIME_LIM |
					TSIF_STS_CTL_EN_TCR |
					TSIF_STS_CTL_MODE_2;
			break;
		default:
			pr_warn("tspp: unknown tsif mode 0x%x",
				tsif_device->mode);
		}
		writel_relaxed(ctl, tsif_device->base + TSIF_STS_CTL_OFF);
		writel_relaxed(tsif_device->time_limit,
			  tsif_device->base + TSIF_TIME_LIMIT_OFF);
		wmb();
		writel_relaxed(ctl | TSIF_STS_CTL_START,
			  tsif_device->base + TSIF_STS_CTL_OFF);
		wmb();
	}

	ctl = readl_relaxed(tsif_device->base + TSIF_STS_CTL_OFF);
	tsif_device->ref_count++;

	return (ctl & TSIF_STS_CTL_START) ? 0 : -EBUSY;
}

static void tspp_stop_tsif(struct tspp_tsif_device *tsif_device)
{
	if (tsif_device->ref_count == 0)
		return;

	tsif_device->ref_count--;

	if (tsif_device->ref_count == 0) {
		writel_relaxed(TSIF_STS_CTL_STOP,
			tsif_device->base + TSIF_STS_CTL_OFF);
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
		pr_err("tspp_free_key_entry: index out of bounds");
		return;
	}

	tspp_key_entry &= ~(1 << entry);
}

static int tspp_alloc_buffer(u32 channel_id, struct tspp_data_descriptor *desc,
	u32 size, tspp_allocator *alloc, void *user)
{
	if (size < TSPP_MIN_BUFFER_SIZE ||
		size > TSPP_MAX_BUFFER_SIZE) {
		pr_err("tspp: bad buffer size %i", size);
		return -ENOMEM;
	}

	if (alloc) {
		TSPP_DEBUG("tspp using alloc function");
		desc->virt_base = alloc(channel_id, size,
			&desc->phys_base, user);
	} else {
	desc->virt_base = dma_alloc_coherent(NULL, size,
		&desc->phys_base, GFP_KERNEL);
	if (desc->virt_base == 0) {
		pr_err("tspp dma alloc coherent failed %i", size);
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
		flags = SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOB;

	/* start the transfer */
	rc = sps_transfer_one(channel->pipe,
		buffer->sps.phys_base,
		buffer->sps.size,
		channel->pdev,
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
	}
	writel_relaxed(TSPP_RST_RESET, pdev->base + TSPP_RST);
	wmb();

	/* BAM */
	if (sps_device_reset(pdev->bam_handle) != 0) {
		pr_err("tspp: error resetting bam");
		return -EBUSY;
	}

	/* TSPP tables */
	for (i = 0; i < TSPP_FILTER_TABLES; i++)
		memset(pdev->filters[i],
			0, sizeof(struct tspp_pid_filter_table));

	/* disable all filters */
	val = (2 << TSPP_NUM_CHANNELS) - 1;
	writel_relaxed(val, pdev->base + TSPP_PS_DISABLE);

	/* TSPP registers */
	val = readl_relaxed(pdev->base + TSPP_CONTROL);
	writel_relaxed(val | TSPP_CLK_CONTROL_FORCE_PERF_CNT,
		pdev->base + TSPP_CONTROL);
	wmb();
	memset(pdev->tspp_global_performance, 0,
		sizeof(struct tspp_global_performance_regs));
	memset(pdev->tspp_pipe_context, 0,
		sizeof(struct tspp_pipe_context_regs));
	memset(pdev->tspp_pipe_performance, 0,
		sizeof(struct tspp_pipe_performance_regs));
	wmb();
	writel_relaxed(val & ~TSPP_CLK_CONTROL_FORCE_PERF_CNT,
		pdev->base + TSPP_CONTROL);
	wmb();

	val = readl_relaxed(pdev->base + TSPP_CONFIG);
	val &= ~(TSPP_CONFIG_PS_LEN_ERR_MASK |
			TSPP_CONFIG_PS_CONT_ERR_UNSP_MASK |
			TSPP_CONFIG_PS_CONT_ERR_MASK);
	TSPP_CONFIG_SET_PACKET_LENGTH(val, TSPP_PACKET_LENGTH);
	writel_relaxed(val, pdev->base + TSPP_CONFIG);
	writel_relaxed(0x000fffff, pdev->base + TSPP_IRQ_MASK);
	writel_relaxed(0x000fffff, pdev->base + TSPP_IRQ_CLEAR);
	writel_relaxed(0, pdev->base + TSPP_RST);
	wmb();

	tspp_key_entry = 0;

	return 0;
}

static int tspp_select_source(u32 dev, u32 channel_id,
	struct tspp_select_source *src)
{
	/* make sure the requested src id is in bounds */
	if (src->source > TSPP_SOURCE_MEM) {
		pr_err("tspp source out of bounds");
		return -EINVAL;
	}

	/* open the stream */
	tspp_open_stream(dev, channel_id, src->source, src->mode);

	return 0;
}

static int tspp_set_iv(struct tspp_channel *channel, struct tspp_iv *iv)
{
	struct tspp_device *pdev = channel->pdev;

	writel_relaxed(iv->data[0], pdev->base + TSPP_CBC_INIT_VAL(0));
	writel_relaxed(iv->data[1], pdev->base + TSPP_CBC_INIT_VAL(1));
	return 0;
}

static int tspp_set_system_keys(struct tspp_channel *channel,
	struct tspp_system_keys *keys)
{
	int i;
	struct tspp_device *pdev = channel->pdev;

	for (i = 0; i < TSPP_NUM_SYSTEM_KEYS; i++)
		writel_relaxed(keys->data[i], pdev->base + TSPP_SYSTEM_KEY(i));

	return 0;
}

static int tspp_channel_init(struct tspp_channel *channel,
	struct tspp_device *pdev)
{
	channel->cdev.owner = THIS_MODULE;
	cdev_init(&channel->cdev, &tspp_fops);
	channel->pdev = pdev;
	channel->data = NULL;
	channel->read = NULL;
	channel->waiting = NULL;
	channel->locked = NULL;
	channel->id = MINOR(tspp_minor);
	channel->used = 0;
	channel->buffer_size = TSPP_MIN_BUFFER_SIZE;
	channel->max_buffers = TSPP_NUM_BUFFERS;
	channel->buffer_count = 0;
	channel->filter_count = 0;
	channel->int_freq = 1;
	channel->notifier = NULL;
	channel->notify_data = NULL;
	channel->notify_timer = 0;
	init_waitqueue_head(&channel->in_queue);

	if (cdev_add(&channel->cdev, tspp_minor++, 1) != 0) {
		pr_err("tspp: cdev_add failed");
		return -EBUSY;
	}

	channel->dd = device_create(tspp_class, NULL, channel->cdev.dev,
		channel, "tspp%02d", channel->id);
	if (IS_ERR(channel->dd)) {
		pr_err("tspp: device_create failed: %i",
			(int)PTR_ERR(channel->dd));
		cdev_del(&channel->cdev);
		return -EBUSY;
	}

	return 0;
}

static int tspp_set_buffer_size(struct tspp_channel *channel,
	struct tspp_buffer *buf)
{
	if (buf->size < TSPP_MIN_BUFFER_SIZE)
		channel->buffer_size = TSPP_MIN_BUFFER_SIZE;
	else if (buf->size > TSPP_MAX_BUFFER_SIZE)
		channel->buffer_size = TSPP_MAX_BUFFER_SIZE;
	else
		channel->buffer_size = buf->size;

	return 0;
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
		pr_warn("tspp: can't set mode for non-tsif source %d",
			channel->src);
		return;
	}
	channel->pdev->tsif[index].mode = mode;
}

/*** TSPP API functions ***/
int tspp_open_stream(u32 dev, u32 channel_id, enum tspp_source src, enum tspp_tsif_mode mode)
{
	u32 val;
	struct tspp_device *pdev;
	struct tspp_channel *channel;

	TSPP_DEBUG("tspp_open_stream %i %i %i %i", dev, channel_id, src, mode);
	if (dev >= TSPP_MAX_DEVICES) {
		pr_err("tspp: device id out of range");
		return -ENODEV;
	}

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}

	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_str: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->src = src;
	tspp_set_tsif_mode(channel, mode);

	switch (src) {
	case TSPP_SOURCE_TSIF0:
		/* make sure TSIF0 is running & enabled */
		if (tspp_start_tsif(&pdev->tsif[0]) != 0) {
			pr_err("tspp: error starting tsif0");
			return -EBUSY;
		}
		val = readl_relaxed(pdev->base + TSPP_CONTROL);
		writel_relaxed(val & ~TSPP_CONTROL_TSP_TSIF0_SRC_DIS,
			pdev->base + TSPP_CONTROL);
		wmb();
		break;
	case TSPP_SOURCE_TSIF1:
		/* make sure TSIF1 is running & enabled */
		if (tspp_start_tsif(&pdev->tsif[1]) != 0) {
			pr_err("tspp: error starting tsif1");
			return -EBUSY;
		}
		val = readl_relaxed(pdev->base + TSPP_CONTROL);
		writel_relaxed(val & ~TSPP_CONTROL_TSP_TSIF1_SRC_DIS,
			pdev->base + TSPP_CONTROL);
		wmb();
		break;
	case TSPP_SOURCE_MEM:
		break;
	default:
		pr_err("tspp: channel %i invalid source %i", channel->id, src);
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(tspp_open_stream);

int tspp_close_stream(u32 dev, u32 channel_id)
{
	u32 val;
	struct tspp_device *pdev;
	struct tspp_channel *channel;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_cs: can't find device %i", dev);
		return -EBUSY;
	}
	channel = &pdev->channels[channel_id];

	switch (channel->src) {
	case TSPP_SOURCE_TSIF0:
		tspp_stop_tsif(&pdev->tsif[0]);
		val = readl_relaxed(pdev->base + TSPP_CONTROL);
		writel_relaxed(val | TSPP_CONTROL_TSP_TSIF0_SRC_DIS,
			pdev->base + TSPP_CONTROL);
		wmb();
		break;
	case TSPP_SOURCE_TSIF1:
		tspp_stop_tsif(&pdev->tsif[1]);
		val = readl_relaxed(pdev->base + TSPP_CONTROL);
		writel_relaxed(val | TSPP_CONTROL_TSP_TSIF1_SRC_DIS,
			pdev->base + TSPP_CONTROL);
		break;
	case TSPP_SOURCE_MEM:
		break;
	case TSPP_SOURCE_NONE:
		break;
	}

	channel->src = TSPP_SOURCE_NONE;
	return 0;
}
EXPORT_SYMBOL(tspp_close_stream);

int tspp_open_channel(u32 dev, u32 channel_id)
{
	int rc = 0;
	struct sps_connect *config;
	struct sps_register_event *event;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_oc: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	if (channel->used) {
		pr_err("tspp channel already in use");
		return -EBUSY;
	}

	config = &channel->config;
	event = &channel->event;

	/* start the clocks if needed */
	tspp_clock_start(pdev);
	if (tspp_channels_in_use(pdev) == 0)
		wake_lock(&pdev->wake_lock);

	/* mark it as used */
	channel->used = 1;

	/* start the bam  */
	channel->pipe = sps_alloc_endpoint();
	if (channel->pipe == 0) {
		pr_err("tspp: error allocating endpoint");
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
		SPS_O_ACK_TRANSFERS; /* must use sps_get_iovec() */
	config->src_pipe_index = channel->id;
	config->desc.size =
		(TSPP_SPS_DESCRIPTOR_COUNT + 1) * SPS_DESCRIPTOR_SIZE;
	config->desc.base = dma_alloc_coherent(NULL,
						config->desc.size,
						&config->desc.phys_base,
						GFP_KERNEL);
	if (config->desc.base == 0) {
		pr_err("tspp: error allocating sps descriptors");
		rc = -ENOMEM;
		goto err_desc_alloc;
	}

	memset(config->desc.base, 0, config->desc.size);

	rc = sps_connect(channel->pipe, config);
	if (rc) {
		pr_err("tspp: error connecting bam");
		goto err_connect;
	}

	event->mode = SPS_TRIGGER_CALLBACK;
	event->options = SPS_O_DESC_DONE;
	event->callback = tspp_sps_complete_cb;
	event->xfer_done = NULL;
	event->user = pdev;

	rc = sps_register_event(channel->pipe, event);
	if (rc) {
		pr_err("tspp: error registering event");
		goto err_event;
	}

	rc = pm_runtime_get(&pdev->pdev->dev);
	if (rc < 0) {
		dev_err(&pdev->pdev->dev,
			"Runtime PM: Unable to wake up tspp device, rc = %d",
			rc);
	}
	return 0;

err_event:
	sps_disconnect(channel->pipe);
err_connect:
	dma_free_coherent(NULL, config->desc.size, config->desc.base,
		config->desc.phys_base);
err_desc_alloc:
	sps_free_endpoint(channel->pipe);
err_sps_alloc:
	return rc;
}
EXPORT_SYMBOL(tspp_open_channel);

int tspp_close_channel(u32 dev, u32 channel_id)
{
	int i;
	int id;
	u32 val;

	struct sps_connect *config;
	struct tspp_device *pdev;
	struct tspp_channel *channel;
	struct tspp_mem_buffer *pbuf, *temp;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_close: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	/* if the channel is not used, we are done */
	if (!channel->used)
		return 0;

	channel->notifier = NULL;
	channel->notify_data = NULL;
	channel->notify_timer = 0;

	config = &channel->config;
	pdev = channel->pdev;

	/* disable pipe (channel) */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val | channel->id, pdev->base + TSPP_PS_DISABLE);
	wmb();

	/* unregister all filters for this channel */
	for (i = 0; i < TSPP_NUM_PRIORITIES; i++) {
		struct tspp_pid_filter *tspp_filter =
			&pdev->filters[channel->src]->filter[i];
		id = FILTER_GET_PIPE_NUMBER0(tspp_filter);
		if (id == channel->id) {
			if (FILTER_HAS_ENCRYPTION(tspp_filter))
				tspp_free_key_entry(
					FILTER_GET_KEY_NUMBER(tspp_filter));
			tspp_filter->config = 0;
			tspp_filter->filter = 0;
		}
	}
	channel->filter_count = 0;

	/* stop the stream */
	tspp_close_stream(dev, channel->id);

	/* disconnect the bam */
	if (sps_disconnect(channel->pipe) != 0)
		pr_warn("tspp: Error freeing sps endpoint (%i)", channel->id);

	/* destroy the buffers */
	dma_free_coherent(NULL, config->desc.size, config->desc.base,
		config->desc.phys_base);

	pbuf = channel->data;
	for (i = 0; i < channel->buffer_count; i++) {
		if (pbuf->desc.phys_base) {
			dma_free_coherent(NULL,
				pbuf->desc.size,
				pbuf->desc.virt_base,
				pbuf->desc.phys_base);
			pbuf->desc.phys_base = 0;
		}
		pbuf->desc.virt_base = 0;
		pbuf->state = TSPP_BUF_STATE_EMPTY;
		temp = pbuf;
		pbuf = pbuf->next;
		kfree(temp);
	}
	channel->buffer_count = 0;
	channel->data = NULL;
	channel->read = NULL;
	channel->waiting = NULL;
	channel->locked = NULL;
	channel->used = 0;

	if (tspp_channels_in_use(pdev) == 0)
		wake_unlock(&pdev->wake_lock);
	tspp_clock_stop(pdev);

	return 0;
}
EXPORT_SYMBOL(tspp_close_channel);

int tspp_add_filter(u32 dev, u32 channel_id,
	struct tspp_filter *filter)
{
	int i;
	int other_channel;
	int entry;
	u32 val, pid, enabled;
	struct tspp_device *pdev;
	struct tspp_pid_filter p;
	struct tspp_channel *channel;

	TSPP_DEBUG("tspp: add filter");
	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_add: can't find device %i", dev);
		return -ENODEV;
	}

	channel = &pdev->channels[channel_id];

	if (filter->source > TSPP_SOURCE_MEM) {
		pr_err("tspp invalid source");
		return -ENOSR;
	}

	if (filter->priority >= TSPP_NUM_PRIORITIES) {
		pr_err("tspp invalid source");
		return -ENOSR;
	}

	/* make sure this filter mode matches the channel mode */
	switch (channel->mode) {
	case TSPP_MODE_DISABLED:
		channel->mode = filter->mode;
		break;
	case TSPP_MODE_RAW:
	case TSPP_MODE_PES:
	case TSPP_MODE_RAW_NO_SUFFIX:
		if (filter->mode != channel->mode) {
			pr_err("tspp: wrong filter mode");
			return -EBADSLT;
		}
	}

	if (filter->mode == TSPP_MODE_PES) {
		for (i = 0; i < TSPP_NUM_PRIORITIES; i++) {
			struct tspp_pid_filter *tspp_filter =
				&pdev->filters[channel->src]->filter[i];
			pid = FILTER_GET_PIPE_PID((tspp_filter));
			enabled = FILTER_GET_PIPE_PROCESS0(tspp_filter);
			if (enabled && (pid == filter->pid)) {
				other_channel =
					FILTER_GET_PIPE_NUMBER0(tspp_filter);
				pr_err("tspp: pid 0x%x already in use by channel %i",
					filter->pid, other_channel);
				return -EBADSLT;
			}
		}
	}

	/* make sure this priority is not already in use */
	enabled = FILTER_GET_PIPE_PROCESS0(
		(&(pdev->filters[channel->src]->filter[filter->priority])));
	if (enabled) {
		pr_err("tspp: filter priority %i source %i is already enabled\n",
			filter->priority, channel->src);
		return -ENOSR;
	}

	if (channel->mode == TSPP_MODE_PES) {
		/* if we are already processing in PES mode, disable pipe
		(channel) and filter to be updated */
		val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
		writel_relaxed(val | (1 << channel->id),
			pdev->base + TSPP_PS_DISABLE);
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
			pr_err("tspp: no more keys available!");
		} else {
			p.config |= FILTER_DECRYPT;
			FILTER_SET_KEY_NUMBER((&p), entry);
		}
	}

	pdev->filters[channel->src]->
		filter[filter->priority].config = p.config;
	pdev->filters[channel->src]->
		filter[filter->priority].filter = p.filter;

	/* allocate buffers if needed */
	tspp_allocate_buffers(dev, channel->id, channel->max_buffers,
		channel->buffer_size, channel->int_freq, 0, 0);
	if (channel->buffer_count < MIN_ACCEPTABLE_BUFFER_COUNT) {
		pr_err("tspp: failed to allocate at least %i buffers",
			MIN_ACCEPTABLE_BUFFER_COUNT);
		return -ENOMEM;
	}

	/* reenable pipe */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val & ~(1 << channel->id), pdev->base + TSPP_PS_DISABLE);
	wmb();
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);

	channel->filter_count++;

	return 0;
}
EXPORT_SYMBOL(tspp_add_filter);

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
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_remove: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	src = channel->src;
	tspp_filter = &(pdev->filters[src]->filter[filter->priority]);

	/* disable pipe (channel) */
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);
	writel_relaxed(val | channel->id, pdev->base + TSPP_PS_DISABLE);
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
	wmb();
	val = readl_relaxed(pdev->base + TSPP_PS_DISABLE);

	return 0;
}
EXPORT_SYMBOL(tspp_remove_filter);

int tspp_set_key(u32 dev, u32 channel_id, struct tspp_key *key)
{
	int i;
	int id;
	int key_index;
	int data;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_set: can't find device %i", dev);
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
		pr_err("tspp: no encryption on this channel");
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

int tspp_register_notification(u32 dev, u32 channel_id,
	tspp_notifier *pNotify, void *userdata, u32 timer_ms)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_reg: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->notifier = pNotify;
	channel->notify_data = userdata;
	channel->notify_timer = timer_ms;
	return 0;
}
EXPORT_SYMBOL(tspp_register_notification);

int tspp_unregister_notification(u32 dev, u32 channel_id)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_unreg: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];
	channel->notifier = NULL;
	channel->notify_data = 0;
	return 0;
}
EXPORT_SYMBOL(tspp_unregister_notification);

const struct tspp_data_descriptor *tspp_get_buffer(u32 dev, u32 channel_id)
{
	struct tspp_mem_buffer *buffer;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return NULL;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_get: can't find device %i", dev);
		return NULL;
	}
	channel = &pdev->channels[channel_id];

	if (!channel->read) {
		pr_warn("tspp: no buffer to get on channel %i!",
			channel->id);
		return NULL;
	}

	buffer = channel->read;
	/* see if we have any buffers ready to read */
	if (buffer->state != TSPP_BUF_STATE_DATA)
		return 0;

	if (buffer->state == TSPP_BUF_STATE_DATA) {
		/* mark the buffer as busy */
		buffer->state = TSPP_BUF_STATE_LOCKED;

		/* increment the pointer along the list */
		channel->read = channel->read->next;
	}

	return &buffer->desc;
}
EXPORT_SYMBOL(tspp_get_buffer);

int tspp_release_buffer(u32 dev, u32 channel_id, u32 descriptor_id)
{
	int i, found = 0;
	struct tspp_mem_buffer *buffer;
	struct tspp_channel *channel;
	struct tspp_device *pdev;

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	if (descriptor_id > channel->buffer_count)
		pr_warn("tspp: desc id looks weird 0x%08x", descriptor_id);

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
		pr_err("tspp: cant find desc %i", descriptor_id);
		return -EINVAL;
	}

	/* make sure the buffer is in the expected state */
	if (buffer->state != TSPP_BUF_STATE_LOCKED) {
		pr_err("tspp: buffer %i not locked", descriptor_id);
		return -EINVAL;
	}
	/* unlock the buffer and requeue it */
	buffer->state = TSPP_BUF_STATE_WAITING;

	if (tspp_queue_buffer(channel, buffer))
		pr_warn("tspp: can't requeue buffer");
	return 0;
}
EXPORT_SYMBOL(tspp_release_buffer);

int tspp_allocate_buffers(u32 dev, u32 channel_id,	u32 count,
	u32 size, u32 int_freq, tspp_allocator *alloc, void *user)
{
	struct tspp_channel *channel;
	struct tspp_device *pdev;
	struct tspp_mem_buffer *last = NULL;

	TSPP_DEBUG("tspp_allocate_buffers");

	if (channel_id >= TSPP_NUM_CHANNELS) {
		pr_err("tspp: channel id out of range");
		return -ECHRNG;
	}
	pdev = tspp_find_by_id(dev);
	if (!pdev) {
		pr_err("tspp_alloc: can't find device %i", dev);
		return -ENODEV;
	}
	channel = &pdev->channels[channel_id];

	channel->max_buffers = count;

	/* set up interrupt frequency */
	if (int_freq > channel->max_buffers)
		int_freq = channel->max_buffers;
	channel->int_freq = int_freq;

	switch (channel->mode) {
	case TSPP_MODE_DISABLED:
	case TSPP_MODE_PES:
		/* give the user what he asks for */
		channel->buffer_size = size;
		break;

	case TSPP_MODE_RAW:
		/* must be a multiple of 192 */
		if (size < (TSPP_PACKET_LENGTH+4))
			channel->buffer_size = (TSPP_PACKET_LENGTH+4);
		else
			channel->buffer_size = (size /
				(TSPP_PACKET_LENGTH+4)) *
				(TSPP_PACKET_LENGTH+4);
		break;

	case TSPP_MODE_RAW_NO_SUFFIX:
		/* must be a multiple of 188 */
		channel->buffer_size = (size / TSPP_PACKET_LENGTH) *
			TSPP_PACKET_LENGTH;
		break;
	}

	for (; channel->buffer_count < channel->max_buffers;
		channel->buffer_count++) {

		/* allocate the descriptor */
		struct tspp_mem_buffer *desc = (struct tspp_mem_buffer *)
			kmalloc(sizeof(struct tspp_mem_buffer), GFP_KERNEL);
		if (!desc) {
			pr_warn("tspp: Can't allocate desc %i",
			channel->buffer_count);
			break;
		}

		desc->desc.id = channel->buffer_count;
		/* allocate the buffer */
		if (tspp_alloc_buffer(channel_id, &desc->desc,
			channel->buffer_size, alloc, user) != 0) {
			kfree(desc);
			pr_warn("tspp: Can't allocate buffer %i",
				channel->buffer_count);
			break;
		}

		/* add the descriptor to the list */
		desc->filled = 0;
		desc->read_index = 0;
		if (!channel->data) {
			channel->data = desc;
			desc->next = channel->data;
		} else {
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
			pr_err("tspp: can't queue buffer %i", desc->desc.id);
	}

	channel->waiting = channel->data;
	channel->read = channel->data;
	channel->locked = channel->data;
	return 0;
}
EXPORT_SYMBOL(tspp_allocate_buffers);

/*** File Operations ***/
static ssize_t tspp_open(struct inode *inode, struct file *filp)
{
	u32 dev;
	struct tspp_channel *channel;

	TSPP_DEBUG("tspp_open");
	channel = container_of(inode->i_cdev, struct tspp_channel, cdev);
	filp->private_data = channel;
	dev = channel->pdev->pdev->id;

	/* if this channel is already in use, quit */
	if (channel->used) {
		pr_err("tspp channel %i already in use",
			MINOR(channel->cdev.dev));
		return -EACCES;
	}

	if (tspp_open_channel(dev, channel->id) != 0) {
		pr_err("tspp: error opening channel");
		return -EACCES;
	}

	return 0;
}

static unsigned int tspp_poll(struct file *filp, struct poll_table_struct *p)
{
	unsigned long flags;
	unsigned int mask = 0;
	struct tspp_channel *channel;
	channel = filp->private_data;

	/* register the wait queue for this channel */
	poll_wait(filp, &channel->in_queue, p);

	spin_lock_irqsave(&channel->pdev->spinlock, flags);
	if (channel->read &&
		channel->read->state == TSPP_BUF_STATE_DATA)
		mask = POLLIN | POLLRDNORM;

	spin_unlock_irqrestore(&channel->pdev->spinlock, flags);

	return mask;
}

static ssize_t tspp_release(struct inode *inode, struct file *filp)
{
	struct tspp_channel *channel = filp->private_data;
	u32 dev = channel->pdev->pdev->id;
	TSPP_DEBUG("tspp_release");

	tspp_close_channel(dev, channel->id);

	return 0;
}

static ssize_t tspp_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	size_t size = 0;
	size_t transferred = 0;
	struct tspp_channel *channel;
	struct tspp_mem_buffer *buffer;
	channel = filp->private_data;

	TSPP_DEBUG("tspp_read");

	while (!channel->read) {
		if (filp->f_flags & O_NONBLOCK) {
			pr_warn("tspp: no buffer on channel %i!",
				channel->id);
			return -EAGAIN;
		}
		/* go to sleep if there is nothing to read */
		if (wait_event_interruptible(channel->in_queue,
			(channel->read != NULL))) {
			pr_err("tspp: rude awakening\n");
			return -ERESTARTSYS;
		}
	}

	buffer = channel->read;

	/* see if we have any buffers ready to read */
	while (buffer->state != TSPP_BUF_STATE_DATA) {
		if (filp->f_flags & O_NONBLOCK) {
			pr_warn("tspp: nothing to read on channel %i!",
				channel->id);
			return -EAGAIN;
		}
		/* go to sleep if there is nothing to read */
		if (wait_event_interruptible(channel->in_queue,
			(buffer->state == TSPP_BUF_STATE_DATA))) {
			pr_err("tspp: rude awakening\n");
			return -ERESTARTSYS;
		}
	}

	while (buffer->state == TSPP_BUF_STATE_DATA) {
		size = min(count, buffer->filled);
		if (size == 0)
			break;

		if (copy_to_user(buf, buffer->desc.virt_base +
			buffer->read_index, size)) {
			pr_err("tspp: error copying to user buffer");
			return -EBUSY;
		}
		buf += size;
		count -= size;
		transferred += size;
		buffer->read_index += size;

		/* after reading the end of the buffer, requeue it,
			and set up for reading the next one */
		if (buffer->read_index == buffer->filled) {
			buffer->state = TSPP_BUF_STATE_WAITING;
			if (tspp_queue_buffer(channel, buffer))
				pr_err("tspp: can't submit transfer");
			channel->locked = channel->read;
			channel->read = channel->read->next;
		}
	}

	return transferred;
}

static long tspp_ioctl(struct file *filp,
			unsigned int param0, unsigned long param1)
{
	u32 dev;
	int rc = -1;
	struct tspp_channel *channel;
	struct tspp_select_source ss;
	struct tspp_filter f;
	struct tspp_key k;
	struct tspp_iv iv;
	struct tspp_system_keys sk;
	struct tspp_buffer b;
	channel = filp->private_data;
	dev = channel->pdev->pdev->id;

	if (!param1)
		return -EINVAL;

	switch (param0) {
	case TSPP_IOCTL_SELECT_SOURCE:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_select_source))) {
			return -EBUSY;
		}
		if (__copy_from_user(&ss, (void *)param1,
			sizeof(struct tspp_select_source)) == 0)
			rc = tspp_select_source(dev, channel->id, &ss);
		break;
	case TSPP_IOCTL_ADD_FILTER:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_filter))) {
			return -ENOSR;
		}
		if (__copy_from_user(&f, (void *)param1,
			sizeof(struct tspp_filter)) == 0)
			rc = tspp_add_filter(dev, channel->id, &f);
		break;
	case TSPP_IOCTL_REMOVE_FILTER:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_filter))) {
			return -EBUSY;
		}
		if (__copy_from_user(&f, (void *)param1,
			sizeof(struct tspp_filter)) == 0)
			rc = tspp_remove_filter(dev, channel->id, &f);
		break;
	case TSPP_IOCTL_SET_KEY:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_key))) {
			return -EBUSY;
		}
		if (__copy_from_user(&k, (void *)param1,
			sizeof(struct tspp_key)) == 0)
			rc = tspp_set_key(dev, channel->id, &k);
		break;
	case TSPP_IOCTL_SET_IV:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_iv))) {
			return -EBUSY;
		}
		if (__copy_from_user(&iv, (void *)param1,
			sizeof(struct tspp_iv)) == 0)
			rc = tspp_set_iv(channel, &iv);
		break;
	case TSPP_IOCTL_SET_SYSTEM_KEYS:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_system_keys))) {
			return -EINVAL;
		}
		if (__copy_from_user(&sk, (void *)param1,
			sizeof(struct tspp_system_keys)) == 0)
			rc = tspp_set_system_keys(channel, &sk);
		break;
	case TSPP_IOCTL_BUFFER_SIZE:
		if (!access_ok(VERIFY_READ, param1,
			sizeof(struct tspp_buffer))) {
			rc = -EINVAL;
		}
		if (__copy_from_user(&b, (void *)param1,
			sizeof(struct tspp_buffer)) == 0)
			rc = tspp_set_buffer_size(channel, &b);
		break;
	default:
		pr_err("tspp: Unknown ioctl %i", param0);
	}

	/* normalize the return code in case one of the subfunctions does
		something weird */
	if (rc != 0)
		rc = -ENOIOCTLCMD;

	return rc;
}

/*** debugfs ***/
static int debugfs_iomem_x32_set(void *data, u64 val)
{
	writel_relaxed(val, data);
	wmb();
	return 0;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	*val = readl_relaxed(data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llx");

static void tsif_debugfs_init(struct tspp_tsif_device *tsif_device,
	int instance)
{
	char name[10];
	snprintf(name, 10, "tsif%i", instance);
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
	}
}

static void tsif_debugfs_exit(struct tspp_tsif_device *tsif_device)
{
	if (tsif_device->dent_tsif) {
		int i;
		debugfs_remove_recursive(tsif_device->dent_tsif);
		tsif_device->dent_tsif = NULL;
		for (i = 0; i < ARRAY_SIZE(debugfs_tsif_regs); i++)
			tsif_device->debugfs_tsif_regs[i] = NULL;
	}
}

static void tspp_debugfs_init(struct tspp_device *device, int instance)
{
	char name[10];
	snprintf(name, 10, "tspp%i", instance);
	device->dent = debugfs_create_dir(
	      name, NULL);
	if (device->dent) {
		int i;
		void __iomem *base = device->base;
		for (i = 0; i < ARRAY_SIZE(debugfs_tspp_regs); i++) {
			device->debugfs_regs[i] =
			   debugfs_create_file(
				debugfs_tspp_regs[i].name,
				debugfs_tspp_regs[i].mode,
				device->dent,
				base + debugfs_tspp_regs[i].offset,
				&fops_iomem_x32);
		}
	}
}

static void tspp_debugfs_exit(struct tspp_device *device)
{
	if (device->dent) {
		int i;
		debugfs_remove_recursive(device->dent);
		device->dent = NULL;
		for (i = 0; i < ARRAY_SIZE(debugfs_tspp_regs); i++)
			device->debugfs_regs[i] = NULL;
	}
}

static int __devinit msm_tspp_probe(struct platform_device *pdev)
{
	int rc = -ENODEV;
	u32 version;
	u32 i;
	struct msm_tspp_platform_data *data;
	struct tspp_device *device;
	struct resource *mem_tsif0;
	struct resource *mem_tsif1;
	struct resource *mem_tspp;
	struct resource *mem_bam;

	/* must have platform data */
	data = pdev->dev.platform_data;
	if (!data) {
		pr_err("tspp: Platform data not available");
		rc = -EINVAL;
		goto out;
	}

	/* check for valid device id */
	if ((pdev->id < 0) || (pdev->id >= TSPP_MAX_DEVICES)) {
		pr_err("tspp: Invalid device ID %d", pdev->id);
		rc = -EINVAL;
		goto out;
	}

	/* OK, we will use this device */
	device = kzalloc(sizeof(struct tspp_device), GFP_KERNEL);
	if (!device) {
		pr_err("tspp: Failed to allocate memory for device");
		rc = -ENOMEM;
		goto out;
	}

	/* set up references */
	device->pdev = pdev;
	platform_set_drvdata(pdev, device);

	/* map clocks */
	if (data->tsif_pclk) {
		device->tsif_pclk = clk_get(&pdev->dev, data->tsif_pclk);
		if (IS_ERR(device->tsif_pclk)) {
			pr_err("tspp: failed to get %s",
				data->tsif_pclk);
			rc = PTR_ERR(device->tsif_pclk);
			device->tsif_pclk = NULL;
			goto err_pclock;
		}
	}
	if (data->tsif_ref_clk) {
		device->tsif_ref_clk = clk_get(&pdev->dev, data->tsif_ref_clk);
		if (IS_ERR(device->tsif_ref_clk)) {
			pr_err("tspp: failed to get %s",
				data->tsif_ref_clk);
			rc = PTR_ERR(device->tsif_ref_clk);
			device->tsif_ref_clk = NULL;
			goto err_refclock;
		}
	}

	/* map I/O memory */
	mem_tsif0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_tsif0) {
		pr_err("tspp: Missing tsif0 MEM resource");
		rc = -ENXIO;
		goto err_res_tsif0;
	}
	device->tsif[0].base = ioremap(mem_tsif0->start,
		resource_size(mem_tsif0));
	if (!device->tsif[0].base) {
		pr_err("tspp: ioremap failed");
		goto err_map_tsif0;
	}

	mem_tsif1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!mem_tsif1) {
		dev_err(&pdev->dev, "Missing tsif1 MEM resource");
		rc = -ENXIO;
		goto err_res_tsif1;
	}
	device->tsif[1].base = ioremap(mem_tsif1->start,
		resource_size(mem_tsif1));
	if (!device->tsif[1].base) {
		dev_err(&pdev->dev, "ioremap failed");
		goto err_map_tsif1;
	}

	mem_tspp = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!mem_tspp) {
		dev_err(&pdev->dev, "Missing MEM resource");
		rc = -ENXIO;
		goto err_res_dev;
	}
	device->base = ioremap(mem_tspp->start, resource_size(mem_tspp));
	if (!device->base) {
		dev_err(&pdev->dev, "ioremap failed");
		goto err_map_dev;
	}

	mem_bam = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!mem_bam) {
		pr_err("tspp: Missing bam MEM resource");
		rc = -ENXIO;
		goto err_res_bam;
	}
	memset(&device->bam_props, 0, sizeof(device->bam_props));
	device->bam_props.phys_addr = mem_bam->start;
	device->bam_props.virt_addr = ioremap(mem_bam->start,
		resource_size(mem_bam));
	if (!device->bam_props.virt_addr) {
		dev_err(&pdev->dev, "ioremap failed");
		goto err_map_bam;
	}

	/* map TSPP IRQ */
	rc = platform_get_irq(pdev, 0);
	if (rc > 0) {
		device->tspp_irq = rc;
		rc = request_irq(device->tspp_irq, tspp_isr, IRQF_SHARED,
				 dev_name(&pdev->dev), device);
		if (rc) {
			dev_err(&pdev->dev, "failed to request IRQ %d : %d",
				device->tspp_irq, rc);
			goto err_irq;
		}
	} else {
		dev_err(&pdev->dev, "failed to get tspp IRQ");
		goto err_irq;
	}

	/* BAM IRQ */
	device->bam_irq = TSIF_BAM_IRQ;

	/* GPIOs */
	rc = tspp_start_gpios(device);
	if (rc)
		goto err_gpio;

	/* power management */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	tspp_debugfs_init(device, 0);

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_init(&device->tsif[i], i);

	wake_lock_init(&device->wake_lock, WAKE_LOCK_SUSPEND,
		dev_name(&pdev->dev));

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

	if (sps_register_bam_device(&device->bam_props,
		&device->bam_handle) != 0) {
		pr_err("tspp: failed to register bam");
		goto err_bam;
	}

	if (tspp_clock_start(device) != 0) {
		dev_err(&pdev->dev, "Can't start clocks");
		goto err_clock;
	}

	spin_lock_init(&device->spinlock);
	tasklet_init(&device->tlet, tspp_sps_complete_tlet,
			(unsigned long)device);

	/* initialize everything to a known state */
	tspp_global_reset(device);

	version = readl_relaxed(device->base + TSPP_VERSION);
	if (version != 1)
		pr_warn("tspp: unrecognized hw version=%i", version);

	/* initialize the channels */
	for (i = 0; i < TSPP_NUM_CHANNELS; i++) {
		if (tspp_channel_init(&(device->channels[i]), device) != 0) {
			pr_err("tspp_channel_init failed");
			goto err_channel;
		}
	}

	/* stop the clocks for power savings */
	tspp_clock_stop(device);

	/* everything is ok, so add the device to the list */
	list_add_tail(&(device->devlist), &tspp_devices);

	return 0;

err_channel:
err_clock:
	sps_deregister_bam_device(device->bam_handle);
err_bam:
	tspp_debugfs_exit(device);
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_exit(&device->tsif[i]);
err_gpio:
err_irq:
	tspp_stop_gpios(device);
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
	kfree(device);

out:
	return rc;
}

static int __devexit msm_tspp_remove(struct platform_device *pdev)
{
	struct tspp_channel *channel;
	u32 i;

	struct tspp_device *device = platform_get_drvdata(pdev);

	/* free the buffers, and delete the channels */
	for (i = 0; i < TSPP_NUM_CHANNELS; i++) {
		channel = &device->channels[i];
		tspp_close_channel(device->pdev->id, i);
		device_destroy(tspp_class, channel->cdev.dev);
		cdev_del(&channel->cdev);
	}

	sps_deregister_bam_device(device->bam_handle);

	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		tsif_debugfs_exit(&device->tsif[i]);

	wake_lock_destroy(&device->wake_lock);
	free_irq(device->tspp_irq, device);
	tspp_stop_gpios(device);

	iounmap(device->bam_props.virt_addr);
	iounmap(device->base);
	for (i = 0; i < TSPP_TSIF_INSTANCES; i++)
		iounmap(device->tsif[i].base);

	if (device->tsif_ref_clk)
		clk_put(device->tsif_ref_clk);

	if (device->tsif_pclk)
		clk_put(device->tsif_pclk);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_put(&pdev->dev);
	kfree(device);

	return 0;
}

/*** power management ***/

static int tspp_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...");
	return 0;
}

static int tspp_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...");
	return 0;
}

static const struct dev_pm_ops tspp_dev_pm_ops = {
	.runtime_suspend = tspp_runtime_suspend,
	.runtime_resume = tspp_runtime_resume,
};

static struct platform_driver msm_tspp_driver = {
	.probe          = msm_tspp_probe,
	.remove         = __exit_p(msm_tspp_remove),
	.driver         = {
		.name   = "msm_tspp",
		.pm     = &tspp_dev_pm_ops,
	},
};


static int __init mod_init(void)
{
	int rc;

	/* make the char devs (channels) */
	rc = alloc_chrdev_region(&tspp_minor, 0, TSPP_NUM_CHANNELS, "tspp");
	if (rc) {
		pr_err("tspp: alloc_chrdev_region failed: %d", rc);
		goto err_devrgn;
	}

	tspp_class = class_create(THIS_MODULE, "tspp");
	if (IS_ERR(tspp_class)) {
		rc = PTR_ERR(tspp_class);
		pr_err("tspp: Error creating class: %d", rc);
		goto err_class;
	}

	/* register the driver, and check hardware */
	rc = platform_driver_register(&msm_tspp_driver);
	if (rc) {
		pr_err("tspp: platform_driver_register failed: %d", rc);
		goto err_register;
	}

	return 0;

err_register:
	class_destroy(tspp_class);
err_class:
	unregister_chrdev_region(0, TSPP_NUM_CHANNELS);
err_devrgn:
	return rc;
}

static void __exit mod_exit(void)
{
	/* delete low level driver */
	platform_driver_unregister(&msm_tspp_driver);

	/* delete upper layer interface */
	class_destroy(tspp_class);
	unregister_chrdev_region(0, TSPP_NUM_CHANNELS);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("TSPP platform device and char dev");
MODULE_LICENSE("GPL v2");
