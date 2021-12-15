/*#define DEBUG*/
#include <linux/module.h>
#include <linux/debugfs.h>
#include <asm/ioctls.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/version.h>

#include "aw_device.h"
#include "aw_dsp.h"
#include "aw_log.h"
/*#include "aw_afe.h"*/

static DEFINE_MUTEX(g_aw_dsp_msg_lock);
static DEFINE_MUTEX(g_aw_dsp_lock);


#define AW_MSG_ID_ENABLE_CALI		(0x00000001)
#define AW_MSG_ID_ENABLE_HMUTE		(0x00000002)
#define AW_MSG_ID_F0_Q			(0x00000003)
#define AW_MSG_ID_DIRECT_CUR_FLAG	(0x00000006)
#define AW_MSG_ID_SPK_STATUS		(0x00000007)

/*dsp params id*/
#define AW_MSG_ID_RX_SET_ENABLE		(0x10013D11)
#define AW_MSG_ID_PARAMS		(0x10013D12)
#define AW_MSG_ID_TX_SET_ENABLE		(0x10013D13)
#define AW_MSG_ID_VMAX_L		(0X10013D17)
#define AW_MSG_ID_VMAX_R		(0X10013D18)
#define AW_MSG_ID_CALI_CFG_L		(0X10013D19)
#define AW_MSG_ID_CALI_CFG_R		(0x10013d1A)
#define AW_MSG_ID_RE_L			(0x10013d1B)
#define AW_MSG_ID_RE_R			(0X10013D1C)
#define AW_MSG_ID_NOISE_L		(0X10013D1D)
#define AW_MSG_ID_NOISE_R		(0X10013D1E)
#define AW_MSG_ID_F0_L			(0X10013D1F)
#define AW_MSG_ID_F0_R			(0X10013D20)
#define AW_MSG_ID_REAL_DATA_L		(0X10013D21)
#define AW_MSG_ID_REAL_DATA_R		(0X10013D22)

#define AFE_MSG_ID_MSG_0	(0X10013D2A)
#define AFE_MSG_ID_MSG_1	(0X10013D2B)
#define AW_MSG_ID_PARAMS_1		(0x10013D2D)

#define AW_MSG_ID_SPIN		(0x10013D2E)

int g_tx_topo_id = AW_TX_DEFAULT_TOPO_ID;
int g_rx_topo_id = AW_RX_DEFAULT_TOPO_ID;
int g_tx_port_id = AW_TX_DEFAULT_PORT_ID;
int g_rx_port_id = AW_RX_DEFAULT_PORT_ID;

enum {
	MSG_PARAM_ID_0 = 0,
	MSG_PARAM_ID_1,
	MSG_PARAM_ID_MAX,
};

static uint32_t afe_param_msg_id[MSG_PARAM_ID_MAX] = {
	AFE_MSG_ID_MSG_0,
	AFE_MSG_ID_MSG_1,
};

/***************dsp communicate**************/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 1))
#include <dsp/msm_audio_ion.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6adm-v2.h>
#else
#include <linux/msm_audio_ion.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6adm-v2.h>
#include <sound/adsp_err.h>
#endif

#define AW_COPP_MODULE_ID (0X10013D02)			/*SKT module id*/
#define AW_COPP_PARAMS_ID_AWDSP_ENABLE (0X10013D14)	/*SKT enable param id*/

#ifdef AW_MTK_PLATFORM
extern int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, uint32_t data_size);
extern int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer, int16_t size, uint32_t *buf_len);
#elif defined AW_QCOM_PLATFORM
extern int afe_get_topology(int port_id);
extern int aw_send_afe_cal_apr(uint32_t param_id,
	void *buf, int cmd_size, bool write);
