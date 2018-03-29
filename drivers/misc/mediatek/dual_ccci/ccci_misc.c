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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <asm/setup.h>
#include <linux/memblock.h>

#include <mach/mtk_ccci_helper.h>
#include <mt-plat/mt_boot_common.h>
#include <mt-plat/battery_common.h>

#include <mt-plat/upmu_common.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#endif
#include <ccci_common.h>
#include <ccci_platform_cfg.h>

#ifdef FEATURE_GET_MD_GPIO_VAL
#include <linux/gpio.h>
#endif

/*-------------grobal variable define----------------*/
#define SHOW_WARNING_NUM (5)

static char kern_func_err_num[MAX_MD_NUM][MAX_KERN_API];

ccci_kern_func_info ccci_func_table[MAX_MD_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_1000[MAX_MD_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_100[MAX_MD_NUM][MAX_KERN_API];

int (*ccci_sys_msg_notify_func[MAX_MD_NUM]) (int, unsigned int, unsigned int);

#if defined(FEATURE_GET_MD_EINT_ATTR)
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
#define MD_SIM_MAX (16)		/*(MD number * SIM number EACH MD) */

enum sim_hot_plug_eint_queryType {
	SIM_HOT_PLUG_EINT_NUMBER,
	SIM_HOT_PLUG_EINT_DEBOUNCETIME,
	SIM_HOT_PLUG_EINT_POLARITY,
	SIM_HOT_PLUG_EINT_SENSITIVITY,
	SIM_HOT_PLUG_EINT_SOCKETTYPE,
	SIM_HOT_PLUG_EINT_DEDICATEDEN,
	SIM_HOT_PLUG_EINT_SRCPIN,

	SIM_HOT_PLUG_EINT_MAX,
};

enum sim_hot_plug_eint_queryErr {
	ERR_SIM_HOT_PLUG_NULL_POINTER = -13,
	ERR_SIM_HOT_PLUG_QUERY_TYPE,
	ERR_SIM_HOT_PLUG_QUERY_STRING,
};

struct eint_struct {
	int type;		/* sync with MD: value type of MD want to get */
	char *property;		/* property name in the node of dtsi */
	int index;		/* cell index in property */
	int value_sim[MD_SIM_MAX];	/* value of each node of current type from property */
};
struct eint_node_name {
	char *node_name;	/*node name in dtsi */
	int md_id;		/* md_id in node_name, no use currently */
	int sim_id;		/* sim_id in node_name, no use currently */
};
struct eint_node_struct {
	unsigned int ExistFlag;	/* if node exist */
	struct eint_node_name *name;
	struct eint_struct *eint_value;
};

static struct eint_struct md_eint_struct[] = {
	/* ID of MD get, property name,  cell index read from property */
	{SIM_HOT_PLUG_EINT_NUMBER, "interrupts", 0,},
	{SIM_HOT_PLUG_EINT_DEBOUNCETIME, "debounce", 1,},
	{SIM_HOT_PLUG_EINT_POLARITY, "interrupts", 1,},
	{SIM_HOT_PLUG_EINT_SENSITIVITY, "interrupts", 1,},
	{SIM_HOT_PLUG_EINT_SOCKETTYPE, "sockettype", 1,},
	{SIM_HOT_PLUG_EINT_DEDICATEDEN, "dedicated", 1,},
	{SIM_HOT_PLUG_EINT_SRCPIN, "src_pin", 1,},
	{SIM_HOT_PLUG_EINT_MAX, "invalid_type", 0xFF,},
};

static struct eint_node_name md_eint_node[] = {
	{"MD1_SIM1_HOT_PLUG_EINT", 1, 1,},
	{"MD1_SIM2_HOT_PLUG_EINT", 1, 2,},
	{"MD1_SIM3_HOT_PLUG_EINT", 1, 3,},
	{"MD1_SIM4_HOT_PLUG_EINT", 1, 4,},
	/* {"MD1_SIM5_HOT_PLUG_EINT", 1, 5, }, */
	/* {"MD1_SIM6_HOT_PLUG_EINT", 1, 6, }, */
	/* {"MD1_SIM7_HOT_PLUG_EINT", 1, 7, }, */
	/* {"MD1_SIM8_HOT_PLUG_EINT", 1, 8, }, */
	/* {"MD2_SIM1_HOT_PLUG_EINT", 2, 1, }, */
	/* {"MD2_SIM2_HOT_PLUG_EINT", 2, 2, }, */
	/* {"MD2_SIM3_HOT_PLUG_EINT", 2, 3, }, */
	/* {"MD2_SIM4_HOT_PLUG_EINT", 2, 4, }, */
	/* {"MD2_SIM5_HOT_PLUG_EINT", 2, 5, }, */
	/* {"MD2_SIM6_HOT_PLUG_EINT", 2, 6, }, */
	/* {"MD2_SIM7_HOT_PLUG_EINT", 2, 7, }, */
	/* {"MD2_SIM8_HOT_PLUG_EINT", 2, 8, }, */
	{NULL,},
};

struct eint_node_struct eint_node_prop = {
	0,
	md_eint_node,
	md_eint_struct,
};

static int get_eint_attr_val(struct device_node *node, int index)
{
	int value;
	int ret = 0, type;

	for (type = 0; type < SIM_HOT_PLUG_EINT_MAX; type++) {
		switch (type) {
		case SIM_HOT_PLUG_EINT_NUMBER:
		case SIM_HOT_PLUG_EINT_DEBOUNCETIME:
		case SIM_HOT_PLUG_EINT_POLARITY:
		case SIM_HOT_PLUG_EINT_SENSITIVITY:
		case SIM_HOT_PLUG_EINT_SOCKETTYPE:
		case SIM_HOT_PLUG_EINT_DEDICATEDEN:
		case SIM_HOT_PLUG_EINT_SRCPIN:
			ret =
			    of_property_read_u32_index(node, md_eint_struct[type].property, md_eint_struct[type].index,
						       &value);
			break;
		default:
			/* CCCI_ERR_MSG(-1, TAG, "maybe you add one type, but no process it!\n"); */
			ret = -1;
			break;
		}
		if (!ret) {
			/* special case: polarity's position == sensitivity's start[ */
			if (type == SIM_HOT_PLUG_EINT_POLARITY) {
				switch (value) {
				case IRQ_TYPE_EDGE_RISING:
				case IRQ_TYPE_EDGE_FALLING:
				case IRQ_TYPE_LEVEL_HIGH:
				case IRQ_TYPE_LEVEL_LOW:
					md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] =
					    (value & 0x5) ? 1 : 0;
					/*1/4: IRQ_TYPE_EDGE_RISING/IRQ_TYPE_LEVEL_HIGH Set 1*/
					md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] =
					    (value & 0x3) ? 1 : 0;
					/*1/2: IRQ_TYPE_EDGE_RISING/IRQ_TYPE_LEVEL_FALLING Set 1*/
					break;
				default:	/* invalid */
					md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] = -1;
					md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] = -1;
					CCCI_MSG_INF(-1, "hlp", "invalid value, please check dtsi!\n");
					break;
				}
				type++;
			} /* special case: polarity's position == sensitivity's end] */
			else
				md_eint_struct[type].value_sim[index] = value;
		} else {
			md_eint_struct[type].value_sim[index] = ERR_SIM_HOT_PLUG_QUERY_TYPE;
			return ERR_SIM_HOT_PLUG_QUERY_TYPE;
		}
	}
	return 0;
}

