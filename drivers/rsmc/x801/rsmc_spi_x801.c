#include "rsmc_spi_x801.h"

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/machine.h>
#include <linux/hwspinlock.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <uapi/linux/gpio.h>
#include "rsmc_stdlib_s.h"
#include <linux/spi/spi.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include "../module_type.h"
#include "rsmc_msg_loop_x801.h"
#include "rsmc_device_x801.h"

#ifdef CETCLOG_TAG
#undef CETCLOG_TAG
#endif
#define CETCLOG_TAG RSMC_SPI

#define RSMC_DEVICE_NAME "xiaomi_rsmc"
#define RSMC_MISC_DEVICE_NAME "rsmc"
#define DELAY_100_MS 100
#define SPI_READ_RETRY_TIME 5

volatile int is_pic_mode = 0;

static struct smc_core_data *g_smc_core = NULL;

struct smc_core_data *smc_get_core_data_x801(void)
{
	return g_smc_core;
}

static int smc_setup_tcxo_pwr(struct smc_core_data *cd)
{
	struct rsmc_power_supply *power = NULL;
	int ret;

	if (cd == NULL)
		return 0;
	power = &cd->rsmc_powers;
	if (power->type == RSMC_POWER_UNUSED)
		return 0;
	if (power->use_count) {
		power->use_count++;
		return 0;
	}
	switch (power->type) {
	case RSMC_POWER_LDO:
		power->regulator = regulator_get(&cd->sdev->dev, "rsmc-tcxo-vcc");
		if (IS_ERR_OR_NULL(power->regulator)) {
			printk(KERN_ERR"%s: fail to get %s\n", __func__, "rsmc-tcxo-vcc");
			return -EINVAL;
		}
		ret = regulator_set_voltage(power->regulator, power->ldo_value,
			power->ldo_value);
		if (ret) {
			regulator_put(power->regulator);
			printk(KERN_ERR"%s: fail to set %s\n", __func__, "rsmc-tcxo-vcc");
			return ret;
		}
		printk(KERN_INFO"%s: tcxo-vcc setup succ!", __func__);
		break;
	case RSMC_POWER_GPIO:
		ret = gpio_request(power->gpio, "rsmc-tcxo-vcc-gpio");
		if (ret) {
			printk(KERN_ERR"%s: request gpio %d for %s failed\n", __func__, power->gpio, "rsmc-tcxo-vcc-gpio");
			return ret;
		}
		ret = gpio_get_value(power->gpio);
		printk(KERN_INFO"%s: tcxo_pwr_gpio %d value %d", __func__, power->gpio, ret);

		ret = gpio_direction_output(power->gpio, GPIO_LOW);
		if (ret) {
			printk(KERN_ERR"%s: tcxo_pwr_gpio %d output high err %d", __func__, power->gpio, ret);
			return ret;
		}

		ret = gpio_get_value(power->gpio);
		printk(KERN_INFO"%s: tcxo_pwr_gpio %d value %d", __func__, power->gpio, ret);
		break;
	default:
		break;
	}
	power->use_count++;
	return 0;
}

static int smc_setup_soc_pwr_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.soc_pwr_gpio)) {
		rc = gpio_request(cd->gpios.soc_pwr_gpio, "rsmc_soc_pwr");
		if (rc) {
			printk(KERN_ERR"%s: soc_pwr_gpio gpio_request %d failed %d\n",
				__func__, cd->gpios.soc_pwr_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.soc_pwr_gpio);
		printk(KERN_INFO"%s: soc_pwr_gpio %d value %d",
			__func__, cd->gpios.soc_pwr_gpio, rc);

		rc = gpio_direction_output(cd->gpios.soc_pwr_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: soc_pwr_gpio %d output high err %d",
				__func__, cd->gpios.soc_pwr_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.soc_pwr_gpio);
		printk(KERN_INFO"%s: soc_pwr_gpio %d value %d",
			__func__, cd->gpios.soc_pwr_gpio, rc);
	}
	return 0;
}

static int smc_setup_cs_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.cs_gpio)) {
		rc = gpio_request(cd->gpios.cs_gpio, "rsmc_cs");
		if (rc) {
			printk(KERN_ERR"%s: cs_gpio gpio_request %d failed %d\n",
				__func__, cd->gpios.cs_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.cs_gpio);
		printk(KERN_INFO"%s: cs_gpio %d value %d",
			__func__, cd->gpios.cs_gpio, rc);

		rc = gpio_direction_output(cd->gpios.cs_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: cs_gpio %d output high err %d",
				__func__, cd->gpios.cs_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.cs_gpio);
		printk(KERN_INFO"%s: cs_gpio %d value %d",
			__func__, cd->gpios.cs_gpio, rc);
	}
	return 0;
}

