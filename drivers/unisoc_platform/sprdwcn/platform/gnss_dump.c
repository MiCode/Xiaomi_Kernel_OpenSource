/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#include "gnss.h"
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>
#include <misc/wcn_integrate_platform.h>
#include <linux/uaccess.h>
#include "bufring.h"
#include "wcn_log.h"
#include "wcn_dump.h"
#include "gnss_common.h"
#include "gnss_dump.h"
#include "wcn_gnss_dump.h"
#include "sprd_wcn.h"

enum {
	REGMAP_AON_APB = 0x0,	/* AON APB */
	REGMAP_PMU_APB,
	/*
	 * NOTES:SharkLE use it,but PIKE2 not.
	 * We should config the DTS for PIKE2 also.
	 */
	REGMAP_PUB_APB, /* SharkLE only:for ddr offset */
	REGMAP_ANLG_WRAP_WCN,
	REGMAP_ANLG_PHY_G5, /* SharkL3 only */
	REGMAP_ANLG_PHY_G6, /* SharkLE only */
	REGMAP_WCN_REG,	/* SharkL3 only:0x403A 0000 */
	REGMAP_TYPE_NR,
};

#define GNSS_DUMP_END_STRING "gnss_memdump_finish"
#define GNSS_SET_OFFSET                 0x1000
#define GNSS_APB_BASE              0x40bc8000
#define REG_GNSS_APB_MCU_AP_RST        (GNSS_APB_BASE + 0x0280) /* s/c */
#define BIT_GNSS_APB_MCU_AP_RST_SOFT (1<<0)/*BIT 0*/

#define GNSS_AHB_BASE			   0x40b18000
#define GNSS_ARCH_EB_REG		   (GNSS_AHB_BASE + 0x084)
#define GNSS_ARCH_EB_REG_BYPASS    (1<<1)

#define GNSSDUMP_INFO(format, arg...) pr_info("gnss_dump: " format, ## arg)
#define GNSSDUMP_ERR(format, arg...) pr_err("gnss_dump: " format, ## arg)

#define GNSS_DUMP_ADDR_OFFSET 0x00000004

/* Get followling value from dts now! */

uint GNSS_CP_START_ADDR;
uint GNSS_FIRMWARE_MAX_SIZE;
uint GNSS_DUMP_PACKET_SIZE;
uint GNSS_SHARE_MEMORY_SIZE;
uint GNSS_DUMP_IRAM_START_ADDR;
uint GNSS_CP_IRAM_DATA_NUM;
uint GNSS_DUMP_REG_NUMBER;

struct gnss_mem_dump {
	uint address;
	uint length;
};

/* dump cp firmware firstly, wait for next adding */
static struct gnss_mem_dump gnss_marlin3_dump[] = {
	{0, 0}, /* gnss firmware code */
	{GNSS_DRAM_ADDR, GNSS_DRAM_SIZE}, /* gnss dram */
	{GNSS_TE_MEM, GNSS_TE_MEM_SIZE}, /* gnss te mem */
	{GNSS_BASE_AON_APB, GNSS_BASE_AON_APB_SIZE}, /* aon apb */
	{CTL_BASE_AON_CLOCK, CTL_BASE_AON_CLOCK_SIZE}, /* aon clock */
};

struct regmap_dump {
	int regmap_type;
	uint reg;
};

struct cp_reg_dump {
	u32 addr;
	u32 len;
};

/* for sharkle ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_sharkle_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_AON_APB, 0x057c}, /* REG_AON_APB_WCN_SYS_CFG2 */
	{REGMAP_AON_APB, 0x0578}, /* REG_AON_APB_WCN_SYS_CFG1 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_SHARKLE_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0100}, /* REG_PMU_APB_PD_WCN_SYS_CFG */
	{REGMAP_PMU_APB, 0x0104}, /* REG_PMU_APB_PD_WIFI_WRAP_CFG */
	{REGMAP_PMU_APB, 0x0108}, /* REG_PMU_APB_PD_GNSS_WRAP_CFG */
};

