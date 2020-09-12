/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "mtk_ir_core.h"
#include "mtk_ir_regs.h"
static int mtk_ir_get_hw_info(struct platform_device *pdev);
static int mtk_ir_core_register_swirq(int trigger_type);
static void mtk_ir_core_free_swirq(void);

static int mtk_ir_core_probe(struct platform_device *pdev);
static int mtk_ir_core_remove(struct platform_device *pdev);
#ifdef CONFIG_PM_SLEEP
static int mtk_ir_core_suspend(struct device *dev);
static int mtk_ir_core_resume(struct device *dev);
#endif

#define LIRCBUF_SIZE 6

int ir_log_debug_on;
static bool timer_log_en;
static bool ir_probe = FALSE;

static struct mtk_ir_init_info *mtk_ir_init_list[MAX_CHOOSE_IR_NUM];
struct mtk_ir_context *mtk_ir_context_obj;

#ifdef CONFIG_OF
static const struct of_device_id mtk_ir_of_match[] = {
	{.compatible = "mediatek,mt2701-irrx",},
	{.compatible = "mediatek,mt8127-irrx",},
	{.compatible = "mediatek,mt8163-irrx",},
	{.compatible = "mediatek,mt8167-irrx",},
	{.compatible = "mediatek,mt8173-irrx",},
	{.compatible = "mediatek,mt8590-irrx",},
	{.compatible = "mediatek,mt8689-irrx",},
	{.compatible = "mediatek,mt8693-irrx",},
	{.compatible = "mediatek,mt8697-irrx",},
	{.compatible = "mediatek,mt2712-irrx",},
	{.compatible = "mediatek,mt7622-irrx",},
	{.compatible = "mediatek,mt8695-irrx",},
	{.compatible = "mediatek,mt8168-irrx",},
	{},
};
#endif
static const struct dev_pm_ops mtk_ir_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ir_core_suspend, mtk_ir_core_resume)
};

static struct platform_driver mtk_ir_driver = {
	.probe = mtk_ir_core_probe,
	.remove = mtk_ir_core_remove,
	.driver = {
		.name = MTK_IR_DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &mtk_ir_pm_ops,
	#ifdef CONFIG_OF
		.of_match_table = mtk_ir_of_match,
	#endif
	}
};

struct pinctrl *irrx_pinctrl1;
struct pinctrl_state *irrx_pins_default;
struct pinctrl_state *irrx_pins_as_ir_input;

static atomic_t ir_log_to = ATOMIC_INIT(0);
void mtk_ir_set_log_to(int value)
{
	atomic_set(&ir_log_to, value);
}

int mtk_ir_get_log_to(void)
{
	return atomic_read(&ir_log_to);
}

void IR_WRITE32(u32 offset, u32 value)
{
	__raw_writel(value, ((void *)((mtk_ir_context_obj->hw->irrx_base_addr) +
		(offset))));
}

u32 IR_READ32(u32 offset)
{
	return __raw_readl((void *)((mtk_ir_context_obj->hw->irrx_base_addr) +
		(offset)));
}

void IR_WRITE_MASK(u32 u4Addr, u32 u4Mask, u32 u4Offset, u32 u4Val)
{
	IR_WRITE32(u4Addr, ((IR_READ32(u4Addr) & (~(u4Mask))) |
		(((u4Val) << (u4Offset)) & (u4Mask))));
}

u32 IR_READ_MASK(u32 u4Addr, u32 u4Mask, u32 u4Offset)
{
	return IR_READ32((u4Addr & u4Mask) >> u4Offset);
}

struct mtk_ir_core_platform_data *get_mtk_ir_ctl_data(void)
{
	return mtk_ir_context_obj->mtk_ir_ctl_data;
}

static void initTimer(struct hrtimer *timer,
	enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct mtk_ir_context *cxt;

	cxt = mtk_ir_context_obj;
	if (cxt == NULL) {
		MTK_IR_ERR("NULL pointer\n");
		return;
	}

	if (first) {
		cxt->target_ktime = ktime_add_ns(ktime_get(),
			(int64_t) delay_ms * 1000000);
	} else {
		do {
			cxt->target_ktime = ktime_add_ns(cxt->target_ktime,
				(int64_t) delay_ms * 1000000);
		} while (ktime_to_ns(cxt->target_ktime)
			< ktime_to_ns(ktime_get()));
	}

	hrtimer_start(timer, cxt->target_ktime, HRTIMER_MODE_ABS);
}

static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}

enum MTK_IR_MODE mtk_ir_core_getmode(void)
{
	enum boot_mode_t boot_mode = get_boot_mode();

	if ((boot_mode == NORMAL_BOOT) || (boot_mode == RECOVERY_BOOT))
		return MTK_IR_NORMAL;
	else if (boot_mode == FACTORY_BOOT)
		return MTK_IR_FACTORY;
	return MTK_IR_MAX;
}

static int mtk_ir_core_enable_clock(int enable)
{
	int res = 0;

	MTK_IR_LOG(" enable clock: %d\n", enable);

	if (enable)
		res = clk_prepare_enable(mtk_ir_context_obj->hw->irrx_clk);
	else
		clk_disable_unprepare(mtk_ir_context_obj->hw->irrx_clk);
	return res;
}