void get_dtsi_eint_node(void)
{
	int i;
	struct device_node *node;

	for (i = 0; i < MD_SIM_MAX; i++) {
		if (eint_node_prop.name[i].node_name != NULL) {
			/* CCCI_INF_MSG(-1, TAG, "node_%d__ %d\n", i,
				(int)(strlen(eint_node_prop.name[i].node_name))); */
			if (strlen(eint_node_prop.name[i].node_name) > 0) {
				node = of_find_node_by_name(NULL, eint_node_prop.name[i].node_name);
				if (node != NULL) {
					eint_node_prop.ExistFlag |= (1 << i);
					get_eint_attr_val(node, i);
				} else {
					CCCI_MSG_INF(-1, "hlp", "%s: node %d no found\n",
						     eint_node_prop.name[i].node_name, i);
				}
			}
		} else {
			CCCI_MSG_INF(-1, "hlp", "node %d is NULL\n", i);
			break;
		}
	}
}

int get_eint_attr_DTSVal(char *name, unsigned int name_len, unsigned int type, char *result, unsigned int *len)
{
	int i, sim_value;
	int *sim_info = (int *)result;

	if ((name == NULL) || (result == NULL) || (len == NULL))
		return ERR_SIM_HOT_PLUG_NULL_POINTER;
	if (type >= SIM_HOT_PLUG_EINT_MAX)
		return ERR_SIM_HOT_PLUG_QUERY_TYPE;

	for (i = 0; i < MD_SIM_MAX; i++) {
		if (eint_node_prop.ExistFlag & (1 << i)) {
			if (!(strncmp(name, eint_node_prop.name[i].node_name, name_len))) {
				sim_value = eint_node_prop.eint_value[type].value_sim[i];
				*len = sizeof(sim_value);
				memcpy(sim_info, &sim_value, *len);
				return 0;
			}
		}
	}
	return ERR_SIM_HOT_PLUG_QUERY_STRING;
}
#else
extern int get_eint_attribute(char *name, unsigned int name_len,
			      unsigned int type, char *result,
			      unsigned int *len);
