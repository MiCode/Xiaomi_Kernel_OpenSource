#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/jiffies.h>
#include "haptic.h"
//#include "aw8624.h"
#include "aw8622x.h"
#include <linux/vmalloc.h>
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AWINIC_DRIVER_VERSION	("v0.1.0")
#define AWINIC_I2C_NAME		("awinic_haptic")
#define AWINIC_HAPTIC_NAME      ("awinic_haptic")
#define AW_READ_CHIPID_RETRIES	(5)
#define AW_I2C_RETRIES		(2)
#define AW8624_CHIP_ID		(0x24)
#define AW8622X_CHIP_ID		(0x00)
#define AW_REG_ID		(0x00)
#define AW8622X_REG_EFRD9	(0x64)


#if 1
const char * const pctl_names[] = {
	"aw86224_reset_reset",
	"aw86224_reset_active",
	"aw86224_interrupt_active",
};
#endif

static int awinic_i2c_read(struct awinic *awinic,
		unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(awinic->i2c, reg_addr);
		if (ret < 0) {
			aw_dev_err(awinic->dev, "%s: i2c_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

static int awinic_i2c_write(struct awinic *awinic,
		 unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		i2c_smbus_write_byte_data(awinic->i2c, reg_addr, reg_data);
		if (ret < 0) {
			aw_dev_err(awinic->dev, "%s: i2c_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

#ifdef ENABLE_PIN_CONTROL
static int select_pin_ctl(struct aw8622x *aw8622x, const char *name)
{
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(aw8622x->pinctrl_state); i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(aw8622x->aw8622x_pinctrl,
						  aw8622x->pinctrl_state[i]);
			if (rc)
				pr_info("%s: cannot select '%s'\n", __func__, name);
			else
				pr_info("%s: Selected '%s'\n", __func__, name);
			goto exit;
		}
	}

	rc = -EINVAL;
	pr_info("%s: '%s' not found\n", __func__, name);

exit:
	return rc;
}

static int aw8622x_set_interrupt(struct aw8622x *aw8622x)
{
	int rc = select_pin_ctl(aw8622x, "aw86224_interrupt_active");
	return rc;
}
#endif

static int awinic_hw_reset(struct awinic *awinic)
{
#ifdef ENABLE_PIN_CONTROL
	int rc  = 0;
#endif
	aw_dev_info(awinic->dev, "%s enter\n", __func__);
#ifdef ENABLE_PIN_CONTROL
    rc = select_pin_ctl(awinic->aw8622x, "aw86224_reset_active");
	msleep(5);
	rc = select_pin_ctl(awinic->aw8622x, "aw86224_reset_reset");
	msleep(5);
	rc = select_pin_ctl(awinic->aw8622x, "aw86224_reset_active");
#endif
	if (awinic && gpio_is_valid(awinic->reset_gpio)) {
		gpio_set_value_cansleep(awinic->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(awinic->reset_gpio, 1);
		usleep_range(3500, 4000);
	} else {
		dev_err(awinic->dev, "%s: failed\n", __func__);
	}
	return 0;
}

static int awinic_haptic_softreset(struct awinic *awinic)
{
	aw_dev_info(awinic->dev, "%s enter\n", __func__);
	awinic_i2c_write(awinic, AW_REG_ID, 0xAA);
	usleep_range(2000, 2500);
	return 0;
}
static int awinic_read_chipid(struct awinic *awinic)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg = 0;
	unsigned char ef_id = 0xff;


	while (cnt < AW_READ_CHIPID_RETRIES) {
		/* hardware reset */
		awinic_hw_reset(awinic);

		ret = awinic_i2c_read(awinic, AW_REG_ID, &reg);
		if (ret < 0) {
			aw_dev_err(awinic->dev,
				"%s: failed to read register AW_REG_ID: %d\n",
				__func__, ret);
		}

		switch (reg) {
		case AW8624_CHIP_ID:
			aw_dev_info(awinic->dev,
				"%s aw8624 detected\n", __func__);
			awinic->name = AW8624;
			awinic_haptic_softreset(awinic);
			return 0;
		case AW8622X_CHIP_ID:
			/* Distinguish products by AW8622X_REG_EFRD9. */
			awinic_i2c_read(awinic, AW8622X_REG_EFRD9, &ef_id);
			if ((ef_id & 0x41) == AW86224_5_EF_ID) {
				awinic->name = AW86224_5;
				aw_dev_info(awinic->dev,
					"%s aw86224_5 detected\n", __func__);
				awinic_haptic_softreset(awinic);
				return 0;
			} else if ((ef_id & 0x41) == AW86223_EF_ID) {
				awinic->name = AW86223;
				aw_dev_info(awinic->dev,
					"%s aw86223 detected\n", __func__);
				awinic_haptic_softreset(awinic);
				return 0;
			} else {
				aw_dev_info(awinic->dev,
					"%s unsupported ef_id = (0x%02X)\n",
					__func__, ef_id);
				break;
			}
		default:
			aw_dev_info(awinic->dev,
				"%s unsupported device revision (0x%x)\n",
			__func__, reg);
			break;
		}
		cnt++;

		usleep_range(2000, 3000);
	}

	return -EINVAL;
}
static int awinic_parse_dt(struct device *dev, struct awinic *awinic,
		struct device_node *np) {
	unsigned int val = 0;

	awinic->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (awinic->reset_gpio >= 0) {
		aw_dev_err(awinic->dev,
			"%s: reset gpio provided ok\n", __func__);
	} else {
		awinic->reset_gpio = -1;
		aw_dev_err(awinic->dev,
			"%s: no reset gpio provided, will not HW reset device\n",
			__func__);
		return -1;
	}

	awinic->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (awinic->irq_gpio < 0) {
		dev_err(dev, "%s: no irq gpio provided.\n", __func__);
		awinic->IsUsedIRQ = false;
	} else {
		aw_dev_err(awinic->dev,
			"%s: irq gpio provided ok.\n", __func__);
		awinic->IsUsedIRQ = true;
	}

	val = of_property_read_u32(np,
			"aw8622x_i2c_addr", &awinic->aw8622x_i2c_addr);
	if (val)
		aw_dev_err(awinic->dev,
			"%s:configure aw8622x_i2c_addr error\n", __func__);
	else
		aw_dev_info(awinic->dev,
			"%s: configure aw8622x_i2c_addr ok\n", __func__);
	return 0;
}


static int aw8622x_haptics_upload_effect (struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
       struct aw8622x *aw8622x = input_get_drvdata(dev);
       struct qti_hap_play_info *play = &aw8622x->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

       pr_err("%s: enter\n", __func__);

       if (aw8622x->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw8622x->timer)) {
		rem = hrtimer_get_remaining(&aw8622x->timer);
		time_us = ktime_to_us(rem);
		printk("waiting for playing clear sequence: %lld us\n",
		       time_us);
		usleep_range(time_us, time_us + 100);
	}

	pr_info("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw8622x->effect_type = effect->type;
	mutex_lock(&aw8622x->lock);
	while (atomic_read(&aw8622x->exit_in_rtp_loop)) {
		pr_info("%s:  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw8622x->lock);
		ret =
		    wait_event_interruptible(aw8622x->stop_wait_q,
					     atomic_read(&aw8622x->
							 exit_in_rtp_loop) ==
					     0);
		pr_info("%s:  wakeup \n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw8622x->lock);
			pr_err("%s: wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&aw8622x->lock);
	}
	if (aw8622x->effect_type == FF_CONSTANT) {
		pr_debug("%s:  effect_type is  FF_CONSTANT! \n", __func__);
		/*cont mode set duration */
		aw8622x->duration = effect->replay.length;
		aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw8622x->effect_id = aw8622x->dts_info.effect_id_boundary;

	} else if (aw8622x->effect_type == FF_PERIODIC) {
		if (aw8622x->effects_count == 0) {
			mutex_unlock(&aw8622x->lock);
			return -EINVAL;
		}

		pr_debug("%s:  effect_type is  FF_PERIODIC! \n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8622x->lock);
			return -EFAULT;
		}

		aw8622x->effect_id = data[0];
		pr_debug("%s: aw8622x->effect_id =%d \n", __func__,
			 aw8622x->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude;	/*vmax level */
		//if (aw8624->info.gain_flag == 1)
		//      play->vmax_mv = AW8624_LIGHT_MAGNITUDE;
		//printk("%s  %d  aw8624->play.vmax_mv = 0x%x\n", __func__, __LINE__, aw8624->play.vmax_mv);

		if (aw8622x->effect_id < 0 ||
		    aw8622x->effect_id > aw8622x->dts_info.effect_max) {
			mutex_unlock(&aw8622x->lock);
			return 0;
		}

		if (aw8622x->effect_id < aw8622x->dts_info.effect_id_boundary) {
			aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RAM_MODE;
			pr_info
			    ("%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
			     __func__, aw8622x->effect_id,
			     aw8622x->activate_mode);
			data[1] = aw8622x->predefined[aw8622x->effect_id].play_rate_us / 1000000;	/*second data */
			data[2] = aw8622x->predefined[aw8622x->effect_id].play_rate_us / 1000;	/*millisecond data */
			pr_debug
			    ("%s: aw8622x->predefined[aw8622x->effect_id].play_rate_us/1000 = %d\n",
			     __func__,
			     aw8622x->predefined[aw8622x->effect_id].
			     play_rate_us / 1000);
		}
		if (aw8622x->effect_id >= aw8622x->dts_info.effect_id_boundary) {
			aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RTP_MODE;
			pr_info
			    ("%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
			     __func__, aw8622x->effect_id,
			     aw8622x->activate_mode);
			data[1] = aw8622x->dts_info.rtp_time[aw8622x->effect_id] / 1000;	/*second data */
			data[2] = aw8622x->dts_info.rtp_time[aw8622x->effect_id];	/*millisecond data */
			pr_debug("%s: data[1] = %d data[2] = %d\n", __func__,
				 data[1], data[2]);
		}

		if (copy_to_user(effect->u.periodic.custom_data, data,
				 sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8622x->lock);
			return -EFAULT;
		}

	} else {
		pr_err("%s: Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw8622x->lock);
	pr_debug("%s	%d	aw8622x->effect_type= 0x%x\n", __func__,
		 __LINE__, aw8622x->effect_type);
	return 0;
}

static int aw8622x_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	int rc = 0;
	pr_debug("%s:  %d enter\n", __func__, __LINE__);

	pr_info("%s: effect_id=%d , val = %d\n", __func__, effect_id, val);
	pr_info("%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
		__func__, aw8622x->effect_id, aw8622x->activate_mode);

	/*for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw8622x->state = 1;
	if (val <= 0)
		aw8622x->state = 0;
	hrtimer_cancel(&aw8622x->timer);

	if (aw8622x->effect_type == FF_CONSTANT &&
	    aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		pr_info("%s: enter ram_loop_mode \n", __func__);
		//schedule_work(&aw8624->long_vibrate_work);
		queue_work(aw8622x->work_queue, &aw8622x->long_vibrate_work);
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RAM_MODE) {
		pr_info("%s: enter  ram_mode\n", __func__);
		//schedule_work(&aw8624->long_vibrate_work)
		queue_work(aw8622x->work_queue, &aw8622x->long_vibrate_work);;
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RTP_MODE) {
		pr_info("%s: enter  rtp_mode\n", __func__);
		//schedule_work(&aw8624->rtp_work);
		queue_work(aw8622x->work_queue, &aw8622x->rtp_work);
		//if we are in the play mode, force to exit
		if (val == 0) {
			atomic_set(&aw8622x->exit_in_rtp_loop, 1);
		}
	} else {
		/*other mode */
	}

	return rc;
}

static int aw8622x_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	aw8622x->effect_type = 0;
	aw8622x->duration = 0;
	return rc;
}

static void aw8622x_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw8622x *aw8622x =
	    container_of(work, struct aw8622x, set_gain_work);

	if (aw8622x->new_gain >= 0x7FFF)
		aw8622x->level = 0x80;	/*128 */
	else if (aw8622x->new_gain <= 0x3FFF)
		aw8622x->level = 0x1E;	/*30 */
	else
		aw8622x->level = (aw8622x->new_gain - 16383) / 128;

	if (aw8622x->level < 0x1E)
		aw8622x->level = 0x1E;	/*30 */
	pr_info("%s: set_gain queue work, new_gain = %x level = %x \n", __func__,
		aw8622x->new_gain, aw8622x->level);

	if (aw8622x->ram_vbat_comp == AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE
		&& aw8622x->vbat)
	{
		pr_debug("%s: ref %d vbat %d ", __func__, AW8622X_VBAT_REFER,
				aw8622x->vbat);
		comp_level = aw8622x->level * AW8622X_VBAT_REFER / aw8622x->vbat;
		if (comp_level > (128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN)) {
			comp_level = 128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN;
			pr_debug("%s: comp level limit is %d ", __func__, comp_level);
		}
		pr_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   aw8622x->level, comp_level);
		//aw8624_i2c_write(aw8622x, AW8624_REG_DATDBG, comp_level);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
				__func__, aw8622x->vbat, AW8622X_VBAT_MIN, AW8622X_VBAT_REFER);
		//aw8622x_i2c_write(aw8622x, AW8624_REG_DATDBG, aw8624->level);
	}
}

static void aw8622x_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	pr_debug("%s: enter\n", __func__);
	aw8622x->new_gain = gain;
	queue_work(aw8622x->work_queue, &aw8622x->set_gain_work);
}





static int
awinic_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct awinic *awinic;
        struct input_dev *input_dev;
	struct device_node *np = i2c->dev.of_node;
        struct ff_device *ff;
        int rc = 0, effect_count_max;
	int ret = -1;
	int irq_flags = 0;
#ifdef ENABLE_PIN_CONTROL
	int i;
#endif

	aw_dev_err(&i2c->dev, "%s enter\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	awinic = devm_kzalloc(&i2c->dev, sizeof(struct awinic), GFP_KERNEL);
	if (awinic == NULL)
		return -ENOMEM;

	awinic->aw8622x = devm_kzalloc(&i2c->dev, sizeof(struct aw8622x), GFP_KERNEL);
	if (awinic->aw8622x == NULL)
		return -ENOMEM;
        
	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;

	awinic->dev = &i2c->dev;
	awinic->i2c = i2c;

	i2c_set_clientdata(i2c, awinic);
	/* aw862xx rst & int */
	if (np) {
		ret = awinic_parse_dt(&i2c->dev, awinic, np);
		if (ret) {
			aw_dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	}

	awinic->aw8622x->enable_pin_control = 0;
#ifdef ENABLE_PIN_CONTROL
	awinic->aw8622x->aw8622x_pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(awinic->aw8622x->aw8622x_pinctrl )) {
		if (PTR_ERR(awinic->aw8622x->aw8622x_pinctrl ) == -EPROBE_DEFER) {
			printk("pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			return rc;
		}
		printk("Target does not use pinctrl\n");
		awinic->aw8622x->aw8622x_pinctrl  = NULL;
		rc = -EINVAL;
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(awinic->aw8622x->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
		    pinctrl_lookup_state(awinic->aw8622x->aw8622x_pinctrl, n);
		if (IS_ERR(state)) {
			printk("cannot find '%s'\n", n);
			rc = -EINVAL;
			//goto exit;
		}
		pr_info("%s: found pin control %s\n", __func__, n);
		awinic->aw8622x->pinctrl_state[i] = state;
		awinic->aw8622x->enable_pin_control = 1;
		aw8622x_set_interrupt(awinic->aw8622x);
	}
#endif
if (!awinic->aw8622x->enable_pin_control) {
	if (gpio_is_valid(awinic->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, awinic->reset_gpio,
			GPIOF_OUT_INIT_LOW, "awinic_rst");
		if (ret) {
			aw_dev_err(&i2c->dev,
				"%s: rst request failed\n", __func__);
			goto err_reset_gpio_request;
		}
	}
}
	if (gpio_is_valid(awinic->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, awinic->irq_gpio,
			GPIOF_DIR_IN, "awinic_int");
		if (ret) {
			aw_dev_err(&i2c->dev,
				"%s: int request failed\n", __func__);
			goto err_irq_gpio_request;
		}
	}
	/* read chip id */
	ret = awinic_read_chipid(awinic);
	if (ret < 0) {
		i2c->addr = (u16)awinic->aw8622x_i2c_addr;
		aw_dev_err(&i2c->dev, "%s awinic->aw8622x_i2c_addr=%d\n",
			   __func__, awinic->aw8622x_i2c_addr);
		ret = awinic_read_chipid(awinic);
		if (ret < 0) {
			aw_dev_err(&i2c->dev,
			"%s: awinic_read_chipid failed ret=%d\n",
			__func__, ret);
			goto err_id;
		}
	}

	/* aw8622x */
	if (awinic->name == AW86223 || awinic->name == AW86224_5) {
		awinic->aw8622x->dev = awinic->dev;
		awinic->aw8622x->i2c = awinic->i2c;
		awinic->aw8622x->reset_gpio = awinic->reset_gpio;
		awinic->aw8622x->irq_gpio = awinic->irq_gpio;
		awinic->aw8622x->isUsedIntn = awinic->IsUsedIRQ;
		/* chip qualify */
		if (!aw8622x_check_qualify(awinic->aw8622x)) {
			aw_dev_err(&i2c->dev,
				"%s:unqualified chip!\n", __func__);
			goto err_aw8622x_check_qualify;
		}
		if (np) {
			ret = aw8622x_parse_dt(&i2c->dev, awinic->aw8622x, np);
			if (ret) {
				aw_dev_err(&i2c->dev,
					"%s: failed to parse device tree node\n",
					__func__);
				goto err_aw8622x_parse_dt;
			}
		}


    input_dev->name = "awinic_haptic";
	input_set_drvdata(input_dev, awinic->aw8622x);
	awinic->aw8622x->input_dev = input_dev;
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	if (awinic->aw8622x->effects_count != 0) {
		input_set_capability(input_dev, EV_FF, FF_PERIODIC);
		input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	}

	if (awinic->aw8622x->effects_count + 1 > FF_EFFECT_COUNT_MAX)
		effect_count_max = awinic->aw8622x->effects_count + 1;
	else
		effect_count_max = FF_EFFECT_COUNT_MAX;
	rc = input_ff_create(input_dev, effect_count_max);
	if (rc < 0) {
		dev_err(awinic->aw8622x->dev, "create FF input device failed, rc=%d\n",
			rc);
		return rc;
	}
        awinic->aw8622x->work_queue =
	    create_singlethread_workqueue("aw8622x_vibrator_work_queue");
	if (!awinic->aw8622x->work_queue) {
		dev_err(&i2c->dev,
			"%s: Error creating aw8622x_vibrator_work_queue\n",
			__func__);
		goto err_sysfs;
	}
	INIT_WORK(&awinic->aw8622x->set_gain_work, aw8622x_haptics_set_gain_work_routine);


		/* aw8622x irq */
		if (gpio_is_valid(awinic->aw8622x->irq_gpio) &&
		    !(awinic->aw8622x->flags & AW8622X_FLAG_SKIP_INTERRUPTS)) {
			/* register irq handler */
			aw8622x_interrupt_setup(awinic->aw8622x);
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
			ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(awinic->aw8622x->irq_gpio),
					NULL, aw8622x_irq, irq_flags,
					"aw8622x", awinic->aw8622x);
			if (ret != 0) {
				aw_dev_err(&i2c->dev,
					"%s: failed to request IRQ %d: %d\n",
					__func__,
					gpio_to_irq(awinic->aw8622x->irq_gpio),
					ret);
				goto err_aw8622x_irq;
			}
		} else {
			aw_dev_err(&i2c->dev,
				"%s skipping IRQ registration\n", __func__);
			/* disable feature support if gpio was invalid */
			awinic->aw8622x->flags |= AW8622X_FLAG_SKIP_INTERRUPTS;
		}
		i2c_set_clientdata(i2c, awinic->aw8622x);
		dev_set_drvdata(&i2c->dev, awinic->aw8622x);
		aw8622x_vibrator_init(awinic->aw8622x);
		aw8622x_haptic_init(awinic->aw8622x);
		aw8622x_ram_work_init(awinic->aw8622x);
	}

    ff = input_dev->ff;
	ff->upload = aw8622x_haptics_upload_effect;
	ff->playback = aw8622x_haptics_playback;
	ff->erase = aw8622x_haptics_erase;
	ff->set_gain = aw8622x_haptics_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		aw_dev_err(awinic->aw8622x->dev, "register input device failed, rc=%d\n",
			rc);
		goto destroy_ff;
	}



	aw_dev_err(&i2c->dev, "%s probe completed successfully!\n", __func__);
	return 0;


err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw8622x->irq_gpio), awinic->aw8622x);
destroy_ff:
	input_ff_destroy(awinic->aw8622x->input_dev);
err_aw8622x_irq:
err_aw8622x_parse_dt:
err_aw8622x_check_qualify:
	if (awinic->name == AW86223 || awinic->name == AW86224_5) {
		devm_kfree(&i2c->dev, awinic->aw8622x);
		awinic->aw8622x = NULL;
	}
/*err_aw8624_irq:
err_aw8624_parse_dt:
	if (awinic->name == AW8624) {
		devm_kfree(&i2c->dev, awinic->aw8624);
		awinic->aw8624 = NULL;
	}
*/
err_id:
	if (gpio_is_valid(awinic->irq_gpio))
		devm_gpio_free(&i2c->dev, awinic->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(awinic->reset_gpio))
		devm_gpio_free(&i2c->dev, awinic->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
	devm_kfree(&i2c->dev, awinic);
	awinic = NULL;
	return ret;

}

static int awinic_i2c_remove(struct i2c_client *i2c)
{
	struct aw8622x *aw8622x = i2c_get_clientdata(i2c);


	aw_dev_info(&i2c->dev, "%s enter\n", __func__);
	cancel_delayed_work_sync(&aw8622x->ram_work);
	cancel_work_sync(&aw8622x->haptic_audio.work);
	hrtimer_cancel(&aw8622x->haptic_audio.timer);
	if (aw8622x->isUsedIntn)
		cancel_work_sync(&aw8622x->rtp_work);
	cancel_work_sync(&aw8622x->long_vibrate_work);

	hrtimer_cancel(&aw8622x->timer);
	mutex_destroy(&aw8622x->lock);
	mutex_destroy(&aw8622x->rtp_lock);
	mutex_destroy(&aw8622x->haptic_audio.lock);
	sysfs_remove_group(&aw8622x->i2c->dev.kobj,
		&aw8622x_vibrator_attribute_group);
	devm_free_irq(&aw8622x->i2c->dev,
		gpio_to_irq(aw8622x->irq_gpio), aw8622x);

	return 0;
}

static const struct i2c_device_id awinic_i2c_id[] = {
	{ AWINIC_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, awinic_i2c_id);

static const struct of_device_id awinic_dt_match[] = {
	{ .compatible = "awinic,awinic_haptic" },
	{ },
};

static struct i2c_driver awinic_i2c_driver = {
	.driver = {
		.name = AWINIC_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(awinic_dt_match),
	},
	.probe = awinic_i2c_probe,
	.remove = awinic_i2c_remove,
	.id_table = awinic_i2c_id,
};

static int __init awinic_i2c_init(void)
{
	int ret = 0;

	pr_info("awinic driver version %s\n", AWINIC_DRIVER_VERSION);

	ret = i2c_add_driver(&awinic_i2c_driver);
	if (ret) {
		pr_err("fail to add awinic device into i2c\n");
		return ret;
	}

	return 0;
}

late_initcall(awinic_i2c_init);

static void __exit awinic_i2c_exit(void)
{
	i2c_del_driver(&awinic_i2c_driver);
}
module_exit(awinic_i2c_exit);

MODULE_DESCRIPTION("awinic Haptic Driver");
MODULE_LICENSE("GPL v2");