#ifdef CONFIG_PM_SLEEP
static int mtk_ir_core_suspend(struct device *dev)
{
	int ret = 0;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();

	if (pdata == NULL) {
		MTK_IR_ERR("pdata is NULL!!!\n");
		return 0;
	}

	if (!(pdata->suspend) || !(pdata->resume)) {
		MTK_IR_ERR("%s, suspend arg wrong\n", pdata->device_name);
		return 0;
	}

	ret = pdata->suspend(NULL);
	MTK_IR_LOG("ret(%d)\n", ret);
	return 0;
}

static int mtk_ir_core_resume(struct device *dev)
{
	int ret = 0;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();

	if (pdata == NULL) {
		MTK_IR_ERR("pdata is NULL!!!\n");
		return 0;
	}

	if (!(pdata->suspend) || !(pdata->resume)) {
		MTK_IR_ERR("%s, resume arg wrong\n", pdata->device_name);
		return 0;
	}

	ret = pdata->resume(NULL);
	MTK_IR_LOG("ret(%d)\n", ret);
	return 0;
}
#endif

#define IR_ASSERT_DEBUG 1

void AssertIR(const char *szExpress, const char *szFile, int i4Line)
{
	MTK_IR_ERR("\nAssertion fails at:\nFile: %s, line %d\n\n",
		szFile, (int)(i4Line));
	MTK_IR_ERR("\t%s\n\n", szExpress);

#if IR_ASSERT_DEBUG
	dump_stack();
#endif
}

