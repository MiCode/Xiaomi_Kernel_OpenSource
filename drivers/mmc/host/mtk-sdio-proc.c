/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/mmc/host.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>

#define SDIO_CCCR_MTK_DDR208    0xF2
#define SUCCESS		0
#define FAIL		1

enum {
	Read = 0,
	Write = 1,
	Tune = 2,
	Hqa_read = 3,
	Hqa_write = 4,
	Reset = 5,
	Stress_read = 6,
	Stress_write = 7,
	Stress_test = 8,
	Func_ctrl = 9
};

static struct mmc_host *host;

/**
 * define some count timer.
 */
#define KAL_TIME_INTERVAL_DECLARATION()  struct timeval __rTs, __rTe
#define KAL_REC_TIME_START()             do_gettimeofday(&__rTs)
#define KAL_REC_TIME_END()	             do_gettimeofday(&__rTe)
#define KAL_GET_TIME_INTERVAL() \
((__rTe.tv_sec * USEC_PER_SEC + __rTe.tv_usec) - \
(__rTs.tv_sec * USEC_PER_SEC + __rTs.tv_usec))

/**
 * sdio_proc_show dispaly the common cccr and cis.
 */
static int sdio_proc_show(struct seq_file *m, void *v)
{
	struct mmc_card *card;
	struct sdio_cccr cccr;

	WARN_ON(!host);
	card = host->card;
	cccr = card->cccr;

	seq_puts(m, "\n=========================================\n");
	seq_puts(m, "common cccr:\n");
	seq_printf(m, "sdio_vsn:%x, sd_vsn:%x, multi_block%x.\n"
			"low_speed:%x, wide_bus:%x, hight_power:%x.\n"
			"high_speed:%x, disable_cd:%x.\n",
			cccr.sdio_vsn, cccr.sd_vsn, cccr.multi_block,
			cccr.low_speed, cccr.wide_bus, cccr.high_power,
			cccr.high_speed, cccr.disable_cd);

	seq_puts(m, "common cis:\n");
	seq_printf(m, "vendor: %x, device:%x, blksize:%x, max_dtr:%x\n",
			card->cis.vendor, card->cis.device,
			card->cis.blksize, card->cis.max_dtr);

	seq_puts(m, "read cmd format:\n");
	seq_puts(m, "echo 0 0xReg 0xfunction> /proc/sdio\n");
	seq_puts(m, "write cmd format:\n");
	seq_puts(m, "echo 1 0xReg 0xfunction 0xValue> /proc/sdio\n");
	seq_puts(m, "tune cmd format:\n");
	seq_puts(m, "echo 2 0xloop_cycles > /proc/sdio\n");
	seq_puts(m, "multi read cmd format:\n");
	seq_puts(m, "echo 3 0x13 0x0 > /proc/sdio\n");
	seq_puts(m, "multi write cmd format:\n");
	seq_puts(m, "echo 4 0x13 0x0 0xvalue > /proc/sdio\n");
	seq_puts(m, "Notice:value is the read result!\n");
	seq_puts(m, "reset sdio cmd format:\n");
	seq_puts(m, "echo 5 0x0 0x0 > /proc/sdio\n");
	seq_puts(m, "throughput read cmd format:\n");
	seq_puts(m, "echo 6 0xb0 0x1 > /proc/sdio\n");
	seq_puts(m, "throughtput write cmd format:\n");
	seq_puts(m, "echo 7 0xb0 0x1 > /proc/sdio\n");
	seq_puts(m, "stress test cmd format:\n");
	seq_puts(m, "echo 8 0x0 0x1 > /proc/sdio\n");
	seq_puts(m, "function enable cmd format:\n");
	seq_puts(m, "echo 9 0x0/0x1 0xfunc_number > /proc/sdio\n");
	seq_puts(m, "=========================================\n");

	return 0;
}

static int sdio_tuning(void)
{
	int err = 0;

	err = mmc_send_tuning(host, MMC_SEND_TUNING_BLOCK, NULL);
	return err;
}

/*
 * This function can be used to do HQA.SDIO SHPA for read.
 * HQA: for SDR104 mode, it is better to set 0xb8(func1) to 0x5a.
 * HQA: for SD3.0 plus, it is better to set 0xb8(func1) to 0x33cc.
 */
