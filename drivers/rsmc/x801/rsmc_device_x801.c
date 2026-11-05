#include "rsmc_device_x801.h"

//#include <securec.h>
#include "rsmc_stdlib_s.h"
//#include <cetc_platform/log/cetc_log.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <asm/string.h>
#include <linux/thread_info.h>
#include <asm/byteorder.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <asm/delay.h>
//#include <asm/memory.h>
#include <linux/kobject.h> 
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/etherdevice.h> 
#include <linux/version.h>
#include <linux/math64.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
//#include <chipset_common/hwpower/hardware_ic/buck_boost.h>
#include "../module_type.h"
#include "rsmc_spi_x801.h"

#ifdef CETCLOG_TAG
#undef CETCLOG_TAG
#endif
#define CETCLOG_TAG RSMC_DEVICE

#define CONFIG_PLL_TIMES 5
#define BOOT_HANDSHAKE_RETRY_TIMES 5
#define BOOT_HANDSHAKE_RETRY_DELAY_TIME 10
#define BOOT_RETRY_TIMES 5
#define DEVICE_REG_TRY_MAX_COUNT 100
#define DEVICE_REG_SLEEP_INTERVAL 1000
#define FW_REQ_BYTE 0x55
#define FW_CNF_BYTE 0xaa
#define FW_BLOCK_HEAD_LEN 2
#define FW_BLOCK_SIZE 254
#define FW_TRANS_BLOCK_SIZE 256
#define FW_SPI_BUFFER_SIZE 128
#define SPI_REV_START_WRITE_NUM 2
#define SPI_MAX_FRAME_LEN 128
#define FW_SPI_HEAD_DATA 0x5e
#define FW_SPI_HEAD_IDX 0
#define FW_SPI_LEN_IDX 1
#define FW_SPI_ID_IDX 2
#define FW_SPI_DATA_IDX 3
#define FW_SPI_UDELAY 50
#define WAIT_STA_RETRY_TIMES 2
#define WAIT_STA_TIMEOUT (500 * HZ / 1000)

#define STA_NONE 0
#define STA_SUCC 1
#define STA_FAIL 2

static void rsmc_spi_recv(void);
static void rsmc_disable(bool send_ack);

struct rsmc_device_ctx {
	notify_event *notifier;
	bool is_chip_on;
	struct mutex access_mutex;
	struct task_struct *firmware_download_task;
	u32 ant_status;
	u32 dtx_status;
	struct timer_list wait_sta_timer;
	bool recv_sta;
	u32 wait_sta_retry_times;
	u32 cause; // RSMC_ENABLE_CAUSE
};
static struct rsmc_device_ctx g_device_ctx;

struct rsmc_spi_ctx {
	struct task_struct *task;
	struct semaphore sema;
	volatile u32 spi_status;
	struct timer_list spi_recv_timer;
	volatile bool spi_recv_timer_running;
	bool rsmc_spi_exit;
	atomic_t spi_irq;
};
static struct rsmc_spi_ctx g_rsmc_spi_ctx = {0};

struct rsmc_fw_ctx {
	u8 *fw_buffer;
	u32 write_cnt;
	u32 file_len;
	u32 every_block_len;
};
static struct rsmc_fw_ctx g_rsmc_fw_ctx = {0};

struct sta_info {
	char soc_ver[SPI_MAX_FRAME_LEN];
	char fw_ver[SPI_MAX_FRAME_LEN];
	u32 cpu_freq;
	u32 aux_adc;
	u32 rfspi_check;
	u32 bbreg_check;
	u32 agc;
	u32 power;
	u32 polar_check;
};

long long current_Num = 0;
unsigned char current_ServerID = 0;
int is_sending  = 0;
int is_sent_last = 0;

u32 get_spi_status(void)
{
	return g_rsmc_spi_ctx.spi_status;
}

void set_spi_status(u32 spi_status)
{
	g_rsmc_spi_ctx.spi_status = spi_status;
}

bool get_chip_on_status(void)
{
	return g_device_ctx.is_chip_on;
}

void send_msg_to_ctrl_x801(struct msg_head *msg)
{
	if (msg == NULL)
		return;
	if (g_device_ctx.notifier == NULL)
		return;
	g_device_ctx.notifier(msg);
}

static int rsmc_tcxo_power_ctrl(struct smc_core_data *cd, bool status)
{
	struct rsmc_power_supply *power = NULL;
	int rc = 0;
	int gpio_status = status ? GPIO_HIGH : GPIO_LOW;

	power = &cd->rsmc_powers;
	if (power->type == RSMC_POWER_UNUSED) {
		printk(KERN_ERR"%s: regulator %s not gotten yet\n", __func__, "tcxo-vcc");
		return -ENODEV;
	}

	printk(KERN_INFO"%s:power %s %d\n", __func__, "tcxo-vcc status: ", status ? 1 : 0);

	if (!power->use_count) {
		printk(KERN_ERR"%s: power use_count is invalid\n", __func__);
		return -ENODEV;
	}
	switch (power->type) {
	case RSMC_POWER_LDO:
		if (IS_ERR_OR_NULL(power->regulator)) {
			printk(KERN_ERR"%s:fail to get %s\n", __func__, "tcxo-vcc");
			return -ENODEV;
		}
		rc = status ? regulator_enable(power->regulator) :
			regulator_disable(power->regulator);
		printk(KERN_INFO"%s: tcxo-power-vcc enable result: %d\n", __func__, rc);
		break;
	case RSMC_POWER_GPIO:
		if (gpio_is_valid(power->gpio))
			gpio_set_value(power->gpio, gpio_status);
		break;
	default:
		printk(KERN_ERR"%s: invalid power type %d\n", __func__, power->type);
		break;
	}
	return rc;
}

static bool config_enable_spi(void)
{
	int rc;
	u32 value, val, retry_times;

	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		rc = rsmc_spi_reg_write(REG_00, REG00_VALUE);
		printk(KERN_INFO"%s: reg00 config 0x527091", __func__);
		mdelay(1);
		rc = rsmc_spi_reg_read(REG_00, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg00 check 0x%x,0x%x", __func__, value, val);
		if ((value == REG00_VALUE) || (val == REG00_VALUE))
			break;
		retry_times--;
	}
	if (retry_times == 0)
		return false;
	return true;
}

static bool config_enable_pll_reset(void)
{
	int rc;
	u32 value, val, retry_times;

	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		rc = rsmc_spi_reg_write(REG_01, 1);
		printk(KERN_INFO"%s: reg01 config 0x1", __func__);
		mdelay(1);
		rc = rsmc_spi_reg_read(REG_01, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg01 check 0x%x,0x%x", __func__, value, val);
		if ((value == 1) || (val == 1))
			break;
		retry_times--;
	}
	if (retry_times == 0)
		return false;
	return true;
}

static bool config_pll(void)
{
	int rc;
	u32 value, val, retry_times;

	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		rc = rsmc_spi_reg_write(REG_03, REG03_VALUE);
		printk(KERN_INFO"%s: reg03 config 0x465", __func__);
		mdelay(1);
		rc = rsmc_spi_reg_read(REG_03, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg03 check 0x%x,0x%x", __func__, value, val);
		if ((value == REG03_VALUE) || (val == REG03_VALUE))
			break;
		retry_times--;
	}
	if (retry_times == 0)
		return false;
	return true;
}

static bool config_clock(void)
{
	int rc;
	u32 value, val, retry_times;

	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		rc = rsmc_spi_reg_write(REG_04, REG04_VALUE);
		printk(KERN_INFO"%s: reg04 config 0x476", __func__);
		mdelay(1);
		rc = rsmc_spi_reg_read(REG_04, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg04 check 0x%x,0x%x", __func__, value, val);
		if ((value == REG04_VALUE) || (val == REG04_VALUE))
			break;
		retry_times--;
	}
	if (retry_times == 0)
		return false;
	return true;
}

static bool config_disable_pll_reset(void)
{
	int rc;
	u32 value, val, retry_times;

	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		rc = rsmc_spi_reg_write(REG_01, 0);
		printk(KERN_INFO"%s: reg01 config 0x0", __func__);
		mdelay(1);
		rc = rsmc_spi_reg_read(REG_01, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg01 check 0x%x,0x%x", __func__, value, val);
		if ((value == 0) || (val == 0))
			break;
		retry_times--;
	}
	if (retry_times == 0)
		return false;
	return true;
}

