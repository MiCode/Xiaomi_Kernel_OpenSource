/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : slp_mgr.c
 * Abstract : This file is a implementation for  sleep manager
 *
 * Authors	: QI.SUN
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <misc/marlin_platform.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <misc/wcn_bus.h>
#include "mem_pd_mgr.h"
#include "../include/wcn_glb_reg.h"
#include "../sleep/sdio_int.h"
#include "../include/wcn_dbg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "WCN MEM_PD" fmt

#define MEM_PD_ERR -3
#define CP_NO_MEM_PD_TIMEROUT 2000
#define CP_TIMEROUT 30000
/* time out in waiting wifi to come up */
#define MEM_PD_UNIT_SIZE 0X8000/* 32k */
#define SDIO_CP_BASE_ADD 0X40400000/* 32k */
#define CP_MEM_OFFSET 0X00100000/* 32k */
#define DRAM_ADD 0X40580000

#define REGS_SPINLOCK_BASE 0X40850000
#define REG_SPINLOCK_SPINLOCKSTS_I (REGS_SPINLOCK_BASE + 0X0800)
#define UNLOCK_TOKEN (0X55AA10C5)
#define SPINLOCKSTS(I)  (REG_SPINLOCK_SPINLOCKSTS_I + 4 * (I))

#define BT_BEGIN_OFFSET_SYNC 4
#define BT_END_OFFSET_SYNC 8
#define WIFI_BEGIN_OFFSET_SYNC 12
#define WIFI_END_OFFSET_SYNC 16

static struct mem_pd_t mem_pd;
static struct mem_pd_meminfo_t mem_info_cp;

/* return 0, no download ini; return 1, need download ini */
unsigned int mem_pd_wifi_state(void)
{
	unsigned int ret = 0;

	if (mem_pd.cp_version)
		return 0;
	ret = mem_pd.wifi_state;

	return ret;
}

unsigned int mem_pd_spinlock_lock(int id)
{
	int ret = 0;
	int i = 0;
	unsigned int reg_val = 0;

	do {
		i++;
		ret = sprdwcn_bus_reg_read(SPINLOCKSTS(id), &reg_val, 4);
		if (!(ret == 0)) {
			WCN_INFO(" sdiohal_dt_read lock error !\n");
			return ret;
		}
		if (reg_val == 0)
			break;
		if (i > 200) {
			i = 0;
			WCN_INFO("get spinlock time out\n");
		}
	} while (i);

	return 0;
}

unsigned int mem_pd_spinlock_unlock(int id)
{
	int ret = 0;
	unsigned int reg_val = UNLOCK_TOKEN;

	ret = sprdwcn_bus_reg_write(SPINLOCKSTS(id),  &reg_val, 4);
	if (!(ret == 0)) {
		WCN_INFO(" dt_write lock error !\n");
		return ret;
	}

	return 0;
}

/* bit_start FORCE SHUTDOWN IRAM [16...31]*32K=512K
 * and bit_start++ bit_cnt how many 32k
 */