static int multi_read(struct sdio_func *func, struct mmc_host *host)
{
	int err, i;
	u32 data, value, count_rw = 0;
	unsigned long total = 0;
	unsigned char *fac_buf = NULL;

	KAL_TIME_INTERVAL_DECLARATION();
	__kernel_suseconds_t usec;

	func->cur_blksize = 0x200;
	fac_buf = kmalloc(0x400, GFP_KERNEL);
	if (!fac_buf)
		return FAIL;

	/* This function can be used to do HQA.SDIO SHPA */
	data = sdio_f0_readb(func, SDIO_CCCR_MTK_DDR208, &err);
	if (err) {
		kfree(fac_buf);
		return FAIL;
	} else if ((data & 0x3) == 0x3) {
		value = 0x33cc33cc;
		err = sdio_writesb(func, 0x0, &value, 0x4);
		dev_info(mmc_dev(host), "[sd3.0+]: value is 0x33cc\n");
	} else {
		value = 0x5a5a5a5a;
		//err = sdio_writesb(func, 0x0, &value, 0x4);
		dev_info(mmc_dev(host), "[sd3.0]: value is 0x5a5a\n");
	}
	//value = 0;
	//err = sdio_readsb(func, &value, 0x0, 0x4);

	i = 0;
	/* byte to bit */
	total = 0x400 * 0x10;
	do {
		if (i % 0x10 == 0)
			KAL_REC_TIME_START();
		//dev_info(mmc_dev(host),
		//"[sd3.0]: value is 0x5a5a try read\n");
		err = sdio_readsb(func, fac_buf, 0x0, 0x400);
		//dev_info(mmc_dev(host),
		//"[sd3.0]: value is 0x5a5a try read end\n");
		if (err)
			count_rw = count_rw + 1;
		i = i + 1;
		if ((i / 0x10) && (i % 0x10 == 0)) {
			KAL_REC_TIME_END();
			usec = KAL_GET_TIME_INTERVAL();
			dev_info(mmc_dev(host), "read: %lu Kbps, err:%x.\n",
				total / (usec / USEC_PER_MSEC), count_rw);
		}
	} while (i < 0x10);

	kfree(fac_buf);

	return SUCCESS;
}

/**
 * This function can be used to do de-sense.HQA.SHPA for write.
 * HQA: for SDR104 mode, it is better to set 0xb8(func1) to 0x5a.
 * HQA: for SD3.0 plus, it is better to set 0xb8(func1) to 0x33cc.
 * de_sense: data is random or not.
 */
static int multi_write(struct sdio_func *func, struct mmc_host *host,
		int de_sense)
{
	int err, i;
	u32 data, value, count_rw = 0;
	unsigned long total = 0;
	unsigned char *fac_buf = NULL;

	KAL_TIME_INTERVAL_DECLARATION();
	__kernel_suseconds_t usec;

	func->cur_blksize = 0x200;
	fac_buf = kmalloc(0x400, GFP_KERNEL);
	if (!fac_buf)
		return FAIL;

	/* This function can be used to do de-sense.HQA.SHPA */
	data = sdio_f0_readb(func, SDIO_CCCR_MTK_DDR208, &err);
	if (err) {
		kfree(fac_buf);
		return FAIL;
	}
	if (de_sense) {
		//err = wait_for_random_bytes();
		if (!err)
			dev_info(mmc_dev(host),
				"wait random bytes success.\n");
		//get_random_bytes(fac_buf, 0x10000);
	} else if ((err & 0x3) == 0x3) {
		value = 0x33cc33cc;
		err = sdio_writesb(func, 0x0, &value, 0x400);
		err = sdio_readsb(func, fac_buf, 0x0, 0x400);
		dev_info(mmc_dev(host), "[sd3.0+]: value is 0x33cc\n");
	} else {
		memset(fac_buf, 0x5a, 0x400);
		dev_info(mmc_dev(host), "[sd3.0]: value is 0x5a5a\n");
	}

	i = 0;
	/* byte to bit */
	total = 0x400 * 0x10;
	do {
		if (i % 0x10 == 0)
			KAL_REC_TIME_START();
		err = sdio_writesb(func, 0x0, fac_buf, 0x400);
		if (err)
			count_rw = count_rw + 1;
		i = i + 1;
		if ((i / 0x10) && (i % 0x10 == 0)) {
			KAL_REC_TIME_END();
			usec = KAL_GET_TIME_INTERVAL();
			dev_info(mmc_dev(host), "write: %lu Kbps, err:%x\n",
				total / (usec / USEC_PER_MSEC), count_rw);
		}
	} while (i < 0x10);

	kfree(fac_buf);

	return SUCCESS;
}

static int sdio_stress_test(struct sdio_func *func, struct mmc_host *host)
{
	int ret;

	while (1) {
		ret = multi_write(func, host, 0);
		sdio_release_host(func);
		if (ret)
			return ret;
		msleep(20);
		sdio_claim_host(func);
		ret = multi_read(func, host);
		if (ret)
			return ret;
	}
}