#endif

#endif

/***************************************************************************/
/*provide API called by ccci module                                                                           */
/**/
/***************************************************************************/
#if defined(FEATURE_GET_MD_GPIO_NUM)
#ifndef GPIO_SIM_SWITCH_DAT_PIN
#define GPIO_SIM_SWITCH_DAT_PIN (34)
#endif

#ifndef GPIO_SIM_SWITCH_CLK_PIN
#define GPIO_SIM_SWITCH_CLK_PIN (67)
#endif

struct gpio_item {
	char gpio_name_from_md[64];
	char gpio_name_from_dts[64];
	int dummy_value;
};
static struct gpio_item gpio_mapping_table[] = {
	{"GPIO_AST_Reset", "GPIO_AST_RESET", -1},
	{"GPIO_AST_Wakeup", "GPIO_AST_WAkEUP", -1},
	{"GPIO_AST_AFC_Switch", "GPIO_AST_AFC_SWITCH", -1},
	{"GPIO_FDD_BAND_Support_Detection_1", "GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN", -1},
	{"GPIO_FDD_BAND_Support_Detection_2", "GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN", -1},
	{"GPIO_FDD_BAND_Support_Detection_3", "GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN", -1},
	{"GPIO_SIM_SWITCH_CLK", "GPIO_SIM_SWITCH_CLK_PIN", GPIO_SIM_SWITCH_CLK_PIN},
	{"GPIO_SIM_SWITCH_DAT", "GPIO_SIM_SWITCH_DAT_PIN", GPIO_SIM_SWITCH_DAT_PIN},
};
#endif

int get_td_eint_info(int md_id, char *eint_name, unsigned int len)
{
#if defined(FEATURE_GET_TD_EINT_NUM)
	return get_td_eint_num(eint_name, len);
#else
	return -1;
#endif
}

int get_md_gpio_info(int md_id, char *gpio_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_GPIO_NUM)
	int i = 0;
	struct device_node *node = NULL;
	int gpio_id = -1;


	CCCI_DBG_MSG(-1, "hlp", "searching %s in device tree\n", gpio_name);
	for (i = 0; i < ARRAY_SIZE(gpio_mapping_table); i++) {
		if (!strncmp(gpio_name, gpio_mapping_table[i].gpio_name_from_md, len))
			break;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,gpio_usage_mapping");
	if (!node) {
		if (i < ARRAY_SIZE(gpio_mapping_table))
			gpio_id = gpio_mapping_table[i].dummy_value;
		CCCI_MSG_INF(-1, "hlp", "MD_USE_GPIO is not set in device tree, use dummy value %d\n", gpio_id);
		return gpio_id;
	}
	if (i < ARRAY_SIZE(gpio_mapping_table)) {
		CCCI_MSG_INF(-1, "hlp", "%s found in device tree\n", gpio_mapping_table[i].gpio_name_from_dts);
		of_property_read_u32(node, gpio_mapping_table[i].gpio_name_from_dts, &gpio_id);
	}
	/* if gpio_name_from_md and gpio_name_from_dts are the same,
	   it will not be listed in gpio_mapping_table,
	   so try read directly from device tree here.
	*/
	if (gpio_id < 0)
		of_property_read_u32(node, gpio_name, &gpio_id);
	/* no device tree node can be read, then return dummy value*/
	if (gpio_id < 0 && i < ARRAY_SIZE(gpio_mapping_table)) {
		gpio_id = gpio_mapping_table[i].dummy_value;
		CCCI_MSG_INF(-1, "hlp", "%s id use dummy value %d\n", gpio_name, gpio_id);
	}  else
		CCCI_MSG_INF(-1, "hlp", "%s id %d\n", gpio_name, gpio_id);
	return gpio_id;

#else
	return -1;
#endif

}

