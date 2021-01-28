// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/module.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include "ccci_common_config.h"

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif
#ifdef FEATURE_RF_CLK_BUF
#include <mtk-clkbuf-bridge.h>
#endif

#include "ccci_core.h"
#include "ccci_auxadc.h"
#include "ccci_bm.h"
#include "ccci_modem.h"
#include "port_rpc.h"
#define MAX_QUEUE_LENGTH 16

static struct gpio_item gpio_mapping_table[] = {
	{"GPIO_FDD_Band_Support_Detection_1",
		"GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN",},
	{"GPIO_FDD_Band_Support_Detection_2",
		"GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN",},
	{"GPIO_FDD_Band_Support_Detection_3",
		"GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN",},
	{"GPIO_FDD_Band_Support_Detection_4",
		"GPIO_FDD_BAND_SUPPORT_DETECT_4TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_5",
		"GPIO_FDD_BAND_SUPPORT_DETECT_5TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_6",
		"GPIO_FDD_BAND_SUPPORT_DETECT_6TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_7",
		"GPIO_FDD_BAND_SUPPORT_DETECT_7TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_8",
		"GPIO_FDD_BAND_SUPPORT_DETECT_8TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_9",
		"GPIO_FDD_BAND_SUPPORT_DETECT_9TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_A",
		"GPIO_FDD_BAND_SUPPORT_DETECT_ATH_PIN",},
};

static int get_md_gpio_val(unsigned int num)
{
	return gpio_get_value(num);
}

static int get_md_adc_val(__attribute__((unused))unsigned int num)
{
#ifdef CONFIG_MTK_AUXADC
	int data[4] = { 0, 0, 0, 0 };
	int val = 0;
	int ret = 0;

	ret = IMM_GetOneChannelValue(num, data, &val);
	if (ret == 0)
		return val;
	else
		return ret;
#endif

#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
	return ccci_get_adc_val();
#endif
	CCCI_ERROR_LOG(0, RPC, "ERR:CONFIG AUXADC and IIO not ready");
	return -1;
}


static int get_td_eint_info(char *eint_name, unsigned int len)
{
	return -1;
}

static int get_md_adc_info(__attribute__((unused))char *adc_name,
			   __attribute__((unused))unsigned int len)
{
#ifdef CONFIG_MTK_AUXADC
	return IMM_get_adc_channel_num(adc_name, len);
#endif

#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
	return ccci_get_adc_num();
#endif
	CCCI_ERROR_LOG(0, RPC, "ERR:CONFIG AUXADC and IIO not ready");
	return -1;
}

static char *md_gpio_name_convert(char *gpio_name, unsigned int len)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_mapping_table); i++) {
		if (!strncmp(gpio_name, gpio_mapping_table[i].gpio_name_from_md,
			len))
			return gpio_mapping_table[i].gpio_name_from_dts;
	}

	return NULL;
}

static int get_gpio_id_from_dt(struct device_node *node,
	char *gpio_name, int *md_view_id)
{
	int gpio_id = -1;
	int md_view_gpio_id = -1;
	int ret;

	/* For new API, there is a shift between AP GPIO ID and MD GPIO ID */
	gpio_id = of_get_named_gpio(node, gpio_name, 0);
	ret = of_property_read_u32_index(node, gpio_name, 1, &md_view_gpio_id);
	if (ret)
		return ret;

	if (gpio_id >= 0)
		*md_view_id = md_view_gpio_id;
	return gpio_id;
}

static int get_md_gpio_info(char *gpio_name,
	unsigned int len, int *md_view_gpio_id)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL,
		"mediatek,gpio_usage_mapping");
	int gpio_id = -1;
	char *name;

	if (len >= 4096) {
		CCCI_NORMAL_LOG(0, RPC,
			"MD GPIO name length abnoremal(%d)\n", len);
		return gpio_id;
	}

	if (!node) {
		CCCI_NORMAL_LOG(0, RPC,
			"MD_USE_GPIO is not set in device tree,need to check?\n");
		return gpio_id;
	}

	name = md_gpio_name_convert(gpio_name, len);
	if (name) {
		gpio_id = get_gpio_id_from_dt(node, name, md_view_gpio_id);
		return gpio_id;
	}
	if (gpio_name[len-1] != 0) {
		name = kmalloc(len + 1, GFP_KERNEL);
		if (name) {
			memcpy(name, gpio_name, len);
			name[len] = 0;
			gpio_id = get_gpio_id_from_dt(node, name,
				md_view_gpio_id);
			kfree(name);
			return gpio_id;
		}
		CCCI_BOOTUP_LOG(0, RPC,
			"alloc memory fail for gpio with size:%d\n", len);
		return gpio_id;
	}
	gpio_id = get_gpio_id_from_dt(node, gpio_name, md_view_gpio_id);
	return gpio_id;
}

