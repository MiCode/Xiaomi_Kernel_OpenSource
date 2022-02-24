// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "DBI"

#include <linux/delay.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/ratelimit.h>

#include "mt-plat/sync_write.h"
#include <debug.h>
#include "disp_drv_log.h"
#include "disp_drv_platform.h"
#include "mtkfb.h"
#include "ddp_drv.h"
#include "ddp_manager.h"
#include "ddp_dump.h"
#include "ddp_irq.h"
#include "ddp_dbi.h"
#include "ddp_log.h"
#include "ddp_mmp.h"
#include "disp_helper.h"
#include "ddp_reg.h"
#include "ddp_dump.h"
#include <linux/of.h>
#include <linux/of_address.h>


#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#else
#include "disp_dts_gpio.h"
#endif

#include "ddp_clkmgr.h"

/*****************************************************************************/


#define DBI_OUTREG32(cmdq, addr, val) DISP_REG_SET(cmdq, addr, val)
#define DBI_BACKUPREG32(cmdq, hSlot, idx, addr) \
			DISP_REG_BACKUP(cmdq, hSlot, idx, addr)
#define DBI_POLLREG32(cmdq, addr, mask, value) \
			DISP_REG_CMDQ_POLLING(cmdq, addr, value, mask)
#define DBI_INREG32(type, addr) INREG32(addr)
#define DBI_READREG32(type, dst, src) mt_reg_sync_writel(INREG32(src), dst)

/* static int dsi_reg_op_debug; */

#define BIT_TO_VALUE(TYPE, bit)  \
do { \
	TYPE r;\
	*(unsigned long *)(&r) = (0x00000000);\
	r.bit = ~(r.bit);\
	r;\
} while (0)

#define DBI_MASKREG32(cmdq, REG, MASK, VALUE) \
			DISP_REG_MASK((cmdq), (REG), (VALUE), (MASK))

#define DBI_OUTREGBIT(cmdq, TYPE, REG, bit, value)  \
do {\
	TYPE r;\
	TYPE v;\
	if (cmdq) {\
		*(unsigned int *)(&r) = (0x00000000); \
		r.bit = ~(r.bit);  \
		*(unsigned int *)(&v) = (0x00000000); \
		v.bit = value; \
		DISP_REG_MASK(cmdq, &REG, AS_UINT32(&v), AS_UINT32(&r)); \
	} else { \
		mt_reg_sync_writel(INREG32(&REG), &r); \
		r.bit = (value); \
		DISP_REG_SET(cmdq, &REG, INREG32(&r)); \
	} \
} while (0)

#define DBI_OUTREGFILED(addr, field, value)  \
do {	\
	unsigned int val = 0; \
	val = __raw_readl((unsigned long *)(addr)); \
	val = (val & ~REG_FLD_MASK(field)) | (REG_FLD_VAL((field), (value))); \
	DBI_OUTREG32(NULL, addr, val);   \
} while (0)


static void __iomem *gpio_base;
static void __iomem *gpio_iocfg_bl_base;
static void __iomem *gpio_iocfg_bm_base;

static void __iomem *apmixed_base;
static void __iomem *topckgen_base;


/* gpio_iocfg_bl_base->IOCFG_BL_BASE filed config */
/* LSDA:GPIO8, LSCE0B:GPIO6,LSCK:GPIO7,LSA0:GPIO23, LPRSTB:GPIO12 */
/* IOCFG_BL_BAS+0X0[13:12] */
#define FLD_GPIO_IOCFG_BL_BASE_DBI_LSDA		    REG_FLD(2, 12)
/* IOCFG_BL_BAS+0X0[9:8] */
#define FLD_GPIO_IOCFG_BL_BASE_DBI_LSCE0B		REG_FLD(2, 8)
/* IOCFG_BL_BAS+0X0[11:10] */
#define FLD_GPIO_IOCFG_BL_BASE_DBI_LSCK			REG_FLD(2, 10)
/* IOCFG_BL_BASE+0X10[5:4] */
#define FLD_GPIO_IOCFG_BL_BASE_DBI_LSA0			REG_FLD(2, 4)
#define FLD_GPIO_IOCFG_BM_BASE_DBI_LPRSTB		REG_FLD(3, 6)
/* ToDo Whether RST needed */