static int smc_setup_soc_en_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.soc_en_gpio)) {
		rc = gpio_request(cd->gpios.soc_en_gpio, "rsmc_soc_en");
		if (rc) {
			printk(KERN_ERR"%s: soc_en_gpio %d request failed %d\n",
				__func__, cd->gpios.soc_en_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.soc_en_gpio);
		printk(KERN_INFO"%s: soc_en_gpio %d value %d",
			__func__, cd->gpios.soc_en_gpio, rc);

		rc = gpio_direction_output(cd->gpios.soc_en_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: soc_en_gpio gpio %d output high err %d",
				__func__, cd->gpios.soc_en_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.soc_en_gpio);
		printk(KERN_INFO"%s: soc_en_gpio %d value %d",
			__func__, cd->gpios.soc_en_gpio, rc);
	}
	return 0;
}

static int smc_setup_mcu_reset_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.mcu_reset_gpio)) {
		rc = gpio_request(cd->gpios.mcu_reset_gpio, "rsmc_mcu_reset");
		if (rc) {
			printk(KERN_ERR"%s: mcu_reset_gpio %d failed %d\n",
				__func__, cd->gpios.mcu_reset_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.mcu_reset_gpio);
		printk(KERN_INFO"%s: mcu_reset_gpio %d value %d",
			__func__, cd->gpios.mcu_reset_gpio, rc);

		rc = gpio_direction_output(cd->gpios.mcu_reset_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: mcu_reset_gpio %d output high err %d",
				__func__, cd->gpios.mcu_reset_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.mcu_reset_gpio);
		printk(KERN_INFO"%s: mcu_reset_gpio %d value %d",
			__func__, cd->gpios.mcu_reset_gpio, rc);
	}
	return 0;
}

static int smc_setup_tx_ant_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.tx_ant_gpio)) {
		rc = gpio_request(cd->gpios.tx_ant_gpio, "rsmc_tx_ant");
		if (rc) {
			printk(KERN_ERR"%s: tx_ant_gpio %d request failed %d\n",
				__func__, cd->gpios.tx_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.tx_ant_gpio);
		printk(KERN_INFO"%s: tx_ant_gpio %d value %d",
			__func__, cd->gpios.tx_ant_gpio, rc);

		rc = gpio_direction_output(cd->gpios.tx_ant_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: tx_ant_gpio %d output low err %d",
				__func__, cd->gpios.tx_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.tx_ant_gpio);
		printk(KERN_INFO"%s: tx_ant_gpio %d value %d",
			__func__, cd->gpios.tx_ant_gpio, rc);
	}
	return 0;
}

static int smc_setup_rx_ant_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.rx_ant_gpio)) {
		rc = gpio_request(cd->gpios.rx_ant_gpio, "rsmc_rx_ant");
		if (rc) {
			printk(KERN_ERR"%s: rx_ant_gpio %d request failed %d\n",
				__func__, cd->gpios.rx_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.rx_ant_gpio);
		printk(KERN_INFO"%s: rx_ant_gpio %d value %d",
			__func__, cd->gpios.rx_ant_gpio, rc);

		rc = gpio_direction_output(cd->gpios.rx_ant_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: rx_ant_gpio %d output high err %d",
				__func__, cd->gpios.rx_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.rx_ant_gpio);
		printk(KERN_INFO"%s: rx_ant_gpio %d value %d",
			__func__, cd->gpios.rx_ant_gpio, rc);
	}
	return 0;
}

