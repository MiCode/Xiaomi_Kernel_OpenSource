/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <ssw.h>
#include <mach/mtk_ccci_helper.h>
/*--------------Feature option---------------*/
#define __ENABLE_SSW_SYSFS 1

/*--------------Global varible---------------*/
void __iomem *reg_base;

/*--------------Register address-------------*/
#define GPIO_SIM1_MODE		(0x0980)
#define GPIO_SIM1_PULL		(0x0990)
#define GPIO_SIM2_MODE		(0x09A0)
#define GPIO_SIM2_PULL		(0x09B0)

/*----------------GPIO settings--------------*/
#define SIM1_PULL_DEFAULT		(0x470)
#define SIM2_PULL_DEFAULT		(0x470)

#define SINGLE_SIM1_MODE				(0x111)
#define SINGLE_SIM2_MODE				(0x111)

#define SINGLE_SIM1_MODE_LITE		(0x444)
#define SINGLE_SIM2_MODE_LITE		(0x444)

#define DUAL_SIM1_MODE					(0x111)
#define DUAL_SIM2_MODE					(0x444)

#define DUAL_SIM1_MODE_SWAP			(0x444)
#define DUAL_SIM2_MODE_SWAP			(0x111)

/*--------------SIM mode list----------------*/
#define SINGLE_TALK_MDSYS				(0x1)
#define SINGLE_TALK_MDSYS_LITE	(0x2)
#define DUAL_TALK								(0x3)
#define DUAL_TALK_SWAP					(0x4)

/*----------------variable define-----------------*/
unsigned int sim_mode_curr = SINGLE_TALK_MDSYS;
struct mutex sim_switch_mutex;

unsigned int get_sim_switch_type(void)
{
	SSW_DBG("[ccci/ssw]SSW_GENERIC\n");
	return SSW_INTERN;
}

/*************************************************************************/
/*create sys file for sim switch mode                                                                     */
 /**/
/*************************************************************************/
static inline void sim_switch_writel(void *addr, unsigned offset, u32 data)
{
	*((volatile unsigned int *)(addr + offset)) = data;
}

static inline u32 sim_switch_readl(const void *addr, unsigned offset)
{
	u32 rc = 0;

	rc = *((volatile unsigned int *)(addr + offset));
	return rc;
}

static int set_sim_gpio(unsigned int mode);
static int get_current_ssw_mode(void);

/*define sysfs entry for configuring debug level and sysrq*/
ssize_t ssw_attr_show(struct kobject *kobj, struct attribute *attr,
		      char *buffer)
{
	struct ssw_sys_entry *entry =
	    container_of(attr, struct ssw_sys_entry, attr);
	return entry->show(kobj, buffer);
}

ssize_t ssw_attr_store(struct kobject *kobj, struct attribute *attr,
		       const char *buffer, size_t size)
{
	struct ssw_sys_entry *entry =
	    container_of(attr, struct ssw_sys_entry, attr);
	return entry->store(kobj, buffer, size);
}

ssize_t ssw_mode_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;

	len = scnprintf(ptr, remain, "0x%x\n", get_current_ssw_mode());
	ptr += len;
	remain -= len;
	SSW_DBG("ssw_mode_show\n");

	return PAGE_SIZE - remain;
}

ssize_t ssw_mode_store(struct kobject *kobj, const char *buffer, size_t size)
{
	int mode;
	int res = kstrtoint(buffer, 0, &mode);

	if (res != 1) {
		SSW_DBG("%s: expect 1 numbers\n", __func__);
	} else {
		SSW_DBG("ssw_mode_store %d\n", mode);
		/*Switch sim mode */
		if ((sim_mode_curr != mode)
		    && (SSW_SUCCESS == set_sim_gpio(mode))) {
			sim_mode_curr = mode;
		}
	}
	return size;
}

const struct sysfs_ops ssw_sysfs_ops = {
	.show = ssw_attr_show,
	.store = ssw_attr_store,
};

struct ssw_sys_entry {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, char *page);
	ssize_t (*store)(struct kobject *kobj, const char *page, size_t size);
};