/* for pike2 ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_pike2_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_PMU_APB, 0x0338}, /* REG_PMU_APB_WCN_SYS_CFG_STATUS */
	{REGMAP_AON_APB, 0x00d8}, /* REG_AON_APB_WCN_CONFIG0 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_PIKE2_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0050}, /* REG_PMU_APB_PD_WCN_TOP_CFG */
	{REGMAP_PMU_APB, 0x0054}, /* REG_PMU_APB_PD_WCN_WIFI_CFG */
	{REGMAP_PMU_APB, 0x0058}, /* REG_PMU_APB_PD_WCN_GNSS_CFG */
};

/* for sharkl3 ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_sharkl3_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_AON_APB, 0x057c}, /* REG_AON_APB_WCN_SYS_CFG2 */
	{REGMAP_AON_APB, 0x0578}, /* REG_AON_APB_WCN_SYS_CFG1 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_SHARKLE_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0100}, /* REG_PMU_APB_PD_WCN_SYS_CFG */
	{REGMAP_PMU_APB, 0x0104}, /* REG_PMU_APB_PD_WIFI_WRAP_CFG */
	{REGMAP_PMU_APB, 0x0108}, /* REG_PMU_APB_PD_GNSS_WRAP_CFG */
};

/* for sharkl6 cp reg dump */
static struct cp_reg_dump cp_reg[] = {
	{REG_AON_WCN_GNSS_CLK, AON_WCN_GNSS_CLK_LEN},
	{REG_AON_APB_PERI, AON_APB_PERI_LEN},
	{REG_AON_AHB_SYS, AON_AHB_SYS_LEN},
	{REG_AON_CONTROL, AON_CONTROL_LEN},
};

static char gnss_dump_level; /* 0: default, all, 1: only data, pmu, aon */

static u32 cgm_gnss_clk_gate_en = 1;
void gnss_set_clk_gate_en(u32 flag)
{
	cgm_gnss_clk_gate_en = flag;
}

static void gnss_write_data_to_phy_addr(phys_addr_t phy_addr,
					      void *src_data, u32 size)
{
	void *virt_addr;

	GNSSDUMP_ERR("gnss_write_data_to_phy_addr entry\n");
	virt_addr = shmem_ram_vmap_nocache(SIPC_ID_GNSS, phy_addr, size);
	if (virt_addr) {
		memcpy(virt_addr, src_data, size);
		shmem_ram_unmap(SIPC_ID_GNSS, virt_addr);
	} else
		GNSSDUMP_ERR("%s shmem_ram_vmap_nocache fail\n", __func__);
}

static void gnss_read_data_from_phy_addr(phys_addr_t phy_addr,
					  void *tar_data, u32 size)
{
	void *virt_addr;

	GNSSDUMP_ERR("gnss_read_data_from_phy_addr\n");
	virt_addr = shmem_ram_vmap_nocache(SIPC_ID_GNSS, phy_addr, size);
	if (virt_addr) {
		memcpy(tar_data, virt_addr, size);
		shmem_ram_unmap(SIPC_ID_GNSS, virt_addr);
	} else
		GNSSDUMP_ERR("%s shmem_ram_vmap_nocache fail\n", __func__);
}

static void gnss_soft_reset_release_cpu(u32 type)
{
	struct regmap *regmap;
	phys_addr_t base_addr;
	u32 value = 0;
	u32 value_tmp = 0;
	u32 platform_type = wcn_platform_chip_type();
	if (platform_type == WCN_PLATFORM_TYPE_QOGIRL6) {
		base_addr = MCU_AP_RST_ADDR + GNSS_AP_ACCESS_CP_OFFSET;
		if (type == GNSS_CPU_RESET) {
			/* reset gnss cpu */
			gnss_read_data_from_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("before rst val=%x\n", value_tmp);
			/* BIT0:rst set */
			value_tmp |= 1;
			gnss_write_data_to_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("rst val=%x\n", value_tmp);
		} else if (type == GNSS_CPU_RESET_RELEASE) {
			/* release gnss cpu */
			gnss_read_data_from_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("before rls val=%x\n", value_tmp);
			/* BIT0:rst clear */
			value_tmp &= ~(1);
			gnss_write_data_to_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("rls val=%x\n", value_tmp);
		}
		return;
	}

	if (platform_type == WCN_PLATFORM_TYPE_SHARKL3) {
		GNSSDUMP_INFO("regmap SHARKL3");
		regmap = wcn_get_gnss_regmap(REGMAP_WCN_REG);
	} else {
		GNSSDUMP_INFO("regmap non SHARKL3");
		regmap = wcn_get_gnss_regmap(REGMAP_ANLG_WRAP_WCN);
	}
	if (type == GNSS_CPU_RESET) {
		/* reset gnss cm4 */
		wcn_regmap_read(regmap, 0X20, &value);
		value |= 1 << 2;
		wcn_regmap_raw_write_bit(regmap, 0X20, value);

		wcn_regmap_read(regmap, 0X24, &value);
		value |= 1 << 3;
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
	} else if (type == GNSS_CPU_RESET_RELEASE) {
		value = 0;
		/* release gnss cm4 */
		wcn_regmap_raw_write_bit(regmap, 0X20, value);
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
	}
}