static int smc_setup_irq_gpio(struct smc_core_data *cd)
{
	int rc;
	if (cd == NULL) {
		printk(KERN_ERR"%s: cd null", __func__);
		return -EINVAL;
	}
	if (gpio_is_valid(cd->gpios.irq_gpio)) {
		rc = gpio_request(cd->gpios.irq_gpio, "rsmc_irq");
		if (rc) {
			printk(KERN_ERR"%s: irq_gpio %d request failed %d\n",
				__func__, cd->gpios.irq_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.irq_gpio);
		printk(KERN_INFO"%s: irq_gpio %d value %d",
			__func__, cd->gpios.irq_gpio, rc);

		gpio_set_value(cd->gpios.irq_gpio, GPIO_HIGH);
                msm_gpio_mpm_wake_set((cd->gpios.irq_gpio - 512), true);

		rc = gpio_direction_input(cd->gpios.irq_gpio);
		if (rc) {
			printk(KERN_ERR"%s: irq_gpio %d input err %d",
				__func__, cd->gpios.irq_gpio, rc);
			return rc;
		}
		rc = gpio_get_value(cd->gpios.irq_gpio);
		printk(KERN_INFO"%s: irq_gpio %d value %d",
			__func__, cd->gpios.irq_gpio, rc);
	}
	return 0;
}

static int smc_setup_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	// tcxo pwr
	rc = smc_setup_tcxo_pwr(cd);
	// cs
	rc = smc_setup_cs_gpio(cd);
	// chip pwr gpio
	rc = smc_setup_soc_pwr_gpio(cd);
	// init gpio
	rc = smc_setup_soc_en_gpio(cd);
	// reset gpio
	rc = smc_setup_mcu_reset_gpio(cd);
	// ant tx gpio
	rc = smc_setup_tx_ant_gpio(cd);
	// rx ant gpio
	rc = smc_setup_rx_ant_gpio(cd);
	// IRQ GPIO
	rc = smc_setup_irq_gpio(cd);
	return 0;
}

static void smc_free_gpio(struct smc_core_data *cd)
{
	if (cd == NULL)
		return;
	if (gpio_is_valid(cd->gpios.soc_pwr_gpio))
		gpio_free(cd->gpios.soc_pwr_gpio);
	if (gpio_is_valid(cd->gpios.cs_gpio))
		gpio_free(cd->gpios.cs_gpio);
	if (gpio_is_valid(cd->gpios.soc_en_gpio))
		gpio_free(cd->gpios.soc_en_gpio);
	if (gpio_is_valid(cd->gpios.mcu_reset_gpio))
		gpio_free(cd->gpios.mcu_reset_gpio);
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio))
		gpio_free(cd->gpios.pa_pwr_gpio);
	if (gpio_is_valid(cd->gpios.tx_ant_gpio))
		gpio_free(cd->gpios.tx_ant_gpio);
	if (gpio_is_valid(cd->gpios.rx_ant_gpio))
		gpio_free(cd->gpios.rx_ant_gpio);
	if (gpio_is_valid(cd->gpios.irq_gpio))
		gpio_free(cd->gpios.irq_gpio);
}

static void rsmc_tcxo_power_free(struct smc_core_data *cd)
{
	struct rsmc_power_supply *power = NULL;

	if (cd == NULL)
		return;
	power = &cd->rsmc_powers;
	if (power->type == RSMC_POWER_UNUSED)
		return;
	if ((--power->use_count) > 0)
		return;

	switch (power->type) {
	case RSMC_POWER_LDO:
		if (IS_ERR_OR_NULL(power->regulator)) {
			printk(KERN_ERR"%s:fail to get %s\n", __func__, "tcxo-vcc");
			return;
		}
		regulator_put(power->regulator);
		break;
	case RSMC_POWER_GPIO:
		gpio_direction_output(power->gpio, 0);
		gpio_free(power->gpio);
		break;
	default:
		printk(KERN_ERR"%s: invalid power type %d\n",
			__func__, power->type);
		break;
	}
}

static int smc_setup_spi(struct smc_core_data *cd)
{
	int rc;

	printk(KERN_INFO"%s: enter", __func__);
	if (cd == NULL)
		return -EINVAL;
	cd->sdev->bits_per_word = X801_MIN_BITS_PER_WORD;
	cd->sdev->mode = SPI_MODE_0;
	cd->sdev->max_speed_hz = X801_MIN_SPEED;

	rc = spi_setup(cd->sdev);
	if (rc) {
		printk(KERN_ERR"%s: spi setup fail\n", __func__);
		return rc;
	}
	printk(KERN_INFO"%s: succ", __func__);
	return 0;
}

static irqreturn_t rsmc_irq_thread(int irq, void *dev_id)
{
	struct smc_core_data *cd = (struct smc_core_data *)dev_id;
	disable_irq_nosync(cd->irq);
	printk(KERN_INFO"%s: irq:%d, spi status:%d", __func__, cd->irq, get_rsmc_spi_status());
	rsmc_spi_recv_proc();
	enable_irq(cd->irq);
	return IRQ_HANDLED;
}

void rsmc_spi_irq_ctl_x801(bool ctl)
{
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (ctl)
		enable_irq(cd->irq);
	else
		disable_irq(cd->irq);
	printk(KERN_INFO"%s: irq:%d,ctl:%d", __func__, cd->irq, (u32)ctl);
}

