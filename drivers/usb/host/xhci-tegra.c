/*
 * xhci-tegra.c - Nvidia xHCI host controller driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/uaccess.h>
#include <linux/circ_buf.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/tegra-fuse.h>

#include <mach/tegra_usb_pad_ctrl.h>
#include <mach/tegra_usb_pmc.h>
#include <mach/pm_domains.h>
#include <mach/mc.h>
#include <mach/xusb.h>

#include "xhci-tegra.h"
#include "xhci.h"
#include "../../../arch/arm/mach-tegra/iomap.h"

/* macros */
#define FW_IOCTL_LOG_DEQUEUE_LOW	(4)
#define FW_IOCTL_LOG_DEQUEUE_HIGH	(5)
#define FW_IOCTL_DATA_SHIFT		(0)
#define FW_IOCTL_DATA_MASK		(0x00ffffff)
#define FW_IOCTL_TYPE_SHIFT		(24)
#define FW_IOCTL_TYPE_MASK		(0xff000000)
#define FW_LOG_SIZE			(sizeof(struct log_entry))
#define FW_LOG_COUNT			(4096)
#define FW_LOG_RING_SIZE		(FW_LOG_SIZE * FW_LOG_COUNT)
#define FW_LOG_PAYLOAD_SIZE		(27)
#define DRIVER				(0x01)
#define CIRC_BUF_SIZE			(4 * (1 << 20))	/* 4MB */
#define FW_LOG_THREAD_RELAX		(msecs_to_jiffies(100))

/* tegra_xhci_firmware_log.flags bits */
#define FW_LOG_CONTEXT_VALID		(0)
#define FW_LOG_FILE_OPENED		(1)

#define PAGE_SELECT_MASK			0xFFFFFE00
#define PAGE_SELECT_SHIFT			9
#define PAGE_OFFSET_MASK			0x000001FF
#define CSB_PAGE_SELECT(_addr)						\
	({								\
		typecheck(u32, _addr);					\
		((_addr & PAGE_SELECT_MASK) >> PAGE_SELECT_SHIFT);	\
	})
#define CSB_PAGE_OFFSET(_addr)						\
	({								\
		typecheck(u32, _addr);					\
		(_addr & PAGE_OFFSET_MASK);				\
	})

#define reg_dump(_dev, _base, _reg)					\
	dev_dbg(_dev, "%s: %s @%x = 0x%x\n", __func__, #_reg,		\
		_reg, readl(_base + _reg))

#define PMC_PORTMAP_MASK(map, pad)	(((map) >> 4*(pad)) & 0xF)

#define PMC_USB_DEBOUNCE_DEL_0			0xec
#define   UTMIP_LINE_DEB_CNT(x)		(((x) & 0xf) << 16)
#define   UTMIP_LINE_DEB_CNT_MASK		(0xf << 16)

#define PMC_UTMIP_UHSIC_SLEEP_CFG_0		0x1fc

/* private data types */
/* command requests from the firmware */
enum MBOX_CMD_TYPE {
	MBOX_CMD_MSG_ENABLED = 1,
	MBOX_CMD_INC_FALC_CLOCK,
	MBOX_CMD_DEC_FALC_CLOCK,
	MBOX_CMD_INC_SSPI_CLOCK,
	MBOX_CMD_DEC_SSPI_CLOCK, /* 5 */
	MBOX_CMD_SET_BW,
	MBOX_CMD_SET_SS_PWR_GATING,
	MBOX_CMD_SET_SS_PWR_UNGATING, /* 8 */
	MBOX_CMD_SAVE_DFE_CTLE_CTX,
	MBOX_CMD_AIRPLANE_MODE_ENABLED, /* unused */
	MBOX_CMD_AIRPLANE_MODE_DISABLED, /* 11, unused */
	MBOX_CMD_STAR_HSIC_IDLE,
	MBOX_CMD_STOP_HSIC_IDLE,
	MBOX_CMD_DBC_WAKE_STACK, /* unused */
	MBOX_CMD_HSIC_PRETEND_CONNECT,

	/* needs to be the last cmd */
	MBOX_CMD_MAX,

	/* resp msg to ack above commands */
	MBOX_CMD_ACK = 128,
	MBOX_CMD_NACK
};

struct log_entry {
	u32 sequence_no;
	u8 data[FW_LOG_PAYLOAD_SIZE];
	u8 owner;
};

/* Usb3 Firmware Cfg Table */
struct cfgtbl {
	u32 boot_loadaddr_in_imem;
	u32 boot_codedfi_offset;
	u32 boot_codetag;
	u32 boot_codesize;

	/* Physical memory reserved by Bootloader/BIOS */
	u32 phys_memaddr;
	u16 reqphys_memsize;
	u16 alloc_phys_memsize;

	/* .rodata section */
	u32 rodata_img_offset;
	u32 rodata_section_start;
	u32 rodata_section_end;
	u32 main_fnaddr;

	u32 fwimg_cksum;
	u32 fwimg_created_time;

	/* Fields that get filled by linker during linking phase
	 * or initialized in the FW code.
	 */
	u32 imem_resident_start;
	u32 imem_resident_end;
	u32 idirect_start;
	u32 idirect_end;
	u32 l2_imem_start;
	u32 l2_imem_end;
	u32 version_id;
	u8 init_ddirect;
	u8 reserved[3];
	u32 phys_addr_log_buffer;
	u32 total_log_entries;
	u32 dequeue_ptr;

	/*	Below two dummy variables are used to replace
	 *	L2IMemSymTabOffsetInDFI and L2IMemSymTabSize in order to
	 *	retain the size of struct _CFG_TBL used by other AP/Module.
	 */
	u32 dummy_var1;
	u32 dummy_var2;

	/* fwimg_len */
	u32 fwimg_len;
	u8 magic[8];
	u32 SS_low_power_entry_timeout;
	u8 num_hsic_port;
	u8 padding[139]; /* padding bytes to makeup 256-bytes cfgtbl */
};

struct xusb_save_regs {
	u32 msi_bar_sz;
	u32 msi_axi_barst;
	u32 msi_fpci_barst;
	u32 msi_vec0;
	u32 msi_en_vec0;
	u32 fpci_error_masks;
	u32 intr_mask;
	u32 ipfs_intr_enable;
	u32 ufpci_config;
	u32 clkgate_hysteresis;
	u32 xusb_host_mccif_fifo_cntrl;

	/* PG does not mention below */
	u32 hs_pls;
	u32 fs_pls;
	u32 hs_fs_speed;
	u32 hs_fs_pp;
	u32 cfg_aru;
	u32 cfg_order;
	u32 cfg_fladj;
	u32 cfg_sid;
	/* DFE and CTLE */
	u32 tap1_val[XUSB_SS_PORT_COUNT];
	u32 amp_val[XUSB_SS_PORT_COUNT];
	u32 ctle_z_val[XUSB_SS_PORT_COUNT];
	u32 ctle_g_val[XUSB_SS_PORT_COUNT];
};

struct tegra_xhci_firmware {
	void *data; /* kernel virtual address */
	size_t size; /* firmware size */
	dma_addr_t dma; /* dma address for controller */
};

struct tegra_xhci_firmware_log {
	dma_addr_t phys_addr;		/* dma-able address */
	void *virt_addr;		/* kernel va of the shared log buffer */
	struct log_entry *dequeue;	/* current dequeue pointer (va) */
	struct circ_buf circ;		/* big circular buffer */
	u32 seq;			/* log sequence number */

	struct task_struct *thread;	/* a thread to consume log */
	struct mutex mutex;
	wait_queue_head_t read_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t intr_wait;
	struct dentry *path;
	struct dentry *log_file;
	unsigned long flags;
};

/* structure to hold the offsets of padctl registers */
struct tegra_xusb_padctl_regs {
	u16 boot_media_0;
	u16 usb2_pad_mux_0;
	u16 usb2_port_cap_0;
	u16 snps_oc_map_0;
	u16 usb2_oc_map_0;
	u16 ss_port_map_0;
	u16 oc_det_0;
	u16 elpg_program_0;
	u16 usb2_bchrg_otgpad0_ctl0_0;
	u16 usb2_bchrg_otgpad0_ctl1_0;
	u16 usb2_bchrg_otgpad1_ctl0_0;
	u16 usb2_bchrg_otgpad1_ctl1_0;
	u16 usb2_bchrg_otgpad2_ctl0_0;
	u16 usb2_bchrg_otgpad2_ctl1_0;
	u16 usb2_bchrg_bias_pad_0;
	u16 usb2_bchrg_tdcd_dbnc_timer_0;
	u16 iophy_pll_p0_ctl1_0;
	u16 iophy_pll_p0_ctl2_0;
	u16 iophy_pll_p0_ctl3_0;
	u16 iophy_pll_p0_ctl4_0;
	u16 iophy_usb3_pad0_ctl1_0;
	u16 iophy_usb3_pad1_ctl1_0;
	u16 iophy_usb3_pad0_ctl2_0;
	u16 iophy_usb3_pad1_ctl2_0;
	u16 iophy_usb3_pad0_ctl3_0;
	u16 iophy_usb3_pad1_ctl3_0;
	u16 iophy_usb3_pad0_ctl4_0;
	u16 iophy_usb3_pad1_ctl4_0;
	u16 iophy_misc_pad_p0_ctl1_0;
	u16 iophy_misc_pad_p1_ctl1_0;
	u16 iophy_misc_pad_p0_ctl2_0;
	u16 iophy_misc_pad_p1_ctl2_0;
	u16 iophy_misc_pad_p0_ctl3_0;
	u16 iophy_misc_pad_p1_ctl3_0;
	u16 iophy_misc_pad_p0_ctl4_0;
	u16 iophy_misc_pad_p1_ctl4_0;
	u16 iophy_misc_pad_p0_ctl5_0;
	u16 iophy_misc_pad_p1_ctl5_0;
	u16 iophy_misc_pad_p0_ctl6_0;
	u16 iophy_misc_pad_p1_ctl6_0;
	u16 usb2_otg_pad0_ctl0_0;
	u16 usb2_otg_pad1_ctl0_0;
	u16 usb2_otg_pad2_ctl0_0;
	u16 usb2_otg_pad0_ctl1_0;
	u16 usb2_otg_pad1_ctl1_0;
	u16 usb2_otg_pad2_ctl1_0;
	u16 usb2_bias_pad_ctl0_0;
	u16 usb2_bias_pad_ctl1_0;
	u16 usb2_hsic_pad0_ctl0_0;
	u16 usb2_hsic_pad1_ctl0_0;
	u16 usb2_hsic_pad0_ctl1_0;
	u16 usb2_hsic_pad1_ctl1_0;
	u16 usb2_hsic_pad0_ctl2_0;
	u16 usb2_hsic_pad1_ctl2_0;
	u16 ulpi_link_trim_ctl0;
	u16 ulpi_null_clk_trim_ctl0;
	u16 hsic_strb_trim_ctl0;
	u16 wake_ctl0;
	u16 pm_spare0;
	u16 iophy_misc_pad_p2_ctl1_0;
	u16 iophy_misc_pad_p3_ctl1_0;
	u16 iophy_misc_pad_p4_ctl1_0;
	u16 iophy_misc_pad_p2_ctl2_0;
	u16 iophy_misc_pad_p3_ctl2_0;
	u16 iophy_misc_pad_p4_ctl2_0;
	u16 iophy_misc_pad_p2_ctl3_0;
	u16 iophy_misc_pad_p3_ctl3_0;
	u16 iophy_misc_pad_p4_ctl3_0;
	u16 iophy_misc_pad_p2_ctl4_0;
	u16 iophy_misc_pad_p3_ctl4_0;
	u16 iophy_misc_pad_p4_ctl4_0;
	u16 iophy_misc_pad_p2_ctl5_0;
	u16 iophy_misc_pad_p3_ctl5_0;
	u16 iophy_misc_pad_p4_ctl5_0;
	u16 iophy_misc_pad_p2_ctl6_0;
	u16 iophy_misc_pad_p3_ctl6_0;
	u16 iophy_misc_pad_p4_ctl6_0;
	u16 usb3_pad_mux_0;
	u16 iophy_pll_s0_ctl1_0;
	u16 iophy_pll_s0_ctl2_0;
	u16 iophy_pll_s0_ctl3_0;
	u16 iophy_pll_s0_ctl4_0;
	u16 iophy_misc_pad_s0_ctl1_0;
	u16 iophy_misc_pad_s0_ctl2_0;
	u16 iophy_misc_pad_s0_ctl3_0;
	u16 iophy_misc_pad_s0_ctl4_0;
	u16 iophy_misc_pad_s0_ctl5_0;
	u16 iophy_misc_pad_s0_ctl6_0;
};

struct tegra_xhci_hcd {
	struct platform_device *pdev;
	struct xhci_hcd *xhci;
	u16 device_id;

	spinlock_t lock;
	struct mutex sync_lock;

	int smi_irq;
	int padctl_irq;
	int usb3_irq;
	int usb2_irq;

	bool ss_wake_event;
	bool ss_pwr_gated;
	bool host_pwr_gated;
	bool hs_wake_event;
	bool host_resume_req;
	bool lp0_exit;
	bool dfe_ctx_saved[XUSB_SS_PORT_COUNT];
	bool ctle_ctx_saved[XUSB_SS_PORT_COUNT];
	unsigned long last_jiffies;
	unsigned long host_phy_base;
	unsigned long host_phy_size;
	void __iomem *host_phy_virt_base;

	void __iomem *padctl_base;
	void __iomem *fpci_base;
	void __iomem *ipfs_base;

	struct tegra_xusb_platform_data *pdata;
	struct tegra_xusb_board_data *bdata;
	struct tegra_xusb_chip_calib *cdata;
	struct tegra_xusb_padctl_regs *padregs;
	const struct tegra_xusb_soc_config *soc_config;
	u64 tegra_xusb_dmamask;

	/* mailbox variables */
	struct mutex mbox_lock;
	u32 mbox_owner;
	u32 cmd_type;
	u32 cmd_data;

	struct regulator *xusb_utmi_vbus_regs[XUSB_UTMI_COUNT];

	struct regulator *xusb_s1p05v_reg;
	struct regulator *xusb_s3p3v_reg;
	struct regulator *xusb_s1p8v_reg;
	struct regulator *vddio_hsic_reg;
	int vddio_hsic_refcnt;

	struct work_struct mbox_work;
	struct work_struct ss_elpg_exit_work;
	struct work_struct host_elpg_exit_work;

	struct clk *host_clk;
	struct clk *ss_clk;

	/* XUSB Falcon SuperSpeed Clock */
	struct clk *falc_clk;

	/* EMC Clock */
	struct clk *emc_clk;
	/* XUSB SS PI Clock */
	struct clk *ss_src_clk;
	/* PLLE Clock */
	struct clk *plle_clk;
	struct clk *pll_u_480M;
	struct clk *clk_m;
	/* refPLLE clk */
	struct clk *pll_re_vco_clk;
	/*
	 * XUSB/IPFS specific registers these need to be saved/restored in
	 * addition to spec defined registers
	 */
	struct xusb_save_regs sregs;
	bool usb2_rh_suspend;
	bool usb3_rh_suspend;
	bool hc_in_elpg;

	/* otg transceiver */
	struct usb_phy *transceiver;
	struct notifier_block otgnb;

	unsigned long usb2_rh_remote_wakeup_ports; /* one bit per port */
	unsigned long usb3_rh_remote_wakeup_ports; /* one bit per port */
	/* firmware loading related */
	struct tegra_xhci_firmware firmware;

	struct tegra_xhci_firmware_log log;

	bool init_done;
};

static int tegra_xhci_probe2(struct tegra_xhci_hcd *tegra);
static int tegra_xhci_remove(struct platform_device *pdev);
static void init_filesystem_firmware_done(const struct firmware *fw,
					void *context);

static struct tegra_usb_pmc_data pmc_data[XUSB_UTMI_COUNT];
static struct tegra_usb_pmc_data pmc_hsic_data[XUSB_HSIC_COUNT];
static void save_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)  __attribute__ ((unused));

#define FIRMWARE_FILE "tegra_xusb_firmware"
static char *firmware_file = FIRMWARE_FILE;
#define FIRMWARE_FILE_HELP	\
	"used to specify firmware file of Tegra XHCI host controller. "\
	"Default value is \"" FIRMWARE_FILE "\"."

module_param(firmware_file, charp, S_IRUGO);
MODULE_PARM_DESC(firmware_file, FIRMWARE_FILE_HELP);

/* functions */
static inline struct tegra_xhci_hcd *hcd_to_tegra_xhci(struct usb_hcd *hcd)
{
	return (struct tegra_xhci_hcd *) dev_get_drvdata(hcd->self.controller);
}

static inline void must_have_sync_lock(struct tegra_xhci_hcd *tegra)
{
#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
	WARN_ON(tegra->sync_lock.owner != current);
#endif
}

#define for_each_enabled_hsic_pad(_pad, _tegra_xhci_hcd)		\
	for (_pad = find_next_enabled_hsic_pad(_tegra_xhci_hcd, 0);	\
	    (_pad < XUSB_HSIC_COUNT) && (_pad >= 0);			\
	    _pad = find_next_enabled_hsic_pad(_tegra_xhci_hcd, _pad + 1))

static inline int find_next_enabled_pad(struct tegra_xhci_hcd *tegra,
						int start, int last)
{
	unsigned long portmap = tegra->bdata->portmap;
	return find_next_bit(&portmap, last , start);
}

static inline int find_next_enabled_hsic_pad(struct tegra_xhci_hcd *tegra,
						int curr_pad)
{
	int start = XUSB_HSIC_INDEX + curr_pad;
	int last = XUSB_HSIC_INDEX + XUSB_HSIC_COUNT;

	if ((curr_pad < 0) || (curr_pad >= XUSB_HSIC_COUNT))
		return -1;

	return find_next_enabled_pad(tegra, start, last) - XUSB_HSIC_INDEX;
}

static void tegra_xhci_setup_gpio_for_ss_lane(struct tegra_xhci_hcd *tegra)
{
	int err = 0;

	if (!tegra->bdata->gpio_controls_muxed_ss_lanes)
		return;

	if (tegra->bdata->lane_owner & BIT(0)) {
		/* USB3_SS port1 is using SATA lane so set (MUX_SATA)
		 * GPIO P11 to '0'
		 */
		err = gpio_request((tegra->bdata->gpio_ss1_sata & 0xffff),
			"gpio_ss1_sata");
		if (err < 0)
			pr_err("%s: gpio_ss1_sata gpio_request failed %d\n",
				__func__, err);
		err = gpio_direction_output((tegra->bdata->gpio_ss1_sata
			& 0xffff), 1);
		if (err < 0)
			pr_err("%s: gpio_ss1_sata gpio_direction failed %d\n",
				__func__, err);
		__gpio_set_value((tegra->bdata->gpio_ss1_sata & 0xffff),
				((tegra->bdata->gpio_ss1_sata >> 16)));
	}
}

static u32 xhci_read_portsc(struct xhci_hcd *xhci, unsigned int port)
{
	int num_ports = HCS_MAX_PORTS(xhci->hcs_params1);
	__le32 __iomem *addr;

	if (port >= num_ports) {
		xhci_err(xhci, "%s invalid port %u\n", __func__, port);
		return -1;
	}

	addr = &xhci->op_regs->port_status_base + (NUM_PORT_REGS * port);
	return xhci_readl(xhci, addr);
}

static void debug_print_portsc(struct xhci_hcd *xhci)
{
	__le32 __iomem *addr = &xhci->op_regs->port_status_base;
	u32 reg;
	int i;
	int ports;

	ports = HCS_MAX_PORTS(xhci->hcs_params1);
	for (i = 0; i < ports; i++) {
		reg = xhci_read_portsc(xhci, i);
		xhci_dbg(xhci, "@%p port %d status reg = 0x%x\n",
				addr, i, (unsigned int) reg);
		addr += NUM_PORT_REGS;
	}
}
static void tegra_xhci_war_for_tctrl_rctrl(struct tegra_xhci_hcd *tegra);

static bool is_otg_host(struct tegra_xhci_hcd *tegra)
{
	if (!tegra->transceiver)
		return true;
	else if (tegra->transceiver->state == OTG_STATE_A_HOST)
		return true;
	else
		return false;
}

