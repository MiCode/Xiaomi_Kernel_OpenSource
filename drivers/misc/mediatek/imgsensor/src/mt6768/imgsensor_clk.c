/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "imgsensor_common.h"

#include <linux/clk.h>
#include "imgsensor_clk.h"
#include <linux/soc/mediatek/mtk-pm-qos.h> 

/*by platform settings and elements should not be reordered */
char *gimgsensor_mclk_name[IMGSENSOR_CCF_MAX_NUM] = {
	"CLK_TOP_CAMTG_SEL",
	"CLK_TOP_CAMTG1_SEL",
	"CLK_TOP_CAMTG2_SEL",
	"CLK_TOP_CAMTG3_SEL",
	"CLK_MCLK_6M",
	"CLK_MCLK_12M",
	"CLK_MCLK_13M",
	"CLK_MCLK_24M",
	"CLK_MCLK_26M",
	"CLK_MCLK_48M",
	"CLK_MCLK_52M",
	"CLK_CAM_SENINF_CG",
	"CLK_MIPI_C0_26M_CG",
	"CLK_MIPI_C1_26M_CG",
	"CLK_MIPI_ANA_0A_CG",
	"CLK_MIPI_ANA_0B_CG",
	"CLK_MIPI_ANA_1A_CG",
	"CLK_MIPI_ANA_1B_CG",
	"CLK_MIPI_ANA_2A_CG",
	"CLK_MIPI_ANA_2B_CG",
	"CLK_TOP_CAMTM_SEL_CG",
	"CLK_TOP_CAMTM_208_CG",
	"CLK_SCP_SYS_CAM",
};


enum {
	MCLK_ENU_START,
	MCLK_6MHZ =	MCLK_ENU_START,
	MCLK_12MHZ,
	MCLK_13MHZ,
	MCLK_24MHZ,
	MCLK_26MHZ,
	MCLK_48MHZ,
	MCLK_52MHZ,
	MCLK_MAX,
};

enum {
	FREQ_6MHZ  =  6,
	FREQ_12MHZ = 12,
	FREQ_13MHZ = 13,
	FREQ_24MHZ = 24,
	FREQ_26MHZ = 26,
	FREQ_48MHZ = 48,
	FREQ_52MHZ = 52,
};

#ifdef IMGSENSOR_DFS_CTRL_ENABLE
struct mtk_pm_qos_request imgsensor_qos;

