#define WM_ADSP_FW_SPK_PROT 9
#define WM_ADSP_FW_SPK_CALI 10

#define xDEBUG

#ifdef CONFIG_SWITCH_PROTECTION
/* Define the max safe gain to protect spk switch.
 * This value should match the handset gain value
 * set by HAL.
 * */
#define SPK_SWITCH_MAX_SAFE_GAIN (13)
#endif

enum drv_status {
	DRV_STATUS_NONE = 0,
	DRV_STATUS_PATH,
	DRV_STATUS_FW,
	DRV_STATUS_END,
};

static int cs35l41_calr_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private	*cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = cs35l41->calr;

	return 0;
}

static int cs35l41_calr_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 = snd_soc_component_get_drvdata(component);

	cs35l41->calr = ucontrol->value.enumerated.item[0];

	dev_info(cs35l41->dev, "calr = %d\n", cs35l41->calr);

	return 0;
}

static int cs35l41_ambient_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private	*cs35l41 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = cs35l41->ambient;

	return 0;
}

static int cs35l41_ambient_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private	*cs35l41 = snd_soc_component_get_drvdata(component);

	cs35l41->ambient = ucontrol->value.enumerated.item[0];

	dev_info(cs35l41->dev, "ambient = %d\n", cs35l41->ambient);

	return 0;
}

static int cs35l41_amp_active_status_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private *cs35l41 =
			snd_soc_component_get_drvdata(component);
	unsigned int val, ret;

	//regmap_test_bits(cs35l41->regmap, CS35L41_PWR_CTRL1, BIT(0));
	ret = regmap_read(cs35l41->regmap, CS35L41_PWR_CTRL1, &val);
	if (ret)
		return ret;
	ucontrol->value.integer.value[0] = ((val & BIT(0)) == BIT(0));

	return 0;
}

static int cs35l41_amp_active_status_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int cs35l41_spksw_gpio_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private	*cs35l41 = snd_soc_component_get_drvdata(component);

	int current_level = gpiod_get_value_cansleep(cs35l41->spksw_gpio);
	dev_info(cs35l41->dev, "spksw level cached = %d, actual = %d\n",
					cs35l41->spksw_level, current_level);

	if (current_level < 0) {
		// failed to read the actual value, fallback to cached value
		ucontrol->value.enumerated.item[0] = cs35l41->spksw_level;
	} else {
		ucontrol->value.enumerated.item[0] = current_level;
		cs35l41->spksw_level = current_level;
	}

	return 0;
}

static int cs35l41_spksw_gpio_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cs35l41_private	*cs35l41 = snd_soc_component_get_drvdata(component);

	cs35l41->spksw_level= ucontrol->value.enumerated.item[0];
	dev_info(cs35l41->dev, "setting spksw level = %d\n", cs35l41->spksw_level);
	gpiod_set_value_cansleep(cs35l41->spksw_gpio, !!cs35l41->spksw_level);

	return 0;
}

static int cs35l41_rw_registers_get(struct snd_kcontrol *kcontrol,
        unsigned int __user *data, unsigned int size)
{
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    struct cs35l41_private *cs35l41 = snd_soc_component_get_drvdata(component);
    int *buff = cs35l41->ctlbuf;
    int ret = 0;

    dev_dbg(cs35l41->dev, "Get: size = %d, %08x %08x %08x %08x %08x %08x %08x\n",
            size, buff[0], buff[1], buff[2], buff[3],
            buff[4], buff[5], buff[6]);

    if (size > REG_VALUE_SIZE)
        return -EINVAL;

    if (copy_to_user(data, buff, size))
        ret = -EFAULT;

    return ret;
}

