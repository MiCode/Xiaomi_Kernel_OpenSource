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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
#include <sec_export.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#ifdef FEATURE_RF_CLK_BUF
#include <mt_clkbuf_ctl.h>
#endif

#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_rpc.h"
#include "port_proxy.h"

#define MAX_QUEUE_LENGTH 16

#if defined(FEATURE_GET_MD_GPIO_NUM)
static struct gpio_item gpio_mapping_table[] = {
	{"GPIO_FDD_Band_Support_Detection_1", "GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN",},
	{"GPIO_FDD_Band_Support_Detection_2", "GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN",},
	{"GPIO_FDD_Band_Support_Detection_3", "GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN",},
	{"GPIO_FDD_Band_Support_Detection_4", "GPIO_FDD_BAND_SUPPORT_DETECT_4TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_5", "GPIO_FDD_BAND_SUPPORT_DETECT_5TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_6", "GPIO_FDD_BAND_SUPPORT_DETECT_6TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_7", "GPIO_FDD_BAND_SUPPORT_DETECT_7TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_8", "GPIO_FDD_BAND_SUPPORT_DETECT_8TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_9", "GPIO_FDD_BAND_SUPPORT_DETECT_9TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_A", "GPIO_FDD_BAND_SUPPORT_DETECT_ATH_PIN",},
};
#endif

static int get_md_gpio_val(unsigned int num)
{
#if defined(FEATURE_GET_MD_GPIO_VAL)
	return __gpio_get_value(num);
#else
	return -1;
#endif
}

static int get_md_adc_val(unsigned int num)
{
#if defined(FEATURE_GET_MD_ADC_VAL)
	int data[4] = { 0, 0, 0, 0 };
	int val = 0;
	int ret = 0;

	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_ADC_VAL\n");
	ret = IMM_GetOneChannelValue(num, data, &val);
	if (ret == 0)
		return val;
	else
		return ret;
#elif defined(FEATURE_GET_MD_PMIC_ADC_VAL)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_VAL\n");
	return PMIC_IMM_GetOneChannelValue(num, 1, 0);
#else
	return -1;
#endif
}

static int get_td_eint_info(char *eint_name, unsigned int len)
{
#if defined(FEATURE_GET_TD_EINT_NUM)
	return get_td_eint_num(eint_name, len);
#else
	return -1;
#endif
}

static int get_md_adc_info(char *adc_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_ADC_NUM)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_ADC_NUM\n");
	return IMM_get_adc_channel_num(adc_name, len);
#elif defined(FEATURE_GET_MD_PMIC_ADC_NUM)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_NUM\n");
	return PMIC_IMM_get_adc_channel_num(adc_name, len);
#else
	return -1;
#endif
}

static int get_md_gpio_info(char *gpio_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_GPIO_NUM)
	int i = 0;
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,gpio_usage_mapping");
	int gpio_id = -1;

	if (!node) {
		CCCI_NORMAL_LOG(0, RPC, "MD_USE_GPIO is not set in device tree,need to check?\n");
		return gpio_id;
	}

	CCCI_BOOTUP_LOG(0, RPC, "looking for %s id, len %d\n", gpio_name, len);
	for (i = 0; i < ARRAY_SIZE(gpio_mapping_table); i++) {
		CCCI_DEBUG_LOG(0, RPC, "compare with %s\n", gpio_mapping_table[i].gpio_name_from_md);
		if (!strncmp(gpio_name, gpio_mapping_table[i].gpio_name_from_md, len)) {
			CCCI_BOOTUP_LOG(0, RPC, "searching %s in device tree\n",
							gpio_mapping_table[i].gpio_name_from_dts);
			of_property_read_u32(node, gpio_mapping_table[i].gpio_name_from_dts, &gpio_id);
			break;
		}
	}

	/*
	 * if gpio_name_from_md and gpio_name_from_dts are the same,
	 * it will not be listed in gpio_mapping_table,
	 * so try read directly from device tree here.
	 */
	if (gpio_id < 0) {
		CCCI_BOOTUP_LOG(0, RPC, "try directly get id from device tree\n");
		of_property_read_u32(node, gpio_name, &gpio_id);
	}

	CCCI_BOOTUP_LOG(0, RPC, "%s id %d\n", gpio_name, gpio_id);
	return gpio_id;

#else
	return -1;
#endif
}