int smc_setup_irq(struct smc_core_data *cd)
{
	int rc, irq;
	unsigned long irq_flag_type;

	if (cd == NULL) {
		printk(KERN_ERR"%s: smc_core_data is null\n", __func__);
		return -EINVAL;
	}

	irq = gpio_to_irq(cd->gpios.irq_gpio);
	irq_flag_type = IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND;

	rc = request_threaded_irq(irq, NULL, rsmc_irq_thread, irq_flag_type, "rsmc", cd);
	if (rc) {
		printk(KERN_ERR"%s: irq:%d,rc:%d", __func__, irq, rc);
		return rc;
	}
	printk(KERN_INFO"%s: irq:%d,rc:%d", __func__, irq, rc);
	enable_irq_wake(irq);
	disable_irq(irq);
	cd->irq = irq;

	return 0;
}

static int smc_core_init(struct smc_core_data *cd)
{
	int rc;
	dev_set_drvdata(&cd->sdev->dev, cd);
	rc = smc_setup_irq(cd);
	if (rc) {
		printk(KERN_ERR"%s:failed to setup irq", __func__);
		return 0;
	}
	atomic_set(&cd->register_flag, 1);
	return 0;
}

int spi_read_data_valid(const unsigned char *data, unsigned int length, int *i)
{
	for (*i = 0; *i < length; (*i)++) {
		if (data[*i] != 0)
			return 1;
	}
	return 0;
}

void rsmc_spi_cs_set(u32 control)
{
#ifdef X801_SPI_CS
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return;
	gpio_set_value(cd->gpios.cs_gpio, control);
#endif
}

int rsmc_spi_read_x801(unsigned char *data, unsigned int length)
{
	int retval;
	struct smc_core_data *cd = smc_get_core_data_x801();
	mutex_lock(&cd->spi_mutex);
	retval = memset_s(&cd->t, sizeof(struct spi_transfer),
		0x00, sizeof(struct spi_transfer));
	if (retval != EOK)
		printk(KERN_ERR"%s: memset_s t fail", __func__);
	retval = memset_s(cd->tx_buf, sizeof(cd->tx_buf), 0, sizeof(cd->tx_buf));
	if (retval != EOK)
		printk(KERN_ERR"%s: memset_s tx_buf fail", __func__);
	spi_message_init(&cd->msg);
	cd->t.len = length;
	cd->t.tx_buf = cd->tx_buf;
	cd->t.rx_buf = data;
#ifdef X801_SPI_CS
	cd->t.cs_change = 0;
#else
	cd->t.cs_change = 1;
#endif
	cd->t.bits_per_word = X801_MAX_BITS_PER_WORD;
	spi_message_add_tail(&cd->t, &cd->msg);
	rsmc_spi_cs_set(GPIO_LOW);
	retval = spi_sync(cd->sdev, &cd->msg);
	rsmc_spi_cs_set(GPIO_HIGH);
	if (retval == 0)
		retval = length;
	else
		printk(KERN_ERR"%s: spi_sync fail %d", __func__, retval);
	mutex_unlock(&cd->spi_mutex);

	return retval;
}

int rsmc_spi_write_x801(unsigned char *data, unsigned int length)
{
	int ret;
	struct smc_core_data *cd = smc_get_core_data_x801();
	mutex_lock(&cd->spi_mutex);
	ret = memset_s(&cd->t, sizeof(struct spi_transfer),
		0x00, sizeof(struct spi_transfer));
	if (ret != EOK)
		printk(KERN_ERR"%s: memset_s fail", __func__);
	ret = memset_s(cd->tx_buf, sizeof(cd->tx_buf), 0, sizeof(cd->tx_buf));
	if (ret != EOK)
		printk(KERN_ERR"%s: tx_buf memset_s fail", __func__);
	ret = memcpy_s(cd->tx_buf, sizeof(cd->tx_buf), data, length);
	if (ret != EOK)
		printk(KERN_ERR"%s: tx_buf memcpy_s fail", __func__);

	spi_message_init(&cd->msg);
	cd->t.len = length;
	cd->t.tx_buf = cd->tx_buf;
#ifdef X801_SPI_CS
	cd->t.cs_change = 0;
#else
	cd->t.cs_change = 1;
#endif
	cd->t.bits_per_word = X801_MAX_BITS_PER_WORD;
	spi_message_add_tail(&cd->t, &cd->msg);
	rsmc_spi_cs_set(GPIO_LOW);
	ret = spi_sync(cd->sdev, &cd->msg);
	rsmc_spi_cs_set(GPIO_HIGH);
	if (ret == 0)
		ret = length;
	else
		printk(KERN_ERR"%s: spi_sync error = %d\n", __func__, ret);
	mutex_unlock(&cd->spi_mutex);

	return ret;
}