static bool config_26m_pll(void)
{
	int rc;
	u32 value, val, retry_times;
	u32 i;
	for (i = REG_00; i <= REG_06; i++) {
		rsmc_spi_reg_read(i, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: addr:%d,value:0x%x,0x%x", __func__, i, value, val);
	}
	// enable spi config
	if (!config_enable_spi())
		return false;

	// enable pll reset
	if (!config_enable_pll_reset())
		return false;

	// config pll
	if (!config_pll())
		return false;

	// config clock
	if (!config_clock())
		return false;

	// disable pll reset
	if (!config_disable_pll_reset())
		return false;

	mdelay(1);
	// status
	retry_times = CONFIG_PLL_TIMES;
	while (retry_times > 0) {
		struct reg02_status_stru *sts = (struct reg02_status_stru *)&value;
		struct reg02_status_stru *sts2 = (struct reg02_status_stru *)&val;
		rc = rsmc_spi_reg_read(REG_02, &value);
		val = value >> 1;
		printk(KERN_INFO"%s: reg02 value:0x%x,pll:%d,val:0x%x,pll:%d", __func__, value, sts->pll, val, sts2->pll);
		if ((sts->pll == 1) || (sts2->pll == 1))
			break;
		retry_times--;
		mdelay(1);
	}
	if (retry_times == 0)
		return false;
	return true;
}

static int smc_setup_pa_soc_pwr_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio)) {
		rc = gpio_request(cd->gpios.pa_pwr_gpio, "rsmc_pa_pwr");
		if (rc) {
			printk(KERN_ERR"%s: pa_pwr_gpio %d request failed %d\n",
				__func__, cd->gpios.pa_pwr_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.pa_pwr_gpio);
		printk(KERN_INFO"%s: pa_soc_pwr_gpio %d value %d",
			__func__, cd->gpios.pa_pwr_gpio, rc);

		rc = gpio_direction_output(cd->gpios.pa_pwr_gpio, GPIO_LOW);
		if (rc) {
			printk(KERN_ERR"%s: pa_soc_pwr_gpio %d output high err %d",
				__func__, cd->gpios.pa_pwr_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.pa_pwr_gpio);
		printk(KERN_INFO"%s: pa_soc_pwr_gpio %d value %d",
			__func__, cd->gpios.pa_pwr_gpio, rc);
	}
	if (gpio_is_valid(cd->gpios.pa_enable_gpio)) {
		rc = gpio_request(cd->gpios.pa_enable_gpio, "rsmc_pa_pwr_enable");
		if (rc) {
			printk(KERN_ERR"%s: pa_enable_gpio %d request failed %d\n",
				__func__, cd->gpios.pa_enable_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.pa_enable_gpio);
		printk(KERN_INFO"%s: pa_enable_gpio %d value %d",
			__func__, cd->gpios.pa_enable_gpio, rc);

		rc = gpio_direction_output(cd->gpios.pa_enable_gpio, GPIO_HIGH);
		if (rc) {
			printk(KERN_ERR"%s: pa_enable_gpio %d output high err %d",
				__func__, cd->gpios.pa_enable_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.pa_enable_gpio);
		printk(KERN_INFO"%s: pa_enable_gpio %d value %d",
			__func__, cd->gpios.pa_enable_gpio, rc);
	}
	return 0;
}

static int smc_setup_vc_ant_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.vc1_ant_gpio)) {
		rc = gpio_request(cd->gpios.vc1_ant_gpio, "rsmc_vc1_ant");
		if (rc) {
			printk(KERN_ERR"%s: vc1 %d reqfail %d", __func__, cd->gpios.vc1_ant_gpio, rc);
			return rc;
		}

		rc = gpio_direction_output(cd->gpios.vc1_ant_gpio, cd->gpios.vc1_default_value);
		if (rc) {
			printk(KERN_ERR"%s: vc1 %d output %d", __func__, cd->gpios.vc1_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vc1_ant_gpio);
		printk(KERN_INFO"%s: vc1_ant_gpio %d value %d", __func__, cd->gpios.vc1_ant_gpio, rc);
	}
	if (gpio_is_valid(cd->gpios.vc2_ant_gpio)) {
		rc = gpio_request(cd->gpios.vc2_ant_gpio, "rsmc_vc2_ant");
		if (rc) {
			printk(KERN_ERR"%s: vc2 %d reqfail %d", __func__, cd->gpios.vc2_ant_gpio, rc);
			return rc;
		}

		rc = gpio_direction_output(cd->gpios.vc2_ant_gpio, cd->gpios.vc2_default_value);
		if (rc) {
			printk(KERN_ERR"%s: vc2 %d output %d", __func__, cd->gpios.vc2_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vc2_ant_gpio);
		printk(KERN_INFO"%s: vc2_ant_gpio %d value %d", __func__, cd->gpios.vc2_ant_gpio, rc);
	}
	if (gpio_is_valid(cd->gpios.vc3_ant_gpio)) {
		rc = gpio_request(cd->gpios.vc3_ant_gpio, "rsmc_vc3_ant");
		if (rc) {
			printk(KERN_ERR"%s: vc3 %d reqfail %d", __func__, cd->gpios.vc3_ant_gpio, rc);
			return rc;
		}

		rc = gpio_direction_output(cd->gpios.vc3_ant_gpio, cd->gpios.vc3_default_value);
		if (rc) {
			printk(KERN_ERR"%s: vc3 %d output %d", __func__, cd->gpios.vc3_ant_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vc3_ant_gpio);
		printk(KERN_INFO"%s: vc3_ant_gpio %d value %d", __func__, cd->gpios.vc3_ant_gpio, rc);
	}
	return 0;
}

static int smc_setup_sel_gpio(struct smc_core_data *cd)
{
	int rc;

	if (cd == NULL)
		return -EINVAL;
	if (gpio_is_valid(cd->gpios.tt_bd_sel_gpio)) {
		rc = gpio_request(cd->gpios.tt_bd_sel_gpio, "tt_bd_sel_gpio");
		if (rc) {
			printk(KERN_ERR"%s: tt_bd_sel_gpio %d request failed %d\n",
				__func__, cd->gpios.tt_bd_sel_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.tt_bd_sel_gpio);
		printk(KERN_INFO"%s: tt_bd_sel_gpio %d value %d",
			__func__, cd->gpios.tt_bd_sel_gpio, rc);

		rc = gpio_direction_output(cd->gpios.tt_bd_sel_gpio, GPIO_HIGH);
		if (rc) {
			printk(KERN_ERR"%s: tt_bd_sel_gpio %d output high err %d",
				__func__, cd->gpios.tt_bd_sel_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vdet_sel_gpio);
		printk(KERN_INFO"%s: vdet_sel_gpio %d value %d",
			__func__, cd->gpios.vdet_sel_gpio, rc);
	}
	if (gpio_is_valid(cd->gpios.vdet_sel_gpio)) {
		rc = gpio_request(cd->gpios.vdet_sel_gpio, "vdet_sel_gpio");
		if (rc) {
			printk(KERN_ERR"%s: vdet_sel_gpio %d request failed %d\n",
				__func__, cd->gpios.vdet_sel_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vdet_sel_gpio);
		printk(KERN_INFO"%s: vdet_sel_gpio %d value %d",
			__func__, cd->gpios.vdet_sel_gpio, rc);

		rc = gpio_direction_output(cd->gpios.vdet_sel_gpio, GPIO_HIGH);
		if (rc) {
			printk(KERN_ERR"%s: vdet_sel_gpio %d output high err %d",
				__func__, cd->gpios.vdet_sel_gpio, rc);
			return rc;
		}

		rc = gpio_get_value(cd->gpios.vdet_sel_gpio);
		printk(KERN_INFO"%s: vdet_sel_gpio %d value %d",
			__func__, cd->gpios.vdet_sel_gpio, rc);
	}
	return 0;
}

static int chip_enable(struct smc_core_data *cd)
{
	int ret;

	ret = rsmc_tcxo_power_ctrl(cd, true);
	if (ret)
		return ret;
	rsmc_spi_cs_set(GPIO_HIGH);
	smc_setup_pa_soc_pwr_gpio(cd);
	smc_setup_vc_ant_gpio(cd);
	smc_setup_sel_gpio(cd);
	if (cd->rsmc_powers.tcxo_pwr_after_delay_ms > 0)
		mdelay(cd->rsmc_powers.tcxo_pwr_after_delay_ms);
	if (gpio_is_valid(cd->gpios.soc_pwr_gpio))
		gpio_set_value(cd->gpios.soc_pwr_gpio, GPIO_HIGH);
	if (gpio_is_valid(cd->gpios.soc_en_gpio))
		gpio_set_value(cd->gpios.soc_en_gpio, GPIO_HIGH);
	mdelay(ENABLE_SIGNAL_DELAY_MS);
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio)) {
		gpio_set_value(cd->gpios.pa_pwr_gpio, GPIO_HIGH);
	} else {
		if(!regulator_is_enabled(cd->pa_power)) {
			regulator_set_voltage(cd->pa_power , 4500000, 4500000);
			ret = regulator_enable(cd->pa_power);
			printk(KERN_INFO"rsmc enable rsmc-pa-dcdc ret %d.\n", ret);
		} else {
			printk(KERN_INFO"%s: rsmc-pa-dcdc already enabled\r\n", __func__);
		}
	}
	rsmc_spi_set_min_speed();
	config_26m_pll();
	rsmc_spi_set_max_speed();
	if (gpio_is_valid(cd->gpios.mcu_reset_gpio))
		gpio_set_value(cd->gpios.mcu_reset_gpio, GPIO_HIGH);
	mdelay(MCU_RST_SIGNAL_DELAY_MS);
	return 0;
}

static int chip_disable(struct smc_core_data *cd)
{
	int ret;

	if (cd == NULL)
		return -EINVAL;
	rsmc_spi_cs_set(GPIO_LOW);
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio)) {
		gpio_set_value(cd->gpios.pa_pwr_gpio, GPIO_LOW);
	} else {
		if(regulator_is_enabled(cd->pa_power)) {
			ret = regulator_disable(cd->pa_power);
			regulator_set_voltage(cd->pa_power , 0, 4600000);
			printk(KERN_INFO"rsmc disable rsmc-pa-dcdc ret %d.\n", ret);
		} else {
			printk(KERN_INFO"%s: rsmc-pa-dcdc already disabled\r\n", __func__);
		}
	}
	if (gpio_is_valid(cd->gpios.mcu_reset_gpio))
		gpio_set_value(cd->gpios.mcu_reset_gpio, GPIO_LOW);
	mdelay(ENABLE_SIGNAL_DELAY_MS);
	if (gpio_is_valid(cd->gpios.soc_en_gpio))
		gpio_set_value(cd->gpios.soc_en_gpio, GPIO_LOW);
	mdelay(ENABLE_SIGNAL_DELAY_MS);
	if (gpio_is_valid(cd->gpios.soc_pwr_gpio))
		gpio_set_value(cd->gpios.soc_pwr_gpio, GPIO_LOW);
	ret = rsmc_tcxo_power_ctrl(cd, false);
	if (gpio_is_valid(cd->gpios.vc1_ant_gpio))
		gpio_free(cd->gpios.vc1_ant_gpio);
	if (gpio_is_valid(cd->gpios.vc2_ant_gpio))
		gpio_free(cd->gpios.vc2_ant_gpio);
	if (gpio_is_valid(cd->gpios.tt_bd_sel_gpio))
		gpio_free(cd->gpios.tt_bd_sel_gpio);
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio))
		gpio_free(cd->gpios.pa_pwr_gpio);
	if (gpio_is_valid(cd->gpios.vdet_sel_gpio))
		gpio_free(cd->gpios.vdet_sel_gpio);
	if (gpio_is_valid(cd->gpios.pa_enable_gpio))
		gpio_free(cd->gpios.pa_enable_gpio);
	return ret;
}