#define SPRINTF_DEV_ATTR(fmt, arg...) \
	do { \
		temp_len = sprintf(buf, fmt, ##arg); \
		buf += temp_len; \
		len += temp_len; \
	} while (0)

static ssize_t mtk_ir_core_show_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct attribute *pattr = &(attr->attr);
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	int len = 0;
	int temp_len = 0;
	u32 vregstart;

	if (strcmp(pattr->name, "switch_dev") == 0)
		len = sprintf(buf, "no support!! used to switch ir device\n");

	if (strcmp(pattr->name, "debug_log") == 0) {
		SPRINTF_DEV_ATTR("0: debug_log off\n1: debug_log on\n");
		SPRINTF_DEV_ATTR("cur debug_log = %d\n ", ir_log_debug_on);
	}

	if (strcmp(pattr->name, "register") == 0) {
		SPRINTF_DEV_ATTR("-------------dump ir register-----------\n");
#ifdef USE_OLD_IRRX_CODA
		for (vregstart = 0; vregstart <= IRRX_CHKDATA16;) {
#else
		for (vregstart = 0; vregstart <= IRRX_CHKDATA31;) {
#endif
			SPRINTF_DEV_ATTR("IR reg 0x%08x = 0x%08x\n",
				vregstart, IR_READ32(vregstart));
			vregstart += 4;
		}
	}

	if (strcmp(pattr->name, "log_to") == 0) {
		SPRINTF_DEV_ATTR("0: log_to kernel\n1: log_to userspace\n");
		SPRINTF_DEV_ATTR("cur log_to= %d\n", mtk_ir_get_log_to());
	}

	if (strcmp(pattr->name, "cuscode") == 0)
		SPRINTF_DEV_ATTR("read cuscode(0x%08x)\n",
			pdata->get_customer_code());

	if (strcmp(pattr->name, "timer_log") == 0)
		SPRINTF_DEV_ATTR("timer_log = %s\n",
			timer_log_en == FALSE ? "FALSE" : "TRUE");

	return len;
}

static ssize_t mtk_ir_core_store_info(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int var;
	int res;
	u32 reg;
	unsigned long val;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	struct attribute *pattr = &(attr->attr);

	if (strcmp(pattr->name, "switch_dev") == 0)
		return -1;

	if (strcmp(pattr->name, "debug_log") == 0) {
		res = kstrtoint(buf, 0, &var);
		if (res == 0)
			ir_log_debug_on = var;
		return count;
	}

	if (strcmp(pattr->name, "register") == 0) {
		res = sscanf(buf, "%x %lx", &reg, &val);
		MTK_IR_LOG("write reg(0x%08x) =  val(0x%08lx)\n", reg, val);
		IR_WRITE32(reg, val);
		MTK_IR_LOG("read reg(0x%08x) = val(0x%08x)\n",
			reg, IR_READ32(reg));
		return count;
	}

	if (strcmp(pattr->name, "log_to") == 0) {
		res = kstrtoint(buf, 0, &var);
		if (res == 0)
			mtk_ir_set_log_to(var);
		return count;
	}

	if (strcmp(pattr->name, "press_timeout") == 0) {
		res = kstrtoint(buf, 0, &var);
		if (res == 0)
			pdata->i4_keypress_timeout = var;
		MTK_IR_LOG("%s, i4_keypress_timeout = %d\n ",
			   pdata->device_name,
			   pdata->i4_keypress_timeout);
		/*rc_set_keypress_timeout(pdata->i4_keypress_timeout);*/
		return count;
	}

	if (strcmp(pattr->name, "cuscode") == 0) {
		res = kstrtou32(buf, 0, &reg);
		MTK_IR_LOG("write cuscode(0x%08x)\n", reg);
		if (res)
			pdata->set_customer_code(reg);
		MTK_IR_LOG("read cuscode(0x%08x)\n",
			pdata->get_customer_code());
		return count;
	}

	if (strcmp(pattr->name, "timer_log") == 0) {
		res = kstrtoint(buf, 0, &var);
		if (res == 0) {
			if (var)
				timer_log_en = TRUE;
			else
				timer_log_en = FALSE;
		}
		MTK_IR_LOG("timer_log = %s\n",
			timer_log_en == FALSE ? "FALSE" : "TRUE");
		return count;
	}

	return count;
}

static ssize_t mtk_ir_core_store_debug(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int val;
	int res;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	struct attribute *pattr = &(attr->attr);

	if (strcmp(pattr->name, "swirq") == 0) {
		res = kstrtoint(buf, 0, &val);
		if (res == 0) {
			if (val == 1)
				mtk_ir_core_register_swirq(IRQF_TRIGGER_LOW);
			else if (val == 2)
				mtk_ir_core_register_swirq(IRQF_TRIGGER_HIGH);
			else
				mtk_ir_core_free_swirq();
		}
	}

	if (strcmp(pattr->name, "hwirq") == 0) {
		res = kstrtoint(buf, 0, &val);
		if (res == 0) {
			if (val)
				pdata->enable_hwirq(1);
			else
				pdata->enable_hwirq(0);
		}
	}

	return count;
}

static ssize_t mtk_ir_show_alldev_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;
	struct rc_dev *rcdev;
	struct lirc_driver *drv;
	struct rc_map *pmap;
	struct rc_map_table *pscan;
	int index = 0;
	int len = 0;
	int temp_len = 0;

	cxt = mtk_ir_context_obj;
	if (cxt == NULL) {
		MTK_IR_ERR("NULL pointer\n");
		return 0;
	}

	pdata = cxt->mtk_ir_ctl_data;
	if (pdata == NULL) {
		MTK_IR_ERR("pdata is NULL!!!\n");
		return 0;
	}

	pmap = &(pdata->p_map_list->map);
	pscan = pmap->scan;

	SPRINTF_DEV_ATTR("chip: MT%d\n", cxt->hw->irrx_chip_id);
	SPRINTF_DEV_ATTR("irq num: %d, type: %d\n", cxt->hw->irrx_irq,
		cxt->hw->irrx_irq_type);
	SPRINTF_DEV_ATTR("irq_registered: %s\n",
		cxt->irq_register ? "true" : "false");

	SPRINTF_DEV_ATTR("devname: %s, protocol: %d\n", cxt->name,
		cxt->protocol);
	SPRINTF_DEV_ATTR("input_devname: %s\n", pdata->device_name);
	SPRINTF_DEV_ATTR("rc_map name: %s\n", pmap->name);
	SPRINTF_DEV_ATTR("i4_keypress_timeout: %dms\n",
		pdata->i4_keypress_timeout);

	ASSERT(pdata->get_customer_code != NULL);
	SPRINTF_DEV_ATTR("customer code: 0x%x\n", pdata->get_customer_code());

	SPRINTF_DEV_ATTR("rc_map_items:\n");
	for (index = 0; index < (pmap->size); index++)
		SPRINTF_DEV_ATTR("{0x%04x, %04d}\n", pscan[index].scancode,
			pscan[index].keycode);

	rcdev = cxt->rcdev;
	drv = cxt->drv;
	if (rcdev != NULL) {
		SPRINTF_DEV_ATTR("after mapping:\n");
		if (likely((drv != NULL) && (rcdev != NULL))) {
			pmap = &(rcdev->rc_map);
			pscan = pmap->scan;
			for (index = 0; index < (pmap->len); index++)
				SPRINTF_DEV_ATTR("{0x%04x, %04d}\n",
					pscan[index].scancode,
					pscan[index].keycode);
		}
	}

#ifdef MTK_IR_WAKEUP
	SPRINTF_DEV_ATTR("clock = disable\n");
#else
	SPRINTF_DEV_ATTR("clock = enable\n");
#endif

	return len;
}


