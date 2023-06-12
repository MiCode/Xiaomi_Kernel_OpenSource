/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-08-20 File created.
 */

#include "fsm_public.h"
#ifdef CONFIG_FSM_Q6AFE
#include "fsm_q6afe.h"
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

static struct fsm_resp_params fsm_resp = { 0 };
//static atomic_t fsm_afe_state;
struct rtac_cal_block_data g_fsm_rtac_block;
static int fsm_rx_port = FSM_RX_PORT;
static int fsm_tx_port = FSM_TX_PORT;
static int g_fsm_re25[FSM_DEV_MAX] = { 0 };
static bool read_re25_done = false;
struct mutex fsm_q6afe_lock;
#ifdef FSM_RUNIN_TEST
static atomic_t fsm_module_switch;
static atomic_t fsm_runin_test;

static int fsm_afe_test_ctrl(int index)
{
	struct fsm_afe afe;
	fsm_config_t *cfg;
	int runin_test;
	int ret;

	switch (index) {
		case FSM_TC_DISABLE_ALL:
			runin_test = 0; // disable all
			break;
		case FSM_TC_DISABLE_EQ:
			runin_test = 3; // disable eq only
			break;
		case FSM_TC_DISABLE_PROT:
			runin_test = 5; // disable protection only
			break;
		case FSM_TC_ENABLE_ALL:
		default:
			runin_test = 1; // enable all
			break;
	}
	cfg = fsm_get_config();
	pr_info("spkon:%d, testcase: %d", cfg->speaker_on, runin_test);
	if (!cfg->speaker_on) {
		atomic_set(&fsm_runin_test, index);
		return 0;
	}
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_MODULE_ENABLE;
	afe.op_set = true;
	ret = fsm_afe_send_apr(&afe, &runin_test, sizeof(runin_test));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	atomic_set(&fsm_runin_test, index);

	return ret;
}

static int fsm_afe_module_ctrl(int index)
{
	struct fsm_afe afe;
	int enable;
	int ret;

	afe.module_id = AFE_MODULE_ID_FSADSP_TX;
	afe.port_id = fsm_tx_port;
	afe.param_id  = CAPI_V2_PARAM_FSADSP_TX_ENABLE;
	afe.op_set = true;
	enable = !!index;
	ret = fsm_afe_send_apr(&afe, &enable, sizeof(enable));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	// fsm_delay_ms(50);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_rx_port;
	afe.param_id  = CAPI_V2_PARAM_FSADSP_RX_ENABLE;
	afe.op_set = true;
	enable = !!index;
	ret = fsm_afe_send_apr(&afe, &enable, sizeof(enable));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	atomic_set(&fsm_module_switch, index);
	ret = fsm_afe_test_ctrl(atomic_read(&fsm_runin_test));
	if (ret) {
		pr_err("test ctrl failed:%d", ret);
		return ret;
	}

	return ret;
}
#endif

int fsm_afe_get_rx_port(void)
{
	return fsm_rx_port;
}

int fsm_afe_get_tx_port(void)
{
	return fsm_tx_port;
}

int fsm_afe_callback_local(int opcode, void *payload, int size)
{
	uint32_t *payload32 = payload;
	uint8_t *buf8;
	int hdr_size;

	if (payload32[1] != AFE_MODULE_ID_FSADSP_RX
		&& payload32[1] != AFE_MODULE_ID_FSADSP_TX) {
		return -EINVAL;
	}
	pr_debug("opcode:%x, status:%d, size:%d", opcode, payload32[0], size);
	if (fsm_resp.params == NULL || fsm_resp.size == 0) {
		pr_err("invalid fsm resp data");
		return 0;
	}
	if (payload32[0] != 0) {
		pr_err("invalid status: %d", payload32[0]);
		return -EINVAL;
	}
	// payload structure:
	// status32
	// param header v1/v3
	// param data
	switch (opcode) {
	case AFE_PORT_CMDRSP_GET_PARAM_V2:
		hdr_size = sizeof(uint32_t) + sizeof(struct param_hdr_v1);
		buf8 = (uint8_t *)payload + hdr_size;
		if (size - hdr_size < fsm_resp.size) {
			fsm_resp.size = size - hdr_size;
		}
		memcpy(fsm_resp.params, buf8, fsm_resp.size);
		break;
#ifdef FSM_PARAM_HDR_V3
	case AFE_PORT_CMDRSP_GET_PARAM_V3:
		hdr_size = sizeof(uint32_t) + sizeof(struct param_hdr_v3);
		buf8 = (uint8_t *)payload + hdr_size;
		if (size - hdr_size < fsm_resp.size) {
			fsm_resp.size = size - hdr_size;
		}
		memcpy(fsm_resp.params, buf8, fsm_resp.size);
		break;
#endif
	default:
		pr_err("invalid opcode:%x", opcode);
		return 0;
	}

	return 0;
}