/*****************************************************************************/
struct t_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

struct t_dbi_context {
	unsigned int lcm_width;
	unsigned int lcm_height;
	struct LCM_DBI_PARAMS dbi_params;
	struct mutex lock;
	unsigned int is_power_on;
	struct t_condition_wq _lcd_wait_queue;
	/* ToDo */
	/* struct t_condition_wq _vsync_wait_queue; */
};

static struct t_dbi_context _dbi_context;
struct DBI_REGS *DBI_REG;

static const struct LCM_UTIL_FUNCS lcm_utils_dbi0;

static void _init_condition_wq(struct t_condition_wq *waitq)
{
	init_waitqueue_head(&(waitq->wq));
	atomic_set(&(waitq->condition), 0);
}

static void _set_condition_and_wake_up(struct t_condition_wq *waitq)
{
	atomic_set(&(waitq->condition), 1);
	wake_up(&(waitq->wq));
}

static void _DBI_INTERNAL_IRQ_Handler(enum DISP_MODULE_ENUM module,
				      unsigned int param)
{
	struct DBI_REG_INTERRUPT status;	/* = DBI_REG->INT_STATUS; */

	status = *(struct DBI_REG_INTERRUPT *) (&param);
	if (status.COMPLETED)
		_set_condition_and_wake_up(&(_dbi_context._lcd_wait_queue));

}

void ddp_dbi_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long module_base = DISPSYS_DBI_BASE;

	DDPDUMP("== START: DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x000, INREG32(module_base + 0x000),
		0x004, INREG32(module_base + 0x004),
		0x008, INREG32(module_base + 0x008),
		0x00c, INREG32(module_base + 0x00c));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x010, INREG32(module_base + 0x010),
		0x018, INREG32(module_base + 0x018),
		0x01c, INREG32(module_base + 0x01c),
		0x020, INREG32(module_base + 0x020));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x028, INREG32(module_base + 0x028),
		0x02c, INREG32(module_base + 0x02c),
		0x030, INREG32(module_base + 0x030),
		0x034, INREG32(module_base + 0x034));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x038, INREG32(module_base + 0x038),
		0x03c, INREG32(module_base + 0x03c),
		0x040, INREG32(module_base + 0x040),
		0x044, INREG32(module_base + 0x044));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x048, INREG32(module_base + 0x048),
		0x04c, INREG32(module_base + 0x04c),
		0x058, INREG32(module_base + 0x058),
		0x060, INREG32(module_base + 0x060));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x064, INREG32(module_base + 0x064),
		0x068, INREG32(module_base + 0x068),
		0x06c, INREG32(module_base + 0x06c),
		0x070, INREG32(module_base + 0x070));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x080, INREG32(module_base + 0x080),
		0x084, INREG32(module_base + 0x084),
		0x088, INREG32(module_base + 0x088),
		0x090, INREG32(module_base + 0x090));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x094, INREG32(module_base + 0x094),
		0x098, INREG32(module_base + 0x098),
		0x0a8, INREG32(module_base + 0x0a8),
		0x0e0, INREG32(module_base + 0x0e0));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x0e4, INREG32(module_base + 0x0e4),
		0x0e8, INREG32(module_base + 0x0e8),
		0x260, INREG32(module_base + 0x260),
		0x270, INREG32(module_base + 0x270));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x278, INREG32(module_base + 0x278),
		0x27c, INREG32(module_base + 0x27c),
		0x290, INREG32(module_base + 0x290),
		0x294, INREG32(module_base + 0x294));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x298, INREG32(module_base + 0x298),
		0x29c, INREG32(module_base + 0x29c),
		0x300, INREG32(module_base + 0x300),
		0x304, INREG32(module_base + 0x304));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0x310, INREG32(module_base + 0x310),
		0xe00, INREG32(module_base + 0xe00),
		0xe14, INREG32(module_base + 0xe14),
		0xe18, INREG32(module_base + 0xe18));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xe1c, INREG32(module_base + 0xe1c),
		0xe20, INREG32(module_base + 0xe20),
		0xe24, INREG32(module_base + 0xe24),
		0xe28, INREG32(module_base + 0xe28));
	DDPDUMP("0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
		0xe2c, INREG32(module_base + 0xe2c),
		0xe30, INREG32(module_base + 0xe30),
		0xe34, INREG32(module_base + 0xe34),
		0xe38, INREG32(module_base + 0xe38));

	DDPDUMP("-- END: DISP %s REGS --\n", ddp_get_module_name(module));
}

