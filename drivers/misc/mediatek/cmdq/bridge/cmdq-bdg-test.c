// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of.h>

#include "cmdq-bdg.h"

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include <linux/soc/mediatek/mtk-cmdq.h>
#else
#include <cmdq_record.h>
#endif

#define SYSBUF_BASE		0xA000
#define SYSBUF_SIZE		0x2000

#define GCE_BASE		0x10000
#define CMDQ_GPR_R32(base, id)	((base) + (0x4 * (id)) + 0x80)

struct cmdq_bdg_test {
	struct device		*dev;
	struct cmdq_client	*clt;
	struct cmdq_client	*clt2;
	u16			token_user0;
	u16			token_gpr_set4;
};

enum flush_flag {sync, async, threaded};

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static void cmdq_bdg_test_mbox_async_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	if (cmplt) {
		cmplt->err = data.err;
		cmdq_msg("cmplt:%p pkt:%p err:%d", cmplt, cmplt->pkt, data.err);
		complete(&cmplt->cmplt);
	}
}

static void cmdq_bdg_test_mbox_threaded_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;

	cmdq_msg("pkt:%p err:%d", pkt, data.err);
	cmdq_pkt_destroy(pkt);
}
#else
static void cmdq_bdg_test_rec_async_cb(void *data)
{
	struct cmdqRecStruct *rec = (struct cmdqRecStruct *)data;

	cmdq_msg("%s: rec:%p pkt:%p priority:%d thread:%d",
		__func__, rec, rec->pkt, rec->pkt->priority, rec->thread);
	cmdqRecDestroy(rec);
}
#endif

static void cmdq_bdg_test_mbox_write(struct cmdq_bdg_test *test,
	const u32 count, const bool masked, const bool reuse)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	struct cmdq_pkt *pkt;
#else
	struct cmdqRecStruct *rec;
#endif
	const dma_addr_t pa = CMDQ_GPR_R32(GCE_BASE, CMDQ_GPR_R15);
	const u32 pttn = BIT(15) | count;
	const u32 mask = masked ? BIT(15) | BIT(7) | BIT(1) : UINT_MAX;
	s32 val, i;

	spi_write_reg(pa, masked ? 0x0 : 0xdeaddead);
	val = spi_read_reg(pa);
	cmdq_msg("%s: masked:%d reuse:%d pa:%pa val:%#x pttn:%#x mask:%#x",
		__func__, masked, reuse, &pa, val, pttn, mask);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	pkt = cmdq_pkt_create(test->clt);
	cmdq_pkt_write(pkt, NULL, pa, pttn, mask);
	for (i = 0; i < count; i++)
		cmdq_pkt_write(pkt, NULL, pa, BIT(15) | (i + 1), mask);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);
#else
	cmdqRecCreate(CMDQ_BDG_SCENARIO_DISP_TEST, &rec);

	for (i = 0; i < count; i++)
		cmdqRecWrite(rec, pa, BIT(15) | (i + 1), mask);

	rec->pkt->reuse = true;
	for (i = 0; reuse && i < count; i++) {
		cmdq_msg("%s: rec:%p pkt:%p reuse:%d:%d scenario:%d thread:%d",
			__func__, rec, rec->pkt, i, rec->pkt->reuse,
			rec->scenario, rec->thread);
		cmdqRecFlush(rec);
	}

	rec->pkt->reuse = false;
	cmdq_msg("%s: rec:%p pkt:%p reuse:%d:%d scenario:%d thread:%d",
		__func__, rec, rec->pkt, i, rec->pkt->reuse,
		rec->scenario, rec->thread);

	cmdqRecFlush(rec);
	cmdqRecDestroy(rec);
#endif

	val = spi_read_reg(pa);
	if (val != (pttn & mask))
		cmdq_err("masked:%d pa:%pa val:%#x ans:%#x",
			masked, &pa, val, pttn & mask);
	else
		cmdq_msg("%s: masked:%d pa:%pa val:%#x ans:%#x",
			__func__, masked, &pa, val, pttn & mask);
}

static int cmdq_bdg_test_mbox_token(void *data)
{
	struct cmdq_bdg_test *test = (struct cmdq_bdg_test *)data;

	spi_write_reg(GCE_BASE + 0x68, BIT(16) | test->token_user0);
	return 0;
}

static void cmdq_bdg_test_mbox_tasks(struct cmdq_bdg_test *test,
	const enum flush_flag flag, const u32 count, const bool error)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	struct cmdq_pkt *pkt[count];
	struct cmdq_flush_completion cmplt[count];
#else
	struct cmdqRecStruct *rec[count];
