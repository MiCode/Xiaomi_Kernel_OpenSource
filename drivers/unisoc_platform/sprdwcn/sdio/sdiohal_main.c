// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : sdiohal_main.c
 * Abstract : This file is a implementation for wcn sdio hal function
 */

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>

#include "sdiohal.h"

#define CP_GPIO1_REG 0x40840014
#define CP_PIN_FUNC_WPU BIT(8)

static void (*scan_card_notify)(void);
static struct sdiohal_data_t *sdiohal_data;
unsigned long long tm_enter_tx_thread;
unsigned long long tm_exit_tx_thread;


struct sdiohal_data_t *sdiohal_get_data(void)
{
	return sdiohal_data;
}

static int sdiohal_card_lock(struct sdiohal_data_t *p_data)
{
	if (atomic_inc_return(&p_data->xmit_cnt) >= SDIOHAL_REMOVE_CARD_VAL) {
		atomic_dec(&p_data->xmit_cnt);
		pr_err("%s failed\n", __func__);
		return -1;
	}

	return 0;
}

static void sdiohal_card_unlock(struct sdiohal_data_t *p_data)
{
	atomic_dec(&p_data->xmit_cnt);
}

static void sdiohal_abort(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int err;
	unsigned char val;

	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	val = sdio_readb(p_data->sdio_func[FUNC_0], 0x0, &err);
	pr_info("before abort, SDIO_VER_CCCR:0x%x\n", val);

	sdio_writeb(p_data->sdio_func[FUNC_0], VAL_ABORT_TRANS,
		    SDIOHAL_CCCR_ABORT, &err);

	val = sdio_readb(p_data->sdio_func[FUNC_0], 0x0, &err);
	pr_info("after abort, SDIO_VER_CCCR:0x%x\n", val);
	sdio_release_host(p_data->sdio_func[FUNC_0]);
}

/* Get Success Transfer pac num Before Abort */
static void sdiohal_success_trans_pac_num(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned char stbba0;
	unsigned char stbba1;
	unsigned char stbba2;
	unsigned char stbba3;
	int err;

	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	stbba0 = sdio_readb(p_data->sdio_func[FUNC_0],
			    SDIOHAL_FBR_STBBA0, &err);
	stbba1 = sdio_readb(p_data->sdio_func[FUNC_0],
			    SDIOHAL_FBR_STBBA1, &err);
	stbba2 = sdio_readb(p_data->sdio_func[FUNC_0],
			    SDIOHAL_FBR_STBBA2, &err);
	stbba3 = sdio_readb(p_data->sdio_func[FUNC_0],
			    SDIOHAL_FBR_STBBA3, &err);
	p_data->success_pac_num = stbba0 | (stbba1 << 8) |
				  (stbba2 << 16) | (stbba3 << 24);
	sdio_release_host(p_data->sdio_func[FUNC_0]);

	pr_info("success num:[%d]\n", p_data->success_pac_num);
}

unsigned int sdiohal_get_trans_pac_num(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	return p_data->success_pac_num;
}

int sdiohal_sdio_pt_write(unsigned char *src, unsigned int datalen)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	ktime_get_real_ts64(&tm_begin);
	if (unlikely(p_data->card_dump_flag == true)) {
		pr_err("%s line %d dump happened\n", __func__, __LINE__);
		return -1;
	}

	if (datalen % 4 != 0)
		return -EINVAL;

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_op_enter();
	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	ret = sdio_writesb(p_data->sdio_func[FUNC_1],
		SDIOHAL_PK_MODE_ADDR, src, datalen);
	sdio_release_host(p_data->sdio_func[FUNC_1]);
	if (ret != 0) {
		sdiohal_success_trans_pac_num();
		sdiohal_abort();
	}
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	ktime_get_real_ts64(&tm_end);
	time_total_ns += timespec64_to_ns(&tm_end) - timespec64_to_ns(&tm_begin);
	times_count++;
	if (!(times_count % PERFORMANCE_COUNT)) {
		sdiohal_pr_perf("tx avg time:%ld\n",
				(time_total_ns / PERFORMANCE_COUNT));
		time_total_ns = 0;
		times_count = 0;
	}

	return ret;
}

int sdiohal_sdio_pt_read(unsigned char *src, unsigned int datalen)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	ktime_get_real_ts64(&tm_begin);

	if (unlikely(p_data->card_dump_flag == true)) {
		pr_err("%s line %d dump happened\n", __func__, __LINE__);
		return -1;
	}

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_op_enter();
	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	ret = sdio_readsb(p_data->sdio_func[FUNC_1], src,
		SDIOHAL_PK_MODE_ADDR, datalen);
	sdio_release_host(p_data->sdio_func[FUNC_1]);
	if (ret != 0)
		sdiohal_abort();
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	ktime_get_real_ts64(&tm_end);
	time_total_ns += timespec64_to_ns(&tm_end) - timespec64_to_ns(&tm_begin);
	times_count++;
	if (!(times_count % PERFORMANCE_COUNT)) {
		sdiohal_pr_perf("rx avg time:%ld\n",
				(time_total_ns / PERFORMANCE_COUNT));
		time_total_ns = 0;
		times_count = 0;
	}

	return ret;
}