static int chip_power_hw(bool status, struct smc_core_data *cd)
{
	int ret;
	if (status) {
		ret = chip_enable(cd);
	} else {
		ret = chip_disable(cd);
	}
	return ret;
}

static void chip_power_on(bool status)
{
	struct smc_core_data *cd = smc_get_core_data_x801();
	int value, ret;

	if (cd == NULL)
		return;
	printk(KERN_INFO"%s: status %d", __func__, (int)status);
	ret = chip_power_hw(status, cd);
	if (ret) {
		printk(KERN_ERR"%s: chip_power_hardware_ctrl error, ret %d", __func__, ret);
		return;
	}
	if (gpio_is_valid(cd->gpios.soc_pwr_gpio)) {
		value = gpio_get_value(cd->gpios.soc_pwr_gpio);
		printk(KERN_INFO"%s: soc_pwr_gpio value %d", __func__, value);
	}
	if (gpio_is_valid(cd->gpios.soc_en_gpio)) {
		value = gpio_get_value(cd->gpios.soc_en_gpio);
		printk(KERN_INFO"%s: soc_en_gpio value %d", __func__, value);
	}
	if (gpio_is_valid(cd->gpios.cs_gpio)) {
		value = gpio_get_value(cd->gpios.cs_gpio);
		printk(KERN_INFO"%s: cs_gpio value %d", __func__, value);
	}
	if (gpio_is_valid(cd->gpios.pa_pwr_gpio)) {
		value = gpio_get_value(cd->gpios.pa_pwr_gpio);
		printk(KERN_INFO"%s: pa_pwr_gpio value %d", __func__, value);
	}
	if (gpio_is_valid(cd->gpios.mcu_reset_gpio)) {
		value = gpio_get_value(cd->gpios.mcu_reset_gpio);
		printk(KERN_INFO"%s: mcu_reset_gpio value %d", __func__, value);
	}
}

static void print_data(u8 *buff, u32 len, char *tag)
{
	const u32 offset_limit = 800;
	u32 i;
	u32 offset = 0;
	int ret;
	int buff_len = len * 2 + 10; // buffer 10

	char *out = kmalloc(buff_len, GFP_ATOMIC);
	if (out == NULL) {
		printk(KERN_ERR"%s: out mem malloc error", __func__);
		return;
	}

	for (i = 0; i < len; i++) {
		ret = sprintf(out + offset,
			"%02x", buff[i] & 0xFF);
		if (ret < 0) {
			printk(KERN_INFO"buff gen is error");
			break;
		}
		offset += ret;
		if (offset >= offset_limit) {
			out[offset] = 0;
			printk(KERN_INFO"%s:%s", tag, out);
			offset = 0;
		}
	}
	out[offset] = 0;
	printk(KERN_INFO"%s:%s", tag, out);
	kfree(out);
}

static bool fw_device_start_handshake(void)
{
	u8 rx_value, tx_value;
	u32 retry = BOOT_HANDSHAKE_RETRY_TIMES;

	while (retry > 0) {
		rx_value = 0;
		rsmc_spi_read_x801(&rx_value, 1);
		printk(KERN_INFO"%s: X801:dload << %c", __func__, rx_value);
		if (rx_value != '$') {
			retry--;
			mdelay(BOOT_HANDSHAKE_RETRY_DELAY_TIME);
			continue;
		}
		printk(KERN_INFO"%s: X801:dload >> 0xaa", __func__);
		tx_value = FW_CNF_BYTE;
		rsmc_spi_write_x801(&tx_value, 1);
		rx_value = 0;
		rsmc_spi_read_x801(&rx_value, 1);
		printk(KERN_INFO"%s: X801:dload << 0x%x", __func__, rx_value);
		if (rx_value != FW_REQ_BYTE) {
			g_device_ctx.cause = EN_FAIL_HANDSHAKE_FAIL;
			return false;
		}
		printk(KERN_INFO"%s: X801:dload >> 0x55", __func__);
		tx_value = FW_REQ_BYTE;
		rsmc_spi_write_x801(&tx_value, 1);
		rx_value = 0;
		rsmc_spi_read_x801(&rx_value, 1);
		printk(KERN_INFO"%s: X801:dload << 0x%x", __func__, rx_value);
		if (rx_value != FW_CNF_BYTE) {
			printk(KERN_ERR"%s: X801:dload handshake fail", __func__);
			g_device_ctx.cause = EN_FAIL_HANDSHAKE_FAIL;
			return false;
		}
		printk(KERN_INFO"%s: X801:dload handshake succ", __func__);
		return true;
	}
	g_device_ctx.cause = EN_FAIL_NOT_RECV_HT;
	return false;
}

static bool fw_device_end_handshake(void)
{
	u8 tx_value = FW_REQ_BYTE;
#ifdef NOT_IGNORE_HS_ACK
	bool ret = false;
	u8 rx_value = 0;
#endif
	printk(KERN_INFO"%s: X801:dload >> 0x55", __func__);
	rsmc_spi_write_x801(&tx_value, 1);
#ifdef NOT_IGNORE_HS_ACK
	rsmc_spi_read_x801(&rx_value, 1);
	printk(KERN_INFO"%s: X801:dload << 0x%x", __func__, rx_value);
	ret = (rx_value == FW_CNF_BYTE) ? true : false;
	return ret;
#else
	return true;
#endif
}