#endif
	const dma_addr_t pa = CMDQ_GPR_R32(GCE_BASE, CMDQ_GPR_R15);
	s32 val, err = UINT_MAX, i;

	if (flag == sync) {
		cmdq_err("%s: count:%u flag:%d", __func__, count, flag);
		return;
	}

	spi_write_reg(pa, 0xdeaddead);
	spi_write_reg(GCE_BASE + 0x68, test->token_user0);
	val = spi_read_reg(pa);
	cmdq_msg("%s: count:%u flag:%d pa:%pa val:%#x err:%d",
		__func__, count, flag, &pa, val, err);

	msleep_interruptible(100);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	for (i = 0; i < count; i++) {
		pkt[i] = cmdq_pkt_create(test->clt);
		pkt[i]->priority = i;

		cmdq_msg("%s: i:%d/%d pkt:%p prio:%d",
			__func__, i, count, pkt[i], pkt[i]->priority);

		cmdq_pkt_wfe(pkt[i], test->token_user0);
		cmdq_pkt_write(pkt[i], NULL, pa, count - i, UINT_MAX);

		if (flag == async) {
			init_completion(&cmplt[i].cmplt);
			cmplt[i].pkt = pkt[i];
			cmdq_pkt_flush_async(pkt[i],
				cmdq_bdg_test_mbox_async_cb, &cmplt[i]);
		} else if (flag == threaded)
			cmdq_pkt_flush_threaded(pkt[i],
				cmdq_bdg_test_mbox_threaded_cb, (void *)pkt[i]);
	}
#else
	for (i = 0; i < count; i++) {
		cmdqRecCreate(CMDQ_BDG_SCENARIO_DISP_TEST, &rec[i]);
		rec[i]->pkt->priority = i;
		cmdq_msg("%s: i:%d rec:%p pkt:%p priority:%d thread:%d",
			__func__, i, rec[i],
			rec[i]->pkt, rec[i]->pkt->priority, rec[i]->thread);

		cmdqRecWait(rec[i], test->token_user0);
		cmdqRecWrite(rec[i], pa, count - i, UINT_MAX);

		cmdqRecFlushAsyncCallback(rec[i],
			(void *)cmdq_bdg_test_rec_async_cb,
			(unsigned long)(void *)rec[i]);
	}
#endif

	if (!error)
		kthread_run(cmdq_bdg_test_mbox_token, test, "cmdq_bdg_token");

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	for (i = 0; i < count; i++) {
		if (!pkt[i]) {
			cmdq_err("pkt:%d is NULL", i);
			continue;
		}

		msleep_interruptible(10);

		if (flag == async) {
			wait_for_completion(&cmplt[i].cmplt);
			err = cmplt[i].err;
			cmdq_pkt_destroy(pkt[i]);
		}
	}
#endif

	msleep_interruptible(100);
	val = spi_read_reg(pa);
	if (val != count)
		cmdq_err("count:%u flag:%d pa:%pa val:%#x err:%d ans:%#x",
			count, flag, &pa, val, err, count);
	else
		cmdq_msg("%s: count:%u flag:%d pa:%pa val:%#x err:%d ans:%#x",
			__func__, count, flag, &pa, val, err, count);
}

static void cmdq_bdg_test_mbox_threads(struct cmdq_bdg_test *test,
	const bool pass)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	struct cmdq_pkt *pkt, *pkt2;
#else
	struct cmdqRecStruct *rec, *rec2;
#endif
	const dma_addr_t pa = CMDQ_GPR_R32(GCE_BASE, CMDQ_GPR_R15);
	s32 val, ans = 0xbeafbeaf;

	spi_write_reg(pa, 0xdeaddead);
	spi_write_reg(GCE_BASE + 0x68, test->token_user0);
	val = spi_read_reg(pa);
	cmdq_msg("%s: pa:%pa val:%#x", __func__, &pa, val);

	msleep_interruptible(100);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	pkt = cmdq_pkt_create(test->clt);
	cmdq_pkt_wfe(pkt, test->token_user0);

	pkt2 = cmdq_pkt_create(test->clt2);
	cmdq_pkt_set_event(pkt2, test->token_user0);

	cmdq_pkt_write(pass ? pkt : pkt2, NULL, pa, 0x0, UINT_MAX);
	cmdq_pkt_write(pass ? pkt2 : pkt, NULL, pa, ans, UINT_MAX);

	cmdq_pkt_flush_threaded(
		pkt, cmdq_bdg_test_mbox_threaded_cb, (void *)pkt);

	msleep_interruptible(100);

	cmdq_pkt_flush_threaded(
		pkt2, cmdq_bdg_test_mbox_threaded_cb, (void *)pkt2);
#else
	cmdqRecCreate(CMDQ_BDG_SCENARIO_DISP_TEST, &rec);
	cmdqRecWait(rec, test->token_user0);
	cmdq_msg("%s: rec1:%p pkt:%p scenario:%d thread:%d",
		__func__, rec, rec->pkt, rec->scenario, rec->thread);

	cmdqRecCreate(CMDQ_BDG_SCENARIO_DISP_TEST2, &rec2);
	cmdqRecSetEventToken(rec2, test->token_user0);
	cmdq_msg("%s: rec2:%p pkt:%p scenario:%d thread:%d",
		__func__, rec2, rec2->pkt, rec2->scenario, rec2->thread);

	cmdqRecWrite(pass ? rec : rec2, pa, 0x0, UINT_MAX);
	cmdqRecWrite(pass ? rec2 : rec, pa, ans, UINT_MAX);

	cmdqRecFlushAsyncCallback(rec, (void *)cmdq_bdg_test_rec_async_cb,
		(unsigned long)(void *)rec);

	msleep_interruptible(10);

	cmdqRecFlushAsyncCallback(rec2, (void *)cmdq_bdg_test_rec_async_cb,
		(unsigned long)(void *)rec2);