static void gnss_hold_cpu(void)
{
	u32 value;
	phys_addr_t base_addr;
	phys_addr_t ph_addr;
	int i = 0;
	GNSSDUMP_INFO("%s enter\n", __func__);
	/* reset cpu */
	gnss_soft_reset_release_cpu(GNSS_CPU_RESET);
	/* set cache flag */
	value = GNSS_CACHE_FLAG_VALUE;
	base_addr = wcn_get_gnss_base_addr();
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		ph_addr = base_addr + GNSS_CACHE_FLAG_ADDR_L6;
	else
		ph_addr = base_addr + GNSS_CACHE_FLAG_ADDR;
	GNSSDUMP_INFO("val=%x bs=%llx ph=%llx\n", value, base_addr, ph_addr);
	gnss_write_data_to_phy_addr(ph_addr, (void *)&value, 4);
	/* release cpu */
	gnss_soft_reset_release_cpu(GNSS_CPU_RESET_RELEASE);

	while (i < 3) {
		gnss_read_data_from_phy_addr(ph_addr, (void *)&value, 4);
		GNSSDUMP_INFO("%s gnss cache value %x\n", __func__, value);
		if (value == GNSS_CACHE_END_VALUE)
			break;
		i++;
		msleep(50);
	}
	if (value != GNSS_CACHE_END_VALUE)
		GNSSDUMP_ERR("%s gnss cache failed value %x\n", __func__,
			value);
	msleep(200);
}

static int gnss_dump_cp_register_data(u32 addr, u32 len)
{
	struct regmap *map;
	int count;
	u32 i;
	u32 cp_access_type;
	u32 chip_tp = wcn_platform_chip_type();
	phys_addr_t phy_addr;
	u8 *buf = NULL;
	u8 *pt  = NULL;
	void  *iram_buffer = NULL;
	//mm_segment_t fs;

	GNSSDUMP_INFO(" start dump cp register!addr:%x,len:%d\n", addr, len);
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		GNSSDUMP_ERR("%s kzalloc buf error\n", __func__);
		return -ENOMEM;
	}

	iram_buffer = vmalloc(len);
	if (!iram_buffer) {
		GNSSDUMP_ERR("%s vmalloc iram_buffer error\n", __func__);
		kfree(buf);
		return -ENOMEM;
	}
	memset(iram_buffer, 0, len);

	if (chip_tp == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;

	if (gnss_dump_level == 0) {
		if (cp_access_type) {
			/* direct map */
			phy_addr =  addr + GNSS_AP_ACCESS_CP_OFFSET;
			for (i = 0; i < len / 4; i++) {
				pt = (u8 *) (buf + i * 4);
				wcn_read_data_from_phy_addr(phy_addr, pt, 4);
				phy_addr = phy_addr + GNSS_DUMP_ADDR_OFFSET;
			}
		} else {
			/* aon funcdma tlb way */
			if (chip_tp == WCN_PLATFORM_TYPE_SHARKL3)
				map = wcn_get_gnss_regmap(REGMAP_WCN_REG);
			else
				map = wcn_get_gnss_regmap(REGMAP_ANLG_WRAP_WCN);
			wcn_regmap_raw_write_bit(map,
						 ANLG_WCN_WRITE_ADDR, addr);
			for (i = 0; i < len / 4; i++) {
				pt = (u8 *) (buf + i * 4);
				wcn_regmap_read(map,
						ANLG_WCN_READ_ADDR, (u32 *)pt);
			}
		}
		memcpy(iram_buffer, buf, len);
	}
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	count = gnss_dump_data(iram_buffer, len);
	kfree(buf);
	vfree(iram_buffer);
	//set_fs(fs);
	//msleep(40);
	if (count != len) {
		GNSSDUMP_ERR("gnss_dump_cp_register_data failed size is %d\n",
					count);
		return -1;
	}
	GNSSDUMP_INFO("gnss_dump_cp_register_data %d ok!\n", count);
	return count;
}

