#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include "ringbuffer.h"
#include "fs_haptic.h"
#include "fs3002.h"

#if KERNEL_VERSION_HIGHER(6, 6, 0)
#include <linux/gpio/consumer.h>
#endif

#define FS3002_CHIP_ID		(0x36)
#define FS_REG_DEVID		(0x01)
#define FS_REG_REVID		(0x02)
#define FS_REG_RESET		(0x10)

extern struct foursemi *g_foursemi;

static char* FSERROR = "FSERROR";
static char* FSREAD = "FSREAD";
static char* FSWRITE = "FSWRITE";

static bool foursemi_gpio_atomic_set(int gpio)
{
#if KERNEL_VERSION_HIGHER(6, 6, 0)
	struct gpio_desc *desc = gpio_to_desc(gpio);
	return desc && !gpiod_cansleep(desc);
#else
	return !gpio_cansleep(gpio);
#endif
}

#ifdef	ENABLE_PIN_CONTROL
	static const char *const pctl_names[] = 
	{
		"foursemi_reset_reset",
		"foursemi_reset_active",
		"foursemi_interrupt_active",
	};
#endif

int CUSTOME_WAVE_ID;
static void foursemi_i2c_remove(struct i2c_client *i2c);
static int foursemi_i2c_probe(struct i2c_client *i2c);//, const struct i2c_device_id *id);


static const struct i2c_device_id foursemi_i2c_id[] = 
{
	{FS_I2C_NAME, 0},
	{}
};

static const struct of_device_id fs_dt_match[] = 
{
	{.compatible = "foursemi,fs3002_haptic"},
	{},
};

static struct i2c_driver foursemi_i2c_driver = 
{
	.driver = 
	{
	   .name = FS_I2C_NAME,
	   .owner = THIS_MODULE,
	   .of_match_table = of_match_ptr(fs_dt_match),
	},
	.probe = foursemi_i2c_probe,
	.remove = foursemi_i2c_remove,
	.id_table = foursemi_i2c_id,
};





