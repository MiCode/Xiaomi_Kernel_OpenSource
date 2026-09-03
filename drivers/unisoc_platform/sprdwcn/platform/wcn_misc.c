/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * File:		wcn_misc.c
 * Description:	WCN misc file for drivers. Some feature or function
 * isn't easy to classify, then write it in this file.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the	1
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/sched/clock.h>
#include <asm/div64.h>

#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "wcn_txrx.h"
#include "mdbg_type.h"
#include "../include/wcn_dbg.h"
#include "sprd_wcn.h"

static struct atcmd_fifo s_atcmd_owner;
static struct wcn_tm tm;
static unsigned long long s_marlin_bootup_time;

void mdbg_atcmd_owner_init(void)
{
	memset(&s_atcmd_owner, 0, sizeof(s_atcmd_owner));
	mutex_init(&s_atcmd_owner.lock);
}

void mdbg_atcmd_owner_deinit(void)
{
	mutex_destroy(&s_atcmd_owner.lock);
}

static void mdbg_atcmd_owner_add(enum atcmd_owner owner)
{
	mutex_lock(&s_atcmd_owner.lock);
	s_atcmd_owner.owner[s_atcmd_owner.tail % ATCMD_FIFO_MAX] = owner;
	s_atcmd_owner.tail++;
	mutex_unlock(&s_atcmd_owner.lock);
}

enum atcmd_owner mdbg_atcmd_owner_peek(void)
{
	enum atcmd_owner owner;

	mutex_lock(&s_atcmd_owner.lock);
	owner = s_atcmd_owner.owner[s_atcmd_owner.head % ATCMD_FIFO_MAX];
	s_atcmd_owner.head++;
	mutex_unlock(&s_atcmd_owner.lock);

	WCN_INFO("owner=%d, head=%d\n", owner, s_atcmd_owner.head - 1);
	return owner;
}

void mdbg_atcmd_clean(void)
{
	mutex_lock(&s_atcmd_owner.lock);
	memset(&s_atcmd_owner.owner[0], 0, ARRAY_SIZE(s_atcmd_owner.owner));
	s_atcmd_owner.tail = 0;
	s_atcmd_owner.head = 0;
	mutex_unlock(&s_atcmd_owner.lock);
}

/*
 * Until now, CP2 response every AT CMD to AP side
 * without owner-id.AP side transfer every ATCMD
 * response info to WCND.If AP send AT CMD on kernel layer,
 * and the response info transfer to WCND,
 * WCND deal other owner's response CMD.
 * We'll not modify CP2 codes because some
 * products already released to customer.
 * We will save all of the owner-id to the atcmd fifo.
 * and dispatch the response ATCMD info to the matched owner.
 * We'd better send all of the ATCMD with this function
 * or caused WCND error
 */
long int mdbg_send_atcmd(char *buf, size_t len, enum atcmd_owner owner)
{
	long int sent_size = 0;

	mdbg_atcmd_owner_add(owner);

	/* confirm write finish */
	mutex_lock(&s_atcmd_owner.lock);
	sent_size = mdbg_send(buf, len, MDBG_SUBTYPE_AT);
	mutex_unlock(&s_atcmd_owner.lock);

	WCN_INFO("%s, owner=%d\n", buf, owner);

	return sent_size;
}

/* copy from function: kdb_gmtime */
static void wcn_gmtime(struct timespec64 *tv, struct wcn_tm *tm)
{
	uint64_t result = 0;
	uint64_t divisor = 0;
	/* This will work from 1970-2099, 2100 is not a leap year */
	static int mon_day[] = { 31, 29, 31, 30, 31, 30, 31,
				 31, 30, 31, 30, 31 };
	memset(tm, 0, sizeof(*tm));
	result = tv->tv_nsec;
	divisor = 1000000;
	do_div(result, divisor);
	tm->tm_msec = result;

	result = tv->tv_sec;
	divisor = 24 * 60 * 60;
	tm->tm_sec = do_div(result, divisor);
	tm->tm_mday = result + (2 * 365 + 1);

	result = tm->tm_sec;
	divisor = 60;
	tm->tm_sec = do_div(result, divisor);

	tm->tm_min = do_div(result, divisor);
	tm->tm_hour = result;

	result = tm->tm_mday;
	divisor = 4 * 365 + 1;
	tm->tm_mday = do_div(result, divisor);
	tm->tm_year = 68 + 4 * result;

	mon_day[1] = 29;
	while (tm->tm_mday >= mon_day[tm->tm_mon]) {
		tm->tm_mday -= mon_day[tm->tm_mon];
		if (++tm->tm_mon == 12) {
			tm->tm_mon = 0;
			++tm->tm_year;
			mon_day[1] = 28;
		}
	}
	++tm->tm_mday;
}

char *wcn_get_kernel_time(void)
{
	struct timespec64 now;
	static char aptime[64];

	/* get ap kernel time and transfer to China-BeiJing Time */
	ktime_get_real_ts64(&now);
	wcn_gmtime(&now, &tm);
	tm.tm_hour = (tm.tm_hour + WCN_BTWF_TIME_OFFSET) % 24;

	/* save time with string: month,day,hour,min,sec,mili-sec */
	memset(aptime, 0, 64);
	sprintf(aptime, "at+aptime=%ld,%ld,%ld,%ld,%ld,%ld\r\n",
		tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_msec);

	return aptime;
}

/* AP notify BTWF time by at+aptime=... cmd */
long int wcn_ap_notify_btwf_time(void)
{
	char *aptime;
	long int send_cnt = 0;

	aptime = wcn_get_kernel_time();

	/* send to BTWF CP2 */
	send_cnt = mdbg_send_atcmd((void *)aptime, strlen(aptime),
				   WCN_ATCMD_KERNEL);
	WCN_INFO("%s, send_cnt=%ld", aptime, send_cnt);

	return send_cnt;
}