static int fsm_afe_send_inband(struct fsm_afe *afe, void *buf, uint32_t length)
{
	int ret;

	pr_debug("port:%x, module:%x, param:0x%x, length:%d",
			afe->port_id, afe->module_id, afe->param_id, length);

	mutex_lock(&fsm_q6afe_lock);
	memset(&afe->param_hdr, 0, sizeof(afe->param_hdr));
	afe->param_hdr.module_id = afe->module_id;
#ifdef FSM_PARAM_HDR_V3
	afe->param_hdr.instance_id = INSTANCE_ID_0;
#endif
	afe->param_hdr.param_id = afe->param_id;
	afe->param_hdr.param_size = length;
	if (!afe->op_set) {
		fsm_afe_set_callback(fsm_afe_callback_local);
		fsm_resp.params = kzalloc(length, GFP_KERNEL);
		if (!fsm_resp.params) {
			pr_err("%s: Memory allocation failed!\n", __func__);
			ret = -ENOMEM;
			goto exit;
		}
		fsm_resp.size = length;
	}
	ret = q6afe_send_fsm_pkt(afe, buf, length);
	if (ret) {
		pr_err("sent packet for port %d failed with code %d",
				afe->port_id, ret);
		goto exit;
	}
	pr_debug("sent packet with param id 0x%08x to module 0x%08x",
			afe->param_id, afe->module_id);
	if (!afe->op_set) {
		memcpy(buf, fsm_resp.params, fsm_resp.size);
		afe->param_size = fsm_resp.size;
	}
exit:
	if (fsm_resp.params) {
		kfree(fsm_resp.params);
		fsm_resp.params = NULL;
		fsm_resp.size = 0;
	}
	mutex_unlock(&fsm_q6afe_lock);

	return ret;
}