static int mem_pd_power_switch(enum wcn_sub_sys subsys, int val)
{
	int ret = 0;
	unsigned int reg_val = 0;
	unsigned int wif_bt_mem_cfg = 0;
	unsigned int bt_ram_mask;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	/* unsigned int mem_pd_power_delay; */
	WCN_INFO("%s\n", __func__);
	/* get the lock to write the register, use spinlock id=0 */
	mem_pd_spinlock_lock(0);
	/* CP reset write 1, mask mem CGG reg */
	/* should write 0 */
	switch (subsys) {
	case MARLIN_WIFI:
		if (val) {
			if (mem_pd.wifi_state == THREAD_DELETE) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("wifi_state=0, forbid on\n");
				return ret;
			}
			/* wifi iram mem pd range */
			wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG1;
			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO(" sdiohal_dt_read error !\n");
				return ret;
			}
			if (reg_val & mem_info_cp.wifi_iram_mask) {
				/* val =1 ,powerdown */
				reg_val &= ~mem_info_cp.wifi_iram_mask;
				/* set bit_start ,mem power down */
				ret = sprdwcn_bus_reg_write(
				wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO("dt_write error !\n");
					return ret;
				}
			}
			WCN_INFO("wifi irammem power on\n");
			/* wifi dram mem pd range */
			if (g_match_config && g_match_config->unisoc_wcn_m3lite)
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG2;
			else
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG3;

			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("sdiohal_dt_read error !\n");
				return ret;
			}
			if (reg_val & mem_info_cp.wifi_dram_mask) {
				/* val =1 ,powerdown */
				reg_val &= ~mem_info_cp.wifi_dram_mask;
				/* set bit_start ,mem power down */
				ret = sprdwcn_bus_reg_write(
				wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO("dt_write error !\n");
					return ret;
				}
			}
			WCN_INFO("wifi drammem power on\n");
		} else {
			if (mem_pd.wifi_state == THREAD_CREATE) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("wifi_state=1, forbid off\n");
				return ret;
			}
			/* wifi iram mem pd range */
			wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG1;
			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("dt read error !\n");
				return ret;
			}
			if (!(reg_val & mem_info_cp.wifi_iram_mask)) {
				reg_val |= mem_info_cp.wifi_iram_mask;
				/* clear bit_start ,mem power on */
				ret = sprdwcn_bus_reg_write(
				wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO("dt write error !\n");
					return ret;
				}
			}
			WCN_INFO("wifi irammem power down\n");
			/* wifi dram mem pd range */
			if (g_match_config && g_match_config->unisoc_wcn_m3lite)
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG2;
			else
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG3;

			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO(" sdio read error !\n");
				return ret;
			}
			if (!(reg_val & mem_info_cp.wifi_dram_mask)) {
				reg_val |= mem_info_cp.wifi_dram_mask;
				/* clear bit_start ,mem power on */
				ret = sprdwcn_bus_reg_write(
					wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO("dt write error !\n");
					return ret;
				}
			}
			WCN_INFO("wifi drammem power down\n");
		}
	break;
	case MARLIN_BLUETOOTH:
		if (val) {
			if (mem_pd.bt_state == THREAD_DELETE) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("bt_state=0, forbid on\n");
				return ret;
			}
			/* bt iram mem pd range */
			if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG2;
				bt_ram_mask = mem_info_cp.bt_dram_mask;
			} else {
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG1;
				bt_ram_mask = mem_info_cp.bt_iram_mask;
			}

			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO(" sdio dt read error !\n");
				return ret;
			}
			if (reg_val & bt_ram_mask) {
				/* val =1 ,powerdown */
				reg_val &= ~bt_ram_mask;
				/* set bit_start ,mem power down */
				ret = sprdwcn_bus_reg_write(
					wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO(" error !\n");
					return ret;
				}
			}
			WCN_INFO("bt irampower on\n");
		} else {
			if (mem_pd.bt_state == THREAD_CREATE) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO("bt_state=1, forbid off\n");
				return ret;
			}
			/* bt iram mem pd range */
			if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG2;
				bt_ram_mask = mem_info_cp.bt_dram_mask;
			} else {
				wif_bt_mem_cfg = REG_AON_APB_BTWF_MEM_CGG1;
				bt_ram_mask = mem_info_cp.bt_iram_mask;
			}

			ret = sprdwcn_bus_reg_read(wif_bt_mem_cfg, &reg_val, 4);
			if (!(ret == 0)) {
				mem_pd_spinlock_unlock(0);
				WCN_INFO(" error !\n");
				return ret;
			}
			if (reg_val & bt_ram_mask) {
				/* val =1 ,powerdown */
				WCN_INFO(" mem reg val =1 !\n");
			} else {
				reg_val |= bt_ram_mask;
				/* clear bit_start ,mem power on */
				ret = sprdwcn_bus_reg_write(
				wif_bt_mem_cfg, &reg_val, 4);
				if (!(ret == 0)) {
					mem_pd_spinlock_unlock(0);
					WCN_INFO(" error !\n");
					return ret;
				}
			}
			WCN_INFO("bt iram power down\n");
		}
	break;
	default:
	break;
	}
	mem_pd_spinlock_unlock(0);

	return 0;
}

int inform_cp_wifi_download(void)
{
	sdio_ap_int_cp0(WIFI_BIN_DOWNLOAD);
	WCN_INFO("%s\n", __func__);

	return 0;
}
static int inform_cp_bt_download(void)
{
	sdio_ap_int_cp0(BT_BIN_DOWNLOAD);
	WCN_INFO("%s\n", __func__);

	return 0;
}