static struct ssw_sys_entry mode_entry = {
	{.name = "mode", .mode = S_IRUGO | S_IWUSR},	/*remove  .owner = NULL,  */
	ssw_mode_show,
	ssw_mode_store,
};

struct attribute *ssw_attributes[] = {
	&mode_entry.attr,
	NULL,
};

struct kobj_type ssw_ktype = {
	.sysfs_ops = &ssw_sysfs_ops,
	.default_attrs = ssw_attributes,
};

static struct ssw_sysobj_t {
	struct kobject kobj;
} ssw_sysobj;

int ssw_sysfs_init(void)
{
	struct ssw_sysobj_t *obj = &ssw_sysobj;

	memset(&obj->kobj, 0x00, sizeof(obj->kobj));

	obj->kobj.parent = kernel_kobj;
	if (kobject_init_and_add(&obj->kobj, &ssw_ktype, NULL, "mtk_ssw")) {
		kobject_put(&obj->kobj);
		return -ENOMEM;
	}
	kobject_uevent(&obj->kobj, KOBJ_ADD);

	return 0;
}

/*************************************************************************/
/*sim switch hardware operation                                                                           */
 /**/
/*************************************************************************/
int get_current_ssw_mode(void)
{
	return sim_mode_curr;
}

static int set_sim_gpio(unsigned int mode)
{
	SSW_DBG("set_sim_gpio: %d\n", mode);
	switch (mode) {
	case SINGLE_TALK_MDSYS:
		mt_set_gpio_mode(GPIO44, 1);
		mt_set_gpio_mode(GPIO45, 1);
		mt_set_gpio_mode(GPIO46, 1);

		mt_set_gpio_mode(GPIO47, 1);
		mt_set_gpio_mode(GPIO48, 1);
		mt_set_gpio_mode(GPIO49, 1);
		break;
	case SINGLE_TALK_MDSYS_LITE:
		mt_set_gpio_mode(GPIO44, 4);
		mt_set_gpio_mode(GPIO45, 4);
		mt_set_gpio_mode(GPIO46, 4);

		mt_set_gpio_mode(GPIO47, 4);
		mt_set_gpio_mode(GPIO48, 4);
		mt_set_gpio_mode(GPIO49, 4);
		break;
	case DUAL_TALK:
		mt_set_gpio_mode(GPIO44, 1);
		mt_set_gpio_mode(GPIO45, 1);
		mt_set_gpio_mode(GPIO46, 1);

		mt_set_gpio_mode(GPIO47, 4);
		mt_set_gpio_mode(GPIO48, 4);
		mt_set_gpio_mode(GPIO49, 4);
		break;
	case DUAL_TALK_SWAP:
		mt_set_gpio_mode(GPIO44, 4);
		mt_set_gpio_mode(GPIO45, 4);
		mt_set_gpio_mode(GPIO46, 4);

		mt_set_gpio_mode(GPIO47, 1);
		mt_set_gpio_mode(GPIO48, 1);
		mt_set_gpio_mode(GPIO49, 1);
		break;
	default:
		SSW_DBG("[Error] Invalid Mode(%d)", mode);
		return SSW_INVALID_PARA;
	}

	SSW_DBG
	    ("Current sim mode(%d), SIM1_MODE(0x%x), SIM2_MODE(0x%x), SIM1_PULL(0x%x), SIM2_PULL(0x%x)\n",
	     mode, sim_switch_readl(reg_base, GPIO_SIM1_MODE),
	     sim_switch_readl(reg_base, GPIO_SIM2_MODE),
	     sim_switch_readl(reg_base, GPIO_SIM1_PULL),
	     sim_switch_readl(reg_base, GPIO_SIM2_PULL));

	return SSW_SUCCESS;
}

static int get_sim_mode(unsigned int mode)
{
	/*ToDo: get mode value from upper layer and convert it to sim mode */
	 /**/ /**/ return mode;
}

int switch_sim_mode(int id, char *buf, unsigned int len)
{
	unsigned int mode = *((unsigned int *)buf);
	unsigned int type = (mode & 0xFFFF0000) >> 16;

	if (type != get_sim_switch_type()) {
		SSW_DBG("[Error]sim switch type is mis-match: type(%d, %d)",
			type, get_sim_switch_type());
		return SSW_INVALID_PARA;
	}

	SSW_DBG("sim switch: %d-->%d\n", mode, sim_mode_curr);

	mode = get_sim_mode(mode);

	mutex_lock(&sim_switch_mutex);

	if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
		sim_mode_curr = mode;


	mutex_unlock(&sim_switch_mutex);

	SSW_DBG("sim switch(%d) OK\n", sim_mode_curr);

	return 0;

}