extern int aw_send_afe_rx_module_enable(void *buf, int size);
extern int aw_send_afe_tx_module_enable(void *buf, int size);
#else
static int aw_send_afe_cal_apr(uint32_t param_id,
	void *buf, int cmd_size, bool write)
{
	return 0;
}
static int aw_send_afe_rx_module_enable(void *buf, int size)
{
	return 0;
}
static int aw_send_afe_tx_module_enable(void *buf, int size)
{
	return 0;
}
#endif

#ifdef AW_QCOM_PLATFORM
extern void aw_set_port_id(int tx_port_id, int rx_port_id);
#else
static void aw_set_port_id(int tx_port_id, int rx_port_id) {
	return;
}
#endif

static int aw_adm_param_enable(int port_id, int module_id, int param_id, int enable)
{
#if 0
	/*for v3*/
	int copp_idx = 0;
	uint32_t enable_param;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	pr_debug("%s port_id %d, module_id 0x%x, enable %d\n",
			__func__, port_id, module_id, enable);

	copp_idx = adm_get_default_copp_idx(port_id);
	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
			pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
			return -EINVAL;
	}

	if (enable < 0 || enable > 1) {
			pr_err("%s: Invalid value for enable %d\n", __func__, enable);
			return -EINVAL;
	}

	pr_debug("%s port_id %d, module_id 0x%x, copp_idx 0x%x, enable %d\n",
			__func__, port_id, module_id, copp_idx, enable);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = module_id;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param_id;
	param_hdr.param_size = sizeof(enable_param);
	enable_param = enable;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					(uint8_t *) &enable_param);
	if (rc)
		pr_err("%s: Failed to set enable of module(%d) instance(%d) to %d, err %d\n",
				__func__, module_id, INSTANCE_ID_0, enable, rc);
	return rc;
#endif
	return 0;
}

static int aw_get_msg_num(int dev_ch, int *msg_num)
{
	switch (dev_ch) {
	case AW_DEV_CH_PRI_L:
		*msg_num = MSG_PARAM_ID_0;
		break;
	case AW_DEV_CH_PRI_R:
		*msg_num = MSG_PARAM_ID_0;
		break;
	case AW_DEV_CH_SEC_L:
		*msg_num = MSG_PARAM_ID_1;
		break;
	case AW_DEV_CH_SEC_R:
		*msg_num = MSG_PARAM_ID_1;
		break;
	default:
		aw_pr_err("can not find msg num, channel %d ", dev_ch);
		return -EINVAL;
	}

	aw_pr_dbg("msg num[%d] ", *msg_num);
	return 0;
}

#ifdef AW_MTK_PLATFORM
/*****************mtk dsp communication function start**********************/
static int aw_mtk_write_data_to_dsp(int param_id, void *data, int size)
{
	int32_t *dsp_data = NULL;
	aw_dsp_msg_t *hdr = NULL;
	int ret;

	dsp_data = kzalloc(sizeof(aw_dsp_msg_t) + size, GFP_KERNEL);
	if (!dsp_data) {
		pr_err("%s: kzalloc dsp_msg error\n", __func__);
		return -ENOMEM;
	}

	hdr = (aw_dsp_msg_t *)dsp_data;
	hdr->type = AW_DSP_MSG_TYPE_DATA;
	hdr->opcode_id = param_id;
	hdr->version = AW_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_data) + sizeof(aw_dsp_msg_t),
		data, size);

	ret = mtk_spk_send_ipi_buf_to_dsp(dsp_data,
				sizeof(aw_dsp_msg_t) + size);
	if (ret < 0) {
		pr_err("%s:write data failed\n", __func__);
		kfree(dsp_data);
		dsp_data = NULL;
		return ret;
	}

	kfree(dsp_data);
	dsp_data = NULL;
	return 0;
}

static int aw_mtk_read_data_from_dsp(int param_id, void *data, int size)
{
	int ret;
	aw_dsp_msg_t hdr;

	hdr.type = AW_DSP_MSG_TYPE_CMD;
	hdr.opcode_id = param_id;
	hdr.version = AW_DSP_MSG_HDR_VER;

	mutex_lock(&g_aw_dsp_msg_lock);
	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, sizeof(aw_dsp_msg_t));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, size, &size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