int get_md_gpio_val(int md_id, unsigned int num)
{
#if defined(FEATURE_GET_MD_GPIO_VAL)
#if defined(CONFIG_MTK_LEGACY)
		return mt_get_gpio_in(num);
#else
		return __gpio_get_value(num);
#endif
#else
		return -1;
#endif

}

int get_md_adc_info(int md_id, char *adc_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_ADC_NUM)
	return IMM_get_adc_channel_num(adc_name, len);

#else
	return -1;
#endif
}

int get_md_adc_val(int md_id, unsigned int num)
{
#if defined(FEATURE_GET_MD_ADC_VAL)
	int data[4] = { 0, 0, 0, 0 };
	int val = 0;
	int ret = 0;

	ret = IMM_GetOneChannelValue(num, data, &val);
	if (ret == 0)
		return val;
	else
		return ret;

#else
	return -1;
#endif
}

int get_eint_attr(char *name, unsigned int name_len, unsigned int type,
		  char *result, unsigned int *len)
{
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
	return get_eint_attr_DTSVal(name, name_len, type, result, len);
#else
#if defined(FEATURE_GET_MD_EINT_ATTR)
	return get_eint_attribute(name, name_len, type, result, len);
#else
	return -1;
#endif
#endif

}

int get_dram_type_clk(int *clk, int *type)
{
#if defined(FEATURE_GET_DRAM_TYPE_CLK)
	return get_dram_info(clk, type);
#else
	return -1;
#endif
}

void md_fast_dormancy(int md_id)
{
#if defined(FEATURE_MD_FAST_DORMANCY)
#ifdef CONFIG_MTK_FD_SUPPORT
	exec_ccci_kern_func_by_md_id(md_id, ID_CCCI_DORMANCY, NULL, 0);
#endif
#endif
}

int get_bat_info(unsigned int para)
{
#if defined(FEATURE_GET_MD_BAT_VOL)
	return (int)BAT_Get_Battery_Voltage(0);
#else
	return -1;
#endif
}

/***************************************************************************/
/*Register kernel API for ccci driver invoking                                                               */
/**/
/***************************************************************************/
int register_ccci_kern_func_by_md_id(int md_id, unsigned int id,
				     ccci_kern_cb_func_t func)
{
	int ret = 0;
	ccci_kern_func_info *info_ptr;

	if ((id >= MAX_KERN_API) || (func == NULL) || (md_id >= MAX_MD_NUM)) {
		CCCI_MSG_INF(-1, "hlp", "register kern func fail: md_id:%d, func_id:%d!\n", md_id + 1, id);
		return E_PARAM;
	}

	info_ptr = &(ccci_func_table[md_id][id]);
	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else
		CCCI_MSG_INF(-1, "hlp", "(%d)register kern func fail: func(%d) registered!\n",
		     md_id + 1, id);

	return ret;
}

int register_ccci_kern_func(unsigned int id, ccci_kern_cb_func_t func)
{
	return register_ccci_kern_func_by_md_id(MD_SYS1, id, func);
}

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
				 unsigned int len)
{
	ccci_kern_cb_func_t func;
	int ret = 0;

	if (md_id >= MAX_MD_NUM) {
		CCCI_MSG_INF(-1, "hlp", "exec kern func fail: invalid md id(%d)\n", md_id + 1);
		return E_PARAM;
	}

	if (id >= MAX_KERN_API) {
		CCCI_MSG_INF(-1, "hlp", "(%d)exec kern func fail: invalid func id(%d)!\n",
		     md_id, id);
		return E_PARAM;
	}

	func = ccci_func_table[md_id][id].func;
	if (func != NULL) {
		ret = func(md_id, buf, len);
	} else {
		ret = E_NO_EXIST;
		if (kern_func_err_num[md_id][id] < SHOW_WARNING_NUM) {
			kern_func_err_num[md_id][id]++;
			CCCI_MSG_INF(-1, "hlp", "(%d)exec kern func fail: func%d not register!\n",
			     md_id + 1, id);
		}
	}

	return ret;
}

int exec_ccci_kern_func(unsigned int id, char *buf, unsigned int len)
{
	return exec_ccci_kern_func_by_md_id(MD_SYS1, id, buf, len);
}

