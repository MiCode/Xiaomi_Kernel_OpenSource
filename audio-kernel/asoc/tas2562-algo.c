#include <dsp/smart_amp.h>
#include <dsp/q6afe-v2.h>

/*Master Control to Bypass the Smartamp TI CAPIv2 module*/
static int smartamp_bypass = TAS_FALSE;
static int smartamp_enable = TAS_FALSE;
static int calib_state = 0;
static int calib_re = 0;
static int spk_id = 0;
static struct mutex routing_lock;

int smartamp_get_val(int value)
{
	return ((value == TAS_TRUE)?TRUE:FALSE);
}

int smartamp_set_val(int value)
{
	return ((value == TRUE)?TAS_TRUE:TAS_FALSE);
}

int afe_smartamp_get_set(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	struct afe_smartamp_get_calib calib_resp;

	switch (get_set) {
		case TAS_SET_PARAM:
			if(smartamp_get_val(smartamp_bypass))
			{
				pr_info("[SmartAmp:%s] SmartAmp is bypassed no control set", __func__);
				goto fail_cmd;
			}
			ret = afe_smartamp_set_calib_data(param_id,
				(struct afe_smartamp_set_params_t *)user_data, length, module_id);
		break;
		case TAS_GET_PARAM:
			memset(&calib_resp, 0, sizeof(calib_resp));
			ret = afe_smartamp_get_calib_data(&calib_resp,
				param_id, module_id);
			memcpy(user_data, calib_resp.res_cfg.payload, length);
		break;
		default:
			goto fail_cmd;
	}
fail_cmd:
	return ret;
}

/*Wrapper arround set/get parameter, all set/get commands pass through this wrapper*/
int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	mutex_lock(&routing_lock);
	ret = afe_smartamp_get_set(user_data, param_id, get_set, length, module_id);
	mutex_unlock(&routing_lock);
	return ret;
}

static int tas2562_dummy_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = 0;
	pUcontrol->value.integer.value[0] = 0;
	pr_info("[SmartAmp:%s] write Only %d", __func__, user_data);
	return ret;
}

/*Control-1: Set Speaker ID*/
static int tas2562_set_spkid_left(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;

	spk_id = user_data;
	pr_info("[SmartAmp:%s] Setting Speaker ID %d", __func__, user_data);

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_SPKID, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to set Speaker ID", __func__);
		return -1;
	}

	return 0;
}

static int tas2562_get_spkid_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int user_data = 0;

	user_data = spk_id;
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting Speaker ID %d", __func__, user_data);

	return 0;
}

/*Control-2: Set Profile*/
static const char *profile_index_text[PROFILE_COUNT] = {"NONE","MUSIC","RING","VOICE","CALIB"};
static const struct soc_enum profile_index_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(profile_index_text), profile_index_text),
};

static int tas2562_set_profile(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;

	//To exclude None
	if((user_data >= 0) && (user_data < PROFILE_COUNT)) {
		pr_info("[SmartAmp:%s] Setting profile %s", __func__, profile_index_text[user_data]);
		if(user_data)
			user_data -= 1;
	} else {
		pr_err("[SmartAmp:%s] Invalid Value", __func__);
		return -1;
	}

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_PROFILE, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to set profile", __func__);
		return -1;
	}

	return 0;
}

static int tas2562_get_profile(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_PROFILE, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get profile", __func__);
			user_data = 0;
		}
		else
			user_data += 1;
	}
	pUcontrol->value.integer.value[0] = user_data;
	if((user_data >= 0) && (user_data < PROFILE_COUNT)) {
		pr_info("[SmartAmp:%s] getting profile %s", __func__, profile_index_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}
	return 0;
}

/*Control-3: Set Calibration Temperature*/
static int tas2562_set_tcal_left(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;

	calib_re = user_data;
	pr_info("[SmartAmp:%s] Setting Tcal %d", __func__, user_data);

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_TCAL, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to set Tcal", __func__);
		return -1;
	}

	return 0;
}

static int tas2562_get_tcal_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_TCAL, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get Tcal", __func__);
			return -1;
		}
	}
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting Tcal %d", __func__, user_data);

	return 0;
}

/*Control-4: Set Calibrated Rdc*/
static int tas2562_set_Re_left(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = pUcontrol->value.integer.value[0];
	int param_id = 0;

	calib_re = user_data;
	pr_info("[SmartAmp:%s] Setting Re %d", __func__, user_data);

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_SET_RE, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to set Re", __func__);
		return -1;
	}

	return 0;
}

/*Control-5: Calibration and Test(F0,Q,Tv) Controls*/
static const char *tas2562_calib_test_text[CALIB_COUNT] = {
	"NONE",
	"CALIB_START",
	"CALIB_STOP",
	"TEST_START",
	"TEST_STOP"
};

static const struct soc_enum tas2562_calib_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2562_calib_test_text), tas2562_calib_test_text),
};