int imgsensor_dfs_ctrl(enum DFS_OPTION option, void *pbuff)
{
	int i4RetValue = 0;
	if ((option == DFS_UPDATE ||
		option == DFS_SUPPORTED_ISP_CLOCKS ||
		option == DFS_CUR_ISP_CLOCK)) {
		if (pbuff == NULL) {
			pr_info("pbuff == null");
			return IMGSENSOR_RETURN_ERROR;
		}
	}
	/*pr_info("%s\n", __func__);*/

	switch (option) {
	case DFS_CTRL_ENABLE:
		mtk_pm_qos_add_request(&imgsensor_qos, PM_QOS_CAM_FREQ, 0);
		pr_debug("seninf PMQoS turn on\n");
		break;
	case DFS_CTRL_DISABLE:
		mtk_pm_qos_remove_request(&imgsensor_qos);
		pr_debug("seninf PMQoS turn off\n");
		break;
	case DFS_UPDATE:
		pr_debug(
			"seninf Set isp clock level:%d\n",
			*(unsigned int *)pbuff);
		mtk_pm_qos_update_request(&imgsensor_qos, *(unsigned int *)pbuff);

		break;
	case DFS_RELEASE:
		pr_debug(
			"seninf release and set isp clk request to 0\n");
		mtk_pm_qos_update_request(&imgsensor_qos, 0);

		break;
	case DFS_SUPPORTED_ISP_CLOCKS:
	{
		int result = 0;
		uint64_t freq_steps[ISP_CLK_LEVEL_CNT] = {0};
		struct IMAGESENSOR_GET_SUPPORTED_ISP_CLK *pIspclks;
		unsigned int lv = 0;

		pIspclks = (struct IMAGESENSOR_GET_SUPPORTED_ISP_CLK *) pbuff;

		/* Call mmdvfs_qos_get_freq_steps
		 * to get supported frequency
		 */
		result = mmdvfs_qos_get_freq_steps(
			PM_QOS_CAM_FREQ,
			freq_steps, (u32 *)&pIspclks->clklevelcnt);

		if (result < 0) {
			pr_err(
				"ERR: get MMDVFS freq steps failed, result: %d\n",
				result);
			i4RetValue = -EFAULT;
			break;
		}

		if (pIspclks->clklevelcnt > ISP_CLK_LEVEL_CNT) {
			pr_err("ERR: clklevelcnt is exceeded");
			i4RetValue = -EFAULT;
			break;
		}

		for (lv = 0; lv < pIspclks->clklevelcnt; lv++) {
			/* Save clk from low to high */
			pIspclks->clklevel[lv] = freq_steps[lv];
			/*pr_debug("DFS Clk level[%d]:%d",
			 *	lv, pIspclks->clklevel[lv]);
			 */
		}
	}
		break;
	case DFS_CUR_ISP_CLOCK:
	{
		unsigned int *pGetIspclk;

		pGetIspclk = (unsigned int *) pbuff;
		*pGetIspclk = (u32)mmdvfs_qos_get_freq(PM_QOS_CAM_FREQ);
		/*pr_debug("current isp clock:%d", *pGetIspclk);*/
	}
		break;
	default:
		pr_info("None\n");
		break;
	}
	return i4RetValue;
}
#endif
static inline void imgsensor_clk_check(struct IMGSENSOR_CLK *pclk)
{
	int i;

	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++) {
		if (IS_ERR(pclk->imgsensor_ccf[i]))
			pr_debug("%s fail %s",
				 __func__,
				 gimgsensor_mclk_name[i]);
	}
}

/************************************************************************
 * Common Clock Framework (CCF)
 ************************************************************************/
enum IMGSENSOR_RETURN imgsensor_clk_init(struct IMGSENSOR_CLK *pclk)
{
	int i;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;

	if (pplatform_dev == NULL) {
		pr_err("[%s] pdev is null\n", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}
	/* get all possible using clocks */
	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++)
		pclk->imgsensor_ccf[i] =
		    devm_clk_get(&pplatform_dev->dev, gimgsensor_mclk_name[i]);

	return IMGSENSOR_RETURN_SUCCESS;
}

int imgsensor_clk_set(
	struct IMGSENSOR_CLK *pclk, struct ACDK_SENSOR_MCLK_STRUCT *pmclk)
{
	int ret = 0;
	int mclk_index = MCLK_ENU_START;
	const int supported_mclk_freq[MCLK_MAX] = {
		FREQ_6MHZ, FREQ_12MHZ, FREQ_13MHZ, FREQ_24MHZ,
		FREQ_26MHZ, FREQ_48MHZ, FREQ_52MHZ };

	for (mclk_index = MCLK_ENU_START; mclk_index < MCLK_MAX; mclk_index++) {
		if (pmclk->freq == supported_mclk_freq[mclk_index])
			break;
	}
	if (pmclk->TG >= IMGSENSOR_CCF_MCLK_TG_MAX_NUM ||
		pmclk->TG < IMGSENSOR_CCF_MCLK_TG_MIN_NUM ||
		mclk_index == MCLK_MAX) {
		pr_err(
		    "[CAMERA SENSOR]kdSetSensorMclk out of range, tg=%d, freq= %d\n",
		    pmclk->TG,
		    pmclk->freq);

		return -EFAULT;
	}
	mclk_index += IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM;
	imgsensor_clk_check(pclk);

	if (pmclk->on) {

		/* Workaround for timestamp: TG1 always ON */
		if (clk_prepare_enable(
		    pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]))