static int fsm_afe_map_rtac_block(struct rtac_cal_block_data *fsm_cal)
{
	size_t len;
	int ret;

	if (fsm_cal == NULL)
		return -EINVAL;
#ifdef FSM_PARAM_HDR_V3
	if (fsm_cal->map_data.dma_buf == NULL) {
		fsm_cal->map_data.map_size = SZ_4K;
		ret = msm_audio_ion_alloc(&(fsm_cal->map_data.dma_buf),
				fsm_cal->map_data.map_size,
				&(fsm_cal->cal_data.paddr), &len,
				&(fsm_cal->cal_data.kvaddr));
		if (ret < 0) {
			pr_err("%s: allocate buffer failed! ret = %d\n", __func__, ret);
			return ret;
		}
	}
#else
	if (fsm_cal->map_data.ion_handle == NULL) {
		fsm_cal->map_data.map_size = SZ_4K;
		ret = msm_audio_ion_alloc("fsm_cal", &(fsm_cal->map_data.ion_client),
				&(fsm_cal->map_data.ion_handle), fsm_cal->map_data.map_size,
				&(fsm_cal->cal_data.paddr), &len,
				&(fsm_cal->cal_data.kvaddr));
		if (ret < 0) {
			pr_err("%s: allocate buffer failed! ret = %d\n", __func__, ret);
			return ret;
		}
	}
#endif
	if (fsm_cal->map_data.map_handle == 0) {
		ret = afe_map_rtac_block(fsm_cal);
		if (ret < 0) {
			pr_err("%s: map buffer failed! ret = %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int fsm_afe_ummap_rtac_block(struct rtac_cal_block_data *fsm_cal)
{
	int ret;

	if (fsm_cal == NULL)
		return 0;
	ret = afe_unmap_rtac_block(&fsm_cal->map_data.map_handle);
	if (ret < 0) {
		pr_err("%s: unmap buffer failed! ret = %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int fsm_afe_send_apr(struct fsm_afe *afe, void *buf, uint32_t length)
{
	struct rtac_cal_block_data *cal_data;
	uint32_t *ptr_data;
	int ret;

	pr_info("port:%x, module:%x, param:0x%x, length:%d",
			afe->port_id, afe->module_id, afe->param_id, length);
	if (length < APR_CHUNK_SIZE) {
		return fsm_afe_send_inband(afe, buf, length);
	}
	mutex_lock(&fsm_q6afe_lock);
	afe->cal_block = &g_fsm_rtac_block;
	cal_data = afe->cal_block;
	ret = fsm_afe_map_rtac_block(afe->cal_block);
	if (ret) {
		pr_err("map rtac block failed:%d", ret);
		mutex_unlock(&fsm_q6afe_lock);
		return -ENOMEM;
	}
	memset(&afe->param_hdr, 0, sizeof(afe->param_hdr));
	memset(&fsm_resp, 0, sizeof(fsm_resp));
	afe->param_hdr.module_id = afe->module_id;
#ifdef FSM_PARAM_HDR_V3
	afe->param_hdr.instance_id = INSTANCE_ID_0;
#endif
	afe->param_hdr.param_id = afe->param_id;
	afe->param_hdr.param_size = length;
	memset(&afe->mem_hdr, 0, sizeof(struct mem_mapping_hdr));
	afe->mem_hdr.data_payload_addr_lsw =
			lower_32_bits(cal_data->cal_data.paddr);
	afe->mem_hdr.data_payload_addr_msw =
			msm_audio_populate_upper_32_bits(cal_data->cal_data.paddr);
	afe->mem_hdr.mem_map_handle = cal_data->map_data.map_handle;
	if (!afe->op_set) {
		fsm_afe_set_callback(fsm_afe_callback_local);
		fsm_resp.params = kzalloc(length, GFP_KERNEL);
		if (!fsm_resp.params) {
			pr_err("%s: Memory allocation failed!\n", __func__);
			goto exit;
		}
		fsm_resp.size = length;
	}
	ptr_data = (uint32_t *)buf;
	ret = q6afe_send_fsm_pkt(afe, buf, length);
	if (ret) {
		pr_err("sent packet for port %d failed with code %d",
				afe->port_id, ret);
		goto exit;
	}
	pr_debug("sent packet with param 0x%08x to module 0x%08x",
			afe->param_id, afe->module_id);
	if (!afe->op_set) {
		memcpy(buf, fsm_resp.params, fsm_resp.size);
		afe->param_size = fsm_resp.size;
	}
exit:
	if (fsm_resp.params) {
		kfree(fsm_resp.params);
		fsm_resp.params = NULL;
		fsm_resp.size = 0;
	}
	fsm_afe_ummap_rtac_block(afe->cal_block);
	mutex_unlock(&fsm_q6afe_lock);

	return ret;
}

int fsm_afe_read_re25(uint32_t *re25, int count)
{
#ifdef CONFIG_WT_QGKI
	struct file *fp;
	//mm_segment_t fs;
	loff_t pos = 0;
	int i, len, result;

	if (read_re25_done) {
		memcpy(re25, g_fsm_re25, sizeof(int) * count);
		return 0;
	}

	if (re25 == NULL || count <= 0) {
		pr_err("bad parameters");
		return -EINVAL;
	}
	fp = filp_open(FSM_CALIB_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("open %s fail!", FSM_CALIB_FILE);
		// set_fs(fs);
		return -EINVAL;
	}
	len = count * sizeof(int);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	result = kernel_read(fp, pos, (char *)re25, len);
#else
	result = kernel_read(fp, (char *)re25, len, &pos);
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	//result = vfs_read(fp, (char *)re25, len, &pos);
	//set_fs(fs);
#endif
	filp_close(fp, NULL);
	if (result <= 0) {
		pr_err("read read fail:%d", result);
		return -EINVAL;
	}
	for (i = 0; i < count; i++) {
		g_fsm_re25[i] = re25[i];
		pr_info("re25.%d: %d", i, re25[i]);
	}
	read_re25_done = true;
	pr_info("read %s success!", FSM_CALIB_FILE);
#endif
	return 0;
}

int fsm_afe_write_re25(uint32_t *re25, int count)
{
#ifdef CONFIG_WT_QGKI
	struct file *fp;
	//mm_segment_t fs;
	loff_t pos = 0;
	int i, len, result;

	if (re25 == NULL || count <= 0) {
		pr_err("bad parameters");
		return -EINVAL;
	}
	for (i = 0; i < count; i++) {
		pr_info("re25.%d: %d", i, re25[i]);
	}
	fp = filp_open(FSM_CALIB_FILE, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp)) {
		pr_err("open %s fail!", FSM_CALIB_FILE);
		// set_fs(fs);
		return PTR_ERR(fp);
	}
	len = count * sizeof(int);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	result = kernel_write(fp, (char *)re25, len, pos);
#else
	result = kernel_write(fp, (char *)re25, len, &pos);
	//fs = get_fs();
	//set_fs(KERNEL_DS);
	//result = vfs_write(fp, (char *)re25, len, &pos);
	//set_fs(fs);
#endif
	if (result != len) {
		pr_err("write file fail:%d, len:%d", result, len);
		filp_close(fp, NULL);
		return -ENOMEM;
	}
	filp_close(fp, NULL);
#endif
	// set_fs(fs);
	pr_info("write %s success!", FSM_CALIB_FILE);

	return 0;
}

int fsm_afe_save_re25(struct fsadsp_cmd_re25 *cmd_re25)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	uint32_t re25[FSM_DEV_MAX] = { 0 };
	struct preset_file *preset;
	fsm_dev_t *fsm_dev;
	struct fsm_afe afe;
	int dev, index;
	int ret = 0;

	fsm_reset_re25_data();
	preset = (struct preset_file *)fsm_get_presets();
	if (preset == NULL) {
		pr_err("not found firmware");
		return -EINVAL;
	}
	memset(cmd_re25, 0, sizeof(struct fsadsp_cmd_re25));
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	cmd_re25->ndev = preset->hdr.ndev;
	pr_info("ndev:%d", cmd_re25->ndev);
	for (dev = 0; dev < cmd_re25->ndev; dev++) {
		pr_info("in_for:%d", dev);
		fsm_dev = fsm_get_fsm_dev_by_id(dev);
		if (fsm_dev == NULL || fsm_skip_device(fsm_dev)) {
			continue;
		}
		if ((fsm_dev->pos_mask & 0x3) == 0) {
			index = 0;
			re25[index] = payload[0]; // left or mono
		} else {
			index = 1;
			re25[index] = payload[6]; // right
		}
		cmd_re25->cal_data[index].re25 = re25[index];
		cmd_re25->cal_data[index].channel = fsm_dev->pos_mask;
		pr_info("re25.%d: %d", index, re25[index]);
	}
	ret = fsm_afe_write_re25(re25, cmd_re25->ndev);
	if (ret) {
		pr_err("write re25 fail:%d", ret);
		return ret;
	}
	memcpy(g_fsm_re25, re25, sizeof(int) * cmd_re25->ndev);

	return ret;
}

int fsm_afe_mod_ctrl(bool enable)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsadsp_cmd_re25 *params;
	uint32_t re25[FSM_DEV_MAX] = { 0 };
	struct preset_file *preset;
	struct fsm_afe afe;
	fsm_dev_t *fsm_dev;
	int param_size;
	int index;
	int dev;
	int ret;

	if (!enable || cfg->dev_count == 0) {
		return 0;
	}
	param_size = sizeof(struct fsadsp_cmd_re25);
	// + cfg->dev_count * sizeof(struct fsadsp_cal_data);
	params = (struct fsadsp_cmd_re25 *)fsm_alloc_mem(param_size);
	if (params == NULL) {
		pr_err("allocate memory failed");
		return -EINVAL;
	}
	memset(params, 0, param_size);
	params->version = FSADSP_RE25_CMD_VERSION_V1;
	preset = (struct preset_file *)fsm_get_presets();
	if (preset == NULL) {
		pr_err("not found firmware");
		return -EINVAL;
	}
	params->ndev = preset->hdr.ndev;
	ret = fsm_afe_read_re25(&re25[0], params->ndev);
	if (ret) {
		pr_err("read back re25 fail:%d", ret);
	}
	for (dev = 0; dev < cfg->dev_count; dev++) {
		fsm_dev = fsm_get_fsm_dev_by_id(dev);
		if (fsm_dev == NULL || fsm_skip_device(fsm_dev))
			continue;
		if ((fsm_dev->pos_mask & 0x3) != 0) { // right
			index = 1;
		} else { // left, mono
			index = 0;
		}
		params->cal_data[index].rstrim = LOW8(fsm_dev->rstrim);
		params->cal_data[index].channel = fsm_dev->pos_mask;
		params->cal_data[index].re25 = re25[index];
		pr_info("re25.%d[%X]:%d", index, fsm_dev->pos_mask,
				re25[index]);
	}
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_RE25;
	afe.op_set = true;
	ret = fsm_afe_send_apr(&afe, params, param_size);
	fsm_free_mem((void **)&params);
	if (ret) {
		pr_err("send re25 failed:%d", ret);
		return ret;
	}
#ifdef FSM_RUNIN_TEST
	ret = fsm_afe_test_ctrl(atomic_read(&fsm_runin_test));
	if (ret) {
		pr_err("test ctrl failed:%d", ret);
		return ret;
	}
#endif

	return ret;
}

void fsm_reset_re25_data(void)
{
	memset(g_fsm_re25, 0, sizeof(g_fsm_re25));
	read_re25_done = false;
}

int fsm_set_re25_data(struct fsm_re25_data *data)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret = 0;
	int dev;

	if (cfg == NULL || data == NULL) {
		return -EINVAL;
	}
	if (data->count <= 0) {
		pr_err("invalid dev count");
		return -EINVAL;
	}
	for (dev = 0; dev < data->count; dev++) {
		g_fsm_re25[dev] = data->re25[dev];
	}
	if (cfg->speaker_on) {
		ret = fsm_afe_mod_ctrl(true);
		if (ret) {
			pr_err("update re25 failed:%d", ret);
		}
	}

	return ret;
}

int fsm_afe_get_livedata(void *ldata, int size)
{
	struct fsm_afe afe;
	int ret;

	if (ldata == NULL) {
		return -EINVAL;
	}
	memset(g_fsm_re25, 0 , sizeof(g_fsm_re25));
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_LIVEDATA;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, ldata, size);
	if (ret) {
		pr_err("send apr failed:%d", ret);
	}
	return ret;
}

int fsm_afe_send_preset(char *preset)
{
	const struct firmware *firmware;
	struct device *dev;
	struct fsm_afe afe;
	int ret;

	if ((dev = fsm_get_pdev()) == NULL) {
		pr_err("bad dev parameter");
		return -EINVAL;
	}
	ret = request_firmware(&firmware, preset, dev);
	if (ret) {
		pr_err("request firmware failed");
		return ret;
	}
	if (firmware->data == NULL && firmware->size <= 0) {
		pr_err("can't read firmware");
		return -EINVAL;
	}
	pr_debug("sending rx %s", preset);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_MODULE_PARAM;
	afe.op_set = true;
	ret = fsm_afe_send_apr(&afe, (void *)firmware->data,
			(uint32_t)firmware->size);
	if (ret) {
		pr_err("send apr failed:%d", ret);
	}
	release_firmware(firmware);

	return ret;
}

static int fsm_afe_rx_port_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int rx_port = ucontrol->value.integer.value[0];

	if ((rx_port < 0) || (rx_port > AFE_PORT_ID_INVALID)) {
		pr_err("out of range (%d)", rx_port);
		return 0;
	}

	pr_debug("change from %d to %d", fsm_rx_port, rx_port);
	fsm_rx_port = rx_port;

	return 0;
}

static int fsm_afe_rx_port_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = fsm_rx_port;
	pr_debug("get rx port:%d", fsm_rx_port);

	return 0;
}