/**
 * sdio_proc_write - read/write sdio function register.
 */
static ssize_t sdio_proc_write(struct file *file, const char *buf,
		size_t count, loff_t *f_pos)
{
	struct mmc_card *card;
	struct sdio_func *func;
	struct mmc_ios *ios;
	char *cmd_buf, *str_hand;
	unsigned char *fac_buf = NULL;
	unsigned int cmd, addr, fn, value, hqa_result, offset, option;
	unsigned char result;
	int i = 0, ret;
	unsigned long count_r = 0, count_w = 0, total = 0;

	KAL_TIME_INTERVAL_DECLARATION();
	__kernel_suseconds_t usec;

	WARN_ON(!host);
	ios = &host->ios;
	card = host->card;

	cmd_buf = kzalloc((count + 1), GFP_KERNEL);
	if (!cmd_buf)
		return count;

	str_hand = kzalloc(2, GFP_KERNEL);
	if (!str_hand) {
		kfree(cmd_buf);
		return count;
	}

	func = kzalloc(sizeof(struct sdio_func), GFP_KERNEL);
	if (!func) {
		kfree(cmd_buf);
		kfree(str_hand);
		return count;
	}

	ret = copy_from_user(cmd_buf, buf, count);
	if (ret < 0)
		goto end;

	*(cmd_buf + count) = '\0';
	ret = sscanf(cmd_buf, "%x %x %x %x %x %x",
			&cmd, &addr, &fn, &value, &offset, &option);
	if (ret == 0) {
		ret = sscanf(cmd_buf, "%s", str_hand);
		if (ret == 0)
			dev_info(mmc_dev(host), "please enter cmd.\n");

		goto end;
	}

	if (cmd == Tune)
		fn = 0;

	/* Judge whether request fn is over the max functions. */
	if (fn > card->sdio_funcs) {
		dev_info(mmc_dev(host), "the fn is over the max sdio funcs.\n");
		goto end;
	}

	if (fn) {
		/**
		 * The test read/write api don't need more func
		 * information. So we just use the card & func->num
		 * to the new struct func.
		 */
		if (card->sdio_func[fn - 1]) {
			func->card = card;
			func->num = card->sdio_func[fn - 1]->num;
			func->tuples = card->sdio_func[fn - 1]->tuples;
			func->tmpbuf = card->sdio_func[fn - 1]->tmpbuf;
			hqa_result = card->sdio_func[fn - 1]->max_blksize;
			func->max_blksize = hqa_result;
			if ((cmd == Hqa_read) || (cmd == Hqa_write)) {
				func->cur_blksize = 8;
			} else if ((cmd == Stress_write) ||
					(cmd == Stress_read)) {
				func->cur_blksize = 0x200;
				fac_buf = kmalloc(0x40000, GFP_KERNEL);
				if (!fac_buf)
					goto end;
				memset(fac_buf, 0x3c, 0x40000);
			} else {
				func->cur_blksize = 1;
			}
		} else {
			dev_info(mmc_dev(host), "func %d is null,.\n", fn);
		}
	} else {
		/**
		 * function 0 should not need struct func.
		 * but the api need the parameter, so we create
		 * the a new func for function 0.
		 */
		func->card = card;
		func->tuples = card->tuples;
		func->num = 0;
		func->max_blksize = 16;
		if ((cmd == Hqa_read) || (cmd == Hqa_write))
			func->cur_blksize = 16;
		else
			func->cur_blksize = 1;

		func->tmpbuf = kmalloc(func->cur_blksize, GFP_KERNEL);
		if (!func->tmpbuf)
			goto end;
		memset(func->tmpbuf, 0, func->cur_blksize);
	}

	sdio_claim_host(func);

	switch (cmd) {
	case Read:
		dev_info(mmc_dev(host), "read addr:%x, fn:%d.\n", addr, fn);
		ret = 0;
		if (fn == 0)
			result = sdio_f0_readb(func, addr, &ret);
		else
			result = sdio_readb(func, addr, &ret);

		if (ret)
			dev_info(mmc_dev(host), "Read fail(%d).\n", ret);
		else
			dev_info(mmc_dev(host), "f%d reg(%x) is 0x%02x.\n",
					func->num, addr, result);
		break;
	case Write:
		dev_info(mmc_dev(host), "write addr:%x, value:%x, fn:%d.\n",
				addr, (u8)value, fn);
		ret = 0;
		if (fn == 0)
			/* (0xF0 - 0xFF) are permiited for func0 */
			sdio_f0_writeb(func, (u8)value, addr, &ret);
		else
			sdio_writeb(func, (u8)value, addr, &ret);

		if (ret)
			dev_info(mmc_dev(host), "write fail(%d).\n", ret);
		else
			dev_info(mmc_dev(host), "write success(%d).\n", ret);

		break;
	case Tune:
		value = 0;

		do {
			result = sdio_tuning();
			if (result)
				value = value + 1;

			i = i + 1;
		} while (i < 64);
		if (value)
			dev_info(mmc_dev(host), "test fail.\n");
		else
			dev_info(mmc_dev(host), "test well.\n");
		break;
	case Hqa_read:
		dev_info(mmc_dev(host), "read addr:%x, fn %d\n", addr, fn);
		i = 0;
		hqa_result = 0;
		do {
			ret = 0;
			hqa_result = sdio_readl(func, addr, &ret);
			if (ret)
				dev_info(mmc_dev(host), "Read f%d reg(%x) fail(%d).\n",
						func->num, addr, ret);
			i = i + 1;
		} while (i < 0x10000);
		dev_info(mmc_dev(host), "Read result: 0x%02x.\n", hqa_result);
		break;
	case Hqa_write:
		dev_info(mmc_dev(host), "write addr:%x, value:%x, fn %d\n",
				addr, value, fn);
		i = 0;
		hqa_result = 0;
		do {
			ret = 0;
			sdio_writel(func, value, addr, &ret);
			if (ret)
				dev_info(mmc_dev(host), "write f%d reg(%x) fail(%d).\n",
						func->num, addr, ret);
			i = i + 1;
		} while (i < 0x10000);
		dev_info(mmc_dev(host), "write success(%d).\n", ret);
		break;
	case Reset:
		mmc_hw_reset(host);
		break;
	case Stress_read:
		i = 0;
		/* byte to bit */
		total = 0x6400000 * 8;
		dev_info(mmc_dev(host), "START test SDIO read throughput\n");
		KAL_REC_TIME_START();
		do {
			ret = sdio_readsb(func, fac_buf, addr, 0x40000);
			if (ret)
				count_r = count_r + 1;
			i = i + 1;
		} while (i < 0x1900);
		KAL_REC_TIME_END();
		usec = KAL_GET_TIME_INTERVAL();
		dev_info(mmc_dev(host), "%lu Kbps, err:%lx\n",
				total / (usec / USEC_PER_MSEC) * 0x10, count_r);
		break;
	case Stress_write:
		i = 0;
		/* byte to bit */
		total = 0x6400000 * 8;
		dev_info(mmc_dev(host), "START test SDIO write throughput\n");
		KAL_REC_TIME_START();
		do {
			ret = sdio_writesb(func, addr, fac_buf, 0x40000);
			if (ret)
				count_w = count_w + 1;
			i = i + 1;
		} while (i < 0x1900);
		KAL_REC_TIME_END();
		usec = KAL_GET_TIME_INTERVAL();
		dev_info(mmc_dev(host), "%lu Kbps, err:%lx\n",
				total / (usec / USEC_PER_MSEC) * 0x10, count_w);
		break;
	case Stress_test:
		ret = sdio_stress_test(func, host);
		if (ret) {
			dev_info(mmc_dev(host), "IO Stress Test Fail!!!\n");
			sdio_release_host(func);
			goto end;
		}
		break;
	case Func_ctrl:
		/* addr is func ctrl value in this case*/
		if (addr)
			ret = sdio_enable_func(func);
		else
			ret = sdio_disable_func(func);
		break;
	default:
		dev_info(mmc_dev(host), "cmd is not valid.\n");
		break;
	}

	sdio_release_host(func);

end:
	kfree(str_hand);
	kfree(cmd_buf);
	kfree(func);

	return count;
}

/**
 * open function show some stable information.
 */
static int sdio_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdio_proc_show, inode->i_private);
}

/**
 * sdio pre is our own function.
 * seq or single pre is the kernel function.
 */
static const struct file_operations sdio_proc_fops = {
	.owner = THIS_MODULE,
	.open = sdio_proc_open,
	.release = single_release,
	.write = sdio_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
};

int sdio_proc_init(struct mmc_host *host_init)
{
	struct proc_dir_entry *prEntry;

	host = host_init;

	prEntry = proc_create("sdio", 0660, NULL, &sdio_proc_fops);
	if (prEntry)
		dev_info(mmc_dev(host), "/proc/sdio is created.\n");
	else
		dev_info(mmc_dev(host), "create /proc/sdio failed.\n");

	return 0;
}