static int gnss_dump_ap_register(void)
{
	struct regmap *regmap;
	u32 value[16] = {0}; /* [0]board+ [..]reg */
	u32 i = 0;
	//mm_segment_t fs;
	u32 len = 0;
	u8 *ptr = NULL;
	int count;
	void  *apreg_buffer = NULL;
	struct regmap_dump *gnss_ap_reg = NULL;

	GNSSDUMP_INFO("%s ap reg data\n", __func__);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE) {
		gnss_ap_reg = gnss_sharkle_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2) {
		gnss_ap_reg = gnss_pike2_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3) {
		gnss_ap_reg = gnss_sharkl3_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		GNSSDUMP_INFO("SHARKL6 dont dump ap reg!\n");
		return 0;
	} else
		return 0;

	apreg_buffer = vmalloc(len);
	if (!apreg_buffer)
		return -2;

	ptr = (u8 *)&value[0];
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE)
		value[0] = 0xF1;
	else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2)
		value[0] = 0xF2;
	else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
		value[0] = 0xF3;
	else
		value[0] = 0xF4;

	for (i = 0; i < GNSS_DUMP_REG_NUMBER; i++) {
		if ((gnss_ap_reg + i)->regmap_type == REGMAP_TYPE_NR) {
			gnss_read_data_from_phy_addr((gnss_ap_reg + i)->reg,
				&value[i+1], 4);
			GNSSDUMP_INFO("%s ap reg[0x%x] data[0x%x]\n", __func__,
				      (gnss_ap_reg + i)->reg, value[i+1]);
		} else {
			regmap =
			wcn_get_gnss_regmap((gnss_ap_reg + i)->regmap_type);
			wcn_regmap_read(regmap, (gnss_ap_reg + i)->reg,
				&value[i+1]);
		}
	}
	memset(apreg_buffer, 0, len);
	memcpy(apreg_buffer, ptr, len);
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	count = gnss_dump_data(apreg_buffer, len);
	vfree(apreg_buffer);
	//set_fs(fs);
	if (count != len) {
		GNSSDUMP_ERR("gnss_dump_ap_register_data failed size is %d\n",
					count);
		return -1;
	}
	GNSSDUMP_INFO("gnss_dump_ap_register_data %d ok!\n", count);
	return 0;
}

static void gnss_dump_cp_register(void)
{
	u32 count;
	int i;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		for (i = 0; i < CP_REG_NUM; i++) {
			count = gnss_dump_cp_register_data(cp_reg[i].addr,
					 cp_reg[i].len);
			GNSSDUMP_INFO("dump cp_reg%d %u\n", i, count);
		}
		return;
	}
	count = gnss_dump_cp_register_data(DUMP_REG_GNSS_APB_CTRL_ADDR,
			DUMP_REG_GNSS_APB_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump gnss_apb_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_REG_GNSS_AHB_CTRL_ADDR,
			DUMP_REG_GNSS_AHB_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump manu_clk_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_COM_SYS_CTRL_ADDR,
			DUMP_COM_SYS_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump com_sys_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_WCN_CP_CLK_CORE_ADDR,
			DUMP_WCN_CP_CLK_LEN);
	GNSSDUMP_INFO("gnss dump manu_clk_ctrl_reg %u ok!\n", count);
}

static void gnss_dump_register(void)
{
	GNSSDUMP_INFO("%s enter\n", __func__);
	gnss_dump_ap_register();
	gnss_dump_cp_register();
	GNSSDUMP_INFO("gnss dump register ok!\n");
}