enum DBI_STATUS DBI_DumpRegisters(enum DISP_MODULE_ENUM module, int level)
{
	ddp_dbi_dump_reg(module);
	return DBI_STATUS_OK;
}

void _dump_dbi_params(struct LCM_DBI_PARAMS *dbi_config)
{
	/* ToDo */
}

static enum DBI_STATUS DBI_Reset(enum DISP_MODULE_ENUM module,
				 struct cmdqRecStruct *cmdq)
{
	DISPFUNC();
	DBI_OUTREGBIT(cmdq, struct DBI_REG_START, DBI_REG->DBI_START, RESET, 1);
	DBI_OUTREGBIT(cmdq, struct DBI_REG_START, DBI_REG->DBI_START, RESET, 0);
	return DBI_STATUS_OK;
}

static void lcm_set_reset_pin(UINT32 value)
{
	DBI_OUTREG32(NULL, &(DBI_REG->RESET), value);
}

static void lcm_udelay(UINT32 us)
{
	udelay(us);
}

static void lcm_mdelay(UINT32 ms)
{
	if (ms < 10)
		udelay(ms * 1000);
	else
		msleep(ms);
}


static void lcm_send_cmd(unsigned int cmd)
{
	unsigned int *cmd_addr = NULL;

	if (_dbi_context.dbi_params.ctrl == LCM_CTRL_SERIAL_DBI)
		cmd_addr = &(DBI_REG->DBI_SCMD0);

	DBI_OUTREG32(NULL, cmd_addr, cmd);
}

static void lcm_send_data(unsigned int data)
{
	unsigned int *data_addr = NULL;

	if (_dbi_context.dbi_params.ctrl == LCM_CTRL_SERIAL_DBI)
		data_addr = &(DBI_REG->DBI_SDAT0);

	/* DBI-B add here */
	/* .... */
	DBI_OUTREG32(NULL, data_addr, data);
}


int ddp_dbi_set_lcm_utils(enum DISP_MODULE_ENUM module,
			  struct LCM_DRIVER *lcm_drv)
{
	struct LCM_UTIL_FUNCS *utils = NULL;

	DISPFUNC();
	if (lcm_drv == NULL) {
		DISPERR("lcm_drv is null\n");
		return -1;
	}

	if (module == DISP_MODULE_DBI) {
		utils = (struct LCM_UTIL_FUNCS *) &lcm_utils_dbi0;
	} else {
		DISPERR("wrong module: %d\n", module);
		return -1;
	}

	utils->set_reset_pin = lcm_set_reset_pin;
	utils->udelay = lcm_udelay;
	utils->mdelay = lcm_mdelay;
	utils->send_cmd = lcm_send_cmd;
	utils->send_data = lcm_send_data;
	/* utils->read_data = lcm_read_data; //ToDo */

	/* GPIO related */
	/* ToDo:use DWS */
	/* utils->set_gpio_out = mt_set_gpio_out; */
	/* utils->set_gpio_mode = mt_set_gpio_mode; */
	/* utils->set_gpio_dir = mt_set_gpio_dir; */

	lcm_drv->set_util_funcs(utils);

	return 0;
}