static ssize_t mtk_ir_core_show_mouse_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_ir_context *cxt = NULL;
	struct mtk_ir_core_platform_data *pdata = NULL;
	struct mtk_ir_mouse_code *p_mousecode = NULL;
	int len = 0;
	int temp_len = 0;

	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	SPRINTF_DEV_ATTR("g_ir_device_mode = %d\n",
		mtk_ir_mouse_get_device_mode());

	SPRINTF_DEV_ATTR("-------- devname(%s), protocol(%d)--------\n",
			cxt->name, cxt->protocol);
	SPRINTF_DEV_ATTR("input_mousename(%s)\n", pdata->mousename);

	p_mousecode = &(pdata->mouse_code);

	SPRINTF_DEV_ATTR("scanleft =  0x%x\n", p_mousecode->scanleft);
	SPRINTF_DEV_ATTR("scanright = 0x%x\n", p_mousecode->scanright);
	SPRINTF_DEV_ATTR("scanup = 0x%x\n", p_mousecode->scanup);
	SPRINTF_DEV_ATTR("scandown = 0x%x\n", p_mousecode->scandown);
	SPRINTF_DEV_ATTR("scanenter = 0x%x\n", p_mousecode->scanenter);
	SPRINTF_DEV_ATTR("scanswitch = 0x%x\n", p_mousecode->scanswitch);

	SPRINTF_DEV_ATTR("x_small_step = %d\n", mtk_ir_mouse_get_x_smallstep());
	SPRINTF_DEV_ATTR("y_small_step = %d\n", mtk_ir_mouse_get_y_smallstep());
	SPRINTF_DEV_ATTR("x_large_step = %d\n", mtk_ir_mouse_get_x_largestep());
	SPRINTF_DEV_ATTR("y_large_step = %d\n", mtk_ir_mouse_get_y_largestep());
	return len;
}

static ssize_t mtk_ir_core_store_mouse_info(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int varx = -1;
	int vary = -1;
	int res;

	struct attribute *pattr = &(attr->attr);

	if (strcmp(pattr->name, "mouse_x_y_small") == 0) {
		res = sscanf(buf, "%d %d", &varx, &vary);
		if (res == 2) {
			mtk_ir_mouse_set_x_smallstep(varx);
			mtk_ir_mouse_set_y_smallstep(vary);
		}
	}
	if (strcmp(pattr->name, "mouse_x_y_large") == 0) {
		res = sscanf(buf, "%d %d", &varx, &vary);
		if (res == 2) {
			mtk_ir_mouse_set_x_largestep(varx);
			mtk_ir_mouse_set_y_largestep(vary);
		}
	}

	return count;

}

