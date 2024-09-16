/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include "plat.h"

#include "fm_cmd.h"

static int (*whole_chip_reset)(signed int sta);

static void WCNfm_wholechip_rst_cb(
	ENUM_WMTDRV_TYPE_T src, ENUM_WMTDRV_TYPE_T dst,
	ENUM_WMTMSG_TYPE_T type, void *buf, unsigned int sz)
{
	/* To handle reset procedure please */
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	if (sz > sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		/*message format invalid */
		WCN_DBG(FM_WAR | LINK, "message format invalid!\n");
		return;
	}

	memcpy((char *)&rst_msg, (char *)buf, sz);
	WCN_DBG(FM_WAR | LINK,
		"[src=%d], [dst=%d], [type=%d], [buf=0x%x], [sz=%d], [max=%d]\n",
		src, dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);

	if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_FM)
	    && (type == WMTMSG_TYPE_RESET)) {

		if (rst_msg == WMTRSTMSG_RESET_START) {
			WCN_DBG(FM_WAR | LINK, "FM restart start!\n");
			if (whole_chip_reset)
				whole_chip_reset(1);
		} else if (rst_msg == WMTRSTMSG_RESET_END_FAIL) {
			WCN_DBG(FM_WAR | LINK, "FM restart end fail!\n");
			if (whole_chip_reset)
				whole_chip_reset(2);
		} else if (rst_msg == WMTRSTMSG_RESET_END) {
			WCN_DBG(FM_WAR | LINK, "FM restart end!\n");
			if (whole_chip_reset)
				whole_chip_reset(0);
		}
	}
}

static void fw_eint_handler(void)
{
	fm_event_parser(fm_rds_parser);
}

static int fm_stp_send_data(unsigned char *buf, unsigned int len)
{
	return mtk_wcn_stp_send_data(buf, len, FM_TASK_INDX);
}

static int fm_stp_recv_data(unsigned char *buf, unsigned int len)
{
	return mtk_wcn_stp_receive_data(buf, len, FM_TASK_INDX);
}

static int fm_stp_register_event_cb(void *cb)
{
	return mtk_wcn_stp_register_event_cb(FM_TASK_INDX, cb);
}

static int fm_wmt_msgcb_reg(void *data)
{
	/* get whole chip reset cb */
	whole_chip_reset = data;
	return mtk_wcn_wmt_msgcb_reg(
		WMTDRV_TYPE_FM, WCNfm_wholechip_rst_cb);
}

static int fm_wmt_func_on(void)
{
	int ret = 0;

	ret = mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM) != MTK_WCN_BOOL_FALSE;

	return ret;
}

static int fm_wmt_func_off(void)
{
	int ret = 0;

	ret = mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM) != MTK_WCN_BOOL_FALSE;

	return ret;
}

static unsigned int fm_wmt_ic_info_get(void)
{
	return mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);
}

static int fm_wmt_chipid_query(void)
{
	return mtk_wcn_wmt_chipid_query();
}

static signed int fm_drv_switch_clk_64m(void)
{
	unsigned int val = 0;
	int i = 0, ret = 0;

	/* switch SPI clock to 64MHz */
	fm_host_reg_read(0x81026004, &val);
	/* Set 0x81026004[0] = 0x1 */
	ret = fm_host_reg_write(0x81026004, val | 0x1);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP,
			"Switch SPI clock to 64MHz failed\n");
		return -1;
	}

	for (i = 0; i < 100; i++) {
		fm_host_reg_read(0x81026004, &val);
		if ((val & 0x18) == 0x10)
			break;
		fm_delayus(10);
	}

	if (i == 100) {
		WCN_DBG(FM_ERR | CHIP,
			"switch_SPI_clock_to_64MHz polling timeout\n");
		return -1;
	}

	/* Capture next (with SPI Clock: 64MHz) */
	fm_host_reg_read(0x81026004, &val);
	/* Set 0x81026004[2] = 0x1 */
	ret = fm_host_reg_write(0x81026004, val | 0x4);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP,
			"Switch SPI clock to 64MHz failed\n");
		return -1;
	}

	return 0;
}

static signed int fm_drv_switch_clk_26m(void)
{
	unsigned int val = 0;
	int i = 0, ret = 0;

	/* Capture next (with SPI Clock: 26MHz) */
	fm_host_reg_read(0x81026004, &val);
	/* Set 0x81026004[2] = 0x0 */
	ret = fm_host_reg_write(0x81026004, val & 0xFFFFFFFB);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP,
			"Switch SPI clock to 26MHz failed\n");
		return -1;
	}

	/* switch SPI clock to 26MHz */
	fm_host_reg_read(0x81026004, &val);
	/* Set 0x81026004[0] = 0x0 */
	ret = fm_host_reg_write(0x81026004, val & 0xFFFFFFFE);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP,
			"Switch SPI clock to 26MHz failed\n");
		return -1;
	}

	for (i = 0; i < 100; i++) {
		fm_host_reg_read(0x81026004, &val);
		if ((val & 0x18) == 0x8)
			break;
		fm_delayus(10);
	}

	if (i == 100) {
		WCN_DBG(FM_ERR | CHIP,
			"switch_SPI_clock_to_26MHz polling timeout\n");
		return -1;
	}

	return 0;
}