static int foursemi_i2c_read(struct foursemi *foursemi,unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < FS_I2C_RETRIES) 
	{
		ret = i2c_smbus_read_byte_data(foursemi->i2c, reg_addr);
		if (ret < 0) 
		{
			pr_err("%s:i2c_read addr=0x%02X, cnt=%d error=%d\n", FSERROR,reg_addr, cnt, ret);
		} 
		else 
		{
			*reg_data = ret;
			pr_info("%s,addr=0x%02X, data=0x%02X\n",FSREAD,reg_addr, ret);
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

static int foursemi_i2c_write(struct foursemi *foursemi,unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < FS_I2C_RETRIES) 
	{
		ret = i2c_smbus_write_byte_data(foursemi->i2c, reg_addr, reg_data);
		if (ret < 0) 
		{
			pr_err("%s:i2c_write addr=0x%02X, data=0x%02X, cnt=%d, error=%d\n", FSERROR, reg_addr, reg_data, cnt, ret);
		} 
		else 
		{
			pr_info("%s,addr=0x%02X, data=0x%02X\n",FSWRITE,reg_addr, reg_data);
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

#ifdef ENABLE_PIN_CONTROL
	static int foursemi_select_pin_ctl(struct foursemi *foursemi, const char *name)
	{
		size_t i;
		int rc;

		for (i = 0; i < ARRAY_SIZE(foursemi->pinctrl_state); i++) 
		{
			const char *n = pctl_names[i];
			if (!strncmp(n, name, strlen(n))) 
			{
				rc = pinctrl_select_state(foursemi->foursemi_pinctrl, foursemi->pinctrl_state[i]);
				if (rc)
					pr_err("%s: cannot select '%s'\n", FSERROR, name);
				else
					pr_err("%s: Selected '%s'\n", FSERROR, name);
				goto exit;
			}
		}

		rc = -EINVAL;
		pr_info("%s: '%s' not found\n", FSERROR, name);
	exit:
		return rc;
	}

	static int foursemi_set_interrupt(struct foursemi *foursemi)
	{
		int rc = foursemi_select_pin_ctl(foursemi, "foursemi_interrupt_active");
		return rc;
	}
#endif

int foursemi_sw_reset(struct foursemi *foursemi)
{
	pr_info("enter\n");
	foursemi_i2c_write(foursemi, FS_REG_RESET, 0x02);
	usleep_range(2000, 2500);
	return 0;
}

static int foursemi_hw_reset(struct foursemi *foursemi)
{
#ifdef ENABLE_PIN_CONTROL
	int rc	= 0;
#endif
	unsigned long flags = 0;
	pr_info("enter\n");

	if(foursemi == NULL)
	{
		pr_err("%s:foursemi is NULL\n",FSERROR);
		return 0;
	}
	mutex_lock(&foursemi->reset_mutex);

#ifdef ENABLE_PIN_CONTROL
	//rc = foursemi_select_pin_ctl(foursemi, "foursemi_reset_active");
	//msleep(5);
	rc = foursemi_select_pin_ctl(foursemi, "foursemi_reset_reset");
	usleep_range(100, 150);
	rc = foursemi_select_pin_ctl(foursemi, "foursemi_reset_active");
	msleep(8);
#endif

	if (foursemi && gpio_is_valid(foursemi->reset_gpio)) 
	{
		if (foursemi_gpio_atomic_set(foursemi->reset_gpio))
		{
			spin_lock_irqsave(&foursemi->reset_lock, flags);
			gpio_set_value(foursemi->reset_gpio, 0);
			udelay(100);
			gpio_set_value(foursemi->reset_gpio, 1);
			spin_unlock_irqrestore(&foursemi->reset_lock, flags);
			pr_info("set hw reset gpio success\n");
		}
		else
		{
			gpio_set_value_cansleep(foursemi->reset_gpio, 0);
			udelay(100);
			gpio_set_value_cansleep(foursemi->reset_gpio, 1);
			pr_info("set hw reset gpio done\n");
		}

		usleep_range(8000, 8500);
	} 
	else 
	{
		pr_err("%s: gpio check failed\n", FSERROR);
	}


	mutex_unlock(&foursemi->reset_mutex);
	return 0;
}

//check chip id
static int foursemi_read_chipid(struct foursemi *foursemi, unsigned char *reg)
{
	int ret = -1;
	unsigned char cnt = 0;

	pr_info("foursemi i2c addr = 0x%02x", foursemi->i2c->addr);
	while (cnt < FS_I2C_RETRIES) 
	{
		ret = i2c_smbus_read_byte_data(foursemi->i2c, FS_REG_DEVID);
		if (ret < 0) 
		{
			pr_info("reading chip id\n");
		} 
		else 
		{
			*reg = ret;
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

static int foursemi_parse_chipid(struct foursemi *foursemi)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg_val = 0;
	unsigned char rev_id = 0xff;

	while (cnt < FS_READ_CHIPID_RETRIES) 
	{
		//hardware reset
		ret = foursemi_hw_reset(foursemi);
		if (ret < 0) 
		{
			pr_err("%s: hardware reset failed!\n", FSERROR);
			break;
		}

		ret = foursemi_read_chipid(foursemi, &reg_val);
		if (ret < 0) 
		{
			pr_err("%s: failed to read FS3002_ID: %d\n", FSERROR, ret);
			goto retry;
		}

		switch (reg_val) 
		{
			case FS3002_CHIP_ID:
				foursemi_i2c_read(foursemi, FS_REG_REVID, &rev_id);
				if (rev_id == FS3002_A1) 
				{
					foursemi->name = FS3002_A1;
					pr_info("FS3002_A1 detected\n");
					foursemi_sw_reset(foursemi);
					return 0;
				} 
				else if (rev_id == FS3002_A2) 
				{
					foursemi->name = FS3002_A2;
					pr_info("FS3002_A2 detected\n");
					foursemi_sw_reset(foursemi);
					return 0;
				} 
				else if (rev_id == FS3002_A3) 
				{
					foursemi->name = FS3002_A3;
					pr_info("FS3002_A3 detected\n");
					foursemi_sw_reset(foursemi);
					return 0;
				} 
				else 
				{
					pr_info("unsupported rev_id = (0x%02X)\n",rev_id);
					break;
				}
			default:

				pr_info("unsupported device revision (0x%x)\n", reg_val);
				break;
		}
retry:
		cnt++;
		usleep_range(2000, 3000);
	}

	return -EINVAL;
}

static int foursemi_get_gpio_from_dt(struct foursemi *foursemi, struct device *dev, struct device_node *np)
{
	foursemi->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (foursemi->reset_gpio >= 0) 
	{
		pr_info("reset gpio provided ok\n");
	} 
	else 
	{
		foursemi->reset_gpio = -1;
		pr_err("%s: no reset gpio provided, will not HW reset device\n", FSERROR);
		return -ERANGE;
	}

	foursemi->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (foursemi->irq_gpio >= 0) 
	{
		pr_info("irq gpio provided ok.\n");
		foursemi->IsUsedIRQ = true;
	} 
	else 
	{
		pr_err("%s: no irq gpio provided.\n", FSERROR);
		foursemi->IsUsedIRQ = false;
	}
	
	return 0;
}

static int foursemi_i2c_probe(struct i2c_client *i2c)//, const struct i2c_device_id *id)
{
	struct foursemi *foursemi;
	struct input_dev *input_dev;
	struct ff_device *ff;
	struct device_node *np = i2c->dev.of_node;
	int effect_count_max;
	int irq_flags = 0;
	int ret = -1;
	int rc = 0;
#ifdef ENABLE_PIN_CONTROL
	int i;
#endif
	pr_info("foursemi driver version %s enter\n", FOURSEMI_DRIVER_VERSION);

	//1: check i2c functionality
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) 
	{
		pr_err("%s: check_functionality failed\n", FSERROR);
		return -EIO;
	}

	//2: Allocate foursemi struct memory
	foursemi = devm_kzalloc(&i2c->dev, sizeof(struct foursemi), GFP_KERNEL);
	if (foursemi == NULL)
		return -ENOMEM;

	//3: Allocate input device memory
	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;
	foursemi->dev = &i2c->dev;
	foursemi->i2c = i2c;
	mutex_init(&foursemi->reset_mutex);
	spin_lock_init(&foursemi->reset_lock);

	//4: enable fs3002 to wakeup sys
	device_init_wakeup(foursemi->dev, true);

	//5: save foursemi to i2c->dev->driver_data  (Driver data, set and get with dev_set/get_drvdata)
	i2c_set_clientdata(i2c, foursemi);

	//6: gpio
	if (np) 
	{
		ret = foursemi_get_gpio_from_dt(foursemi, &i2c->dev, np);
		if (ret) 
		{
			pr_err("%s: failed to parse device tree node\n", FSERROR);
			goto err_parse_dt;
		}
	} 
	else 
	{
		foursemi->reset_gpio = -1;
		foursemi->irq_gpio = -1;
	}
	foursemi->enable_pin_control = 0;
#ifdef ENABLE_PIN_CONTROL
	foursemi->foursemi_pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(foursemi->foursemi_pinctrl)) 
	{
		if (PTR_ERR(foursemi->foursemi_pinctrl) == -EPROBE_DEFER) 
		{
			pr_info("pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			return rc;
		}
		pr_info("Target does not use pinctrl\n");
		foursemi->foursemi_pinctrl = NULL;
		rc = -EINVAL;
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(foursemi->pinctrl_state); i++) 
	{
		const char *n = pctl_names[i];
		struct pinctrl_state *state = pinctrl_lookup_state(foursemi->foursemi_pinctrl, n);
		if (!IS_ERR(state)) 
		{
			pr_info("found pin control %s\n", n);
			foursemi->pinctrl_state[i] = state;
			foursemi->enable_pin_control = 1;
			foursemi_set_interrupt(foursemi);
			continue;
		}
		pr_info("cannot find '%s'\n", n);
	}
#endif
	//if not find pin_control or ENABLE_PIN_CONTROL not defined
	// set reset pin low if pinctrl is not supported
	if (!foursemi->enable_pin_control) 
	{
		pr_info("foursemi->enable_pin_control==0\n");
		if (gpio_is_valid(foursemi->reset_gpio)) 
		{
			ret = devm_gpio_request_one(&i2c->dev, foursemi->reset_gpio, GPIOF_OUT_INIT_LOW, "foursemi_rst");
			if (ret) 
			{
				pr_err("%s: rst request failed\n", FSERROR);
				goto err_reset_gpio_request;
			}
		}
	}

	if (gpio_is_valid(foursemi->irq_gpio)) 
	{
		ret = devm_gpio_request_one(&i2c->dev, foursemi->irq_gpio, GPIOF_IN, "foursemi_int");
		if (ret) 
		{
			pr_err("%s: int request failed\n", FSERROR);
			goto err_irq_gpio_request;
		}
	}

	//7: get device id
	ret = foursemi_parse_chipid(foursemi);
	if (ret < 0) 
	{
		pr_err("%s: read_chipid failed ret=%d\n", FSERROR, ret);
		goto err_id;
	}

	//8: Allocate fs3002 device memory
	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3) 
	{
		foursemi->fs3002 = devm_kzalloc(&i2c->dev, sizeof(struct fs3002), GFP_KERNEL);
		if (foursemi->fs3002 == NULL) 
		{
			if (gpio_is_valid(foursemi->irq_gpio))
				//devm_gpio_free(&i2c->dev, foursemi->irq_gpio);
			if (gpio_is_valid(foursemi->reset_gpio))
				//devm_gpio_free(&i2c->dev, foursemi->reset_gpio);
			devm_kfree(&i2c->dev, foursemi);
			foursemi = NULL;
			return -ENOMEM;
		}
		foursemi->fs3002->dev = foursemi->dev;
		foursemi->fs3002->i2c = foursemi->i2c;
		foursemi->fs3002->reset_gpio = foursemi->reset_gpio;
		foursemi->fs3002->irq_gpio = foursemi->irq_gpio;

#ifdef FS_CHECK_QUAL
		if (fs3002_check_qualify(foursemi->fs3002)) 
		{
			pr_err("%s:unqualified chip!\n", FSERROR);
			goto err_fs3002_check_qualify;
		}
#endif
		//9: get configuration from dts
		foursemi->fs3002->fs3002_debug_enable = 0;//zzzz
		if (np) 
		{
			ret = fs3002_parse_dt(foursemi->fs3002, &i2c->dev, np);
			if (ret) 
			{
				pr_err("%s: failed to parse device tree node\n", FSERROR);
				goto err_fs3002_parse_dt;
			}
		}

		if (gpio_is_valid(foursemi->fs3002->irq_gpio) && !(foursemi->fs3002->flags & FS3002_FLAG_SKIP_INTERRUPTS)) 
		{
			//register irq handler
			fs3002_interrupt_setup(foursemi->fs3002);
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
			ret = devm_request_threaded_irq(&i2c->dev, gpio_to_irq(foursemi->fs3002->irq_gpio), NULL, fs3002_irq, irq_flags, "fs3002", foursemi->fs3002);
			if (ret != 0) 
			{
				pr_err("%s: failed to request IRQ %d: %d\n", FSERROR, gpio_to_irq(foursemi->fs3002->irq_gpio), ret);
				goto err_fs3002_irq;
			}
		} 
		else 
		{
			pr_info("skipping IRQ registration\n");
			//disable feature support if gpio was invalid 
			foursemi->fs3002->flags |= FS3002_FLAG_SKIP_INTERRUPTS;
			pr_info("fs3002_irq failed.\n");
		}

		foursemi->fs3002->work_queue = alloc_ordered_workqueue("fs3002_vibrate_work_queue", __WQ_LEGACY | WQ_HIGHPRI | WQ_MEM_RECLAIM);
		if (!foursemi->fs3002->work_queue) 
		{
			pr_err("%s: Error creating fs3002_vibrate_work_queue\n", FSERROR);
			goto err_fs3002_sysfs;
		}
		
		fs3002_vibrator_init(foursemi->fs3002);
		fs3002_reg_init(foursemi->fs3002);
		fs3002_f0_cali_setting_init(foursemi->fs3002);
		fs3002_haptic_init(foursemi->fs3002);
		fs3002_ram_init(foursemi->fs3002);
		
		CUSTOME_WAVE_ID = foursemi->fs3002->dts_info.fs3002_rtp_max;

		//fs3002 input config
		input_dev->name = "fshaptic";
		input_set_drvdata(input_dev, foursemi->fs3002);
		foursemi->fs3002->input_dev = input_dev;
		input_set_capability(input_dev, EV_FF, FF_CONSTANT);
		input_set_capability(input_dev, EV_FF, FF_GAIN);
		if (foursemi->fs3002->effects_count != 0) 
		{
			input_set_capability(input_dev, EV_FF, FF_PERIODIC);
			input_set_capability(input_dev, EV_FF, FF_CUSTOM);
		}

		if (foursemi->fs3002->effects_count + 1 > FF_EFFECT_COUNT_MAX)
			effect_count_max = foursemi->fs3002->effects_count + 1;
		else
			effect_count_max = FF_EFFECT_COUNT_MAX;

		rc = input_ff_create(input_dev, effect_count_max);
		if (rc < 0) 
		{
			pr_err("%s create FF input device failed, rc=%d\n", FSERROR, rc);
			goto err_fs3002_input_ff;
		}

		INIT_WORK(&foursemi->fs3002->set_gain_work, fs3002_haptic_ff_set_gain_work_routine);
		ff = input_dev->ff;
		ff->upload = fs3002_haptics_upload_effect;
		ff->playback = fs3002_haptics_playback;
		ff->erase = fs3002_haptics_erase;
		ff->set_gain = fs3002_haptic_ff_set_gain;
		rc = input_register_device(input_dev);
		if (rc < 0) 
		{
			pr_err("%s register input device failed, rc=%d\n", FSERROR, rc);
			goto fs3002_destroy_ff;
		}
	} 
	else 
	{
		goto err_parse_dt;
	}

	dev_set_drvdata(&i2c->dev, foursemi);
	g_foursemi = foursemi;
	
	ret =  create_rb();
	if (ret < 0) 
	{
		pr_err("%s error creating ringbuffer\n", FSERROR);
		goto err_rb;
	}

	pr_info("probe completed successfully!\n");
	return 0;
err_rb:

fs3002_destroy_ff:
	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3)
		input_ff_destroy(foursemi->fs3002->input_dev);
err_fs3002_input_ff:
	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3)
		destroy_workqueue(foursemi->fs3002->work_queue);
err_fs3002_sysfs:
	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3)
		devm_free_irq(&i2c->dev, gpio_to_irq(foursemi->fs3002->irq_gpio), foursemi->fs3002);
err_fs3002_irq:
err_fs3002_parse_dt:
#ifdef FS_CHECK_QUAL
err_fs3002_check_qualify:
#endif
	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3) 
	{
		devm_kfree(&i2c->dev, foursemi->fs3002);
		foursemi->fs3002 = NULL;
	}
err_id:
	if (gpio_is_valid(foursemi->irq_gpio))
		//devm_gpio_free(&i2c->dev, foursemi->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(foursemi->reset_gpio))
		//devm_gpio_free(&i2c->dev, foursemi->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
	device_init_wakeup(foursemi->dev, false);
	devm_kfree(&i2c->dev, foursemi);
	foursemi = NULL;
	return ret;
}


