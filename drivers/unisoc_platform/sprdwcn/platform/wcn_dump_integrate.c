/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 /
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <misc/wcn_integrate_platform.h>
#include "bufring.h"
#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "mdbg_type.h"
#include "loopcheck.h"
#include "../include/wcn_dbg.h"
#include "gnss_dump.h"
#include "wcn_debug_bus.h"

/* SUB_NAME len not more than 15 bytes */
#define UMW2631_WCN_DUMP_VERSION_SUB_NAME "SIPC_26xx"
#define WCN_DUMP_VERSION_SUB_NAME "SIPC_23xx"

/* CP2 iram start and end */
#define WCN_DUMP_CP2_IRAM_START 1
#define WCN_DUMP_CP2_IRAM_END 2
/* AP regs start and end */
#define WCN_DUMP_AP_REGS_START (WCN_DUMP_CP2_IRAM_END + 1)
#define WCN_DUMP_AP_REGS_END 9
/* CP2 regs start and end */
#define WCN_DUMP_CP2_REGS_START (WCN_DUMP_AP_REGS_END + 1)

#define WCN_DUMP_END_STRING "marlin_memdump_finish"
#define WCN_CP2_STATUS_DUMP_REG	0x6a6b6c6d

#define DUMP_PACKET_SIZE	(1024)

/* units is ms, 2500ms */
#define WCN_DUMP_TIMEOUT 2500

/* use for umw2631_integrate only */
#define WCN_AON_ADDR_OFFSET 0x11000000
#define WCN_DUMP_ADDR_OFFSET 0x00000004

/* magic number, not change it */
#define WCN_DUMP_VERSION_NAME "WCN_DUMP_HEAD__"
#define WCN_DUMP_ALIGN(x) (((x) + 3) & ~3)
/* used for HEAD, so all dump mem in this array.
 * if new member added, please modify the macor XXX_START XXX_end above.
 */
struct wcn_dump_mem_reg {
	/* some CP regs can't dump */
	u32 addr;
	/* 4 btyes align */
	u32 len;
	u32 (*prerequisite)(void);
	const char *dump_name;
};

struct wcn_dump_section_info {
	/* cp load start addr */
	__le32 start;
	/* cp load end addr */
	__le32 end;
	/* load from file offset */
	__le32 off;
	__le32 reserv;
} __packed;

struct wcn_dump_head_info {
	/* WCN_DUMP_VERSION_NAME */
	u8 version[16];
	/* WCN_DUMP_VERSION_SUB_NAME */
	u8 sub_version[16];
	/* numbers of wcn_dump_section_info */
	__le32 n_sec;
	/* used to check if dump is full */
	__le32 file_size;
	u8 reserv[8];
	struct wcn_dump_section_info section[0];
} __packed;

static struct wcn_dump_mem_reg s_wcn_dump_regs[] = {
	/* share mem */
	{0, 0x300000, NULL, "share mem"},
	/* iram mem */
	{0x10000000, 0x8000, NULL, "wcn iram"},
	{0x18004000, 0x4000, NULL, "gnss iram"},
	/* ap regs */
	{0x402B00CC, 4, NULL, "PMU_SLEEP_CTRL"},
	{0x402B00D4, 4, NULL, "PMU_SLEEP_STATUS"},
	{0x402B0100, 4, NULL, "PMU_PD_WCN_SYS_CFG"},
	{0x402B0104, 4, NULL, "PMU_PD_WIFI_WRAP_CFG"},
	{0x402B0244, 4, NULL, "PMU_WCN_SYS_DSLP_ENA"},
	{0x402B0248, 4, NULL, "PMU_WIFI_WRAP_DSLP_ENA"},
	{0x402E057C, 4, NULL, "AON_APB_WCN_SYS_CFG2"},
	/* cp regs */
	{0x40060000, 0x300, NULL, "BTWF_CTRL"},
	{0x60300000, 0x400, NULL, "BTWF_AHB_CTRL"},
	{0x40010000, 0x38, NULL, "BTWF_INTC"},
	{0x40020000, 0x10, NULL, "BTWF_SYSTEM_TIMER"},
	{0x40030000, 0x20, NULL, "BTWF_TIMER0"},
	{0x40030020, 0x20, NULL, "BTWF_TIMER1"},
	{0x40030040, 0x20, NULL, "BTWF_TIMER2"},
	{0x40040000, 0x24, NULL, "BTWF_WATCHDOG"},
	{0xd0010000, 0xb4, NULL, "COM_AHB_CTRL"},
	{0xd0020800, 0xc, NULL, "MANU_CLK_CTRL"},
	{0x70000000, 0x10000, NULL, "WIFI"},
	{0x400b0000, 0x850, NULL, "FM"},
	{0x60700000, 0x400, NULL, "BT_CMD"},
	{0x60740000, 0xa400, NULL, "BT"}
};