static int sdiohal_config_packer_chain(struct sdiohal_list_t *data_list,
	struct sdio_func *sdio_func, uint fix_inc, bool dir, uint addr)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mmc_request mmc_req;
	struct mmc_command mmc_cmd;
	struct mmc_data mmc_dat;
	struct mmc_host *host = sdio_func->card->host;
	bool fifo = (fix_inc == SDIOHAL_DATA_FIX);
	uint fn_num = sdio_func->num;
	uint blk_num, blk_size, max_blk_count, max_req_size;
	struct mbuf_t *mbuf_node;
	unsigned int sg_count, sg_data_size;
	unsigned int i, ttl_len = 0, node_num;
	int err_ret;

	node_num = data_list->node_num;
	if (node_num > MAX_CHAIN_NODE_NUM)
		node_num = MAX_CHAIN_NODE_NUM;

	sdiohal_list_check(data_list, __func__, dir);

	blk_size = SDIOHAL_BLK_SIZE;
	max_blk_count = min_t(unsigned int,
			      host->max_blk_count, (uint)MAX_IO_RW_BLK);
	max_req_size = min_t(unsigned int,
			     max_blk_count * blk_size, host->max_req_size);

	sg_count = 0;
	memset(&mmc_req, 0, sizeof(struct mmc_request));
	memset(&mmc_cmd, 0, sizeof(struct mmc_command));
	memset(&mmc_dat, 0, sizeof(struct mmc_data));
	sg_init_table(p_data->sg_list, ARRAY_SIZE(p_data->sg_list));

	mbuf_node = data_list->mbuf_head;
	for (i = 0; i < node_num; i++, mbuf_node = mbuf_node->next) {
		if (!mbuf_node) {
			pr_err("%s tx config adma, mbuf ptr error:%p\n",
				__func__, mbuf_node);
			return -1;
		}

		if (sg_count >= ARRAY_SIZE(p_data->sg_list)) {
			pr_err("%s:sg list exceed limit\n", __func__);
			return -1;
		}

		if (dir)
			sg_data_size = SDIOHAL_ALIGN_4BYTE(mbuf_node->len +
				       sizeof(struct bus_puh_t));
		else
			sg_data_size = MAX_PAC_SIZE;
		if (sg_data_size > MAX_PAC_SIZE) {
			pr_err("pac size > cp buf size,len %d\n",
				sg_data_size);
			return -1;
		}

		if (sg_data_size > host->max_seg_size)
			sg_data_size = host->max_seg_size;

		sg_set_buf(&p_data->sg_list[sg_count++],
			   mbuf_node->buf, sg_data_size);
		ttl_len += sg_data_size;
	}

	if (dir) {
		sg_data_size = SDIOHAL_ALIGN_BLK(ttl_len +
			SDIO_PUB_HEADER_SIZE) - ttl_len;
		if (sg_data_size > MAX_PAC_SIZE) {
			pr_err("eof pac size > cp buf size,len %d\n",
			       sg_data_size);
			return -1;
		}
		sg_set_buf(&p_data->sg_list[sg_count++],
			   p_data->eof_buf, sg_data_size);
		ttl_len = SDIOHAL_ALIGN_BLK(ttl_len +
			  SDIO_PUB_HEADER_SIZE);
	} else {
		sg_data_size = SDIOHAL_DTBS_BUF_SIZE;
		sg_set_buf(&p_data->sg_list[sg_count++],
			   p_data->dtbs_buf, sg_data_size);
		ttl_len += sg_data_size;
	}

	if (ttl_len % blk_size != 0) {
		pr_err("ttl_len %d not aligned to blk size\n", ttl_len);
		return -1;
	}

	sdiohal_debug("ttl len:%d sg_count:%d\n", ttl_len, sg_count);

	blk_num = ttl_len / blk_size;
	mmc_dat.sg = p_data->sg_list;
	mmc_dat.sg_len = sg_count;
	mmc_dat.blksz = blk_size;
	mmc_dat.blocks = blk_num;
	mmc_dat.flags = dir ? MMC_DATA_WRITE : MMC_DATA_READ;
	mmc_cmd.opcode = SD_IO_RW_EXTENDED;
	mmc_cmd.arg = dir ? 1<<31 : 0;
	mmc_cmd.arg |= (fn_num & 0x7) << 28;
	mmc_cmd.arg |= 1<<27;
	mmc_cmd.arg |= fifo ? 0 : 1<<26;
	mmc_cmd.arg |= (addr & 0x1FFFF) << 9;
	mmc_cmd.arg |= blk_num & 0x1FF;
	mmc_cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;
	mmc_req.cmd = &mmc_cmd;
	mmc_req.data = &mmc_dat;
	if (!fifo)
		addr += ttl_len;

	sdio_claim_host(sdio_func);
	mmc_set_data_timeout(&mmc_dat, sdio_func->card);
	mmc_wait_for_req(host, &mmc_req);
	sdio_release_host(sdio_func);

	err_ret = mmc_cmd.error ? mmc_cmd.error : mmc_dat.error;
	if (err_ret != 0) {
		pr_err("%s:CMD53 %s failed with code %d\n",
		       __func__, dir ? "write" : "read", err_ret);
		return -1;
	}

	return 0;
}

