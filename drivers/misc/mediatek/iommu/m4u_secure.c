// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[M4U] " fmt

#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/pm_runtime.h>
#include "m4u_secure.h"

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include "mobicore_driver_api.h"
#endif

static struct m4u_device *m4u_dev;
static unsigned long m4u_mm_base;
static unsigned int m4u_dev_irq[SECURE_BANK_NUM];

int M4U_L2_ENABLE = 1;
static unsigned long mtk_m4u_get_pt(void);

#ifdef M4U_TEE_SERVICE_ENABLE
static DEFINE_MUTEX(gM4u_sec_init);
bool m4u_tee_en;

static struct m4u_sec_gp_context m4u_gp_ta_ctx = {
	.uuid = (struct TEEC_UUID)M4U_TA_UUID,
	.ctx_lock = __MUTEX_INITIALIZER(m4u_gp_ta_ctx.ctx_lock),
	.ctx_type = CTX_TYPE_TA,
};
struct m4u_sec_context m4u_ta_ctx;

void m4u_sec_set_context(void)
{
	m4u_ta_ctx.name = "m4u_ta";
	m4u_ta_ctx.imp = &m4u_gp_ta_ctx;
}

static int m4u_exec_session(struct m4u_sec_context *ctx)
{
	int ret;
	struct TEEC_Operation m4u_operation;
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	if (!ctx->m4u_msg) {
		pr_err("%s TCI/DCI error\n", __func__);
		return -1;
	}

	pr_info("%s, Notify 0x%x\n", __func__, ctx->m4u_msg->cmd);

	memset(&m4u_operation, 0, sizeof(struct TEEC_Operation));
	m4u_operation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_MEMREF_PARTIAL_INPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	m4u_operation.params[0].memref.parent = &gp_ctx->shared_mem;
	m4u_operation.params[0].memref.offset = 0;
	m4u_operation.params[0].memref.size = gp_ctx->shared_mem.size;

	ret = TEEC_InvokeCommand(&gp_ctx->session,
				ctx->m4u_msg->cmd, &m4u_operation, NULL);

	if (ret != TEEC_SUCCESS) {
		pr_err("tz_m4u Notify failed: %d\n", ret);
		goto exit;
	}

	pr_info("%s, get_resp %x\n", __func__, ctx->m4u_msg->cmd);
exit:
	return ret;
}