static int update_speed(struct tegra_xhci_hcd *tegra, u8 port)
{
	struct usb_hcd *hcd = xhci_to_hcd(tegra->xhci);
	u32 portsc;

	portsc = readl(hcd->regs + BAR0_XHCI_OP_PORTSC(port));
	if (DEV_FULLSPEED(portsc))
		return USB_PMC_PORT_SPEED_FULL;
	else if (DEV_HIGHSPEED(portsc))
		return USB_PMC_PORT_SPEED_HIGH;
	else if (DEV_LOWSPEED(portsc))
		return USB_PMC_PORT_SPEED_LOW;
	else if (DEV_SUPERSPEED(portsc))
		return USB_PMC_PORT_SPEED_SUPER;
	else
		return USB_PMC_PORT_SPEED_UNKNOWN;
}

static void pmc_init(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	int pad;

	for (pad = 0; pad < XUSB_UTMI_COUNT; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			if (tegra->soc_config->pmc_portmap)
				pmc->instance = PMC_PORTMAP_MASK(
						tegra->soc_config->pmc_portmap,
						pad);
			else
				pmc->instance = pad;
			pmc->phy_type = TEGRA_USB_PHY_INTF_UTMI;
			pmc->port_speed = USB_PMC_PORT_SPEED_UNKNOWN;
			pmc->controller_type = TEGRA_USB_3_0;
			tegra_usb_pmc_init(pmc);
		}
	}

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);
		pmc = &pmc_hsic_data[pad];
		pmc->instance = pad + 1;
		pmc->phy_type = TEGRA_USB_PHY_INTF_HSIC;
		pmc->port_speed = USB_PMC_PORT_SPEED_HIGH;
		pmc->controller_type = TEGRA_USB_3_0;
		tegra_usb_pmc_init(pmc);
	}
}

static void pmc_setup_wake_detect(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	u32 portsc;
	int port;
	int pad;

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);

		pmc = &pmc_hsic_data[pad];
		port = hsic_pad_to_port(pad);
		portsc = xhci_read_portsc(tegra->xhci, port);
		dev_dbg(dev, "%s hsic pad %d portsc 0x%x\n",
			__func__, pad, portsc);

		if (((int) portsc != -1) && (portsc & PORT_CONNECT))
			pmc->pmc_ops->setup_pmc_wake_detect(pmc);
	}

	for (pad = 0; pad < XUSB_UTMI_COUNT; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			pmc->port_speed = update_speed(tegra, pad);
			if (pad == 0) {
				if (is_otg_host(tegra))
					pmc->pmc_ops->setup_pmc_wake_detect(
									pmc);
			} else
				pmc->pmc_ops->setup_pmc_wake_detect(pmc);
		}
	}
}

static void pmc_disable_bus_ctrl(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	int pad;

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);

		pmc = &pmc_hsic_data[pad];
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
	}

	for (pad = 0; pad < XUSB_UTMI_COUNT; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
		}
	}
}

u32 csb_read(struct tegra_xhci_hcd *tegra, u32 addr)
{
	void __iomem *fpci_base = tegra->fpci_base;
	struct platform_device *pdev = tegra->pdev;
	u32 input_addr;
	u32 data;
	u32 csb_page_select;

	/* to select the appropriate CSB page to write to */
	csb_page_select = CSB_PAGE_SELECT(addr);

	dev_dbg(&pdev->dev, "csb_read: csb_page_select= 0x%08x\n",
			csb_page_select);

	iowrite32(csb_page_select, fpci_base + XUSB_CFG_ARU_C11_CSBRANGE);

	/* selects the appropriate offset in the page to read from */
	input_addr = CSB_PAGE_OFFSET(addr);
	data = ioread32(fpci_base + XUSB_CFG_CSB_BASE_ADDR + input_addr);

	dev_dbg(&pdev->dev, "csb_read: input_addr = 0x%08x data = 0x%08x\n",
			input_addr, data);
	return data;
}

void csb_write(struct tegra_xhci_hcd *tegra, u32 addr, u32 data)
{
	void __iomem *fpci_base = tegra->fpci_base;
	struct platform_device *pdev = tegra->pdev;
	u32 input_addr;
	u32 csb_page_select;

	/* to select the appropriate CSB page to write to */
	csb_page_select = CSB_PAGE_SELECT(addr);

	dev_dbg(&pdev->dev, "csb_write:csb_page_selectx = 0x%08x\n",
			csb_page_select);

	iowrite32(csb_page_select, fpci_base + XUSB_CFG_ARU_C11_CSBRANGE);

	/* selects the appropriate offset in the page to write to */
	input_addr = CSB_PAGE_OFFSET(addr);
	iowrite32(data, fpci_base + XUSB_CFG_CSB_BASE_ADDR + input_addr);

	dev_dbg(&pdev->dev, "csb_write: input_addr = 0x%08x data = %0x08x\n",
			input_addr, data);
}

static int fw_message_send(struct tegra_xhci_hcd *tegra,
	enum MBOX_CMD_TYPE type, u32 data)
{
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->fpci_base;
	unsigned long target;
	u32 reg;

	dev_dbg(dev, "%s type %d data 0x%x\n", __func__, type, data);

	mutex_lock(&tegra->mbox_lock);

	target = jiffies + msecs_to_jiffies(20);
	/* wait mailbox to become idle, timeout in 20ms */
	while (((reg = readl(base + XUSB_CFG_ARU_MBOX_OWNER)) != 0) &&
		time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->mbox_lock);
		usleep_range(100, 200);
		mutex_lock(&tegra->mbox_lock);
	}

	if (reg != 0) {
		dev_err(dev, "%s mailbox is still busy\n", __func__);
		goto timeout;
	}

	target = jiffies + msecs_to_jiffies(10);
	/* acquire mailbox , timeout in 10ms */
	writel(MBOX_OWNER_SW, base + XUSB_CFG_ARU_MBOX_OWNER);
	while (((reg = readl(base + XUSB_CFG_ARU_MBOX_OWNER)) != MBOX_OWNER_SW)
		&& time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->mbox_lock);
		usleep_range(100, 200);
		mutex_lock(&tegra->mbox_lock);
		writel(MBOX_OWNER_SW, base + XUSB_CFG_ARU_MBOX_OWNER);
	}

	if (reg != MBOX_OWNER_SW) {
		dev_err(dev, "%s acquire mailbox timeout\n", __func__);
		goto timeout;
	}

	reg = CMD_TYPE(type) | CMD_DATA(data);
	writel(reg, base + XUSB_CFG_ARU_MBOX_DATA_IN);

	reg = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	reg |= MBOX_INT_EN | MBOX_FALC_INT_EN;
	writel(reg, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	mutex_unlock(&tegra->mbox_lock);
	return 0;

timeout:
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_CMD);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_DATA_IN);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_DATA_OUT);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_OWNER);
	mutex_unlock(&tegra->mbox_lock);
	return -ETIMEDOUT;
}

/**
 * fw_log_next - find next log entry in a tegra_xhci_firmware_log context.
 *	This function takes care of wrapping. That means when current log entry
 *	is the last one, it returns with the first one.
 *
 * @param log	The tegra_xhci_firmware_log context.
 * @param this	The current log entry.
 * @return	The log entry which is next to the current one.
 */
static inline struct log_entry *fw_log_next(
		struct tegra_xhci_firmware_log *log, struct log_entry *this)
{
	struct log_entry *first = (struct log_entry *) log->virt_addr;
	struct log_entry *last = first + FW_LOG_COUNT - 1;

	WARN((this < first) || (this > last), "%s: invalid input\n", __func__);

	return (this == last) ? first : (this + 1);
}

/**
 * fw_log_update_dequeue_pointer - update dequeue pointer to both firmware and
 *	tegra_xhci_firmware_log.dequeue.
 *
 * @param log	The tegra_xhci_firmware_log context.
 * @param n	Counts of log entries to fast-forward.
 */
static inline void fw_log_update_deq_pointer(
		struct tegra_xhci_firmware_log *log, int n)
{
	struct tegra_xhci_hcd *tegra =
			container_of(log, struct tegra_xhci_hcd, log);
	struct device *dev = &tegra->pdev->dev;
	struct log_entry *deq = tegra->log.dequeue;
	dma_addr_t physical_addr;
	u32 reg;

	dev_dbg(dev, "curr 0x%p fast-forward %d entries\n", deq, n);
	while (n-- > 0)
		deq = fw_log_next(log, deq);

	tegra->log.dequeue = deq;
	physical_addr = tegra->log.phys_addr +
			((u8 *)deq - (u8 *)tegra->log.virt_addr);

	/* update dequeue pointer to firmware */
	reg = (FW_IOCTL_LOG_DEQUEUE_LOW << FW_IOCTL_TYPE_SHIFT);
	reg |= (physical_addr & 0xffff); /* lower 16-bits */
	iowrite32(reg, tegra->fpci_base + XUSB_CFG_ARU_FW_SCRATCH);

	reg = (FW_IOCTL_LOG_DEQUEUE_HIGH << FW_IOCTL_TYPE_SHIFT);
	reg |= ((physical_addr >> 16) & 0xffff); /* higher 16-bits */
	iowrite32(reg, tegra->fpci_base + XUSB_CFG_ARU_FW_SCRATCH);

	dev_dbg(dev, "new 0x%p physical addr 0x%x\n", deq, (u32)physical_addr);
}

static inline bool circ_buffer_full(struct circ_buf *circ)
{
	int space = CIRC_SPACE(circ->head, circ->tail, CIRC_BUF_SIZE);

	return (space <= FW_LOG_SIZE);
}

static inline bool fw_log_available(struct tegra_xhci_hcd *tegra)
{
	return (tegra->log.dequeue->owner == DRIVER);
}

/**
 * fw_log_wait_empty_timeout - wait firmware log thread to clean up shared
 *	log buffer.
 * @param tegra:	tegra_xhci_hcd context
 * @param msec:		timeout value in millisecond
 * @return true:	shared log buffer is empty,
 *	   false:	shared log buffer isn't empty.
 */
static inline bool fw_log_wait_empty_timeout(struct tegra_xhci_hcd *tegra,
		unsigned timeout)
{
	unsigned long target = jiffies + msecs_to_jiffies(timeout);
	bool ret;

	mutex_lock(&tegra->log.mutex);

	while (fw_log_available(tegra) && time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->log.mutex);
		usleep_range(1000, 2000);
		mutex_lock(&tegra->log.mutex);
	}

	ret = fw_log_available(tegra);
	mutex_unlock(&tegra->log.mutex);

	return ret;
}

/**
 * fw_log_copy - copy firmware log from device's buffer to driver's circular
 *	buffer.
 * @param tegra	tegra_xhci_hcd context
 * @return true,	We still have firmware log in device's buffer to copy.
 *			This function returned due the driver's circular buffer
 *			is full. Caller should invoke this function again as
 *			soon as there is space in driver's circular buffer.
 *	   false,	Device's buffer is empty.
 */
static inline bool fw_log_copy(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	int buffer_len, copy_len;
	struct log_entry *entry;
	struct log_entry *first = tegra->log.virt_addr;

	while (fw_log_available(tegra)) {

		/* calculate maximum contiguous driver buffer length */
		head = circ->head;
		tail = ACCESS_ONCE(circ->tail);
		buffer_len = CIRC_SPACE_TO_END(head, tail, CIRC_BUF_SIZE);
		/* round down to FW_LOG_SIZE */
		buffer_len -= (buffer_len % FW_LOG_SIZE);
		if (!buffer_len)
			return true; /* log available but no space left */

		/* calculate maximum contiguous log copy length */
		entry = tegra->log.dequeue;
		copy_len = 0;
		do {
			if (tegra->log.seq != entry->sequence_no) {
				dev_warn(dev,
				"%s: discontinuous seq no, expect %u get %u\n",
				__func__, tegra->log.seq, entry->sequence_no);
			}
			tegra->log.seq = entry->sequence_no + 1;

			copy_len += FW_LOG_SIZE;
			buffer_len -= FW_LOG_SIZE;
			if (!buffer_len)
				break; /* no space left */
			entry = fw_log_next(&tegra->log, entry);
		} while ((entry->owner == DRIVER) && (entry != first));

		memcpy(&circ->buf[head], tegra->log.dequeue, copy_len);
		memset(tegra->log.dequeue, 0, copy_len);
		circ->head = (circ->head + copy_len) & (CIRC_BUF_SIZE - 1);

		mb();

		fw_log_update_deq_pointer(&tegra->log, copy_len/FW_LOG_SIZE);

		dev_dbg(dev, "copied %d entries, new dequeue 0x%p\n",
				copy_len/FW_LOG_SIZE, tegra->log.dequeue);
		wake_up_interruptible(&tegra->log.read_wait);
	}

	return false;
}

static int fw_log_thread(void *data)
{
	struct tegra_xhci_hcd *tegra = data;
	struct device *dev = &tegra->pdev->dev;
	struct circ_buf *circ = &tegra->log.circ;
	bool logs_left;

	dev_dbg(dev, "start firmware log thread\n");

	do {
		mutex_lock(&tegra->log.mutex);
		if (circ_buffer_full(circ)) {
			mutex_unlock(&tegra->log.mutex);
			dev_info(dev, "%s: circ buffer full\n", __func__);
			wait_event_interruptible(tegra->log.write_wait,
			    kthread_should_stop() || !circ_buffer_full(circ));
			mutex_lock(&tegra->log.mutex);
		}

		logs_left = fw_log_copy(tegra);
		mutex_unlock(&tegra->log.mutex);

		/* relax if no logs left  */
		if (!logs_left)
			wait_event_interruptible_timeout(tegra->log.intr_wait,
				fw_log_available(tegra), FW_LOG_THREAD_RELAX);
	} while (!kthread_should_stop());

	dev_dbg(dev, "stop firmware log thread\n");
	return 0;
}

static inline bool circ_buffer_empty(struct circ_buf *circ)
{
	return (CIRC_CNT(circ->head, circ->tail, CIRC_BUF_SIZE) == 0);
}

static ssize_t fw_log_file_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp)
{
	struct tegra_xhci_hcd *tegra = file->private_data;
	struct platform_device *pdev = tegra->pdev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	size_t n = 0;
	int s;

	mutex_lock(&tegra->log.mutex);

	while (circ_buffer_empty(circ)) {
		mutex_unlock(&tegra->log.mutex);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN; /* non-blocking read */

		dev_dbg(&pdev->dev, "%s: nothing to read\n", __func__);

		if (wait_event_interruptible(tegra->log.read_wait,
				!circ_buffer_empty(circ)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&tegra->log.mutex))
			return -ERESTARTSYS;
	}

	while (count > 0) {
		head = ACCESS_ONCE(circ->head);
		tail = circ->tail;
		s = min_t(int, count,
				CIRC_CNT_TO_END(head, tail, CIRC_BUF_SIZE));

		if (s > 0) {
			if (copy_to_user(&buf[n], &circ->buf[tail], s)) {
				dev_warn(&pdev->dev, "copy_to_user failed\n");
				mutex_unlock(&tegra->log.mutex);
				return -EFAULT;
			}
			circ->tail = (circ->tail + s) & (CIRC_BUF_SIZE - 1);

			count -= s;
			n += s;
		} else
			break;
	}

	mutex_unlock(&tegra->log.mutex);

	wake_up_interruptible(&tegra->log.write_wait);

	dev_dbg(&pdev->dev, "%s: %d bytes\n", __func__, n);

	return n;
}