static void wifi_cp_open(void)
{
	WCN_INFO(" wifi_open int from cp\n");
	complete(&(mem_pd.wifi_open_completion));
}
static void wifi_cp_close(void)
{
	complete(&(mem_pd.wifi_cls_cpl));
	WCN_INFO("wifi_thread_delete int\n");
}

static void bt_cp_open(void)
{
	WCN_INFO("bt_open int from cp\n");
	complete(&(mem_pd.bt_open_completion));
}
static void bt_cp_close(void)
{
	complete(&(mem_pd.bt_close_completion));
	WCN_INFO("bt_thread_delete int\n");
}
static void save_bin_cp_ready(void)
{
	if (mem_pd.cp_mem_all_off == 0) {
		complete(&(mem_pd.save_bin_completion));
		WCN_INFO("%s ,cp while(1) state\n", __func__);
		mem_pd.cp_mem_all_off = 1;
		return;
	}
	WCN_INFO("%s ,wifi/bt power down, wait event\n", __func__);
}
static int mem_pd_pub_int_RegCb(void)
{
	sdio_pub_int_regcb(WIFI_OPEN, (PUB_INT_ISR)wifi_cp_open);
	sdio_pub_int_regcb(WIFI_CLOSE, (PUB_INT_ISR)wifi_cp_close);
	sdio_pub_int_regcb(BT_OPEN, (PUB_INT_ISR)bt_cp_open);
	sdio_pub_int_regcb(BT_CLOSE, (PUB_INT_ISR)bt_cp_close);
	sdio_pub_int_regcb(MEM_SAVE_BIN, (PUB_INT_ISR)save_bin_cp_ready);

	return 0;
}

static int sdio_read_mem_from_cp(void)
{
	int err = 0;

	WCN_INFO("%s  read wifi/bt mem bin\n", __func__);
	err = sprdwcn_bus_direct_read(mem_info_cp.wifi_begin_addr,
				      mem_pd.wifi_mem, mem_info_cp.wifi_size);
	if (err < 0) {
		pr_err("%s wifi save mem bin error:%d", __func__, err);
		return err;
	}
	err = sprdwcn_bus_direct_read(mem_info_cp.bt_begin_addr,
				      mem_pd.bt_mem, mem_info_cp.bt_size);
	if (err < 0) {
		pr_err("%s bt save mem bin error:%d", __func__, err);
		return err;
	}
	WCN_INFO("%s save wifi/bt mem bin ok\n", __func__);

	return 0;
}
static int sdio_ap_int_cp_save_cp_mem(void)
{
	sdio_ap_int_cp0(SAVE_CP_MEM);
	WCN_INFO("%s, cp while(1) break\n", __func__);

	return 0;
}