static int aw_mtk_write_msg_to_dsp(int msg_num, int inline_id,
				void *data, int size)
{
	int32_t *dsp_msg = NULL;
	aw_dsp_msg_t *hdr = NULL;
	int ret;

	dsp_msg = kzalloc(sizeof(aw_dsp_msg_t) + size,
			GFP_KERNEL);
	if (!dsp_msg) {
		pr_err("%s: inline_id:0x%x kzalloc dsp_msg error\n",
			__func__, inline_id);
		return -ENOMEM;
	}
	hdr = (aw_dsp_msg_t *)dsp_msg;
	hdr->type = AW_DSP_MSG_TYPE_DATA;
	hdr->opcode_id = inline_id;
	hdr->version = AW_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_msg) + sizeof(aw_dsp_msg_t),
			data, size);

	ret = aw_mtk_write_data_to_dsp(afe_param_msg_id[msg_num], (void *)dsp_msg,
					sizeof(aw_dsp_msg_t) + size);
	if (ret < 0) {
		pr_err("%s:inline_id:0x%x, write data failed\n",
			__func__, inline_id);
		kfree(dsp_msg);
		dsp_msg = NULL;
		return ret;
	}

	kfree(dsp_msg);
	dsp_msg = NULL;
	return 0;
}

static int aw_mtk_read_msg_from_dsp(int msg_num, int inline_id,
					char *data, int size)
{
	aw_dsp_msg_t hdr[2];
	int ret;

	hdr[0].type = AW_DSP_MSG_TYPE_DATA;
	hdr[0].opcode_id = afe_param_msg_id[msg_num];
	hdr[0].version = AW_DSP_MSG_HDR_VER;
	hdr[1].type = AW_DSP_MSG_TYPE_CMD;
	hdr[1].opcode_id = inline_id;
	hdr[1].version = AW_DSP_MSG_HDR_VER;

	mutex_lock(&g_aw_dsp_msg_lock);
	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, 2 * sizeof(aw_dsp_msg_t));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, size, &size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

/*****************mtk dsp communication function end**********************/
#else
/*****************qcom dsp communication function start**********************/
static int aw_afe_get_topology(uint32_t param_id)
{
	if (param_id == AW_MSG_ID_TX_SET_ENABLE)
		return afe_get_topology(g_tx_port_id);
	else
		return afe_get_topology(g_rx_port_id);
}

static int aw_check_dsp_ready(uint32_t param_id)
{
	int ret;

	ret = aw_afe_get_topology(param_id);

	aw_pr_dbg("topo_id 0x%x ", ret);

	if (param_id == AW_MSG_ID_TX_SET_ENABLE) {
		if (ret != g_tx_topo_id)
			return false;
		else
			return true;
	} else {
		if (ret != g_rx_topo_id)
			return false;
		else
			return true;
	}
}

static int aw_qcom_write_data_to_dsp(uint32_t param_id, void *data, int size)
{
	int ret;
	int try = 0;

	mutex_lock(&g_aw_dsp_lock);
	while (try < AW_DSP_TRY_TIME) {
		if (aw_check_dsp_ready(param_id)) {
			ret = aw_send_afe_cal_apr(param_id, data, size, true);
			mutex_unlock(&g_aw_dsp_lock);
			return ret;
		} else {
			try++;
			usleep_range(AW_10000_US, AW_10000_US + 10);
			aw_pr_info("afe topo not ready try again");
		}
	}
	mutex_unlock(&g_aw_dsp_lock);

	return -EINVAL;
}