static DEVICE_ATTR(alldev_info, 0444, mtk_ir_show_alldev_info, NULL);
static DEVICE_ATTR(switch_dev, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(debug_log, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(register, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(clock, 0220, NULL, mtk_ir_core_store_debug);
static DEVICE_ATTR(swirq, 0220, NULL, mtk_ir_core_store_debug);
static DEVICE_ATTR(hwirq, 0220, NULL, mtk_ir_core_store_debug);
static DEVICE_ATTR(press_timeout, 0220, NULL, mtk_ir_core_store_info);
static DEVICE_ATTR(cuscode, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(timer_log, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(log_to, 0664, mtk_ir_core_show_info,
		mtk_ir_core_store_info);
static DEVICE_ATTR(mouse, 0444, mtk_ir_core_show_mouse_info, NULL);
static DEVICE_ATTR(mouse_x_y_small, 0220, NULL, mtk_ir_core_store_mouse_info);
static DEVICE_ATTR(mouse_x_y_large, 0220, NULL, mtk_ir_core_store_mouse_info);

static struct device_attribute *ir_attr_list[] = {
	&dev_attr_alldev_info,
	&dev_attr_switch_dev,
	&dev_attr_debug_log,
	&dev_attr_register,
	&dev_attr_clock,
	&dev_attr_swirq,
	&dev_attr_hwirq,
	&dev_attr_press_timeout,
	&dev_attr_cuscode,
	&dev_attr_timer_log,
	&dev_attr_log_to,
	&dev_attr_mouse,
	&dev_attr_mouse_x_y_small,
	&dev_attr_mouse_x_y_large,
};

void mtk_ir_core_send_scancode(u32 scancode)
{
	struct mtk_ir_context *cxt;
	struct rc_dev *rcdev;
	struct lirc_driver *drv;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	struct mtk_ir_mouse_code *p_mousecode = &(pdata->mouse_code);
	enum MTK_IR_DEVICE_MODE dev_mode;

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	rcdev = cxt->rcdev;
	drv = cxt->drv;

	MTK_IR_LOG(" get scancode 0x%08x !!\n", scancode);
	dev_mode = mtk_ir_mouse_get_device_mode();
	if (pdata->mouse_support && (scancode == p_mousecode->scanswitch)) {
		MTK_IR_LOG(" switch mode code 0x%08x\n", scancode);
		if (!(rcdev->keypressed)) {
			if (dev_mode == MTK_IR_AS_IRRX)
				mtk_ir_mouse_set_device_mode(MTK_IR_AS_MOUSE);
			else
				mtk_ir_mouse_set_device_mode(MTK_IR_AS_IRRX);
		}
		rc_keydown(rcdev, cxt->protocol, 0xffff, 0);
	} else {
		if (dev_mode == MTK_IR_AS_MOUSE) {
			if (mtk_ir_mouse_proc_key(scancode, cxt))
				MTK_IR_LOG(" process mouse key\n");
			else {
				MTK_IR_LOG(" process ir key\n");
				rc_keydown(rcdev, cxt->protocol, scancode, 0);
				}
		} else {
			MTK_IR_LOG(" rc_keydown 0x%08x !!\n", scancode);
			rc_keydown(rcdev, cxt->protocol, scancode, 0);
		}
	}

	if (likely((drv != NULL))) {
		int index = 0;
		struct rc_map *pmap = &(rcdev->rc_map);
		struct rc_map_table *pscan = pmap->scan;
		struct mtk_ir_msg msg = { scancode, BTN_NONE };

		for (index = 0; index < (pmap->len); index++) {
			if (pscan[index].scancode == scancode) {
				msg.scancode = scancode;
				msg.keycode = pscan[index].keycode;
				break;
			}
		}
		MTK_IR_LOG("lirc write scancode: 0x%08x, keycode: 0x%08x\n",
			msg.scancode, msg.keycode);
		lirc_buffer_write(drv->rbuf, (unsigned char *)&msg);
		wake_up(&(drv->rbuf->wait_poll));
	}
	MTK_IR_LOG("%s----", __func__);

}

/* ir irq function */
static void mtk_ir_irq_function(struct work_struct *work)
{
	u32 scancode = BTN_NONE;
	struct mtk_ir_context *cxt;
	struct rc_dev *rcdev;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	rcdev = cxt->rcdev;

	ASSERT(pdata != NULL);
	ASSERT(pdata->ir_hw_decode != NULL);

	scancode = pdata->ir_hw_decode(NULL);

	if ((scancode != BTN_INVALID_KEY) && (scancode != BTN_NONE)) {
		if (likely(rcdev != NULL))
			mtk_ir_core_send_scancode(scancode);
	}
	pdata->enable_hwirq(1);
	MTK_IR_LOG("%s----\n", __func__);
}

/* ir irq handler */
static irqreturn_t mtk_ir_core_irq(int irq, void *dev_id)
{
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();

	MTK_IR_FUN();
	pdata->enable_hwirq(0);
	queue_work(mtk_ir_context_obj->mtk_ir_workqueue,
		&mtk_ir_context_obj->report_irq);
	MTK_IR_LOG("%s----\n", __func__);
	return IRQ_HANDLED;
}

static int mtk_ir_core_register_swirq(int trigger_type)
{
	int ret;
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;

	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	if (cxt->irq_register == true) {
		MTK_IR_LOG("irq number(%d) already registered\n",
			cxt->hw->irrx_irq);
		return 0;
	}

	ret = request_irq(cxt->hw->irrx_irq, mtk_ir_core_irq, trigger_type,
			  pdata->device_name, (void *)pdata);
	if (ret) {
		MTK_IR_LOG("fail to request irq(%d), ir_dev(%s) ret = %d !!!\n",
			   cxt->hw->irrx_irq, pdata->device_name, ret);
		return -1;
	}
	cxt->irq_register = true;

	MTK_IR_LOG("registe mtk_ir_core_irq irq(%d) success.\n",
		cxt->hw->irrx_irq);
	return 0;
}

static void mtk_ir_core_free_swirq(void)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;

	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	if (cxt->irq_register == false) {
		MTK_IR_LOG("irq number(%d) already freed\n", cxt->hw->irrx_irq);
		return;
	}
	MTK_IR_LOG("free irq number(%d)\n", cxt->hw->irrx_irq);
	free_irq(cxt->hw->irrx_irq, (void *)pdata);
	cxt->irq_register = false;
}

static const struct file_operations mtk_lirc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lirc_dev_fop_ioctl,
	.read = lirc_dev_fop_read,
	.poll = lirc_dev_fop_poll,
	.open = lirc_dev_fop_open,
	.release = lirc_dev_fop_close,
	.llseek = no_llseek,
};

static int mtk_ir_core_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(ir_attr_list);

	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, ir_attr_list[idx]);
		if (err)
			break;
	}
	return err;
}

static void mtk_ir_core_remove_attr(struct device *dev)
{
	int idx;
	int num = (int)ARRAY_SIZE(ir_attr_list);

	if (!dev)
		return;

	for (idx = 0; idx < num; idx++)
		device_remove_file(dev, ir_attr_list[idx]);
}