static bool fw_block_write_crc(u32 cnt, const u8 *data, u32 len)
{
	u8 crc;
	u8 rx_value;
	u8 tx_buffer[FW_TRANS_BLOCK_SIZE] = {0};
	u32 j;
	u32 pos = 0;
	u32 cur_len = FW_BLOCK_SIZE;
	u32 index = 1;
#ifdef FW_DIV_BLOCK
	u32 trans_len;
#endif
	while (1) {
		crc = 0;
		memset_s(tx_buffer, sizeof(tx_buffer), 0x00, sizeof(tx_buffer));
		// write start symbol
		tx_buffer[0] = (index == cnt) ? '*' : '$';
		crc ^= tx_buffer[0];
		if (index == cnt)
			cur_len = len - (index - 1) * FW_BLOCK_SIZE;
		// write data length
		tx_buffer[1] = (u8)(cur_len + 1);
		crc ^= tx_buffer[1];
		// write a block data
		for (j = 0; j < cur_len; j++) {
			tx_buffer[j + FW_BLOCK_HEAD_LEN] = data[pos + j];
			crc ^= tx_buffer[j + FW_BLOCK_HEAD_LEN];
		}
#ifdef FW_DIV_BLOCK
		trans_len = (cur_len + FW_BLOCK_HEAD_LEN) * FW_SPI_BUFFER_SIZE / FW_TRANS_BLOCK_SIZE;
		rsmc_spi_write_x801(&tx_buffer[0], trans_len);
		udelay(FW_SPI_UDELAY);
		rsmc_spi_write_x801(&tx_buffer[trans_len], cur_len + FW_BLOCK_HEAD_LEN - trans_len);
		udelay(FW_SPI_UDELAY);
#else
		rsmc_spi_write_x801(&tx_buffer[0], cur_len + FW_BLOCK_HEAD_LEN);
#endif
		// read CRC result
		rx_value = 0;
		rsmc_spi_read_x801(&rx_value, 1);
		if (rx_value != crc) {
			printk(KERN_ERR"%s: X801:dload id:%d/%d,crc:%d/%d", __func__, index, cnt, rx_value, crc);
			break;
		}
		if (index >= cnt)
			return fw_device_end_handshake();
		pos += cur_len;
		index++;
	}
	g_device_ctx.cause = EN_FAIL_FW_CRC_FAIL;
	return false;
}

static bool rsmc_firmware_download_proc(void)
{
	u32 write_cnt;
	u32 file_len = g_rsmc_fw_ctx.file_len;
	bool ret = false;

	printk(KERN_INFO"%s: rsmc fw start, file_len:%d", __func__, file_len);
	if (!fw_device_start_handshake()) {
		printk(KERN_ERR"%s: fw_device_start_handshake fail", __func__);
		goto fw_return;
	}
	write_cnt = ((file_len % FW_BLOCK_SIZE == 0) ? (file_len / FW_BLOCK_SIZE) : (file_len / FW_BLOCK_SIZE + 1));
	if (g_rsmc_fw_ctx.fw_buffer == NULL) {
		printk(KERN_ERR"%s: fw_buffer is null\n", __func__);
		goto fw_return;
	}
	ret = fw_block_write_crc(write_cnt, g_rsmc_fw_ctx.fw_buffer, file_len);
fw_return:
	if (ret)
		printk(KERN_INFO"%s: firmware download success", __func__);
	else
		printk(KERN_ERR"%s: firmware download failure", __func__);
	return ret;
}

static void rsmc_wait_sta_timeout(struct timer_list *t)
{
	struct msg_head msg = {0};
	printk(KERN_INFO"%s", __func__);
	msg.type = CMD_INTER_WAIT_STA_TIMEOUT;
	msg.module = MODULE_TYPE_KNL;
	msg.len = sizeof(struct msg_head);
	rsmc_nl_dn_msg_notify_inter_nonblock((struct msg_head *)&msg);
}

static void rsmc_start_wait_sta_timer(void)
{
	printk(KERN_INFO"%s", __func__);
	timer_setup(&g_device_ctx.wait_sta_timer, rsmc_wait_sta_timeout, TIMER_IRQSAFE);
	g_device_ctx.wait_sta_timer.expires = jiffies + WAIT_STA_TIMEOUT;
	add_timer(&g_device_ctx.wait_sta_timer);
}

static bool rsmc_firmware_download(void)
{
	u32 retry_times = BOOT_RETRY_TIMES;
	printk(KERN_INFO"%s", __func__);
	while (retry_times > 0) {
		chip_power_on(true);
		if (rsmc_firmware_download_proc()) {
			g_device_ctx.is_chip_on = true;
			set_spi_status(SPI_NORMAL);
			rsmc_spi_irq_ctl_x801(true);
			rsmc_start_wait_sta_timer();
			return true;
		} else {
			chip_power_on(false);
			g_device_ctx.is_chip_on = false;
			retry_times--;
		}
	}
	return false;
}

static void rsmc_stop_wait_sta_timer(void)
{
	printk(KERN_INFO"%s", __func__);
	del_timer_sync(&g_device_ctx.wait_sta_timer);
	if (g_device_ctx.recv_sta) {
		if (g_rsmc_fw_ctx.fw_buffer != NULL)
			vfree(g_rsmc_fw_ctx.fw_buffer);
		g_rsmc_fw_ctx.fw_buffer = NULL;
	} else {
		struct enable_cnf_msg cnf_msg;
		cnf_msg.head.type = CMD_UP_INIT_CNF;
		cnf_msg.head.module = MODULE_TYPE_CTRL;
		cnf_msg.head.len = sizeof(struct enable_cnf_msg);
		cnf_msg.type = 1;
		cnf_msg.result = 0;
		printk(KERN_INFO"%s: retry remain %d times", __func__, g_device_ctx.wait_sta_retry_times);
		if (g_device_ctx.wait_sta_retry_times == 0) {
			if (g_rsmc_fw_ctx.fw_buffer != NULL)
				vfree(g_rsmc_fw_ctx.fw_buffer);
			g_rsmc_fw_ctx.fw_buffer = NULL;
			g_device_ctx.is_chip_on = true;
			rsmc_disable(false);
			cnf_msg.cause = EN_FAIL_NO_STA;
			send_msg_to_ctrl_x801((struct msg_head *)&cnf_msg);
			return;
		}
		g_device_ctx.wait_sta_retry_times--;
		rsmc_spi_irq_ctl_x801(false);
		atomic_inc(&g_rsmc_spi_ctx.spi_irq);
		rsmc_spi_recv();
		chip_power_on(false);
		g_device_ctx.is_chip_on = false;
		if (!rsmc_firmware_download()) {
			if (g_rsmc_fw_ctx.fw_buffer != NULL)
				vfree(g_rsmc_fw_ctx.fw_buffer);
			g_rsmc_fw_ctx.fw_buffer = NULL;
			cnf_msg.cause = g_device_ctx.cause;
			send_msg_to_ctrl_x801((struct msg_head *)&cnf_msg);
		}
	}
}

static void get_ant_gpio_value(const struct smc_core_data *cd)
{
	u32 get_value;

	if (cd == NULL)
		return;

	if (gpio_is_valid(cd->gpios.vc1_ant_gpio)) {
		get_value = gpio_get_value(cd->gpios.vc1_ant_gpio);
		printk(KERN_INFO"%s: vc1_ant_gpio %d get:%d",
			__func__, cd->gpios.vc1_ant_gpio, get_value);
	}

	if (gpio_is_valid(cd->gpios.vc2_ant_gpio)) {
		get_value = gpio_get_value(cd->gpios.vc2_ant_gpio);
		printk(KERN_INFO"%s: vc2_ant_gpio %d get:%d",
			__func__, cd->gpios.vc2_ant_gpio, get_value);
	}

	if (gpio_is_valid(cd->gpios.vc3_ant_gpio)) {
		get_value = gpio_get_value(cd->gpios.vc3_ant_gpio);
		printk(KERN_INFO"%s: vc3_ant_gpio %d get:%d",
			__func__, cd->gpios.vc3_ant_gpio, get_value);
	}

	if (gpio_is_valid(cd->gpios.tx_ant_gpio)) {
		get_value = gpio_get_value(cd->gpios.tx_ant_gpio);
		printk(KERN_INFO"%s: tx_ant_gpio %d get:%d",
			__func__, cd->gpios.tx_ant_gpio, get_value);
	}

	if (gpio_is_valid(cd->gpios.rx_ant_gpio)) {
		get_value = gpio_get_value(cd->gpios.rx_ant_gpio);
		printk(KERN_INFO"%s: rx_ant_gpio %d get:%d",
			__func__, cd->gpios.rx_ant_gpio, get_value);
	}
}

static void enable_default_ant(void)
{
	if(is_pic_mode != 0){
		printk(KERN_INFO"@law not default");
		return;
	}
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return;
	printk(KERN_INFO"@ant %s", __func__);
	g_device_ctx.ant_status = RSMC_RNSS_RX;
	get_ant_gpio_value(cd);
	if (gpio_is_valid(cd->gpios.rx_ant_gpio))
		gpio_set_value(cd->gpios.rx_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.tx_ant_gpio))
		gpio_set_value(cd->gpios.tx_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.vc1_ant_gpio))
		gpio_set_value(cd->gpios.vc1_ant_gpio, cd->gpios.vc1_default_value);
	if (gpio_is_valid(cd->gpios.vc2_ant_gpio))
		gpio_set_value(cd->gpios.vc2_ant_gpio, cd->gpios.vc2_default_value);
	if (gpio_is_valid(cd->gpios.vc3_ant_gpio))
		gpio_set_value(cd->gpios.vc3_ant_gpio, cd->gpios.vc3_default_value);
	get_ant_gpio_value(cd);
}