int sdiohal_adma_pt_write(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	ktime_get_real_ts64(&tm_begin);

	if (unlikely(p_data->card_dump_flag == true)) {
		pr_err("%s line %d dump happened\n", __func__, __LINE__);
		pr_err("tx_thread_enter_time=%llu,tx_thread_exit_time=%llu\n",
		       tm_enter_tx_thread, tm_exit_tx_thread);
		return -1;
	}

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_op_enter();
	ret = sdiohal_config_packer_chain(data_list,
					  p_data->sdio_func[FUNC_1],
					  SDIOHAL_DATA_FIX, SDIOHAL_WRITE,
					  SDIOHAL_PK_MODE_ADDR);
	if (ret != 0) {
		sdiohal_success_trans_pac_num();
		sdiohal_abort();
	}
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	ktime_get_real_ts64(&tm_end);
	time_total_ns += timespec64_to_ns(&tm_end) - timespec64_to_ns(&tm_begin);
	times_count++;
	if (!(times_count % PERFORMANCE_COUNT)) {
		sdiohal_pr_perf("tx avg time:%ld\n",
				(time_total_ns / PERFORMANCE_COUNT));
		time_total_ns = 0;
		times_count = 0;
	}

	return ret;
}

int sdiohal_adma_pt_read(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	ktime_get_real_ts64(&tm_begin);

	if (unlikely(p_data->card_dump_flag == true)) {
		pr_err("%s line %d dump happened\n", __func__, __LINE__);
		return -1;
	}

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_op_enter();
	ret = sdiohal_config_packer_chain(data_list,
					  p_data->sdio_func[FUNC_1],
					  SDIOHAL_DATA_FIX, SDIOHAL_READ,
					  SDIOHAL_PK_MODE_ADDR);
	if (ret != 0)
		sdiohal_abort();
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	ktime_get_real_ts64(&tm_end);
	time_total_ns += timespec64_to_ns(&tm_end) - timespec64_to_ns(&tm_begin);
	times_count++;
	if (!(times_count % PERFORMANCE_COUNT)) {
		sdiohal_pr_perf("rx avg time:%ld\n",
				(time_total_ns / PERFORMANCE_COUNT));
		time_total_ns = 0;
		times_count = 0;
	}

	return ret;
}

static int sdiohal_dt_set_addr(unsigned int addr)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned char address[4];
	int err = 0;
	int i;

	for (i = 0; i < 4; i++)
		address[i] = (addr >> (8 * i)) & 0xFF;

	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	sdio_writeb(p_data->sdio_func[FUNC_0], address[0],
		    SDIOHAL_FBR_SYSADDR0, &err);
	if (err != 0)
		goto exit;

	sdio_writeb(p_data->sdio_func[FUNC_0], address[1],
		    SDIOHAL_FBR_SYSADDR1, &err);
	if (err != 0)
		goto exit;

	sdio_writeb(p_data->sdio_func[FUNC_0], address[2],
		    SDIOHAL_FBR_SYSADDR2, &err);
	if (err != 0)
		goto exit;

	sdio_writeb(p_data->sdio_func[FUNC_0], address[3],
		    SDIOHAL_FBR_SYSADDR3, &err);
	if (err != 0)
		goto exit;

exit:
	sdio_release_host(p_data->sdio_func[FUNC_0]);

	return err;
}

static char *sdiohal_haddr[8] = {
	"cm4d",
	"cm4i",
	"cm4s",
	"dmaw",
	"dmar",
	"aon_to_ahb",
	"axi_to_ahb",
	"hready_status",
};

void sdiohal_dump_aon_reg(void)
{
	unsigned char reg_buf[16];
	unsigned char i, j, val = 0;

	pr_info("sdio dump_aon_reg entry\n");
	for (i = 0; i <= CP_128BIT_SIZE; i++) {
		sdiohal_aon_readb(CP_PMU_STATUS + i, &reg_buf[i]);
		pr_info("pmu sdio status:[0x%x]:0x%x\n",
			CP_PMU_STATUS + i, reg_buf[i]);
	}

	for (i = 0; i < 8; i++) {
		sdiohal_aon_readb(CP_SWITCH_SGINAL, &val);
		val &= ~BIT(4);
		sdiohal_aon_writeb(CP_SWITCH_SGINAL, val);

		/* bit3:0 bt wifi sys, 1:gnss sys */
		val &= ~(BIT(0) | BIT(1) | BIT(2) | BIT(3));
		val |= i;
		sdiohal_aon_writeb(CP_SWITCH_SGINAL, val);

		val |= BIT(4);
		sdiohal_aon_writeb(CP_SWITCH_SGINAL, val);

		for (j = 0; j < CP_HREADY_SIZE; j++) {
			sdiohal_aon_readb(CP_BUS_HREADY + j, &reg_buf[j]);
			pr_info("%s haddr %d:[0x%x]:0x%x\n",
				sdiohal_haddr[i], i,
				CP_BUS_HREADY + j, reg_buf[j]);
		}
	}

	/*
	 * check hready_status, if bt hung the bus, reset it.
	 * BIT(2):bt2 hready out
	 * BIT(3):bt2 hready
	 * BIT(4):bt1 hready out
	 * BIT(5):bt1 hready
	 */
	val = (reg_buf[0] & (BIT(2) | BIT(3) | BIT(4) | BIT(5)));
	pr_info("val:0x%x\n", val);
	if (val != 0xf) {
		sdiohal_aon_readb(CP_RESET_SLAVE, &val);
		val |=  BIT(5) | BIT(6);
		sdiohal_aon_writeb(CP_RESET_SLAVE, val);

		for (i = 0; i < CP_HREADY_SIZE; i++) {
			sdiohal_aon_readb(CP_BUS_HREADY + i, &reg_buf[i]);
			pr_info("after reset hready status:[0x%x]:0x%x\n",
				 CP_BUS_HREADY + i, reg_buf[i]);
		}
	}

	pr_info("sdio dump_aon_reg end\n\n");
}
EXPORT_SYMBOL_GPL(sdiohal_dump_aon_reg);