static void foursemi_i2c_remove(struct i2c_client *i2c)
{
	struct foursemi *foursemi = i2c_get_clientdata(i2c);
	pr_info("enter\n");

	//2025123, need to do reset work
	foursemi_sw_reset(foursemi);	

	if (foursemi->name == FS3002_A1 || foursemi->name == FS3002_A2 || foursemi->name == FS3002_A3) 
	{
		pr_info("remove fs3002\n");
		
#ifdef FS_HAPSTREAM
		if (foursemi->fs3002 && foursemi->fs3002->fs_config_proc)
			proc_remove(foursemi->fs3002->fs_config_proc);

		if (foursemi->fs3002 && foursemi->fs3002->start_buf) {
			ClearPageReserved(virt_to_page(foursemi->fs3002->start_buf));
			free_pages((unsigned long)foursemi->fs3002->start_buf, HAPSTREAM_MMAP_PAGE_ORDER);
			foursemi->fs3002->start_buf = NULL;
		}
#endif

		sysfs_remove_group(&i2c->dev.kobj, &fs3002_vibrator_attribute_group);
		devm_free_irq(&i2c->dev, gpio_to_irq(foursemi->fs3002->irq_gpio), foursemi->fs3002);
		if (gpio_is_valid(foursemi->fs3002->irq_gpio))
		{
			//devm_gpio_free(&i2c->dev, foursemi->fs3002->irq_gpio);
		}
		if (gpio_is_valid(foursemi->fs3002->reset_gpio))
		{
			//devm_gpio_free(&i2c->dev, foursemi->fs3002->reset_gpio);
		}
		if (foursemi->fs3002 != NULL) 
		{
			flush_workqueue(foursemi->fs3002->work_queue);
			destroy_workqueue(foursemi->fs3002->work_queue);
		}
		
		devm_kfree(&i2c->dev, foursemi->fs3002);
		foursemi->fs3002 = NULL;
	} 
	else 
	{
		pr_err("%s no chip\n", FSERROR);
	}

	release_rb();
	device_init_wakeup(foursemi->dev, false);

	pr_info("exit\n");
}

module_i2c_driver(foursemi_i2c_driver);



MODULE_DEVICE_TABLE(i2c, foursemi_i2c_id);
MODULE_DESCRIPTION("Foursemi Haptic Driver");
MODULE_LICENSE("GPL v2");