static void enable_tx_ant(void)
{
	if(is_pic_mode == 2 && is_sent_last == 1){
		printk(KERN_INFO"@law not tx");
		return;
	}
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return;
	printk(KERN_INFO"@ant %s", __func__);
	g_device_ctx.ant_status = RSMC_RDSS_TX;
	get_ant_gpio_value(cd);
	if (gpio_is_valid(cd->gpios.vc1_ant_gpio))
		gpio_set_value(cd->gpios.vc1_ant_gpio, GPIO_HIGH);
	if (gpio_is_valid(cd->gpios.vc2_ant_gpio))
		gpio_set_value(cd->gpios.vc2_ant_gpio, GPIO_HIGH);
	if (gpio_is_valid(cd->gpios.vc3_ant_gpio))
		gpio_set_value(cd->gpios.vc3_ant_gpio, !cd->gpios.vc3_default_value);
	if (gpio_is_valid(cd->gpios.rx_ant_gpio))
		gpio_set_value(cd->gpios.rx_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.tx_ant_gpio))
		gpio_set_value(cd->gpios.tx_ant_gpio, GPIO_HIGH);
	get_ant_gpio_value(cd);
}

static void enable_rx_ant(void)
{
	struct smc_core_data *cd = smc_get_core_data_x801();
	if (cd == NULL)
		return;
	printk(KERN_INFO"@ant %s", __func__);
	g_device_ctx.ant_status = RSMC_RDSS_RX;
	get_ant_gpio_value(cd);
	if (gpio_is_valid(cd->gpios.vc1_ant_gpio))
		gpio_set_value(cd->gpios.vc1_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.vc2_ant_gpio))
		gpio_set_value(cd->gpios.vc2_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.vc3_ant_gpio))
		gpio_set_value(cd->gpios.vc3_ant_gpio, !cd->gpios.vc3_default_value);
	if (gpio_is_valid(cd->gpios.tx_ant_gpio))
		gpio_set_value(cd->gpios.tx_ant_gpio, GPIO_LOW);
	if (gpio_is_valid(cd->gpios.rx_ant_gpio))
		gpio_set_value(cd->gpios.rx_ant_gpio, GPIO_HIGH);
	get_ant_gpio_value(cd);
}

static void dtx_rsp_ant_ctrl(const char *data)
{
	int ret, res;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$DTX");
	if (cmd == NULL)
		return;

	ret = sscanf(cmd, "$DTX,%d", &res);
	if (ret <= 0) {
		printk(KERN_ERR"%s: DTX param fail", __func__);
		return;
	}
	if (res == CMD_TX_CMD_ACK){
          	printk(KERN_INFO"%s @law do not change ant", __func__);
		return;
        }
	if (g_device_ctx.dtx_status == RSMC_RNSS_RX_AFTER_DTX)
		enable_default_ant();
	else if (g_device_ctx.dtx_status == RSMC_RDSS_RX_AFTER_DTX)
		enable_rx_ant();
}

static void stx_rsp_ant_ctrl(const char *data)
{
	int ret, res;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$STX");
	if (cmd == NULL)
		return;
	ret = sscanf(cmd, "$STX,%d", &res);
	if (ret <= 0) {
		printk(KERN_ERR"%s: STX param fail", __func__);
		return;
	}
	if (res != CMD_TX_CMD_ACK)
		enable_default_ant();
}

static void ac3_rsp_ant_ctrl(const char *data)
{
	int ret, res;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$AC3");
	if (cmd == NULL)
		return;
	ret = sscanf(cmd, "$AC3,%d", &res);
	if (ret <= 0) {
		printk(KERN_ERR"%s: AC3 param fail", __func__);
		return;
	}
	if (res != CMD_SUCC)
		enable_default_ant();
}

static void los_rsp_ant_ctrl(const char *data)
{
	int ret, freq;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$LOS");
	if (cmd == NULL)
		return;
	ret = sscanf(cmd, "$LOS,%d", &freq);
	if (ret <= 0) {
		printk(KERN_ERR"%s: LOS param fail", __func__);
		return;
	}
	if (freq == RX_FREQ_S2C)
		enable_default_ant();
}

static void rsp_cmd_ant_ctrl(const struct any_msg *msg)
{
	if (msg == NULL)
		return;
	dtx_rsp_ant_ctrl(msg->data);
	stx_rsp_ant_ctrl(msg->data);
	ac3_rsp_ant_ctrl(msg->data);
	los_rsp_ant_ctrl(msg->data);
}

void spi_recv_data_proc(char *data, int len)
{
	struct any_msg msg;
	msg.head.type = CMD_UP_ANY_IND;
	msg.head.module = MODULE_TYPE_CTRL;
	msg.head.len = sizeof(struct any_msg);
	memcpy_s(msg.data, sizeof(msg.data), data, len);
	rsp_cmd_ant_ctrl((struct any_msg *)&msg);
	send_msg_to_ctrl_x801((struct msg_head *)&msg);
}

static void rsmc_stop_spi_recv_timer(void)
{
	printk(KERN_INFO"%s: spi_status:%d", __func__, g_rsmc_spi_ctx.spi_status);
	if (g_rsmc_spi_ctx.spi_recv_timer_running) {
		printk(KERN_INFO"%s: del spi recv timer", __func__);
		del_timer_sync(&g_rsmc_spi_ctx.spi_recv_timer);
		g_rsmc_spi_ctx.spi_recv_timer_running = false;
	}
	if (g_rsmc_spi_ctx.spi_status != SPI_NORMAL) {
		set_spi_status(SPI_NORMAL);
		up_spi_sema();
	}
	printk(KERN_INFO"%s: end", __func__);
}

static void rsmc_spi_recv_callback(struct timer_list *t)
{
	struct msg_head msg = {0};
	printk(KERN_INFO"%s", __func__);
	msg.type = CMD_INTER_STOP_SPI_TIMER;
	msg.module = MODULE_TYPE_KNL;
	msg.len = sizeof(struct msg_head);
	rsmc_nl_dn_msg_notify_inter_nonblock((struct msg_head *)&msg);
}

void rsmc_start_spi_recv_timer(void)
{
	printk(KERN_INFO"%s: spi_status:%d", __func__, g_rsmc_spi_ctx.spi_status);
	if (g_rsmc_spi_ctx.spi_recv_timer_running) {
		printk(KERN_INFO"%s: spi_recv_timer is already running!", __func__);
		return;
	}
	g_rsmc_spi_ctx.spi_status = SPI_RECV;
	g_rsmc_spi_ctx.spi_recv_timer_running = true;
	timer_setup(&g_rsmc_spi_ctx.spi_recv_timer, rsmc_spi_recv_callback, TIMER_IRQSAFE);
	g_rsmc_spi_ctx.spi_recv_timer.expires = jiffies + HZ;
	add_timer(&g_rsmc_spi_ctx.spi_recv_timer);
	printk(KERN_INFO"%s: end", __func__);
}

void rsmc_spi_recv_proc(void)
{
	atomic_inc(&g_rsmc_spi_ctx.spi_irq);
	up_spi_sema();
}

u32 sta(char *data)
{
	struct sta_info info;
        //struct *sta_info info;
	int crc, ret;
	char *cmd = NULL;
        //info = kzalloc();
	if (g_device_ctx.recv_sta)
		return STA_NONE;
	if (data == NULL)
		return STA_NONE;
	cmd = strstr(data, "$STA");
	if (cmd == NULL)
		return STA_NONE;
	ret = memset_s(&info, sizeof(struct sta_info), 0, sizeof(struct sta_info));
	if (ret != EOK)
		printk(KERN_ERR"%s: memset_s fail", __func__);
	/*ret = sscanf(cmd, "$STA,%[0-9a-zA-Z],%[0-9a-zA-Z],%d,%d,%d,%d,%d,%d,%d*%x\r\n",
		info.soc_ver, info.fw_ver, &info.cpu_freq, &info.aux_adc,
		&info.rfspi_check, &info.bbreg_check, &info.agc, &info.power, &info.polar_check, &crc);*/
	printk(KERN_INFO"%s: cmd is %s,data is %s", __func__,cmd,data);
	ret = sscanf(cmd, "$STA,%s,%s,%d,%d,%d,%d,%d,%d,%d*%x\r\n",
		info.soc_ver, info.fw_ver, &info.cpu_freq, &info.aux_adc,
		&info.rfspi_check, &info.bbreg_check, &info.agc, &info.power, &info.polar_check, &crc);
	if (ret <= 0) {
		printk(KERN_ERR"%s: sscanf_s fail，ret is %d", __func__,ret);
		return STA_NONE;
	}
	printk(KERN_INFO"%s: soc:%s,fw:%s,cpu:%d,adc:%d,rfspi:%d,bbreg:%d,agc:%d,power:%d,polar:%d",
		__func__, info.soc_ver, info.fw_ver, info.cpu_freq, info.aux_adc, info.rfspi_check,
		info.bbreg_check, info.agc, info.power, info.polar_check);
	if ((info.rfspi_check != 0) || (info.bbreg_check != 0) || (info.polar_check != 0))
		return STA_FAIL;
	rsmc_start_heartbeat_x801();
	g_device_ctx.recv_sta = true;
	return STA_SUCC;
}