static void _init_dbi_sw(enum DISP_MODULE_ENUM module,
			 struct LCM_DBI_PARAMS *plcm)
{
	struct device_node *np;

	DISPFUNC();

	DBI_REG = (struct DBI_REGS *)DISPSYS_DBI_BASE;

	memset(&_dbi_context, 0, sizeof(_dbi_context));
	mutex_init(&(_dbi_context.lock));
	_init_condition_wq(&(_dbi_context._lcd_wait_queue));
	/* ToDo:_init_condition_wq(&(_dbi_context._vsync_wait_queue)); */
	memcpy(&(_dbi_context.dbi_params), plcm, sizeof(struct LCM_DBI_PARAMS));
	_dump_dbi_params(&(_dbi_context.dbi_params));

	disp_register_module_irq_callback(module, _DBI_INTERNAL_IRQ_Handler);


	/* ToDo:move to ddp_clkmgr */
	if (gpio_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
		gpio_base = of_iomap(np, 0);
		DISPMSG("of_iomap for gpio base @ 0x%p\n", gpio_base);
	}
	if (gpio_iocfg_bl_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,iocfg_3");
		gpio_iocfg_bl_base = of_iomap(np, 0);
		DISPMSG("of_iomap for gpio_iocfg_base_for_dbi @ 0x%p\n",
				gpio_iocfg_bl_base);
	}
	if (gpio_iocfg_bm_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,iocfg_4");
		gpio_iocfg_bm_base = of_iomap(np, 0);
		DISPMSG("of_iomap for gpio_iocfg_bm_base @ 0x%p\n",
				gpio_iocfg_bm_base);
	}

	if (apmixed_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		apmixed_base = of_iomap(np, 0);
		pr_debug("of_iomap for apmixed base @ 0x%p\n", apmixed_base);
		DISPMSG("of_iomap for apmixed base @ 0x%p\n", apmixed_base);
	}

	if (topckgen_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
		topckgen_base = of_iomap(np, 0);
		pr_debug("of_iomap for topckgen base @ 0x%p\n", topckgen_base);
	}

}

enum DBI_STATUS _dbi_gpio_pinmux(enum DISP_MODULE_ENUM module, void *cmdq)
{
	DISPFUNC();
	if (module != DISP_MODULE_DBI) {
		DDPERR("module is not DBI\n");
		return DBI_STATUS_ERROR;
	}
	if (gpio_base == NULL) {
		DDPERR("gpio_base == NULL\n");
		return DBI_STATUS_ERROR;
	}
	/* ToDo: chang to use pinctrl */
	if (_dbi_context.dbi_params.ctrl == LCM_CTRL_SERIAL_DBI) {
		/* LSCE0B:GPIO6,LSCK:GPIO7 */
		DBI_OUTREG32(NULL, gpio_base + 0x30C, 0xAA000000);
		/* LSDA:GPIO8,LPRSTB:GPIO12, */
		DBI_OUTREG32(NULL, gpio_base + 0x31C, 0x000B000A);
		/* LSA0:GPIO23, */
		DBI_OUTREG32(NULL, gpio_base + 0x32C, 0xA0000000);
		/* LPTE:GPIO84,DISP_PWM:GPIO85 */
		DBI_OUTREG32(NULL, gpio_base + 0x3ac, 0x009B0000);


		/* TE change to use GPIO11 */
		/* LSCE0B:GPIO6,LSCK:GPIO7 */
		/* DBI_OUTREG32(NULL,gpio_base + 0x30C,0xAA000000); */
		/* LSDA:GPIO8,LPTE:GPIO11,LPRSTB:GPIO12 */
		/* DBI_OUTREG32(NULL,gpio_base + 0x31C,0x000BB00A); */
		/* LSA0:GPIO23 */
		/* DBI_OUTREG32(NULL,gpio_base + 0x32C,0xA0000000); */
		/* DISP_PWM:GPIO85 */
		/* DBI_OUTREG32(NULL,gpio_base + 0x3ac,0x00900000); */
	}
	return DBI_STATUS_OK;
}