/*
 * Only marlin poweron and marlin starts to run,
 * it can call this function.
 * The time will be sent to marlin with loopcheck CMD.
 * NOTES:If marlin power off, and power on again, it
 * should call this function again.
 */
void marlin_bootup_time_update(void)
{
	s_marlin_bootup_time = local_clock();
	WCN_INFO("s_marlin_bootup_time=%llu",
		 s_marlin_bootup_time);
}

unsigned long long marlin_bootup_time_get(void)
{
	return s_marlin_bootup_time;
}

#define WCN_VMAP_RETRY_CNT (20)
static void *wcn_mem_ram_vmap(phys_addr_t start, size_t size,
			      int noncached, unsigned int *count)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;
	phys_addr_t addr;
	int retry = 0;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	*count = page_count;
	if (noncached)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = PAGE_KERNEL;

retry1:
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		if (retry++ < WCN_VMAP_RETRY_CNT) {
			usleep_range(8000, 10000);
			goto retry1;
		} else {
			WCN_ERR("malloc err\n");
			return NULL;
		}
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
retry2:
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	if (!vaddr) {
		if (retry++ < WCN_VMAP_RETRY_CNT) {
			usleep_range(8000, 10000);
			goto retry2;
		} else {
			WCN_ERR("vmap err\n");
			goto out;
		}
	} else {
		vaddr += offset_in_page(start);
	}
out:
	kfree(pages);

	return vaddr;
}

void wcn_mem_ram_unmap(const void *mem, unsigned int count)
{
	//vm_unmap_ram(mem - offset_in_page(mem), count);
	vunmap(mem - offset_in_page(mem));
}

void *wcn_mem_ram_vmap_nocache(phys_addr_t start, size_t size,
			       unsigned int *count)
{
	return wcn_mem_ram_vmap(start, size, 1, count);
}

#ifdef CONFIG_ARM64
static inline void wcn_unalign_memcpy_toio(void *to, const void *from, u32 len)
{

	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)from & 7) && len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
		memcpy_toio(to, from, len);
	} else if (((unsigned long)to & 3) == ((unsigned long)from & 3)) {
		while (((unsigned long)from & 3) && len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
		while (len >= 4) {
			*(u32 *)(to) = *(u32 *)(from);
			to += 4;
			from += 4;
			len -= 4;
		}
		while (len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
	} else {
		while (len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
	}
}

static inline void wcn_unalign_memcpy_fromio(void *to, const void *from, u32 len)
{

	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)from & 7) && len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
		memcpy_fromio(to, from, len);
	} else if (((unsigned long)to & 3) == ((unsigned long)from & 3)) {
		while (((unsigned long)from & 3) && len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
		while (len >= 4) {
			*(u32 *)(to) = *(u32 *)(from);
			to += 4;
			from += 4;
			len -= 4;
		}
		while (len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
	} else {
		while (len) {
			*(char *)(to++) = *(char *)(from++);
			len--;
		}
	}
}
#else
#define wcn_unalign_memcpy_fromio(to, from, len)	wcn_unalign_memcpy(to, from, len)
#define wcn_unalign_memcpy_toio(to, from, len)	wcn_unalign_memcpy(to, from, len)

static inline void wcn_unalign_memcpy(void *to, const void *from, u32 len)
{
	memcpy(to, from, len);
}
#endif

int wcn_write_zero_to_phy_addr(phys_addr_t phy_addr, u32 size)
{
	char *virt_addr;
	unsigned int cnt;
	unsigned char zero = 0x00;
	unsigned int loop = 0;

	virt_addr = (char *)wcn_mem_ram_vmap_nocache(phy_addr, size, &cnt);
	if (virt_addr) {
		for (loop = 0; loop < size; loop++)
			wcn_unalign_memcpy_toio((void *)(virt_addr + loop), &zero, 1);

		wcn_mem_ram_unmap(virt_addr, cnt);
		return 0;
	}

	WCN_ERR("%s fail\n", __func__);
	return -1;
}

int wcn_write_data_to_phy_addr(phys_addr_t phy_addr,
			       void *src_data, u32 size)
{
	char *virt_addr, *src;
	unsigned int cnt;

	src = (char *)src_data;
	virt_addr = (char *)wcn_mem_ram_vmap_nocache(phy_addr, size, &cnt);
	if (virt_addr) {
		wcn_unalign_memcpy_toio((void *)virt_addr, (void *)src, size);
		wcn_mem_ram_unmap(virt_addr, cnt);
		return 0;
	}

	WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
	return -1;
}
EXPORT_SYMBOL_GPL(wcn_write_data_to_phy_addr);

int wcn_read_data_from_phy_addr(phys_addr_t phy_addr,
				void *tar_data, u32 size)
{
	char *virt_addr, *tar;
	unsigned int cnt;

	tar = (char *)tar_data;
	virt_addr = wcn_mem_ram_vmap_nocache(phy_addr, size, &cnt);
	if (virt_addr) {
		wcn_unalign_memcpy_fromio((void *)tar, (void *)virt_addr, size);
		wcn_mem_ram_unmap(virt_addr, cnt);
		return 0;
	}

	WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
	return -1;
}
EXPORT_SYMBOL_GPL(wcn_read_data_from_phy_addr);

enum cp2_chip_type wcn_get_cp2_type(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_l6)
		return l6_ums2631;

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		return m3_sc2355;

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		return m3e_umw2653;

	if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		return m3lite_ums2653;

	if (g_match_config && g_match_config->unisoc_wcn_l3)
		return l3_ums2342;

	return type_unknow;
}
EXPORT_SYMBOL_GPL(wcn_get_cp2_type);