#endif

	msleep_interruptible(100);
	val = spi_read_reg(pa);
	if (val != ans)
		cmdq_err("pa:%pa val:%#x ans:%#x", &pa, val, ans);
	else
		cmdq_msg("%s: pa:%pa val:%#x ans:%#x", __func__, &pa, val, ans);
}

static ssize_t cmdq_bdg_test_write(struct file *filp, const char *buf,
	size_t count, loff_t *offp)
{
	struct cmdq_bdg_test *test =
		(struct cmdq_bdg_test *)filp->f_inode->i_private;
	char str[RTSIG_MAX] = {0};
	s32 id, flag;

	if (copy_from_user(str, buf, count)) {
		cmdq_err("copy_from_user buf:%s count:%ld str:%s failed",
			buf, count, str);
		return count;
	}

	str[count] = '\0';

	if (sscanf(str, "%d %d", &id, &flag) != 2) {
		cmdq_err("sscanf id:%d flag:%d failed", id, flag);
		return count;
	}

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	cmdq_msg("%s:mailbox test:%p id:%d flag:%d", __func__, test, id, flag);
#else
	cmdq_msg("%s:record test:%p id:%d flag:%d", __func__, test, id, flag);
#endif

	switch (id) {
	case 1:
		cmdq_bdg_test_mbox_write(test, 1, false, false);
		break;
	case 2:
		cmdq_bdg_test_mbox_write(test, 1, true, false); // masked
		break;
	case 3:
		cmdq_bdg_test_mbox_tasks(test, async, 1, false);
		break;
	case 4:
		cmdq_bdg_test_mbox_tasks(test, threaded, 1, false);
		break;
	case 5:
		cmdq_bdg_test_mbox_write(test, 127, false, false);
		break;
	case 6:
		cmdq_bdg_test_mbox_tasks(test, async, 5, false);
		break;
	case 7:
		cmdq_bdg_test_mbox_tasks(test, threaded, 5, false);
		break;
	case 8:
		cmdq_bdg_test_mbox_tasks(test, async, 1, true); // error
		break;
	case 9:
		cmdq_bdg_test_mbox_threads(test, true);
		break;
	case 10:
		cmdq_bdg_test_mbox_threads(test, false);
		break;
	case 11:
		cmdq_bdg_test_mbox_write(test, 2, false, true); // reuse
		break;
	}

	return count;
}

static const struct file_operations cmdq_bdg_test_fops = {
	.write = cmdq_bdg_test_write,
};

static int cmdq_bdg_test_probe(struct platform_device *pdev)
{
	struct cmdq_bdg_test *test;
	struct dentry *dir, *fs;
	s32 ret;

	test = devm_kzalloc(&pdev->dev, sizeof(*test), GFP_KERNEL);
	if (!test)
		return -ENOMEM;

	test->dev = &pdev->dev;

	test->clt = cmdq_mbox_create(&pdev->dev, 0);
	if (IS_ERR(test->clt) || !test->clt)
		return -ENXIO;

	test->clt2 = cmdq_mbox_create(&pdev->dev, 1);
	if (IS_ERR(test->clt2) || !test->clt2)
		return -ENXIO;

	ret = of_property_read_u16(
		pdev->dev.of_node, "token_user0", &test->token_user0);

	ret = of_property_read_u16(
		pdev->dev.of_node, "token_gpr_set4", &test->token_gpr_set4);

	cmdq_msg("%s: dev:%p clt:%p clt2:%p user0:%hu gpr:%hu",
		__func__, test->dev, test->clt, test->clt2,
		test->token_user0, test->token_gpr_set4);

	dir = debugfs_lookup("cmdq", NULL);
	if (!dir) {
		dir = debugfs_create_dir("cmdq", NULL);
		if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST)
			return PTR_ERR(dir);
	}

	fs = debugfs_create_file(
		"cmdq-bdg-test", 0444, dir, test, &cmdq_bdg_test_fops);
	if (IS_ERR(fs))
		return PTR_ERR(fs);

	platform_set_drvdata(pdev, test);

	cmdq_msg("%s: dir:%p fs:%p", __func__, dir, fs);

	return 0;
}

static int cmdq_bdg_test_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cmdq_bdg_test_of_ids[] = {
	{.compatible = "mediatek,cmdq-bdg-test",},
	{}
};

static struct platform_driver cmdq_bdg_test_drv = {
	.probe = cmdq_bdg_test_probe,
	.remove = cmdq_bdg_test_remove,
	.driver = {
		.name = "cmdq-bdg-test",
		.of_match_table = cmdq_bdg_test_of_ids,
	},
};

module_platform_driver(cmdq_bdg_test_drv);