static void md_drdi_gpio_status_scan(void)
{
	int i;
	int size;
	int gpio_id;
	int gpio_md_view;
	char *curr;
	int val;

	CCCI_BOOTUP_LOG(0, RPC, "scan didr gpio status\n");
	for (i = 0; i < ARRAY_SIZE(gpio_mapping_table); i++) {
		curr = gpio_mapping_table[i].gpio_name_from_md;
		size = strlen(curr) + 1;
		gpio_md_view = -1;
		gpio_id = get_md_gpio_info(curr, size, &gpio_md_view);
		if (gpio_id >= 0) {
			val = get_md_gpio_val(gpio_id);
			CCCI_BOOTUP_LOG(0, RPC, "GPIO[%s]%d(%d@md),val:%d\n",
					curr, gpio_id, gpio_md_view, val);
		}
	}
}

static int get_dram_type_clk(int *clk, int *type)
{
	return -1;
}

static struct eint_struct md_eint_struct[] = {
	/* ID of MD get, property name,  cell index read from property */
	{SIM_EINT_NUM, "interrupts", 0,},
	{SIM_EINT_DEBOUNCE, "debounce", 1,},
	{SIM_EINT_POLA, "interrupts", 1,},
	{SIM_EINT_SENS, "interrupts", 1,},
	{SIM_EINT_SOCKE, "sockettype", 1,},
	{SIM_EINT_DEDICATEDEN, "dedicated", 1,},
	{SIM_EINT_SRCPIN, "src_pin", 1,},
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

static int get_eint_attr_val(int md_id, struct device_node *node, int index)
{
	int value;
	int ret = 0, type;

	/* unit of AP eint is us, but unit of MD eint is ms.
	 * So need covertion here.
	 */
	int covert_AP_to_MD_unit = 1000;

	for (type = 0; type < SIM_HOT_PLUG_EINT_MAX; type++) {
		ret = of_property_read_u32_index(node,
			md_eint_struct[type].property,
			md_eint_struct[type].index, &value);
		if (ret != 0) {
			md_eint_struct[type].value_sim[index] =
			ERR_SIM_HOT_PLUG_QUERY_TYPE;
			CCCI_NORMAL_LOG(md_id, RPC, "%s:  not found\n",
			md_eint_struct[type].property);
			ret = ERR_SIM_HOT_PLUG_QUERY_TYPE;
			continue;
		}
		/* special case: polarity's position == sensitivity's start[ */
		if (type == SIM_EINT_POLA) {
			switch (value) {
			case IRQ_TYPE_EDGE_RISING:
			case IRQ_TYPE_EDGE_FALLING:
			case IRQ_TYPE_LEVEL_HIGH:
			case IRQ_TYPE_LEVEL_LOW:
				md_eint_struct[SIM_EINT_POLA].value_sim[index]
					= (value & 0x5) ? 1 : 0;
				/* 1/4:
				 * IRQ_TYPE_EDGE_RISING/
				 * IRQ_TYPE_LEVEL_HIGH Set 1
				 */
				md_eint_struct[SIM_EINT_SENS].value_sim[index]
					= (value & 0x3) ? 1 : 0;
				/* 1/2:
				 * IRQ_TYPE_EDGE_RISING/
				 * IRQ_TYPE_LEVEL_FALLING Set 1
				 */
				break;
			default:	/* invalid */
				md_eint_struct[SIM_EINT_POLA].value_sim[index]
					= -1;
				md_eint_struct[SIM_EINT_SENS].value_sim[index]
					= -1;
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid value, please check dtsi!\n");
				break;
			}
			type++;
		} else if (type == SIM_EINT_DEBOUNCE) {
			/* debounce time should divide by 1000 due
			 * to different unit in AP and MD.
			 */
			md_eint_struct[type].value_sim[index] =
				value/covert_AP_to_MD_unit;
		} else
			md_eint_struct[type].value_sim[index] = value;
	}
	return ret;
}

void get_dtsi_eint_node(int md_id)
{
	static int init; /*default is 0*/
	int i;
	struct device_node *node;

	if (init)
		return;
	init = 1;
	for (i = 0; i < MD_SIM_MAX; i++) {
		if (eint_node_prop.name[i].node_name == NULL) {
			CCCI_INIT_LOG(md_id, RPC, "node %d is NULL\n", i);
			break;
		}
		node = of_find_node_by_name(NULL,
			eint_node_prop.name[i].node_name);
		if (node != NULL) {
			eint_node_prop.ExistFlag |= (1 << i);
			get_eint_attr_val(md_id, node, i);
		} else {
			CCCI_INIT_LOG(md_id, RPC, "%s: node %d no found\n",
				     eint_node_prop.name[i].node_name, i);
		}
	}
}

int get_eint_attr_DTSVal(int md_id, char *name, unsigned int name_len,
			unsigned int type, char *result, unsigned int *len)
{
	int i, sim_value;
	int *sim_info = (int *)result;

	if ((name == NULL) || (result == NULL) || (len == NULL))
		return ERR_SIM_HOT_PLUG_NULL_POINTER;
	if (type >= SIM_HOT_PLUG_EINT_MAX)
		return ERR_SIM_HOT_PLUG_QUERY_TYPE;

	for (i = 0; i < MD_SIM_MAX; i++) {
		if ((eint_node_prop.ExistFlag & (1 << i)) == 0)
			continue;
		if (!(strncmp(name,
			eint_node_prop.name[i].node_name, name_len))) {
			sim_value =
			eint_node_prop.eint_value[type].value_sim[i];
			*len = sizeof(sim_value);
			memcpy(sim_info, &sim_value, *len);
			CCCI_BOOTUP_LOG(md_id, RPC,
			"md_eint:%s, sizeof: %d, sim_info: %d, %d\n",
			eint_node_prop.eint_value[type].property,
			*len, *sim_info,
			eint_node_prop.eint_value[type].value_sim[i]);
			if (sim_value >= 0)
				return 0;
		}
	}
	return ERR_SIM_HOT_PLUG_QUERY_STRING;
}

static int get_eint_attr(int md_id, char *name, unsigned int name_len,
			unsigned int type, char *result, unsigned int *len)
{
	return get_eint_attr_DTSVal(md_id, name, name_len, type, result, len);
}

static void get_md_dtsi_val(struct ccci_rpc_md_dtsi_input *input,
	struct ccci_rpc_md_dtsi_output *output)
{
	int ret = -1;
	int value = 0;
	struct device_node *node =
	of_find_compatible_node(NULL, NULL, "mediatek,md_attr_node");