static int fw_log_file_open(struct inode *inode, struct file *file)
{
	struct tegra_xhci_hcd *tegra;
	file->private_data = inode->i_private;
	tegra = file->private_data;

	if (test_and_set_bit(FW_LOG_FILE_OPENED, &tegra->log.flags)) {
		dev_info(&tegra->pdev->dev, "%s: already opened\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static int fw_log_file_close(struct inode *inode, struct file *file)
{
	struct tegra_xhci_hcd *tegra = file->private_data;

	clear_bit(FW_LOG_FILE_OPENED, &tegra->log.flags);

	return 0;
}

static const struct file_operations firmware_log_fops = {
		.open		= fw_log_file_open,
		.release	= fw_log_file_close,
		.read		= fw_log_file_read,
		.owner		= THIS_MODULE,
};

static int fw_log_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int rc = 0;

	/* allocate buffer to be shared between driver and firmware */
	tegra->log.virt_addr = dma_alloc_writecombine(&pdev->dev,
			FW_LOG_RING_SIZE, &tegra->log.phys_addr, GFP_KERNEL);

	if (!tegra->log.virt_addr) {
		dev_err(&pdev->dev, "dma_alloc_writecombine() size %d failed\n",
				FW_LOG_RING_SIZE);
		return -ENOMEM;
	}

	dev_info(&pdev->dev,
		"%d bytes log buffer physical 0x%u virtual 0x%p\n",
		FW_LOG_RING_SIZE, (u32)tegra->log.phys_addr,
		tegra->log.virt_addr);

	memset(tegra->log.virt_addr, 0, FW_LOG_RING_SIZE);
	tegra->log.dequeue = tegra->log.virt_addr;

	tegra->log.circ.buf = vmalloc(CIRC_BUF_SIZE);
	if (!tegra->log.circ.buf) {
		dev_err(&pdev->dev, "vmalloc size %d failed\n", CIRC_BUF_SIZE);
		rc = -ENOMEM;
		goto error_free_dma;
	}

	tegra->log.circ.head = 0;
	tegra->log.circ.tail = 0;

	init_waitqueue_head(&tegra->log.read_wait);
	init_waitqueue_head(&tegra->log.write_wait);
	init_waitqueue_head(&tegra->log.intr_wait);

	mutex_init(&tegra->log.mutex);

	tegra->log.path = debugfs_create_dir("tegra_xhci", NULL);
	if (IS_ERR_OR_NULL(tegra->log.path)) {
		dev_warn(&pdev->dev, "debugfs_create_dir() failed\n");
		rc = -ENOMEM;
		goto error_free_mem;
	}

	tegra->log.log_file = debugfs_create_file("firmware_log",
			S_IRUGO, tegra->log.path, tegra, &firmware_log_fops);
	if ((!tegra->log.log_file) ||
			(tegra->log.log_file == ERR_PTR(-ENODEV))) {
		dev_warn(&pdev->dev, "debugfs_create_file() failed\n");
		rc = -ENOMEM;
		goto error_remove_debugfs_path;
	}

	tegra->log.thread = kthread_run(fw_log_thread, tegra, "xusb-fw-log");
	if (IS_ERR(tegra->log.thread)) {
		dev_warn(&pdev->dev, "kthread_run() failed\n");
		rc = -ENOMEM;
		goto error_remove_debugfs_file;
	}

	set_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags);
	return rc;

error_remove_debugfs_file:
	debugfs_remove(tegra->log.log_file);
error_remove_debugfs_path:
	debugfs_remove(tegra->log.path);
error_free_mem:
	vfree(tegra->log.circ.buf);
error_free_dma:
	dma_free_writecombine(&pdev->dev, FW_LOG_RING_SIZE,
			tegra->log.virt_addr, tegra->log.phys_addr);
	memset(&tegra->log, sizeof(tegra->log), 0);
	return rc;
}

static void fw_log_deinit(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;

	if (test_and_clear_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {

		debugfs_remove(tegra->log.log_file);
		debugfs_remove(tegra->log.path);

		wake_up_interruptible(&tegra->log.read_wait);
		wake_up_interruptible(&tegra->log.write_wait);
		kthread_stop(tegra->log.thread);

		mutex_lock(&tegra->log.mutex);
		dma_free_writecombine(&pdev->dev, FW_LOG_RING_SIZE,
			tegra->log.virt_addr, tegra->log.phys_addr);
		vfree(tegra->log.circ.buf);
		tegra->log.circ.head = tegra->log.circ.tail = 0;
		mutex_unlock(&tegra->log.mutex);

		mutex_destroy(&tegra->log.mutex);
	}
}

/* hsic pad operations */
/*
 * HSIC pads need VDDIO_HSIC power rail turned on to be functional. There is
 * only one VDDIO_HSIC power rail shared by all HSIC pads.
 */
static int hsic_power_rail_enable(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	struct tegra_xusb_regulator_name *supply = &tegra->bdata->supply;
	int ret;

	if (tegra->vddio_hsic_reg)
		goto done;

	tegra->vddio_hsic_reg = devm_regulator_get(dev, supply->vddio_hsic);
	if (IS_ERR_OR_NULL(tegra->vddio_hsic_reg)) {
		dev_err(dev, "%s get vddio_hsic failed\n", __func__);
		ret = PTR_ERR(tegra->vddio_hsic_reg);
		goto get_failed;
	}

	dev_dbg(dev, "%s regulator_enable vddio_hsic\n", __func__);
	ret = regulator_enable(tegra->vddio_hsic_reg);
	if (ret < 0) {
		dev_err(dev, "%s enable vddio_hsic failed\n", __func__);
		goto enable_failed;
	}

done:
	tegra->vddio_hsic_refcnt++;
	WARN(tegra->vddio_hsic_refcnt > XUSB_HSIC_COUNT,
			"vddio_hsic_refcnt exceeds\n");
	return 0;

enable_failed:
	devm_regulator_put(tegra->vddio_hsic_reg);
get_failed:
	tegra->vddio_hsic_reg = NULL;
	return ret;
}

static int hsic_power_rail_disable(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	int ret;

	WARN_ON(!tegra->vddio_hsic_reg || !tegra->vddio_hsic_refcnt);

	tegra->vddio_hsic_refcnt--;
	if (tegra->vddio_hsic_refcnt)
		return 0;

	dev_dbg(dev, "%s regulator_disable vddio_hsic\n", __func__);
	ret = regulator_disable(tegra->vddio_hsic_reg);
	if (ret < 0) {
		dev_err(dev, "%s disable vddio_hsic failed\n", __func__);
		tegra->vddio_hsic_refcnt++;
		return ret;
	}

	devm_regulator_put(tegra->vddio_hsic_reg);
	tegra->vddio_hsic_reg = NULL;

	return 0;
}

static int hsic_pad_enable(struct tegra_xhci_hcd *tegra, unsigned pad)
{
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->padctl_base;
	struct tegra_xusb_hsic_config *hsic = &tegra->bdata->hsic[pad];
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	reg = readl(base + HSIC_PAD_CTL_2(pad));
	reg &= ~(RX_STROBE_TRIM(~0) | RX_DATA_TRIM(~0));
	reg |= RX_STROBE_TRIM(hsic->rx_strobe_trim);
	reg |= RX_DATA_TRIM(hsic->rx_data_trim);
	writel(reg, base + HSIC_PAD_CTL_2(pad));

	reg = readl(base + HSIC_PAD_CTL_0(pad));
	reg &= ~(TX_RTUNEP(~0) | TX_RTUNEN(~0) | TX_SLEWP(~0) | TX_SLEWN(~0));
	reg |= TX_RTUNEP(hsic->tx_rtune_p);
	reg |= TX_RTUNEN(hsic->tx_rtune_n);
	reg |= TX_SLEWP(hsic->tx_slew_p);
	reg |= TX_SLEWN(hsic->tx_slew_n);
	writel(reg, base + HSIC_PAD_CTL_0(pad));

	reg = readl(base + HSIC_PAD_CTL_1(pad));
	reg &= ~(RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE);
	reg |= (RPD_DATA | RPU_STROBE); /* keep HSIC in IDLE */
	if (hsic->auto_term_en)
		reg |= AUTO_TERM_EN;
	else
		reg &= ~AUTO_TERM_EN;
	reg &= ~(PD_RX | HSIC_PD_ZI | PD_TRX | PD_TX);
	writel(reg, base + HSIC_PAD_CTL_1(pad));

	/* Wait for 25 us */
	usleep_range(25, 50);

	/* Power down tracking circuit */
	reg = readl(base + HSIC_PAD_CTL_1(pad));
	reg |= PD_TRX;
	writel(reg, base + HSIC_PAD_CTL_1(pad));

	reg = readl(base + HSIC_STRB_TRIM_CONTROL);
	reg &= ~(STRB_TRIM_VAL(~0));
	reg |= STRB_TRIM_VAL(hsic->strb_trim_val);
	writel(reg, base + HSIC_STRB_TRIM_CONTROL);

	reg = readl(base + USB2_PAD_MUX);
	reg |= USB2_HSIC_PAD_PORT(pad);
	writel(reg, base + USB2_PAD_MUX);

	reg_dump(dev, base, HSIC_PAD_CTL_0(pad));
	reg_dump(dev, base, HSIC_PAD_CTL_1(pad));
	reg_dump(dev, base, HSIC_PAD_CTL_2(pad));
	reg_dump(dev, base, HSIC_STRB_TRIM_CONTROL);
	reg_dump(dev, base, USB2_PAD_MUX);
	return 0;
}

static void hsic_pad_pretend_connect(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	struct tegra_xusb_hsic_config *hsic;
	struct usb_device *hs_root_hub = tegra->xhci->main_hcd->self.root_hub;
	int pad;
	u32 portsc;
	int port;
	int enabled_pads = 0;
	unsigned long wait_ports = 0;
	unsigned long target;

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic = &tegra->bdata->hsic[pad];
		if (hsic->pretend_connect)
			enabled_pads++;
	}

	if (enabled_pads == 0) {
		dev_dbg(dev, "%s no hsic pretend_connect enabled\n", __func__);
		return;
	}

	usb_disable_autosuspend(hs_root_hub);

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic = &tegra->bdata->hsic[pad];
		if (!hsic->pretend_connect)
			continue;

		port = hsic_pad_to_port(pad);
		portsc = xhci_read_portsc(tegra->xhci, port);
		dev_dbg(dev, "%s pad %u portsc 0x%x\n", __func__, pad, portsc);

		if (!(portsc & PORT_CONNECT)) {
			/* firmware wants 1-based port index */
			fw_message_send(tegra,
				MBOX_CMD_HSIC_PRETEND_CONNECT, BIT(port + 1));
		}

		set_bit(port, &wait_ports);
	}

	/* wait till port reaches U0 */
	target = jiffies + msecs_to_jiffies(500);
	do {
		for_each_set_bit(port, &wait_ports, BITS_PER_LONG) {
			portsc = xhci_read_portsc(tegra->xhci, port);
			pad = port_to_hsic_pad(port);
			dev_dbg(dev, "%s pad %u portsc 0x%x\n", __func__,
				pad, portsc);
			if ((PORT_PLS_MASK & portsc) == XDEV_U0)
				clear_bit(port, &wait_ports);
		}

		if (wait_ports)
			usleep_range(1000, 5000);
	} while (wait_ports && time_is_after_jiffies(target));

	if (wait_ports)
		dev_warn(dev, "%s HSIC pad(s) didn't reach U0.\n", __func__);

	usb_enable_autosuspend(hs_root_hub);

	return;
}

static int hsic_pad_disable(struct tegra_xhci_hcd *tegra, unsigned pad)
{
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->padctl_base;
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	reg = readl(base + USB2_PAD_MUX);
	reg &= ~USB2_HSIC_PAD_PORT(pad);
	writel(reg, base + USB2_PAD_MUX);

	reg = readl(base + HSIC_PAD_CTL_1(pad));
	reg |= (PD_RX | HSIC_PD_ZI | PD_TRX | PD_TX);
	writel(reg, base + HSIC_PAD_CTL_1(pad));

	reg_dump(dev, base, HSIC_PAD_CTL_0(pad));
	reg_dump(dev, base, HSIC_PAD_CTL_1(pad));
	reg_dump(dev, base, HSIC_PAD_CTL_2(pad));
	reg_dump(dev, base, USB2_PAD_MUX);
	return 0;
}

enum hsic_pad_pupd {
	PUPD_DISABLE = 0,
	PUPD_IDLE,
	PUPD_RESET
};

static int hsic_pad_pupd_set(struct tegra_xhci_hcd *tegra, unsigned pad,
	enum hsic_pad_pupd pupd)
{
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->padctl_base;
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %u\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u pupd %d\n", __func__, pad, pupd);

	reg = readl(base + HSIC_PAD_CTL_1(pad));
	reg &= ~(RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE);

	if (pupd == PUPD_IDLE)
		reg |= (RPD_DATA | RPU_STROBE);
	else if (pupd == PUPD_RESET)
		reg |= (RPD_DATA | RPD_STROBE);
	else if (pupd != PUPD_DISABLE) {
		dev_err(dev, "%s invalid pupd %d\n", __func__, pupd);
		return -EINVAL;
	}

	writel(reg, base + HSIC_PAD_CTL_1(pad));

	reg_dump(dev, base, HSIC_PAD_CTL_1(pad));

	return 0;
}


static void tegra_xhci_debug_read_pads(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	xhci_info(xhci, "============ PADCTL VALUES START =================\n");
	reg = readl(tegra->padctl_base + padregs->usb2_pad_mux_0);
	xhci_info(xhci, " PAD MUX = %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_port_cap_0);
	xhci_info(xhci, " PORT CAP = %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->snps_oc_map_0);
	xhci_info(xhci, " SNPS OC MAP = %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_oc_map_0);
	xhci_info(xhci, " USB2 OC MAP = %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->ss_port_map_0);
	xhci_info(xhci, " SS PORT MAP = %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->oc_det_0);
	xhci_info(xhci, " OC DET 0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->iophy_usb3_pad0_ctl2_0);
	xhci_info(xhci, " iophy_usb3_pad0_ctl2_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->iophy_usb3_pad1_ctl2_0);
	xhci_info(xhci, " iophy_usb3_pad1_ctl2_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_otg_pad0_ctl0_0);
	xhci_info(xhci, " usb2_otg_pad0_ctl0_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_otg_pad1_ctl0_0);
	xhci_info(xhci, " usb2_otg_pad1_ctl0_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_otg_pad0_ctl1_0);
	xhci_info(xhci, " usb2_otg_pad0_ctl1_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_otg_pad1_ctl1_0);
	xhci_info(xhci, " usb2_otg_pad1_ctl1_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);
	xhci_info(xhci, " usb2_bias_pad_ctl0_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_hsic_pad0_ctl0_0);
	xhci_info(xhci, " usb2_hsic_pad0_ctl0_0= %x\n", reg);
	reg = readl(tegra->padctl_base + padregs->usb2_hsic_pad1_ctl0_0);
	xhci_info(xhci, " usb2_hsic_pad1_ctl0_0= %x\n", reg);
	xhci_info(xhci, "============ PADCTL VALUES END=================\n");
}

static void tegra_xhci_cfg(struct tegra_xhci_hcd *tegra)
{
	u32 reg;

	reg = readl(tegra->ipfs_base + IPFS_XUSB_HOST_CONFIGURATION_0);
	reg |= IPFS_EN_FPCI;
	writel(reg, tegra->ipfs_base + IPFS_XUSB_HOST_CONFIGURATION_0);
	udelay(10);

	/* Program Bar0 Space */
	reg = readl(tegra->fpci_base + XUSB_CFG_4);
	reg |= tegra->host_phy_base;
	writel(reg, tegra->fpci_base + XUSB_CFG_4);
	usleep_range(100, 200);

	/* Enable Bus Master */
	reg = readl(tegra->fpci_base + XUSB_CFG_1);
	reg |= 0x7;
	writel(reg, tegra->fpci_base + XUSB_CFG_1);

	/* Set intr mask to enable intr assertion */
	reg = readl(tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);
	reg |= IPFS_IP_INT_MASK;
	writel(reg, tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	/* Set hysteris to 0x80 */
	writel(0x80, tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);
}

static int tegra_xusb_regulator_init(struct tegra_xhci_hcd *tegra,
		struct platform_device *pdev)
{
	struct tegra_xusb_regulator_name *supply = &tegra->bdata->supply;
	int i;
	int err = 0;

	tegra->xusb_s3p3v_reg =
			devm_regulator_get(&pdev->dev, supply->s3p3v);
	if (IS_ERR(tegra->xusb_s3p3v_reg)) {
		dev_err(&pdev->dev, "3p3v: regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s3p3v_reg));
		err = PTR_ERR(tegra->xusb_s3p3v_reg);
		goto err_null_regulator;
	} else {
		err = regulator_enable(tegra->xusb_s3p3v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
				"3p3v: regulator enable failed:%d\n", err);
			goto err_null_regulator;
		}
	}

	/* enable utmi vbuses */
	memset(tegra->xusb_utmi_vbus_regs, 0,
			sizeof(tegra->xusb_utmi_vbus_regs));
	for (i = 0; i < XUSB_UTMI_COUNT; i++) {
		struct regulator *reg = NULL;
		const char *reg_name = supply->utmi_vbuses[i];
		if (BIT(XUSB_UTMI_INDEX + i) & tegra->bdata->portmap) {
			if (i == 0 && tegra->transceiver)
				continue;
			reg = devm_regulator_get(&pdev->dev, reg_name);
			if (IS_ERR(reg)) {
				dev_err(&pdev->dev,
					"%s regulator not found: %ld.",
					reg_name, PTR_ERR(reg));
				err = PTR_ERR(reg);
			} else {
				err = regulator_enable(reg);
				if (err < 0) {
					dev_err(&pdev->dev,
					"%s: regulator enable failed: %d\n",
					reg_name, err);
				}
			}
			if (err)
				goto err_put_utmi_vbus_reg;
		}
		tegra->xusb_utmi_vbus_regs[i] = reg;
	}

	tegra->xusb_s1p8v_reg =
		devm_regulator_get(&pdev->dev, supply->s1p8v);
	if (IS_ERR(tegra->xusb_s1p8v_reg)) {
		dev_err(&pdev->dev, "1p8v regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s1p8v_reg));
		err = PTR_ERR(tegra->xusb_s1p8v_reg);
		goto err_put_utmi_vbus_reg;
	} else {
		err = regulator_enable(tegra->xusb_s1p8v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
			"1p8v: regulator enable failed:%d\n", err);
			goto err_put_utmi_vbus_reg;
		}
	}

	tegra->xusb_s1p05v_reg =
			devm_regulator_get(&pdev->dev, supply->s1p05v);
	if (IS_ERR(tegra->xusb_s1p05v_reg)) {
		dev_err(&pdev->dev, "1p05v: regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s1p05v_reg));
		err = PTR_ERR(tegra->xusb_s1p05v_reg);
		goto err_put_s1p8v_reg;
	} else {
		err = regulator_enable(tegra->xusb_s1p05v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
			"1p05v: regulator enable failed:%d\n", err);
			goto err_put_s1p8v_reg;
		}
	}

	return err;

err_put_s1p8v_reg:
	regulator_disable(tegra->xusb_s1p8v_reg);
err_put_utmi_vbus_reg:
	for (i = 0; i < XUSB_UTMI_COUNT; i++) {
		struct regulator *reg = tegra->xusb_utmi_vbus_regs[i];
		if (!IS_ERR_OR_NULL(reg))
			regulator_disable(reg);
	}
	regulator_disable(tegra->xusb_s3p3v_reg);
err_null_regulator:
	for (i = 0; i < XUSB_UTMI_COUNT; i++)
		tegra->xusb_utmi_vbus_regs[i] = NULL;
	tegra->xusb_s1p05v_reg = NULL;
	tegra->xusb_s3p3v_reg = NULL;
	tegra->xusb_s1p8v_reg = NULL;
	return err;
}

static void tegra_xusb_regulator_deinit(struct tegra_xhci_hcd *tegra)
{
	int i;

	regulator_disable(tegra->xusb_s1p05v_reg);
	regulator_disable(tegra->xusb_s1p8v_reg);

	for (i = 0; i < XUSB_UTMI_COUNT; i++) {
		if (BIT(XUSB_UTMI_INDEX + i) & tegra->bdata->portmap) {
			struct regulator *reg = tegra->xusb_utmi_vbus_regs[i];
			if (!IS_ERR_OR_NULL(reg))
				regulator_disable(reg);
			tegra->xusb_utmi_vbus_regs[i] = NULL;
		}
	}

	regulator_disable(tegra->xusb_s3p3v_reg);

	tegra->xusb_s1p05v_reg = NULL;
	tegra->xusb_s1p8v_reg = NULL;
	tegra->xusb_s3p3v_reg = NULL;
}

/*
 * We need to enable only plle_clk as pllu_clk, utmip_clk and plle_re_vco_clk
 * are under hardware control
 */
static int tegra_usb2_clocks_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int err = 0;

	tegra->plle_clk = devm_clk_get(&pdev->dev, "pll_e");
	if (IS_ERR(tegra->plle_clk)) {
		dev_err(&pdev->dev, "%s: Failed to get plle clock\n", __func__);
		err = PTR_ERR(tegra->plle_clk);
		return err;
	}
	err = clk_enable(tegra->plle_clk);
	if (err) {
		dev_err(&pdev->dev, "%s: could not enable plle clock\n",
			__func__);
		return err;
	}

	return err;
}

static void tegra_usb2_clocks_deinit(struct tegra_xhci_hcd *tegra)
{
	clk_disable(tegra->plle_clk);
	tegra->plle_clk = NULL;
}

static int tegra_xusb_partitions_clk_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int err = 0;

	tegra->emc_clk = devm_clk_get(&pdev->dev, "emc");
	if (IS_ERR(tegra->emc_clk)) {
		dev_err(&pdev->dev, "Failed to get xusb.emc clock\n");
		return PTR_ERR(tegra->emc_clk);
	}

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		tegra->pll_re_vco_clk = devm_clk_get(&pdev->dev, "pll_re_vco");
		if (IS_ERR(tegra->pll_re_vco_clk)) {
			dev_err(&pdev->dev, "Failed to get refPLLE clock\n");
			err = PTR_ERR(tegra->pll_re_vco_clk);
			goto get_pll_re_vco_clk_failed;
		}
	}

	/* get the clock handle of 120MHz clock source */
	tegra->pll_u_480M = devm_clk_get(&pdev->dev, "pll_u_480M");
	if (IS_ERR(tegra->pll_u_480M)) {
		dev_err(&pdev->dev, "Failed to get pll_u_480M clk handle\n");
		err = PTR_ERR(tegra->pll_u_480M);
		goto get_pll_u_480M_failed;
	}

	/* get the clock handle of 12MHz clock source */
	tegra->clk_m = devm_clk_get(&pdev->dev, "clk_m");
	if (IS_ERR(tegra->clk_m)) {
		dev_err(&pdev->dev, "Failed to get clk_m clk handle\n");
		err = PTR_ERR(tegra->clk_m);
		goto clk_get_clk_m_failed;
	}

	tegra->ss_src_clk = devm_clk_get(&pdev->dev, "ss_src");
	if (IS_ERR(tegra->ss_src_clk)) {
		dev_err(&pdev->dev, "Failed to get SSPI clk\n");
		err = PTR_ERR(tegra->ss_src_clk);
		tegra->ss_src_clk = NULL;
		goto get_ss_src_clk_failed;
	}

	tegra->host_clk = devm_clk_get(&pdev->dev, "host");
	if (IS_ERR(tegra->host_clk)) {
		dev_err(&pdev->dev, "Failed to get host partition clk\n");
		err = PTR_ERR(tegra->host_clk);
		tegra->host_clk = NULL;
		goto get_host_clk_failed;
	}

	tegra->ss_clk = devm_clk_get(&pdev->dev, "ss");
	if (IS_ERR(tegra->ss_clk)) {
		dev_err(&pdev->dev, "Failed to get ss partition clk\n");
		err = PTR_ERR(tegra->ss_clk);
		tegra->ss_clk = NULL;
		goto get_ss_clk_failed;
	}

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		err = clk_enable(tegra->pll_re_vco_clk);
		if (err) {
			dev_err(&pdev->dev, "Failed to enable refPLLE clk\n");
			goto enable_pll_re_vco_clk_failed;
		}
	}
	return 0;

enable_pll_re_vco_clk_failed:
	tegra->ss_clk = NULL;

get_ss_clk_failed:
	tegra->host_clk = NULL;

get_host_clk_failed:
	tegra->ss_src_clk = NULL;

get_ss_src_clk_failed:
	tegra->clk_m = NULL;

clk_get_clk_m_failed:
	tegra->pll_u_480M = NULL;

get_pll_u_480M_failed:
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		tegra->pll_re_vco_clk = NULL;

get_pll_re_vco_clk_failed:
	tegra->emc_clk = NULL;

	return err;
}

static void tegra_xusb_partitions_clk_deinit(struct tegra_xhci_hcd *tegra)
{
	clk_disable(tegra->ss_clk);
	clk_disable(tegra->host_clk);
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_disable(tegra->pll_re_vco_clk);
	tegra->ss_clk = NULL;
	tegra->host_clk = NULL;
	tegra->ss_src_clk = NULL;
	tegra->clk_m = NULL;
	tegra->pll_u_480M = NULL;
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		tegra->pll_re_vco_clk = NULL;
}

static void tegra_xhci_rx_idle_mode_override(struct tegra_xhci_hcd *tegra,
	bool enable)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 != tegra->device_id)
		return;

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P0) {
		reg = readl(tegra->padctl_base +
			padregs->iophy_misc_pad_p0_ctl3_0);
		if (enable) {
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
		} else {
			reg |= RX_IDLE_MODE;
			reg &= ~RX_IDLE_MODE_OVRD;
		}
		writel(reg, tegra->padctl_base +
			padregs->iophy_misc_pad_p0_ctl3_0);
	}

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P1) {
		reg = readl(tegra->padctl_base +
			padregs->iophy_misc_pad_p1_ctl3_0);
		if (enable) {
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
		} else {
			reg |= RX_IDLE_MODE;
			reg &= ~RX_IDLE_MODE_OVRD;
		}
		writel(reg, tegra->padctl_base +
			padregs->iophy_misc_pad_p1_ctl3_0);

		/* SATA lane also if USB3_SS port1 mapped to it */
		if (XUSB_DEVICE_ID_T114 != tegra->device_id &&
				tegra->bdata->lane_owner & BIT(0)) {
			reg = readl(tegra->padctl_base +
				padregs->iophy_misc_pad_s0_ctl3_0);
			if (enable) {
				reg &= ~RX_IDLE_MODE;
				reg |= RX_IDLE_MODE_OVRD;
			} else {
				reg |= RX_IDLE_MODE;
				reg &= ~RX_IDLE_MODE_OVRD;
			}
			writel(reg, tegra->padctl_base +
				padregs->iophy_misc_pad_s0_ctl3_0);
		}
	}
}

/* Enable ss clk, host clk, falcon clk,
 * fs clk, dev clk, plle and refplle
 */

static int
tegra_xusb_request_clk_rate(struct tegra_xhci_hcd *tegra,
		struct clk *clk_handle, u32 rate, u32 *sw_resp)
{
	int ret = 0;
	enum MBOX_CMD_TYPE cmd_ack = MBOX_CMD_ACK;
	int fw_req_rate = rate, cur_rate;

	/* Do not handle clock change as needed for HS disconnect issue */
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		*sw_resp = CMD_DATA(fw_req_rate) | CMD_TYPE(MBOX_CMD_ACK);
		return ret;
	}

	/* frequency request from firmware is in KHz.
	 * Convert it to MHz
	 */

	/* get current rate of clock */
	cur_rate = clk_get_rate(clk_handle);
	cur_rate /= 1000;

	if (fw_req_rate == cur_rate) {
		cmd_ack = MBOX_CMD_ACK;

	} else {

		if (clk_handle == tegra->ss_src_clk && fw_req_rate == 12000) {
			/* Change SS clock source to CLK_M at 12MHz */
			clk_set_parent(clk_handle, tegra->clk_m);
			clk_set_rate(clk_handle, fw_req_rate * 1000);

			/* save leakage power when SS freq is being decreased */
			tegra_xhci_rx_idle_mode_override(tegra, true);
		} else if (clk_handle == tegra->ss_src_clk &&
				fw_req_rate == 120000) {
			/* Change SS clock source to HSIC_480 at 120MHz */
			clk_set_rate(clk_handle,  3000 * 1000);
			clk_set_parent(clk_handle, tegra->pll_u_480M);

			/* clear ovrd bits when SS freq is being increased */
			tegra_xhci_rx_idle_mode_override(tegra, false);
		}

		cur_rate = (clk_get_rate(clk_handle) / 1000);

		if (cur_rate != fw_req_rate) {
			xhci_err(tegra->xhci, "cur_rate=%d, fw_req_rate=%d\n",
				cur_rate, fw_req_rate);
			cmd_ack = MBOX_CMD_NACK;
		}
	}
	*sw_resp = CMD_DATA(cur_rate) | CMD_TYPE(cmd_ack);
	return ret;
}

static void tegra_xusb_set_bw(struct tegra_xhci_hcd *tegra, unsigned int bw)
{
	unsigned int freq_khz;

	freq_khz = tegra_emc_bw_to_freq_req(bw);
	clk_set_rate(tegra->emc_clk, freq_khz * 1000);
}

static void tegra_xhci_save_dfe_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 offset;
	u32 reg;

	if (port > (XUSB_SS_PORT_COUNT - 1)) {
		pr_err("%s invalid SS port number %u\n", __func__, port);
		return;
	}

	xhci_info(xhci, "saving restore DFE context for port %d\n", port);

	/* if port1 is mapped to SATA lane then read from SATA register */
	if (port == 1 && XUSB_DEVICE_ID_T114 != tegra->device_id &&
			tegra->bdata->lane_owner & BIT(0))
		offset = padregs->iophy_misc_pad_s0_ctl6_0;
	else
		offset = MISC_PAD_CTL_6_0(port);

	/*
	 * Value set to IOPHY_MISC_PAD_x_CTL_6 where x P0/P1/S0/ is from,
	 * T114 refer PG USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 * T124 refer PG T124_USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 */
	reg = readl(tegra->padctl_base + offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x32);
	writel(reg, tegra->padctl_base + offset);

	reg = readl(tegra->padctl_base + offset);
	tegra->sregs.tap1_val[port] = MISC_OUT_TAP_VAL(reg);

	reg = readl(tegra->padctl_base + offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x33);
	writel(reg, tegra->padctl_base + offset);

	reg = readl(tegra->padctl_base + offset);
	tegra->sregs.amp_val[port] = MISC_OUT_AMP_VAL(reg);

	reg = readl(tegra->padctl_base + USB3_PAD_CTL_4_0(port));
	reg &= ~DFE_CNTL_TAP_VAL(~0);
	reg |= DFE_CNTL_TAP_VAL(tegra->sregs.tap1_val[port]);
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_4_0(port));

	reg = readl(tegra->padctl_base + USB3_PAD_CTL_4_0(port));
	reg &= ~DFE_CNTL_AMP_VAL(~0);
	reg |= DFE_CNTL_AMP_VAL(tegra->sregs.amp_val[port]);
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_4_0(port));

	tegra->dfe_ctx_saved[port] = true;
}

static void save_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 offset;
	u32 reg;

	if (port > (XUSB_SS_PORT_COUNT - 1)) {
		pr_err("%s invalid SS port number %u\n", __func__, port);
		return;
	}

	xhci_info(xhci, "saving restore CTLE context for port %d\n", port);

	/* if port1 is mapped to SATA lane then read from SATA register */
	if (port == 1 && XUSB_DEVICE_ID_T114 != tegra->device_id &&
			tegra->bdata->lane_owner & BIT(0))
		offset = padregs->iophy_misc_pad_s0_ctl6_0;
	else
		offset = MISC_PAD_CTL_6_0(port);

	/*
	 * Value set to IOPHY_MISC_PAD_x_CTL_6 where x P0/P1/S0/ is from,
	 * T114 refer PG USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 * T124 refer PG T124_USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 */
	reg = readl(tegra->padctl_base + offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0xa1);
	writel(reg, tegra->padctl_base + offset);

	reg = readl(tegra->padctl_base + offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x21);
	writel(reg, tegra->padctl_base + offset);

	reg = readl(tegra->padctl_base + offset);
	tegra->sregs.ctle_g_val[port] = MISC_OUT_G_Z_VAL(reg);

	reg = readl(tegra->padctl_base + offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x48);
	writel(reg, tegra->padctl_base + offset);

	reg = readl(tegra->padctl_base + offset);
	tegra->sregs.ctle_z_val[port] = MISC_OUT_G_Z_VAL(reg);

	reg = readl(tegra->padctl_base + USB3_PAD_CTL_2_0(port));
	reg &= ~RX_EQ_Z_VAL(~0);
	reg |= RX_EQ_Z_VAL(tegra->sregs.ctle_z_val[port]);
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_2_0(port));

	reg = readl(tegra->padctl_base + USB3_PAD_CTL_2_0(port));
	reg &= ~RX_EQ_G_VAL(~0);
	reg |= RX_EQ_G_VAL(tegra->sregs.ctle_g_val[port]);
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_2_0(port));

	tegra->ctle_ctx_saved[port] = true;
}