static int mtk_ir_lirc_register(struct device *dev_parent)
{
	struct lirc_driver *drv;
	struct lirc_buffer *rbuf;
	int ret = -ENOMEM;
	unsigned long features;

	MTK_IR_FUN();
	drv = kzalloc(sizeof(struct lirc_driver), GFP_KERNEL);
	if (!drv) {
		MTK_IR_ERR(" kzalloc lirc_driver fail!!!\n ");
		return ret;
	}

	rbuf = kzalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
	if (!rbuf) {
		MTK_IR_ERR(" kzalloc lirc_buffer fail!!!\n ");
		goto rbuf_alloc_failed;
	}

	ret = lirc_buffer_init(rbuf, MTK_IR_CHUNK_SIZE, LIRCBUF_SIZE);
	if (ret) {
		MTK_IR_ERR(" lirc_buffer_init fail ret(%d) !!!\n", ret);
		goto rbuf_init_failed;
	}

	features = 0;

	snprintf(drv->name, sizeof(drv->name), MTK_IR_LIRC_DEV_NAME);

	drv->minor = -1;
	drv->features = features;
	drv->data = NULL;
	drv->rbuf = rbuf;
	drv->chunk_size = MTK_IR_CHUNK_SIZE;
	drv->code_length = MTK_IR_CHUNK_SIZE;
	drv->fops = &mtk_lirc_fops;
	drv->dev = dev_parent;
	drv->owner = THIS_MODULE;

	drv->minor = lirc_register_driver(drv);
	if (drv->minor < 0) {
		ret = -ENODEV;
		MTK_IR_ERR(" lirc_register_driver fail ret(%d) !!!\n", ret);
		goto lirc_register_failed;
	}
	mtk_ir_context_obj->drv = drv;
	MTK_IR_LOG("%s[%s]----\n", __func__, drv->name);
	return 0;

lirc_register_failed:
	lirc_buffer_free(rbuf);
rbuf_init_failed:
	kfree(rbuf);
rbuf_alloc_failed:
	kfree(drv);
	return ret;
}


static int mtk_ir_lirc_unregister(struct mtk_ir_context *pdev)
{
	struct lirc_driver *drv = pdev->drv;

	if (drv) {
		lirc_unregister_driver(drv->minor);
		lirc_buffer_free(drv->rbuf);
		kfree(drv);
		pdev->drv = NULL;
	}
	return 0;
}


static int mtk_ir_get_hw_info(struct platform_device *pdev)
{
	const char *clkname = "irrx_clock";
	const char *compatible_name;
	struct device_node *node;
	struct mtk_ir_context *cxt;
	u32 irrx_chip_id;
	int ret;

	MTK_IR_LOG("%s[%s] :entry\n", __func__, pdev->name);

	cxt = mtk_ir_context_obj;
	cxt->hw->irrx_base_addr = of_iomap(pdev->dev.of_node, 0);
	if (!cxt->hw->irrx_base_addr) {
		MTK_IR_ERR(" Cannot get irrx register base address!\n");
		return -ENODEV;
	}
	cxt->hw->irrx_clk = devm_clk_get(&pdev->dev, clkname);
	if (IS_ERR(cxt->hw->irrx_clk)) {
		MTK_IR_ERR(" Cannot get irrx clock!\n");
		ret = PTR_ERR(cxt->hw->irrx_clk);
		MTK_IR_ERR(" ret=%d!\n", ret);
		iounmap(cxt->hw->irrx_base_addr);
		return ret;
	}

	ret = mtk_ir_core_enable_clock(1);
	if (ret) {
		MTK_IR_ERR(" Enable clk failed!\n");
		iounmap(cxt->hw->irrx_base_addr);
		return -ENODEV;
	}

	node = of_find_matching_node(NULL, mtk_ir_of_match);
	if (node) {
		cxt->hw->irrx_irq = irq_of_parse_and_map(node, 0);
		cxt->hw->irrx_irq_type =
			irq_get_trigger_type(cxt->hw->irrx_irq);
		MTK_IR_LOG(" irq: %d, type: %d!\n", cxt->hw->irrx_irq,
			cxt->hw->irrx_irq_type);

		ret = of_property_read_string(node, "compatible",
			&compatible_name);
		ret = sscanf(compatible_name, "mediatek,mt%d", &irrx_chip_id);
		MTK_IR_LOG(" chip ID is: %d!\n", irrx_chip_id);
		if (ret == 1)
			cxt->hw->irrx_chip_id = irrx_chip_id;
		else
			MTK_IR_ERR(" Device Tree: Find property fail!\n");
	} else {
		MTK_IR_ERR(" Device Tree: can not find IR node!\n");
		clk_disable_unprepare(cxt->hw->irrx_clk);
		iounmap(cxt->hw->irrx_base_addr);
		return -ENODEV;
	}

	irrx_pinctrl1 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(irrx_pinctrl1)) {
		ret = PTR_ERR(irrx_pinctrl1);
		MTK_IR_ERR(" Cannot find pinctrl %d!\n", ret);
		clk_disable_unprepare(cxt->hw->irrx_clk);
		iounmap(cxt->hw->irrx_base_addr);
		return ret;
	}
	irrx_pins_default = pinctrl_lookup_state(irrx_pinctrl1, "default");
	if (IS_ERR(irrx_pins_default)) {
		ret = PTR_ERR(irrx_pins_default);
		MTK_IR_LOG("Cannot find pinctrl default %d!\n", ret);
		clk_disable_unprepare(cxt->hw->irrx_clk);
		iounmap(cxt->hw->irrx_base_addr);
		return ret;
	}
	pinctrl_select_state(irrx_pinctrl1, irrx_pins_default);

	MTK_IR_LOG("%s----\n", __func__);
	return 0;
}

/*
 * This timer function is for IRRX routine work.
 * You can add stuff you want to do in this function.
 */
static void mtk_ir_timer_function(struct work_struct *work)
{
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	char data[2] = { 0 };

	data[0] = timer_log_en;
	pdata->timer_func(data);
	startTimer(&mtk_ir_context_obj->hrTimer,
		atomic_read(&mtk_ir_context_obj->delay), false);
}

