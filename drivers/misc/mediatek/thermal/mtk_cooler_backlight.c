#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>

#include "mach/mtk_thermal_monitor.h"

/* This API function is implemented in mediatek/kernel/drivers/leds/leds.c */
extern int setMaxbrightness(int max_level, int enable);

#define mtk_cooler_backlight_dprintk(fmt, args...) \
  do { pr_debug("thermal/cooler/backlight " fmt, ##args); } while (0)

#define BACKLIGHT_COOLER_NR 3

static struct thermal_cooling_device *cl_backlight_dev[BACKLIGHT_COOLER_NR] = { 0 };
static unsigned int g_cl_backlight_state[BACKLIGHT_COOLER_NR] = { 0 };

/* static unsigned int g_cl_backlight_last_state[BACKLIGHT_COOLER_NR] = {0}; */
static unsigned int g_cl_id[BACKLIGHT_COOLER_NR];
static unsigned int g_backlight_level;
static unsigned int g_backlight_last_level;


static void mtk_cl_backlight_set_max_brightness_limit(void)
{
	if (g_backlight_last_level != g_backlight_level) {
		mtk_cooler_backlight_dprintk("set brightness level = %d\n", g_backlight_level);
		switch (g_backlight_level) {
		case 0:
			setMaxbrightness(255, 0);	/* 100% */
			break;
		case 1:
			setMaxbrightness(178, 1);	/* 70% */
			break;
		case 2:
			setMaxbrightness(102, 1);	/* 40% */
			break;
		case 3:
			setMaxbrightness(25, 1);	/* 10% */
			break;
		default:
			setMaxbrightness(255, 0);
			break;
		}
	}
}

static int mtk_cl_backlight_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	/* mtk_cooler_backlight_dprintk("mtk_cl_backlight_get_max_state() %d\n", *state); */
	return 0;
}

static int mtk_cl_backlight_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int nCoolerId;

	/* Get Cooler ID */
	nCoolerId = *((int *)cdev->devdata);

	*state = g_cl_backlight_state[nCoolerId];
	/* mtk_cooler_backlight_dprintk("mtk_cl_backlight_get_cur_state() %d CoolerID:%d\n", *state, nCoolerId); */
	return 0;
}

static int mtk_cl_backlight_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	int i;
	int nCoolerId;		/* /< Backlight Cooler ID */

	/* Get Cooler ID */
	nCoolerId = *((int *)cdev->devdata);

	/* mtk_cooler_backlight_dprintk("mtk_cl_backlight_set_cur_state() %d CoolerID:%d\n", state, nCoolerId); */

	g_cl_backlight_state[nCoolerId] = state;

	g_backlight_level = 0;
	for (i = 0; i < BACKLIGHT_COOLER_NR; i++) {
		g_backlight_level += g_cl_backlight_state[i];
	}

/* Mark for test */
/* if(g_backlight_last_level != g_backlight_level) */
	{
		/* send uevent to notify current call must be dropped */
		/* char event[20] = {0}; */
		/* char *envp[] = { event, NULL }; */
		/* sprintf(event, "BACKLIGHT=%d", g_backlight_level);  ///< BACKLIGHT01=1 ... */
		/* kobject_uevent_env(&(cl_backlight_dev[nCoolerId]->device.kobj), KOBJ_CHANGE, envp); */

		mtk_cl_backlight_set_max_brightness_limit();

		g_backlight_last_level = g_backlight_level;

		/* mtk_cooler_backlight_dprintk("mtk_cl_backlight_set_cur_state() event:%s g_backlight_level:%d\n", event, g_backlight_level); */

	}

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_backlight_ops = {
	.get_max_state = mtk_cl_backlight_get_max_state,
	.get_cur_state = mtk_cl_backlight_get_cur_state,
	.set_cur_state = mtk_cl_backlight_set_cur_state,
};

static int mtk_cooler_backlight_register_ltf(void)
{
	int i;
	mtk_cooler_backlight_dprintk("register ltf\n");

	for (i = 0; i < BACKLIGHT_COOLER_NR; i++) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-backlight%02d", i + 1);	/* /< Cooler Name: mtk-cl-backlight01 */
		g_cl_id[i] = i;
		cl_backlight_dev[i] = mtk_thermal_cooling_device_register(temp, (void *)&g_cl_id[i],	/* /< 0, 1, 2 ... */
									  &mtk_cl_backlight_ops);
	}

	return 0;
}

static void mtk_cooler_backlight_unregister_ltf(void)
{
	int i;
	mtk_cooler_backlight_dprintk("unregister ltf\n");

	for (i = 0; i < BACKLIGHT_COOLER_NR; i++) {
		if (cl_backlight_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_backlight_dev[i]);
			cl_backlight_dev[i] = NULL;
		}
	}
}


static int __init mtk_cooler_backlight_init(void)
{
	int err = 0;

	mtk_cooler_backlight_dprintk("init\n");

	err = mtk_cooler_backlight_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

 err_unreg:
	mtk_cooler_backlight_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_backlight_exit(void)
{
	mtk_cooler_backlight_dprintk("exit\n");

	mtk_cooler_backlight_unregister_ltf();
}
module_init(mtk_cooler_backlight_init);
module_exit(mtk_cooler_backlight_exit);