int rsmc_spi_reg_read(u32 addr, u32 *value)
{
	int rc, ret;
	struct smc_core_data *cd = smc_get_core_data_x801();

	if (value == NULL)
		return -EINVAL;
	mutex_lock(&cd->spi_mutex);

	cd->tx_buff[0] = 0x80 | addr;

	/* read data */
	ret = memset_s(&cd->t, sizeof(struct spi_transfer), 0, sizeof(struct spi_transfer));
	if (ret != EOK)
		printk(KERN_ERR"%s: memset_s fail", __func__);

	spi_message_init(&cd->msg);

	cd->t.rx_buf = cd->rx_buff;
	cd->t.tx_buf = cd->tx_buff;
	cd->t.len = sizeof(u32);
#ifdef X801_SPI_CS
	cd->t.cs_change = 0;
#else
	cd->t.cs_change = 1;
#endif
	cd->t.bits_per_word = X801_MIN_BITS_PER_WORD;
	spi_message_add_tail(&cd->t, &cd->msg);
	rsmc_spi_cs_set(GPIO_LOW);
	rc = spi_sync(cd->sdev, &cd->msg);
	rsmc_spi_cs_set(GPIO_HIGH);
	if (rc) {
		printk(KERN_ERR"%s: spi_sync %d\n", __func__, rc);
		mdelay(SPI_MSLEEP_SHORT_TIME);
		rc = spi_sync(cd->sdev, &cd->msg);
		printk(KERN_ERR"%s: spi_sync %d\n", __func__, rc);
	}
	*value = (cd->rx_buff[0]<<24) |
		(cd->rx_buff[1]<<16) |
		(cd->rx_buff[2]<<8) |
		(cd->rx_buff[3]);
	mutex_unlock(&cd->spi_mutex);

	printk(KERN_INFO"%s: addr:%d,value:0x%x", __func__, addr, *value);

	return rc;
}

int rsmc_spi_reg_write(u32 addr, u32 value)
{
	int rc, ret;
	struct smc_core_data *cd = smc_get_core_data_x801();

	mutex_lock(&cd->spi_mutex);

	/* set header */
	cd->tx_buff[0] = addr & 0x7F;
	cd->tx_buff[1] = (0xff & (value >> 16));
	cd->tx_buff[2] = (0xff & (value >> 8));
	cd->tx_buff[3] = (0xff & value);

	/* write data */
	ret = memset_s(&cd->t, sizeof(struct spi_transfer), 0, sizeof(struct spi_transfer));
	if (ret != EOK)
		printk(KERN_ERR"%s: memset_s fail", __func__);

	spi_message_init(&cd->msg);

	cd->t.tx_buf = cd->tx_buff;
	cd->t.rx_buf = cd->rx_buff;
	cd->t.len = sizeof(u32);
#ifdef X801_SPI_CS
	cd->t.cs_change = 0;
#else
	cd->t.cs_change = 1;
#endif
	cd->t.bits_per_word = X801_MIN_BITS_PER_WORD;
	spi_message_add_tail(&cd->t, &cd->msg);
	rsmc_spi_cs_set(GPIO_LOW);
	rc = spi_sync(cd->sdev, &cd->msg);
	rsmc_spi_cs_set(GPIO_HIGH);
	if (rc) {
		printk(KERN_ERR"%s: spi_sync %d\n", __func__, rc);
		mdelay(SPI_MSLEEP_SHORT_TIME);
		rc = spi_sync(cd->sdev, &cd->msg);
		printk(KERN_ERR"%s: spi_sync %d\n", __func__, rc);
	}

	mutex_unlock(&cd->spi_mutex);

	return rc;
}

int rsmc_spi_set_min_speed(void)
{
	int rc;
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return -EINVAL;
	cd->sdev->bits_per_word = X801_MIN_BITS_PER_WORD;
	cd->sdev->mode = SPI_MODE_0;
	cd->sdev->max_speed_hz = X801_MIN_SPEED;
	rc = spi_setup(cd->sdev);
	if (rc) {
		printk(KERN_ERR"%s: spi setup fail\n", __func__);
		return rc;
	}
	printk(KERN_INFO"%s: set speed %d succ", __func__, cd->sdev->max_speed_hz);
	return 0;
}