static void tegra_xhci_restore_dfe_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	/* don't restore if not saved */
	if (tegra->dfe_ctx_saved[port] == false)
		return;

	xhci_info(xhci, "restoring dfe context of port %d\n", port);

	/* restore dfe_cntl for the port */
	reg = readl(tegra->padctl_base + USB3_PAD_CTL_4_0(port));
	reg &= ~(DFE_CNTL_AMP_VAL(~0) |
			DFE_CNTL_TAP_VAL(~0));
	reg |= DFE_CNTL_AMP_VAL(tegra->sregs.amp_val[port]) |
		DFE_CNTL_TAP_VAL(tegra->sregs.tap1_val[port]);
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_4_0(port));
}

void restore_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	/* don't restore if not saved */
	if (tegra->ctle_ctx_saved[port] == false)
		return;

	xhci_info(xhci, "restoring CTLE context of port %d\n", port);

	/* restore ctle for the port */
	reg = readl(tegra->padctl_base + USB3_PAD_CTL_2_0(port));
	reg &= ~(RX_EQ_Z_VAL(~0) |
			RX_EQ_G_VAL(~0));
	reg |= (RX_EQ_Z_VAL(tegra->sregs.ctle_z_val[port]) |
		RX_EQ_G_VAL(tegra->sregs.ctle_g_val[port]));
	writel(reg, tegra->padctl_base + USB3_PAD_CTL_2_0(port));
}

static void tegra_xhci_program_ulpi_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	reg = readl(tegra->padctl_base + padregs->usb2_pad_mux_0);
	reg &= ~USB2_ULPI_PAD;
	reg |= USB2_ULPI_PAD_OWNER_XUSB;
	writel(reg, tegra->padctl_base + padregs->usb2_pad_mux_0);

	reg = readl(tegra->padctl_base + padregs->usb2_port_cap_0);
	reg &= ~USB2_ULPI_PORT_CAP;
	reg |= (tegra->bdata->ulpicap << 24);
	writel(reg, tegra->padctl_base + padregs->usb2_port_cap_0);
	/* FIXME: Program below when more details available
	 * XUSB_PADCTL_ULPI_LINK_TRIM_CONTROL_0
	 * XUSB_PADCTL_ULPI_NULL_CLK_TRIM_CONTROL_0
	 */
}

static void tegra_xhci_program_utmip_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;
	u32 ctl0_offset, ctl1_offset;

	reg = readl(tegra->padctl_base + padregs->usb2_pad_mux_0);
	reg &= ~USB2_OTG_PAD_PORT_MASK(port);
	reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(port);
	writel(reg, tegra->padctl_base + padregs->usb2_pad_mux_0);

	reg = readl(tegra->padctl_base + padregs->usb2_port_cap_0);
	reg &= ~USB2_PORT_CAP_MASK(port);
	reg |= USB2_PORT_CAP_HOST(port);
	writel(reg, tegra->padctl_base + padregs->usb2_port_cap_0);

	/*
	 * Modify only the bits which belongs to the port
	 * and enable respective VBUS_PAD for the port
	 */
	if (tegra->bdata->uses_external_pmic == false) {
		reg = readl(tegra->padctl_base + padregs->oc_det_0);
		reg &= ~(port == 2 ? OC_DET_VBUS_ENABLE2_OC_MAP :
			port ? OC_DET_VBUS_ENABLE1_OC_MAP :
				OC_DET_VBUS_ENABLE0_OC_MAP);

		reg |= (port == 2) ? OC_DET_VBUS_EN2_OC_DETECTED_VBUS_PAD2 :
			port ? OC_DET_VBUS_EN1_OC_DETECTED_VBUS_PAD1 :
				OC_DET_VBUS_EN0_OC_DETECTED_VBUS_PAD0;
		writel(reg, tegra->padctl_base + padregs->oc_det_0);
	}
	/*
	 * enable respective VBUS_PAD if port is mapped to any SS port
	 */
	reg = readl(tegra->padctl_base + padregs->usb2_oc_map_0);
	reg &= ~((port == 2) ? USB2_OC_MAP_PORT2 :
		port ? USB2_OC_MAP_PORT1 : USB2_OC_MAP_PORT0);
	reg |= (0x4 | port) << (port * 3);
	writel(reg, tegra->padctl_base + padregs->usb2_oc_map_0);

	ctl0_offset = (port == 2) ? padregs->usb2_otg_pad2_ctl0_0 :
			port ? padregs->usb2_otg_pad1_ctl0_0 :
				padregs->usb2_otg_pad0_ctl0_0;
	ctl1_offset = (port == 2) ? padregs->usb2_otg_pad2_ctl1_0 :
			port ? padregs->usb2_otg_pad1_ctl1_0 :
				padregs->usb2_otg_pad0_ctl1_0;

	reg = readl(tegra->padctl_base + ctl0_offset);
	reg &= ~(USB2_OTG_HS_CURR_LVL | USB2_OTG_HS_SLEW |
		USB2_OTG_FS_SLEW | USB2_OTG_LS_RSLEW |
		USB2_OTG_PD | USB2_OTG_PD2 | USB2_OTG_PD_ZI);

	reg |= tegra->soc_config->hs_slew;
	reg |= (port == 2) ? tegra->soc_config->ls_rslew_pad2 :
			port ? tegra->soc_config->ls_rslew_pad1 :
			tegra->soc_config->ls_rslew_pad0;
	reg |= (port == 2) ? tegra->cdata->hs_curr_level_pad2 :
			port ? tegra->cdata->hs_curr_level_pad1 :
			tegra->cdata->hs_curr_level_pad0;
	writel(reg, tegra->padctl_base + ctl0_offset);

	reg = readl(tegra->padctl_base + ctl1_offset);
	reg &= ~(USB2_OTG_TERM_RANGE_AD | USB2_OTG_HS_IREF_CAP
		| USB2_OTG_PD_CHRP_FORCE_POWERUP
		| USB2_OTG_PD_DISC_FORCE_POWERUP
		| USB2_OTG_PD_DR);
	reg |= (tegra->cdata->hs_iref_cap << 9) |
		(tegra->cdata->hs_term_range_adj << 3);
	writel(reg, tegra->padctl_base + ctl1_offset);

	/*Release OTG port if not in host mode*/

	if ((port == 0) && !is_otg_host(tegra))
		tegra_xhci_release_otg_port(true);
}

static inline bool xusb_use_sata_lane(struct tegra_xhci_hcd *tegra)
{
	return ((XUSB_DEVICE_ID_T114 == tegra->device_id) ? false
	: ((tegra->bdata->portmap & TEGRA_XUSB_SS_P1)
		&& (tegra->bdata->lane_owner & BIT(0))));
}

static void tegra_xhci_program_ss_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 ctl2_offset, ctl4_offset, ctl5_offset;
	u32 reg;

	ctl2_offset = port ? padregs->iophy_usb3_pad1_ctl2_0 :
			padregs->iophy_usb3_pad0_ctl2_0;
	ctl4_offset = port ? padregs->iophy_usb3_pad1_ctl4_0 :
			padregs->iophy_usb3_pad0_ctl4_0;
	ctl5_offset = port ? padregs->iophy_misc_pad_p1_ctl5_0 :
			padregs->iophy_misc_pad_p0_ctl5_0;

	reg = readl(tegra->padctl_base + ctl2_offset);
	reg &= ~(IOPHY_USB3_RXWANDER | IOPHY_USB3_RXEQ |
		IOPHY_USB3_CDRCNTL);
	reg |= tegra->soc_config->rx_wander | tegra->soc_config->rx_eq |
		tegra->soc_config->cdr_cntl;
	writel(reg, tegra->padctl_base + ctl2_offset);

	reg = readl(tegra->padctl_base + ctl4_offset);
	reg = tegra->soc_config->dfe_cntl;
	writel(reg, tegra->padctl_base + ctl4_offset);

	reg = readl(tegra->padctl_base + ctl5_offset);
	reg |= RX_QEYE_EN;
	writel(reg, tegra->padctl_base + ctl5_offset);

	reg = readl(tegra->padctl_base + MISC_PAD_CTL_2_0(port));
	reg &= ~SPARE_IN(~0);
	reg |= SPARE_IN(tegra->soc_config->spare_in);
	writel(reg, tegra->padctl_base + MISC_PAD_CTL_2_0(port));

	if (xusb_use_sata_lane(tegra)) {
		reg = readl(tegra->padctl_base + MISC_PAD_S0_CTL_5_0);
		reg |= RX_QEYE_EN;
		writel(reg, tegra->padctl_base + MISC_PAD_S0_CTL_5_0);

		reg = readl(tegra->padctl_base + MISC_PAD_S0_CTL_2_0);
		reg &= ~SPARE_IN(~0);
		reg |= SPARE_IN(tegra->soc_config->spare_in);
		writel(reg, tegra->padctl_base + MISC_PAD_S0_CTL_2_0);
	}

	reg = readl(tegra->padctl_base + padregs->ss_port_map_0);
	reg &= ~(port ? SS_PORT_MAP_P1 : SS_PORT_MAP_P0);
	reg |= (tegra->bdata->ss_portmap &
		(port ? TEGRA_XUSB_SS1_PORT_MAP : TEGRA_XUSB_SS0_PORT_MAP));
	writel(reg, tegra->padctl_base + padregs->ss_port_map_0);

	tegra_xhci_restore_dfe_context(tegra, port);
	tegra_xhci_restore_ctle_context(tegra, port);
}

/* This function assigns the USB ports to the controllers,
 * then programs the port capabilities and pad parameters
 * of ports assigned to XUSB after booted to OS.
 */
void
tegra_xhci_padctl_portmap_and_caps(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg, oc_bits = 0;
	unsigned pad;

	reg = readl(tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);
	reg &= ~(USB2_BIAS_HS_SQUELCH_LEVEL | USB2_BIAS_HS_DISCON_LEVEL);
	reg |= tegra->cdata->hs_squelch_level | tegra->soc_config->hs_disc_lvl;
	writel(reg, tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);

	reg = readl(tegra->padctl_base + padregs->snps_oc_map_0);
	reg |= SNPS_OC_MAP_CTRL1 | SNPS_OC_MAP_CTRL2 | SNPS_OC_MAP_CTRL3;
	writel(reg, tegra->padctl_base + padregs->snps_oc_map_0);
	reg = readl(tegra->padctl_base + padregs->snps_oc_map_0);

	reg = readl(tegra->padctl_base + padregs->oc_det_0);
	reg |= OC_DET_VBUS_ENABLE0_OC_MAP | OC_DET_VBUS_ENABLE1_OC_MAP;
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
		reg |= OC_DET_VBUS_ENABLE2_OC_MAP;
	writel(reg, tegra->padctl_base + padregs->oc_det_0);

	/* check if over current seen. Clear if present */
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)
		oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD0;
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P1)
		oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD1;
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
		oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD2;

	reg = readl(tegra->padctl_base + padregs->oc_det_0);
	if (reg & oc_bits) {
		xhci_info(tegra->xhci, "Over current detected. Clearing...\n");
		writel(reg, tegra->padctl_base + padregs->oc_det_0);

		usleep_range(100, 200);

		reg = readl(tegra->padctl_base + padregs->oc_det_0);
		if (reg & oc_bits)
			xhci_info(tegra->xhci, "Over current still present\n");
	}

	reg = readl(tegra->padctl_base + padregs->usb2_oc_map_0);
	reg = USB2_OC_MAP_PORT0 | USB2_OC_MAP_PORT1;
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
		reg |= USB2_OC_MAP_PORT2;
	writel(reg, tegra->padctl_base + padregs->usb2_oc_map_0);

	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)
		tegra_xhci_program_utmip_pad(tegra, 0);
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P1)
		tegra_xhci_program_utmip_pad(tegra, 1);
	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
		tegra_xhci_program_utmip_pad(tegra, 2);

	for_each_enabled_hsic_pad(pad, tegra)
		hsic_pad_enable(tegra, pad);

	if (tegra->bdata->portmap & TEGRA_XUSB_ULPI_P0)
		tegra_xhci_program_ulpi_pad(tegra, 0);

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P0) {
		tegra_xhci_program_ss_pad(tegra, 0);
	} else {
		/* set rx_idle_mode_ovrd for unused SS ports to save power */
		reg = readl(tegra->padctl_base +
			padregs->iophy_misc_pad_p0_ctl3_0);
		reg &= ~RX_IDLE_MODE;
		reg |= RX_IDLE_MODE_OVRD;
		writel(reg, tegra->padctl_base +
			padregs->iophy_misc_pad_p0_ctl3_0);
	}

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P1) {
		tegra_xhci_program_ss_pad(tegra, 1);
	} else {
		/* set rx_idle_mode_ovrd for unused SS ports to save power */
		reg = readl(tegra->padctl_base +
			padregs->iophy_misc_pad_p1_ctl3_0);
		reg &= ~RX_IDLE_MODE;
		reg |= RX_IDLE_MODE_OVRD;
		writel(reg, tegra->padctl_base +
			padregs->iophy_misc_pad_p1_ctl3_0);

		/* SATA lane also if USB3_SS port1 mapped to it but unused */
		if (XUSB_DEVICE_ID_T114 != tegra->device_id &&
				tegra->bdata->lane_owner & BIT(0)) {
			reg = readl(tegra->padctl_base +
				padregs->iophy_misc_pad_s0_ctl3_0);
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
			writel(reg, tegra->padctl_base +
				padregs->iophy_misc_pad_s0_ctl3_0);
		}
	}
	if (XUSB_DEVICE_ID_T114 != tegra->device_id) {
		tegra_xhci_setup_gpio_for_ss_lane(tegra);
		usb3_phy_pad_enable(tegra->bdata->lane_owner);
	}
}