enum hrtimer_restart mtk_ir_timer_poll(struct hrtimer *timer)
{
	struct mtk_ir_context *obj =
		(struct mtk_ir_context *)container_of(
			timer, struct mtk_ir_context, hrTimer);

	queue_work(obj->mtk_ir_workqueue, &obj->report);
	return HRTIMER_NORESTART;
}

static int mtk_ir_core_probe(struct platform_device *pdev)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;
	struct rc_dev *rcdev;
	int ret = 0;

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	ret = mtk_ir_get_hw_info(pdev);
	if (ret) {
		MTK_IR_ERR(" get gpio info fail!\n");
		goto err_probe;
	}

	ret = mtk_ir_core_create_attr(&(pdev->dev));
	if (ret) {
		MTK_IR_ERR(" create device attribute fail!\n");
		goto err_probe;
	}

	ret = mtk_ir_lirc_register(&(pdev->dev));
	if (ret) {
		MTK_IR_ERR(" mtk_ir_lirc_register fail ret(%d) !!!\n", ret);
		goto err_probe;
	}

	ASSERT(pdata != NULL);
	ASSERT(pdata->init_hw != NULL);
	ASSERT(pdata->uninit_hw != NULL);
	ASSERT(pdata->p_map_list != NULL);
	ASSERT(pdata->ir_hw_decode != NULL);

	ret = pdata->init_hw();
	if (ret) {
		MTK_IR_ERR("fail to init_hw for ir_dev(%s) ret = %d!!!\n",
			pdata->device_name, ret);
		goto err_probe;
	}

	rcdev = rc_allocate_device(RC_DRIVER_SCANCODE);
	if (!rcdev) {
		ret = -ENOMEM;
		MTK_IR_ERR(" rc_allocate_device fail\n");
		goto err_allocate_device;
	}

	rcdev->driver_type = RC_DRIVER_SCANCODE;
	rcdev->device_name = pdata->device_name;
	rcdev->input_id.bustype = BUS_HOST;
	rcdev->input_id.version = IR_VERSION;
	rcdev->input_id.product = IR_PRODUCT;
	rcdev->input_id.vendor = IR_VENDOR;
	rcdev->driver_name = MTK_IR_DRIVER_NAME;
	rcdev->map_name = pdata->p_map_list->map.name;

	ret = rc_register_device(rcdev);
	if (ret < 0) {
		MTK_IR_ERR("failed to register ir_dev(%s) ret(%d)!!!\n",
			pdata->device_name, ret);
		goto err_register_rc_device;
	} else
		MTK_IR_LOG("register rc_dev input[%s] success.\n",
			rcdev->device_name);

	/*rc_set_keypress_timeout(pdata->i4_keypress_timeout);*/
	clear_bit(EV_MSC, rcdev->input_dev->evbit);
	clear_bit(MSC_SCAN, rcdev->input_dev->mscbit);
	cxt->rcdev = rcdev;

	cxt->p_devmouse = mtk_ir_mouse_register_input(pdev);
	if (cxt->p_devmouse == NULL) {
		MTK_IR_LOG("fail to register ir_mouse device(%s)\n",
			pdata->mousename);
		goto err_register_mousedev;
	}

	ret = mtk_ir_core_register_swirq(cxt->hw->irrx_irq_type);
	if (ret) {
		MTK_IR_ERR("register irq failed!\n");
		goto err_request_irq;
	}

	ret = pdata->enable_hwirq(1);
	if (ret) {
		MTK_IR_ERR(" enable_hwirq failed!\n");
		return ret;
	}

	startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
	ir_probe = TRUE;
	MTK_IR_LOG("%s----\n", __func__);
	return 0;

err_request_irq:
err_register_mousedev:
	rc_unregister_device(rcdev);
	rcdev = NULL;
err_register_rc_device:
	rc_free_device(rcdev);
	rcdev = NULL;
	cxt->rcdev = NULL;
err_allocate_device:
	pdata->uninit_hw();
err_probe:
	kfree(mtk_ir_context_obj);
	MTK_IR_ERR("probe error----\n");
	return ret;
}

static int mtk_ir_core_remove(struct platform_device *pdev)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;
	struct rc_dev *rcdev;

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;
	rcdev = cxt->rcdev;

	mtk_ir_lirc_unregister(cxt);
	mtk_ir_core_remove_attr(&(pdev->dev));

	if (rcdev != NULL) {
		mtk_ir_core_free_swirq();
		pdata->uninit_hw();
		if (cxt->p_devmouse != NULL) {
			mtk_ir_mouse_unregister_input(cxt->p_devmouse);
			cxt->p_devmouse = NULL;
		}
		rc_unregister_device(rcdev);
		cxt->rcdev = NULL;
	}
	kfree(mtk_ir_context_obj);
	return 0;
}


void mtk_ir_core_log_always(const char *fmt, ...)
{
	va_list args;

	if (mtk_ir_get_log_to() == 0) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_core_early_suspend(struct early_suspend *h)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	ASSERT(pdata != NULL);
	if (pdata->early_suspend)
		pdata->early_suspend(NULL);
}