int rsmc_spi_set_max_speed(void)
{
	int rc;
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return -EINVAL;
	cd->sdev->bits_per_word = X801_MAX_BITS_PER_WORD;
	cd->sdev->mode = SPI_MODE_0;
	cd->sdev->max_speed_hz = X801_MAX_SPEED;
	rc = spi_setup(cd->sdev);
	if (rc) {
		printk(KERN_ERR"%s: spi setup fail\n", __func__);
		return rc;
	}
	printk(KERN_INFO"%s: set speed %d succ", __func__, cd->sdev->max_speed_hz);
	return 0;
}

int rsmc_register_dev_x801(struct smc_device *dev)
{
	int rc = -EINVAL;
	struct smc_core_data *cd = smc_get_core_data_x801();

	printk(KERN_INFO"%s: enter", __func__);

	if (dev == NULL || cd == NULL) {
		printk(KERN_ERR"%s: input null", __func__);
		goto register_err;
	}
	/* check device configed ot not */
	if (atomic_read(&cd->register_flag)) {
		printk(KERN_ERR"%s: smc have registerd", __func__);
		goto register_err;
	}
	dev->smc_core = cd;
	dev->gpios = &cd->gpios;
	dev->sdev = cd->sdev;
	cd->smc_dev = dev;

	rc = smc_setup_gpio(cd);
	if (rc) {
		printk(KERN_ERR"%s: spi dev init fail", __func__);
		goto dev_init_err;
	}
	rc = smc_setup_spi(cd);
	if (rc) {
		printk(KERN_ERR"%s: spi dev init fail", __func__);
		goto err;
	}

	rc = smc_core_init(cd);
	if (rc) {
		printk(KERN_ERR"%s: core init", __func__);
		goto err;
	}
	return 0;
err:
	smc_free_gpio(cd);
	rsmc_tcxo_power_free(cd);
dev_init_err:
	cd->smc_dev = 0;
register_err:
	return rc;
}

int rsmc_unregister_dev_x801(struct smc_device *dev)
{
	struct smc_core_data *cd = smc_get_core_data_x801();

	smc_free_gpio(cd);
	rsmc_tcxo_power_free(cd);
	return 0;
}

static int smc_parse_tcxo_power_config(struct device_node *smc_node,
	struct smc_core_data *cd)
{
	struct rsmc_power_supply *power = NULL;
	int rc;
	u32 value;

	power = &cd->rsmc_powers;
	rc = of_property_read_u32(smc_node, "rsmc-tcxo-vcc-type", &power->type);
	if (rc || power->type == RSMC_POWER_UNUSED) {
		printk(KERN_INFO"%s: power type not config or 0, unused\n", __func__);
		return 0;
	}
	switch (power->type) {
	case RSMC_POWER_GPIO:
		power->gpio = of_get_named_gpio(smc_node, "tcxo_pwr_gpio", 0);
		printk(KERN_INFO"rsmc-vcc-gpio = %d\n", power->gpio);
		break;
	case RSMC_POWER_LDO:
		rc = of_property_read_u32(smc_node, "rsmc-tcxo-vcc-value",
				&power->ldo_value);
		printk(KERN_INFO"rsmc-tcxo-vcc-value = %d\n", power->ldo_value);
		break;
	default:
		printk(KERN_INFO"%s: invaild power type %d", __func__, power->type);
		break;
	}
	rc = of_property_read_u32(smc_node, "tcxo_pwr_after_delay_ms", &value);
	if (!rc) {
		power->tcxo_pwr_after_delay_ms = (u32)value;
		printk(KERN_INFO"%s: tcxo_pwr_after_delay_ms %d\n", __func__, value);
	}
	return 0;
}

int smc_parse_gpio_config_x801(struct device_node *smc_node,
	struct smc_core_data *cd)
{
	u32 value;
	if ((smc_node == NULL) || (cd == NULL)) {
		printk(KERN_INFO"%s: input null\n", __func__);
		return -ENODEV;
	}
	value = of_get_named_gpio(smc_node, "soc_pwr_gpio", 0);
	printk(KERN_INFO"pwr_gpio = %d", value);
	cd->gpios.soc_pwr_gpio = (u32)value;

	value = of_get_named_gpio(smc_node, "cs_gpio", 0);
	printk(KERN_INFO"cs_gpio = %d", value);
	cd->gpios.cs_gpio = (u32)value;

	value = of_get_named_gpio(smc_node, "en_gpio", 0);
	printk(KERN_INFO"soc_en_gpio = %d", value);
	cd->gpios.soc_en_gpio = value;