enum DBI_STATUS _dbi_set_DrivingCurrent(enum DISP_MODULE_ENUM module,
				void *cmdq)
{

	DISPFUNC();
	if (module != DISP_MODULE_DBI) {
		DDPERR("module is not DBI\n");
		return DBI_STATUS_ERROR;
	}
	/* ToDo:no need use cmdq */

	/* GPIO driving */
	if (gpio_iocfg_bl_base == NULL) {
		DDPERR("gpio_iocfg_bl_base == NULL\n");
		return DBI_STATUS_ERROR;
	}
	if (gpio_iocfg_bm_base == NULL) {
		DDPERR("gpio_iocfg_bm_base == NULL\n");
		return DBI_STATUS_ERROR;
	}
	if (_dbi_context.dbi_params.ctrl == LCM_CTRL_SERIAL_DBI) {
		switch (_dbi_context.dbi_params.io_driving_current) {
		case LCM_DRIVING_CURRENT_4MA:
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSDA, 0);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCE0B, 0);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCK, 0);
			DBI_OUTREGFILED(gpio_iocfg_bl_base + 0x10,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSA0, 0);
			DBI_OUTREGFILED(gpio_iocfg_bm_base,
					FLD_GPIO_IOCFG_BM_BASE_DBI_LPRSTB, 1);
			break;
		case LCM_DRIVING_CURRENT_8MA:
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSDA, 1);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCE0B, 1);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCK, 1);
			DBI_OUTREGFILED(gpio_iocfg_bl_base + 0x10,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSA0, 1);
			DBI_OUTREGFILED(gpio_iocfg_bm_base,
					FLD_GPIO_IOCFG_BM_BASE_DBI_LPRSTB, 3);
			break;
		case LCM_DRIVING_CURRENT_12MA:
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSDA, 2);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCE0B, 2);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCK, 2);
			DBI_OUTREGFILED(gpio_iocfg_bl_base + 0x10,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSA0, 2);
			DBI_OUTREGFILED(gpio_iocfg_bm_base,
					FLD_GPIO_IOCFG_BM_BASE_DBI_LPRSTB, 5);
			break;
		case LCM_DRIVING_CURRENT_16MA:
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSDA, 3);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCE0B, 3);
			DBI_OUTREGFILED(gpio_iocfg_bl_base,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSCK, 3);
			DBI_OUTREGFILED(gpio_iocfg_bl_base + 0x10,
					FLD_GPIO_IOCFG_BL_BASE_DBI_LSA0, 3);
			DBI_OUTREGFILED(gpio_iocfg_bm_base,
					FLD_GPIO_IOCFG_BM_BASE_DBI_LPRSTB, 7);
			break;
		default:
			break;
		}

	}
	return DBI_STATUS_OK;
}

int ddp_dbi_init(enum DISP_MODULE_ENUM module, void *cmdq)
{
	DISPFUNC();

	/* GPIO pinmux setting */
	_dbi_gpio_pinmux(module, NULL);
	/* set GPIO IO Driving current according to lcm request */
	_dbi_set_DrivingCurrent(module, NULL);

	DBI_OUTREG32(NULL, &(DBI_REG->DBIS_CHKSUM), 0x80000000);
	return DBI_STATUS_OK;
}

int ddp_dbi_deinit(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISPFUNC();
	return DBI_STATUS_OK;
}


static void _dbi_basic_irq_enable(enum DISP_MODULE_ENUM module, void *cmdq)
{
	if (module == DISP_MODULE_DBI) {
		DBI_OUTREGBIT(cmdq, struct DBI_REG_INTERRUPT,
				DBI_REG->INT_ENABLE, COMPLETED, 1);
		DBI_OUTREGBIT(cmdq, struct DBI_REG_INTERRUPT,
				DBI_REG->INT_ENABLE, SYNC, 1);
	}
}