static int get_dram_type_clk(int *clk, int *type)
{
#if defined(FEATURE_GET_DRAM_TYPE_CLK)
	return get_dram_info(clk, type);
#else
	return -1;
#endif
}

#ifdef FEATURE_GET_MD_EINT_ATTR_DTS

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

static int get_eint_attr_val(int md_id, struct device_node *node, int index)
{
	int value;
	int ret = 0, type;
	int covert_AP_to_MD_unit = 1000; /*unit of AP eint is us, but unit of MD eint is ms. So need covertion here.*/

	for (type = 0; type < SIM_HOT_PLUG_EINT_MAX; type++) {
		ret = of_property_read_u32_index(node, md_eint_struct[type].property,
			md_eint_struct[type].index, &value);
		if (ret != 0) {
			md_eint_struct[type].value_sim[index] = ERR_SIM_HOT_PLUG_QUERY_TYPE;
			CCCI_NORMAL_LOG(md_id, RPC, "%s:  not found\n", md_eint_struct[type].property);
			return ERR_SIM_HOT_PLUG_QUERY_TYPE;
		}
		/* special case: polarity's position == sensitivity's start[ */
		if (type == SIM_HOT_PLUG_EINT_POLARITY) {
			switch (value) {
			case IRQ_TYPE_EDGE_RISING:
			case IRQ_TYPE_EDGE_FALLING:
			case IRQ_TYPE_LEVEL_HIGH:
			case IRQ_TYPE_LEVEL_LOW:
				md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] =
				    (value & 0x5) ? 1 : 0;
				/*1/4: IRQ_TYPE_EDGE_RISING/IRQ_TYPE_LEVEL_HIGH Set 1 */
				md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] =
				    (value & 0x3) ? 1 : 0;
				/*1/2: IRQ_TYPE_EDGE_RISING/IRQ_TYPE_LEVEL_FALLING Set 1 */
				break;
			default:	/* invalid */
				md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] = -1;
				md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] = -1;
				CCCI_ERROR_LOG(md_id, RPC, "invalid value, please check dtsi!\n");
				break;
			}
			type++;
		} else if (type == SIM_HOT_PLUG_EINT_DEBOUNCETIME) {
			/*debounce time should divide by 1000 due to different unit in AP and MD.*/
			md_eint_struct[type].value_sim[index] = value/covert_AP_to_MD_unit;
		} else
			md_eint_struct[type].value_sim[index] = value;
	}
	return 0;
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
		node = of_find_node_by_name(NULL, eint_node_prop.name[i].node_name);
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
		if (!(strncmp(name, eint_node_prop.name[i].node_name, name_len))) {
			sim_value = eint_node_prop.eint_value[type].value_sim[i];
			*len = sizeof(sim_value);
			memcpy(sim_info, &sim_value, *len);
			CCCI_BOOTUP_LOG(md_id, RPC, "md_eint:%s, sizeof: %d, sim_info: %d, %d\n",
					eint_node_prop.eint_value[type].property,
					*len, *sim_info, eint_node_prop.eint_value[type].value_sim[i]);
			return 0;
		}
	}
	return ERR_SIM_HOT_PLUG_QUERY_STRING;
}
#endif

static int get_eint_attr(int md_id, char *name, unsigned int name_len,
			unsigned int type, char *result, unsigned int *len)
{
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
	return get_eint_attr_DTSVal(md_id, name, name_len, type, result, len);
#else
#if defined(FEATURE_GET_MD_EINT_ATTR)
	return get_eint_attribute(name, name_len, type, result, len);
#else
	return -1;
#endif
#endif
}