	if (node == NULL) {
		CCCI_INIT_LOG(-1, RPC, "%s: No node: %s\n", __func__,
			input->strName);
		CCCI_NORMAL_LOG(-1, RPC, "%s: No node: %s\n", __func__,
			input->strName);
		return;
	}

	switch (input->req) {
	case RPC_REQ_PROP_VALUE:
		ret = of_property_read_u32(node, input->strName, &value);
		if (ret == 0)
			output->retValue = value;
		break;
	}
	CCCI_INIT_LOG(-1, RPC, "%s %d, %s -- 0x%x\n", __func__,
		input->req, input->strName, output->retValue);
	CCCI_NORMAL_LOG(-1, RPC, "%s %d, %s -- 0x%x\n", __func__,
		input->req, input->strName, output->retValue);
}

static void get_md_dtsi_debug(void)
{
	struct ccci_rpc_md_dtsi_input input;
	struct ccci_rpc_md_dtsi_output output;
	int ret;

	input.req = RPC_REQ_PROP_VALUE;
	output.retValue = 0;
	ret = snprintf(input.strName, sizeof(input.strName), "%s",
		"mediatek,md_drdi_rf_set_idx");
	if (ret <= 0 || ret >= sizeof(input.strName)) {
		CCCI_ERROR_LOG(-1, RPC, "%s:snprintf input.strName fail\n",
			__func__);
		return;
	}
	get_md_dtsi_val(&input, &output);
}

static void ccci_rpc_get_gpio_adc(struct ccci_rpc_gpio_adc_intput *input,
	struct ccci_rpc_gpio_adc_output *output)
{
	int num;
	unsigned int val, i, md_val = -1;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) ==
		(RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i],
						strlen(input->gpioPinName[i]),
						&md_val);
				if (num >= 0) {
					output->gpioPinNum[i] = md_val;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(
					input->gpioPinName[i],
					strlen(input->gpioPinName[i]), &md_val);
					if (num >= 0)
						output->gpioPinNum[i] = md_val;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(
					input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) ==
		(RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName,
				strlen(input->adcChName));

		if (num >= 0) {
			output->adcChNum = num;
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(num);
				output->adcChMeasSum += val;
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_ADC_PIN) {
			num = get_md_adc_info(input->adcChName,
					strlen(input->adcChName));
			if (num >= 0)
				output->adcChNum = num;
		}
		if (input->reqMask & RPC_REQ_ADC_VALUE) {
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(input->adcChNum);
				output->adcChMeasSum += val;
			}
		}
	}
}

static void ccci_rpc_get_gpio_adc_v2(struct ccci_rpc_gpio_adc_intput_v2 *input,
	struct ccci_rpc_gpio_adc_output_v2 *output)
{
	int num, md_val = -1;
	unsigned int val, i;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) ==
		(RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i],
						strlen(input->gpioPinName[i]),
						&md_val);
				if (num >= 0) {
					output->gpioPinNum[i] = md_val;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(
						input->gpioPinName[i],
						strlen(input->gpioPinName[i]),
						&md_val);
					if (num >= 0)
						output->gpioPinNum[i] = md_val;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(
							input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) ==
		(RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName,
				strlen(input->adcChName));
		if (num >= 0) {
			output->adcChNum = num;
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(num);
				output->adcChMeasSum += val;
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_ADC_PIN) {
			num = get_md_adc_info(input->adcChName,
					strlen(input->adcChName));
			if (num >= 0)
				output->adcChNum = num;
		}
		if (input->reqMask & RPC_REQ_ADC_VALUE) {
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(input->adcChNum);
				output->adcChMeasSum += val;
			}
		}
	}
}