/* This function read XUSB registers and stores in device context */
static void
tegra_xhci_save_xusb_ctx(struct tegra_xhci_hcd *tegra)
{

	/* a. Save the IPFS registers */
	tegra->sregs.msi_bar_sz =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_BAR_SZ_0);

	tegra->sregs.msi_axi_barst =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0);

	tegra->sregs.msi_fpci_barst =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_BAR_ST_0);

	tegra->sregs.msi_vec0 =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_VEC0_0);

	tegra->sregs.msi_en_vec0 =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_EN_VEC0_0);

	tegra->sregs.fpci_error_masks =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0);

	tegra->sregs.intr_mask =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	tegra->sregs.ipfs_intr_enable =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0);

	tegra->sregs.ufpci_config =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_UFPCI_CONFIG_0);

	tegra->sregs.clkgate_hysteresis =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);

	tegra->sregs.xusb_host_mccif_fifo_cntrl =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0);

	/* b. Save the CFG registers */

	tegra->sregs.hs_pls =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HS_PLS);

	tegra->sregs.fs_pls =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_FS_PLS);

	tegra->sregs.hs_fs_speed =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_SPEED);

	tegra->sregs.hs_fs_pp =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_PP);

	tegra->sregs.cfg_aru =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT);

	tegra->sregs.cfg_order =
		readl(tegra->fpci_base + XUSB_CFG_FPCICFG);

	tegra->sregs.cfg_fladj =
		readl(tegra->fpci_base + XUSB_CFG_24);

	tegra->sregs.cfg_sid =
		readl(tegra->fpci_base + XUSB_CFG_16);
}

/* This function restores XUSB registers from device context */
static void
tegra_xhci_restore_ctx(struct tegra_xhci_hcd *tegra)
{
	/* Restore Cfg registers */
	writel(tegra->sregs.hs_pls,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HS_PLS);

	writel(tegra->sregs.fs_pls,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_FS_PLS);

	writel(tegra->sregs.hs_fs_speed,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_SPEED);

	writel(tegra->sregs.hs_fs_pp,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_PP);

	writel(tegra->sregs.cfg_aru,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT);

	writel(tegra->sregs.cfg_order,
		tegra->fpci_base + XUSB_CFG_FPCICFG);

	writel(tegra->sregs.cfg_fladj,
		tegra->fpci_base + XUSB_CFG_24);

	writel(tegra->sregs.cfg_sid,
		tegra->fpci_base + XUSB_CFG_16);

	/* Restore IPFS registers */

	writel(tegra->sregs.msi_bar_sz,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_BAR_SZ_0);

	writel(tegra->sregs.msi_axi_barst,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0);

	writel(tegra->sregs.msi_fpci_barst,
		tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_BAR_ST_0);

	writel(tegra->sregs.msi_vec0,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_VEC0_0);

	writel(tegra->sregs.msi_en_vec0,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_EN_VEC0_0);

	writel(tegra->sregs.fpci_error_masks,
		tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0);

	writel(tegra->sregs.intr_mask,
		tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	writel(tegra->sregs.ipfs_intr_enable,
		tegra->ipfs_base + IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0);

	writel(tegra->sregs.ufpci_config,
		tegra->fpci_base + IPFS_XUSB_HOST_UFPCI_CONFIG_0);

	writel(tegra->sregs.clkgate_hysteresis,
		tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);

	writel(tegra->sregs.xusb_host_mccif_fifo_cntrl,
		tegra->ipfs_base + IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0);
}

static void tegra_xhci_enable_fw_message(struct tegra_xhci_hcd *tegra)
{
	fw_message_send(tegra, MBOX_CMD_MSG_ENABLED, 0 /* no data needed */);
}

static int load_firmware(struct tegra_xhci_hcd *tegra, bool resetARU)
{
	struct platform_device *pdev = tegra->pdev;
	struct cfgtbl *cfg_tbl = (struct cfgtbl *) tegra->firmware.data;
	u32 phys_addr_lo;
	u32 HwReg;
	u16 nblocks;
	time_t fw_time;
	struct tm fw_tm;
	u8 hc_caplength;
	u32 usbsts, count = 0xff;
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;

	/* enable mbox interrupt */
	writel(readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD) | MBOX_INT_EN,
		tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	/* First thing, reset the ARU. By the time we get to
	 * loading boot code below, reset would be complete.
	 * alternatively we can busy wait on rst pending bit.
	 */
	/* Don't reset during ELPG/LP0 exit path */
	if (resetARU) {
		iowrite32(0x1, tegra->fpci_base + XUSB_CFG_ARU_RST);
		usleep_range(1000, 2000);
	}

	if (csb_read(tegra, XUSB_CSB_MP_ILOAD_BASE_LO) != 0) {
		dev_info(&pdev->dev, "Firmware already loaded, Falcon state 0x%x\n",
				csb_read(tegra, XUSB_FALC_CPUCTL));
		return 0;
	}

	/* update the phys_log_buffer and total_entries here */
	cfg_tbl->phys_addr_log_buffer = tegra->log.phys_addr;
	cfg_tbl->total_log_entries = FW_LOG_COUNT;

	phys_addr_lo = tegra->firmware.dma;
	phys_addr_lo += sizeof(struct cfgtbl);

	/* Program the size of DFI into ILOAD_ATTR */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_ATTR, tegra->firmware.size);

	/* Boot code of the firmware reads the ILOAD_BASE_LO register
	 * to get to the start of the dfi in system memory.
	 */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_BASE_LO, phys_addr_lo);

	/* Program the ILOAD_BASE_HI with a value of MSB 32 bits */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_BASE_HI, 0);

	/* Set BOOTPATH to 1 in APMAP Register. Bit 31 is APMAP_BOOTMAP */
	csb_write(tegra, XUSB_CSB_MP_APMAP, APMAP_BOOTPATH);

	/* Invalidate L2IMEM. */
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_TRIG, L2IMEM_INVALIDATE_ALL);

	/* Initiate fetch of Bootcode from system memory into L2IMEM.
	 * Program BootCode location and size in system memory.
	 */
	HwReg = ((cfg_tbl->boot_codetag / IMEM_BLOCK_SIZE) &
			L2IMEMOP_SIZE_SRC_OFFSET_MASK)
			<< L2IMEMOP_SIZE_SRC_OFFSET_SHIFT;
	HwReg |= ((cfg_tbl->boot_codesize / IMEM_BLOCK_SIZE) &
			L2IMEMOP_SIZE_SRC_COUNT_MASK)
			<< L2IMEMOP_SIZE_SRC_COUNT_SHIFT;
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_SIZE, HwReg);

	/* Trigger L2IMEM Load operation. */
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_TRIG, L2IMEM_LOAD_LOCKED_RESULT);

	/* Setup Falcon Auto-fill */
	nblocks = (cfg_tbl->boot_codesize / IMEM_BLOCK_SIZE);
	if ((cfg_tbl->boot_codesize % IMEM_BLOCK_SIZE) != 0)
		nblocks += 1;
	csb_write(tegra, XUSB_FALC_IMFILLCTL, nblocks);

	HwReg = (cfg_tbl->boot_codetag / IMEM_BLOCK_SIZE) & IMFILLRNG_TAG_MASK;
	HwReg |= (((cfg_tbl->boot_codetag + cfg_tbl->boot_codesize)
			/IMEM_BLOCK_SIZE) - 1) << IMFILLRNG1_TAG_HI_SHIFT;
	csb_write(tegra, XUSB_FALC_IMFILLRNG1, HwReg);

	csb_write(tegra, XUSB_FALC_DMACTL, 0);
	msleep(50);

	csb_write(tegra, XUSB_FALC_BOOTVEC, cfg_tbl->boot_codetag);

	/* Start Falcon CPU */
	csb_write(tegra, XUSB_FALC_CPUCTL, CPUCTL_STARTCPU);
	usleep_range(1000, 2000);

	fw_time = cfg_tbl->fwimg_created_time;
	time_to_tm(fw_time, 0, &fw_tm);
	dev_info(&pdev->dev,
		"Firmware timestamp: %ld-%02d-%02d %02d:%02d:%02d UTC, "\
		"Falcon state 0x%x\n", fw_tm.tm_year + 1900,
		fw_tm.tm_mon + 1, fw_tm.tm_mday, fw_tm.tm_hour,
		fw_tm.tm_min, fw_tm.tm_sec,
		csb_read(tegra, XUSB_FALC_CPUCTL));

	dev_dbg(&pdev->dev, "num_hsic_port %d\n", cfg_tbl->num_hsic_port);

	/* return fail if firmware status is not good */
	if (csb_read(tegra, XUSB_FALC_CPUCTL) == XUSB_FALC_STATE_HALTED)
		return -EFAULT;

	cap_regs = IO_ADDRESS(tegra->host_phy_base);
	hc_caplength = HC_LENGTH(ioread32(&cap_regs->hc_capbase));
	op_regs = IO_ADDRESS(tegra->host_phy_base + hc_caplength);

	/* wait for USBSTS_CNR to get set */
	do {
		usbsts = ioread32(&op_regs->status);
	} while ((usbsts & STS_CNR) && count--);

	if (!count && (usbsts & STS_CNR)) {
		dev_err(&pdev->dev, "Controller not ready\n");
		return -EFAULT;
	}

	return 0;
}

static void tegra_xhci_release_port_ownership(struct tegra_xhci_hcd *tegra,
	bool release)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 != tegra->device_id)
		return;

	reg = readl(tegra->padctl_base + padregs->usb2_pad_mux_0);
	reg &= ~(USB2_OTG_PAD_PORT_MASK(0) | USB2_OTG_PAD_PORT_MASK(1) |
			USB2_OTG_PAD_PORT_MASK(2));

	if (!release) {
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)
			if (is_otg_host(tegra))
				reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(0);
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P1)
			reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(1);
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
			reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(2);
	}

	writel(reg, tegra->padctl_base + padregs->usb2_pad_mux_0);
}
/* SS ELPG Entry initiated by fw */
static int tegra_xhci_ss_elpg_entry(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 ret = 0;

	must_have_sync_lock(tegra);

	/* update maximum BW requirement to 0 */
	tegra_xusb_set_bw(tegra, 0);

	/* This is SS partition ELPG entry
	 * STEP 0: firmware will set WOC WOD bits in PVTPORTSC2 regs.
	 */

	/* Step 0: Acquire mbox and send PWRGATE msg to firmware
	 * only if it is sw initiated one
	 */

	/* STEP 1: xHCI firmware and xHCIPEP driver communicates
	 * SuperSpeed partition ELPG entry via mailbox protocol
	 */

	/* STEP 2: xHCI PEP driver and XUSB device mode driver
	 * enable the XUSB wakeup interrupts for the SuperSpeed
	 * and USB2.0 ports assigned to host.Section 4.1 Step 3
	 */
	tegra_xhci_ss_wake_on_interrupts(tegra->bdata->portmap, true);

	/* STEP 3: xHCI PEP driver initiates the signal sequence
	 * to enable the XUSB SSwake detection logic for the
	 * SuperSpeed ports assigned to host.Section 4.1 Step 4
	 */
	tegra_xhci_ss_wake_signal(tegra->bdata->portmap, true);

	/* STEP 4: System Power Management driver asserts reset
	 * to XUSB SuperSpeed partition then disables its clocks
	 */
	tegra_periph_reset_assert(tegra->ss_clk);
	clk_disable(tegra->ss_clk);

	usleep_range(100, 200);

	/* STEP 5: System Power Management driver disables the
	 * XUSB SuperSpeed partition power rails.
	 */
	debug_print_portsc(xhci);

	/* tegra_powergate_partition also does partition reset assert */
	ret = tegra_powergate_partition(TEGRA_POWERGATE_XUSBA);
	if (ret) {
		xhci_err(xhci, "%s: could not powergate xusba partition\n",
				__func__);
		/* TODO: error recovery? */
	}
	tegra->ss_pwr_gated = true;

	/* STEP 6: xHCI PEP driver initiates the signal sequence
	 * to enable the XUSB SSwake detection logic for the
	 * SuperSpeed ports assigned to host.Section 4.1 Step 7
	 */
	tegra_xhci_ss_vcore(tegra->bdata->portmap, true);

	return ret;
}

/* Host ELPG Entry */
static int tegra_xhci_host_elpg_entry(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 ret;

	must_have_sync_lock(tegra);

	/* If ss is already powergated skip ss ctx save stuff */
	if (tegra->ss_pwr_gated) {
		xhci_info(xhci, "%s: SS partition is already powergated\n",
			__func__);
	} else {
		ret = tegra_xhci_ss_elpg_entry(tegra);
		if (ret) {
			xhci_err(xhci, "%s: ss_elpg_entry failed %d\n",
				__func__, ret);
			return ret;
		}
	}

	/* 1. IS INTR PENDING INT_PENDING=1 ? */

	/* STEP 1.1: Do a context save of XUSB and IPFS registers */
	tegra_xhci_save_xusb_ctx(tegra);

	/* calculate rctrl_val and tctrl_val */
	tegra_xhci_war_for_tctrl_rctrl(tegra);

	pmc_setup_wake_detect(tegra);

	tegra_xhci_hs_wake_on_interrupts(tegra->bdata->portmap, true);
	xhci_dbg(xhci, "%s: PMC_UTMIP_UHSIC_SLEEP_CFG_0 = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));

	/* tegra_powergate_partition also does partition reset assert */
	ret = tegra_powergate_partition(TEGRA_POWERGATE_XUSBC);
	if (ret) {
		xhci_err(xhci, "%s: could not unpowergate xusbc partition %d\n",
			__func__, ret);
		/* TODO: error handling? */
		return ret;
	}
	tegra->host_pwr_gated = true;
	clk_disable(tegra->host_clk);

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_disable(tegra->pll_re_vco_clk);
	clk_disable(tegra->emc_clk);
	/* set port ownership to SNPS */
	tegra_xhci_release_port_ownership(tegra, true);

	xhci_dbg(xhci, "%s: PMC_UTMIP_UHSIC_SLEEP_CFG_0 = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));

	xhci_info(xhci, "%s: elpg_entry: completed\n", __func__);
	xhci_dbg(xhci, "%s: HOST POWER STATUS = %d\n",
		__func__, tegra_powergate_is_powered(TEGRA_POWERGATE_XUSBC));
	return ret;
}

/* SS ELPG Exit triggered by PADCTL irq */
/**
 * tegra_xhci_ss_partition_elpg_exit - bring XUSBA partition out from elpg
 *
 * This function must be called with tegra->sync_lock acquired.
 *
 * @tegra: xhci controller context
 * @return 0 for success, or error numbers
 */
static int tegra_xhci_ss_partition_elpg_exit(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	int ret = 0;

	must_have_sync_lock(tegra);

	if (tegra->ss_pwr_gated && (tegra->ss_wake_event ||
			tegra->hs_wake_event || tegra->host_resume_req)) {

		/*
		 * PWR_UNGATE SS partition. XUSBA
		 * tegra_unpowergate_partition also does partition reset
		 * deassert
		 */
		ret = tegra_unpowergate_partition(TEGRA_POWERGATE_XUSBA);
		if (ret) {
			xhci_err(xhci,
			"%s: could not unpowergate xusba partition %d\n",
			__func__, ret);
			goto out;
		}
		if (tegra->ss_wake_event)
			tegra->ss_wake_event = false;

	} else {
		xhci_info(xhci, "%s: ss already power gated\n",
			__func__);
		return ret;
	}

	/* Step 3: Enable clock to ss partition */
	clk_enable(tegra->ss_clk);

	/* Step 4: Disable ss wake detection logic */
	tegra_xhci_ss_wake_on_interrupts(tegra->bdata->portmap, false);

	/* Step 4.1: Disable ss wake detection logic */
	tegra_xhci_ss_vcore(tegra->bdata->portmap, false);

	/* wait 150us */
	usleep_range(150, 200);

	/* Step 4.2: Disable ss wake detection logic */
	tegra_xhci_ss_wake_signal(tegra->bdata->portmap, false);

	/* Step 6 Deassert reset for ss clks */
	tegra_periph_reset_deassert(tegra->ss_clk);

	xhci_dbg(xhci, "%s: SS ELPG EXIT. ALL DONE\n", __func__);
	tegra->ss_pwr_gated = false;
out:
	return ret;
}

static void ss_partition_elpg_exit_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
		ss_elpg_exit_work);

	mutex_lock(&tegra->sync_lock);
	tegra_xhci_ss_partition_elpg_exit(tegra);
	mutex_unlock(&tegra->sync_lock);
}

/* read pmc WAKE2_STATUS register to know if SS port caused remote wake */
static void update_remote_wakeup_ports(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 wake2_status;
	int port;

#define PMC_WAKE2_STATUS	0x168
#define PADCTL_WAKE		(1 << (58 - 32)) /* PADCTL is WAKE#58 */

	wake2_status = tegra_usb_pmc_reg_read(PMC_WAKE2_STATUS);

	if (wake2_status & PADCTL_WAKE) {
		/* FIXME: This is customized for Dalmore, find a generic way */
		set_bit(0, &tegra->usb3_rh_remote_wakeup_ports);
		/* clear wake status */
		tegra_usb_pmc_reg_write(PMC_WAKE2_STATUS, PADCTL_WAKE);
	}

	/* set all usb2 ports with RESUME link state as wakup ports  */
	for (port = 0; port < xhci->num_usb2_ports; port++) {
		u32 portsc = xhci_readl(xhci, xhci->usb2_ports[port]);
		if ((portsc & PORT_PLS_MASK) == XDEV_RESUME)
			set_bit(port, &tegra->usb2_rh_remote_wakeup_ports);
	}

	xhci_dbg(xhci, "%s: usb2 roothub remote_wakeup_ports 0x%lx\n",
			__func__, tegra->usb2_rh_remote_wakeup_ports);
	xhci_dbg(xhci, "%s: usb3 roothub remote_wakeup_ports 0x%lx\n",
			__func__, tegra->usb3_rh_remote_wakeup_ports);
}

static void wait_remote_wakeup_ports(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	int port, num_ports;
	unsigned long *remote_wakeup_ports;
	u32 portsc;
	__le32 __iomem	**port_array;
	unsigned char *rh;
	unsigned int retry = 64;
	struct xhci_bus_state *bus_state;

	bus_state = &xhci->bus_state[hcd_index(hcd)];

	if (hcd == xhci->shared_hcd) {
		port_array = xhci->usb3_ports;
		num_ports = xhci->num_usb3_ports;
		remote_wakeup_ports = &tegra->usb3_rh_remote_wakeup_ports;
		rh = "usb3 roothub";
	} else {
		port_array = xhci->usb2_ports;
		num_ports = xhci->num_usb2_ports;
		remote_wakeup_ports = &tegra->usb2_rh_remote_wakeup_ports;
		rh = "usb2 roothub";
	}

	while (*remote_wakeup_ports && retry--) {
		for_each_set_bit(port, remote_wakeup_ports, num_ports) {
			bool can_continue;

			portsc = xhci_readl(xhci, port_array[port]);

			if (!(portsc & PORT_CONNECT)) {
				/* nothing to do if already disconnected */
				clear_bit(port, remote_wakeup_ports);
				continue;
			}

			if (hcd == xhci->shared_hcd) {
				can_continue =
					(portsc & PORT_PLS_MASK) == XDEV_U0;
			} else {
				unsigned long flags;

				spin_lock_irqsave(&xhci->lock, flags);
				can_continue =
				test_bit(port, &bus_state->resuming_ports);
				spin_unlock_irqrestore(&xhci->lock, flags);
			}

			if (can_continue)
				clear_bit(port, remote_wakeup_ports);
			else
				xhci_dbg(xhci, "%s: %s port %d status 0x%x\n",
					__func__, rh, port, portsc);
		}

		if (*remote_wakeup_ports)
			msleep(20); /* give some time, irq will direct U0 */
	}

	xhci_dbg(xhci, "%s: %s remote_wakeup_ports 0x%lx\n", __func__, rh,
			*remote_wakeup_ports);
}