static struct wcn_dump_mem_reg s_wcn_dump_regs_l6[] = {
	/* share mem */
	{0,
	 0x800000,
	 NULL,
	 "share mem",
	},

	/* iram mem */
	{0x40500000,
	 0x4000,
	 mdbg_check_wifi_ip_status,
	 "wcn iram 16k",
	},

	{0x40a50000,
	 0x8000,
	 mdbg_check_gnss_poweron,
	 "gnss iram 32k",
	},

	/* ap regs */
	{0x640203a8,
	 4,
	 NULL,
	 "PMU_PD_WCN_SYS_CFG",
	},

	{0x640203ac,
	 4,
	 NULL,
	 "PMU_PD_WIFI_WRAP_CFG",
	},

	{0x6402078c,
	 4,
	 NULL,
	 "PMU_WCN_SYS_DSLP_ENA",
	},

	{0x64020818,
	 4,
	 NULL,
	 "PMU_FORCE_DEEP_SLEEP_CFG0",
	},

	{0x64020860,
	 4,
	 NULL,
	 "PMU_SLEEP_STATUS",
	},

	{0x64000000,
	 4,
	 NULL,
	 "AON_APB_WCN_SYS_CFG2",
	},

	/* cp regs */
	/* top cp AON regs */
	{0x40880000,
	 0x138,
	 mdbg_check_wcn_sys_exit_sleep,
	 "AON_AHB",
	},

	{0x4080c000,
	 0x444,
	 mdbg_check_wcn_sys_exit_sleep,
	 "AON_APB",
	},

	{0x40800000,
	 0x400,
	 mdbg_check_wcn_sys_exit_sleep,
	 "AON_CLK",
	},

	/* BTWF sys regs */
	{0x40130000,
	 0x420,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_AHB",
	},

	{0x40088000,
	 0x2a8,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_APB",
	},

	{0x40000000,
	 0x8000,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_INTC",
	},
	{0x40018000,
	 0x8000,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_SYSTEM_TIMER",
	},

	{0x40020000,
	 0x8000,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_TIMER0",
	},

	{0x40050000,
	 0x8000,
	 mdbg_check_btwf_sys_exit_sleep,
	 "BTWF_TIMER1",
	},

	/* wifi */
	{0x400f0000,
	 0x608,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_AON",
	},

	{0x40300000,
	 0x18000,
	 mdbg_check_wifi_ip_status,
	 "WIFI_REG_MEM",
	},

	{0x40804000,
	 0xd0,
	 mdbg_check_wifi_ip_status,
	 "WIFI_CLK_SWITCH_CONTROL",
	},

	{0x400a0000,
	 0x54,
	 mdbg_check_wifi_ip_status,
	 "WIFI_GLB",
	},

	{0x400b7000,
	 0x774,
	 mdbg_check_wifi_ip_status,
	 "WIFI_DFE",
	},

	{0x400b2000,
	 0xa8c,
	 mdbg_check_wifi_ip_status,
	 "WIFI_PHY_RX11A",
	},

	{0x400b3000,
	 0xd4,
	 mdbg_check_wifi_ip_status,
	 "WIFI_PHY_11B",
	},

	{0x400b0000,
	 0x808,
	 mdbg_check_wifi_ip_status,
	 "WIFI_PHY_TOP",
	},