int ddp_dbi_config(enum DISP_MODULE_ENUM module,
		struct disp_ddp_path_config *config, void *cmdq)
{
	struct LCM_DBI_PARAMS *dbi_config;
	struct DBI_ROICON_REG _roi_con;
	struct DBI_SIF_PIX_CON_REG _sif_pix_con;

	DISPFUNC();

	if (!config->dst_dirty)
		return 0;

	dbi_config = &(config->dispif_config.dbi);

	memcpy(&(_dbi_context.dbi_params), dbi_config,
			sizeof(struct LCM_DBI_PARAMS));
	_dbi_context.lcm_width = config->dst_w;
	_dbi_context.lcm_height = config->dst_h;
	_dump_dbi_params(&(_dbi_context.dbi_params));

	_dbi_basic_irq_enable(module, cmdq);

	if (_dbi_context.dbi_params.ctrl != LCM_CTRL_SERIAL_DBI)
		goto done;

	/* command and data are sent to DBI-C LCM CS0 */
	DBI_OUTREGBIT(cmdq, struct DBI_ROI_CADD_REG, DBI_REG->DBI_ROI_CADD,
			addr, 0x8);
	DBI_OUTREGBIT(cmdq, struct DBI_ROI_DADD_REG, DBI_REG->DBI_ROI_DADD,
			addr, 0x9);

	/* control the LSCK frequence */
	/* css(chip selection setup time):7,csh(chip selection hold time):7 */
	/* LSCK will be 1/2 of DBI clock */
	DBI_OUTREG32(cmdq, &(DBI_REG->SIF_TIMING[0]), 0x00007700);
	/* ToDo: change to write once */
	/* cs is controlled by hw */
	DBI_OUTREGBIT(cmdq, struct DBI_SCNF_REG, DBI_REG->DBI_SCNF, HW_CS, 1);
	/* default LSCK is high */
	DBI_OUTREGBIT(cmdq, struct DBI_SCNF_REG,
			DBI_REG->DBI_SCNF, SCK_DEF_0, 1);

	if (_dbi_context.dbi_params.serial.wire_num == LCM_DBI_C_3WIRE)
		DBI_OUTREGBIT(cmdq, struct DBI_SCNF_REG, DBI_REG->DBI_SCNF,
			THREE_WIRE_0, 1);
	else if (_dbi_context.dbi_params.serial.wire_num == LCM_DBI_C_4WIRE)
		DBI_OUTREGBIT(cmdq, struct DBI_SCNF_REG, DBI_REG->DBI_SCNF,
			THREE_WIRE_0, 0);

	DBI_OUTREGBIT(cmdq, struct DBI_SCNF_REG, DBI_REG->DBI_SCNF, SIZE_0, 0);
	memset(&_roi_con, 0, sizeof(struct DBI_ROICON_REG));
	/* ToDo: whether need read first */
	DBI_READREG32(struct DBI_ROICON_REG, &_roi_con, &DBI_REG->DBI_ROICON);

	memset(&_sif_pix_con, 0, sizeof(struct DBI_SIF_PIX_CON_REG));

	if (_dbi_context.dbi_params.data_format.color_order ==
			LCM_COLOR_ORDER_RGB) {
		_roi_con.RGB_ORDER = 1;
	} else {
		_roi_con.RGB_ORDER = 0;
	}

	switch (_dbi_context.dbi_params.data_format.format) {
	case LCM_DBI_FORMAT_RGB332:
		_roi_con.DATA_FMT = 0;
		break;
	case LCM_DBI_FORMAT_RGB444:
		_roi_con.DATA_FMT = 1;
		break;
	case LCM_DBI_FORMAT_RGB565:
		_roi_con.DATA_FMT = 2;
		break;
	case LCM_DBI_FORMAT_RGB666:
		_roi_con.DATA_FMT = 3;
		break;
	case LCM_DBI_FORMAT_RGB888:
		_roi_con.DATA_FMT = 4;
		break;
	default:
		break;

	}
	switch (_dbi_context.dbi_params.data_width) {
	case LCM_DBI_DATA_WIDTH_8BITS:
		_roi_con.IF_SIZE = 0;
		break;
	case LCM_DBI_DATA_WIDTH_16BITS:
		_roi_con.IF_SIZE = 1;
		break;
	case LCM_DBI_DATA_WIDTH_9BITS:
		_roi_con.IF_SIZE = 2;
		break;
	case LCM_DBI_DATA_WIDTH_18BITS:
		_roi_con.IF_SIZE = 3;
		break;
	default:
		break;
	}