static int tas2562_calib_test_set_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int param_id = 0;
	int user_data = pUcontrol->value.integer.value[0];
	int data = 1;

	calib_state = user_data;
	if((user_data >= 0) && (user_data < CALIB_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_calib_test_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid Value", __func__);
		return -1;
	}

	switch(user_data) {
		case CALIB_START:
			param_id = TAS_CALC_PARAM_IDX(TAS_SA_CALIB_INIT, 1, CHANNEL0);
		break;
		case CALIB_STOP:
			param_id = TAS_CALC_PARAM_IDX(TAS_SA_CALIB_DEINIT, 1, CHANNEL0);
		break;
		case TEST_START:
			pr_info("[SmartAmp:%s] Not Required!", __func__);
		break;
		case TEST_STOP:
			pr_info("[SmartAmp:%s] Not Required!", __func__);
		break;
		default:
			pr_info("[SmartAmp:%s] Normal", __func__);
		break;
	}

	if(param_id)
	{
		ret = afe_smartamp_algo_ctrl((u8 *)&data, param_id,
			TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to set calib/test", __func__);
			return -1;
		}
	}

	return ret;
}

static int tas2562_calib_test_get_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	pUcontrol->value.integer.value[0] = calib_state;
	pr_info("[SmartAmp:%s] case %s", __func__, tas2562_calib_test_text[calib_state]);
	return ret;
}

/*Control-6: Get Re*/
static int tas2562_get_re_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_RE, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get Re", __func__);
			return -1;
		}
	}

	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting Re %d", __func__, user_data);

	return 0;
}

/*Control-7: Get F0*/
static int tas2562_get_f0_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_F0, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get F0", __func__);
			return -1;
		}
	}
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting F0 %d", __func__, user_data);

	return 0;
}

/*Control-8: Get Q*/
static int tas2562_get_q_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_Q, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get q", __func__);
			return -1;
		}
	}
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting q %d", __func__, user_data);

	return 0;
}

/*Control-9: Get Tv*/
static int tas2562_get_tv_left(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;

	if(smartamp_get_val(smartamp_enable))
	{
		param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_TV, 1, CHANNEL0);
		ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
		if(ret < 0)
		{
			pr_err("[SmartAmp:%s] Failed to get Tv", __func__);
			return -1;
		}
	}
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("[SmartAmp:%s] Getting Tv %d", __func__, user_data);

	return 0;
}

/*Control-10: Smartamp Enable*/
static const char *tas2562_smartamp_enable_text[STATUS_COUNT] = {
	"DISABLE",
	"ENABLE"
};

static const struct soc_enum tas2562_smartamp_enable_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2562_smartamp_enable_text), tas2562_smartamp_enable_text),
};
static int tas2562_smartamp_enable_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int param_id = 0;
	int user_data = pUcontrol->value.integer.value[0];

	if((user_data >= 0) && (user_data < STATUS_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_smartamp_enable_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}
	smartamp_enable = smartamp_set_val(user_data);
	if(smartamp_get_val(smartamp_enable) == FALSE)
	{
		pr_info("[SmartAmp:%s] Disable called", __func__);
		calib_state = 0;
		calib_re = 0;
		return 0;
	}
	pr_info("[SmartAmp:%s] Setting the feedback module info for TAS", __func__);
	ret = afe_spk_prot_feed_back_cfg(TAS_TX_PORT,TAS_RX_PORT, 1, 0, 1);
	if(ret)
		pr_err("[SmartAmp:%s] FB Path Info failed ignoring ret = 0x%x", __func__, ret);

	pr_info("[SmartAmp:%s] Sending TX Enable", __func__);
	param_id = CAPI_V2_TAS_TX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_TX);
	if(ret)
	{
		pr_err("[SmartAmp:%s] TX Enable Failed ret = 0x%x", __func__, ret);
		goto fail_cmd;
	}
	
	pr_info("[SmartAmp:%s] Sending RX Enable", __func__);
	param_id = CAPI_V2_TAS_RX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret)
	{
		pr_err("[SmartAmp:%s] RX Enable Failed ret = 0x%x", __func__, ret);
		goto fail_cmd;
	}

	pr_info("[SmartAmp:%s] Sending RX Config", __func__);
	param_id = CAPI_V2_TAS_RX_CFG;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_SET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
		pr_err("[SmartAmp:%s] Failed to set config", __func__);
fail_cmd:
	return ret;
}

static int tas2562_smartamp_enable_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = smartamp_get_val(smartamp_enable);
	if((user_data >= 0) && (user_data < STATUS_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_smartamp_enable_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}
	pUcontrol->value.integer.value[0] = user_data;
	return ret;
}

/*Control-11: Smartamp Bypass */
static const char *tas2562_smartamp_bypass_text[STATUS_COUNT] = {
	"FALSE",
	"TRUE"
};

static const struct soc_enum tas2562_smartamp_bypass_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2562_smartamp_bypass_text), tas2562_smartamp_bypass_text),
};