	{0x400b1000,
	 0x154,
	 mdbg_check_wifi_ip_status,
	 "WIFI_PHY_TX_11A",
	},

	{0x400b4000,
	 0xa70,
	 mdbg_check_wifi_ip_status,
	 "WIFI_PHY_RFIF",
	},

	{0x400f6000,
	 0xa4,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_CE",
	},

	{0x400fe000,
	 0x80,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_DC_PD",
	},

	{0x400fc000,
	 0x910,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_MH_PD",
	},

	{0x400f8000,
	 0x2d54,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_PA_PD",
	},

	{0x400f1004,
	 0x3718,
	 mdbg_check_wifi_ip_status,
	 "WIFI_MAC_RTN",
	},

	{0x400d3000,
	 0x48,
	 mdbg_check_wifi_ip_status,
	 "WIFI_SPI_MASTER_RF",
	},

	/* bt/fm */
	{0x40240000,
	 0x310,
	 mdbg_check_bt_poweron,
	 "BT_ACC_CONTROL",
	},

	{0x40243000,
	 0x7d0,
	 mdbg_check_bt_poweron,
	 "BT_AON_CONTROL",
	},

	{0x40244000,
	 0xf4,
	 mdbg_check_bt_poweron,
	 "BT_TIM_EXT",
	},

	{0x40246000,
	 0x8fff,
	 mdbg_check_bt_poweron,
	 "BT_BASE",
	},

	/* dump reg of merlion if merlion accessible:to be added */
};

static struct wcn_dump_mem_reg *get_wcn_dump_mem_area(size_t *array_size)
{
	struct wcn_dump_mem_reg *reg_area = NULL;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		reg_area = s_wcn_dump_regs_l6;
		*array_size = ARRAY_SIZE(s_wcn_dump_regs_l6);
	} else {
		reg_area = s_wcn_dump_regs;
		*array_size = ARRAY_SIZE(s_wcn_dump_regs);
	}

	return reg_area;
}

static struct mdbg_ring_t	*mdev_ring;
gnss_dump_callback gnss_dump_handle;

static int wcn_fill_dump_head_info(struct wcn_dump_mem_reg *mem_cfg, size_t cnt)
{
	unsigned int i, len, head_len;
	struct wcn_dump_mem_reg *mem;
	struct wcn_dump_head_info *head;
	struct wcn_dump_section_info *sec;

	head_len = sizeof(*head) + sizeof(*sec) * cnt;
	head = kzalloc(head_len, GFP_KERNEL);
	if (unlikely(!head)) {
		WCN_ERR("system has no mem for dump mem\n");
		return -1;
	}

	strncpy(head->version, WCN_DUMP_VERSION_NAME,
		strlen(WCN_DUMP_VERSION_NAME) + 1);
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		strncpy(head->sub_version, UMW2631_WCN_DUMP_VERSION_SUB_NAME,
			strlen(UMW2631_WCN_DUMP_VERSION_SUB_NAME) + 1);
	} else {
		strncpy(head->sub_version, WCN_DUMP_VERSION_SUB_NAME,
			strlen(WCN_DUMP_VERSION_SUB_NAME) + 1);
	}

	head->n_sec = cpu_to_le32(cnt);
	len = head_len;
	for (i = 0; i < cnt; i++) {
		sec = head->section + i;
		mem = mem_cfg + i;
		sec->off = cpu_to_le32(WCN_DUMP_ALIGN(len));
		sec->start = cpu_to_le32(mem->addr);
		sec->end = cpu_to_le32(sec->start + mem->len - 1);
		len += mem->len;
		WCN_INFO("section[%d] [0x%x 0x%x 0x%x]\n",
			 i, le32_to_cpu(sec->start),
			 le32_to_cpu(sec->end), le32_to_cpu(sec->off));
	}
	head->file_size = cpu_to_le32(len + strlen(WCN_DUMP_END_STRING));

	mdbg_ring_write(mdev_ring, head, head_len);
	wake_up_log_wait();
	kfree(head);

	return 0;
}