static int hexCharToValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    } else {
        return -1; // 非法字符
    }
}

static char ValueTohexChar(int val) {
    if (val >= 0 && val <= 9) {
        return val + '0';
    } else if (val >= 0xA && val <= 0xF) {
        return val + 'A' - 10;
    }else
      return 0;
}

static void AddXorCheck(char* data)
{
  int i = 0;
  char result = 0;
  int len = 0;
  
  len = strchr(data, '*')-data;
  
  for (i = 1;i<len;i++)
  {
    result ^= data[i];
  }
  
  char High = ValueTohexChar(result >> 4);
  char Low = ValueTohexChar(result & 0x0F);
  
  data[len+1] = High;
  data[len+2] = Low;
}

static void convertBytetoArray(char* src,char* result,int len)
{
  int i = 0;
  for (i = 0;i<2*len;i++)
  {
    if (i%2 == 0)
      result[i] = ValueTohexChar(src[i/2] >> 4);
    else
      result[i] = ValueTohexChar(src[i/2] & 0xF);
  }
  
}

static void convertlonglongtobytes(long long num, char* bytes)
{
  char temp_bytes[6] = {0};
  long long  temp = num<<6;
  
  temp_bytes[0] = (temp>>40)&0xFF;
  temp_bytes[1] = (temp>>32)&0xFF;
  temp_bytes[2] = (temp>>24)&0xFF;
  temp_bytes[3] = (temp>>16)&0xFF;
  temp_bytes[4] = (temp>>8)&0xFF;
  temp_bytes[5] = temp&0xFF;
  
  temp_bytes[0] |= 0x10;
  
  convertBytetoArray(temp_bytes,&bytes[12],6);
    
  AddXorCheck(bytes);
}

static size_t hexStringToBytes(const char *hexString, unsigned char* bytes, size_t len) {
    if (len % 2 != 0) {
        return 0; // 十六进制字符串的长度必须为偶数
    }

    size_t i, j;
    for (i = 0, j = 0; i < len; i += 2, j++) {
        int high = hexCharToValue(hexString[i]);
        int low = hexCharToValue(hexString[i + 1]);
        if (high == -1 || low == -1) {
            return 0; // 包含非法字符
        }
        bytes[j] = (high << 4) | low;
    }
    return j; // 返回实际转换的字节数
}

static void convertchartobytes(unsigned char ID, char* bytes)
{
  unsigned char temp = 0;
  
  hexStringToBytes(&bytes[22],&temp,2);
  
  temp &= 0xC1;
  
  temp |= ID<<1;
  
  convertBytetoArray((char*)&temp,&bytes[22],1);
  
  AddXorCheck(bytes);
}

static void rsmc_spi_recv(void)
{
	u8 read_cmd[] = {0xaa, 0xaa};
	//char rx_buf[SPI_MAX_FRAME_LEN + 1] = {0};
	char *rx_buf = NULL;
	u32 head, len, data_id;
	char *data = NULL;
	if (!g_device_ctx.is_chip_on)
		return;
	rx_buf = kzalloc(2*SPI_MAX_FRAME_LEN,GFP_DMA);
	rsmc_spi_read_x801(rx_buf, SPI_MAX_FRAME_LEN);
	head = (u32)rx_buf[FW_SPI_HEAD_IDX];
	len = (u32)rx_buf[FW_SPI_LEN_IDX];
	data_id = (u32)rx_buf[FW_SPI_ID_IDX];
	data = (char *)&rx_buf[FW_SPI_DATA_IDX];
	print_data(rx_buf, SPI_MAX_FRAME_LEN, "X801 RECV");
	if (head != FW_SPI_HEAD_DATA) {
		rsmc_start_spi_recv_timer();
		rsmc_spi_write_x801(read_cmd, SPI_REV_START_WRITE_NUM);
		atomic_dec(&g_rsmc_spi_ctx.spi_irq);
		printk(KERN_INFO"%s: spi_irq:%d", __func__, (u32)atomic_read(&g_rsmc_spi_ctx.spi_irq));
		kfree(rx_buf);
		return;
	}
	if (len == FW_SPI_HEAD_DATA && data_id == FW_SPI_HEAD_DATA) {
		rsmc_start_spi_recv_timer();
		rsmc_spi_write_x801(read_cmd, SPI_REV_START_WRITE_NUM);
		atomic_dec(&g_rsmc_spi_ctx.spi_irq);
		printk(KERN_INFO"%s: spi_irq:%d", __func__, (u32)atomic_read(&g_rsmc_spi_ctx.spi_irq));
		kfree(rx_buf);
		return;
	}
	rx_buf[len] = '\0';
	printk(KERN_INFO"X801_DATA << %x|%d|%d|%s", head, len, data_id, data);
	atomic_dec(&g_rsmc_spi_ctx.spi_irq);
	printk(KERN_INFO"%s: spi_irq:%d", __func__, (u32)atomic_read(&g_rsmc_spi_ctx.spi_irq));
	rsmc_stop_spi_recv_timer();
	if (sta(data) != STA_FAIL){
		char* cmd = NULL;
          	cmd = strstr(data, "$DRX,0");
          	if((is_pic_mode == 2) && (cmd != NULL)){
                      int tmp1 = hexCharToValue(data[23]) & 0x01;
                      int tmp2 = hexCharToValue(data[24]) & 0x0e;
                      //printk(KERN_INFO"@law $drx 0 here %d, %d", tmp1, tmp2);
                      if( tmp1 == 1 && tmp2  == 0){
                        rsmc_update_soc_rx_jiffies();
                      	printk(KERN_INFO"@law intercept $drx");
			kfree(rx_buf);
                        return;
                      }
                }
		spi_recv_data_proc(data, len);
		cmd = strstr(data, "$DTX,0");
          	//picture mode
		if (cmd != NULL && is_pic_mode == 1){
                      is_sent_last = 0;
                      is_sending = 1;
                      char temp_data[] = "$DRX,0,16,0,11184cc1b1c00000000000000000000000000000000000000000*45";
                      convertlonglongtobytes(current_Num,temp_data);
                      convertchartobytes(current_ServerID,temp_data);
                      printk(KERN_INFO"temp_data = %s", temp_data);
                      spi_recv_data_proc(temp_data, 72);
                }
          	cmd = strstr(data, "$TST");
          	if (cmd != NULL && is_pic_mode == 2){
                      rsmc_update_soc_rx_jiffies();
                      char cmd2[] = "$TRK,2*53\r\n";
                      printk(KERN_INFO"@law $TRK,2");
          	      rsmc_spi_write_x801(cmd2, strlen(cmd2));
                }
		if (is_pic_mode == 2 && strstr(data, "$DTX,3") != NULL){
		      is_sent_last = 1;
		      is_sending = 0;
		      printk(KERN_INFO"@law set is_sent_last as 1");
		}
        }
	rsmc_update_soc_rx_jiffies();
	kfree(rx_buf);
	printk(KERN_INFO"%s: exit", __func__);
}

void up_spi_sema(void)
{
	up(&g_rsmc_spi_ctx.sema);
}

int rsmc_spi_msg_thread(void *data)
{
	struct msg_entity *msg = NULL;
	printk(KERN_INFO"%s: enter", __func__);
	while (!kthread_should_stop()) {
		down(&g_rsmc_spi_ctx.sema);
		if (g_rsmc_spi_ctx.rsmc_spi_exit) {
			printk(KERN_INFO"%s: rsmc_spi_exit:%d", __func__, (u32)g_rsmc_spi_ctx.rsmc_spi_exit);
			break;
		}
		printk(KERN_INFO"%s: spi_status:%d,spi_irq:%d", __func__,
			g_rsmc_spi_ctx.spi_status, (u32)atomic_read(&g_rsmc_spi_ctx.spi_irq));
		while (atomic_read(&g_rsmc_spi_ctx.spi_irq) > 0)
			rsmc_spi_recv();
		if (g_rsmc_spi_ctx.spi_status != SPI_RECV) {
			set_spi_status(SPI_TRANS);
			msg = rsmc_nl_dn_get_spi_msg();
			while (msg != NULL) {
				rsmc_process_cmd_x801(&msg->msg);
				kfree(msg);
				msg = rsmc_nl_dn_get_spi_msg();
			}
			set_spi_status(SPI_NORMAL);
		}
	}
	g_rsmc_spi_ctx.task = NULL;
	printk(KERN_INFO"%s: exit", __func__);
	return 0;
}