			pr_err(
			    "[CAMERA SENSOR] failed tg=%d\n",
			    IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL);
		else
			atomic_inc(
			   &pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[pmclk->TG]))
			pr_err("[CAMERA SENSOR] failed tg=%d\n", pmclk->TG);
		else
			atomic_inc(&pclk->enable_cnt[pmclk->TG]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[mclk_index]))
			pr_err(
			    "[CAMERA SENSOR]imgsensor_ccf failed freq= %d, mclk_index %d\n",
			    pmclk->freq,
			    mclk_index);
		else
			atomic_inc(&pclk->enable_cnt[mclk_index]);

		ret = clk_set_parent(
		    pclk->imgsensor_ccf[pmclk->TG],
		    pclk->imgsensor_ccf[mclk_index]);

	} else {

		/* Workaround for timestamp: TG1 always ON */
		clk_disable_unprepare(
		    pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		atomic_dec(&pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		clk_disable_unprepare(pclk->imgsensor_ccf[pmclk->TG]);
		atomic_dec(&pclk->enable_cnt[pmclk->TG]);
		clk_disable_unprepare(pclk->imgsensor_ccf[mclk_index]);
		atomic_dec(&pclk->enable_cnt[mclk_index]);
	}

	return ret;
}

void imgsensor_clk_enable_all(struct IMGSENSOR_CLK *pclk)
{
	int i;

	pr_info("imgsensor_clk_enable_all_cg\n");
	for (i = IMGSENSOR_CCF_MTCMOS_MIN_NUM;
		i < IMGSENSOR_CCF_MTCMOS_MAX_NUM;
		i++) {
		if (!IS_ERR(pclk->imgsensor_ccf[i])) {
			if (clk_prepare_enable(pclk->imgsensor_ccf[i]))
				pr_debug(
					"[CAMERA SENSOR]imgsensor_ccf enable cmos fail cg_index = %d\n",
					i);
			else
				atomic_inc(&pclk->enable_cnt[i]);
			/*pr_debug("imgsensor_clk_enable_all %s ok\n",*/
				/*gimgsensor_mclk_name[i]);*/
		}
	}
	for (i = IMGSENSOR_CCF_CG_MIN_NUM; i < IMGSENSOR_CCF_CG_MAX_NUM; i++) {
		if (!IS_ERR(pclk->imgsensor_ccf[i])) {
			if (clk_prepare_enable(pclk->imgsensor_ccf[i]))
				pr_debug(
					"[CAMERA SENSOR]imgsensor_ccf enable cg fail cg_index = %d\n",
					i);
			else
				atomic_inc(&pclk->enable_cnt[i]);
			/*pr_debug("imgsensor_clk_enable_all %s ok\n",*/
				/*gimgsensor_mclk_name[i]);*/

		}
	}
}

void imgsensor_clk_disable_all(struct IMGSENSOR_CLK *pclk)
{
	unsigned int i;
	pr_info("%s\n", __func__);
	for (i = IMGSENSOR_CCF_MCLK_TG_MIN_NUM;
		i < IMGSENSOR_CCF_MAX_NUM;
		i++) {
		for (; !IS_ERR(pclk->imgsensor_ccf[i]) &&
			atomic_read(&pclk->enable_cnt[i]) > 0 ;) {
			clk_disable_unprepare(pclk->imgsensor_ccf[i]);
			atomic_dec(&pclk->enable_cnt[i]);
		}
	}
}

int imgsensor_clk_ioctrl_handler(void *pbuff)
{
	if (pbuff == NULL)
		pr_info(" %s pbuff == null", __func__);
	else
		*(unsigned int *)pbuff = mt_get_ckgen_freq(*(unsigned int *)pbuff);
	pr_info("hf_fcamtg_ck = %d, hf_fmm_ck = %d, f_fseninf_ck = %d\n",
		mt_get_ckgen_freq(7),
		mt_get_ckgen_freq(3),
		mt_get_ckgen_freq(27));
	return 0;
}
