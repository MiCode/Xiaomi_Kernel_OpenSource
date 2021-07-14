// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-debug.h"

#define DL_CHECK_ENG_NUM 9
struct imgsys_dbg_engine_t dbg_engine_name_list[DL_CHECK_ENG_NUM] = {
	{IMGSYS_ENG_WPE_EIS, "WPE_EIS"},
	{IMGSYS_ENG_WPE_TNR, "WPE_TNR"},
	{IMGSYS_ENG_WPE_LITE, "WPE_LITE"},
	{IMGSYS_ENG_TRAW, "TRAW"},
	{IMGSYS_ENG_LTR, "LTRAW"},
	{IMGSYS_ENG_XTR, "XTRAW"},
	{IMGSYS_ENG_DIP, "DIP"},
	{IMGSYS_ENG_PQDIP_A, "PQDIPA"},
	{IMGSYS_ENG_PQDIP_B, "PQDIPB"},
};

void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
	const struct module_ops *imgsys_modules,
	int imgsys_module_num, unsigned int hw_comb)
{
	bool module_on[IMGSYS_MOD_MAX] = {
		false, false, false, false, false};
	int i = 0;

	dev_info(imgsys_dev->dev,
			"%s: hw comb set: 0x%lx\n",
			__func__, hw_comb);

	imgsys_dl_debug_dump(imgsys_dev, hw_comb);

	if ((hw_comb & IMGSYS_ENG_WPE_EIS) || (hw_comb & IMGSYS_ENG_WPE_TNR)
		 || (hw_comb & IMGSYS_ENG_WPE_LITE))
		module_on[IMGSYS_MOD_WPE] = true;
	if ((hw_comb & IMGSYS_ENG_TRAW) || (hw_comb & IMGSYS_ENG_LTR)
		 || (hw_comb & IMGSYS_ENG_XTR))
		module_on[IMGSYS_MOD_TRAW] = true;
	if ((hw_comb & IMGSYS_ENG_DIP))
		module_on[IMGSYS_MOD_DIP] = true;
	if ((hw_comb & IMGSYS_ENG_PQDIP_A) || (hw_comb & IMGSYS_ENG_PQDIP_B))
		module_on[IMGSYS_MOD_PQDIP] = true;
	if ((hw_comb & IMGSYS_ENG_ME))
		module_on[IMGSYS_MOD_ME] = true;

	/* in case module driver did not set imgsys_modules in module order */
	dev_info(imgsys_dev->dev,
			"%s: imgsys_module_num: %d\n",
			__func__, imgsys_module_num);
	for (i = 0 ; i < imgsys_module_num ; i++) {
		if (module_on[imgsys_modules[i].module_id])
			imgsys_modules[i].dump(imgsys_dev, hw_comb);
	}
}
EXPORT_SYMBOL(imgsys_debug_dump_routine);