static int tas2562_smartamp_bypass_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = pUcontrol->value.integer.value[0];
	if((user_data >= 0) && (user_data < STATUS_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_smartamp_bypass_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}
	if(smartamp_get_val(smartamp_enable))
	{
		pr_debug("[SmartAmp:%s] cannot update while smartamp enabled", __func__);
		return ret;
	}
	smartamp_bypass = smartamp_set_val(user_data);
	return ret;
}

static int tas2562_smartamp_bypass_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = smartamp_get_val(smartamp_bypass);
	pUcontrol->value.integer.value[0] = user_data;
	if((user_data >= 0) && (user_data < STATUS_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_smartamp_bypass_text[user_data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}
	return ret;
}

/*Control-12: Smartamp Status*/
static const char *tas2562_smartamp_status_text[STATUS_COUNT] = {
	"DISABLED",
	"ENABLED"
};

static const struct soc_enum tas2562_smartamp_status_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2562_smartamp_status_text), tas2562_smartamp_status_text),
};

static int tas2562_get_smartamp_status(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pUcontrol)
{
	int ret;
	int user_data = 0;
	int param_id = 0;
	int data = 1;

	param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_STATUS, 1, CHANNEL0);
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to get Status", __func__);
		user_data = 0;
	}
	else
		pr_info("[SmartAmp:%s] status = %d", __func__, user_data);
	data &= user_data;
	user_data = 0;
	param_id = CAPI_V2_TAS_RX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_RX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to get Rx Enable", __func__);
		user_data = 0;
	}else
		pr_info("[SmartAmp:%s] Rx Enable = %d", __func__, user_data);

	data &= user_data;
	user_data = 0;
	param_id = CAPI_V2_TAS_TX_ENABLE;
	ret = afe_smartamp_algo_ctrl((u8 *)&user_data, param_id,
		TAS_GET_PARAM, sizeof(uint32_t), AFE_SMARTAMP_MODULE_TX);
	if(ret < 0)
	{
		pr_err("[SmartAmp:%s] Failed to get Tx Enable", __func__);
		user_data = 0;
	}else
		pr_info("[SmartAmp:%s] Tx Enable = %d", __func__, user_data);

	data &= user_data;
	pUcontrol->value.integer.value[0] = data;

	if((data >= 0) && (data < STATUS_COUNT)) {
		pr_info("[SmartAmp:%s] case %s", __func__, tas2562_smartamp_status_text[data]);
	} else {
		pr_err("[SmartAmp:%s] Invalid value", __func__);
		return -1;
	}

	return 0;
}

static const struct snd_kcontrol_new msm_smartamp_tas2562_mixer_controls[] = {
	SOC_SINGLE_EXT("TAS2562_SET_SPKID_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	    tas2562_get_spkid_left, tas2562_set_spkid_left),
	SOC_ENUM_EXT("TAS2562_ALGO_PROFILE", profile_index_enum[0],
		tas2562_get_profile, tas2562_set_profile),
	SOC_SINGLE_EXT("TAS2562_SET_TCAL_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	    tas2562_get_tcal_left, tas2562_set_tcal_left),
	SOC_SINGLE_EXT("TAS2562_SET_RE_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	        tas2562_dummy_get, tas2562_set_Re_left),
	SOC_ENUM_EXT("TAS2562_ALGO_CALIB_TEST", tas2562_calib_test_enum[0],
		tas2562_calib_test_get_left, tas2562_calib_test_set_left),
	SOC_SINGLE_EXT("TAS2562_GET_RE_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	        tas2562_get_re_left, NULL),
	SOC_SINGLE_EXT("TAS2562_GET_F0_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	        tas2562_get_f0_left, NULL),
	SOC_SINGLE_EXT("TAS2562_GET_Q_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	        tas2562_get_q_left, NULL),
	SOC_SINGLE_EXT("TAS2562_GET_TV_LEFT", SND_SOC_NOPM, 0, 0x7fffffff, 0,
	        tas2562_get_tv_left, NULL),
	SOC_ENUM_EXT("TAS2562_SMARTPA_ENABLE", tas2562_smartamp_enable_enum[0],
	        tas2562_smartamp_enable_get, tas2562_smartamp_enable_set),
	SOC_ENUM_EXT("TAS2562_ALGO_BYPASS", tas2562_smartamp_bypass_enum[0],
		tas2562_smartamp_bypass_get, tas2562_smartamp_bypass_set),
	SOC_ENUM_EXT("TAS2562_ALGO_STATUS", tas2562_smartamp_status_enum[0],
		tas2562_get_smartamp_status, NULL),
};

void msm_smartamp_add_controls(struct snd_soc_platform *platform)
{
	mutex_init(&routing_lock);
	snd_soc_add_platform_controls(
			platform, msm_smartamp_tas2562_mixer_controls,
			ARRAY_SIZE(msm_smartamp_tas2562_mixer_controls));
}