static int ccci_rpc_remap_queue(int md_id, struct ccci_rpc_queue_mapping *remap)
{
	struct port_t *port;

	port = port_get_by_minor(md_id, remap->net_if + CCCI_NET_MINOR_BASE);

	if (!port) {
		CCCI_ERROR_LOG(md_id, RPC, "can't find ccmni for netif: %d\n",
			remap->net_if);
		return -1;
	}

	if (remap->lhif_q == LHIF_HWQ_AP_UL_Q0) {
		/*normal queue*/
		port->txq_index = 0;
		port->txq_exp_index = 0xF0 | 0x1;
		CCCI_NORMAL_LOG(md_id, RPC, "remap port %s Tx to cldma%d\n",
			port->name, port->txq_index);
	} else if (remap->lhif_q == LHIF_HWQ_AP_UL_Q1) {
		/*IMS queue*/
		port->txq_index = 3;
		port->txq_exp_index = 0xF0 | 0x3;
		CCCI_NORMAL_LOG(md_id, RPC, "remap port %s Tx to cldma%d\n",
			port->name, port->txq_index);
	} else
		CCCI_ERROR_LOG(md_id, RPC, "invalid remap for q%d\n",
			remap->lhif_q);

	return 0;
}

static void ccci_rpc_work_helper(struct port_t *port, struct rpc_pkt *pkt,
	struct rpc_buffer *p_rpc_buf, unsigned int tmp_data[])
{
	/*
	 * tmp_data[] is used to make sure memory address is valid
	 * after this function return, be careful with the size!
	 */
	int pkt_num = p_rpc_buf->para_num;
	int md_id = port->md_id;
	int md_val = -1;