static int m4u_sec_gp_init(struct m4u_sec_context *ctx)
{
	int ret;
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	ret = TEEC_InitializeContext(TA_UUID, &gp_ctx->ctx);
	if (ret != TEEC_SUCCESS) {
		pr_err("teec_initialize_context failed: %x\n", ret);
		return ret;
	}

	pr_info("%s, ta teec_initialize_context\n", __func__);


	memset(&gp_ctx->shared_mem, 0, sizeof(struct TEEC_SharedMemory));

	gp_ctx->shared_mem.size = sizeof(struct m4u_msg);
	gp_ctx->shared_mem.flags = TEEC_MEM_INPUT;

	ret = TEEC_AllocateSharedMemory(&gp_ctx->ctx, &gp_ctx->shared_mem);
	if (ret == TEEC_SUCCESS) {
		ctx->m4u_msg = (struct m4u_msg *)gp_ctx->shared_mem.buffer;
		pr_info("teec_allocate_shared_memory buf: 0x%p\n",
		gp_ctx->shared_mem.buffer);
	} else {
		pr_err("teec_allocate_shared_memory failed: %d\n", ret);
		goto exit_finalize;
	}

	if (!ctx->m4u_msg) {
		pr_err("m4u msg is invalid\n");
		return -1;
	}
	if (!gp_ctx->init) {
		ret = TEEC_OpenSession(&gp_ctx->ctx, &gp_ctx->session,
			&gp_ctx->uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
		if (ret != TEEC_SUCCESS) {
			pr_err("teec_open_session failed: %x\n", ret);
			goto exit_release;
		}
		gp_ctx->init = 1;
	}

	pr_info("%s, open TCI session success\n", __func__);
	return ret;

exit_release:
	TEEC_ReleaseSharedMemory(&gp_ctx->shared_mem);
exit_finalize:
	TEEC_FinalizeContext(&gp_ctx->ctx);
	return ret;
}

static int m4u_sec_gp_deinit(struct m4u_sec_context *ctx)
{
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	TEEC_ReleaseSharedMemory(&gp_ctx->shared_mem);
	TEEC_CloseSession(&gp_ctx->session);
	TEEC_FinalizeContext(&gp_ctx->ctx);
	gp_ctx->init = 0;

	return 0;
}

static int m4u_sec_ta_open(void)
{
	return m4u_sec_gp_init(&m4u_ta_ctx);
}

static int m4u_sec_ta_close(void)
{
	return m4u_sec_gp_deinit(&m4u_ta_ctx);
}

int m4u_sec_context_init(void)
{
	return m4u_sec_ta_open();
}

int m4u_sec_context_deinit(void)
{
	return m4u_sec_ta_close();
}

struct m4u_sec_context *m4u_sec_ctx_get(unsigned int cmd)
{
	struct m4u_sec_context *ctx = NULL;
	struct m4u_sec_gp_context *gp_ctx;

	ctx = &m4u_ta_ctx;
	gp_ctx = ctx->imp;
	if (!gp_ctx->init) {
		pr_err("%s: before init\n", __func__);
		return NULL;
	}
	mutex_lock(&gp_ctx->ctx_lock);

	return ctx;
}

int m4u_sec_ctx_put(struct m4u_sec_context *ctx)
{
	struct m4u_sec_gp_context *gp_ctx = ctx->imp;

	mutex_unlock(&gp_ctx->ctx_lock);

	return 0;
}

int m4u_exec_cmd(struct m4u_sec_context *ctx)
{
	int ret;

	if (ctx->m4u_msg == NULL) {
		pr_err("%s TCI/DCI error\n", __func__);
		return -1;
	}
	ret = m4u_exec_session(ctx);
	if (ret < 0)
		return -1;

	return 0;
}

static int __m4u_sec_init(void)
{
	int ret;
	struct m4u_sec_context *ctx;
	unsigned long pt_pa_nonsec = mtk_m4u_get_pt();

	ctx = m4u_sec_ctx_get(CMD_M4UTL_INIT);
	if (!ctx)
		return -EFAULT;

	pm_runtime_get_sync(m4u_dev->dev);

	ctx->m4u_msg->cmd = CMD_M4UTL_INIT;
	ctx->m4u_msg->init_param.nonsec_pt_pa = pt_pa_nonsec;
	ctx->m4u_msg->init_param.l2_en = M4U_L2_ENABLE;
	ctx->m4u_msg->init_param.sec_pt_pa = 0;

	pr_info("%s call CMD_M4UTL_INIT, nonsec_pt_pa: 0x%lx\n",
		__func__, pt_pa_nonsec);
	ret = m4u_exec_cmd(ctx);
	if (ret) {
		pr_err("m4u exec command fail\n");
		goto out;
	}

	ret = ctx->m4u_msg->rsp;
out:
	pm_runtime_put_sync(m4u_dev->dev);
	m4u_sec_ctx_put(ctx);
	return ret;
}

int m4u_sec_init(void)
{
	int ret;

	pr_info("%s: start\n", __func__);

	if (m4u_tee_en) {
		pr_info("warning: re-initiation, %d\n", m4u_tee_en);
		goto m4u_sec_reinit;
	}

	m4u_sec_set_context();

	if (!m4u_tee_en) {
		ret = m4u_sec_context_init();
		if (ret)
			return ret;

		m4u_tee_en = 1;
	} else {
		pr_warn("[M4U] warning: reinit sec m4u en=%d\n", m4u_tee_en);
	}
m4u_sec_reinit:
	ret = __m4u_sec_init();
	if (ret < 0) {
		m4u_tee_en = 0;
		m4u_sec_context_deinit();
		pr_err("%s:init fail,ret=0x%x\n", __func__, ret);
		return ret;
	}

	/* don't deinit ta because of multiple init operation */
	pr_info("%s:normal init done\n", __func__);
	return 0;
}

/*#ifdef TO_BE_IMPL*/
int m4u_larb_backup_sec(unsigned int larb_idx)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_LARB_BACKUP);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_LARB_BACKUP;
	ctx->m4u_msg->larb_param.larb_idx = larb_idx;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		pr_err("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

int m4u_larb_restore_sec(unsigned int larb_idx)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_LARB_RESTORE);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_LARB_RESTORE;
	ctx->m4u_msg->larb_param.larb_idx = larb_idx;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		pr_err("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}


static int m4u_reg_backup_sec(void)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_REG_BACKUP);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_REG_BACKUP;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		pr_err("m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

static int m4u_reg_restore_sec(void)
{
	int ret;
	struct m4u_sec_context *ctx;

	ctx = m4u_sec_ctx_get(CMD_M4U_REG_RESTORE);
	if (!ctx)
		return -EFAULT;

	ctx->m4u_msg->cmd = CMD_M4U_REG_RESTORE;

	ret = m4u_exec_cmd(ctx);
	if (ret) {
		pr_err("m4u exec command fail\n");
		ret = -1;
		goto out;
	}

	ret = ctx->m4u_msg->rsp;

out:
	m4u_sec_ctx_put(ctx);
	return ret;
}

static void m4u_early_suspend(void)
{
	pr_info("%s +, %d\n", __func__, m4u_tee_en);

	if (m4u_tee_en)
		m4u_reg_backup_sec();

	pr_info("%s -\n", __func__);
}

static void m4u_late_resume(void)
{
	pr_info("%s +, %d\n", __func__, m4u_tee_en);

	if (m4u_tee_en)
		m4u_reg_restore_sec();

	pr_info("%s -\n", __func__);
}

static struct notifier_block m4u_fb_notifier;
static int m4u_fb_notifier_callback(
	struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	pr_info("%s %ld, %d\n", __func__, event, FB_EVENT_BLANK);

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		m4u_late_resume();
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		m4u_early_suspend();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

irqreturn_t mtk_m4u_isr_sec(int irq, void *dev_id)
{
	struct arm_smccc_res res;
	unsigned long  tf_en = 0;
	unsigned long  tf_port = 0;
	unsigned long  m4u_id = 0;

	if (irq == m4u_dev_irq[0]) {
		m4u_id = 0;
		pr_info("This is secure MM_IOMMU domian\n");
	} else if (irq == m4u_dev_irq[1]) {
		m4u_id = 1;
		pr_info("This is secure VPU_IOMMU domian\n");
	} else {
		pr_err("%s(), Invalid secure irq number %d\n", __func__, irq);
		return -1;
	}

	pr_info("secure bank irq in normal world!\n");
	arm_smccc_smc(MTK_M4U_DEBUG_DUMP, m4u_id, 0, 0,
		0, 0, 0, 0, &res);
	tf_en = res.a0;
	tf_port = res.a1;
	pr_warn("secure bank go back form secure world! en:%zu, fault_id:0x%lx\n",
		tf_en, tf_port);

	return IRQ_HANDLED;
}

static unsigned long mtk_m4u_get_pt(void)
{
	unsigned long reval;

	reval = readl_relaxed((void *)m4u_mm_base + REG_MMU_PT_BASE_ADDR);
	if (reval & F_PGD_REG_BIT32)
		reval |= BIT_ULL(32);
	if (reval & F_PGD_REG_BIT33)
		reval |= BIT_ULL(33);

	pr_info("get pt: 0x%lx\n", reval);
	return reval;
}

static int m4u_open(struct inode *inode, struct file *file)
{
	pr_info("%s process : %s\n", __func__, current->comm);

	return 0;
}

static int m4u_release(struct inode *inode, struct file *file)
{
	pr_info("%s process : %s\n", __func__, current->comm);

	return 0;
}

static long m4u_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
#ifdef M4U_TEE_SERVICE_ENABLE
	case MTK_M4U_T_SEC_INIT:
		{
			pr_info("MTK M4U ioctl : MTK_M4U_T_SEC_INIT command!! 0x%x\n",
				cmd);
			mutex_lock(&gM4u_sec_init);
			ret = m4u_sec_init();
			mutex_unlock(&gM4u_sec_init);
		}
		break;
#endif
	default:
		pr_err("MTK M4U ioctl:No such command(0x%x)!!\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long m4u_compat_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
#ifdef M4U_TEE_SERVICE_ENABLE
	case COMPAT_MTK_M4U_T_SEC_INIT:
		{
			pr_info("MTK_M4U_T_SEC_INIT command!! 0x%x\n",
				  cmd);
			mutex_lock(&gM4u_sec_init);
			ret = m4u_sec_init();
			mutex_unlock(&gM4u_sec_init);
		}
		break;
#endif
	default:
		pr_err("compat ioctl:No such command(0x%x)!!\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#else
#define m4u_compat_ioctl  NULL
#endif

static const struct file_operations m4u_fops = {
	.owner = THIS_MODULE,
	.open = m4u_open,
	.release = m4u_release,
	.unlocked_ioctl = m4u_ioctl,
	.compat_ioctl = m4u_compat_ioctl,
};

static int m4u_probe(struct platform_device *pdev)
{
	struct device           *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	int i, ret;
	struct device_node *mm_m4unode = NULL;
	struct platform_device *plarbdev;

	mm_m4unode = of_parse_phandle(dev->of_node, "mediatek,mm_m4u", 0);
	if (!mm_m4unode) {
		pr_err("not find mm m4u dts\n");
		return -EINVAL;
	}

	plarbdev = of_find_device_by_node(mm_m4unode);
	if (!plarbdev || !plarbdev->dev.driver) {
		pr_info("mm m4u not ready!\n");
		return -EPROBE_DEFER;
	}

	m4u_dev = kzalloc(sizeof(struct m4u_device), GFP_KERNEL);
	if (!m4u_dev)
		return -ENOMEM;

	m4u_dev->dev = dev;
	m4u_dev->m4u_dev_proc_entry = proc_create("m4u", 0644, NULL,
							 &m4u_fops);
	if (!m4u_dev->m4u_dev_proc_entry) {
		pr_err("proc m4u create error\n");
		kfree(m4u_dev);
		return -ENODEV;
	}

	m4u_mm_base = (unsigned long)of_iomap(mm_m4unode, 0);

	dev_info(m4u_dev->dev, "mm m4u base:0x%lx\n", m4u_mm_base);

	for (i = MM_SECURE_BANK; i < SECURE_BANK_NUM; i++) {
		m4u_dev_irq[i] = irq_of_parse_and_map(node, i);

		dev_info(m4u_dev->dev, "secure irq:%u, id:%d\n",
			 m4u_dev_irq[i], i);

		if (request_irq(m4u_dev_irq[i], mtk_m4u_isr_sec,
				IRQF_TRIGGER_NONE, "secure_m4u", NULL)) {
			pr_err("request secure m4u%d IRQ line failed\n",
				i);
			return -ENODEV;
		}
	}

#ifdef M4U_TEE_SERVICE_ENABLE
	m4u_fb_notifier.notifier_call = m4u_fb_notifier_callback;
	ret = fb_register_client(&m4u_fb_notifier);
	if (ret)
		dev_err(m4u_dev->dev, "register fb_notifier failed!\n");
	else
		dev_info(m4u_dev->dev, "register fb_notifier OK!\n");
#endif

	dev_info(m4u_dev->dev, "%s done\n", __func__);
	return 0;
}

static int m4u_remove(struct platform_device *pdev)
{
	if (m4u_dev->m4u_dev_proc_entry)
		proc_remove(m4u_dev->m4u_dev_proc_entry);
	return 0;
}

static const struct of_device_id mtk_m4u_sec_of_ids[] = {
	{ .compatible = "mediatek,secure_m4u",},
	{}
};

static struct platform_driver mtk_m4u_sec_driver = {
	.probe = m4u_probe,
	.remove = m4u_remove,
	.driver = {
		.name = M4U_DEVNAME,
		.of_match_table = of_match_ptr(mtk_m4u_sec_of_ids),
		.owner = THIS_MODULE,
	}
};

module_platform_driver(mtk_m4u_sec_driver);