	if (_dbi_context.dbi_params.serial.datapin_num == LCM_DBI_C_2DATA_PIN) {
		_sif_pix_con.SIF0_PIX_2PIN = 1;

		switch (_dbi_context.dbi_params.data_width) {
		case LCM_DBI_DATA_WIDTH_8BITS:
			_roi_con.IF_SIZE = 1;
			_sif_pix_con.SIF0_2PIN_SIZE = 2;
			break;
		case LCM_DBI_DATA_WIDTH_9BITS:
			_roi_con.IF_SIZE = 3;
			_sif_pix_con.SIF0_2PIN_SIZE = 3;
			break;
		default:
			break;
		}
		DBI_OUTREG32(cmdq, &(DBI_REG->DBI_SIF_PIX_CON),
				AS_UINT32(&_sif_pix_con));

	}
	DBI_OUTREG32(cmdq, &(DBI_REG->DBI_ROICON), AS_UINT32(&_roi_con));


done:
	/* enable TE */
	/* DBI_OUTREG32(cmdq,&(DBI_REG->DBI_TECON),0x00000001); */

	/* ROI size */
	DBI_OUTREG32(cmdq, &(DBI_REG->DBI_ROI_SIZE),
		(_dbi_context.lcm_height << 16) | (_dbi_context.lcm_width));

	/* enable all interrupt: complete interrupt & TE interrupt */
	DBI_OUTREG32(cmdq, &(DBI_REG->INT_ENABLE), 0x00000039);

	return 0;
}

int ddp_dbi_start(enum DISP_MODULE_ENUM module, void *cmdq)
{

	DISPFUNC();
	DBI_OUTREG32(cmdq, &(DBI_REG->DBI_SCMD0), 0x2C);
	DBI_OUTREG32(cmdq, &(DBI_REG->DBI_START), 0x00000000);
	DBI_OUTREG32(cmdq, &(DBI_REG->DBI_START), 0x00008000);
	return 0;
}

int ddp_dbi_stop(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;
	static const long WAIT_TIMEOUT = HZ;	/* 1 sec , value=1000 */

	DISPFUNC();
	ASSERT(cmdq_handle == NULL);


	ret = wait_event_timeout(_dbi_context._lcd_wait_queue.wq,
				 !((DBI_REG->DBI_STA).BUSY), WAIT_TIMEOUT);

	if (ret == 0) {
		DISPERR("dbi wait event for not busy timeout\n");
		/* DBI_DumpRegisters(module, 1); */
		DBI_Reset(module, NULL);
	}
	/* disable all interrupt */
	DBI_OUTREG32(cmdq_handle, &(DBI_REG->INT_ENABLE), 0x0);

	return 0;
}

int ddp_dbi_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle,
		  enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = 0;
	enum DDP_IOCTL_NAME ioctl = (enum DDP_IOCTL_NAME)ioctl_cmd;

	DISPFUNC();

	switch (ioctl) {
	case DDP_DBI_SW_INIT:
		{
			struct LCM_DBI_PARAMS *plcm =
					(struct LCM_DBI_PARAMS *) params;

			_init_dbi_sw(module, plcm);
			break;
		}
	default:
		break;
	}
	return ret;
}

int ddp_dbi_trigger(enum DISP_MODULE_ENUM module, void *cmdq)
{
	DISPFUNC();
	return 0;
}

int ddp_dbi_reset(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DBI_Reset(module, cmdq_handle);

	return 0;
}