static int cs35l41_rw_registers_put(struct snd_kcontrol *kcontrol,
        const unsigned int __user *data, unsigned int size)
{
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    struct cs35l41_private *cs35l41 = snd_soc_component_get_drvdata(component);
    int *buff, reg, val;
    int rw, num = 0;
    int i, ret = 0;

    if (size > REG_VALUE_SIZE)
        return -EINVAL;

    memset(cs35l41->ctlbuf, 0x00, REG_VALUE_SIZE);
    if (copy_from_user(cs35l41->ctlbuf, data, size)) {
        ret = -EFAULT;
        goto exit;
    }

    buff = cs35l41->ctlbuf;
    rw = be32_to_cpu(buff[2]) & 0xFF;
    num = (be32_to_cpu(buff[2]) >> 8) & 0xFF;
    dev_dbg(cs35l41->dev, "Put: size = %d, %08x %08x %08x %08x %08x %08x %08x\n",
            size, buff[0], buff[1], buff[2], buff[3],
            buff[4], buff[5], buff[6]);

    buff = &buff[3];
    if(rw == 0x80) { /* 0x80 stand for write */
        for(i = 0; i < num; i ++) {
            reg = be32_to_cpu(buff[i * 2]);
            val = be32_to_cpu(buff[i * 2 + 1]);
            ret = regmap_write(cs35l41->regmap, reg, val);
            dev_info(cs35l41->dev, "ctl write: %x <== %x, %s\n",
                    reg, val, ret==0 ? "ok":"failed");
        }
    }

    if(rw == 0x81) { /* 0x81 stand for read */
        for(i = 0; i < num; i ++) {
            reg = be32_to_cpu(buff[i * 2]);
            ret = regmap_read(cs35l41->regmap, reg, &val);
            dev_info(cs35l41->dev, "ctl read: %x ==> %x, ret = %d\n",
                    reg, val, ret);
            buff[i * 2 + 1] = ((ret==0) ? be32_to_cpu(val) : be32_to_cpu(0xBEDEAD));
        }
    }

exit:
    return 0;
}

static int cs35l41_spksw_gpio_init(struct cs35l41_private *cs35l41)
{
    int ret = 0;

	cs35l41->spksw_gpio = devm_gpiod_get_optional(cs35l41->dev, "spk-sw",
							GPIOD_OUT_LOW);
	if (IS_ERR(cs35l41->spksw_gpio)) {
		ret = PTR_ERR(cs35l41->spksw_gpio);
		cs35l41->spksw_gpio = NULL;
		dev_err(cs35l41->dev, "Failed to get spksw GPIO: %d\n", ret);
	}

	return ret;
}

static void cs35l41_ignore_suspend_widgets(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	char widget[32];

	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "AMP Playback");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "AMP Capture");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "Main AMP");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "SPK");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "VP");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "VBST");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "ISENSE");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "VSENSE");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "TEMP");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "DSP1 Preloader");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "DSP1 Preload");
	snd_soc_dapm_ignore_suspend(dapm, widget);
	snprintf(widget, sizeof(widget), "%s %s", component->name_prefix, "AMP Enable");
	snd_soc_dapm_ignore_suspend(dapm, widget);
}

#ifdef CONFIG_SWITCH_PROTECTION
static void cs35l41_spksw_safety_check(struct cs35l41_private *cs35l41)
{
	unsigned int val = 0;

	if (cs35l41->spksw_gpio != NULL) {
		regmap_read(cs35l41->regmap, CS35L41_AMP_GAIN_CTRL, &val);
		val = (val >> 5) & 0x1f; /* amp gain: bit[9:5] */
		if (val > SPK_SWITCH_MAX_SAFE_GAIN) {
			if (gpiod_get_raw_value(cs35l41->spksw_gpio) == 1) {
				dev_err(cs35l41->dev, "%s: AMP Gain: %d, switch GPIO is On.\n", __func__, val);
				gpiod_set_value_cansleep(cs35l41->spksw_gpio, 0);
			}
		}
	}
}
#endif

