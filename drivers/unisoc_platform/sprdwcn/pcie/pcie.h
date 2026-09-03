/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_DRV_H__
#define __PCIE_DRV_H__

#include <linux/pci.h>
#include <misc/wcn_bus.h>
#include <linux/time.h>

#define DRVER_NAME      "wcn_pcie"

/* Synopsis PCIE configuration registers */

/* For Region control 2 */
#define REGION_EN	BIT(31)
/*
 * 0: Address Match Mode. The iATU operates using addresses as in the outbound
 * direction. The Region Base and Limit Registers must be setup.
 *
 * 1:BAR Match Mode. BAR matching is used. The "BAR Number" field is relevant.
 * Not used for RC.
 */
#define MATCH_MODE		BIT(30)
#define ADDR_MATCH_MODE		~BIT(30)
#define BAR_MATCH_MODE		BIT(30)
#define BAR_NUM			(BIT(10) | BIT(9) | BIT(8))
#define BAR_0			BIT(8)
#define BAR_1			BIT(9)
#define BAR_2			(BIT(9) | BIT(8))
#define IATU_OFFSET_ADDR	0x10000
#define OBREG0_OFFSET_ADDR	(0x10000 + (0 * 0x200))
#define IBREG0_OFFSET_ADDR	(0x10000 + (0 * 0x200) + 0x100)
#define OBREG1_OFFSET_ADDR	(0x10000 + (1 * 0x200))
#define IBREG1_OFFSET_ADDR	(0x10000 + (1 * 0x200) + 0x100)

#define EP_IBAR0_BASE_M3E		0X40800000
#define EDMA_GLB_REG_BASE_M3E	0x600000
#define EDMA_CHN_REG_BASE_M3E	0x601000
/* 8M align */
#define EP_INBOUND_ALIGN_M3E	0x800000

#define EP_IBAR0_BASE		0x40000000
#define EDMA_GLB_REG_BASE	0x160000
#define EDMA_CHN_REG_BASE	0X161000
/* 4M align */
#define EP_INBOUND_ALIGN	0x400000

/* 4K align */
#define EP_OUTBOUND_ALIGN	0x1000

/* Parameters for the waiting for iATU enabled routine */
#define LINK_WAIT_MAX_IATU_RETRIES	5
#define LINK_WAIT_IATU			9
#define PCIE_ATU_ENABLE			(0x1 << 31)
#define PCIE_ATU_BAR_MODE_ENABLE	(0x1 << 30)

#define BUS_REMOVE_CARD_VAL 0x8000

#define WCN_PCIE_DEV_AND_VND_ID 0x180000
#define WCN_PCIE_CMD 0x180004

struct bar_info {
	resource_size_t mmio_start;
	resource_size_t mmio_end;
	resource_size_t mmio_len;
	unsigned long mmio_flags;
	unsigned char *mem;
	unsigned char *vmem;
};

struct dma_buf {
	unsigned long vir;
	unsigned long phy;
	int size;
};

struct sub_sys_pm_state {
	unsigned int bt:2;
	unsigned int wifi:2;
	unsigned int fm:2;
	unsigned int state:2;
	unsigned int rsvd:26;
};
struct wcn_pcie_info {
	struct platform_device *rc_pd;
	struct pci_dev *dev;
	struct pci_saved_state *saved_state;
	int legacy_en;
	int msi_en;
	int msix_en;
	int in_use;
	int irq;
	int irq_num;
	int irq_en;
	int bar_num;
	struct bar_info bar[8];
#ifdef CONFIG_PCI
	struct msix_entry msix[100];
#endif
	struct sub_sys_pm_state pm_state;
	/* board info */
	unsigned char revision;
	unsigned char irq_pin;
	unsigned char irq_line;
	unsigned short sub_vendor_id;
	unsigned short sub_system_id;
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned int card_dump_flag;
	struct char_drv_info *p_char;
	enum wcn_bus_state pci_status;
	struct completion scan_done;
	struct completion remove_done;
	atomic_t xmit_cnt;
	atomic_t edma_ready;
	atomic_t tx_complete;
	atomic_t card_exist;
	atomic_t is_suspending;
	struct mutex pm_lock;
};