static void mtk_ir_core_late_resume(struct early_suspend *h)
{
	struct mtk_ir_context *cxt;
	struct mtk_ir_core_platform_data *pdata;

	MTK_IR_FUN();
	cxt = mtk_ir_context_obj;
	pdata = cxt->mtk_ir_ctl_data;

	ASSERT(pdata != NULL);
	if (pdata->late_resume)
		pdata->late_resume(NULL);
}

static struct early_suspend mtk_ir_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = mtk_ir_core_early_suspend,
	.resume = mtk_ir_core_late_resume,
};
#endif

struct mtk_ir_hw *get_mtk_ir_cus_dts(struct mtk_ir_hw *hw)
{
	return hw;
}

int mtk_ir_driver_add(struct mtk_ir_init_info *obj)
{
	int err = 0;
	int i = 0;

	MTK_IR_LOG("%s: %s\n", __func__, obj->name);
	if (!obj) {
		MTK_IR_ERR("mtk_ir_driver add fail, acc_init_info is NULL\n");
		return -1;
	}
	for (i = 0; i < MAX_CHOOSE_IR_NUM; i++) {
		if (mtk_ir_init_list[i] == NULL) {
			obj->platform_diver_addr = &mtk_ir_driver;
			mtk_ir_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_IR_NUM) {
		MTK_IR_ERR("mtk_ir_driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(mtk_ir_driver_add);

int mtk_ir_register_ctl_data_path(struct mtk_ir_core_platform_data *ctl_data,
		struct mtk_ir_hw *hw, enum rc_proto protocol)
{
	struct mtk_ir_context *cxt;

	cxt = mtk_ir_context_obj;
	cxt->mtk_ir_ctl_data = ctl_data;
	cxt->hw = hw;
	cxt->protocol = protocol;

	if (cxt->mtk_ir_ctl_data->ir_hw_decode == NULL) {
		MTK_IR_ERR("mtk_ir register control data path fail\n");
		return -1;
	}

	return 0;
}

static struct mtk_ir_context *mtk_ir_context_alloc_object(void)
{
	struct mtk_ir_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	MTK_IR_FUN();
	if (!obj) {
		MTK_IR_ERR("Alloc mtk_ir object error!\n");
		return NULL;
	}
	obj->name = MTK_IR_DRIVER_NAME;
	obj->irq_register = false;

	INIT_WORK(&obj->report, mtk_ir_timer_function);
	INIT_WORK(&obj->report_irq, mtk_ir_irq_function);
	obj->mtk_ir_workqueue = create_workqueue("mtk_ir_workqueue");
	if (!obj->mtk_ir_workqueue) {
		kfree(obj);
		return NULL;
	}

	atomic_set(&obj->delay, 1000);
	initTimer(&obj->hrTimer, mtk_ir_timer_poll);

	MTK_IR_LOG("%s----\n", __func__);
	return obj;
}

static int mtk_ir_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	MTK_IR_FUN();
	for (i = 0; i < MAX_CHOOSE_IR_NUM; i++) {
		MTK_IR_LOG(" driver num= %d\n", i);
		if (mtk_ir_init_list[i] != NULL) {
			MTK_IR_LOG(" mtk_ir try to init driver [%s]\n",
				mtk_ir_init_list[i]->name);
			err = mtk_ir_init_list[i]->init();
			if (err == 0) {
				MTK_IR_LOG(" mtk_ir real driver [%s] init ok\n",
					   mtk_ir_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_IR_NUM) {
		MTK_IR_ERR(" %s fail\n", __func__);
		err = -1;
	}
	MTK_IR_LOG("%s(%d)----\n", __func__, err);
	return err;
}

static int __init mtk_ir_core_init(void)
{
	int ret = 0;

	MTK_IR_FUN();
	mtk_ir_context_obj = mtk_ir_context_alloc_object();
	if (mtk_ir_context_obj == NULL) {
		MTK_IR_ERR(" mtk_ir_context_alloc_object fail\n");
		return -1;
	}

	ret = mtk_ir_real_driver_init();
	if (ret) {
		MTK_IR_ERR(" mtk_ir_real_driver_init fail\n");
		return ret;
	}

	ret = platform_driver_register(&mtk_ir_driver);
	if ((ret) || (ir_probe == FALSE)) {
		MTK_IR_ERR(" failed to register mtk_ir_driver\n");
		return ret;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mtk_ir_early_suspend_desc);
#endif

	MTK_IR_LOG("%s----\n", __func__);
	return ret;
}

static void __exit mtk_ir_core_exit(void)
{
	stopTimer(&mtk_ir_context_obj->hrTimer);

	platform_driver_unregister(&mtk_ir_driver);
	kfree(mtk_ir_context_obj);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mtk_ir_early_suspend_desc);
#endif
}

module_init(mtk_ir_core_init);
module_exit(mtk_ir_core_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ir common driver");
MODULE_AUTHOR("Zhimin.Tang@mediatek.com");