static int aw_qcom_read_data_from_dsp(uint32_t param_id, void *data, int size)
{
	int ret;
	int try = 0;

	mutex_lock(&g_aw_dsp_lock);
	while (try < AW_DSP_TRY_TIME) {
		if (aw_check_dsp_ready(param_id)) {
			ret = aw_send_afe_cal_apr(param_id, data, size, false);
			mutex_unlock(&g_aw_dsp_lock);
			return ret;
		} else {
			try++;
			usleep_range(AW_10000_US, AW_10000_US + 10);
			aw_pr_info("afe topo not ready try again");
		}
	}
	mutex_unlock(&g_aw_dsp_lock);

	return -EINVAL;
}

static int aw_qcom_write_msg_to_dsp(int msg_num, uint32_t msg_id, char *data_ptr, unsigned int size)
{
	int32_t *dsp_msg;
	int ret = 0;
	int msg_len = (int)(sizeof(aw_dsp_msg_t) + size);

	mutex_lock(&g_aw_dsp_msg_lock);
	dsp_msg = kzalloc(msg_len, GFP_KERNEL);
	if (!dsp_msg) {
		aw_pr_err("msg_id:0x%x kzalloc dsp_msg error",
			msg_id);
		ret = -ENOMEM;
		goto w_mem_err;
	}
	dsp_msg[0] = AW_DSP_MSG_TYPE_DATA;
	dsp_msg[1] = msg_id;
	dsp_msg[2] = AW_DSP_MSG_HDR_VER;

	memcpy(dsp_msg + (sizeof(aw_dsp_msg_t) / sizeof(int32_t)),
		data_ptr, size);

	ret = aw_qcom_write_data_to_dsp(afe_param_msg_id[msg_num],
			(void *)dsp_msg, msg_len);
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, write data to dsp failed", msg_id);
		kfree(dsp_msg);
		goto w_mem_err;
	}

	aw_pr_dbg("msg_id:0x%x, write data[%d] to dsp success", msg_id, msg_len);
	mutex_unlock(&g_aw_dsp_msg_lock);
	kfree(dsp_msg);
	return 0;
w_mem_err:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

static int aw_qcom_read_msg_from_dsp(int msg_num, uint32_t msg_id, char *data_ptr, unsigned int size)
{
	aw_dsp_msg_t cmd_msg;
	int ret;

	mutex_lock(&g_aw_dsp_msg_lock);
	cmd_msg.type = AW_DSP_MSG_TYPE_CMD;
	cmd_msg.opcode_id = msg_id;
	cmd_msg.version = AW_DSP_MSG_HDR_VER;

	ret = aw_qcom_write_data_to_dsp(afe_param_msg_id[msg_num],
			&cmd_msg, sizeof(aw_dsp_msg_t));
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, write cmd to dsp failed", msg_id);
		goto dsp_msg_failed;
	}

	ret = aw_qcom_read_data_from_dsp(afe_param_msg_id[msg_num],
			data_ptr, (int)size);
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, read data from dsp failed", msg_id);
		goto dsp_msg_failed;
	}

	aw_pr_dbg("msg_id:0x%x, read data[%d] from dsp success", msg_id, size);
	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;
dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

#endif

/******************* afe module communication function ************************/
static int aw_dsp_set_afe_rx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#else
	return aw_send_afe_rx_module_enable(buf, size);
#endif
}

static int aw_dsp_set_afe_tx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#else
	return aw_send_afe_tx_module_enable(buf, size);
#endif
}

static int aw_dsp_get_afe_rx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#else
	return aw_qcom_read_data_from_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#endif
}

static int aw_dsp_get_afe_tx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#else
	return aw_qcom_read_data_from_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#endif
}

/******************* read/write msg communication function ***********************/
static int aw_read_msg_from_dsp(int msg_num, uint32_t msg_id, char *data_ptr, unsigned int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_read_msg_from_dsp(msg_num, msg_id, data_ptr, size);
#else
	return aw_qcom_read_msg_from_dsp(msg_num, msg_id, data_ptr, size);
#endif
}