static bool recv_fw_data_buffer(struct enable_msg *msg)
{
	int ret;
	u32 start_idx;

	if (msg == NULL) {
		printk(KERN_ERR"%s: msg null", __func__);
		return false;
	}
	g_rsmc_fw_ctx.file_len = msg->file_len;
	if ((g_rsmc_fw_ctx.file_len <= 0) || (msg->data_block_len <= 0)) {
		printk(KERN_ERR"%s: data err file_len:%d,block_idx:%d,block_len:%d",
			__func__, msg->file_len, msg->data_block_idx, msg->data_block_len);
		return false;
	}
	if (msg->data_block_idx == 0) {
		if (g_rsmc_fw_ctx.fw_buffer != NULL)
			vfree(g_rsmc_fw_ctx.fw_buffer);
		g_rsmc_fw_ctx.fw_buffer = vmalloc(g_rsmc_fw_ctx.file_len);
		if (g_rsmc_fw_ctx.fw_buffer == NULL) {
			printk(KERN_ERR"%s:fw_buffer request memory fail\n", __func__);
			return false;
		}
		ret = memset_s(g_rsmc_fw_ctx.fw_buffer, g_rsmc_fw_ctx.file_len, 0x00, g_rsmc_fw_ctx.file_len);
		if (ret != EOK) {
			vfree(g_rsmc_fw_ctx.fw_buffer);
			g_rsmc_fw_ctx.fw_buffer = NULL;
			return false;
		}
		g_rsmc_fw_ctx.every_block_len = msg->data_block_len;
		g_rsmc_fw_ctx.write_cnt = ((g_rsmc_fw_ctx.file_len % g_rsmc_fw_ctx.every_block_len == 0) ?
			(g_rsmc_fw_ctx.file_len / g_rsmc_fw_ctx.every_block_len) :
			(g_rsmc_fw_ctx.file_len / g_rsmc_fw_ctx.every_block_len + 1));
	}
	if (g_rsmc_fw_ctx.fw_buffer == NULL) {
		printk(KERN_INFO"%s: fw_buffer is NULL", __func__);
		return false;
	}
	start_idx = msg->data_block_idx * g_rsmc_fw_ctx.every_block_len;
	ret = memcpy_s(g_rsmc_fw_ctx.fw_buffer + start_idx, g_rsmc_fw_ctx.file_len,
		(char*)(msg + 1), msg->data_block_len);
	if (ret != EOK) {
		vfree(g_rsmc_fw_ctx.fw_buffer);
		g_rsmc_fw_ctx.fw_buffer = NULL;
		return false;
	}
	printk(KERN_INFO"%s: sum:%d,file_len:%d,every_block_len:%d,block_len:%d,block_idx:%d",
		__func__, g_rsmc_fw_ctx.write_cnt, g_rsmc_fw_ctx.file_len, g_rsmc_fw_ctx.every_block_len,
		msg->data_block_len, msg->data_block_idx);
	return true;
}

static void exit_spi_task(void)
{
	g_rsmc_spi_ctx.rsmc_spi_exit = true;
	up(&g_rsmc_spi_ctx.sema);
	printk(KERN_INFO"%s: g_rsmc_spi_ctx sema", __func__);
}

static void rsmc_disable(bool send_ack)
{
	struct enable_cnf_msg msg_resp;
	msg_resp.head.type = CMD_UP_INIT_CNF;
	msg_resp.head.module = MODULE_TYPE_CTRL;
	msg_resp.head.len = sizeof(struct enable_cnf_msg);
	msg_resp.result = 1;
	msg_resp.type = 0;
	if (!g_device_ctx.is_chip_on) {
		printk(KERN_INFO"%s: soc already off", __func__);
		if (send_ack)
			send_msg_to_ctrl_x801((struct msg_head *)&msg_resp);
		return;
	}
	printk(KERN_INFO"%s: enter", __func__);
	rsmc_spi_irq_ctl_x801(false);
	enable_default_ant();
	g_device_ctx.dtx_status = RSMC_RNSS_RX_AFTER_DTX;
	chip_power_on(false);
	g_device_ctx.is_chip_on = false;
	rsmc_clear_dn_inter_msg_list();
	rsmc_clear_dn_spi_msg_list();
	printk(KERN_INFO"%s: clear msg list done", __func__);
	rsmc_stop_heartbeat_x801();
	printk(KERN_INFO"%s: rsmc_stop_heartbeat_x801", __func__);
	exit_spi_task();
	printk(KERN_INFO"%s: exit_spi_task", __func__);
	if (send_ack)
		send_msg_to_ctrl_x801((struct msg_head *)&msg_resp);
	printk(KERN_INFO"%s: exit", __func__);
}

static bool rsmc_create_spi_task(void)
{
	struct sched_param param;
	if (g_rsmc_spi_ctx.task == NULL) {
		sema_init(&g_rsmc_spi_ctx.sema, 0);
		g_rsmc_spi_ctx.rsmc_spi_exit = false;
		g_rsmc_spi_ctx.task = kthread_run(rsmc_spi_msg_thread, NULL, "rsmc_spi_msg_thread");
		if (IS_ERR(g_rsmc_spi_ctx.task)) {
			printk(KERN_ERR"%s: failed to create thread", __func__);
			g_rsmc_spi_ctx.task = NULL;
			return false;
		}
		param.sched_priority = MAX_RT_PRIO - 1;
		sched_setscheduler(g_rsmc_spi_ctx.task, SCHED_FIFO, &param);
	}
	return true;
}

static void rsmc_enable(struct enable_msg *msg)
{
	bool ret;
	struct enable_cnf_msg cnf_msg;
	cnf_msg.head.type = CMD_UP_INIT_CNF;
	cnf_msg.head.module = MODULE_TYPE_CTRL;
	cnf_msg.head.len = sizeof(struct enable_cnf_msg);
	cnf_msg.type = 1;
	cnf_msg.result = 0;
	cnf_msg.cause = EN_FAIL_OTHER;
	g_device_ctx.cause = EN_SUCC;
	if (msg == NULL) {
		send_msg_to_ctrl_x801((struct msg_head *)&cnf_msg);
		return;
	}
	if (g_device_ctx.is_chip_on)
		rsmc_disable(false);
	if (!rsmc_create_spi_task()) {
		send_msg_to_ctrl_x801((struct msg_head *)&cnf_msg);
		return;
	}
	ret = recv_fw_data_buffer(msg);
	if (!ret) {
		if (g_rsmc_fw_ctx.fw_buffer != NULL)
			vfree(g_rsmc_fw_ctx.fw_buffer);
		g_rsmc_fw_ctx.fw_buffer = NULL;
	}
	if (msg->data_block_idx == g_rsmc_fw_ctx.write_cnt - 1) {
		enable_default_ant();
		g_device_ctx.dtx_status = RSMC_RNSS_RX_AFTER_DTX;
		g_device_ctx.recv_sta = false;
		g_device_ctx.wait_sta_retry_times = WAIT_STA_RETRY_TIMES;
		ret = rsmc_firmware_download();
		if (!ret) {
			if (g_rsmc_fw_ctx.fw_buffer != NULL)
				vfree(g_rsmc_fw_ctx.fw_buffer);
			g_rsmc_fw_ctx.fw_buffer = NULL;
		}
	}
	ret ? (cnf_msg.result = 1) : (cnf_msg.result = 0);
	cnf_msg.cause = g_device_ctx.cause;
	send_msg_to_ctrl_x801((struct msg_head *)&cnf_msg);
}

void rsmc_set_init_x801(struct enable_msg *msg)
{
	if (msg == NULL) {
		printk(KERN_ERR"%s: msg null", __func__);
		return;
	}
	mutex_lock(&g_device_ctx.access_mutex);
	if (msg->status == 1) {
		printk(KERN_INFO"%s: enable", __func__);
		rsmc_enable(msg);
	} else {
		printk(KERN_INFO"%s: disable", __func__);
		if (msg->head.type == CMD_INTER_INIT_REQ)
			rsmc_disable(false);
		else
			rsmc_disable(true);
	}
	mutex_unlock(&g_device_ctx.access_mutex);
}

static void rsmc_init_msg_proc(struct enable_msg *msg)
{
	rsmc_set_init_x801(msg);
}

static void rna_req_ant_ctrl(const char *data)
{
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$RNA");
	if (cmd == NULL)
		return;
	enable_default_ant();
}

static void rda_req_ant_ctrl(const char *data)
{
	int ret, status;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$RDA");
	if (cmd == NULL)
		return;
	ret = sscanf(cmd, "$RDA,%d", &status);
	if (ret <= 0) {
		printk(KERN_ERR"%s: RDA param fail", __func__);
		return;
	}
	printk(KERN_INFO"%s: enable rx status: %d", __func__, status);
	(status == 1) ? enable_rx_ant() : enable_default_ant();
}