	value = of_get_named_gpio(smc_node, "mcu_rst_gpio", 0);
	printk(KERN_INFO"rst_gpio = %d", value);
	cd->gpios.mcu_reset_gpio = value;

	value = of_get_named_gpio(smc_node, "pa_gpio", 0);
	printk(KERN_INFO"pa_gpio = %d", value);
	cd->gpios.pa_pwr_gpio = value;

	value = of_get_named_gpio(smc_node, "rx_ant_gpio", 0);
	printk(KERN_INFO"rx_ant_gpio = %d", value);
	cd->gpios.rx_ant_gpio = value;

	value = of_get_named_gpio(smc_node, "tx_ant_gpio", 0);
	printk(KERN_INFO"tx_ant_gpio = %d", value);
	cd->gpios.tx_ant_gpio = value;

	value = of_get_named_gpio(smc_node, "irq_gpio", 0);
	printk(KERN_INFO"irq_gpio = %d", value);
	cd->gpios.irq_gpio = value;

	value = of_get_named_gpio(smc_node, "tt_bd_sel_gpio", 0);
	printk(KERN_INFO"tt_bd_sel_gpio = %d", value);
	cd->gpios.tt_bd_sel_gpio = value;

	value = of_get_named_gpio(smc_node, "vdet_sel_gpio", 0);
	printk(KERN_INFO"vdet_sel_gpio = %d", value);
	cd->gpios.vdet_sel_gpio = value;

	value = of_get_named_gpio(smc_node, "pa_enable_gpio", 0);
	printk(KERN_INFO"pa_enable_gpio = %d", value);
	cd->gpios.pa_enable_gpio = value;

	value = of_get_named_gpio(smc_node, "rsmc_vc1_ant_gpio", 0);
	 of_property_read_u32(smc_node,"rsmc_vc1_default_value",&cd->gpios.vc1_default_value);
	printk(KERN_INFO"rsmc_vc1_ant_gpio = %d, default_value = %d",
		value, cd->gpios.vc1_default_value);
	cd->gpios.vc1_ant_gpio = value;

	value = of_get_named_gpio(smc_node, "rsmc_vc2_ant_gpio", 0);
	of_property_read_u32(smc_node,"rsmc_vc2_default_value",&cd->gpios.vc2_default_value);
	printk(KERN_ERR"rsmc_vc2_ant_gpio = %d, default_value = %d",
		value, cd->gpios.vc2_default_value);
	cd->gpios.vc2_ant_gpio = value;

	value = of_get_named_gpio(smc_node, "rsmc_vc3_ant_gpio", 0);
    of_property_read_u32(smc_node,"rsmc_vc3_default_value",&cd->gpios.vc3_default_value);
	printk(KERN_INFO"rsmc_vc3_ant_gpio = %d, default_value = %d",
		value, cd->gpios.vc3_default_value);
	cd->gpios.vc3_ant_gpio = value;

	cd->pa_power = regulator_get(&cd->sdev->dev, "rsmc-pa-dcdc");
	return 0;
}

int smc_parse_spi_config_x801(struct device_node *smc_node,
	struct smc_core_data *cd)
{
	int rc;
	u32 value;
	struct smc_spi_config *spi_config = NULL;

	if (smc_node == NULL || cd == NULL) {
		printk(KERN_INFO"%s: input null\n", __func__);
		return -ENODEV;
	}

	spi_config = &cd->spi_config;

	rc = of_property_read_u32(smc_node, "spi-max-frequency", &value);
	if (!rc) {
		spi_config->max_speed_hz = value;
		printk(KERN_INFO"%s: spi-max-frequency configured %d\n",
				__func__, value);
	}
	rc = of_property_read_u32(smc_node, "spi-bus-id", &value);
	if (!rc) {
		spi_config->bus_id = (u8)value;
		printk(KERN_INFO"%s: spi-bus-id configured %d\n", __func__, value);
	}
	rc = of_property_read_u32(smc_node, "spi-mode", &value);
	if (!rc) {
		spi_config->mode = value;
		printk(KERN_INFO"%s: spi-mode configured %d\n", __func__, value);
	}
	rc = of_property_read_u32(smc_node, "bits-per-word", &value);
	if (!rc) {
		spi_config->bits_per_word = value;
		printk(KERN_INFO"%s: bits-per-word configured %d\n", __func__, value);
	}

	if (!cd->spi_config.max_speed_hz)
		cd->spi_config.max_speed_hz = SMC_SPI_SPEED_DEFAULT;
	if (!cd->spi_config.mode)
		cd->spi_config.mode = SPI_MODE_0;
	if (!cd->spi_config.bits_per_word)
		cd->spi_config.bits_per_word = SMC_SPI_DEFAULT_BITS_PER_WORD;