/***************************************************************************/
/*Register ccci call back function when AP receive system channel message                    */
/**/
/***************************************************************************/
int register_sys_msg_notify_func(int md_id,
				 int (*func)(int, unsigned int, unsigned int))
{
	int ret = 0;

	if (md_id >= MAX_MD_NUM) {
		CCCI_MSG_INF(-1, "hlp", "register_sys_msg_notify_func fail: invalid md id(%d)\n",
		     md_id + 1);
		return E_PARAM;
	}

	if (ccci_sys_msg_notify_func[md_id] == NULL)
		ccci_sys_msg_notify_func[md_id] = func;
	else
		CCCI_MSG_INF(-1, "hlp", "ccci_sys_msg_notify_func fail: func registered!\n");

	return ret;
}

int notify_md_by_sys_msg(int md_id, unsigned int msg, unsigned int data)
{
	int ret = 0;
	int (*func)(int, unsigned int, unsigned int);

	if (md_id >= MAX_MD_NUM) {
		CCCI_MSG_INF(-1, "hlp", "notify_md_by_sys_msg: invalid md id(%d)\n",
		     md_id + 1);
		return E_PARAM;
	}

	func = ccci_sys_msg_notify_func[md_id];
	if (func != NULL) {
		ret = func(md_id, msg, data);
	} else {
		ret = E_NO_EXIST;
		CCCI_MSG_INF(-1, "hlp", "notify_md_by_sys_msg fail: func not register!\n");
	}

	return ret;
}

int register_ccci_sys_call_back(int md_id, unsigned int id,
				ccci_sys_cb_func_t func)
{
	int ret = 0;
	ccci_sys_cb_func_info_t *info_ptr;

	if (md_id >= MAX_MD_NUM) {
		CCCI_MSG_INF(-1, "hlp", "register_sys_call_back fail: invalid md id(%d)\n",
		     md_id + 1);
		return E_PARAM;
	}

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[md_id][id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[md_id][id - 0x1000]);
	} else {
		CCCI_MSG_INF(-1, "hlp", "register_sys_call_back fail: invalid func id(0x%x)\n", id);
		return E_PARAM;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else
		CCCI_MSG_INF(-1, "hlp", "register_sys_call_back fail: func(0x%x) registered!\n", id);

	return ret;
}

void exec_ccci_sys_call_back(int md_id, int cb_id, int data)
{
	ccci_sys_cb_func_t func;
	int id;
	ccci_sys_cb_func_info_t *curr_table;

	if (md_id >= MAX_MD_NUM) {
		CCCI_MSG_INF(-1, "hlp", "exec_sys_cb fail: invalid md id(%d)\n",
		       md_id + 1);
		return;
	}

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		CCCI_MSG_INF(-1, "hlp", "(%d)exec_sys_cb fail: invalid func id(0x%x)\n",
		     md_id + 1, cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000[md_id];
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100[md_id];
	} else {
		CCCI_MSG_INF(-1, "hlp", "(%d)exec_sys_cb fail: invalid func id(0x%x)\n",
		     md_id + 1, cb_id);
		return;
	}

	func = curr_table[id].func;
	if (func != NULL) {
		func(md_id, data);
	} else {
		CCCI_MSG_INF(-1, "hlp", "(%d)exec_sys_cb fail: func id(0x%x) not register!\n",
		     md_id + 1, cb_id);
	}
}

int ccci_helper_init(void)
{
	/*init ccci kernel API register table */
	memset((void *)ccci_func_table, 0, sizeof(ccci_func_table));
	memset((void *)kern_func_err_num, 0, sizeof(kern_func_err_num));

	/*init ccci system channel call back function register table */
	memset((void *)ccci_sys_cb_table_100, 0, sizeof(ccci_sys_cb_table_100));
	memset((void *)ccci_sys_cb_table_1000, 0,
	       sizeof(ccci_sys_cb_table_1000));

#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
	get_dtsi_eint_node();
#endif
	return 0;
}

void ccci_helper_exit(void)
{
	CCCI_MSG_INF(-1, "hlp", "ccci_helper_exit\n");

	/*free ccci kernel API register table */
	memset((void *)ccci_func_table, 0, sizeof(ccci_func_table));
	memset((void *)ccci_sys_msg_notify_func, 0,
	       sizeof(ccci_sys_msg_notify_func));

	/*free ccci system channel call back function register table */
	memset((void *)ccci_sys_cb_table_100, 0, sizeof(ccci_sys_cb_table_100));
	memset((void *)ccci_sys_cb_table_1000, 0,
	       sizeof(ccci_sys_cb_table_1000));
}