static int mem_pd_read_add_from_cp(void)
{
	int ret;
	unsigned int bt_begin = 0, bt_end = 0;
	unsigned int wifi_begin = 0, wifi_end = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	ret = sprdwcn_bus_reg_read(get_sync_addr() + BT_BEGIN_OFFSET_SYNC,
				   &bt_begin, 4);
	if (ret < 0) {
		WCN_ERR("%s mem_pd read  bt begin addr error:%d\n",
			__func__, ret);
		return ret;
	}
	ret = sprdwcn_bus_reg_read(get_sync_addr() + BT_END_OFFSET_SYNC, &bt_end, 4);
	if (ret < 0) {
		WCN_ERR("%s mem_pd read  bt end addr error:%d\n",
			__func__, ret);
		return ret;
	}
	ret = sprdwcn_bus_reg_read(get_sync_addr() + WIFI_BEGIN_OFFSET_SYNC,
				   &wifi_begin, 4);
	if (ret < 0) {
		WCN_ERR("%s mem_pd read  wifi begin addr error:%d\n",
			__func__, ret);
		return ret;
	}
	ret = sprdwcn_bus_reg_read(get_sync_addr() + WIFI_END_OFFSET_SYNC,
				   &wifi_end, 4);
	if (ret < 0) {
		WCN_ERR("%s mem_pd read  wifi end addr error:%d\n",
			__func__, ret);
		return ret;
	}
	mem_info_cp.chip_id = marlin_get_wcn_chipid();
	mem_info_cp.bt_begin_addr = bt_begin + SDIO_CP_BASE_ADD;
	mem_info_cp.bt_end_addr = bt_end + SDIO_CP_BASE_ADD;
	mem_info_cp.bt_size = bt_end - bt_begin;
	mem_info_cp.wifi_begin_addr = wifi_begin + SDIO_CP_BASE_ADD;
	mem_info_cp.wifi_end_addr = wifi_end + SDIO_CP_BASE_ADD;
	mem_info_cp.wifi_size = wifi_end - wifi_begin;

	if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
		mem_info_cp.bt_iram_mask = 0;
		mem_info_cp.bt_dram_mask = 0xC00;
		mem_info_cp.wifi_iram_mask = 0xFC00;
		mem_info_cp.wifi_dram_mask = 0x80;
	} else {
		mem_info_cp.bt_iram_mask = 0x0FE00000;
		mem_info_cp.bt_dram_mask = 0;
		mem_info_cp.wifi_iram_mask = 0xE0000000;
		mem_info_cp.wifi_dram_mask = 0x1FFF0000;
	}

	mem_pd.wifi_mem = kmalloc(mem_info_cp.wifi_size, GFP_KERNEL);
	if (!mem_pd.wifi_mem) {
		WCN_INFO("mem pd wifi save buff malloc Failed.\n");
		return MEM_PD_ERR;
	}
	mem_pd.bt_mem = kmalloc(mem_info_cp.bt_size, GFP_KERNEL);
	if (!mem_pd.bt_mem) {
		kfree(mem_pd.wifi_mem);
		WCN_INFO("mem pd bt save buff malloc Failed.\n");
		return MEM_PD_ERR;
	}
	mem_pd.wifi_clear = kmalloc(mem_info_cp.wifi_size, GFP_KERNEL);
	if (!mem_pd.wifi_clear) {
		kfree(mem_pd.wifi_mem);
		kfree(mem_pd.bt_mem);
		WCN_INFO("mem pd clear buff malloc Failed.\n");
		return MEM_PD_ERR;
	}
	mem_pd.bt_clear = kmalloc(mem_info_cp.bt_size, GFP_KERNEL);
	if (!mem_pd.bt_clear) {
		kfree(mem_pd.wifi_mem);
		kfree(mem_pd.bt_mem);
		kfree(mem_pd.wifi_clear);
		WCN_INFO("mem pd clear buff malloc Failed.\n");
		return MEM_PD_ERR;
	}
	memset(mem_pd.wifi_clear, 0x0, mem_info_cp.wifi_size);
	memset(mem_pd.bt_clear, 0x0, mem_info_cp.bt_size);

	return 0;
}

int mem_pd_save_bin(void)
{
	/* mutex_lock(&(mem_pd.mem_pd_lock)); */
	WCN_INFO("%s entry\n", __func__);
	if (wait_for_completion_timeout(
		&(mem_pd.save_bin_completion),
		msecs_to_jiffies(CP_NO_MEM_PD_TIMEROUT)) <= 0) {
		WCN_INFO("cp version is wcn_trunk ,cp_version =1\n");
		/* mutex_unlock(&(mem_pd.mem_pd_lock)); */
		mem_pd.cp_version = 1;
		return 0;
	}
	if (mem_pd.bin_save_done == 0) {
		mem_pd.bin_save_done = 1;
		WCN_INFO("cp first power on");
		mem_pd_read_add_from_cp();
		sdio_read_mem_from_cp();
		/* save to char[] */
	} else
		WCN_INFO("cp not first power on %s do nothing\n",
			 __func__);
	mem_pd_power_switch(MARLIN_WIFI, FALSE);
	mem_pd_power_switch(MARLIN_BLUETOOTH, FALSE);
	WCN_INFO("wifi/bt mem power down\n");
	sdio_ap_int_cp_save_cp_mem();
	/* save done , AP inform cp by INT. */
	/* mutex_unlock(&(mem_pd.mem_pd_lock)); */

	return 0;
}

static int ap_int_cp_wifi_bin_done(int subsys)
{
	switch (subsys) {
	case MARLIN_WIFI:
		inform_cp_wifi_download();
	break;
	case MARLIN_BLUETOOTH:
		inform_cp_bt_download();
	break;
	default:
	return MEM_PD_ERR;
	}

	return 0;
}