static int aw_write_msg_to_dsp(int msg_num, uint32_t msg_id, char *data_ptr, unsigned int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_msg_to_dsp(msg_num, msg_id, data_ptr, size);
#else
	return aw_qcom_write_msg_to_dsp(msg_num, msg_id, data_ptr, size);
#endif
}

/******************* read/write data communication function ***********************/
static int aw_read_data_from_dsp(uint32_t param_id, void *data, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_read_data_from_dsp(param_id, data, size);
#else
	return aw_qcom_read_data_from_dsp(param_id, data, size);
#endif
}

static int aw_write_data_to_dsp(uint32_t param_id, void *data, int size)
{
#ifdef AW_MTK_PLATFORM
	return aw_mtk_write_data_to_dsp(param_id, data, size);
#else
	return aw_qcom_write_data_to_dsp(param_id, data, size);
#endif
}

/************************* dsp communication function *****************************/
int aw_dsp_set_afe_module_en(int type, int enable)
{
	int ret;

	switch (type) {
	case AW_RX_MODULE:
		ret = aw_dsp_set_afe_rx_module_enable(&enable, sizeof(int32_t));
		break;
	case AW_TX_MODULE:
		ret = aw_dsp_set_afe_tx_module_enable(&enable, sizeof(int32_t));
		break;
	default:
		pr_err("%s: unsupported type %d\n", __func__, type);
		return -EINVAL;
	}

	return ret;
}

int aw_dsp_get_afe_module_en(int type, int *status)
{
	int ret;

	switch (type) {
	case AW_RX_MODULE:
		ret = aw_dsp_get_afe_rx_module_enable(status, sizeof(int32_t));
		break;
	case AW_TX_MODULE:
		ret = aw_dsp_get_afe_tx_module_enable(status, sizeof(int32_t));
		break;
	default:
		pr_err("%s: unsupported type %d\n", __func__, type);
		return -EINVAL;
	}

	return ret;
}

int aw_dsp_read_te(struct aw_device *aw_dev, int32_t *te)
{
	int ret;
	int msg_num;
	int32_t data[8]; /*[re:r0:Te:r0_te]*/

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	ret = aw_read_msg_from_dsp(msg_num, AW_MSG_ID_SPK_STATUS,
				(char *)data, sizeof(int32_t) * 8);
	if (ret) {
		aw_dev_err(aw_dev->dev, " read Te failed ");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0)
		*te = data[2];
	else
		*te = data[6];

	aw_dev_dbg(aw_dev->dev, "read Te %d", *te);
	return 0;
}

int aw_dsp_read_st(struct aw_device *aw_dev, int32_t *r0, int32_t *te)
{
	int ret;
	int msg_num;
	int32_t data[8]; /*[re:r0:Te:r0_te]*/

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	ret = aw_read_msg_from_dsp(msg_num, AW_MSG_ID_SPK_STATUS,
				(char *)data, sizeof(int32_t) * 8);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read spk st failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0) {
		*r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
		*te = data[2];
	} else {
		*r0 = AW_DSP_RE_TO_SHOW_RE(data[4]);
		*te = data[6];
	}
	aw_dev_dbg(aw_dev->dev, "read Re %d , Te %d", *r0, *te);
	return 0;
}

int aw_dsp_read_spin(int *spin_mode)
{
	int ret;
	int32_t spin = 0;

	ret = aw_read_data_from_dsp(AW_MSG_ID_SPIN, &spin, sizeof(int32_t));
	if (ret) {
		*spin_mode = 0;
		aw_pr_err("read spin failed ");
		return ret;
	}
	*spin_mode = spin;
	aw_pr_dbg("read spin done");
	return 0;
}

int aw_dsp_write_spin(int spin_mode)
{
	int ret;
	int32_t spin = spin_mode;

	if (spin >= AW_SPIN_MAX) {
		aw_pr_err("spin [%d] unsupported ", spin);
		return -EINVAL;
	}

	ret = aw_write_data_to_dsp(AW_MSG_ID_SPIN, &spin, sizeof(int32_t));
	if (ret) {
		aw_pr_err("write spin failed ");
		return ret;
	}
	aw_pr_dbg("write spin done");
	return 0;
}