static void mdbg_dump_str(char *str, int str_len)
{
	if (!str)
		return;

	mdbg_ring_write_timeout(mdev_ring, str, str_len, WCN_DUMP_TIMEOUT);
	wake_up_log_wait();
	WCN_INFO("dump str finish!");
}

static int mdbg_dump_ap_register_data(phys_addr_t addr, u32 len)
{
	u32 value = 0;
	u8 *ptr = NULL;

	ptr = (u8 *)&value;
	wcn_read_data_from_phy_addr(addr, &value, len);
	mdbg_ring_write_timeout(mdev_ring, ptr, len, WCN_DUMP_TIMEOUT);
	wake_up_log_wait();

	return 0;
}

static int mdbg_dump_cp_register_data(u32 addr, u32 len)
{
	struct regmap *regmap;
	u32 i;
	u32 count, trans_size;
	u32 cp_access_type;
	phys_addr_t phy_addr;
	u8 *buf = NULL;
	u8 *ptr = NULL;

	WCN_INFO("start dump cp register!addr:%x,len:%d", addr, len);
	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;
	if (cp_access_type) {
		/* direct map */
		phy_addr =  addr + WCN_AON_ADDR_OFFSET;
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_read_data_from_phy_addr(phy_addr, ptr, 4);
			phy_addr = phy_addr + WCN_DUMP_ADDR_OFFSET;
		}
	} else {
		/* aon funcdma tlb way */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
			regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
		else
			regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);

		wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
		}
	}
	count = 0;
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		mdbg_ring_write_timeout(mdev_ring, buf + count,
					trans_size, WCN_DUMP_TIMEOUT);
		count += trans_size;
		wake_up_log_wait();
	}

	kfree(buf);
	WCN_INFO("dump cp register finish count %u\n", count);

	return count;
}

static void mdbg_dump_iram(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	int count;

	for (i = WCN_DUMP_CP2_IRAM_START; i <= WCN_DUMP_CP2_IRAM_END; i++) {
		WCN_INFO("dump iram reg: %s!\n", mem[i].dump_name);
		if ((mem[i].prerequisite == NULL) || mem[i].prerequisite()) {
			count = mdbg_dump_cp_register_data(mem[i].addr, mem[i].len);
			WCN_INFO("dump iram section[%d] %d ok!\n", i, count);
		}
	}
}
static void mdbg_dump_ap_register(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	u32 wcn_dump_ap_regs_end;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		wcn_dump_ap_regs_end = UMW2631_WCN_DUMP_AP_REGS_END;
	else
		wcn_dump_ap_regs_end = WCN_DUMP_AP_REGS_END;
	for (i = WCN_DUMP_AP_REGS_START; i <= wcn_dump_ap_regs_end; i++) {
		WCN_INFO("dump ap reg: %s!\n", mem[i].dump_name);
		if ((mem[i].prerequisite == NULL) || mem[i].prerequisite()) {
			mdbg_dump_ap_register_data(mem[i].addr, mem[i].len);
			WCN_INFO("dump ap reg section[%d] ok!\n", i);
		}
	}
}

static void mdbg_dump_cp_register(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	int count;
	size_t array_size;
	u32 wcn_dump_cp2_regs_start;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		wcn_dump_cp2_regs_start = UMW2631_WCN_DUMP_CP2_REGS_START;
	else
		wcn_dump_cp2_regs_start = WCN_DUMP_CP2_REGS_START;
	get_wcn_dump_mem_area(&array_size);
	for (i = wcn_dump_cp2_regs_start; i <= array_size - 1; i++) {
		WCN_INFO("dump cp reg: %s!\n", mem[i].dump_name);
		if ((mem[i].prerequisite == NULL) || mem[i].prerequisite()) {
			count = mdbg_dump_cp_register_data(mem[i].addr, mem[i].len);
			WCN_INFO("dump cp reg section[%d] %d ok!\n", i, count);
		}
	}
}