static int fsm_afe_tx_port_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int tx_port = ucontrol->value.integer.value[0];

	if ((tx_port < 0) || (tx_port > AFE_PORT_ID_INVALID)) {
		pr_err("out of range (%d)", tx_port);
		return 0;
	}

	pr_debug("change from %d to %d", fsm_tx_port, tx_port);
	fsm_tx_port = tx_port;

	return 0;
}

static int fsm_afe_tx_port_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = fsm_tx_port;
	pr_debug("get tx port:%d", fsm_tx_port);

	return 0;
}

static const char *fsm_afe_switch_text[] = {
	"Off", "On"
};

static const struct soc_enum fsm_afe_switch_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsm_afe_switch_text),
			fsm_afe_switch_text)
};

#ifdef FSM_RUNIN_TEST
static int fsm_afe_module_switch_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int index = atomic_read(&fsm_module_switch);

	pUcontrol->value.integer.value[0] = index;
	pr_info("Switch %s", fsm_afe_switch_text[index]);

	return 0;
}

static int fsm_afe_module_switch_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int ret;

	pr_info("Switch %s", fsm_afe_switch_text[index]);
	ret = fsm_afe_module_ctrl(index);
	if (ret) {
		pr_err("module ctrl failed:%d", ret);
		return ret;
	}

	return 0;
}