static void tegra_xhci_war_for_tctrl_rctrl(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg, utmip_rctrl_val, utmip_tctrl_val, pad_mux, portmux, portowner;

	portmux = USB2_OTG_PAD_PORT_MASK(0) | USB2_OTG_PAD_PORT_MASK(1);
	portowner = USB2_OTG_PAD_PORT_OWNER_XUSB(0) |
			USB2_OTG_PAD_PORT_OWNER_XUSB(1);

	if (XUSB_DEVICE_ID_T114 != tegra->device_id) {
		portmux |= USB2_OTG_PAD_PORT_MASK(2);
		portowner |= USB2_OTG_PAD_PORT_OWNER_XUSB(2);
	}

	/* Use xusb padctl space only when xusb owns all UTMIP port */
	pad_mux = readl(tegra->padctl_base + padregs->usb2_pad_mux_0);
	if ((pad_mux & portmux) == portowner) {
		/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD = 0 and
		 * XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD_TRK = 0
		 */
		reg = readl(tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);
		reg &= ~((1 << 12) | (1 << 13));
		writel(reg, tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);

		/* wait 20us */
		usleep_range(20, 30);

		/* Read XUSB_PADCTL:: XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0
		 * :: TCTRL and RCTRL
		 */
		reg = readl(tegra->padctl_base + padregs->usb2_bias_pad_ctl1_0);
		utmip_rctrl_val = RCTRL(reg);
		utmip_tctrl_val = TCTRL(reg);

		/*
		 * tctrl_val = 0x1f - (16 - ffz(utmip_tctrl_val)
		 * rctrl_val = 0x1f - (16 - ffz(utmip_rctrl_val)
		 */
		utmip_rctrl_val = 0xf + ffz(utmip_rctrl_val);
		utmip_tctrl_val = 0xf + ffz(utmip_tctrl_val);
		utmi_phy_update_trking_data(utmip_tctrl_val, utmip_rctrl_val);
		xhci_dbg(tegra->xhci, "rctrl_val = 0x%x, tctrl_val = 0x%x\n",
					utmip_rctrl_val, utmip_tctrl_val);

		/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD = 1 and
		 * XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD_TRK = 1
		 */
		reg = readl(tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);
		reg |= (1 << 13);
		writel(reg, tegra->padctl_base + padregs->usb2_bias_pad_ctl0_0);

		/* Program these values into PMC regiseter and program the
		 * PMC override.
		 */
		reg = PMC_TCTRL_VAL(utmip_tctrl_val) |
				PMC_RCTRL_VAL(utmip_rctrl_val);
		tegra_usb_pmc_reg_update(PMC_UTMIP_TERM_PAD_CFG,
					0xffffffff, reg);
		reg = UTMIP_RCTRL_USE_PMC_P2 | UTMIP_TCTRL_USE_PMC_P2;
		tegra_usb_pmc_reg_update(PMC_SLEEP_CFG, reg, reg);
	} else {
		/* Use common PMC API to use SNPS register space */
		utmi_phy_set_snps_trking_data();
	}
}

/* Host ELPG Exit triggered by PADCTL irq */
/**
 * tegra_xhci_host_partition_elpg_exit - bring XUSBC partition out from elpg
 *
 * This function must be called with tegra->sync_lock acquired.
 *
 * @tegra: xhci controller context
 * @return 0 for success, or error numbers
 */
static int
tegra_xhci_host_partition_elpg_exit(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	struct xhci_hcd *xhci = tegra->xhci;
	int ret = 0;

	must_have_sync_lock(tegra);

	if (!tegra->hc_in_elpg)
		return 0;

	clk_enable(tegra->emc_clk);
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_enable(tegra->pll_re_vco_clk);

	if (tegra->lp0_exit) {
		u32 reg, oc_bits = 0;

		/* Issue is only applicable for T114 */
		if (XUSB_DEVICE_ID_T114 == tegra->device_id)
			tegra_xhci_war_for_tctrl_rctrl(tegra);
		/* check if over current seen. Clear if present */
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)
			oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD0;
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P1)
			oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD1;
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
			oc_bits |= OC_DET_OC_DETECTED_VBUS_PAD2;

		reg = readl(tegra->padctl_base + padregs->oc_det_0);
		xhci_dbg(xhci, "%s: oc_det_0=0x%x\n", __func__, reg);
		if (reg & oc_bits) {
			xhci_info(xhci, "Over current detected. Clearing...\n");
			writel(reg, tegra->padctl_base + padregs->oc_det_0);

			usleep_range(100, 200);

			reg = readl(tegra->padctl_base + padregs->oc_det_0);
			if (reg & oc_bits)
				xhci_info(xhci, "Over current still present\n");
		}
		tegra_xhci_padctl_portmap_and_caps(tegra);
		/* release clamps post deassert */
		tegra->lp0_exit = false;
	}

	/* Clear FLUSH_ENABLE of MC client */
	tegra_powergate_mc_flush_done(TEGRA_POWERGATE_XUSBC);

	/* set port ownership back to xusb */
	tegra_xhci_release_port_ownership(tegra, false);

	/*
	 * PWR_UNGATE Host partition. XUSBC
	 * tegra_unpowergate_partition also does partition reset deassert
	 */
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_XUSBC);
	if (ret) {
		xhci_err(xhci, "%s: could not unpowergate xusbc partition %d\n",
			__func__, ret);
		goto out;
	}
	clk_enable(tegra->host_clk);

	/* Step 4: Deassert reset to host partition clk */
	tegra_periph_reset_deassert(tegra->host_clk);

	/* Step 6.1: IPFS and XUSB BAR initialization */
	tegra_xhci_cfg(tegra);

	/* Step 6.2: IPFS and XUSB related restore */
	tegra_xhci_restore_ctx(tegra);

	/* Step 8: xhci spec related ctx restore
	 * will be done in xhci_resume().Do it here.
	 */

	tegra_xhci_ss_partition_elpg_exit(tegra);

	/* Change SS clock source to HSIC_480 and set ss_src_clk at 120MHz */
	if (clk_get_rate(tegra->ss_src_clk) == 12000000) {
		clk_set_rate(tegra->ss_src_clk,  3000 * 1000);
		clk_set_parent(tegra->ss_src_clk, tegra->pll_u_480M);
	}

	/* clear ovrd bits */
	tegra_xhci_rx_idle_mode_override(tegra, false);

	/* Load firmware */
	xhci_dbg(xhci, "%s: elpg_exit: loading firmware from pmc.\n"
			"ss (p1=0x%x, p2=0x%x, p3=0x%x), "
			"hs (p1=0x%x, p2=0x%x, p3=0x%x),\n"
			"fs (p1=0x%x, p2=0x%x, p3=0x%x)\n",
			__func__,
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC3),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC3),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC3));
	debug_print_portsc(xhci);

	ret = load_firmware(tegra, false /* EPLG exit, do not reset ARU */);
	if (ret < 0) {
		xhci_err(xhci, "%s: failed to load firmware %d\n",
			__func__, ret);
		goto out;
	}

	pmc_disable_bus_ctrl(tegra);

	tegra->hc_in_elpg = false;
	ret = xhci_resume(tegra->xhci, 0);
	if (ret) {
		xhci_err(xhci, "%s: could not resume right %d\n",
				__func__, ret);
		goto out;
	}

	update_remote_wakeup_ports(tegra);

	if (tegra->hs_wake_event)
		tegra->hs_wake_event = false;

	if (tegra->host_resume_req)
		tegra->host_resume_req = false;

	xhci_info(xhci, "elpg_exit: completed: lp0/elpg time=%d msec\n",
		jiffies_to_msecs(jiffies - tegra->last_jiffies));

	tegra->host_pwr_gated = false;
out:
	return ret;
}

static void host_partition_elpg_exit_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
		host_elpg_exit_work);

	mutex_lock(&tegra->sync_lock);
	tegra_xhci_host_partition_elpg_exit(tegra);
	mutex_unlock(&tegra->sync_lock);
}

/* Mailbox handling function. This function handles requests
 * from firmware and communicates with clock and powergating
 * module to alter clock rates and to power gate/ungate xusb
 * partitions.
 *
 * Following is the structure of mailbox messages.
 * bit 31:28 - msg type
 * bits 27:0 - mbox data
 * FIXME:  Check if we can just call clock functions like below
 * or should we schedule it for calling later ?
 */

static void
tegra_xhci_process_mbox_message(struct work_struct *work)
{
	u32 sw_resp = 0, cmd, data_in, fw_msg;
	int ret = 0;
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
					mbox_work);
	struct xhci_hcd *xhci = tegra->xhci;
	int pad, port;
	unsigned long ports;

	mutex_lock(&tegra->mbox_lock);

	/* get the mbox message from firmware */
	fw_msg = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_OUT);

	data_in = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_IN);
	if (data_in) {
		dev_warn(&tegra->pdev->dev, "%s data_in 0x%x\n",
			__func__, data_in);
		mutex_unlock(&tegra->mbox_lock);
		return;
	}

	/* get cmd type and cmd data */
	tegra->cmd_type = (fw_msg >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK;
	tegra->cmd_data = (fw_msg >> CMD_DATA_SHIFT) & CMD_DATA_MASK;

	/* decode the message and make appropriate requests to
	 * clock or powergating module.
	 */

	switch (tegra->cmd_type) {
	case MBOX_CMD_INC_FALC_CLOCK:
	case MBOX_CMD_DEC_FALC_CLOCK:
		ret = tegra_xusb_request_clk_rate(
				tegra,
				tegra->falc_clk,
				tegra->cmd_data,
				&sw_resp);
		if (ret)
			xhci_err(xhci, "%s: could not set required falc rate\n",
				__func__);
		goto send_sw_response;
	case MBOX_CMD_INC_SSPI_CLOCK:
	case MBOX_CMD_DEC_SSPI_CLOCK:
		ret = tegra_xusb_request_clk_rate(
				tegra,
				tegra->ss_src_clk,
				tegra->cmd_data,
				&sw_resp);
		if (ret)
			xhci_err(xhci, "%s: could not set required ss rate.\n",
				__func__);
		goto send_sw_response;

	case MBOX_CMD_SET_BW:
		/* fw sends BW request in MByte/sec */
		mutex_lock(&tegra->sync_lock);
		tegra_xusb_set_bw(tegra, tegra->cmd_data << 10);
		mutex_unlock(&tegra->sync_lock);
		break;

	case MBOX_CMD_SAVE_DFE_CTLE_CTX:
		tegra_xhci_save_dfe_context(tegra, tegra->cmd_data);
		tegra_xhci_save_ctle_context(tegra, tegra->cmd_data);
		sw_resp = CMD_DATA(tegra->cmd_data) | CMD_TYPE(MBOX_CMD_ACK);
		goto send_sw_response;

	case MBOX_CMD_STAR_HSIC_IDLE:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			pad = port_to_hsic_pad(port - 1);
			mutex_lock(&tegra->sync_lock);
			ret = hsic_pad_pupd_set(tegra, pad, PUPD_IDLE);
			mutex_unlock(&tegra->sync_lock);
			if (ret)
				break;
		}

		sw_resp = CMD_DATA(tegra->cmd_data);
		if (!ret)
			sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		else
			sw_resp |= CMD_TYPE(MBOX_CMD_ACK);

		goto send_sw_response;

	case MBOX_CMD_STOP_HSIC_IDLE:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			pad = port_to_hsic_pad(port - 1);
			mutex_lock(&tegra->sync_lock);
			ret = hsic_pad_pupd_set(tegra, pad, PUPD_DISABLE);
			mutex_unlock(&tegra->sync_lock);
			if (ret)
				break;
		}

		sw_resp = CMD_DATA(tegra->cmd_data);
		if (!ret)
			sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		else
			sw_resp |= CMD_TYPE(MBOX_CMD_NACK);
		goto send_sw_response;

	case MBOX_CMD_ACK:
		xhci_dbg(xhci, "%s firmware responds with ACK\n", __func__);
		break;
	case MBOX_CMD_NACK:
		xhci_warn(xhci, "%s firmware responds with NACK\n", __func__);
		break;
	default:
		xhci_err(xhci, "%s: invalid cmdtype %d\n",
				__func__, tegra->cmd_type);
	}

	/* clear MBOX_SMI_INT_EN bit */
	cmd = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	cmd &= ~MBOX_SMI_INT_EN;
	writel(cmd, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	/* clear mailbox ownership */
	writel(0, tegra->fpci_base + XUSB_CFG_ARU_MBOX_OWNER);

	mutex_unlock(&tegra->mbox_lock);
	return;

send_sw_response:
	if (((sw_resp >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK) == MBOX_CMD_NACK)
		xhci_err(xhci, "%s respond fw message 0x%x with NAK\n",
				__func__, fw_msg);

	writel(sw_resp, tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_IN);
	cmd = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	cmd |= MBOX_INT_EN | MBOX_FALC_INT_EN;
	writel(cmd, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	mutex_unlock(&tegra->mbox_lock);
}

static irqreturn_t pmc_usb_phy_wake_isr(int irq, void *data)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) data;
	struct xhci_hcd *xhci = tegra->xhci;

	xhci_dbg(xhci, "%s irq %d", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_xhci_padctl_irq(int irq, void *ptrdev)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) ptrdev;
	struct xhci_hcd *xhci = tegra->xhci;
	struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 elpg_program0 = 0;

	spin_lock(&tegra->lock);

	tegra->last_jiffies = jiffies;

	/* Check the intr cause. Could be  USB2 or HSIC or SS wake events */
	elpg_program0 = tegra_usb_pad_reg_read(padregs->elpg_program_0);

	/* Clear the interrupt cause. We already read the intr status. */
	tegra_xhci_ss_wake_on_interrupts(tegra->bdata->portmap, false);
	tegra_xhci_hs_wake_on_interrupts(tegra->bdata->portmap, false);

	xhci_dbg(xhci, "%s: elpg_program0 = %x\n", __func__, elpg_program0);
	xhci_dbg(xhci, "%s: PMC REGISTER = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));
	xhci_dbg(xhci, "%s: OC_DET Register = %x\n",
		__func__, readl(tegra->padctl_base + padregs->oc_det_0));
	xhci_dbg(xhci, "%s: usb2_bchrg_otgpad0_ctl0_0 Register = %x\n",
		__func__,
		readl(tegra->padctl_base + padregs->usb2_bchrg_otgpad0_ctl0_0));
	xhci_dbg(xhci, "%s: usb2_bchrg_otgpad1_ctl0_0 Register = %x\n",
		__func__,
		readl(tegra->padctl_base + padregs->usb2_bchrg_otgpad1_ctl0_0));
	xhci_dbg(xhci, "%s: usb2_bchrg_bias_pad_0 Register = %x\n",
		__func__,
		readl(tegra->padctl_base + padregs->usb2_bchrg_bias_pad_0));

	if (elpg_program0 & (SS_PORT0_WAKEUP_EVENT | SS_PORT1_WAKEUP_EVENT))
		tegra->ss_wake_event = true;
	else if (elpg_program0 & (USB2_PORT0_WAKEUP_EVENT |
			USB2_PORT1_WAKEUP_EVENT |
			USB2_PORT2_WAKEUP_EVENT |
			USB2_HSIC_PORT0_WAKEUP_EVENT |
			USB2_HSIC_PORT1_WAKEUP_EVENT))
		tegra->hs_wake_event = true;

	if (tegra->ss_wake_event || tegra->hs_wake_event) {
		if (tegra->ss_pwr_gated && !tegra->host_pwr_gated) {
			xhci_err(xhci, "SS gated Host ungated. Should not happen\n");
			WARN_ON(tegra->ss_pwr_gated && tegra->host_pwr_gated);
		} else if (tegra->ss_pwr_gated
				&& tegra->host_pwr_gated) {
			xhci_dbg(xhci, "[%s] schedule host_elpg_exit_work\n",
				__func__);
			schedule_work(&tegra->host_elpg_exit_work);
		}
	} else {
		xhci_err(xhci, "error: wake due to no hs/ss event\n");
		tegra_usb_pad_reg_write(padregs->elpg_program_0, 0xffffffff);
	}
	spin_unlock(&tegra->lock);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_xhci_smi_irq(int irq, void *ptrdev)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) ptrdev;
	u32 temp;

	spin_lock(&tegra->lock);

	/* clear the mbox intr status 1st thing. Other
	 * bits are W1C bits, so just write to SMI bit.
	 */

	temp = readl(tegra->fpci_base + XUSB_CFG_ARU_SMI_INTR);

	/* write 1 to clear SMI INTR en bit ( bit 3 ) */
	temp = MBOX_SMI_INTR_EN;
	writel(temp, tegra->fpci_base + XUSB_CFG_ARU_SMI_INTR);

	schedule_work(&tegra->mbox_work);

	spin_unlock(&tegra->lock);
	return IRQ_HANDLED;
}

static void tegra_xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
	xhci->quirks &= ~XHCI_SPURIOUS_REBOOT;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, tegra_xhci_plat_quirks);
}

static int tegra_xhci_request_mem_region(struct platform_device *pdev,
	int num, void __iomem **region)
{
	struct resource	*res;
	void __iomem *mem;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (!res) {
		dev_err(&pdev->dev, "memory resource %d doesn't exist\n", num);
		return -ENODEV;
	}

	mem = devm_request_and_ioremap(&pdev->dev, res);
	if (!mem) {
		dev_err(&pdev->dev, "failed to ioremap for %d\n", num);
		return -EFAULT;
	}
	*region = mem;

	return 0;
}

static int tegra_xhci_request_irq(struct platform_device *pdev,
	int num, irq_handler_t handler, unsigned long irqflags,
	const char *devname, int *irq_no)
{
	int ret;
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct resource	*res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, num);
	if (!res) {
		dev_err(&pdev->dev, "irq resource %d doesn't exist\n", num);
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, res->start, handler, irqflags,
			devname, tegra);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"failed to request_irq for %s (irq %d), error = %d\n",
			devname, (int)res->start, ret);
		return ret;
	}
	*irq_no = res->start;

	return 0;
}

#ifdef CONFIG_PM

static int tegra_xhci_bus_suspend(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int err = 0;
	unsigned long flags;

	mutex_lock(&tegra->sync_lock);

	if (xhci->shared_hcd == hcd) {
		tegra->usb3_rh_suspend = true;
		xhci_dbg(xhci, "%s: usb3 root hub\n", __func__);
	} else if (xhci->main_hcd == hcd) {
		tegra->usb2_rh_suspend = true;
		xhci_dbg(xhci, "%s: usb2 root hub\n", __func__);
	}

	WARN_ON(tegra->hc_in_elpg);

	/* suspend xhci bus. This will also set remote mask */
	err = xhci_bus_suspend(hcd);
	if (err) {
		xhci_err(xhci, "%s: xhci_bus_suspend failed %d\n",
				__func__, err);
		goto xhci_bus_suspend_failed;
	}

	if (!(tegra->usb2_rh_suspend && tegra->usb3_rh_suspend))
		goto done; /* one of the root hubs is still working */

	spin_lock_irqsave(&tegra->lock, flags);
	tegra->hc_in_elpg = true;
	spin_unlock_irqrestore(&tegra->lock, flags);

	WARN_ON(tegra->ss_pwr_gated && tegra->host_pwr_gated);

	/* save xhci spec ctx. Already done by xhci_suspend */
	err = xhci_suspend(tegra->xhci);
	if (err) {
		xhci_err(xhci, "%s: xhci_suspend failed %d\n", __func__, err);
		goto xhci_suspend_failed;
	}

	/* Powergate host. Include ss power gate if not already done */
	err = tegra_xhci_host_elpg_entry(tegra);
	if (err) {
		xhci_err(xhci, "%s: unable to perform elpg entry %d\n",
				__func__, err);
		goto tegra_xhci_host_elpg_entry_failed;
	}

	/* At this point,ensure ss/hs intr enables are always on */
	tegra_xhci_ss_wake_on_interrupts(tegra->bdata->portmap, true);
	tegra_xhci_hs_wake_on_interrupts(tegra->bdata->portmap, true);

	/* In ELPG, firmware log context is gone. Rewind shared log buffer. */
	if (fw_log_wait_empty_timeout(tegra, 100))
		xhci_warn(xhci, "%s still has logs\n", __func__);
	tegra->log.dequeue = tegra->log.virt_addr;
	tegra->log.seq = 0;

done:
	/* pads are disabled only if usb2 root hub in xusb is idle */
	/* pads will actually be disabled only when all usb2 ports are idle */
	if (xhci->main_hcd == hcd) {
		utmi_phy_pad_disable();
		utmi_phy_iddq_override(true);
	}
	mutex_unlock(&tegra->sync_lock);
	return 0;

tegra_xhci_host_elpg_entry_failed:

xhci_suspend_failed:
	tegra->hc_in_elpg = false;
xhci_bus_suspend_failed:
	if (xhci->shared_hcd == hcd)
		tegra->usb3_rh_suspend = false;
	else if (xhci->main_hcd == hcd)
		tegra->usb2_rh_suspend = false;

	mutex_unlock(&tegra->sync_lock);
	return err;
}