int sdiohal_writel(unsigned int system_addr, void *buf)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_cp_tx_wakeup(DT_WRITEL);
	sdiohal_op_enter();

	ret = sdiohal_dt_set_addr(system_addr);
	if (ret != 0) {
		sdiohal_op_leave();
		sdiohal_cp_tx_sleep(DT_WRITEL);
		sdiohal_card_unlock(p_data);
		return ret;
	}

	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	sdio_writel(p_data->sdio_func[FUNC_1],
		    *(unsigned int *)buf, SDIOHAL_DT_MODE_ADDR, &ret);

	sdio_release_host(p_data->sdio_func[FUNC_1]);
	sdiohal_op_leave();
	sdiohal_cp_tx_sleep(DT_WRITEL);

	if (ret != 0) {
		pr_err("dt writel fail ret:%d\n", ret);
		sdiohal_dump_aon_reg();
		sdiohal_abort();
	}
	sdiohal_card_unlock(p_data);

	return ret;
}

int sdiohal_readl(unsigned int system_addr, void *buf)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_cp_rx_wakeup(DT_READL);
	sdiohal_op_enter();
	ret = sdiohal_dt_set_addr(system_addr);
	if (ret != 0) {
		sdiohal_op_leave();
		sdiohal_cp_rx_sleep(DT_READL);
		sdiohal_card_unlock(p_data);
		return ret;
	}

	sdio_claim_host(p_data->sdio_func[FUNC_1]);

	*(unsigned int *)buf = sdio_readl(p_data->sdio_func[FUNC_1],
					  SDIOHAL_DT_MODE_ADDR, &ret);

	sdio_release_host(p_data->sdio_func[FUNC_1]);
	sdiohal_op_leave();
	sdiohal_cp_rx_sleep(DT_READL);
	if (ret != 0) {
		pr_err("dt readl fail ret:%d\n", ret);
		sdiohal_dump_aon_reg();
		sdiohal_abort();
	}
	sdiohal_card_unlock(p_data);

	return ret;
}

static int sdiohal_blksz_for_byte_mode(const struct mmc_card *c)
{
	return c->quirks & MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;
}

static int sdiohal_card_broken_byte_mode_512(
	const struct mmc_card *c)
{
	return c->quirks & MMC_QUIRK_BROKEN_BYTE_MODE_512;
}

static unsigned int max_bytes(struct sdio_func *func)
{
	unsigned int mval = func->card->host->max_blk_size;

	if (sdiohal_blksz_for_byte_mode(func->card))
		mval = min(mval, func->cur_blksize);
	else
		mval = min(mval, func->max_blksize);

	if (sdiohal_card_broken_byte_mode_512(func->card))
		return min(mval, 511u);

	/* maximum size for byte mode */
	return min(mval, 512u);
}

int sdiohal_dt_write(unsigned int system_addr,
			    void *buf, unsigned int len)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned int remainder = len;
	unsigned int trans_len;
	int ret;

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_cp_tx_wakeup(DT_WRITE);
	sdiohal_op_enter();

	ret = sdiohal_dt_set_addr(system_addr);
	if (ret != 0) {
		sdiohal_op_leave();
		sdiohal_cp_tx_sleep(DT_WRITE);
		sdiohal_card_unlock(p_data);
		return ret;
	}

	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	while (remainder > 0) {
		if (remainder >= p_data->sdio_func[FUNC_1]->cur_blksize)
			trans_len = p_data->sdio_func[FUNC_1]->cur_blksize;
		else
			trans_len = min(remainder,
					max_bytes(p_data->sdio_func[FUNC_1]));
		ret = sdio_memcpy_toio(p_data->sdio_func[FUNC_1],
				       SDIOHAL_DT_MODE_ADDR, buf, trans_len);
		if (ret)
			break;

		remainder -= trans_len;
		buf += trans_len;
	}
	sdio_release_host(p_data->sdio_func[FUNC_1]);
	sdiohal_op_leave();
	sdiohal_cp_tx_sleep(DT_WRITE);
	if (ret != 0) {
		pr_err("dt write fail ret:%d\n", ret);
		sdiohal_dump_aon_reg();
		sdiohal_abort();
	}
	sdiohal_card_unlock(p_data);

	return ret;
}