static int fsm_afe_runin_test_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int index = atomic_read(&fsm_runin_test);

	pUcontrol->value.integer.value[0] = index;
	pr_info("case: %d", index);

	return 0;
}

static int fsm_afe_runin_test_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int ret;

	pr_info("case: %d", index);
	ret = fsm_afe_test_ctrl(index);
	if (ret) {
		pr_err("test ctrl failed:%d", ret);
		return ret;
	}

	return 0;
}
#endif

static const struct snd_kcontrol_new fsm_afe_controls[] = {
	SOC_SINGLE_EXT("FSM_RX_Port", SND_SOC_NOPM, 0, AFE_PORT_ID_INVALID, 0,
			fsm_afe_rx_port_get, fsm_afe_rx_port_set),
	SOC_SINGLE_EXT("FSM_TX_Port", SND_SOC_NOPM, 0, AFE_PORT_ID_INVALID, 0,
			fsm_afe_tx_port_get, fsm_afe_tx_port_set),
#ifdef FSM_RUNIN_TEST
	SOC_ENUM_EXT("FSM_Module_Switch", fsm_afe_switch_enum[0],
			fsm_afe_module_switch_get, fsm_afe_module_switch_set),
	SOC_SINGLE_EXT("FSM_Runin_Test", SND_SOC_NOPM, 0, FSM_TC_MAX, 0,
			fsm_afe_runin_test_get, fsm_afe_runin_test_set),
#endif
};

void fsm_afe_init_controls(struct snd_soc_codec *codec)
{
	mutex_init(&fsm_q6afe_lock);
	memset(&g_fsm_rtac_block, 0, sizeof(struct rtac_cal_block_data));
#ifdef FSM_RUNIN_TEST
	atomic_set(&fsm_module_switch, 0);
	atomic_set(&fsm_runin_test, 0);
#endif
	snd_soc_add_codec_controls(codec, fsm_afe_controls,
			ARRAY_SIZE(fsm_afe_controls));
}
#else
#define fsm_afe_mod_ctrl(...)
#define fsm_afe_init_controls(...)
#endif