static void _set_power_on_status(enum DISP_MODULE_ENUM module,
				 unsigned int ispoweon)
{
	if (module == DISP_MODULE_DBI)
		_dbi_context.is_power_on = ispoweon;
}

static unsigned int _is_power_on_status(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_DBI)
		return _dbi_context.is_power_on;

	return 0;

}

int ddp_dbi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISPFUNC();
	if (module == DISP_MODULE_DBI) {

		/* enable hf_fdbi0_ck and choose one pll */
		ddp_clk_prepare_enable(MUX_DPI0);
		ddp_clk_set_parent(MUX_DPI0, TOP_UNIVPLL_D3_D2);
		ddp_clk_disable_unprepare(MUX_DPI0);

		/* enable DBI MM clock and DBI INTERFACE CG */
		ddp_clk_prepare_enable(DISP0_DBI_MM_CLOCK);
		ddp_clk_prepare_enable(DISP0_DBI_INTERFACE_CLOCK);
	}

	_set_power_on_status(module, 1);

	return DBI_STATUS_OK;
}

int ddp_dbi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISPFUNC();

	/* disable DBI MM clock and DBI INTERFACE clock */
	if (module == DISP_MODULE_DBI) {
		ddp_clk_disable_unprepare(DISP0_DBI_MM_CLOCK);
		ddp_clk_disable_unprepare(DISP0_DBI_INTERFACE_CLOCK);

	}
	_set_power_on_status(module, 0);
	return DBI_STATUS_OK;
}


int ddp_dbi_is_busy(enum DISP_MODULE_ENUM module)
{
	int busy = 0;
	struct DBI_REG_STATUS status;

	status = DBI_REG->DBI_STA;

	if (status.BUSY)
		busy++;

	DISPDBG("%s is %s\n",
		ddp_get_module_name(module), busy ? "busy" : "idle");
	return busy;
}

int ddp_dbi_is_idle(enum DISP_MODULE_ENUM module)
{
	return !ddp_dbi_is_busy(module);
}


void dbi_analysis(enum DISP_MODULE_ENUM module)
{
	/* ToDo */
	/* unsigned long dbi_base_addr = (unsigned long)DBI_REG; */
	DISPFUNC();

	/* DDPDUMP("== DISP DBI ANALYSIS ==\n"); */

	/* DDPDUMP("DBI Start:%x, Busy:%d\n", */
	/* (DBI_REG->DBI_START).START, (DBI_REG->DBI_STA).BUSY); */
	/* DISPMSG("dbi_analysis Start:%x, Busy:%d\n", */
	/* (DBI_REG->DBI_START).START,(DBI_REG->DBI_STA).BUSY); */
}

int ddp_dbi_dump(enum DISP_MODULE_ENUM module, int level)
{
	if (!_is_power_on_status(module)) {
		DISPERR("sleep dump is invalid\n");
		return 0;
	}

	dbi_analysis(module);
	DBI_DumpRegisters(module, level);

	return 0;
}

int ddp_dbi_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle,
		       enum CMDQ_STATE state)
{
	/* ToDo */
	return 0;
}

struct DDP_MODULE_DRIVER ddp_driver_dbi = {
	.module = DISP_MODULE_DBI,
	.init = ddp_dbi_init,
	.deinit = ddp_dbi_deinit,
	.config = ddp_dbi_config,
	.build_cmdq = ddp_dbi_build_cmdq,
	.trigger = ddp_dbi_trigger,
	.start = ddp_dbi_start,
	.stop = ddp_dbi_stop,
	.reset = ddp_dbi_reset,
	.power_on = ddp_dbi_power_on,
	.power_off = ddp_dbi_power_off,
	.is_idle = ddp_dbi_is_idle,
	.is_busy = ddp_dbi_is_busy,
	.dump_info = ddp_dbi_dump,
	.set_lcm_utils = ddp_dbi_set_lcm_utils,
	.ioctl = ddp_dbi_ioctl
};