int test_mem_clrear(enum wcn_sub_sys subsys)
{
	int err;

	switch (subsys) {
	case MARLIN_WIFI:
		err = sprdwcn_bus_direct_write(mem_info_cp.wifi_begin_addr,
					       mem_pd.wifi_clear,
					       mem_info_cp.wifi_size);
		if (err < 0) {
			pr_err("%s wifi down bin error:%d", __func__, err);
			return err;
		}
	break;
	case MARLIN_BLUETOOTH:
		err = sprdwcn_bus_direct_write(mem_info_cp.bt_begin_addr,
					       mem_pd.bt_clear,
					       mem_info_cp.bt_size);
		if (err < 0) {
			pr_err("%s bt down mem bin error:%d", __func__, err);
			return err;
		}
	break;
	default:
	return MEM_PD_ERR;
	}

	return 0;
}

static int mem_pd_download_mem_bin(int subsys)
{
	int err;
	unsigned int addr = 0;
	char *mem;
	unsigned int len = 0;

	WCN_INFO("%s\n", __func__);
	switch (subsys) {
	case MARLIN_WIFI:
		addr = mem_info_cp.wifi_begin_addr;
		mem = mem_pd.wifi_mem;
		len = mem_info_cp.wifi_size;
		WCN_INFO("%s, wifi mem download ok\n", __func__);
	break;
	case MARLIN_BLUETOOTH:
		addr = mem_info_cp.bt_begin_addr;
		mem = mem_pd.bt_mem;
		len = mem_info_cp.bt_size;
		WCN_INFO("%s, bt mem download ok\n", __func__);
	break;
	default:
		return MEM_PD_ERR;
	}
	err = sprdwcn_bus_direct_write(addr, mem, len);
	if (err < 0) {
		pr_err("%s download mem bin error:%d", __func__, err);
		return err;
	}

	return 0;
}
int mem_pd_mgr(enum wcn_sub_sys subsys, int val)
{
	if (mem_pd.cp_version)
		return 0;
	WCN_INFO("%s mem on/off\n", __func__);
	if ((subsys != MARLIN_WIFI) && (subsys != MARLIN_BLUETOOTH)) {
		WCN_INFO("subsys:%d, do nothing, return ok\n", subsys);
		return 0;
	}
	mutex_lock(&(mem_pd.mem_pd_lock));
	switch (subsys) {
	case MARLIN_WIFI:
		WCN_INFO("marlin wifi state:%d, subsys %d power %d\n",
			 mem_pd.wifi_state, subsys, val);
		if (val) {
			if (mem_pd.wifi_state != THREAD_DELETE) {
				WCN_INFO("wifi opened ,do nothing\n");
				goto out;
			}
			mem_pd.wifi_state = THREAD_CREATE;
			mem_pd_power_switch(subsys, val);
			mem_pd_download_mem_bin(subsys);
			/* avoid fake interrupt , and reinit */
			reinit_completion(&(mem_pd.wifi_open_completion));
			ap_int_cp_wifi_bin_done(subsys);
			if (wait_for_completion_timeout(
				&(mem_pd.wifi_open_completion),
			msecs_to_jiffies(CP_TIMEROUT))
			<= 0) {
				WCN_INFO("wifi creat fail\n");
				goto mem_pd_err;
			}
			WCN_INFO("cp wifi creat thread ok\n");
		} else {
			if (mem_pd.wifi_state != THREAD_CREATE) {
				WCN_INFO("wifi closed ,do nothing\n");
				goto out;
			}
			/* avoid fake interrupt , and reinit */
			reinit_completion(&(mem_pd.wifi_cls_cpl));
			sprdwcn_bus_aon_writeb(0x1b0, 0x10);
			/* instead of cp wifi delet thread ,inform sdio. */
			if (wait_for_completion_timeout(&(mem_pd.wifi_cls_cpl),
						msecs_to_jiffies(CP_TIMEROUT))
			<= 0) {
				WCN_INFO("wifi delete fail\n");
				goto mem_pd_err;
			}
			mem_pd.wifi_state = THREAD_DELETE;
			mem_pd_power_switch(subsys, val);
			WCN_INFO("cp wifi delete thread ok\n");
		}
		break;
	case MARLIN_BLUETOOTH:
		WCN_INFO("marlin bt state:%d, subsys %d power %d\n",
			 mem_pd.bt_state, subsys, val);
		if (val) {
			if (mem_pd.bt_state != THREAD_DELETE) {
				WCN_INFO("bt opened ,do nothing\n");
				goto out;
			}
			mem_pd.bt_state = THREAD_CREATE;
			mem_pd_power_switch(subsys, val);
			mem_pd_download_mem_bin(subsys);
			/* avoid fake interrupt, and reinit */
			reinit_completion(&(mem_pd.bt_open_completion));
			ap_int_cp_wifi_bin_done(subsys);
		if (wait_for_completion_timeout(&(mem_pd.bt_open_completion),
			msecs_to_jiffies(CP_TIMEROUT)) <= 0) {
			WCN_INFO("cp bt creat thread fail\n");
			goto mem_pd_err;
		}
			WCN_INFO("cp bt creat thread ok\n");
		} else {
			if (mem_pd.bt_state != THREAD_CREATE) {
				WCN_INFO("bt closed ,do nothing\n");
				goto out;
			}
			if (wait_for_completion_timeout(
				&(mem_pd.bt_close_completion),
			msecs_to_jiffies(CP_TIMEROUT))
				<= 0) {
				WCN_INFO("bt delete fail\n");
				goto mem_pd_err;
			}
			mem_pd.bt_state = THREAD_DELETE;
			mem_pd_power_switch(subsys, val);
			WCN_INFO("cp bt delete thread ok\n");
		}
		break;
	default:
		WCN_INFO("%s switch default\n", __func__);
	}

out:
		mutex_unlock(&(mem_pd.mem_pd_lock));

		return 0;

mem_pd_err:
		mutex_unlock(&(mem_pd.mem_pd_lock));
		WCN_ERR("%s return error\n", __func__);

		return -1;
}
int mem_pd_poweroff_deinit(void)
{
	if (mem_pd.cp_version)
		return 0;
	WCN_INFO("mem_pd_chip_poweroff_deinit\n");
	mem_pd.wifi_state = 0;
	mem_pd.bt_state = 0;
	mem_pd.cp_version = 0;
	mem_pd.cp_mem_all_off = 0;
	reinit_completion(&(mem_pd.wifi_open_completion));
	reinit_completion(&(mem_pd.wifi_cls_cpl));
	reinit_completion(&(mem_pd.bt_open_completion));
	reinit_completion(&(mem_pd.bt_close_completion));
	reinit_completion(&(mem_pd.save_bin_completion));

	return 0;
}
int mem_pd_init(void)
{
	WCN_INFO("%s enter\n", __func__);
	mutex_init(&(mem_pd.mem_pd_lock));
	init_completion(&(mem_pd.wifi_open_completion));
	init_completion(&(mem_pd.wifi_cls_cpl));
	init_completion(&(mem_pd.bt_open_completion));
	init_completion(&(mem_pd.bt_close_completion));
	init_completion(&(mem_pd.save_bin_completion));
	mem_pd_pub_int_RegCb();
	/* mem_pd.wifi_state = 0; */
	/* mem_pd.bt_state = 0; */
	/* mem_pd.cp_version = 0; */
	/* mem_pd.cp_mem_all_off = 0; */

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}

int mem_pd_exit(void)
{
	WCN_INFO("%s enter\n", __func__);
	/* atomic_set(&(slp_mgr.cp2_state), STAY_SLPING); */
	/* sleep_active_modules = 0; */
	/* wake_cnt = 0; */
	mutex_destroy(&(mem_pd.mem_pd_lock));
	/* mutex_destroy(&(slp_mgr.wakeup_lock)); */
	kfree(mem_pd.wifi_mem);
	mem_pd.wifi_mem = NULL;
	kfree(mem_pd.bt_mem);
	mem_pd.bt_mem = NULL;
	kfree(mem_pd.wifi_clear);
	kfree(mem_pd.bt_clear);
	mem_pd.wifi_clear = NULL;
	mem_pd.bt_clear = NULL;
	WCN_INFO("%s ok!\n", __func__);

	return 0;
}