int sdiohal_dt_read(unsigned int system_addr, void *buf,
			   unsigned int len)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned int remainder = len;
	unsigned int trans_len;
	int ret;

	if (sdiohal_card_lock(p_data))
		return -ENODEV;

	sdiohal_resume_check();
	sdiohal_cp_rx_wakeup(DT_READ);
	sdiohal_op_enter();
	ret = sdiohal_dt_set_addr(system_addr);
	if (ret != 0) {
		sdiohal_op_leave();
		sdiohal_cp_rx_sleep(DT_READ);
		sdiohal_card_unlock(p_data);
		return ret;
	}

	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	while (remainder > 0) {
		if (remainder >= p_data->sdio_func[FUNC_1]->cur_blksize)
			trans_len = p_data->sdio_func[FUNC_1]->cur_blksize;
		else
			trans_len = min(remainder,
					max_bytes(p_data->sdio_func[FUNC_1]));
		ret = sdio_memcpy_fromio(p_data->sdio_func[FUNC_1],
					 buf, SDIOHAL_DT_MODE_ADDR, trans_len);
		if (ret)
			break;

		remainder -= trans_len;
		buf += trans_len;
	}
	sdio_release_host(p_data->sdio_func[FUNC_1]);
	sdiohal_op_leave();
	sdiohal_cp_rx_sleep(DT_READ);
	if (ret != 0) {
		pr_err("dt read fail ret:%d\n", ret);
		sdiohal_dump_aon_reg();
		sdiohal_abort();
	}
	sdiohal_card_unlock(p_data);

	return ret;
}

int sdiohal_aon_readb(unsigned int addr, unsigned char *val)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int err = 0;
	unsigned char reg_val = 0;

	if (sdiohal_card_lock(p_data))
		return -1;

	sdiohal_resume_check();
	sdiohal_op_enter();
	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	reg_val = sdio_readb(p_data->sdio_func[FUNC_0], addr, &err);
	if (val)
		*val = reg_val;
	sdio_release_host(p_data->sdio_func[FUNC_0]);
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	return err;
}

int sdiohal_aon_writeb(unsigned int addr, unsigned char val)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int err = 0;

	if (sdiohal_card_lock(p_data))
		return -1;

	sdiohal_resume_check();
	sdiohal_op_enter();
	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	sdio_writeb(p_data->sdio_func[FUNC_0], val, addr, &err);
	sdio_release_host(p_data->sdio_func[FUNC_0]);
	sdiohal_op_leave();
	sdiohal_card_unlock(p_data);

	return err;
}

unsigned long long sdiohal_get_rx_total_cnt(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	return p_data->rx_packer_cnt;
}

void sdiohal_set_carddump_status(unsigned int flag)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	pr_info("carddump flag set[%d]\n", flag);
	if (flag == true) {
		disable_irq(p_data->irq_num);
		pr_info("disable rx int for dump\n");
	}
	p_data->card_dump_flag = flag;
}

extern char wcn_assert_str[128];
unsigned int sdiohal_get_carddump_status(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (p_data->card_dump_flag != 0)
		pr_err("fw assert:%s\n", wcn_assert_str);

	return p_data->card_dump_flag;
}

static void sdiohal_disable_rx_irq(int irq)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_atomic_add(1, &p_data->irq_cnt);
	disable_irq_nosync(irq);
}

void sdiohal_enable_rx_irq(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_atomic_sub(1, &p_data->irq_cnt);
	irq_set_irq_type(p_data->irq_num, IRQF_TRIGGER_HIGH);
	/* WARNING: when the card is removed, sdiohal_remove():free(pdata->irq) */
	enable_irq(p_data->irq_num);
}

static irqreturn_t sdiohal_irq_handler(int irq, void *para)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_debug("%s entry\n", __func__);

	sdiohal_lock_rx_ws();
	sdiohal_disable_rx_irq(irq);

	ktime_get_real_ts64(&p_data->tm_begin_irq);
	sdiohal_rx_up();

	return IRQ_HANDLED;
}

static int sdiohal_enable_slave_irq(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int err;
	unsigned char reg_val;

	sdiohal_resume_check();
	sdiohal_op_enter();
	sdio_claim_host(p_data->sdio_func[FUNC_0]);
	reg_val = sdio_readb(p_data->sdio_func[FUNC_0],
			     SDIOHAL_FBR_DEINT_EN, &err);
	sdio_writeb(p_data->sdio_func[FUNC_0],
		    reg_val | VAL_DEINT_ENABLE, SDIOHAL_FBR_DEINT_EN, &err);
	reg_val = sdio_readb(p_data->sdio_func[FUNC_0],
			     SDIOHAL_FBR_DEINT_EN, &err);
	sdio_release_host(p_data->sdio_func[FUNC_0]);
	sdiohal_op_leave();

	return 0;
}

static int sdiohal_host_irq_init(unsigned int irq_gpio_num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;

	ret = gpio_request(irq_gpio_num, "sdiohal_gpio");
	if (ret < 0) {
		pr_err("req gpio irq = %d fail!!!", irq_gpio_num);
		return ret;
	}

	ret = gpio_direction_input(irq_gpio_num);
	if (ret < 0) {
		pr_err("gpio:%d input set fail!!!", irq_gpio_num);
		return ret;
	}

	p_data->irq_num = gpio_to_irq(irq_gpio_num);

	return 0;
}