static int fm_drv_spi_clock_switch(enum fm_spi_speed speed)
{
	int ret = 0;

	switch (speed) {
	case FM_SPI_SPEED_26M:
		ret = fm_drv_switch_clk_26m();
		break;
	case FM_SPI_SPEED_64M:
		ret = fm_drv_switch_clk_64m();
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int drv_get_hw_version(void)
{
#if defined(MT6625_FM) || defined(MT6627_FM) || defined(MT6630_FM) || defined(MT6632_FM) || defined(soc)
	return FM_CONNAC_LEGACY;
#else
	int id = fm_wmt_chipid_query();
	int ret = FM_CONNAC_UNKNOWN;

	switch (id) {
	case 0x6758:
	case 0x6759:
	case 0x6771:
	case 0x6775:
	case 0x6797:
		ret = FM_CONNAC_LEGACY;
		break;
	case 0x6765:
	case 0x6761:
	case 0x3967:
		ret = FM_CONNAC_1_0;
		break;
	case 0x6768:
	case 0x6785:
	case 0x8168:
		ret = FM_CONNAC_1_2;
		break;
	case 0x6779:
	case 0x6873:
	case 0x6853:
		ret = FM_CONNAC_1_5;
		break;
	default:
		ret = FM_CONNAC_UNKNOWN;
		break;
	}

	return ret;
#endif
}

static unsigned char drv_get_top_index(void)
{
	if (drv_get_hw_version() < FM_CONNAC_1_5)
		return 4;
	return 5;
}

static unsigned int drv_get_get_adie(void)
{
#if defined(MT6625_FM) || defined(MT6627_FM) || defined(MT6630_FM) || defined(MT6632_FM) || defined(soc)
	return 0;
#elif defined(MT6631_FM)
	return 0x6631;
#elif defined(MT6635_FM)
	return 0x6635;
#else
	return mtk_wcn_wmt_ic_info_get(WMTCHIN_ADIE);
#endif
}

signed int __weak fm_low_ops_register(struct fm_callback *cb,
				       struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak fm_low_ops_unregister(struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak fm_rds_ops_register(struct fm_basic_interface *bi,
				      struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak fm_rds_ops_unregister(struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6631_fm_low_ops_register(
	struct fm_callback *cb, struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6631_fm_low_ops_unregister(struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6631_fm_rds_ops_register(
	struct fm_basic_interface *bi, struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6631_fm_rds_ops_unregister(struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}
signed int __weak mt6635_fm_low_ops_register(
	struct fm_callback *cb, struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6635_fm_low_ops_unregister(struct fm_basic_interface *bi)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6635_fm_rds_ops_register(
	struct fm_basic_interface *bi, struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

signed int __weak mt6635_fm_rds_ops_unregister(struct fm_rds_interface *ri)
{
	WCN_DBG(FM_NTC | CHIP, "default %s\n", __func__);
	return -1;
}

void register_fw_ops_init(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;
	unsigned int adie = drv_get_get_adie();

	ei->eint_handler = fw_eint_handler;
	ei->stp_send_data = fm_stp_send_data;
	ei->stp_recv_data = fm_stp_recv_data;
	ei->stp_register_event_cb = fm_stp_register_event_cb;
	ei->wmt_msgcb_reg = fm_wmt_msgcb_reg;
	ei->wmt_func_on = fm_wmt_func_on;
	ei->wmt_func_off = fm_wmt_func_off;
	ei->wmt_ic_info_get = fm_wmt_ic_info_get;
	ei->wmt_chipid_query = fm_wmt_chipid_query;
	ei->spi_clock_switch = fm_drv_spi_clock_switch;
	ei->get_hw_version = drv_get_hw_version;
	ei->get_top_index = drv_get_top_index;
	ei->get_get_adie = drv_get_get_adie;

	WCN_DBG(FM_NTC | CHIP, "adie=0x%x\n", adie);

	if (adie == 0x6631) {
		ei->low_ops_register = mt6631_fm_low_ops_register;
		ei->low_ops_unregister = mt6631_fm_low_ops_unregister;
		ei->rds_ops_register = mt6631_fm_rds_ops_register;
		ei->rds_ops_unregister = mt6631_fm_rds_ops_unregister;
	} else if (adie == 0x6635) {
		ei->low_ops_register = mt6635_fm_low_ops_register;
		ei->low_ops_unregister = mt6635_fm_low_ops_unregister;
		ei->rds_ops_register = mt6635_fm_rds_ops_register;
		ei->rds_ops_unregister = mt6635_fm_rds_ops_unregister;
	} else {
#if defined(MT6631_FM)
		ei->low_ops_register = mt6631_fm_low_ops_register;
		ei->low_ops_unregister = mt6631_fm_low_ops_unregister;
		ei->rds_ops_register = mt6631_fm_rds_ops_register;
		ei->rds_ops_unregister = mt6631_fm_rds_ops_unregister;
#elif defined(MT6635_FM)
		ei->low_ops_register = mt6635_fm_low_ops_register;
		ei->low_ops_unregister = mt6635_fm_low_ops_unregister;
		ei->rds_ops_register = mt6635_fm_rds_ops_register;
		ei->rds_ops_unregister = mt6635_fm_rds_ops_unregister;
#else
		ei->low_ops_register = fm_low_ops_register;
		ei->low_ops_unregister = fm_low_ops_unregister;
		ei->rds_ops_register = fm_rds_ops_register;
		ei->rds_ops_unregister = fm_rds_ops_unregister;
#endif
	}
}

void register_fw_ops_uninit(void)
{
}

int fm_register_irq(struct platform_driver *drv)
{
	return 0;
}

int fm_wcn_ops_register(void)
{
	register_fw_ops_init();

	return 0;
}

int fm_wcn_ops_unregister(void)
{
	register_fw_ops_uninit();

	return 0;
}