static void ccci_rpc_get_gpio_adc(struct ccci_rpc_gpio_adc_intput *input, struct ccci_rpc_gpio_adc_output *output)
{
	int num;
	unsigned int val, i;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) == (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
				if (num >= 0) {
					output->gpioPinNum[i] = num;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
					if (num >= 0)
						output->gpioPinNum[i] = num;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) == (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
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
			num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
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
	int num;
	unsigned int val, i;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) == (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
				if (num >= 0) {
					output->gpioPinNum[i] = num;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
					if (num >= 0)
						output->gpioPinNum[i] = num;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) == (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
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
			num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
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

static void ccci_rpc_work_helper(struct ccci_port *port, struct rpc_pkt *pkt,
				 struct rpc_buffer *p_rpc_buf, unsigned int tmp_data[])
{
	/*
	 * tmp_data[] is used to make sure memory address is valid
	 * after this function return, be careful with the size!
	 */
	int pkt_num = p_rpc_buf->para_num;
	int md_id = port->md_id;

	CCCI_DEBUG_LOG(md_id, RPC, "ccci_rpc_work_helper++ %d\n", p_rpc_buf->para_num);
	tmp_data[0] = 0;
	switch (p_rpc_buf->op_id) {
	case IPC_RPC_CPSVC_SECURE_ALGO_OP:
		{
			unsigned char Direction = 0;
			unsigned long ContentAddr = 0;
			unsigned int ContentLen = 0;
			sed_t CustomSeed = SED_INITIALIZER;

			unsigned char *ResText __always_unused;
			unsigned char *RawText __always_unused;
			unsigned int i __always_unused;

			if (pkt_num < 4 || pkt_num >= RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid pkt_num %d for RPC_SECURE_ALGO_OP!\n", pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}

			Direction = *(unsigned char *)pkt[0].buf;
			ContentAddr = (unsigned long)pkt[1].buf;
			CCCI_DEBUG_LOG(md_id, RPC,
				     "RPC_SECURE_ALGO_OP: Content_Addr = 0x%p, RPC_Base = 0x%p, RPC_Len = %zu\n",
				     (void *)ContentAddr, p_rpc_buf, sizeof(unsigned int) + RPC_MAX_BUF_SIZE);
			if (ContentAddr < (unsigned long)p_rpc_buf
			    || ContentAddr > ((unsigned long)p_rpc_buf + sizeof(unsigned int) + RPC_MAX_BUF_SIZE)) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid ContentAdddr[0x%p] for RPC_SECURE_ALGO_OP!\n",
					     (void *)ContentAddr);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			ContentLen = *(unsigned int *)pkt[2].buf;
			/* CustomSeed = *(sed_t*)pkt[3].buf; */
			WARN_ON(sizeof(CustomSeed.sed) < pkt[3].len);
			memcpy(CustomSeed.sed, pkt[3].buf, pkt[3].len);

#ifdef ENCRYPT_DEBUG
			unsigned char log_buf[128];
			int curr;

			if (Direction == TRUE)
				CCCI_DEBUG_LOG(md_id, RPC, "HACC_S: EnCrypt_src:\n");
			else
				CCCI_DEBUG_LOG(md_id, RPC, "HACC_S: DeCrypt_src:\n");
			for (i = 0; i < ContentLen; i++) {
				if (i % 16 == 0) {
					if (i != 0)
						CCCI_NORMAL_LOG(md_id, RPC, "%s\n", log_buf);
					curr = 0;
					curr += snprintf(log_buf, sizeof(log_buf) - curr, "HACC_S: ");
				}
				/* CCCI_NORMAL_LOG(md_id, RPC, "0x%02X ", *(unsigned char*)(ContentAddr+i)); */
				curr +=
				    snprintf(&log_buf[curr], sizeof(log_buf) - curr, "0x%02X ",
					     *(unsigned char *)(ContentAddr + i));
				/* sleep(1); */
			}
			CCCI_NORMAL_LOG(md_id, RPC, "%s\n", log_buf);

			RawText = kmalloc(ContentLen, GFP_KERNEL);
			if (RawText == NULL)
				CCCI_ERROR_LOG(md_id, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
			else
				memcpy(RawText, (unsigned char *)ContentAddr, ContentLen);
#endif

			ResText = kmalloc(ContentLen, GFP_KERNEL);
			if (ResText == NULL) {
				CCCI_ERROR_LOG(md_id, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			if (!masp_secure_algo_init()) {
				CCCI_ERROR_LOG(md_id, RPC, "masp_secure_algo_init fail!\n");
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}

			CCCI_DEBUG_LOG(md_id, RPC,
				     "RPC_SECURE_ALGO_OP: Dir=0x%08X, Addr=0x%08lX, Len=0x%08X, Seed=0x%016llX\n",
				     Direction, ContentAddr, ContentLen, *(long long *)CustomSeed.sed);
			masp_secure_algo(Direction, (unsigned char *)ContentAddr, ContentLen, CustomSeed.sed, ResText);

			if (!masp_secure_algo_deinit())
				CCCI_ERROR_LOG(md_id, RPC, "masp_secure_algo_deinit fail!\n");
#endif

			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = ContentLen;

#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			memcpy(pkt[pkt_num++].buf, ResText, ContentLen);
			CCCI_DEBUG_LOG(md_id, RPC, "RPC_Secure memory copy OK: %d!", ContentLen);
#else
			memcpy(pkt[pkt_num++].buf, (void *)ContentAddr, ContentLen);
			CCCI_DEBUG_LOG(md_id, RPC, "RPC_NORMAL memory copy OK: %d!", ContentLen);
#endif

#ifdef ENCRYPT_DEBUG
			if (Direction == TRUE)
				CCCI_DEBUG_LOG(md_id, RPC, "HACC_D: EnCrypt_dst:\n");
			else
				CCCI_DEBUG_LOG(md_id, RPC, "HACC_D: DeCrypt_dst:\n");
			for (i = 0; i < ContentLen; i++) {
				if (i % 16 == 0) {
					if (i != 0)
						CCCI_DEBUG_LOG(md_id, RPC, "%s\n", log_buf);
					curr = 0;
					curr += snprintf(&log_buf[curr], sizeof(log_buf) - curr, "HACC_D: ");
				}
				/* CCCI_NORMAL_LOG(md_id, RPC, "%02X ", *(ResText+i)); */
				curr += snprintf(&log_buf[curr], sizeof(log_buf) - curr, "0x%02X ", *(ResText + i));
				/* sleep(1); */
			}

			CCCI_DEBUG_LOG(md_id, RPC, "%s\n", log_buf);
			kfree(RawText);
#endif

			kfree(ResText);
			break;
		}

#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	case IPC_RPC_GET_SECRO_OP:
		{
			unsigned char *addr = NULL;
			unsigned int img_len = 0;
			unsigned int img_len_bak = 0;
			unsigned int blk_sz = 0;
			unsigned int tmp = 1;
			unsigned int cnt = 0;
			unsigned int req_len = 0;
			char *img_post_fix = NULL;

			img_post_fix = port_proxy_get_md_img_post_fix(port->port_proxy);
			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC, "RPC_GET_SECRO_OP: invalid parameter: pkt_num=%d\n",
					     pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				break;
			}

			req_len = *(unsigned int *)(pkt[0].buf);
			if (masp_secro_en()) {
				img_len = masp_secro_md_len(img_post_fix);

				if ((img_len > RPC_MAX_BUF_SIZE) || (req_len > RPC_MAX_BUF_SIZE)) {
					pkt_num = 0;
					tmp_data[0] = FS_MEM_OVERFLOW;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					/* set it as image length for modem ccci check when error happens */
					pkt[pkt_num].len = img_len;
					/* /pkt[pkt_num].len = sizeof(unsigned int); */
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					CCCI_ERROR_LOG(md_id, RPC,
						     "RPC_GET_SECRO_OP: md request length is larger than rpc memory: (%d, %d)\n",
						     req_len, img_len);
					break;
				}

				if (img_len > req_len) {
					pkt_num = 0;
					tmp_data[0] = FS_NO_MATCH;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					/* set it as image length for modem ccci check when error happens */
					pkt[pkt_num].len = img_len;
					/* /pkt[pkt_num].len = sizeof(unsigned int); */
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					CCCI_ERROR_LOG(md_id, RPC,
						     "RPC_GET_SECRO_OP: AP mis-match MD request length: (%d, %d)\n",
						     req_len, img_len);
					break;
				}

				/* TODO : please check it */
				/* save original modem secro length */
				CCCI_DEBUG_LOG(md_id, RPC, "<rpc>RPC_GET_SECRO_OP: save MD SECRO length: (%d)\n",
					     img_len);
				img_len_bak = img_len;

				blk_sz = masp_secro_blk_sz();
				for (cnt = 0; cnt < blk_sz; cnt++) {
					tmp = tmp * 2;
					if (tmp >= blk_sz)
						break;
				}
				++cnt;
				img_len = ((img_len + (blk_sz - 1)) >> cnt) << cnt;

				addr = (unsigned char *)&(p_rpc_buf->para_num) + 4 * sizeof(unsigned int);
				tmp_data[0] = masp_secro_md_get_data(img_post_fix, addr, 0, img_len);

				/* TODO : please check it */
				/* restore original modem secro length */
				img_len = img_len_bak;

				CCCI_DEBUG_LOG(md_id, RPC, "<rpc>RPC_GET_SECRO_OP: restore MD SECRO length: (%d)\n",
					     img_len);

				if (tmp_data[0] != 0) {
					CCCI_ERROR_LOG(md_id, RPC, "RPC_GET_SECRO_OP: get data fail:%d\n",
						     tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					pkt[pkt_num].len = sizeof(unsigned int);
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
				} else {
					CCCI_DEBUG_LOG(md_id, RPC, "RPC_GET_SECRO_OP: get data OK: %d,%d\n",
									img_len, tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					/* pkt[pkt_num++].buf = (void*) &img_len; */
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					pkt[pkt_num].len = img_len;
					pkt[pkt_num++].buf = (void *)addr;
					/* tmp_data[2] = (unsigned int)addr; */
					/* pkt[pkt_num++].buf = (void*) &tmp_data[2]; */
				}
			} else {
				CCCI_DEBUG_LOG(md_id, RPC, "RPC_GET_SECRO_OP: secro disable\n");
				tmp_data[0] = FS_NO_FEATURE;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
			}

			break;
		}
#endif

		/* call EINT API to get TDD EINT configuration for modem EINT initial */
	case IPC_RPC_GET_TDD_EINT_NUM_OP:
	case IPC_RPC_GET_GPIO_NUM_OP:
	case IPC_RPC_GET_ADC_NUM_OP:
		{
			int get_num = 0;
			unsigned char *name = NULL;
			unsigned int length = 0;

			if (pkt_num < 2 || pkt_num > RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
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
				CCCI_ERROR_LOG(md_id, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err1;
			} else {
				memcpy(name, (unsigned char *)(pkt[0].buf), length);

				if (p_rpc_buf->op_id == IPC_RPC_GET_TDD_EINT_NUM_OP) {
					get_num = get_td_eint_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				} else if (p_rpc_buf->op_id == IPC_RPC_GET_GPIO_NUM_OP) {
					get_num = get_md_gpio_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				} else if (p_rpc_buf->op_id == IPC_RPC_GET_ADC_NUM_OP) {
					get_num = get_md_adc_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				}

				CCCI_NORMAL_LOG(md_id, RPC, "[0x%08X]: name:%s, len=%d, get_num:%d\n",
					     p_rpc_buf->op_id, name, length, get_num);
				pkt_num = 0;

				/* NOTE: tmp_data[1] not [0] */
				tmp_data[1] = (unsigned int)get_num;
				/* get_num may be invalid after exit this function */
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
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err2;
			}

			if (get_dram_type_clk(&dram_clk, &dram_type)) {
				tmp_data[0] = FS_FUNC_FAIL;
				goto err2;
			} else {
				tmp_data[0] = 0;
				CCCI_NORMAL_LOG(md_id, RPC, "[0x%08X]: dram_clk: %d, dram_type:%d\n",
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
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
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
				CCCI_ERROR_LOG(md_id, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err3;
			} else {
				memcpy(eint_name, (unsigned char *)(pkt[0].buf), name_len);
			}

			type = *(unsigned int *)(pkt[2].buf);
			res = (unsigned char *)&(p_rpc_buf->para_num) + 4 * sizeof(unsigned int);
			ret = get_eint_attr(md_id, eint_name, name_len, type, res, &res_len);
			if (ret == 0) {
				tmp_data[0] = ret;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = res_len;
				pkt[pkt_num++].buf = (void *)res;
				CCCI_DEBUG_LOG(md_id, RPC,
					     "[0x%08X] OK: name:%s, len:%d, type:%d, res:%d, res_len:%d\n",
					     p_rpc_buf->op_id, eint_name, name_len, type, *res, res_len);
				kfree(eint_name);
			} else {
				tmp_data[0] = ret;
				CCCI_DEBUG_LOG(md_id, RPC, "[0x%08X] fail: name:%s, len:%d, type:%d, ret:%d\n",
					     p_rpc_buf->op_id, eint_name, name_len, type, ret);
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
#ifdef MD_UMOLY_EE_SUPPORT
			struct ccci_rpc_clkbuf_input *clkinput;
			u32 AfcDac;
#endif

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
#ifdef MD_UMOLY_EE_SUPPORT
			clkinput = (struct ccci_rpc_clkbuf_input *)pkt[0].buf;
			AfcDac = clkinput->AfcCwData;
			count = clkinput->CLKBuf_Num;
#else
			count = *(u16 *) (pkt[0].buf);
#endif
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_clkbuf_result);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			clkbuf = (struct ccci_rpc_clkbuf_result *)&tmp_data[1];
			if (count != CLKBUF_MAX_COUNT) {
				CCCI_ERROR_LOG(md_id, RPC, "IPC_RPC_GET_RF_CLK_BUF, wrong count %d/%d\n", count,
					     CLKBUF_MAX_COUNT);
				clkbuf->CLKBuf_Count = 0xFF;
				memset(&clkbuf->CLKBuf_Status, 0, sizeof(clkbuf->CLKBuf_Status));
			} else if (is_clk_buf_from_pmic()) {
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				memset(&clkbuf->CLKBuf_Status, 0, sizeof(clkbuf->CLKBuf_Status));
				memset(&clkbuf->CLKBuf_SWCtrl_Status, 0, sizeof(clkbuf->CLKBuf_SWCtrl_Status));
#ifdef MD_UMOLY_EE_SUPPORT
				memset(&clkbuf->ClkBuf_Driving, 0, sizeof(clkbuf->ClkBuf_Driving));
#endif
			} else {
#ifdef MD_UMOLY_EE_SUPPORT
				unsigned int vals_drv[CLKBUF_MAX_COUNT] = {2, 2, 2, 2};
#endif
#if !defined(CONFIG_MTK_LEGACY)
				u32 vals[CLKBUF_MAX_COUNT] = {0, 0, 0, 0};
				struct device_node *node;

				node = of_find_compatible_node(NULL, NULL, "mediatek,rf_clock_buffer");
				if (node) {
					of_property_read_u32_array(node, "mediatek,clkbuf-config", vals,
						CLKBUF_MAX_COUNT);
				} else {
					CCCI_ERROR_LOG(md_id, RPC, "%s can't find compatible node\n", __func__);
				}
#else
				u32 vals[4] = {CLK_BUF1_STATUS, CLK_BUF2_STATUS, CLK_BUF3_STATUS, CLK_BUF4_STATUS};
#endif
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				clkbuf->CLKBuf_Status[0] = vals[0];
				clkbuf->CLKBuf_Status[1] = vals[1];
				clkbuf->CLKBuf_Status[2] = vals[2];
				clkbuf->CLKBuf_Status[3] = vals[3];
				clk_buf_get_swctrl_status(swctrl_status);
#ifdef MD_UMOLY_EE_SUPPORT
				clk_buf_get_rf_drv_curr(vals_drv);
				clk_buf_save_afc_val(AfcDac);
#endif
				clkbuf->CLKBuf_SWCtrl_Status[0] = swctrl_status[0];
				clkbuf->CLKBuf_SWCtrl_Status[1] = swctrl_status[1];
				clkbuf->CLKBuf_SWCtrl_Status[2] = swctrl_status[2];
				clkbuf->CLKBuf_SWCtrl_Status[3] = swctrl_status[3];
#ifdef MD_UMOLY_EE_SUPPORT
				clkbuf->ClkBuf_Driving[0] = vals_drv[0];
				clkbuf->ClkBuf_Driving[1] = vals_drv[1];
				clkbuf->ClkBuf_Driving[2] = vals_drv[2];
				clkbuf->ClkBuf_Driving[3] = vals_drv[3];
				CCCI_INF_MSG(md_id, RPC, "RF_CLK_BUF*_DRIVING_CURR %d, %d, %d, %d, AfcDac: %d\n",
					vals_drv[0], vals_drv[1], vals_drv[2], vals_drv[3], AfcDac);
#endif
			}
			CCCI_DEBUG_LOG(md_id, RPC, "IPC_RPC_GET_RF_CLK_BUF count=%x\n", clkbuf->CLKBuf_Count);
			break;
		}
#endif
	case IPC_RPC_GET_GPIO_VAL_OP:
	case IPC_RPC_GET_ADC_VAL_OP:
		{
			unsigned int num = 0;
			int val = 0;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
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
			CCCI_DEBUG_LOG(md_id, RPC, "[0x%X]: num=%d, val=%d!\n", p_rpc_buf->op_id, num, val);

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
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
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
			if (pkt_size == sizeof(struct ccci_rpc_gpio_adc_intput)) {
				input = (struct ccci_rpc_gpio_adc_intput *)(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(struct ccci_rpc_gpio_adc_output);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output = (struct ccci_rpc_gpio_adc_output *)&tmp_data[1];
				memset(output, 0xF, sizeof(struct ccci_rpc_gpio_adc_output));	/* 0xF for failure */
				CCCI_BOOTUP_LOG(md_id, KERN, "IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
								input->reqMask);
				ccci_rpc_get_gpio_adc(input, output);
			} else if (pkt_size == sizeof(struct ccci_rpc_gpio_adc_intput_v2)) {
				input_v2 = (struct ccci_rpc_gpio_adc_intput_v2 *)(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(struct ccci_rpc_gpio_adc_output_v2);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output_v2 = (struct ccci_rpc_gpio_adc_output_v2 *)&tmp_data[1];
				/* 0xF for failure */
				memset(output_v2, 0xF, sizeof(struct ccci_rpc_gpio_adc_output_v2));
				CCCI_BOOTUP_LOG(md_id, KERN, "IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
					     input_v2->reqMask);
				ccci_rpc_get_gpio_adc_v2(input_v2, output_v2);
			} else {
				CCCI_ERROR_LOG(md_id, RPC, "can't recognize pkt size%d!\n", pkt_size);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
			}
			break;
		}

	case IPC_RPC_DSP_EMI_MPU_SETTING:
		{
			struct ccci_rpc_dsp_emi_mpu_input *input, *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			input = (struct ccci_rpc_dsp_emi_mpu_input *)(pkt[0].buf);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_dsp_emi_mpu_input);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			output = (struct ccci_rpc_dsp_emi_mpu_input *)&tmp_data[1];
			output->request = 0;
			CCCI_NORMAL_LOG(md_id, KERN, "IPC_RPC_DSP_EMI_MPU_SETTING request=%x\n", input->request);
			port_proxy_set_md_dsp_protection(port->port_proxy, 1);
			break;
		}

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	case IPC_RPC_USIM2NFC_OP:
		{
			struct ccci_rpc_usim2nfs *input, *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md_id, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
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
			CCCI_DEBUG_LOG(md_id, KERN, "IPC_RPC_USIM2NFC_OP request=%x\n", input->lock_vsim1);
			/* lock_vsim1==1, NFC not power VSIM; lock_vsim==0, NFC power VSIM */
			inform_nfc_vsim_change(md_id, 1, input->lock_vsim1);
			break;
		}
#endif

	case IPC_RPC_IT_OP:
		{
			int i;

			CCCI_NORMAL_LOG(md_id, RPC, "[RPCIT] enter IT operation in ccci_rpc_work\n");
			/* exam input parameters in pkt */
			for (i = 0; i < pkt_num; i++) {
				CCCI_NORMAL_LOG(md_id, RPC, "len=%d val=%X\n", pkt[i].len,
					     *((unsigned int *)pkt[i].buf));
			}
			tmp_data[0] = 1;
			tmp_data[1] = 0xA5A5;
			pkt_num = 0;
			CCCI_NORMAL_LOG(md_id, RPC, "[RPCIT] prepare output parameters\n");
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			CCCI_NORMAL_LOG(md_id, RPC, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 0, pkt[0].len,
				     *((unsigned int *)pkt[0].buf));
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			CCCI_NORMAL_LOG(md_id, RPC, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 1, pkt[1].len,
				     *((unsigned int *)pkt[1].buf));
			break;
		}

	default:
		CCCI_NORMAL_LOG(md_id, RPC, "[Error]Unknown Operation ID (0x%08X)\n", p_rpc_buf->op_id);
		tmp_data[0] = FS_NO_OP;
		pkt_num = 0;
		pkt[pkt_num].len = sizeof(int);
		pkt[pkt_num++].buf = (void *)&tmp_data[0];
		break;
	}

	p_rpc_buf->para_num = pkt_num;
	CCCI_DEBUG_LOG(md_id, RPC, "ccci_rpc_work_helper-- %d\n", p_rpc_buf->para_num);
}

static void rpc_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct rpc_buffer *rpc_buf = (struct rpc_buffer *)skb->data;
	int i, data_len = 0, AlignLength, ret;
	struct rpc_pkt pkt[RPC_MAX_ARG_NUM];
	char *ptr, *ptr_base;
	/* unsigned int tmp_data[128]; */	/* size of tmp_data should be >= any RPC output result */
	unsigned int *tmp_data = kmalloc(128*sizeof(unsigned int), GFP_ATOMIC);

	if (tmp_data == NULL) {
		CCCI_ERROR_LOG(md_id, RPC, "RPC request buffer fail 128*sizeof(unsigned int)\n");
		goto err_out;
	}
	/* sanity check */
	if (rpc_buf->header.reserved < 0 || rpc_buf->header.reserved > RPC_REQ_BUFFER_NUM ||
	    rpc_buf->para_num < 0 || rpc_buf->para_num > RPC_MAX_ARG_NUM) {
		CCCI_ERROR_LOG(md_id, RPC, "invalid RPC index %d/%d\n", rpc_buf->header.reserved,
						rpc_buf->para_num);
		goto err_out;
	}
	/* parse buffer */
	ptr_base = ptr = rpc_buf->buffer;
	for (i = 0; i < rpc_buf->para_num; i++) {
		pkt[i].len = *((unsigned int *)ptr);
		ptr += sizeof(pkt[i].len);
		pkt[i].buf = ptr;
		ptr += ((pkt[i].len + 3) >> 2) << 2;	/* 4byte align */
	}
	if ((ptr - ptr_base) > RPC_MAX_BUF_SIZE) {
		CCCI_ERROR_LOG(md_id, RPC, "RPC overflow in parse 0x%p\n", (void *)(ptr - ptr_base));
		goto err_out;
	}
	/* handle RPC request */
	ccci_rpc_work_helper(port, pkt, rpc_buf, tmp_data);
	/* write back to modem */
	/* update message */
	rpc_buf->op_id |= RPC_API_RESP_ID;
	data_len += (sizeof(rpc_buf->op_id) + sizeof(rpc_buf->para_num));
	ptr = rpc_buf->buffer;
	for (i = 0; i < rpc_buf->para_num; i++) {
		if ((data_len + sizeof(pkt[i].len) + pkt[i].len) > RPC_MAX_BUF_SIZE) {
			CCCI_ERROR_LOG(md_id, RPC, "RPC overflow in write %zu\n",
				     data_len + sizeof(pkt[i].len) + pkt[i].len);
			goto err_out;
		}

		*((unsigned int *)ptr) = pkt[i].len;
		ptr += sizeof(pkt[i].len);
		data_len += sizeof(pkt[i].len);

		AlignLength = ((pkt[i].len + 3) >> 2) << 2;	/* 4byte aligned */
		data_len += AlignLength;

		if (ptr != pkt[i].buf)
			memcpy(ptr, pkt[i].buf, pkt[i].len);
		else
			CCCI_DEBUG_LOG(md_id, RPC, "same addr, no copy, op_id=0x%x\n", rpc_buf->op_id);

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
	CCCI_DEBUG_LOG(md_id, RPC, "Write %d/%d, %08X, %08X, %08X, %08X, op_id=0x%x\n", skb->len, data_len,
		     rpc_buf->header.data[0], rpc_buf->header.data[1], rpc_buf->header.channel,
		     rpc_buf->header.reserved, rpc_buf->op_id);
	/* switch to Tx request */
	ret = port_proxy_send_skb_to_md(port->port_proxy, port, skb, 1);
	if (ret)
		goto err_out;
	kfree(tmp_data);
	return;

 err_out:
	kfree(tmp_data);
	ccci_free_skb(skb);
}

static int port_rpc_init(struct ccci_port *port)
{
	CCCI_DEBUG_LOG(port->md_id, KERN, "rpc port %s is initializing\n", port->name);
	port->skb_handler = &rpc_msg_handler;
	port->private_data = kthread_run(port_kthread_handler, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	get_dtsi_eint_node(port->md_id);
	return 0;
}

int port_rpc_recv_match(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct rpc_buffer *rpc_buf;

	if (ccci_h->channel == CCCI_RPC_RX) {
		rpc_buf = (struct rpc_buffer *)skb->data;
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
			CCCI_DEBUG_LOG(md_id, KERN, "userspace rpc msg 0x%x on %s\n",
						rpc_buf->op_id, port->name);
			return 0;
#endif
		default:
			CCCI_DEBUG_LOG(md_id, KERN, "kernelspace rpc msg 0x%x on %s\n",
						rpc_buf->op_id, port->name);
			return 1;
		}
	}
	return 0;
}

struct ccci_port_ops rpc_port_ops = {
	.init = &port_rpc_init,
	.recv_match = &port_rpc_recv_match,
	.recv_skb = &port_recv_skb,
};