static int sdiohal_get_dev_func(struct sdio_func *func)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (func->num >= SDIOHAL_MAX_FUNCS)
		return -1;

	if (func->num == 1) {
		p_data->sdio_func[FUNC_0] = kmemdup(func, sizeof(*func),
						    GFP_KERNEL);
		p_data->sdio_func[FUNC_0]->num = 0;
		p_data->sdio_func[FUNC_0]->max_blksize = SDIOHAL_BLK_SIZE;
	}

	p_data->sdio_func[FUNC_1] = func;

	return 0;
}

static struct mmc_host *sdiohal_dev_get_host(struct device_node *np_node)
{
	void *drv_data;
	struct mmc_host *host_mmc;
	struct platform_device *pdev;

	pdev = of_find_device_by_node(np_node);
	if (pdev == NULL) {
		pr_err("sdio dev get platform device failed!!!");
		return NULL;
	}

	drv_data = platform_get_drvdata(pdev);
	if (drv_data == NULL) {
		pr_err("sdio dev get drv data failed!!!");
		return NULL;
	}

	host_mmc = drv_data;
	pr_info("host_mmc:%p parent:%p, pdev->dev:%p\n",
		host_mmc, host_mmc->parent, &pdev->dev);

	if (host_mmc->parent == &pdev->dev)
		return host_mmc;
	else
		return container_of(drv_data, struct mmc_host, private);
}

static int sdiohal_parse_dt(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct device_node *np;
	struct device_node *sdio_node;

	np = of_find_node_by_name(NULL, "sprd-marlin3");
	if (!np) {
		pr_err("dts node not found");
		return -1;
	}

	if (of_get_property(np, "adma-tx", NULL))
		p_data->adma_tx_enable = true;

	if (of_get_property(np, "adma-rx", NULL))
		p_data->adma_rx_enable = true;

	pr_info("adma enable tx:%d, rx:%d\n",
		p_data->adma_tx_enable, p_data->adma_rx_enable);

	if (of_get_property(np, "pwrseq", NULL))
		p_data->pwrseq_enable = true;

	pr_info("%s sdio pwrseq enable:%d\n",
		__func__, p_data->pwrseq_enable);

	p_data->gpio_num = of_get_named_gpio(np, "m2-wakeup-ap-gpios", 0);

	sdio_node = of_parse_phandle(np, "sdhci-name", 0);
	if (sdio_node == NULL) {
		pr_err("get sdhci-name failed");
		return -1;
	}
	p_data->sdio_dev_host = sdiohal_dev_get_host(sdio_node);
	if (p_data->sdio_dev_host == NULL) {
		pr_err("get host failed!!!");
		return -1;
	}
	pr_info("get host ok!!!");

	return 0;
}

static int sdiohal_suspend(struct device *dev)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mchn_ops_t *sdiohal_ops;
	struct sdio_func *func;
	int chn;
	int ret;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	pr_debug("[%s]enter\n", __func__);

	atomic_set(&p_data->flag_suspending, 1);
	mdbg_device_lock_notify();
	for (chn = 0; chn < SDIO_CHANNEL_NUM; chn++) {
		sdiohal_ops = chn_ops(chn);
		if (sdiohal_ops && sdiohal_ops->power_notify) {
			if (g_match_config && g_match_config->unisoc_wcn_slp)
				sdio_record_power_notify(true);
			ret = sdiohal_ops->power_notify(chn, false);
			if (ret != 0) {
				pr_info("[%s] chn:%d suspend fail\n",
					__func__, chn);
				atomic_set(&p_data->flag_suspending, 0);
				mdbg_device_unlock_notify();
				return ret;
			}
		}
	}

	if (g_match_config && g_match_config->unisoc_wcn_slp) {
		sdio_wait_pub_int_done();
		sdio_record_power_notify(false);
	}

	atomic_set(&p_data->flag_resume, 0);
	atomic_set(&p_data->flag_suspending, 0);
	if (atomic_read(&p_data->irq_cnt))
		sdiohal_lock_rx_ws();

	if (WCN_CARD_EXIST(&p_data->xmit_cnt)) {
		func = container_of(dev, struct sdio_func, dev);
		func->card->host->pm_flags |= MMC_PM_KEEP_POWER;
	}
	mdbg_device_unlock_notify();

	/* WARNING: wait for sending to complete? */
	pr_info("[%s]done xmit_lock:%d\n", __func__, mutex_is_locked(&p_data->xmit_lock));
	return 0;
}

static int sdiohal_resume(struct device *dev)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mchn_ops_t *sdiohal_ops;
	struct sdio_func *func;
	int chn;
	int ret;

	pr_info("[%s]enter xmit_lock:%d\n", __func__, mutex_is_locked(&p_data->xmit_lock));

	if (WCN_CARD_EXIST(&p_data->xmit_cnt)) {
		func = container_of(dev, struct sdio_func, dev);
		func->card->host->pm_flags &= ~MMC_PM_KEEP_POWER;
	}

	atomic_set(&p_data->flag_resume, 1);
	wake_up_all(&p_data->resume_waitq);
	mdbg_device_lock_notify();

	for (chn = 0; chn < SDIO_CHANNEL_NUM; chn++) {
		sdiohal_ops = chn_ops(chn);
		if (sdiohal_ops && sdiohal_ops->power_notify) {
			ret = sdiohal_ops->power_notify(chn, true);
			if (ret != 0)
				pr_info("[%s] chn:%d resume fail\n",
					__func__, chn);
		}
	}
	mdbg_device_unlock_notify();
	pr_debug("[%s]done xmit_lock:%d\n", __func__, mutex_is_locked(&p_data->xmit_lock));

	return 0;
}