static int mdbg_dump_share_memory(struct wcn_dump_mem_reg *mem)
{
	u32 count, len, trans_size;
	void *virt_addr;
	phys_addr_t base_addr;
	unsigned int cnt;
	unsigned long timeout;

	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}
	len = mem[0].len;
	base_addr = wcn_get_btwf_base_addr();
	WCN_INFO("dump sharememory start!");
	WCN_INFO("ring->pbuff=%p, ring->end=%p.\n",
		 mdev_ring->pbuff, mdev_ring->end);
	virt_addr = wcn_mem_ram_vmap_nocache(base_addr, len, &cnt);
	if (!virt_addr) {
		WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
		return -1;
	}
	count = 0;
	/* 20s timeout */
	timeout = jiffies + msecs_to_jiffies(20000);
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		/* copy data from ddr to ring buf  */

		mdbg_ring_write_timeout(mdev_ring, virt_addr + count,
					trans_size, WCN_DUMP_TIMEOUT);
		count += trans_size;
		wake_up_log_wait();
		if (time_after(jiffies, timeout)) {
			WCN_ERR("Dump share mem timeout:count:%u\n", count);
			return -1;
		}
	}
	wcn_mem_ram_unmap(virt_addr, cnt);
	WCN_INFO("share memory dump finish! total count %u\n", count);

	return 0;
}

void mdbg_dump_gnss_register(gnss_dump_callback callback_func, void *para)
{
	gnss_dump_handle = (gnss_dump_callback)callback_func;
	WCN_INFO("gnss_dump register success!\n");
}

void mdbg_dump_gnss_unregister(void)
{
	gnss_dump_handle = NULL;
}
EXPORT_SYMBOL_GPL(mdbg_dump_gnss_unregister);

u32 mdbg_check_wcn_sys_exit_sleep(void)
{
	u32 need_dump_status = 0;
	u32 wcn_sys_domain_power = ((0x6<<28));
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0860, &need_dump_status);
	WCN_INFO("REG 0x64020860:val=0x%x!\n", need_dump_status);

	return (need_dump_status & wcn_sys_domain_power);

}

u32 mdbg_check_btwf_sys_exit_sleep(void)
{
	u32 need_dump_status = 0;
	u32 btwf_sys_domain_power = ((0x6<<7));
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			 0x0364, &need_dump_status);
	WCN_INFO("REG 0x64000364:val=0x%x!\n", need_dump_status);

	return (need_dump_status & btwf_sys_domain_power);
}

u32 mdbg_check_wifi_ip_status(void)
{
	u32 need_dump_status = 0;
	u32 btwf_wrap_mac_mask = (0x2);
	u32 btwf_wrap_phy_mask = (0x4);
	u32 btwf_arm_domain_power = (0x1);
	u32 btwf_arm_wrap_mask = (0x8);
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					0x03b0, &need_dump_status);
	WCN_INFO("%s:0x03b0=0x%x\n", __func__, need_dump_status);

	return ((need_dump_status & btwf_wrap_mac_mask) &&
			(need_dump_status & btwf_wrap_phy_mask) &&
			(need_dump_status & btwf_arm_wrap_mask) &&
			(need_dump_status & btwf_arm_domain_power));
}

u32 mdbg_check_bt_poweron(void)
{
	u32 need_dump_status = 0;
	u32 btwf_bt_domain_power = (0x1<<4);
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					0x03b0, &need_dump_status);
	WCN_INFO("%s:0x03b0=0x%x\n", __func__, need_dump_status);

	return (need_dump_status & btwf_bt_domain_power);
}

u32 mdbg_check_gnss_poweron(void)
{
	u32 need_dump_status = 0;
	u32 gnss_ss_poweron_mask = (0x4<<4);
	u32 gnss_poweron_mask = (0x2<<4);
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.gnss_device;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					0x03b0, &need_dump_status);
	WCN_INFO("%s:0x03b0=0x%x\n", __func__, need_dump_status);

	return ((need_dump_status & gnss_poweron_mask) &&
			(need_dump_status & gnss_ss_poweron_mask));

}