static void gnss_dump_iram(void)
{
	u32 count;
	u32 count_pchannel;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	GNSSDUMP_INFO("%s enter\n", __func__);
	count = gnss_dump_cp_register_data(GNSS_DUMP_IRAM_START_ADDR,
			GNSS_CP_IRAM_DATA_NUM * 4);
	GNSSDUMP_INFO("gnss dump iram %u ok!\n", count);

	if (g_match_config && g_match_config->unisoc_wcn_l3)
		goto end;

	count_pchannel = gnss_dump_cp_register_data(
			GNSS_DUMP_IRAM_START_ADDR_PCHANNEL,
			GNSS_PCHANNEL_IRAM_DATA_NUM * 4);
	GNSSDUMP_INFO("gnss pchannel dump iram %u ok!\n", count_pchannel);
end:
	GNSSDUMP_INFO("%s finish\n", __func__);
}

static int gnss_dump_share_memory(u32 len)
{
	void *virt_addr;
	phys_addr_t base_addr;
	long int count = 0;
	//mm_segment_t fs;
	void  *ddr_buffer = NULL;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	GNSSDUMP_INFO("%s enter\n", __func__);
	if (len == 0)
		return -1;

	base_addr = wcn_get_gnss_base_addr();
	GNSSDUMP_ERR(" %s base_addr is 0x%llx\n", __func__, base_addr);
	virt_addr = shmem_ram_vmap_nocache(SIPC_ID_GNSS, base_addr, len);
	if (!virt_addr) {
		GNSSDUMP_ERR(" %s shmem_ram_vmap_nocache fail\n", __func__);
		return -1;
	}

	ddr_buffer = vmalloc(len);
	if (!ddr_buffer) {
		GNSSDUMP_ERR(" %s vmalloc ddr_buffer fail\n", __func__);
		return -1;
	}
	memset(ddr_buffer, 0, len);
	memcpy(ddr_buffer, virt_addr, len);
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	count = gnss_dump_data(ddr_buffer, len);

	shmem_ram_unmap(SIPC_ID_GNSS, virt_addr);
	//set_fs(fs);
	vfree(ddr_buffer);
	if (count != len) {
		GNSSDUMP_ERR("%s dump ddr error,data len is %ld\n", __func__,
			count);
		return -1;
	}
	GNSSDUMP_INFO("gnss dump share memory  size = %ld\n", count);

	if (g_match_config && g_match_config->unisoc_wcn_l3)
		return 0;

	GNSSDUMP_ERR("%s dump sipc buffer start ", __func__);
	base_addr = GNSS_DUMP_IRAM_START_ADDR_SIPC;
	virt_addr = shmem_ram_vmap_nocache(SIPC_ID_GNSS, base_addr, SIPC_BUFFER_DATA_NUM);
	if (!virt_addr) {
		GNSSDUMP_ERR(" %s shmem for sipc buffer is fail\n", __func__);
		return -1;
	}

	ddr_buffer = vmalloc(SIPC_BUFFER_DATA_NUM);
	if (!ddr_buffer) {
		GNSSDUMP_ERR(" %s vmalloc ddr_buffer fail\n", __func__);
		return -1;
	}
	memset(ddr_buffer, 0, SIPC_BUFFER_DATA_NUM);
	memcpy(ddr_buffer, virt_addr, SIPC_BUFFER_DATA_NUM);
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	count = gnss_dump_data(ddr_buffer, SIPC_BUFFER_DATA_NUM);

	shmem_ram_unmap(SIPC_ID_GNSS, virt_addr);
	//set_fs(fs);
	vfree(ddr_buffer);
	if (count != SIPC_BUFFER_DATA_NUM) {
		GNSSDUMP_ERR("%s dump ddr error,sipc data is %ld\n", __func__,
			     count);
		return -1;
	}

	GNSSDUMP_INFO("gnss dump sipc buffer size = %ld\n", count);
	GNSSDUMP_INFO("%s finish\n", __func__);

	return 0;
}