static int sdiohal_set_cp_pin_status(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
		/*
		 * Because of cp pin pull up on default, It's lead to
		 * the sdio mistaken interruption before cp run,
		 * So set the pin to no pull up on init.
		 */
		int reg_value;

		sdiohal_readl(CP_GPIO1_REG, &reg_value);
		pr_info("reg_value: 0x%x\n", reg_value);
		reg_value &= ~(CP_PIN_FUNC_WPU);
		sdiohal_writel(CP_GPIO1_REG, &reg_value);

		sdiohal_readl(CP_GPIO1_REG, &reg_value);
		pr_info("reg_value: 0x%x\n", reg_value);
	}
	return 0;
}

int sdiohal_runtime_get(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	int ret;

	if (!p_data)
		return -ENODEV;

	if (!p_data->pwrseq_enable) {
		if (p_data->irq_num != 0) {
			enable_irq(p_data->irq_num);
			pr_info("%s enable irq ok\n", __func__);
		}
		return 0;
	}

	ret = pm_runtime_get_sync(&p_data->sdio_func[FUNC_1]->dev);
	if (ret < 0) {
		pr_err("sdiohal_rumtime_get err: %d", ret);
		return ret;
	}

	/* Enable Function 1 */
	sdio_claim_host(p_data->sdio_func[FUNC_1]);
	ret = sdio_enable_func(p_data->sdio_func[FUNC_1]);
	sdio_set_block_size(p_data->sdio_func[FUNC_1], SDIOHAL_BLK_SIZE);
	p_data->sdio_func[FUNC_1]->max_blksize = SDIOHAL_BLK_SIZE;
	sdio_release_host(p_data->sdio_func[FUNC_1]);
	if (ret < 0) {
		pr_err("%s enable func1 err!!! ret is %d", __func__, ret);
		return ret;
	}

	sdiohal_set_cp_pin_status();
	sdiohal_enable_slave_irq();
	enable_irq(p_data->irq_num);
	pr_info("%s enable device ok\n", __func__);

	return ret;
}

int sdiohal_runtime_put(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	pr_info("%s disable device\n", __func__);

	if (!p_data)
		return -ENODEV;

	if (p_data->irq_num != 0)
		disable_irq(p_data->irq_num);

	if (!p_data->pwrseq_enable)
		return 0;

	return pm_runtime_put_sync(&p_data->sdio_func[FUNC_1]->dev);
}

static int sdiohal_probe(struct sdio_func *func,
	const struct sdio_device_id *id)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int ret;

	pr_info("%s func num is %d!!!", __func__, func->num);

	ret = sdiohal_get_dev_func(func);
	if (ret < 0)
		return ret;

	if (!p_data->pwrseq_enable) {
		/* Enable Function 1 */
		sdio_claim_host(p_data->sdio_func[FUNC_1]);
		ret = sdio_enable_func(p_data->sdio_func[FUNC_1]);
		sdio_set_block_size(p_data->sdio_func[FUNC_1],
				    SDIOHAL_BLK_SIZE);
		p_data->sdio_func[FUNC_1]->max_blksize = SDIOHAL_BLK_SIZE;
		sdio_release_host(p_data->sdio_func[FUNC_1]);
		if (ret < 0)
			return ret;

		pr_info("enable sdio func1 ok\n");

		sdiohal_enable_slave_irq();
	} else
		pm_runtime_put_noidle(&func->dev);

	if (!WCN_CARD_EXIST(&p_data->xmit_cnt))
		atomic_sub(SDIOHAL_REMOVE_CARD_VAL, &p_data->xmit_cnt);

	sdiohal_set_cp_pin_status();

	ret = request_irq(p_data->irq_num, sdiohal_irq_handler,
			  IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			  "sdiohal_irq", &func->dev);
	if (ret != 0) {
		pr_err("request irq err gpio is %d\n", p_data->irq_num);
		return ret;
	}

	disable_irq(p_data->irq_num);
	complete(&p_data->scan_done);

	/* the card is nonremovable */
	p_data->sdio_dev_host->caps |= MMC_CAP_NONREMOVABLE;

	/* calling rescan callback to inform download */
	if (scan_card_notify != NULL)
		scan_card_notify();

	pr_info("%s scaffcard successful! nodify callback:%p\n",
		__func__, scan_card_notify);

	return 0;
}

static void sdiohal_remove(struct sdio_func *func)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (WCN_CARD_EXIST(&p_data->xmit_cnt))
		atomic_add(SDIOHAL_REMOVE_CARD_VAL, &p_data->xmit_cnt);

	complete(&p_data->remove_done);

	if (p_data->irq_num != 0)
		free_irq(p_data->irq_num, &func->dev);

	pr_info("%s remove card successful\n", __func__);
}

/*
 *    @dir: 0 rx, 1 tx
 */