#define log_length (64)
void imgsys_dl_checksum_dump(struct mtk_imgsys_dev *imgsys_dev,
	unsigned int hw_comb, char *logBuf_path,
	char *logBuf_inport, char *logBuf_outport, int dl_path)
{
	void __iomem *imgsysmainRegBA = 0L;
	void __iomem *wpedip1RegBA = 0L;
	unsigned int checksum_dbg_sel = 0x0;
	unsigned int original_dbg_sel_value = 0x0;
	char logBuf_final[log_length * 4];
	int debug0_req[2] = {0, 0};
	int debug0_rdy[2] = {0, 0};
	int debug0_checksum[2] = {0, 0};
	int debug1_line_cnt[2] = {0, 0};
	int debug1_pix_cnt[2] = {0, 0};
	int debug2_line_cnt[2] = {0, 0};
	int debug2_pix_cnt[2] = {0, 0};
	unsigned int dbg_sel_value[2] = {0x0, 0x0};
	unsigned int debug0_value[2] = {0x0, 0x0};
	unsigned int debug1_value[2] = {0x0, 0x0};
	unsigned int debug2_value[2] = {0x0, 0x0};
	unsigned int wpe_pqdip_mux_v = 0x0;
	char logBuf_temp[log_length];

	memset((char *)logBuf_final, 0x0, log_length * 4);
	logBuf_final[strlen(logBuf_final)] = '\0';
	memset((char *)logBuf_temp, 0x0, log_length);
	logBuf_temp[strlen(logBuf_temp)] = '\0';

	dev_info(imgsys_dev->dev,
		"%s: + hw_comb/path(0x%x/%s) dl_path:%d, start dump\n",
		__func__, hw_comb, logBuf_path, dl_path);
	/* iomap registers */
	imgsysmainRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);
	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	/*dump former engine in DL (imgsys main in port) status */
	checksum_dbg_sel = (unsigned int)((dl_path << 1) | (0 << 0));
	original_dbg_sel_value = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	original_dbg_sel_value = original_dbg_sel_value & 0xff00ffff; /*clear last time data*/
	dbg_sel_value[0] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[0], (imgsysmainRegBA + 0x4C));
	dbg_sel_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	debug0_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
	debug0_checksum[0] = (debug0_value[0] & 0x0000ffff);
	debug0_rdy[0] = (debug0_value[0] & 0x00800000) >> 23;
	debug0_req[0] = (debug0_value[0] & 0x01000000) >> 24;
	debug1_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
	debug1_line_cnt[0] = ((debug1_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[0] = (debug1_value[0] & 0x0000ffff);
	debug2_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
	debug2_line_cnt[0] = ((debug2_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[0] = (debug2_value[0] & 0x0000ffff);

	/*dump later engine in DL (imgsys main out port) status */
	checksum_dbg_sel = (unsigned int)((dl_path << 1) | (1 << 0));
	dbg_sel_value[1] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[1], (imgsysmainRegBA + 0x4C));
	dbg_sel_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	debug0_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
	debug0_checksum[1] = (debug0_value[1] & 0x0000ffff);
	debug0_rdy[1] = (debug0_value[1] & 0x00800000) >> 23;
	debug0_req[1] = (debug0_value[1] & 0x01000000) >> 24;
	debug1_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
	debug1_line_cnt[1] = ((debug1_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[1] = (debug1_value[1] & 0x0000ffff);
	debug2_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
	debug2_line_cnt[1] = ((debug2_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[1] = (debug2_value[1] & 0x0000ffff);

	/* macro_comm status */
	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	wpedip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE_DIP1);
	if (!wpedip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux_v = (unsigned int)ioread32((void *)(wpedip1RegBA + 0xA8));
	iounmap(wpedip1RegBA);
	/*}*/

	/* dump information */

	if (debug0_req[0] == 1) {
		snprintf(logBuf_temp, log_length,
			"%s req to send data to %s/",
			logBuf_inport, logBuf_outport);
	} else {
		snprintf(logBuf_temp, log_length,
			"%s not send data to %s/",
			logBuf_inport, logBuf_outport);
	}
	strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
	memset((char *)logBuf_temp, 0x0, log_length);
	logBuf_temp[strlen(logBuf_temp)] = '\0';
	if (debug0_rdy[0] == 1) {
		snprintf(logBuf_temp, log_length,
			"%s rdy to receive data from %s",
			logBuf_outport, logBuf_inport);
	} else {
		snprintf(logBuf_temp, log_length,
			"%s not rdy to receive data from %s",
			logBuf_outport, logBuf_inport);
	}
	strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
	dev_info(imgsys_dev->dev,
		"%s: %s", __func__, logBuf_final);

	memset((char *)logBuf_final, 0x0, log_length * 4);
	logBuf_final[strlen(logBuf_final)] = '\0';
	memset((char *)logBuf_temp, 0x0, log_length);
	logBuf_temp[strlen(logBuf_temp)] = '\0';
	if (debug0_req[1] == 1) {
		snprintf(logBuf_temp, log_length,
			"%s req to send data to %sPIPE/",
			logBuf_outport, logBuf_outport);
	} else {
		snprintf(logBuf_temp, log_length,
			"%s not send data to %sPIPE/",
			logBuf_outport, logBuf_outport);
	}
	strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
	memset((char *)logBuf_temp, 0x0, log_length);
	logBuf_temp[strlen(logBuf_temp)] = '\0';
	if (debug0_rdy[1] == 1) {
		snprintf(logBuf_temp, log_length,
			"%sPIPE rdy to receive data from %s",
			logBuf_outport, logBuf_outport);
	} else {
		snprintf(logBuf_temp, log_length,
			"%sPIPE not rdy to receive data from %s",
			logBuf_outport, logBuf_outport);
	}
	strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
	dev_info(imgsys_dev->dev,
		"%s: %s", __func__, logBuf_final);
	dev_info(imgsys_dev->dev,
		"%s: in_req/in_rdy/out_req/out_rdy = %d/%d/%d/%d,(cheskcum: in/out) = (%d/%d)",
		__func__,
		debug0_req[0], debug0_rdy[0],
		debug0_req[1], debug0_rdy[1],
		debug0_checksum[0], debug0_checksum[1]);
	dev_info(imgsys_dev->dev,
		"%s: info01 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
		__func__,
		debug1_line_cnt[0], debug1_pix_cnt[0], debug1_line_cnt[1],
		debug1_pix_cnt[1]);
	dev_info(imgsys_dev->dev,
		"%s: info02 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
		__func__,
		debug2_line_cnt[0], debug2_pix_cnt[0], debug2_line_cnt[1],
		debug2_pix_cnt[1]);
	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_inport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4C), dbg_sel_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x200), debug0_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x204), debug1_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x208), debug2_value[0]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_outport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4C), dbg_sel_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x200), debug0_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x204), debug1_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x208), debug2_value[1]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): IMGMAIN CG INFO===",
		__func__, logBuf_path);
	dev_info(imgsys_dev->dev, "%s: CG_CON  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x0),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x0)));
	dev_info(imgsys_dev->dev, "%s: CG_SET  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4)));
	dev_info(imgsys_dev->dev, "%s: CG_CLR  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x8),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x8)));

	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15220000 + 0xA8), wpe_pqdip_mux_v);
	/*}*/
	iounmap(imgsysmainRegBA);
}

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb)
{
	int dl_path = 0;
	char logBuf_path[log_length];
	char logBuf_inport[log_length];
	char logBuf_outport[log_length];
	char logBuf_eng[log_length];
	int i = 0, get = false;

	memset((char *)logBuf_path, 0x0, log_length);
	logBuf_path[strlen(logBuf_path)] = '\0';
	memset((char *)logBuf_inport, 0x0, log_length);
	logBuf_inport[strlen(logBuf_inport)] = '\0';
	memset((char *)logBuf_outport, 0x0, log_length);
	logBuf_outport[strlen(logBuf_outport)] = '\0';

	for (i = 0 ; i < DL_CHECK_ENG_NUM ; i++) {
		memset((char *)logBuf_eng, 0x0, log_length);
		logBuf_eng[strlen(logBuf_eng)] = '\0';
		if (hw_comb & dbg_engine_name_list[i].eng_e) {
			if (get) {
				snprintf(logBuf_eng, log_length, "-%s",
					dbg_engine_name_list[i].eng_name);
			} else {
				snprintf(logBuf_eng, log_length, "%s",
					dbg_engine_name_list[i].eng_name);
			}
			get = true;
		}
		strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));
	}
	memset((char *)logBuf_eng, 0x0, log_length);
	logBuf_eng[strlen(logBuf_eng)] = '\0';
	snprintf(logBuf_eng, log_length, "%s", " FAIL");
	strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));

	dev_info(imgsys_dev->dev, "%s: %s\n",
			__func__, logBuf_path);
	switch (hw_comb) {
	/*DL checksum case*/
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPEE_XTRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPET_XTRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW):
		/*
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		*/
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL TRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_LTR):
		/*
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		*/
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL LTRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_XTR):
		/*
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		*/
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL XTRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			 "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		/*
		dl_path = IMGSYS_DL_WPEL_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		*/
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dev_info(imgsys_dev->dev,
			"%s: TOBE CHECKED SELECTION BASED ON FMT..\n",
			__func__);
		break;
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
	    logBuf_inport[strlen(logBuf_inport)] = '\0';
	    memset((char *)logBuf_outport, 0x0, log_length);
	    logBuf_outport[strlen(logBuf_outport)] = '\0';
	    dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	default:
		break;
	}

	dev_info(imgsys_dev->dev, "%s: -\n", __func__);
}