/* First, USB2HCD and then USB3HCD resume will be called */
static int tegra_xhci_bus_resume(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int err = 0;

	mutex_lock(&tegra->sync_lock);

	tegra->host_resume_req = true;

	if (xhci->shared_hcd == hcd)
		xhci_dbg(xhci, "%s: usb3 root hub\n", __func__);
	else if (xhci->main_hcd == hcd)
		xhci_dbg(xhci, "%s: usb2 root hub\n", __func__);

	/* pads are disabled only if usb2 root hub in xusb is idle */
	/* pads will actually be disabled only when all usb2 ports are idle */
	if (xhci->main_hcd == hcd && tegra->usb2_rh_suspend) {
		utmi_phy_pad_enable();
		utmi_phy_iddq_override(false);
	}
	if (tegra->usb2_rh_suspend && tegra->usb3_rh_suspend) {
		if (tegra->ss_pwr_gated && tegra->host_pwr_gated)
			tegra_xhci_host_partition_elpg_exit(tegra);
	}

	 /* handle remote wakeup before resuming bus */
	wait_remote_wakeup_ports(hcd);

	err = xhci_bus_resume(hcd);
	if (err) {
		xhci_err(xhci, "%s: xhci_bus_resume failed %d\n",
				__func__, err);
		goto xhci_bus_resume_failed;
	}

	if (xhci->shared_hcd == hcd)
		tegra->usb3_rh_suspend = false;
	else if (xhci->main_hcd == hcd)
		tegra->usb2_rh_suspend = false;

	mutex_unlock(&tegra->sync_lock);
	return 0;

xhci_bus_resume_failed:
	/* TODO: reverse elpg? */
	mutex_unlock(&tegra->sync_lock);
	return err;
}
#endif

static irqreturn_t tegra_xhci_irq(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	irqreturn_t iret = IRQ_HANDLED;
	u32 status;

	spin_lock(&tegra->lock);
	if (tegra->hc_in_elpg) {
		spin_lock(&xhci->lock);
		if (HCD_HW_ACCESSIBLE(hcd)) {
			status = xhci_readl(xhci, &xhci->op_regs->status);
			status |= STS_EINT;
			xhci_writel(xhci, status, &xhci->op_regs->status);
		}
		xhci_dbg(xhci, "%s: schedule host_elpg_exit_work\n",
				__func__);
		schedule_work(&tegra->host_elpg_exit_work);
		spin_unlock(&xhci->lock);
	} else
		iret = xhci_irq(hcd);
	spin_unlock(&tegra->lock);

	wake_up_interruptible(&tegra->log.intr_wait);

	return iret;
}


static const struct hc_driver tegra_plat_xhci_driver = {
	.description =		"tegra-xhci",
	.product_desc =		"Nvidia xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd *),

	/*
	 * generic hardware linkage
	 */
	.irq =			tegra_xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =		xhci_plat_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =		xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,

#ifdef CONFIG_PM
	.bus_suspend =		tegra_xhci_bus_suspend,
	.bus_resume =		tegra_xhci_bus_resume,
#endif
};

#ifdef CONFIG_PM
static int
tegra_xhci_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd *xhci = tegra->xhci;

	int ret = 0;

	mutex_lock(&tegra->sync_lock);
	if (!tegra->init_done) {
		xhci_warn(xhci, "%s: xhci probe not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}
	if (!tegra->hc_in_elpg) {
		xhci_warn(xhci, "%s: lp0 suspend entry while elpg not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}
	mutex_unlock(&tegra->sync_lock);

	tegra_xhci_ss_wake_on_interrupts(tegra->bdata->portmap, false);
	tegra_xhci_hs_wake_on_interrupts(tegra->bdata->portmap, false);

	/* enable_irq_wake for ss ports */
	ret = enable_irq_wake(tegra->padctl_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable USB host mode wakeup, irq=%d, error=%d\n",
		__func__, tegra->padctl_irq, ret);
	}

	/* enable_irq_wake for utmip/uhisc wakes */
	ret = enable_irq_wake(tegra->usb3_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable utmip/uhsic wakeup, irq=%d, error=%d\n",
		__func__, tegra->usb3_irq, ret);
	}

	/* enable_irq_wake for utmip/uhisc wakes */
	ret = enable_irq_wake(tegra->usb2_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable utmip/uhsic wakeup, irq=%d, error=%d\n",
		__func__, tegra->usb2_irq, ret);
	}

	regulator_disable(tegra->xusb_s1p8v_reg);
	regulator_disable(tegra->xusb_s1p05v_reg);
	tegra_usb2_clocks_deinit(tegra);

	return ret;
}

static int
tegra_xhci_resume(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd *xhci = tegra->xhci;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	mutex_lock(&tegra->sync_lock);
	if (!tegra->init_done) {
		xhci_warn(xhci, "%s: xhci probe not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}
	mutex_unlock(&tegra->sync_lock);

	tegra->last_jiffies = jiffies;

	disable_irq_wake(tegra->padctl_irq);
	disable_irq_wake(tegra->usb3_irq);
	disable_irq_wake(tegra->usb2_irq);
	tegra->lp0_exit = true;

	regulator_enable(tegra->xusb_s1p05v_reg);
	regulator_enable(tegra->xusb_s1p8v_reg);
	tegra_usb2_clocks_init(tegra);

	return 0;
}
#endif

static int init_filesystem_firmware(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int ret;

	ret = request_firmware_nowait(THIS_MODULE, true, firmware_file,
		&pdev->dev, GFP_KERNEL, tegra, init_filesystem_firmware_done);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_firmware failed %d\n", ret);
		return ret;
	}

	return ret;
}

static void init_filesystem_firmware_done(const struct firmware *fw,
					void *context)
{
	struct tegra_xhci_hcd *tegra = context;
	struct platform_device *pdev = tegra->pdev;
	struct cfgtbl *fw_cfgtbl;
	size_t fw_size;
	void *fw_data;
	dma_addr_t fw_dma;
	int ret;

	mutex_lock(&tegra->sync_lock);

	if (fw == NULL) {
		dev_err(&pdev->dev,
			"failed to init firmware from filesystem: %s\n",
			firmware_file);
		goto err_firmware_done;
	}

	fw_cfgtbl = (struct cfgtbl *) fw->data;
	fw_size = fw_cfgtbl->fwimg_len;
	dev_info(&pdev->dev, "Firmware File: %s (%d Bytes)\n",
			firmware_file, fw_size);

	fw_data = dma_alloc_coherent(&pdev->dev, fw_size,
			&fw_dma, GFP_KERNEL);
	if (!fw_data) {
		dev_err(&pdev->dev, "%s: dma_alloc_coherent failed\n",
			__func__);
		goto err_firmware_done;
	}

	memcpy(fw_data, fw->data, fw_size);
	dev_info(&pdev->dev,
		"Firmware DMA Memory: dma 0x%p mapped 0x%p (%d Bytes)\n",
		(void *) fw_dma, fw_data, fw_size);

	/* all set and ready to go */
	tegra->firmware.data = fw_data;
	tegra->firmware.dma = fw_dma;
	tegra->firmware.size = fw_size;

	ret = tegra_xhci_probe2(tegra);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to probe: %d\n", __func__, ret);
		goto err_firmware_done;
	}

	release_firmware(fw);
	mutex_unlock(&tegra->sync_lock);
	return;

err_firmware_done:
	release_firmware(fw);
	mutex_unlock(&tegra->sync_lock);
	device_release_driver(&pdev->dev);
}

static void deinit_filesystem_firmware(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;

	if (tegra->firmware.data) {
		dma_free_coherent(&pdev->dev, tegra->firmware.size,
			tegra->firmware.data, tegra->firmware.dma);
	}

	memset(&tegra->firmware, 0, sizeof(tegra->firmware));
}
static int init_firmware(struct tegra_xhci_hcd *tegra)
{
	return init_filesystem_firmware(tegra);
}

static void deinit_firmware(struct tegra_xhci_hcd *tegra)
{
	return deinit_filesystem_firmware(tegra);
}

static int tegra_enable_xusb_clk(struct tegra_xhci_hcd *tegra,
		struct platform_device *pdev)
{
	int err = 0;
	/* enable ss clock */
	err = clk_enable(tegra->host_clk);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable host partition clk\n");
		goto enable_host_clk_failed;
	}

	err = clk_enable(tegra->ss_clk);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable ss partition clk\n");
		goto eanble_ss_clk_failed;
	}

	err = clk_enable(tegra->emc_clk);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable xusb.emc clk\n");
		goto eanble_emc_clk_failed;
	}

	return 0;

eanble_emc_clk_failed:
	clk_disable(tegra->ss_clk);

eanble_ss_clk_failed:
	clk_disable(tegra->host_clk);

enable_host_clk_failed:
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_disable(tegra->pll_re_vco_clk);
	return err;
}

static struct tegra_xusb_padctl_regs t114_padregs_offset = {
	.boot_media_0			= 0x0,
	.usb2_pad_mux_0			= 0x4,
	.usb2_port_cap_0		= 0x8,
	.snps_oc_map_0			= 0xc,
	.usb2_oc_map_0			= 0x10,
	.ss_port_map_0			= 0x14,
	.oc_det_0			= 0x18,
	.elpg_program_0			= 0x1c,
	.usb2_bchrg_otgpad0_ctl0_0	= 0x20,
	.usb2_bchrg_otgpad0_ctl1_0	= 0xffff,
	.usb2_bchrg_otgpad1_ctl0_0	= 0x24,
	.usb2_bchrg_otgpad1_ctl1_0	= 0xffff,
	.usb2_bchrg_otgpad2_ctl0_0	= 0xffff,
	.usb2_bchrg_otgpad2_ctl1_0	= 0xffff,
	.usb2_bchrg_bias_pad_0		= 0x28,
	.usb2_bchrg_tdcd_dbnc_timer_0	= 0x2c,
	.iophy_pll_p0_ctl1_0		= 0x30,
	.iophy_pll_p0_ctl2_0		= 0x34,
	.iophy_pll_p0_ctl3_0		= 0x38,
	.iophy_pll_p0_ctl4_0		= 0x3c,
	.iophy_usb3_pad0_ctl1_0		= 0x40,
	.iophy_usb3_pad1_ctl1_0		= 0x44,
	.iophy_usb3_pad0_ctl2_0		= 0x48,
	.iophy_usb3_pad1_ctl2_0		= 0x4c,
	.iophy_usb3_pad0_ctl3_0		= 0x50,
	.iophy_usb3_pad1_ctl3_0		= 0x54,
	.iophy_usb3_pad0_ctl4_0		= 0x58,
	.iophy_usb3_pad1_ctl4_0		= 0x5c,
	.iophy_misc_pad_p0_ctl1_0	= 0x60,
	.iophy_misc_pad_p1_ctl1_0	= 0x64,
	.iophy_misc_pad_p0_ctl2_0	= 0x68,
	.iophy_misc_pad_p1_ctl2_0	= 0x6c,
	.iophy_misc_pad_p0_ctl3_0	= 0x70,
	.iophy_misc_pad_p1_ctl3_0	= 0x74,
	.iophy_misc_pad_p0_ctl4_0	= 0x78,
	.iophy_misc_pad_p1_ctl4_0	= 0x7c,
	.iophy_misc_pad_p0_ctl5_0	= 0x80,
	.iophy_misc_pad_p1_ctl5_0	= 0x84,
	.iophy_misc_pad_p0_ctl6_0	= 0x88,
	.iophy_misc_pad_p1_ctl6_0	= 0x8c,
	.usb2_otg_pad0_ctl0_0		= 0x90,
	.usb2_otg_pad1_ctl0_0		= 0x94,
	.usb2_otg_pad2_ctl0_0		= 0xffff,
	.usb2_otg_pad0_ctl1_0		= 0x98,
	.usb2_otg_pad1_ctl1_0		= 0x9c,
	.usb2_otg_pad2_ctl1_0		= 0xffff,
	.usb2_bias_pad_ctl0_0		= 0xa0,
	.usb2_bias_pad_ctl1_0		= 0xa4,
	.usb2_hsic_pad0_ctl0_0		= 0xa8,
	.usb2_hsic_pad1_ctl0_0		= 0xac,
	.usb2_hsic_pad0_ctl1_0		= 0xb0,
	.usb2_hsic_pad1_ctl1_0		= 0xb4,
	.usb2_hsic_pad0_ctl2_0		= 0xb8,
	.usb2_hsic_pad1_ctl2_0		= 0xbc,
	.ulpi_link_trim_ctl0		= 0xc0,
	.ulpi_null_clk_trim_ctl0	= 0xc4,
	.hsic_strb_trim_ctl0		= 0xc8,
	.wake_ctl0			= 0xcc,
	.pm_spare0			= 0xd0,
	.iophy_misc_pad_p2_ctl1_0	= 0xffff,
	.iophy_misc_pad_p3_ctl1_0	= 0xffff,
	.iophy_misc_pad_p4_ctl1_0	= 0xffff,
	.iophy_misc_pad_p2_ctl2_0	= 0xffff,
	.iophy_misc_pad_p3_ctl2_0	= 0xffff,
	.iophy_misc_pad_p4_ctl2_0	= 0xffff,
	.iophy_misc_pad_p2_ctl3_0	= 0xffff,
	.iophy_misc_pad_p3_ctl3_0	= 0xffff,
	.iophy_misc_pad_p4_ctl3_0	= 0xffff,
	.iophy_misc_pad_p2_ctl4_0	= 0xffff,
	.iophy_misc_pad_p3_ctl4_0	= 0xffff,
	.iophy_misc_pad_p4_ctl4_0	= 0xffff,
	.iophy_misc_pad_p2_ctl5_0	= 0xffff,
	.iophy_misc_pad_p3_ctl5_0	= 0xffff,
	.iophy_misc_pad_p4_ctl5_0	= 0xffff,
	.iophy_misc_pad_p2_ctl6_0	= 0xffff,
	.iophy_misc_pad_p3_ctl6_0	= 0xffff,
	.iophy_misc_pad_p4_ctl6_0	= 0xffff,
	.usb3_pad_mux_0			= 0xffff,
	.iophy_pll_s0_ctl1_0		= 0xffff,
	.iophy_pll_s0_ctl2_0		= 0xffff,
	.iophy_pll_s0_ctl3_0		= 0xffff,
	.iophy_pll_s0_ctl4_0		= 0xffff,
	.iophy_misc_pad_s0_ctl1_0	= 0xffff,
	.iophy_misc_pad_s0_ctl2_0	= 0xffff,
	.iophy_misc_pad_s0_ctl3_0	= 0xffff,
	.iophy_misc_pad_s0_ctl4_0	= 0xffff,
	.iophy_misc_pad_s0_ctl5_0	= 0xffff,
	.iophy_misc_pad_s0_ctl6_0	= 0xffff,
};

static struct tegra_xusb_padctl_regs t124_padregs_offset = {
	.boot_media_0			= 0x0,
	.usb2_pad_mux_0			= 0x4,
	.usb2_port_cap_0		= 0x8,
	.snps_oc_map_0			= 0xc,
	.usb2_oc_map_0			= 0x10,
	.ss_port_map_0			= 0x14,
	.oc_det_0			= 0x18,
	.elpg_program_0			= 0x1c,
	.usb2_bchrg_otgpad0_ctl0_0	= 0x20,
	.usb2_bchrg_otgpad0_ctl1_0	= 0x24,
	.usb2_bchrg_otgpad1_ctl0_0	= 0x28,
	.usb2_bchrg_otgpad1_ctl1_0	= 0x2c,
	.usb2_bchrg_otgpad2_ctl0_0	= 0x30,
	.usb2_bchrg_otgpad2_ctl1_0	= 0x34,
	.usb2_bchrg_bias_pad_0		= 0x38,
	.usb2_bchrg_tdcd_dbnc_timer_0	= 0x3c,
	.iophy_pll_p0_ctl1_0		= 0x40,
	.iophy_pll_p0_ctl2_0		= 0x44,
	.iophy_pll_p0_ctl3_0		= 0x48,
	.iophy_pll_p0_ctl4_0		= 0x4c,
	.iophy_usb3_pad0_ctl1_0		= 0x50,
	.iophy_usb3_pad1_ctl1_0		= 0x54,
	.iophy_usb3_pad0_ctl2_0		= 0x58,
	.iophy_usb3_pad1_ctl2_0		= 0x5c,
	.iophy_usb3_pad0_ctl3_0		= 0x60,
	.iophy_usb3_pad1_ctl3_0		= 0x64,
	.iophy_usb3_pad0_ctl4_0		= 0x68,
	.iophy_usb3_pad1_ctl4_0		= 0x6c,
	.iophy_misc_pad_p0_ctl1_0	= 0x70,
	.iophy_misc_pad_p1_ctl1_0	= 0x74,
	.iophy_misc_pad_p0_ctl2_0	= 0x78,
	.iophy_misc_pad_p1_ctl2_0	= 0x7c,
	.iophy_misc_pad_p0_ctl3_0	= 0x80,
	.iophy_misc_pad_p1_ctl3_0	= 0x84,
	.iophy_misc_pad_p0_ctl4_0	= 0x88,
	.iophy_misc_pad_p1_ctl4_0	= 0x8c,
	.iophy_misc_pad_p0_ctl5_0	= 0x90,
	.iophy_misc_pad_p1_ctl5_0	= 0x94,
	.iophy_misc_pad_p0_ctl6_0	= 0x98,
	.iophy_misc_pad_p1_ctl6_0	= 0x9c,
	.usb2_otg_pad0_ctl0_0		= 0xa0,
	.usb2_otg_pad1_ctl0_0		= 0xa4,
	.usb2_otg_pad2_ctl0_0		= 0xa8,
	.usb2_otg_pad0_ctl1_0		= 0xac,
	.usb2_otg_pad1_ctl1_0		= 0xb0,
	.usb2_otg_pad2_ctl1_0		= 0xb4,
	.usb2_bias_pad_ctl0_0		= 0xb8,
	.usb2_bias_pad_ctl1_0		= 0xbc,
	.usb2_hsic_pad0_ctl0_0		= 0xc0,
	.usb2_hsic_pad1_ctl0_0		= 0xc4,
	.usb2_hsic_pad0_ctl1_0		= 0xc8,
	.usb2_hsic_pad1_ctl1_0		= 0xcc,
	.usb2_hsic_pad0_ctl2_0		= 0xd0,
	.usb2_hsic_pad1_ctl2_0		= 0xd4,
	.ulpi_link_trim_ctl0		= 0xd8,
	.ulpi_null_clk_trim_ctl0	= 0xdc,
	.hsic_strb_trim_ctl0		= 0xe0,
	.wake_ctl0			= 0xe4,
	.pm_spare0			= 0xe8,
	.iophy_misc_pad_p2_ctl1_0	= 0xec,
	.iophy_misc_pad_p3_ctl1_0	= 0xf0,
	.iophy_misc_pad_p4_ctl1_0	= 0xf4,
	.iophy_misc_pad_p2_ctl2_0	= 0xf8,
	.iophy_misc_pad_p3_ctl2_0	= 0xfc,
	.iophy_misc_pad_p4_ctl2_0	= 0x100,
	.iophy_misc_pad_p2_ctl3_0	= 0x104,
	.iophy_misc_pad_p3_ctl3_0	= 0x108,
	.iophy_misc_pad_p4_ctl3_0	= 0x10c,
	.iophy_misc_pad_p2_ctl4_0	= 0x110,
	.iophy_misc_pad_p3_ctl4_0	= 0x114,
	.iophy_misc_pad_p4_ctl4_0	= 0x118,
	.iophy_misc_pad_p2_ctl5_0	= 0x11c,
	.iophy_misc_pad_p3_ctl5_0	= 0x120,
	.iophy_misc_pad_p4_ctl5_0	= 0x124,
	.iophy_misc_pad_p2_ctl6_0	= 0x128,
	.iophy_misc_pad_p3_ctl6_0	= 0x12c,
	.iophy_misc_pad_p4_ctl6_0	= 0x130,
	.usb3_pad_mux_0			= 0x134,
	.iophy_pll_s0_ctl1_0		= 0x138,
	.iophy_pll_s0_ctl2_0		= 0x13c,
	.iophy_pll_s0_ctl3_0		= 0x140,
	.iophy_pll_s0_ctl4_0		= 0x144,
	.iophy_misc_pad_s0_ctl1_0	= 0x148,
	.iophy_misc_pad_s0_ctl2_0	= 0x14c,
	.iophy_misc_pad_s0_ctl3_0	= 0x150,
	.iophy_misc_pad_s0_ctl4_0	= 0x154,
	.iophy_misc_pad_s0_ctl5_0	= 0x158,
	.iophy_misc_pad_s0_ctl6_0	= 0x15c,
};