	CCCI_DEBUG_LOG(md_id, RPC, "%s++ %d\n", __func__,
		p_rpc_buf->para_num);
	tmp_data[0] = 0;
	switch (p_rpc_buf->op_id) {
	/* call EINT API to get TDD EINT configuration for modem EINT initial */
	case IPC_RPC_GET_TDD_EINT_NUM_OP:
	case IPC_RPC_GET_GPIO_NUM_OP:
	case IPC_RPC_GET_ADC_NUM_OP:
		{
			int get_num = 0;
			unsigned char *name = NULL;
			unsigned int length = 0;

			if (pkt_num < 2 || pkt_num > RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md_id, RPC,
				"invalid parameter for [0x%X]: pkt_num=%d!\n",
				p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}
			length = pkt[0].len;
			if (length < 1) {
				CCCI_ERROR_LOG(md_id, RPC,
				"invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
				p_rpc_buf->op_id, pkt_num, length);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}

			name = kmalloc(length, GFP_KERNEL);
			if (name == NULL) {
				CCCI_ERROR_LOG(md_id, RPC,
				"Fail alloc Mem for [0x%X]!\n",
				p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err1;
			} else {
				memcpy(name, (unsigned char *)(pkt[0].buf),
				length);

				if (p_rpc_buf->op_id ==
					IPC_RPC_GET_TDD_EINT_NUM_OP) {
					get_num = get_td_eint_info(name,
								length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				} else if (p_rpc_buf->op_id ==
						IPC_RPC_GET_GPIO_NUM_OP) {
					get_num = get_md_gpio_info(name,
								length,
								&md_val);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
					else
						get_num = md_val;
				} else if (p_rpc_buf->op_id ==
						IPC_RPC_GET_ADC_NUM_OP) {
					get_num = get_md_adc_info(name,
								length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				}

				CCCI_NORMAL_LOG(md_id, RPC,
					"[0x%08X]: name:%s, len=%d, get_num:%d\n",
					p_rpc_buf->op_id, name,
					length, get_num);
				pkt_num = 0;

				/* NOTE: tmp_data[1] not [0] */
				tmp_data[1] = (unsigned int)get_num;
				/* get_num may be invalid after
				 * exit this function
				 */
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
				kfree(name);
			}
			break;

 err1:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_EMI_CLK_TYPE_OP:
		{
			int dram_type = 0;
			int dram_clk = 0;

			if (pkt_num != 0) {
				CCCI_ERROR_LOG(md_id, RPC,
				"invalid parameter for [0x%X]: pkt_num=%d!\n",
				p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err2;
			}

			if (get_dram_type_clk(&dram_clk, &dram_type)) {
				tmp_data[0] = FS_FUNC_FAIL;
				goto err2;
			} else {
				tmp_data[0] = 0;
				CCCI_NORMAL_LOG(md_id, RPC,
				"[0x%08X]: dram_clk: %d, dram_type:%d\n",
				p_rpc_buf->op_id, dram_clk, dram_type);
			}

			tmp_data[1] = (unsigned int)dram_type;
			tmp_data[2] = (unsigned int)dram_clk;

			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[0]);
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[2]);
			break;

 err2:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_EINT_ATTR_OP:
		{
			char *eint_name = NULL;
			unsigned int name_len = 0;
			unsigned int type = 0;
			char *res = NULL;
			unsigned int res_len = 0;
			int ret = 0;

			if (pkt_num < 3 || pkt_num > RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md_id, RPC,
				"invalid parameter for [0x%X]: pkt_num=%d!\n",
				p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}
			name_len = pkt[0].len;
			if (name_len < 1) {
				CCCI_ERROR_LOG(md_id, RPC,
				"invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
				p_rpc_buf->op_id, pkt_num, name_len);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}

			eint_name = kmalloc(name_len, GFP_KERNEL);
			if (eint_name == NULL) {
				CCCI_ERROR_LOG(md_id, RPC,
				"Fail alloc Mem for [0x%X]!\n",
				p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err3;
			} else {
				memcpy(eint_name, (unsigned char *)(pkt[0].buf),
				name_len);
			}

			type = *(unsigned int *)(pkt[2].buf);
			res = (unsigned char *)&(p_rpc_buf->para_num) +
					4 * sizeof(unsigned int);
			ret = get_eint_attr(md_id, eint_name, name_len, type,
					res, &res_len);
			if (ret == 0) {
				tmp_data[0] = ret;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = res_len;
				pkt[pkt_num++].buf = (void *)res;
				CCCI_DEBUG_LOG(md_id, RPC,
					"[0x%08X] OK: name:%s, len:%d, type:%d, res:%d, res_len:%d\n",
					p_rpc_buf->op_id, eint_name, name_len,
					type, *res, res_len);
				kfree(eint_name);
			} else {
				tmp_data[0] = ret;
				CCCI_DEBUG_LOG(md_id, RPC,
					"[0x%08X] fail: name:%s, len:%d, type:%d, ret:%d\n",
					p_rpc_buf->op_id, eint_name, name_len,
					type, ret);
				kfree(eint_name);
				goto err3;
			}
			break;

 err3:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}
#ifdef FEATURE_RF_CLK_BUF
	case IPC_RPC_GET_RF_CLK_BUF_OP:
		{
			u16 count = 0;
			struct ccci_rpc_clkbuf_result *clkbuf;
			CLK_BUF_SWCTRL_STATUS_T swctrl_status[CLKBUF_MAX_COUNT];
			struct ccci_rpc_clkbuf_input *clkinput;
			u32 AfcDac;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			clkinput = (struct ccci_rpc_clkbuf_input *)pkt[0].buf;
			AfcDac = clkinput->AfcCwData;
			count = clkinput->CLKBuf_Num;
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len =
				sizeof(struct ccci_rpc_clkbuf_result);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			clkbuf = (struct ccci_rpc_clkbuf_result *)&tmp_data[1];
			if (count != CLKBUF_MAX_COUNT) {
				CCCI_ERROR_LOG(md_id, RPC,
				"IPC_RPC_GET_RF_CLK_BUF, wrong count %d/%d\n",
				count, CLKBUF_MAX_COUNT);
				clkbuf->CLKBuf_Count = 0xFF;
				memset(&clkbuf->CLKBuf_Status, 0,
					sizeof(clkbuf->CLKBuf_Status));
			} else if (is_clk_buf_from_pmic()) {
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				memset(&clkbuf->CLKBuf_Status, 0,
					sizeof(clkbuf->CLKBuf_Status));
				memset(&clkbuf->CLKBuf_SWCtrl_Status, 0,
					sizeof(clkbuf->CLKBuf_SWCtrl_Status));
				memset(&clkbuf->ClkBuf_Driving, 0,
					sizeof(clkbuf->ClkBuf_Driving));
			} else {
				unsigned int vals_drv[CLKBUF_MAX_COUNT] = {
					2, 2, 2, 2};
				u32 vals[CLKBUF_MAX_COUNT] = {0, 0, 0, 0};
				struct device_node *node;

				node = of_find_compatible_node(NULL, NULL,
						"mediatek,rf_clock_buffer");
				if (node) {
					of_property_read_u32_array(node,
						"mediatek,clkbuf-config", vals,
						CLKBUF_MAX_COUNT);
				} else {
					CCCI_ERROR_LOG(md_id, RPC,
					"%s can't find compatible node\n",
					__func__);
				}
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				clkbuf->CLKBuf_Status[0] = vals[0];
				clkbuf->CLKBuf_Status[1] = vals[1];
				clkbuf->CLKBuf_Status[2] = vals[2];
				clkbuf->CLKBuf_Status[3] = vals[3];
				clk_buf_get_swctrl_status(swctrl_status);
				clk_buf_get_rf_drv_curr(vals_drv);
				clk_buf_save_afc_val(AfcDac);
				clkbuf->CLKBuf_SWCtrl_Status[0] =
					swctrl_status[0];
				clkbuf->CLKBuf_SWCtrl_Status[1] =
					swctrl_status[1];
				clkbuf->CLKBuf_SWCtrl_Status[2] =
					swctrl_status[2];
				clkbuf->CLKBuf_SWCtrl_Status[3] =
					swctrl_status[3];
				clkbuf->ClkBuf_Driving[0] = vals_drv[0];
				clkbuf->ClkBuf_Driving[1] = vals_drv[1];
				clkbuf->ClkBuf_Driving[2] = vals_drv[2];
				clkbuf->ClkBuf_Driving[3] = vals_drv[3];
				CCCI_NORMAL_LOG(md_id, RPC,
					"RF_CLK_BUF*_DRIVING_CURR %d, %d, %d, %d, AfcDac: %d\n",
					vals_drv[0], vals_drv[1], vals_drv[2],
					vals_drv[3], AfcDac);
			}
			CCCI_DEBUG_LOG(md_id, RPC,
				"IPC_RPC_GET_RF_CLK_BUF count=%x\n",
				clkbuf->CLKBuf_Count);
			break;
		}
#endif
	case IPC_RPC_GET_GPIO_VAL_OP:
	case IPC_RPC_GET_ADC_VAL_OP:
		{
			unsigned int num = 0;
			int val = 0;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err4;
			}

			num = *(unsigned int *)(pkt[0].buf);
			if (p_rpc_buf->op_id == IPC_RPC_GET_GPIO_VAL_OP)
				val = get_md_gpio_val(num);
			else if (p_rpc_buf->op_id == IPC_RPC_GET_ADC_VAL_OP)
				val = get_md_adc_val(num);
			tmp_data[0] = val;
			CCCI_DEBUG_LOG(md_id, RPC, "[0x%X]: num=%d, val=%d!\n",
				p_rpc_buf->op_id, num, val);

 err4:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_GPIO_ADC_OP:
		{
			struct ccci_rpc_gpio_adc_intput *input;
			struct ccci_rpc_gpio_adc_output *output;
			struct ccci_rpc_gpio_adc_intput_v2 *input_v2;
			struct ccci_rpc_gpio_adc_output_v2 *output_v2;
			unsigned int pkt_size;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			pkt_size = pkt[0].len;
			if (pkt_size ==
				sizeof(struct ccci_rpc_gpio_adc_intput)) {
				input =
				(struct ccci_rpc_gpio_adc_intput *)(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len =
				sizeof(struct ccci_rpc_gpio_adc_output);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output =
				(struct ccci_rpc_gpio_adc_output *)&tmp_data[1];
				/* 0xF for failure */
				memset(output, 0xF,
				sizeof(struct ccci_rpc_gpio_adc_output));
				CCCI_BOOTUP_LOG(md_id, RPC,
					"IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
					input->reqMask);
				ccci_rpc_get_gpio_adc(input, output);
			} else if (pkt_size ==
				sizeof(struct ccci_rpc_gpio_adc_intput_v2)) {
				input_v2 =
				(struct ccci_rpc_gpio_adc_intput_v2 *)
				(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len =
				sizeof(struct ccci_rpc_gpio_adc_output_v2);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output_v2 =
				(struct ccci_rpc_gpio_adc_output_v2 *)
				&tmp_data[1];
				/* 0xF for failure */
				memset(output_v2, 0xF,
				sizeof(struct ccci_rpc_gpio_adc_output_v2));
				CCCI_BOOTUP_LOG(md_id, RPC,
					"IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
					input_v2->reqMask);
				ccci_rpc_get_gpio_adc_v2(input_v2, output_v2);
			} else {
				CCCI_ERROR_LOG(md_id, RPC,
					"can't recognize pkt size%d!\n",
					pkt_size);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
			}
			break;
		}

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	case IPC_RPC_USIM2NFC_OP:
		{
			struct ccci_rpc_usim2nfs *input, *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			input = (struct ccci_rpc_usim2nfs *)(pkt[0].buf);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_usim2nfs);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			output = (struct ccci_rpc_usim2nfs *)&tmp_data[1];
			output->lock_vsim1 = input->lock_vsim1;
			CCCI_DEBUG_LOG(md_id, RPC,
				"IPC_RPC_USIM2NFC_OP request=%x\n",
				input->lock_vsim1);
			/* lock_vsim1==1, NFC not power VSIM;
			 * lock_vsim==0, NFC power VSIM
			 */
			inform_nfc_vsim_change(md_id, 1, input->lock_vsim1);
			break;
		}
#endif
	case IPC_RPC_CCCI_LHIF_MAPPING:
		{
			struct ccci_rpc_queue_mapping *remap;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}

			CCCI_NORMAL_LOG(md_id, RPC,
				"op_id[0x%X]: pkt_num=%d, pkt[0] len %u!\n",
				p_rpc_buf->op_id, pkt_num, pkt[0].len);

			remap = (struct ccci_rpc_queue_mapping *)(pkt[0].buf);
			ccci_rpc_remap_queue(md_id, remap);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];

			break;
		}
	case IPC_RPC_DTSI_QUERY_OP:
		{
			struct ccci_rpc_md_dtsi_input *input;
			struct ccci_rpc_md_dtsi_output *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC,
					"invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			input = (struct ccci_rpc_md_dtsi_input *)(pkt[0].buf);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len =
				sizeof(struct ccci_rpc_md_dtsi_output);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			output = (struct ccci_rpc_md_dtsi_output *)&tmp_data[1];
			/* 0xF for failure */
			memset(output, 0xF,
				sizeof(struct ccci_rpc_md_dtsi_output));
			get_md_dtsi_val(input, output);
			break;
		}
	case IPC_RPC_IT_OP:
		{
			int i;

			CCCI_NORMAL_LOG(md_id, RPC,
				"[RPCIT] enter IT operation in ccci_rpc_work\n");
			/* exam input parameters in pkt */
			for (i = 0; i < pkt_num; i++) {
				CCCI_NORMAL_LOG(md_id, RPC,
					"len=%d val=%X\n", pkt[i].len,
					*((unsigned int *)pkt[i].buf));
			}
			tmp_data[0] = 1;
			tmp_data[1] = 0xA5A5;
			pkt_num = 0;
			CCCI_NORMAL_LOG(md_id, RPC,
				"[RPCIT] prepare output parameters\n");
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			CCCI_NORMAL_LOG(md_id, RPC,
				"[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n",
				0, pkt[0].len, *((unsigned int *)pkt[0].buf));
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			CCCI_NORMAL_LOG(md_id, RPC,
			"[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n",
			1, pkt[1].len, *((unsigned int *)pkt[1].buf));
			break;
		}

	default:
		CCCI_NORMAL_LOG(md_id, RPC,
		"[Error]Unknown Operation ID (0x%08X)\n",
		p_rpc_buf->op_id);
		tmp_data[0] = FS_NO_OP;
		pkt_num = 0;
		pkt[pkt_num].len = sizeof(int);
		pkt[pkt_num++].buf = (void *)&tmp_data[0];
		break;
	}

	p_rpc_buf->para_num = pkt_num;
	CCCI_DEBUG_LOG(md_id, RPC, "%s-- %d\n", __func__,
		p_rpc_buf->para_num);
}

static void rpc_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct rpc_buffer *rpc_buf = (struct rpc_buffer *)skb->data;
	int i, data_len, AlignLength, ret;
	struct rpc_pkt pkt[RPC_MAX_ARG_NUM];
	char *ptr, *ptr_base;
	/* unsigned int tmp_data[128]; */
	/* size of tmp_data should be >= any RPC output result */
	unsigned int *tmp_data =
		kmalloc(128*sizeof(unsigned int), GFP_ATOMIC);

	if (tmp_data == NULL) {
		CCCI_ERROR_LOG(md_id, RPC,
			"RPC request buffer fail 128*sizeof(unsigned int)\n");
		goto err_out;
	}
	/* sanity check */
	if (skb->len > RPC_MAX_BUF_SIZE) {
		CCCI_ERROR_LOG(md_id, RPC,
				"invalid RPC buffer size 0x%x/0x%x\n",
				skb->len, RPC_MAX_BUF_SIZE);
		goto err_out;
	}
	if (rpc_buf->header.reserved < 0 ||
		rpc_buf->header.reserved > RPC_REQ_BUFFER_NUM ||
	    rpc_buf->para_num < 0 ||
		rpc_buf->para_num > RPC_MAX_ARG_NUM) {
		CCCI_ERROR_LOG(md_id, RPC,
			"invalid RPC index %d/%d\n",
			rpc_buf->header.reserved, rpc_buf->para_num);
		goto err_out;
	}
	/* parse buffer */
	ptr_base = ptr = rpc_buf->buffer;
	data_len = sizeof(rpc_buf->op_id) + sizeof(rpc_buf->para_num);
	for (i = 0; i < rpc_buf->para_num; i++) {
		pkt[i].len = *((unsigned int *)ptr);
		if (pkt[i].len >= skb->len) {
			CCCI_ERROR_LOG(md_id, RPC,
				"invalid packet length in parse %u\n",
				pkt[i].len);
			goto err_out;
		}
		if ((data_len + sizeof(pkt[i].len) + pkt[i].len) >
			RPC_MAX_BUF_SIZE) {
			CCCI_ERROR_LOG(md_id, RPC,
				"RPC buffer overflow in parse %zu\n",
				data_len + sizeof(pkt[i].len) + pkt[i].len);
			goto err_out;
		}
		ptr += sizeof(pkt[i].len);
		pkt[i].buf = ptr;
		AlignLength = ((pkt[i].len + 3) >> 2) << 2;
		ptr += AlignLength;	/* 4byte align */
		data_len += (sizeof(pkt[i].len) + AlignLength);
	}
	if ((ptr - ptr_base) > RPC_MAX_BUF_SIZE) {
		CCCI_ERROR_LOG(md_id, RPC,
			"RPC overflow in parse 0x%p\n",
			(void *)(ptr - ptr_base));
		goto err_out;
	}
	/* handle RPC request */
	ccci_rpc_work_helper(port, pkt, rpc_buf, tmp_data);
	/* write back to modem */
	/* update message */
	rpc_buf->op_id |= RPC_API_RESP_ID;
	data_len = sizeof(rpc_buf->op_id) + sizeof(rpc_buf->para_num);
	ptr = rpc_buf->buffer;
	for (i = 0; i < rpc_buf->para_num; i++) {
		if ((data_len + sizeof(pkt[i].len) + pkt[i].len) >
			RPC_MAX_BUF_SIZE) {
			CCCI_ERROR_LOG(md_id, RPC,
				"RPC overflow in write %zu\n",
				data_len + sizeof(pkt[i].len) + pkt[i].len);
			goto err_out;
		}

		*((unsigned int *)ptr) = pkt[i].len;
		ptr += sizeof(pkt[i].len);
		data_len += sizeof(pkt[i].len);
		/* 4byte aligned */
		AlignLength = ((pkt[i].len + 3) >> 2) << 2;
		data_len += AlignLength;

		if (ptr != pkt[i].buf)
			memcpy(ptr, pkt[i].buf, pkt[i].len);
		else
			CCCI_DEBUG_LOG(md_id, RPC,
				"same addr, no copy, op_id=0x%x\n",
				rpc_buf->op_id);

		ptr += AlignLength;
	}
	/* resize skb */
	data_len += sizeof(struct ccci_header);
	if (data_len > skb->len)
		skb_put(skb, data_len - skb->len);
	else if (data_len < skb->len)
		skb_trim(skb, data_len);
	/* update CCCI header */
	rpc_buf->header.channel = CCCI_RPC_TX;
	rpc_buf->header.data[1] = data_len;
	CCCI_DEBUG_LOG(md_id, RPC,
		"Write %d/%d, %08X, %08X, %08X, %08X, op_id=0x%x\n",
		skb->len, data_len, rpc_buf->header.data[0],
		rpc_buf->header.data[1], rpc_buf->header.channel,
		rpc_buf->header.reserved, rpc_buf->op_id);
	/* switch to Tx request */
	ret = port_send_skb_to_md(port, skb, 1);
	if (ret)
		goto err_out;
	kfree(tmp_data);
	return;

 err_out:
	kfree(tmp_data);
	ccci_free_skb(skb);
}

/*
 * define character device operation for rpc_u
 */
static const struct file_operations rpc_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /*use default API*/
	.read = &port_dev_read, /*use default API*/
	.write = &port_dev_write, /*use default API*/
	.release = &port_dev_close,/*use default API*/
};
static int port_rpc_init(struct port_t *port)
{
	struct cdev *dev;
	int ret = 0;
	static int first_init = 1;

	CCCI_DEBUG_LOG(port->md_id, RPC,
		"rpc port %s is initializing\n", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->interception = 0;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
		if (unlikely(!dev)) {
			CCCI_ERROR_LOG(port->md_id, CHAR,
				"alloc rpc char dev fail!!\n");
			return -1;
		}
		cdev_init(dev, &rpc_dev_fops);
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev, MKDEV(port->major,
			port->minor_base + port->minor), 1);
		ret = ccci_register_dev_node(port->name, port->major,
			port->minor_base + port->minor);
		port->flags |= PORT_F_ADJUST_HEADER;
	} else {
		port->skb_handler = &rpc_msg_handler;
		kthread_run(port_kthread_handler, port, "%s", port->name);
	}

	if (first_init) {
		get_dtsi_eint_node(port->md_id);
		get_md_dtsi_debug();
		md_drdi_gpio_status_scan();
		first_init = 0;
	}
	return 0;
}

int port_rpc_recv_match(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	int is_userspace_msg = 0;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct rpc_buffer *rpc_buf = (struct rpc_buffer *)skb->data;

	if (ccci_h->channel == CCCI_RPC_RX) {
		switch (rpc_buf->op_id) {
#ifdef CONFIG_MTK_TC1_FEATURE
		/* LGE specific OP ID */
		case RPC_CCCI_LGE_FAC_READ_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_FUSG_FLAG:
		case RPC_CCCI_LGE_FAC_CHECK_UNLOCK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_CHECK_NETWORK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_WRITE_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_IMEI:
		case RPC_CCCI_LGE_FAC_WRITE_IMEI:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_VERIFY_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_INIT_SIM_LOCK_DATA:
			is_userspace_msg = 1;
#endif
			break;

		case IPC_RPC_QUERY_AP_SYS_PROPERTY:
			is_userspace_msg = 1;
			break;
		default:
			is_userspace_msg = 0;
			break;
		}
	}
	if (is_userspace_msg &&
		(port->flags & PORT_F_WITH_CHAR_NODE)) {
		/*userspace msg, so need match userspace port*/
		CCCI_DEBUG_LOG(md_id, RPC, "userspace rpc msg 0x%x on %s\n",
						rpc_buf->op_id, port->name);
	} else {
		/*kernel msg, so need match kernel port*/
		if (is_userspace_msg == 0 &&
			!(port->flags & PORT_F_WITH_CHAR_NODE)) {
			CCCI_DEBUG_LOG(md_id, RPC,
				"kernelspace rpc msg 0x%x on %s\n",
				rpc_buf->op_id, port->name);
		} else {
			CCCI_DEBUG_LOG(md_id, RPC,
				"port_rpc cfg error, need check:msg 0x%x on %s\n",
				rpc_buf->op_id, port->name);
			return 0;
		}
	}
	return 1;
}

struct port_ops rpc_port_ops = {
	.init = &port_rpc_init,
	.recv_match = &port_rpc_recv_match,
	.recv_skb = &port_recv_skb,
};