int wm_halo_apply_calibration(struct snd_soc_dapm_widget *w)
{
    int ret = 0;
    int dc_offset_hold_time = -1;
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l41_private *cs35l41 =
		snd_soc_component_get_drvdata(component);
	struct wm_adsp *dsp = &cs35l41->dsp;
	__be32 calr = cpu_to_be32(cs35l41->calr);
	/* status, must be 1 */
	__be32 status = cpu_to_be32(1);
	/* checksum = calr + status*/
	__be32 checksum = cpu_to_be32(1 + cs35l41->calr);
	__be32 ambient = cpu_to_be32(cs35l41->ambient);
	__be32 us_bypass = cpu_to_be32(1);
    __be32 max_lrclk_delay = cpu_to_be32(0x20);

	if (dsp->fw == WM_ADSP_FW_SPK_PROT) {
		ret = wm_adsp_write_ctl(dsp, "MAX_LRCLK_DELAY", WMFW_ADSP2_XM, 262308,
                                &max_lrclk_delay, sizeof(max_lrclk_delay));
		dev_info(dsp->dev, "Setting %s DSP1 Protection 400a4 MAX_LRCLK_DELAY to 0x%x, ret = %d\n",
				 dsp->component->name_prefix, be32_to_cpu(max_lrclk_delay), ret);
        // Read back to confirm
        max_lrclk_delay = 0;
		wm_adsp_read_ctl(dsp, "MAX_LRCLK_DELAY", WMFW_ADSP2_XM, 262308, 
                         &max_lrclk_delay, sizeof(max_lrclk_delay));
        dev_info(dsp->dev, "Read back %s DSP1 Protection 400a4 MAX_LRCLK_DELAY is 0x%x \n",
                 dsp->component->name_prefix, be32_to_cpu(max_lrclk_delay));

        wm_adsp_read_ctl(dsp, "DC_OFFSET_HOLD_TIME", WMFW_ADSP2_XM, 262308, 
                         &dc_offset_hold_time, sizeof(dc_offset_hold_time));
        dev_info(dsp->dev, "Read back %s DSP1 Protection 400a4 DC_OFFSET_HOLD_TIME is 0x%x \n",
                 dsp->component->name_prefix, be32_to_cpu(dc_offset_hold_time));

		if (dsp->component->name_prefix &&
				(!strcmp(dsp->component->name_prefix, "TL")
				|| !strcmp(dsp->component->name_prefix, "T"))) {
			ret = wm_adsp_write_ctl(dsp, "ENABLE_FULL_US_BYPASS",
					WMFW_ADSP2_XM, 262308, &us_bypass, sizeof(us_bypass));
			dev_info(dsp->dev, "Setting %s US_BYPASS to %d, ret = %d \n",
				dsp->component->name_prefix, be32_to_cpu(us_bypass), ret);

            // Read back to confirm
			us_bypass = 0;
			wm_adsp_read_ctl(dsp, "ENABLE_FULL_US_BYPASS", WMFW_ADSP2_XM, 262308, 
                             &us_bypass, sizeof(us_bypass));
            dev_info(dsp->dev, "Read back %s DSP1 Protection 400a4 BLE_FULL_US_BYPASS is %d \n",
                     dsp->component->name_prefix, be32_to_cpu(us_bypass));
		}

		dev_info(dsp->dev, "CAL_R <= %d\n", be32_to_cpu(calr));
        if (cs35l41->calr <= 0) {
            dev_err(dsp->dev, "Illegal cs35l41->calr value %d !\n", cs35l41->calr);
            return 0;
        }

		ret = wm_adsp_write_ctl(dsp, "CAL_R", WMFW_ADSP2_XM, 0xcd, &calr, sizeof(calr));
		wm_adsp_write_ctl(dsp, "CAL_CHECKSUM", WMFW_ADSP2_XM, 0xcd,
				&checksum, sizeof(checksum));
		wm_adsp_write_ctl(dsp, "CAL_STATUS", WMFW_ADSP2_XM, 0xcd,
				&status, sizeof(status));

		ret = wm_adsp_read_ctl(dsp, "CAL_R", WMFW_ADSP2_XM, 0xcd,
				&calr, sizeof(calr));
		dev_info(dsp->dev, "CAL_R => %d , ret = %d\n", be32_to_cpu(calr), ret);
		wm_adsp_read_ctl(dsp, "CAL_CHECKSUM", WMFW_ADSP2_XM, 0xcd,
				&checksum, sizeof(checksum));
		wm_adsp_read_ctl(dsp, "CAL_STATUS", WMFW_ADSP2_XM, 0xcd,
				&status, sizeof(status));

		dev_info(dsp->dev, "CAL_CHECKSUM => %d \n", be32_to_cpu(checksum));
		dev_info(dsp->dev, "CAL_STATUS => %d \n", be32_to_cpu(status));
	}

	if (dsp->fw == WM_ADSP_FW_SPK_CALI) {
		dev_info(dsp->dev, "AMBIENT <= %d\n", be32_to_cpu(ambient));
		wm_adsp_write_ctl(dsp, "CAL_AMBIENT", WMFW_ADSP2_XM, 0xcd,
				&ambient,sizeof(ambient));
	}

	return 0;
}