	cd->sdev->mode = spi_config->mode;
	cd->sdev->max_speed_hz = spi_config->max_speed_hz;
	cd->sdev->bits_per_word = spi_config->bits_per_word;
	return 0;
}

int smc_parse_config_x801(struct device_node *smc_node,
	struct smc_core_data *cd)
{
	int ret;

	if ((smc_node == NULL) || (cd == NULL)) {
		printk(KERN_INFO"%s: input null\n", __func__);
		return -ENODEV;
	}

	ret = smc_parse_tcxo_power_config(smc_node, cd);
	if (ret != 0)
		return ret;
	ret = smc_parse_gpio_config_x801(smc_node, cd);
	if (ret != 0)
		return ret;
	ret = smc_parse_spi_config_x801(smc_node, cd);
	if (ret != 0)
		return ret;

	cd->smc_node = smc_node;

	return 0;
}

static ssize_t rsmc_pic_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t rsmc_pic_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	if (buf[0] == '1'){
          is_pic_mode = 1;
          printk(KERN_ERR"Set rsmc_pic to 1\n");
        }
  	else if (buf[0] == '2'){
          is_pic_mode = 2;
          printk(KERN_ERR"Set rsmc_pic to 2\n");
        }
	else{
          is_pic_mode = 0;
          printk(KERN_ERR"Set rsmc_pic to 0\n");
        }
	return count;
}

static DEVICE_ATTR(rsmc_pic,0664,rsmc_pic_show,rsmc_pic_store);

static struct attribute *dev_attrs[] = {
  	&dev_attr_rsmc_pic.attr,
  	NULL,
 };

static struct attribute_group dev_attr_group = {
  	.attrs = dev_attrs,
};
static int smc_probe(struct spi_device *sdev)
{
	struct smc_core_data *cd = NULL;
	int rc;
	int ret=0;

	printk(KERN_INFO"%s: in\n", __func__);

	cd = kzalloc(sizeof(struct smc_core_data), GFP_KERNEL);
	if (cd == NULL)
		return -ENOMEM;
	cd->sdev = sdev;
	rc = smc_parse_config_x801(sdev->dev.of_node, cd);
	if (rc) {
		printk(KERN_ERR"%s: parse dts fail\n", __func__);
		kfree(cd);
		return -ENODEV;
	}
	mutex_init(&cd->spi_mutex);
	atomic_set(&cd->register_flag, 0);
	spi_set_drvdata(sdev, cd);
	rc = memset_s(cd->tx_buf, sizeof(cd->tx_buf), 0x00, sizeof(cd->tx_buf));
	if (rc != EOK)
		return -ENODEV;
	g_smc_core = cd;
  
	ret = sysfs_create_group(&sdev->dev.kobj,&dev_attr_group);
	if (ret < 0)
          printk(KERN_INFO"sysfs_create_group failed\n");
	printk(KERN_INFO"%s: out\n", __func__);
	return 0;
}

static void smc_remove(struct spi_device *sdev)
{
	struct smc_core_data *cd = spi_get_drvdata(sdev);

	printk(KERN_INFO"%s: in\n", __func__);
	mutex_destroy(&cd->spi_mutex);
	kfree(cd);
	cd = NULL;
	sysfs_remove_group(&sdev->dev.kobj,&dev_attr_group);
}

const struct of_device_id g_rsmc_psoc_match_table_x801[] = {
	{.compatible = "xiaomi_rsmc"},
	{},
};
EXPORT_SYMBOL_GPL(g_rsmc_psoc_match_table_x801);

static const struct spi_device_id g_rsmc_device_id_x801[] = {
	{RSMC_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(spi, g_rsmc_device_id_x801);

static struct spi_driver g_rsmc_spi_driver = {
	.probe = smc_probe,
	.remove = smc_remove,
	.id_table = g_rsmc_device_id_x801,
	.driver = {
		.name = RSMC_DEVICE_NAME,
		.owner = THIS_MODULE,
		.bus = &spi_bus_type,
		.of_match_table = of_match_ptr(g_rsmc_psoc_match_table_x801),
	},
};

int rsmc_spi_init_x801(void)
{
	int ret = spi_register_driver(&g_rsmc_spi_driver);

	printk(KERN_INFO"%s: call spi_register_driver ret %d", __func__, ret);
	return ret;
}

void rsmc_spi_exit_x801(void)
{
	spi_unregister_driver(&g_rsmc_spi_driver);
}