int aw_dsp_read_r0(struct aw_device *aw_dev, int32_t *r0)
{
	uint32_t msg_id;
	int ret;
	int msg_num;
	int32_t data[6];

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_REAL_DATA_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_REAL_DATA_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, (char *)data, sizeof(int32_t) * 6);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read real re failed ");
		return ret;
	}

	*r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
	aw_dev_dbg(aw_dev->dev, "read r0 %d\n", *r0);
	return 0;
}

int aw_dsp_read_cali_data(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_REAL_DATA_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_REAL_DATA_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali dara failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read cali_data");
	return 0;
}

int aw_dsp_get_dc_status(struct aw_device *aw_dev)
{
	int ret;
	int msg_num;
	int32_t data[2];

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	ret = aw_read_msg_from_dsp(msg_num, AW_MSG_ID_DIRECT_CUR_FLAG, (char *)data, sizeof(int32_t) * 2);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read dc flag failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0)
		ret = data[0];
	else
		ret = data[1];

	aw_dev_dbg(aw_dev->dev, "read direct current status:%d", ret);
	return ret;
}

int aw_dsp_read_f0_q(struct aw_device *aw_dev, int32_t *f0, int32_t *q)
{
	int ret;
	int msg_num;
	int32_t data[4];

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	ret = aw_read_msg_from_dsp(msg_num, AW_MSG_ID_F0_Q, (char *)data, sizeof(int32_t) * 4);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read f0 & q failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0) {
		*f0 = data[0];
		*q  = data[1];
	} else {
		*f0 = data[2];
		*q  = data[3];
	}
	aw_dev_dbg(aw_dev->dev, "read f0 & q");
	return ret;
}

int aw_dsp_read_f0(struct aw_device *aw_dev, int32_t *f0)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_F0_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_F0_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, (char *)f0, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "read f0 failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read f0");
	return 0;
}