static void srx_req_ant_ctrl(const char *data)
{
	int status, freq, ret;
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$SRX");
	if (cmd == NULL)
		return;
	ret = sscanf(cmd, "$SRX,%d,%d", &status, &freq);
	if (ret <= 0) {
		printk(KERN_ERR"%s: SRX param fail", __func__);
		return;
	}
	printk(KERN_INFO"%s: srx status:%d,freq:%d", __func__, status, freq);
	if (freq == RX_FREQ_S2C)
		(status == 1) ? enable_rx_ant() : enable_default_ant();
	else
		enable_default_ant();
}

static void stx_req_ant_ctrl(const char *data)
{
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$STX");
	if (cmd == NULL)
		return;
	enable_tx_ant();
}

static void dtx_req_ant_ctrl(const char *data)
{
	char *cmd = NULL;
	if (data == NULL)
		return;
	cmd = strstr(data, "$DTX");
	if (cmd == NULL)
		return;
	enable_tx_ant();
}

static void req_cmd_ant_ctrl(const char *data)
{
	dtx_req_ant_ctrl(data);
	rna_req_ant_ctrl(data);
	rda_req_ant_ctrl(data);
	stx_req_ant_ctrl(data);
	srx_req_ant_ctrl(data);
}

static void rsmc_any_msg_proc(struct any_msg *msg)
{
  	if (strstr(msg->data, "$TRK,1") != NULL && is_pic_mode == 1 && is_sending == 1){
          printk(KERN_INFO"@law intercept trk 1");
          return;
        }
  	if (strstr(msg->data, "$TRK,0") != NULL && is_pic_mode == 2 && is_sent_last == 1){
          printk(KERN_INFO"@law intercept trk 0");
          return;
        }
  	if (strstr(msg->data, "$RDA,0") != NULL && is_pic_mode == 2){
          rsmc_update_soc_tx_jiffies();
          printk(KERN_INFO"@law intercept $RDA,0");
          return;
        }
  	if (strstr(msg->data, "$RNA") != NULL && is_pic_mode == 2){
          rsmc_update_soc_tx_jiffies();
          printk(KERN_INFO"@law intercept $RNA");
          return;
        }
  
	req_cmd_ant_ctrl(msg->data);
	printk(KERN_INFO"X801_DATA >> %s", msg->data);
        rsmc_spi_write_x801(msg->data, strlen(msg->data));
	rsmc_update_soc_tx_jiffies();
}

static int findFourthOccurrence(const char *str, char ch) {
    int count = 0; // 用于计数字符出现的次数
    int position = -1; // 用于存储位置

    // 遍历字符串
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ch) {
            count++;
            if (count == 4) {
                // 第四次遇到该字符
                position = i;
                break;
            }
        }
    }

    return position;
}

static long long extractPhoneNum(const char *data)
{
  long long phoneNum = 0;
  
  int data_index = 0;
  
  unsigned char bytes[6] ={0};
  
  data_index = findFourthOccurrence(data,',');
  
  hexStringToBytes(&data[data_index+1],bytes,12);
  
  for (int i = 0; i < 6; ++i) {
        phoneNum |= (long long)(bytes[5 - i]) << (8 * i);
  }
  
  phoneNum = phoneNum >> 5;
  phoneNum &= 0x1FFFFFFFFF;
  
  return phoneNum;
  
}

static unsigned char extractServerID(const char *data)
{
  int data_index = 0;
  
  unsigned char ServerID = 0;
  
  data_index = findFourthOccurrence(data,',');
  
  hexStringToBytes(&data[data_index+17],&ServerID,2);
  
  ServerID = ServerID >> 2;
  ServerID &= 0x1F;
  
  return ServerID;
  
}

static void rsmc_dtx_msg_proc(struct dtx_msg *msg)
{
  	if (strstr(msg->data, "$DTX") != NULL && is_pic_mode == 2 && is_sent_last == 1){
          	is_sent_last = 0;
          	printk(KERN_INFO"@law intercept $DTX");
          	return;
        }
  
        static int last_ndi = 0x08;
        int new_ndi = 0;
        int temp = 0;
        int data_index = 0;

	g_device_ctx.dtx_status = msg->status;
  	if(is_pic_mode == 1)
        	g_device_ctx.dtx_status = CMD_TX_CMD_ACK;
        
	req_cmd_ant_ctrl(msg->data);
	printk(KERN_INFO"X801_DATA >> %s", msg->data);
        current_Num = extractPhoneNum(msg->data);
        current_ServerID = extractServerID(msg->data);
        printk(KERN_INFO"current_Num = %lld", current_Num);
        printk(KERN_INFO"current_ServerID = %d", current_ServerID);
        data_index = findFourthOccurrence(msg->data,',');
        
  	// only in rich meida mode needs to revert ndi flag
  	if(is_pic_mode != 0){
          temp = hexCharToValue(msg->data[data_index+17]);
          new_ndi = temp & 0x08;
          if (new_ndi == last_ndi){
            new_ndi = new_ndi ^ 0x08;
          }
          last_ndi = new_ndi;

          msg->data[data_index+17] = ValueTohexChar(new_ndi | temp);
        }
        AddXorCheck(msg->data);
        printk(KERN_INFO"new X801_DATA >> %s", msg->data);
	rsmc_spi_write_x801(msg->data, strlen(msg->data));
	rsmc_update_soc_tx_jiffies();
}

static void rsmc_device_msg_proc(struct msg_head *req)
{
	if (req == NULL)
		return;
	printk(KERN_INFO"%s: msg_type:%d", __func__, req->type);
	switch (req->type) {
	case CMD_DN_INIT_REQ:
	case CMD_INTER_INIT_REQ:
		rsmc_init_msg_proc((struct enable_msg *)req);
		break;
	case CMD_DN_ANY_REQ:
		rsmc_any_msg_proc((struct any_msg *)req);
		break;
	case CMD_DN_DTX_REQ:
		rsmc_dtx_msg_proc((struct dtx_msg *)req);
		break;
	case CMD_INTER_HB_TIMER_REQ:
		if (g_device_ctx.is_chip_on)
			rsmc_start_heartbeat_x801();
		break;
	case CMD_INTER_STOP_SPI_TIMER:
		rsmc_stop_spi_recv_timer();
		break;
	case CMD_INTER_WAIT_STA_TIMEOUT:
		rsmc_stop_wait_sta_timer();
		break;
	default:
		break;
	}
}

static int rsmc_driver_init(void)
{
	int rc;
	struct smc_device *dev = kzalloc(sizeof(struct smc_device), GFP_KERNEL);

	if (dev == NULL)
		return -ENOMEM;
	dev->ic_name = RSMC_IC_NAME;
	dev->dev_node_name = RSMC_DEV_NODE_NAME;

	rc = rsmc_register_dev_x801(dev);
	if (rc) {
		printk(KERN_ERR"%s: register fail", __func__);
		goto err;
	}
	return rc;
err:
	kfree(dev);
	dev = NULL;
	return rc;
}

static void rsmc_driver_exit(void)
{
	struct smc_core_data *cd = smc_get_core_data_x801();
	rsmc_unregister_dev_x801(cd->smc_dev);
	if (g_rsmc_fw_ctx.fw_buffer != NULL)
		vfree(g_rsmc_fw_ctx.fw_buffer);
	g_rsmc_fw_ctx.fw_buffer = NULL;
	printk(KERN_INFO"%s: called\n", __func__);
};

msg_process *rsmc_device_reg_x801(notify_event *fun)
{
	int count = 0;
	int ret;
	struct smc_core_data *cd = NULL;

	printk(KERN_INFO"%s: enter", __func__);
	rsmc_spi_init_x801();
	cd = smc_get_core_data_x801();
	while (cd == NULL && count < DEVICE_REG_TRY_MAX_COUNT) {
		msleep(DEVICE_REG_SLEEP_INTERVAL);
		count++;
		cd = smc_get_core_data_x801();
	}
	printk(KERN_INFO"%s: count=%d, cd=%p", __func__, count, cd);

	ret = rsmc_driver_init();
	if (ret)
		printk(KERN_ERR"%s: rsmc_driver_init fail, ret=%d", __func__, ret);
	g_device_ctx.notifier = fun;
	g_device_ctx.is_chip_on = false;
	printk(KERN_INFO"%s: exit", __func__);
	mutex_init(&g_device_ctx.access_mutex);
	atomic_set(&g_rsmc_spi_ctx.spi_irq, 0);
	return rsmc_device_msg_proc;
}

void rsmc_device_unreg_x801(int reason)
{
	mutex_destroy(&g_device_ctx.access_mutex);
	rsmc_spi_exit_x801();
	rsmc_driver_exit();
}
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