struct inbound_reg {
	unsigned int type;/* region contril 1 ;0:mem, 2:i/o 4:cfg */
	unsigned int en;/* region contril 2 [10:8]:BAR_NUM, */
	unsigned int lower_base_addr;
	unsigned int upper_base_addr;
	unsigned int limit;
	unsigned int lower_target_addr;
	unsigned int upper_target_addr;
} __packed;

struct outbound_reg {
	unsigned int type;
	unsigned int en;
	unsigned int lower_base_addr;
	unsigned int upper_base_addr;
	unsigned int limit;
	unsigned int lower_target_addr;
	unsigned int upper_target_addr;
} __packed;

int pcie_bar_write(struct wcn_pcie_info *priv, int bar, int offset, void *buf,
		   int len);
int pcie_bar_read(struct wcn_pcie_info *priv, int bar, int offset, void *buf,
		  int len);
unsigned char *ibreg_base(struct wcn_pcie_info *priv, char region);
unsigned char *obreg_base(struct wcn_pcie_info *priv, char region);
int pcie_config_read(struct wcn_pcie_info *priv, int offset, char *buf,
		     int len);
int sprd_pcie_bar_map(struct wcn_pcie_info *priv, int bar,
		      unsigned int addr, char region);
int sprd_pcie_mem_write(unsigned int addr, void *buf, unsigned int len);
int sprd_pcie_mem_read(unsigned int addr, void *buf, unsigned int len);
int sprd_pcie_update_bits(unsigned int reg, unsigned int mask,
			  unsigned int val);

#ifdef BUILD_WCN_PCIE
char *pcie_bar_vmem(struct wcn_pcie_info *priv, int bar);
int dmalloc(struct wcn_pcie_info *priv, struct dma_buf *dm, int size);
int dmfree(struct wcn_pcie_info *priv, struct dma_buf *dm);
struct wcn_pcie_info *get_wcn_device_info(void);
#else
static inline char *pcie_bar_vmem(struct wcn_pcie_info *priv, int bar)
{
	return NULL;
}

static inline int dmalloc(struct wcn_pcie_info *priv, struct dma_buf *dm, int size)
{
	return -EINVAL;
}

static inline int dmfree(struct wcn_pcie_info *priv, struct dma_buf *dm)
{
	return -EINVAL;
}

static inline struct wcn_pcie_info *get_wcn_device_info(void)
{
	return NULL;
}
#endif

#ifdef CONFIG_PCIEASPM
int sprd_pcie_set_aspm_policy(enum sub_sys subsys, enum wcn_bus_pm_state state);
enum wcn_bus_pm_state sprd_pcie_get_aspm_policy(void);
#else
static inline int sprd_pcie_set_aspm_policy(enum sub_sys subsys,
					    enum wcn_bus_pm_state state)
{
	return -EINVAL;
}
static inline enum wcn_bus_pm_state sprd_pcie_get_aspm_policy(void)
{
	return 0;
}
#endif

int wcn_pcie_get_bus_status(void);
void sprd_pcie_set_carddump_status(unsigned int flag);
unsigned int sprd_pcie_get_carddump_status(void);
int sprd_pcie_scan_card(void *wcn_dev);
void sprd_pcie_register_scan_notify(void *func);
void sprd_pcie_remove_card(void *wcn_dev);
u32 sprd_pcie_read_reg32(struct wcn_pcie_info *priv, int offset);
void sprd_pcie_write_reg32(struct wcn_pcie_info *priv, u32 reg_offset,
			   u32 value);
int wcn_get_edma_status(void);
void wcn_set_tx_complete_status(int flag);
int wcn_get_tx_complete_status(void);
void wcn_dump_ep_regs(struct wcn_pcie_info *priv);

int sprd_pcie_init(void);
void sprd_pcie_exit(void);

#endif