static int gnss_integrated_dump_mem(void)
{
	int ret = 0;

	GNSSDUMP_INFO("gnss_dump_mem entry\n");

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6
		|| wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
		gnss_hold_cpu();
	ret = gnss_dump_share_memory(GNSS_SHARE_MEMORY_SIZE);
	gnss_dump_iram();
	gnss_dump_register();
	GNSSDUMP_INFO("%s finish\n", __func__);
	return ret;
}

static int gnss_ext_hold_cpu(void)
{
	uint temp = 0;
	int ret = 0;

	GNSSDUMP_INFO("%s entry\n", __func__);
	temp = BIT_GNSS_APB_MCU_AP_RST_SOFT;
	ret = sprdwcn_bus_reg_write(REG_GNSS_APB_MCU_AP_RST + GNSS_SET_OFFSET,
		&temp, 4);
	if (ret < 0) {
		GNSSDUMP_ERR("%s write reset reg error:%d\n", __func__, ret);
		return ret;
	}
	temp = GNSS_ARCH_EB_REG_BYPASS;
	ret = sprdwcn_bus_reg_write(GNSS_ARCH_EB_REG + GNSS_SET_OFFSET,
				    &temp, 4);
	if (ret < 0)
		GNSSDUMP_ERR("%s write bypass reg error:%d\n", __func__, ret);

	return ret;
}

static int gnss_ext_dump_data(unsigned int start_addr, int len)
{
	u8 *buf = NULL;
	int ret = 0;
	//, count = 0, trans = 0;
	void  *iram_buffer = NULL;
	//mm_segment_t fs;

	GNSSDUMP_INFO("%s, addr:%x,len:%d\n", __func__, start_addr, len);
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		GNSSDUMP_ERR("%s kzalloc buf error\n", __func__);
		return -ENOMEM;
	}

	iram_buffer = vmalloc(len);
	if (!iram_buffer) {
		GNSSDUMP_ERR("%s vmalloc iram_buffer error\n", __func__);
		kfree(buf);
		return -ENOMEM;
	}
	memset(iram_buffer, 0, len);

	ret = sprdwcn_bus_direct_read(start_addr, buf, len);

	//fs = get_fs();
	//set_fs(KERNEL_DS);
	if (ret < 0) {
		GNSSDUMP_ERR("%s read error:%d\n", __func__, ret);
		goto dump_data_done;
	}

	memcpy(iram_buffer, buf, len);

	ret = gnss_dump_data(buf, len);
	if (ret != len) {
		GNSSDUMP_ERR("%s failed size is %d, ret %d\n", __func__,
				     len, ret);
		goto dump_data_done;
	}
	GNSSDUMP_INFO("%s finish %d\n", __func__, len);
	ret = 0;

dump_data_done:
	kfree(buf);
	//set_fs(fs);
	return ret;
}

static int gnss_ext_dump_mem(void)
{
	int ret = 0;
	int i = 0;

	GNSSDUMP_INFO("%s entry\n", __func__);
	gnss_ext_hold_cpu();
	gnss_ring_reset();
	gnss_marlin3_dump[0].address = GNSS_CP_START_ADDR;
	gnss_marlin3_dump[0].length = GNSS_FIRMWARE_MAX_SIZE;
	for (i = 0; i < ARRAY_SIZE(gnss_marlin3_dump); i++)
		if (gnss_ext_dump_data(gnss_marlin3_dump[i].address,
			gnss_marlin3_dump[i].length)) {
			GNSSDUMP_ERR("%s dumpdata i %d error\n", __func__, i);
			break;
		}
	return ret;
}

int gnss_dump_mem(char flag)
{
	int ret = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated) {
		if (wcn_get_gnss_power_status() == WCN_POWER_STATUS_OFF) {
			GNSSDUMP_INFO("gnss power status off:can not dump gnss\n");
			return -1;
		}
		GNSSDUMP_INFO("need dump gnss\n");
		gnss_dump_level = flag;
		ret = gnss_integrated_dump_mem();
	} else {
		ret = gnss_ext_dump_mem();
	}
	msleep(40);
	gnss_dump_str(GNSS_DUMP_END_STRING, strlen(GNSS_DUMP_END_STRING));
	return ret;
}
EXPORT_SYMBOL_GPL(gnss_dump_mem);