/* FIXME: using notifier to transfer control to host from suspend
 * for otg port when xhci is in elpg. Find  better alternative
 */
static int tegra_xhci_otg_notify(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	struct tegra_xhci_hcd *tegra = container_of(nb,
					struct tegra_xhci_hcd, otgnb);

	if ((event == USB_EVENT_ID))
		if (tegra->hc_in_elpg) {
			schedule_work(&tegra->host_elpg_exit_work);
			tegra->host_resume_req = true;
	}

	return NOTIFY_OK;
}

static void tegra_xusb_read_board_data(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_board_data *bdata = tegra->bdata;
	struct device_node *node = tegra->pdev->dev.of_node;
	int ret;

	bdata->uses_external_pmic = of_property_read_bool(node,
					"nvidia,uses_external_pmic");
	bdata->gpio_controls_muxed_ss_lanes = of_property_read_bool(node,
					"nvidia,gpio_controls_muxed_ss_lanes");
	ret = of_property_read_u32(node, "nvidia,gpio_ss1_sata",
					&bdata->gpio_ss1_sata);
	ret = of_property_read_u32(node, "nvidia,portmap",
					&bdata->portmap);
	ret = of_property_read_u32(node, "nvidia,ss_portmap",
					(u32 *) &bdata->ss_portmap);
	ret = of_property_read_u32(node, "nvidia,lane_owner",
					(u32 *) &bdata->lane_owner);
	ret = of_property_read_u32(node, "nvidia,ulpicap",
					(u32 *) &bdata->ulpicap);
	ret = of_property_read_string_index(node, "nvidia,supply_utmi_vbuses",
					0, &bdata->supply.utmi_vbuses[0]);
	ret = of_property_read_string_index(node, "nvidia,supply_utmi_vbuses",
					1, &bdata->supply.utmi_vbuses[1]);
	ret = of_property_read_string_index(node, "nvidia,supply_utmi_vbuses",
					2, &bdata->supply.utmi_vbuses[2]);
	ret = of_property_read_string(node, "nvidia,supply_s3p3v",
					&bdata->supply.s3p3v);
	ret = of_property_read_string(node, "nvidia,supply_s1p8v",
					&bdata->supply.s1p8v);
	ret = of_property_read_string(node, "nvidia,supply_vddio_hsic",
					&bdata->supply.vddio_hsic);
	ret = of_property_read_string(node, "nvidia,supply_s1p05v",
					&bdata->supply.s1p05v);
	ret = of_property_read_u8_array(node, "nvidia,hsic0",
					(u8 *) &bdata->hsic[0],
					sizeof(bdata->hsic[0]));
	ret = of_property_read_u8_array(node, "nvidia,hsic1",
					(u8 *) &bdata->hsic[1],
					sizeof(bdata->hsic[0]));
	/* TODO: Add error conditions check */
}

static void tegra_xusb_read_calib_data(struct tegra_xhci_hcd *tegra)
{
	u32 usb_calib0 = tegra_fuse_readl(FUSE_SKU_USB_CALIB_0);
	struct tegra_xusb_chip_calib *cdata = tegra->cdata;

	pr_info("tegra_xusb_read_usb_calib: usb_calib0 = 0x%08x\n", usb_calib0);
	/*
	 * read from usb_calib0 and pass to driver
	 * set HS_CURR_LEVEL (PAD0)	= usb_calib0[5:0]
	 * set TERM_RANGE_ADJ		= usb_calib0[10:7]
	 * set HS_SQUELCH_LEVEL		= usb_calib0[12:11]
	 * set HS_IREF_CAP		= usb_calib0[14:13]
	 * set HS_CURR_LEVEL (PAD1)	= usb_calib0[20:15]
	 */

	cdata->hs_curr_level_pad0 = (usb_calib0 >> 0) & 0x3f;
	cdata->hs_term_range_adj = (usb_calib0 >> 7) & 0xf;
	cdata->hs_squelch_level = (usb_calib0 >> 11) & 0x3;
	cdata->hs_iref_cap = (usb_calib0 >> 13) & 0x3;
	cdata->hs_curr_level_pad1 = (usb_calib0 >> 15) & 0x3f;
	cdata->hs_curr_level_pad2 = (usb_calib0 >> 15) & 0x3f;
}

static const struct tegra_xusb_soc_config tegra114_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 4),
	.quirks = TEGRA_XUSB_USE_HS_SRC_CLOCK2,
	.rx_wander = (0x3 << 4),
	.rx_eq = (0x3928 << 8),
	.cdr_cntl = (0x26 << 24),
	.dfe_cntl = 0x002008EE,
	.hs_slew = (0xE << 6),
	.ls_rslew_pad0 = (0x3 << 14),
	.ls_rslew_pad1 = (0x0 << 14),
	.hs_disc_lvl = (0x5 << 2),
	.spare_in = 0x0,
};

static const struct tegra_xusb_soc_config tegra124_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT1 << 4) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 8),
	.rx_wander = (0xF << 4),
	.rx_eq = (0xF070 << 8),
	.cdr_cntl = (0x26 << 24),
	.dfe_cntl = 0x002008EE,
	.hs_slew = (0xE << 6),
	.ls_rslew_pad0 = (0x3 << 14),
	.ls_rslew_pad1 = (0x0 << 14),
	.ls_rslew_pad2 = (0x0 << 14),
	.hs_disc_lvl = (0x5 << 2),
	.spare_in = 0x1,
};

static struct of_device_id tegra_xhci_of_match[] = {
	{ .compatible = "nvidia,tegra114-xhci", .data = &tegra114_soc_config },
	{ .compatible = "nvidia,tegra124-xhci", .data = &tegra124_soc_config },
	{ },
};

/* TODO: we have to refine error handling in tegra_xhci_probe() */
static int tegra_xhci_probe(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra;
	struct resource	*res;
	unsigned pad;
	u32 val;
	int ret;
	int irq;
	const struct tegra_xusb_soc_config *soc_config;
	const struct of_device_id *match;

	BUILD_BUG_ON(sizeof(struct cfgtbl) != 256);

	if (usb_disabled())
		return -ENODEV;

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}
	mutex_init(&tegra->sync_lock);
	spin_lock_init(&tegra->lock);
	mutex_init(&tegra->mbox_lock);

	tegra->init_done = false;

	tegra->bdata = devm_kzalloc(&pdev->dev, sizeof(
					struct tegra_xusb_board_data),
					GFP_KERNEL);
	if (!tegra->bdata) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}
	tegra->cdata = devm_kzalloc(&pdev->dev, sizeof(
					struct tegra_xusb_chip_calib),
					GFP_KERNEL);
	if (!tegra->cdata) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}
	match = of_match_device(tegra_xhci_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	soc_config = match->data;
	/* Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	tegra->tegra_xusb_dmamask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &tegra->tegra_xusb_dmamask;

	tegra->pdev = pdev;
	tegra_xusb_read_calib_data(tegra);
	tegra_xusb_read_board_data(tegra);
	tegra->pdata = dev_get_platdata(&pdev->dev);
	tegra->bdata->portmap = tegra->pdata->portmap;
	tegra->bdata->hsic[0].pretend_connect =
				tegra->pdata->pretend_connect_0;
	if (tegra->bdata->portmap == NULL)
		return -ENODEV;
	tegra->bdata->lane_owner = tegra->pdata->lane_owner;
	tegra->soc_config = soc_config;
	tegra->ss_pwr_gated = false;
	tegra->host_pwr_gated = false;
	tegra->hc_in_elpg = false;
	tegra->hs_wake_event = false;
	tegra->host_resume_req = false;
	tegra->lp0_exit = false;

	/* request resource padctl base address */
	ret = tegra_xhci_request_mem_region(pdev, 3, &tegra->padctl_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map padctl\n");
		return ret;
	}

	/* request resource fpci base address */
	ret = tegra_xhci_request_mem_region(pdev, 1, &tegra->fpci_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map fpci\n");
		return ret;
	}

	/* request resource ipfs base address */
	ret = tegra_xhci_request_mem_region(pdev, 2, &tegra->ipfs_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map ipfs\n");
		return ret;
	}

	ret = tegra_xusb_partitions_clk_init(tegra);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to initialize xusb partitions clocks\n");
		return ret;
	}

	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0) {
		tegra->transceiver = usb_get_phy(USB_PHY_TYPE_USB2);
		if (IS_ERR_OR_NULL(tegra->transceiver)) {
			dev_err(&pdev->dev, "failed to get usb phy\n");
			tegra->transceiver = NULL;
		}
	}

	/* Enable power rails to the PAD,VBUS
	 * and pull-up voltage.Initialize the regulators
	 */
	ret = tegra_xusb_regulator_init(tegra, pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize xusb regulator\n");
		if (ret == -ENODEV) {
			ret = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Retry at a later stage\n");
		}
		goto err_deinit_xusb_partition_clk;
	}

	/* Enable UTMIP, PLLU and PLLE */
	ret = tegra_usb2_clocks_init(tegra);
	if (ret) {
		dev_err(&pdev->dev, "error initializing usb2 clocks\n");
		goto err_deinit_tegra_xusb_regulator;
	}

	/* tegra_unpowergate_partition also does partition reset deassert */
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_XUSBA);
	if (ret)
		dev_err(&pdev->dev, "could not unpowergate xusba partition\n");

	/* tegra_unpowergate_partition also does partition reset deassert */
	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_XUSBC);
	if (ret)
		dev_err(&pdev->dev, "could not unpowergate xusbc partition\n");

	ret = tegra_enable_xusb_clk(tegra, pdev);
	if (ret)
		dev_err(&pdev->dev, "could not enable partition clock\n");

	/* reset the pointer back to NULL. driver uses it */
	/* platform_set_drvdata(pdev, NULL); */

	/* request resource host base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "mem resource host doesn't exist\n");
		ret = -ENODEV;
		goto err_deinit_usb2_clocks;
	}
	tegra->host_phy_base = res->start;
	tegra->host_phy_size = resource_size(res);

	tegra->host_phy_virt_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
	if (!tegra->host_phy_virt_base) {
		dev_err(&pdev->dev, "error mapping host phy memory\n");
		ret = -ENOMEM;
		goto err_deinit_usb2_clocks;
	}

	/* Setup IPFS access and BAR0 space */
	tegra_xhci_cfg(tegra);

	val = readl(tegra->fpci_base + XUSB_CFG_0);
	tegra->device_id = (val >> 16) & 0xffff;

	dev_info(&pdev->dev, "XUSB device id = 0x%x (%s)\n", tegra->device_id,
		(XUSB_DEVICE_ID_T114 == tegra->device_id) ? "T114" : "T124+");

	if (XUSB_DEVICE_ID_T114 == tegra->device_id) {
		tegra->padregs = &t114_padregs_offset;
	} else if (XUSB_DEVICE_ID_T124 == tegra->device_id) {
		tegra->padregs = &t124_padregs_offset;
	} else {
		dev_info(&pdev->dev, "XUSB device_id neither T114 nor T124!\n");
		dev_info(&pdev->dev, "XUSB using T124 pad register offsets!\n");
		tegra->padregs = &t124_padregs_offset;
	}

	/* calculate rctrl_val and tctrl_val once at boot time */
	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 == tegra->device_id)
		tegra_xhci_war_for_tctrl_rctrl(tegra);

	for_each_enabled_hsic_pad(pad, tegra)
		hsic_power_rail_enable(tegra);

	/* Program the XUSB pads to take ownership of ports */
	tegra_xhci_padctl_portmap_and_caps(tegra);

	/* Release XUSB wake logic state latching */
	tegra_xhci_ss_wake_signal(tegra->bdata->portmap, false);
	tegra_xhci_ss_vcore(tegra->bdata->portmap, false);

	/* Deassert reset to XUSB host, ss, dev clocks */
	tegra_periph_reset_deassert(tegra->host_clk);
	tegra_periph_reset_deassert(tegra->ss_clk);

	platform_set_drvdata(pdev, tegra);
	fw_log_init(tegra);
	ret = init_firmware(tegra);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init firmware\n");
		ret = -ENODEV;
		goto err_deinit_firmware_log;
	}

	return 0;

err_deinit_firmware_log:
	fw_log_deinit(tegra);
err_deinit_usb2_clocks:
	tegra_usb2_clocks_deinit(tegra);
err_deinit_tegra_xusb_regulator:
	tegra_xusb_regulator_deinit(tegra);
err_deinit_xusb_partition_clk:
	if (tegra->transceiver)
		usb_unregister_notifier(tegra->transceiver, &tegra->otgnb);

	tegra_xusb_partitions_clk_deinit(tegra);

	return ret;
}

static int tegra_xhci_probe2(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	const struct hc_driver *driver;
	int ret;
	struct resource	*res;
	int irq;
	struct xhci_hcd	*xhci;
	struct usb_hcd	*hcd;
	unsigned port;


	ret = load_firmware(tegra, true /* do reset ARU */);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to load firmware\n");
		return -ENODEV;
	}
	pmc_init(tegra);

	device_init_wakeup(&pdev->dev, 1);
	driver = &tegra_plat_xhci_driver;

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "failed to create usb2 hcd\n");
		return -ENOMEM;
	}

	/* request resource host base address */
	ret = tegra_xhci_request_mem_region(pdev, 0, &hcd->regs);
	if (ret) {
		dev_err(&pdev->dev, "failed to map host\n");
		goto err_put_usb2_hcd;
	}
	hcd->rsrc_start = tegra->host_phy_base;
	hcd->rsrc_len = tegra->host_phy_size;

	/* Register interrupt handler for HOST */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "irq resource host doesn't exist\n");
		ret = -ENODEV;
		goto err_put_usb2_hcd;
	}

	irq = res->start;
	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "failed to add usb2hcd, error = %d\n", ret);
		goto err_put_usb2_hcd;
	}

	/* USB 2.0 roothub is stored in the platform_device now. */
	hcd = dev_get_drvdata(&pdev->dev);
	xhci = hcd_to_xhci(hcd);
	tegra->xhci = xhci;
	platform_set_drvdata(pdev, tegra);

	if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0) {
		if (!IS_ERR_OR_NULL(tegra->transceiver)) {
			otg_set_host(tegra->transceiver->otg, &hcd->self);
			tegra->otgnb.notifier_call = tegra_xhci_otg_notify;
			usb_register_notifier(tegra->transceiver,
				&tegra->otgnb);
		}
	}

	xhci->shared_hcd = usb_create_shared_hcd(driver, &pdev->dev,
						dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		dev_err(&pdev->dev, "failed to create usb3 hcd\n");
		ret = -ENOMEM;
		goto err_remove_usb2_hcd;
	}

	/*
	 * Set the xHCI pointer before xhci_plat_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "failed to add usb3hcd, error = %d\n", ret);
		goto err_put_usb3_hcd;
	}

	device_init_wakeup(&hcd->self.root_hub->dev, 1);
	device_init_wakeup(&xhci->shared_hcd->self.root_hub->dev, 1);

	/* do mailbox related initializations */
	tegra->mbox_owner = 0xffff;
	INIT_WORK(&tegra->mbox_work, tegra_xhci_process_mbox_message);

	tegra_xhci_enable_fw_message(tegra);

	/* do ss partition elpg exit related initialization */
	INIT_WORK(&tegra->ss_elpg_exit_work, ss_partition_elpg_exit_work);

	/* do host partition elpg exit related initialization */
	INIT_WORK(&tegra->host_elpg_exit_work, host_partition_elpg_exit_work);

	/* Register interrupt handler for SMI line to handle mailbox
	 * interrupt from firmware
	 */

	ret = tegra_xhci_request_irq(pdev, 1, tegra_xhci_smi_irq,
			IRQF_SHARED, "tegra_xhci_mbox_irq", &tegra->smi_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt handler for PADCTRL line to
	 * handle wake on connect irqs interrupt from
	 * firmware
	 */
	ret = tegra_xhci_request_irq(pdev, 2, tegra_xhci_padctl_irq,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			"tegra_xhci_padctl_irq", &tegra->padctl_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt wake handler for USB2 */
	ret = tegra_xhci_request_irq(pdev, 4, pmc_usb_phy_wake_isr,
		IRQF_SHARED | IRQF_TRIGGER_HIGH, "pmc_usb_phy_wake_isr",
		&tegra->usb2_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt wake handler for USB3 */
	ret = tegra_xhci_request_irq(pdev, 3, pmc_usb_phy_wake_isr,
		IRQF_SHARED | IRQF_TRIGGER_HIGH, "pmc_usb_phy_wake_isr",
		&tegra->usb3_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	for (port = 0; port < XUSB_SS_PORT_COUNT; port++) {
		tegra->ctle_ctx_saved[port] = false;
		tegra->dfe_ctx_saved[port] = false;
	}

	hsic_pad_pretend_connect(tegra);

	tegra_xhci_debug_read_pads(tegra);
	utmi_phy_pad_enable();
	utmi_phy_iddq_override(false);

	tegra_pd_add_device(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	tegra->init_done = true;

	return 0;

err_remove_usb3_hcd:
	usb_remove_hcd(xhci->shared_hcd);
err_put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);
err_remove_usb2_hcd:
	kfree(tegra->xhci);
	usb_remove_hcd(hcd);
err_put_usb2_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int tegra_xhci_remove(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	unsigned pad;

	if (tegra == NULL)
		return -EINVAL;

	mutex_lock(&tegra->sync_lock);

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic_pad_disable(tegra, pad);
		hsic_power_rail_disable(tegra);
	}

	if (tegra->init_done) {
		struct xhci_hcd	*xhci = NULL;
		struct usb_hcd *hcd = NULL;

		xhci = tegra->xhci;
		hcd = xhci_to_hcd(xhci);

		devm_free_irq(&pdev->dev, tegra->usb3_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->padctl_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->smi_irq, tegra);
		usb_remove_hcd(xhci->shared_hcd);
		usb_put_hcd(xhci->shared_hcd);
		usb_remove_hcd(hcd);
		usb_put_hcd(hcd);
		kfree(xhci);
	}

	deinit_firmware(tegra);
	fw_log_deinit(tegra);

	tegra_xusb_regulator_deinit(tegra);

	if (tegra->transceiver)
		usb_unregister_notifier(tegra->transceiver, &tegra->otgnb);

	tegra_usb2_clocks_deinit(tegra);
	if (!tegra->hc_in_elpg)
		tegra_xusb_partitions_clk_deinit(tegra);

	utmi_phy_pad_disable();
	utmi_phy_iddq_override(true);

	tegra_pd_remove_device(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	mutex_unlock(&tegra->sync_lock);

	return 0;
}

static void tegra_xhci_shutdown(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd	*xhci = NULL;
	struct usb_hcd *hcd = NULL;

	if (tegra == NULL)
		return;

	if (tegra->hc_in_elpg) {
		pmc_disable_bus_ctrl(tegra);
	} else {
		xhci = tegra->xhci;
		hcd = xhci_to_hcd(xhci);
		xhci_shutdown(hcd);
	}
}

static struct platform_driver tegra_xhci_driver = {
	.probe	= tegra_xhci_probe,
	.remove	= tegra_xhci_remove,
	.shutdown = tegra_xhci_shutdown,
#ifdef CONFIG_PM
	.suspend = tegra_xhci_suspend,
	.resume  = tegra_xhci_resume,
#endif
	.driver	= {
		.name = "tegra-xhci",
		.of_match_table = tegra_xhci_of_match,
	},
};
MODULE_ALIAS("platform:tegra-xhci");

int tegra_xhci_register_plat(void)
{
	return platform_driver_register(&tegra_xhci_driver);
}

void tegra_xhci_unregister_plat(void)
{
	platform_driver_unregister(&tegra_xhci_driver);
}