int wcn_thread_setattr(unsigned int dir, struct sched_attr *attr)
{
	int ret;
	struct task_struct *target;
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	pr_info("%s: dir %u\n", __func__, dir);

	if (!attr)
		return -EINVAL;

	target = dir ? p_data->tx_thread : p_data->rx_thread;

	ret = sched_setattr(target, attr);
	if (ret) {
		pr_err("%s err %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wcn_thread_setattr);

static void sdiohal_launch_thread(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->tx_thread = kthread_create(sdiohal_tx_thread,
				   NULL, "sdiohal_tx_thread");
	if (p_data->tx_thread)
		wake_up_process(p_data->tx_thread);
	else {
		pr_err("create sdiohal_tx_thread fail\n");
		return;
	}

	p_data->rx_thread = kthread_create(sdiohal_rx_thread,
				   NULL, "sdiohal_rx_thread");
	if (p_data->rx_thread)
		wake_up_process(p_data->rx_thread);
	else
		pr_err("creat sdiohal_rx_thread fail\n");
}

static const struct dev_pm_ops sdiohal_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdiohal_suspend, sdiohal_resume)
};

static const struct sdio_device_id sdiohal_ids[] = {
	{SDIO_DEVICE(0, 0)},
	{},
};

static struct sdio_driver sdiohal_driver = {
	.probe = sdiohal_probe,
	.remove = sdiohal_remove,
	.name = "sdiohal",
	.id_table = sdiohal_ids,
	.drv = {
		.pm = &sdiohal_pm_ops,
	},
};

#define WCN_SDIO_CARD_REMOVED	BIT(4)
void sdiohal_remove_card(void *wcn_dev)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (!WCN_CARD_EXIST(&p_data->xmit_cnt))
		return;

	atomic_add(SDIOHAL_REMOVE_CARD_VAL, &p_data->xmit_cnt);
	sdiohal_lock_scan_ws();
	sdiohal_resume_check();
	while (atomic_read(&p_data->xmit_cnt) > SDIOHAL_REMOVE_CARD_VAL)
		usleep_range_state(4000, 6000, TASK_UNINTERRUPTIBLE);

	init_completion(&p_data->remove_done);
	p_data->sdio_dev_host->card->state |= WCN_SDIO_CARD_REMOVED;

	/* enable remove the card */
	p_data->sdio_dev_host->caps &= ~MMC_CAP_NONREMOVABLE;

	mmc_detect_change(p_data->sdio_dev_host, 0);
	if (wait_for_completion_timeout(&p_data->remove_done,
					msecs_to_jiffies(5000)) == 0)
		pr_err("remove card time out\n");
	else
		pr_info("remove card end\n");

	sdio_unregister_driver(&sdiohal_driver);
	sdiohal_unlock_scan_ws();
}

int sdiohal_scan_card(void *wcn_dev)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	pr_info("%s\n", __func__);

	if (!p_data->sdio_dev_host) {
		pr_err("sdio_sdio_rescan get host failed!\n");
		return -1;
	}

	if (WCN_CARD_EXIST(&p_data->xmit_cnt)) {
		pr_info("Already exist card!\n");
		sdiohal_remove_card(wcn_dev);
		msleep(100);
	}

	sdiohal_lock_scan_ws();
	sdiohal_resume_check();
	init_completion(&p_data->scan_done);
	sdio_register_driver(&sdiohal_driver);
	mmc_detect_change(p_data->sdio_dev_host, 0);
	if (wait_for_completion_timeout
		(&p_data->scan_done,
		 msecs_to_jiffies(2500)) == 0) {
		sdiohal_unlock_scan_ws();
		pr_err("wait scan card time out\n");
		return -1;
	}

	sdiohal_unlock_scan_ws();
	pr_info("scan end\n");

	return 0;
}

void sdiohal_register_scan_notify(void *func)
{
	scan_card_notify = func;
}

int sdiohal_init(void)
{
	struct sdiohal_data_t *p_data;
	int ret;

	p_data = kzalloc(sizeof(struct sdiohal_data_t), GFP_KERNEL);
	if (!p_data) {
		WARN_ON_ONCE(1);
		return -ENOMEM;
	}
	sdiohal_data = p_data;

	if (sdiohal_parse_dt() < 0) {
		kfree(p_data);
		return -1;
	}

	ret = sdiohal_misc_init();
	if (ret != 0) {
		kfree(p_data);
		pr_err("sdiohal_misc_init error :%d\n", ret);
		return -1;
	}

	sdiohal_launch_thread();
	sdiohal_host_irq_init(p_data->gpio_num);
	p_data->flag_init = true;
	/* card not ready */
	atomic_set(&p_data->xmit_cnt, SDIOHAL_REMOVE_CARD_VAL);
#if 0
#ifdef CONFIG_DEBUG_FS
	sdiohal_debug_init();
#endif
#endif

	pr_info("%s sdiohal driver init successful\n", __func__);

	return 0;
}

void sdiohal_exit(void)
{
	sdio_unregister_driver(&sdiohal_driver);
#if 0
#ifdef CONFIG_DEBUG_FS
	sdiohal_debug_deinit();
#endif
#endif
	sdiohal_misc_deinit();
	if (sdiohal_data) {
		sdiohal_data->sdio_dev_host = NULL;
		sdiohal_data->flag_init = false;
		kfree(sdiohal_data);
		sdiohal_data = NULL;
	}

	pr_info("%s sdiohal driver exit successful\n", __func__);
}