/*To decide sim mode according to compile option*/
static int get_sim_mode_init(void)
{
	unsigned int sim_mode = 0;

#ifdef MTK_ENABLE_MD1
	sim_mode = SINGLE_TALK_MDSYS;
#ifdef MTK_ENABLE_MD2
	sim_mode = DUAL_TALK;
#endif
#elif defined MTK_ENABLE_MD2
	sim_mode = SINGLE_TALK_MDSYS_LITE;
#endif

	return sim_mode;
}

/*sim switch hardware initial*/
static int sim_switch_init(void)
{
	SSW_DBG("sim_switch_init\n");

	reg_base = (void *)GPIO_BASE;

	/*better to set pull_en and pull_sel first, then mode */
	mt_set_gpio_pull_enable(GPIO44, 1);
	mt_set_gpio_pull_enable(GPIO45, 1);
	mt_set_gpio_pull_enable(GPIO46, 1);
	mt_set_gpio_pull_enable(GPIO47, 1);
	mt_set_gpio_pull_enable(GPIO48, 1);
	mt_set_gpio_pull_enable(GPIO49, 1);

	mt_set_gpio_pull_select(GPIO44, 0);
	mt_set_gpio_pull_select(GPIO45, 0);
	mt_set_gpio_pull_select(GPIO46, 1);
	mt_set_gpio_pull_select(GPIO47, 0);
	mt_set_gpio_pull_select(GPIO48, 0);
	mt_set_gpio_pull_select(GPIO49, 1);

	sim_mode_curr = get_sim_mode_init();
	if (SSW_SUCCESS != set_sim_gpio(sim_mode_curr)) {
		SSW_DBG("sim_switch_init fail\n");
		return SSW_INVALID_PARA;
	}

	return 0;
}

static int sim_switch_probe(struct platform_device *dev)
{
	SSW_DBG("sim_switch_probe\n");

#if __ENABLE_SSW_SYSFS
	ssw_sysfs_init();
#endif

	sim_switch_init();

	/*move this to sim_switch_driver_init(). Because this function not exceute on device tree branch.   */
	/*mutex_init(&sim_switch_mutex); */

	register_ccci_kern_func(ID_SSW_SWITCH_MODE, switch_sim_mode);

	return 0;
}

static int sim_switch_remove(struct platform_device *dev)
{
	/*SSW_DBG("sim_switch_remove\n"); */
	return 0;
}

static void sim_switch_shutdown(struct platform_device *dev)
{
	/*SSW_DBG("sim_switch_shutdown\n"); */
}

static int sim_switch_suspend(struct platform_device *dev, pm_message_t state)
{
	/*SSW_DBG("sim_switch_suspend\n"); */
	return 0;
}

static int sim_switch_resume(struct platform_device *dev)
{
	/*SSW_DBG("sim_switch_resume\n"); */
	return 0;
}

static struct platform_driver sim_switch_driver = {
	.driver = {
		   .name = "sim-switch",
		   },
	.probe = sim_switch_probe,
	.remove = sim_switch_remove,
	.shutdown = sim_switch_shutdown,
	.suspend = sim_switch_suspend,
	.resume = sim_switch_resume,
};

static int __init sim_switch_driver_init(void)
{
	int ret = 0;

	SSW_DBG("sim_switch_driver_init\n");
	ret = platform_driver_register(&sim_switch_driver);
	if (ret) {
		SSW_DBG("ssw_driver register fail(%d)\n", ret);
		return ret;
	}

	mutex_init(&sim_switch_mutex);
	/*sim_switch_init(); */

	return ret;
}

static void __exit sim_switch_driver_exit(void)
{

}

module_init(sim_switch_driver_init);
module_exit(sim_switch_driver_exit);

MODULE_DESCRIPTION("MTK SIM Switch Driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");