int aw_dsp_cali_en(struct aw_device *aw_dev, bool is_enable)
{
	int ret;
	int msg_num;
	int32_t enable = is_enable;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	ret = aw_write_msg_to_dsp(msg_num, AW_MSG_ID_ENABLE_CALI, (char *)&enable, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali en failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write cali_en[%d]", is_enable);
	return 0;
}

int aw_dsp_hmute_en(struct aw_device *aw_dev, bool is_hmute)
{
	int32_t hmute = is_hmute;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	ret = aw_write_msg_to_dsp(msg_num, AW_MSG_ID_ENABLE_HMUTE, (char *)&hmute, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write hmue failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write hmute[%d]", is_hmute);
	return 0;
}

int aw_dsp_read_cali_re(struct aw_device *aw_dev, int32_t *cali_re)
{
	uint32_t msg_id;
	int ret;
	int msg_num;
	int32_t read_re = 0;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_RE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_RE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, (char *)&read_re, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali re failed ");
		return ret;
	}
	*cali_re = AW_DSP_RE_TO_SHOW_RE(read_re);
	aw_dev_dbg(aw_dev->dev, "read cali re done");
	return 0;
}

int aw_dsp_write_cali_re(struct aw_device *aw_dev, int32_t cali_re)
{
	uint32_t msg_id;
	int ret;
	int msg_num;
	int32_t local_re = AW_SHOW_RE_TO_DSP_RE(cali_re);

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_RE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_RE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_write_msg_to_dsp(msg_num, msg_id, (char *)&local_re, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali re failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write cali re done");
	return 0;
}

int aw_dsp_write_params(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	if (msg_num == MSG_PARAM_ID_0)
		msg_id = AW_MSG_ID_PARAMS;
	else
		msg_id = AW_MSG_ID_PARAMS_1;

	ret = aw_write_data_to_dsp(msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write params failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write params done");
	return 0;
}

int aw_dsp_read_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_VMAX_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_VMAX_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read vmax failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read vmax done");
	return 0;
}

int aw_dsp_write_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_VMAX_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_VMAX_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_write_msg_to_dsp(msg_num, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write vmax failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write vmax done");
	return 0;
}

int aw_dsp_noise_en(struct aw_device *aw_dev, bool is_noise)
{
	int32_t noise = is_noise;
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_NOISE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_NOISE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_write_msg_to_dsp(msg_num, msg_id, (char *)&noise, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write noise failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write noise[%d] done", noise);
	return 0;
}

int aw_dsp_read_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_CALI_CFG_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_CALI_CFG_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_read_msg_from_dsp(msg_num, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali_cfg failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read cali_cfg done");
	return 0;
}

int aw_dsp_write_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id;
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed ");
		return ret;
	}

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L) {
		msg_id = AW_MSG_ID_CALI_CFG_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R) {
		msg_id = AW_MSG_ID_CALI_CFG_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_write_msg_to_dsp(msg_num, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali_cfg failed ");
		return ret;
	}

	aw_dev_dbg(aw_dev->dev, "write cali_cfg done");
	return 0;
}

int aw_dsp_read_msg(struct aw_device *aw_dev,
	uint32_t msg_id, char *data_ptr, unsigned int data_size)
{
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	return aw_read_msg_from_dsp(msg_num, msg_id, data_ptr, data_size);
}

int aw_dsp_write_msg(struct aw_device *aw_dev,
	uint32_t msg_id, char *data_ptr, unsigned int data_size)
{
	int ret;
	int msg_num;

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	return aw_write_msg_to_dsp(msg_num, msg_id, data_ptr, data_size);
}

int aw_dsp_set_copp_module_en(bool enable)
{
	int ret;

	ret = aw_adm_param_enable(g_rx_port_id, AW_COPP_MODULE_ID,
			AW_COPP_PARAMS_ID_AWDSP_ENABLE, enable);
	if (ret)
		return -EINVAL;

	aw_pr_info("set skt %s", enable == 1 ? "enable" : "disable");
	return 0;
}

void aw_device_parse_topo_id_dt(struct aw_device *aw_dev)
{
	int ret;

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-tx-topo-id", &g_tx_topo_id);
	if (ret < 0) {
		g_tx_topo_id = AW_TX_DEFAULT_TOPO_ID;
		aw_dev_info(aw_dev->dev, "read aw-tx-topo-id failed,use default");
	}

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-rx-topo-id", &g_rx_topo_id);
	if (ret < 0) {
		g_rx_topo_id = AW_RX_DEFAULT_TOPO_ID;
		aw_dev_info(aw_dev->dev, "read aw-rx-topo-id failed,use default");
	}

	aw_dev_info(aw_dev->dev, "tx-topo-id: 0x%x, rx-topo-id: 0x%x",
						g_tx_topo_id, g_rx_topo_id);
}

void aw_device_parse_port_id_dt(struct aw_device *aw_dev)
{
	int ret;

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-tx-port-id", &g_tx_port_id);
	if (ret < 0) {
		g_tx_port_id = AW_TX_DEFAULT_PORT_ID;
		aw_dev_info(aw_dev->dev, "read aw-tx-port-id failed,use default");
	}

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-rx-port-id", &g_rx_port_id);
	if (ret < 0) {
		g_rx_port_id = AW_RX_DEFAULT_PORT_ID;
		aw_dev_info(aw_dev->dev, "read aw-rx-port-id failed,use default");
	}

	aw_set_port_id(g_tx_port_id, g_rx_port_id);
	aw_dev_info(aw_dev->dev, "tx-port-id: 0x%x, rx-port-id: 0x%x",
						g_tx_port_id, g_rx_port_id);

}