static int btwf_dump_mem(enum wcn_source_type type)
{
	u32 cp2_status = 0;
	phys_addr_t sleep_addr;
	size_t dump_array_size;
	struct wcn_dump_mem_reg *p_wcn_dump_regs = NULL;

	if (wcn_get_btwf_power_status() == WCN_POWER_STATUS_OFF) {
		WCN_INFO("wcn power status off:can not dump btwf!\n");
		return -1;
	}

	p_wcn_dump_regs = get_wcn_dump_mem_area(&dump_array_size);
	mdbg_send_atcmd("at+sleep_switch=0\r",
			strlen("at+sleep_switch=0\r"),
			WCN_ATCMD_KERNEL);
	msleep(500);
	sleep_addr = wcn_get_btwf_sleep_addr();
	wcn_read_data_from_phy_addr(sleep_addr, &cp2_status, sizeof(u32));
	mdev_ring = mdbg_dev->ring_dev->ring;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (btwf_sys_force_exit_deep_sleep(s_wcn_device.btwf_device)) {
			WCN_INFO("Dump need prerequisite!\n");
			return 0;
		}
	}

	if (type == WCN_SOURCE_GNSS && wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3) {
		mdbg_cpu_reset();//only soft reset not reset release
		return 0;
	}

	mdbg_hold_cpu();
	msleep(100);
	mdbg_ring_reset(mdev_ring);
	mdbg_atcmd_clean();
	if (wcn_fill_dump_head_info(p_wcn_dump_regs, dump_array_size))
		return -1;
	if (mdbg_dump_share_memory(p_wcn_dump_regs)) {
		WCN_INFO("Dump ringbuf is full!\n");
		return 0;
	}

	/* The precondition provided by the current access register is
	 incorrect and cannot cover the full scene dump access.
	 So return.
	 */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		WCN_INFO("dump register ok!\n");
		mdbg_dump_str(WCN_DUMP_END_STRING,
					  strlen(WCN_DUMP_END_STRING));
		return 0;
	}

	mdbg_dump_iram(p_wcn_dump_regs);
	mdbg_dump_ap_register(p_wcn_dump_regs);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		mdbg_dump_cp_register(p_wcn_dump_regs);
	else if (cp2_status == WCN_CP2_STATUS_DUMP_REG)
		mdbg_dump_cp_register(p_wcn_dump_regs);

	WCN_INFO("dump register ok!\n");
	mdbg_dump_str(WCN_DUMP_END_STRING, strlen(WCN_DUMP_END_STRING));

	return 0;
}

/*
 * 9863a dump,if gnss and btwf both hold cpu,the dump will very slowly.
 * this time kernel log will has scheedule_timeout
 * now dump only dump the source type
 *
 */
void mdbg_dump_mem_integ(enum wcn_source_type type)
{
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3) {
		if (type == WCN_SOURCE_GNSS) {
			/* dump gnss */
			gnss_dump_mem(0);
			/* dump btwf */
			btwf_dump_mem(type);//only send sleep ,not dump btwf
		} else {
			/* dump btwf */
			btwf_dump_mem(type);
		}
	} else {
		 /* dump gnss */
		gnss_dump_mem(0);
		/* dump btwf */
		btwf_dump_mem(type);
	}
}

int dump_arm_reg_integ(void)
{
	mdbg_hold_cpu();

	return 0;
}

static int mdbg_snap_shoot_iram_data(void *buf, u32 addr, u32 len)
{
	struct regmap *regmap;
	u32 i;
	u32 cp_access_type;
	phys_addr_t phy_addr;
	u8 *ptr = NULL;

	WCN_INFO("start snap_shoot iram data!addr:%x,len:%d", addr, len);
	if (marlin_get_module_status() == 0) {
		WCN_ERR("module status off:can not get iram data!\n");
		return -1;
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;
	if (cp_access_type) {
		/* direct map */
		phy_addr =  addr + WCN_AON_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, ptr, len);
	} else {
		/* aon funcdma tlb way */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
			regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
		else
			regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);
		wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
		}
	}
	WCN_INFO("snap_shoot iram data success\n");

	return 0;
}

int mdbg_snap_shoot_iram(void *buf)
{
	u32 ret;

	ret = mdbg_snap_shoot_iram_data(buf,
			0x18000000, 1024 * 32);

	return ret;
}
