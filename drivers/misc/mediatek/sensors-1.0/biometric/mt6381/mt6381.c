/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include "cust_biometric.h"
#include "mt6381.h"
#include "vsm_signal_reg.h"
#include "biometric.h"
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <mt-plat/aee.h>
//#include <mt-plat/mtk_gpio_core.h>
#include "ppg_control.h"
#include <linux/ctype.h>
/*----------------------------------------------------------------------------*/
#define DBG                 0
#define DBG_READ            0
#define DBG_WRITE           0
#define DBG_SRAM            0

#define MT6381_DEV_NAME        "MT6381_BIOMETRIC"
static const struct i2c_device_id mt6381_i2c_id[] = {
	{MT6381_DEV_NAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id biometric_of_match[] = {
	{.compatible = "mediatek,biosensor"},
	{},
};
#endif

/* two slave addr */
#define MT2511_SLAVE_I	0x23
#define MT2511_SLAVE_II 0x33

/* sram type addr */
#define SRAM_EKG_ADDR	0xC8
#define SRAM_PPG1_ADDR	0xD8
#define SRAM_PPG2_ADDR	0xE8
#define SRAM_BISI_ADDR	0xF8

/* read counter addr */
#define SRAM_EKG_READ_COUNT_ADDR	0xC0
#define SRAM_PPG1_READ_COUNT_ADDR	0xD0
#define SRAM_PPG2_READ_COUNT_ADDR	0xE0
#define SRAM_BISI_READ_COUNT_ADDR	0xF0

/* write counter addr */
#define SRAM_EKG_WRITE_COUNT_ADDR	0xCC
#define SRAM_PPG1_WRITE_COUNT_ADDR	0xDC
#define SRAM_PPG2_WRITE_COUNT_ADDR	0xEC
#define SRAM_BISI_WRITE_COUNT_ADDR	0xFC

#define SRAM_COUNTER_RESET_MASK     0x20000000
#define SRAM_COUNTER_OFFSET         29

#define UPDATE_COMMAND_ADDR     0x2328
#define SEC_UPDATE_COMMAND_ADDR 0x2728

#define PPG1_GAIN_ADDR		0x3318
#define PPG1_GAIN_MASK      0x00000007
#define PPG1_GAIN_OFFSET    0

#define PPG2_GAIN_ADDR      PPG1_GAIN_ADDR
#define PPG2_GAIN_MASK      0x00000038
#define PPG2_GAIN_OFFSET    3

#define PPG_AMDAC_ADDR      PPG1_GAIN_ADDR
#define PPG_AMDAC_MASK      0x3C00000
#define PPG_AMDAC_OFFSET    22

#define PPG_AMDAC1_MASK      0x1C00000
#define PPG_AMDAC1_OFFSET    22
#define PPG_AMDAC2_MASK      0xE000000
#define PPG_AMDAC2_OFFSET    25


#define PPG_PGA_GAIN_ADDR      PPG1_GAIN_ADDR
#define PPG_PGA_GAIN_MASK      0x1C0000
#define PPG_PGA_GAIN_OFFSET    18

#define PPG1_CURR_ADDR      0x332C
#define PPG1_CURR_MASK      0x000000FF
#define PPG1_CURR_OFFSET    0
#define PPG2_CURR_ADDR      PPG1_CURR_ADDR
#define PPG2_CURR_MASK      0x0000FF00
#define PPG2_CURR_OFFSET    8

#define CHIP_VERSION_ADDR       0x23AC
#define CHIP_VERSION_E1         0X1
#define CHIP_VERSION_E2         0X2
#define CHIP_VERSION_UNKNOWN    0XFF

#define DIGITAL_START_ADDR 0x3360

#define EKG_SAMPLE_RATE_ADDR1 0x3364
#define EKG_SAMPLE_RATE_ADDR2 0x3310
#define EKG_DEFAULT_SAMPLE_RATE 256

#define PPG_SAMPLE_RATE_ADDR 0x232C
#define PPG_FSYS             1048576
#define PPG_DEFAULT_SAMPLE_RATE 125

#define VSM_BISI_SRAM_LEN   256

#define MAX_WRITE_LENGTH 4

#define DEFAULT_PGA6		6002
#define DEFAULT_AMBDAC5_5	21570977
#define CALI_DATA_STABLE_LEN	100
#define CALI_DATA_LEN		200

/* threshold = 6050 */
/* code = 6050 * (2 ^ 16) / 3200 - 129583 = -5679 */
/* 129583 is ambdac offset code, measured and provided by DE */
#define PPG_THRESHOLD (-5679)

#define EKGFE_CON0_ADDR			0x3308
#define EKGFE_CON0_RL_MODE_SHIFT	10
#define EKGFE_CON0_RL_MODE_MASK	(0x1 << EKGFE_CON0_RL_MODE_SHIFT)

enum ekg_mode_t {
	EKG_RLD_MODE = 0,
	EKG_2E_MODE = 1,
};

#define FTM_PPG_IR_THRESHOLD	12291
#define FTM_PPG_R_THRESHOLD	19459
#define FTM_PPG_EKG_THRESHOLD	125829 /* +10mV */

enum {
	EKG = 0,
	PPG1,
	PPG2,
	NUM_OF_TYPE,
};

struct sensor_info {
	unsigned short read_counter;
	unsigned short write_counter;
	unsigned short upsram_rd_data;
	char *raw_data_path;
	struct file *filp;
	unsigned int numOfData;
	unsigned int enBit;
};

static struct sensor_info test_info[NUM_OF_TYPE];
static struct task_struct *bio_tsk[4] = { 0 };
static DEFINE_MUTEX(bio_data_collection_mutex);

u64 pre_ppg1_timestamp;
u64 pre_ppg2_timestamp;
u64 pre_ekg_timestamp;
static int8_t vsm_chip_version = -1;
static int biosensor_init_flag = -1;
static unsigned int pga6;
static unsigned int ambdac5_5;
static long long dc_offset;
static unsigned int cali_pga, cali_ambdac_amb, cali_ambdac_led;
static bool inCali;

struct i2c_msg ekg_msg[VSM_SRAM_LEN * 2];
struct i2c_msg ppg1_msg[VSM_SRAM_LEN * 2];
struct i2c_msg ppg2_msg[VSM_SRAM_LEN * 2];
struct i2c_msg bisi_msg[VSM_SRAM_LEN * 2];
char mt6381_sram_addr[4] = {
	SRAM_EKG_ADDR, SRAM_PPG1_ADDR,
	SRAM_PPG2_ADDR, SRAM_BISI_ADDR };

/* used to store the data from i2c */
u32 ekg_buf[VSM_SRAM_LEN], sram1_buf[VSM_SRAM_LEN], sram2_buf[VSM_SRAM_LEN];
/* used to store the dispatched ppg data */
u32 temp_buf[VSM_SRAM_LEN], ppg1_buf2[VSM_SRAM_LEN], ppg2_buf2[VSM_SRAM_LEN];
/* used to store ambient light data */
u32 ppg1_amb_buf[VSM_SRAM_LEN], ppg2_amb_buf[VSM_SRAM_LEN];
/* used to store agc (auto. gain control) data for algorithm debug */
u32 ppg1_agc_buf[VSM_SRAM_LEN], ppg2_agc_buf[VSM_SRAM_LEN];
u32 ppg1_buf2_len, ppg2_buf2_len;
/* used for AGC */
u32 amb_temp_buf[VSM_SRAM_LEN];
/* used to store downsample data (16Hz) for AGC */
u32 agc_ppg1_buf[VSM_SRAM_LEN/32], agc_ppg1_amb_buf[VSM_SRAM_LEN/32];
u32 agc_ppg2_buf[VSM_SRAM_LEN/32], agc_ppg2_amb_buf[VSM_SRAM_LEN/32];
u32 agc_ppg1_buf_len, agc_ppg2_buf_len;
bool ppg1_led_status, ppg2_led_status;

u32 ppg1_amb_buf[VSM_SRAM_LEN], ppg2_amb_buf[VSM_SRAM_LEN];
enum vsm_signal_t current_signal;
struct mutex op_lock;
struct signal_data_t VSM_SIGNAL_MODIFY_array[50];
struct signal_data_t VSM_SIGNAL_NEW_INIT_array[50];
int mod_ary_len; /* modify array length */
int new_init_array_len;
u32 set_AFE_TCTRL_CON2, set_AFE_TCTRL_CON3;
u32 AFE_TCTRL_CON2, AFE_TCTRL_CON3;

struct pinctrl *pinctrl_gpios;
struct pinctrl_state *bio_pins_default;
struct pinctrl_state *bio_pins_reset_high, *bio_pins_reset_low;
struct pinctrl_state *bio_pins_pwd_high, *bio_pins_pwd_low;

static int64_t enable_time;
static int64_t pre_t[VSM_SRAM_PPG2+1];
static int64_t numOfData[3] = {0};
static int numOfEKGDataNeedToDrop;
static bool data_dropped;
atomic_t bio_trace;
static u32 lastAddress;
static u8 lastRead[] = {0, 0, 0, 0};
static unsigned int polling_delay;
static struct {
	bool in_latency_test;
	uint32_t first_data;
	uint32_t second_data;
	uint32_t delay_num;
	uint32_t ekg_num;
	uint32_t ppg1_num;
	uint32_t ppg2_num;
} latency_test_data;
static int stress_test;

#define NUM_OF_DATA_PER_LINE	(12)
#define MAX_FILE_LENGTH		(128)
#define MAX_LENGTH_PER_LINE	(256)

struct raw_data_info {
	struct file *filp;
	int apk_type;
	int num_of_data;
	int raw_data[NUM_OF_DATA_PER_LINE];
};

struct offline_mode_info {
	int64_t en_time;
	char raw_data_path[MAX_FILE_LENGTH];
	struct raw_data_info sensor[3];
};
static bool offline_mode_en;
static char offline_mode_file_name[MAX_FILE_LENGTH];
struct offline_mode_info *olm_info_p;

static enum vsm_status_t
vsm_driver_write_signal(struct signal_data_t *reg_addr,
			 int32_t len,
			 uint32_t *enable_data);
static enum vsm_status_t vsm_driver_set_led(enum vsm_signal_t signal,
					     bool enable);
static int MT6381_WriteCalibration(struct biometric_cali *cali);
static struct file *bio_file_open(const char *path, int flags, int rights);
static int bio_file_write(struct file *file,
			   unsigned long long offset,
			   unsigned char *data,
			   unsigned int size);
static enum vsm_status_t vsm_driver_set_signal(enum vsm_signal_t signal);
static enum vsm_status_t vsm_driver_disable_signal(enum vsm_signal_t signal);
static enum vsm_status_t
vsm_driver_read_sram(enum vsm_sram_type_t sram_type,
		      uint32_t *data_buf, uint32_t *amb_buf,
		      u32 *len);
static int mt6381_local_init(void);
static int mt6381_local_remove(void);
static struct biometric_init_info mt6381_init_info = {
	.name = "mt6381",
	.init = mt6381_local_init,
	.uninit = mt6381_local_remove,
};

void mt6381_init_i2c_msg(u8 addr)
{
	int i;

	for (i = 0; i < VSM_SRAM_LEN; i++) {
		ekg_msg[2 * i].addr = addr;
		ekg_msg[2 * i].flags = 0;
		ekg_msg[2 * i].len = 1;
		ekg_msg[2 * i].buf = mt6381_sram_addr;
		ekg_msg[2 * i + 1].addr = addr;
		ekg_msg[2 * i + 1].flags = I2C_M_RD;
		ekg_msg[2 * i + 1].len = 4;
		ekg_msg[2 * i + 1].buf = (u8 *) (ekg_buf + i);

		ppg1_msg[2 * i].addr = addr;
		ppg1_msg[2 * i].flags = 0;
		ppg1_msg[2 * i].len = 1;
		ppg1_msg[2 * i].buf = mt6381_sram_addr + 1;
		ppg1_msg[2 * i + 1].addr = addr;
		ppg1_msg[2 * i + 1].flags = I2C_M_RD;
		ppg1_msg[2 * i + 1].len = 4;
		ppg1_msg[2 * i + 1].buf = (u8 *) (sram1_buf + i);

		ppg2_msg[2 * i].addr = addr;
		ppg2_msg[2 * i].flags = 0;
		ppg2_msg[2 * i].len = 1;
		ppg2_msg[2 * i].buf = mt6381_sram_addr + 2;
		ppg2_msg[2 * i + 1].addr = addr;
		ppg2_msg[2 * i + 1].flags = I2C_M_RD;
		ppg2_msg[2 * i + 1].len = 4;
		ppg2_msg[2 * i + 1].buf = (u8 *) (sram2_buf + i);
	}
}

struct i2c_client *mt6381_i2c_client;

int mt6381_i2c_write_read(u8 addr, u8 reg, u8 *buf, u16 length)
{
	int res;
	struct i2c_msg msg[2];

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;
	msg[0].buf[0] = reg;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = buf;
	res = i2c_transfer(mt6381_i2c_client->adapter, msg, ARRAY_SIZE(msg));
	if (res < 0) {
		pr_err("mt6381 i2c read failed. addr:%x, reg:%x, errno:%d\n",
			addr, reg, res);
		return res;
	}
	return VSM_STATUS_OK;
}

int mt6381_i2c_write(u8 addr, u8 *buf, u16 length)
{
	mt6381_i2c_client->addr = addr;
	return i2c_master_send(mt6381_i2c_client, buf, length);
}

static int driver_type_to_apk_type(enum vsm_signal_t signal)
{
	switch (signal) {
	case VSM_SIGNAL_EKG:
		return 5;
	case VSM_SIGNAL_PPG1:
		return 9;
	case VSM_SIGNAL_PPG2:
		return 10;
	default:
		return -1;
	}
}

static int driver_type_to_index(enum vsm_signal_t signal)
{
	switch (signal) {
	case VSM_SIGNAL_EKG:
		return 0;
	case VSM_SIGNAL_PPG1:
		return 1;
	case VSM_SIGNAL_PPG2:
		return 2;
	default:
		return -1;
	}
}

static int bio_thread(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	struct sensor_info *s_info = (struct sensor_info *)arg;
	u8 buf[4];
	u32 rc, wc;
	/* u32 count = 0; */
	/* int status = 0; */
	size_t size;
	u8 str_buf[100];
	int len;
	uint32_t read_counter1 = 0, read_counter2 = 0;
	uint32_t write_counter1 = 0, write_counter2 = 0;
	struct bus_data_t data;

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop())
			break;

		msleep_interruptible(polling_delay);
		mutex_lock(&bio_data_collection_mutex);
		if (s_info->filp != NULL) {
			/* read counter */
			data.addr = s_info->read_counter >> 8;
			data.reg = s_info->read_counter & 0xFF;
			data.data_buf = (uint8_t *) &read_counter2;
			data.length = sizeof(read_counter2);
			vsm_driver_read_register(&data);
			do {
				read_counter1 = read_counter2;
				vsm_driver_read_register(&data);
			} while ((read_counter1 & 0x1ff0000) !=
				(read_counter2 & 0x1ff0000));

			/* write counter */
			data.addr = s_info->write_counter >> 8;
			data.reg = s_info->write_counter & 0xFF;
			data.data_buf = (uint8_t *) &write_counter2;
			data.length = sizeof(write_counter2);
			vsm_driver_read_register(&data);
			do {
				write_counter1 = write_counter2;
				vsm_driver_read_register(&data);
			} while ((write_counter1 & 0x1ff0000) !=
				(write_counter2 & 0x1ff0000));
			/* mt2511_read(s_info->read_counter, buf); */
			/* rc = ((*(int *)buf) & 0x1ff0000) >> 16; */
			rc = (read_counter2 & 0x1ff0000) >> 16;
			/* mt2511_read(s_info->write_counter, buf); */
			/* wc = ((*(int *)buf) & 0x1ff0000) >> 16; */
			wc = (write_counter2 & 0x1ff0000) >> 16;

			data.addr = s_info->upsram_rd_data >> 8;
			data.reg = s_info->upsram_rd_data & 0xFF;
			data.data_buf = (uint8_t *)buf;
			data.length = sizeof(buf);
			if (atomic_read(&bio_trace) != 0)
				pr_debug("rc = %d, wc = %d\n", rc, wc);
			while (rc != wc && s_info->numOfData) {
				vsm_driver_read_register(&data);
				if (atomic_read(&bio_trace) != 0)
					pr_debug("%d, %d, %x, %x, %lld\n",
						rc, wc, *(int *)buf,
						s_info->upsram_rd_data,
						sched_clock());
				len = sprintf(str_buf, "%x\n", *(int *)buf);
				size = bio_file_write(s_info->filp,
					0, str_buf, len);
				rc = (rc + 1) % 384;
				s_info->numOfData--;
			}

			if (s_info->numOfData == 0) {
				vfs_fsync(s_info->filp, 0);
				filp_close(s_info->filp, NULL);
				s_info->filp = NULL;
			} else {
				/* read counter */
				data.addr = s_info->read_counter >> 8;
				data.reg = s_info->read_counter & 0xFF;
				data.data_buf = (uint8_t *) &read_counter2;
				data.length = sizeof(read_counter2);
				vsm_driver_read_register(&data);
				do {
					read_counter1 = read_counter2;
					vsm_driver_read_register(&data);
				} while ((read_counter1 & 0x1ff0000) !=
					(read_counter2 & 0x1ff0000));
				/* mt2511_read(s_info->read_counter, buf); */
				/* rc = ((*(int *)buf) & 0x1ff0000) >> 16; */
				rc = (read_counter2 & 0x1ff0000) >> 16;
				if (rc != wc) {
					len = sprintf(str_buf,
						"Unexpected rc = %d, wr = %d\n",
						rc, wc);
					size = bio_file_write(s_info->filp,
						0, str_buf, len);
				}
			}
		}
		mutex_unlock(&bio_data_collection_mutex);
	}
	return 0;
}

static int bio_thread_stress(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	struct file *ekg_filp = NULL;
	struct file *ppg_filp = NULL;
	size_t size;
	u8 str_buf[100];
	int len;
	int length;
	int *raw_data;
	int i;

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop())
			break;

		msleep_interruptible(polling_delay);
		mutex_lock(&bio_data_collection_mutex);
		if (stress_test != 0) {
			if (ekg_filp == NULL || ppg_filp == NULL) {
				ekg_filp = bio_file_open("/data/bio/ekg_stress",
					O_CREAT | O_WRONLY | O_TRUNC, 0644);
				if (ekg_filp == NULL) {
					pr_err("open %s fail\n",
						"/data/bio/ekg_stress");
					continue;
				}
				ppg_filp = bio_file_open("/data/bio/ppg_stress",
					O_CREAT | O_WRONLY | O_TRUNC, 0644);
				if (ekg_filp == NULL) {
					pr_err("open %s fail\n",
						"/data/bio/ppg_stress");
					continue;
				}
			}
			raw_data = kzalloc(sizeof(int) * VSM_SRAM_LEN,
				GFP_KERNEL);
			vsm_driver_read_sram(VSM_SRAM_EKG,
				raw_data, NULL, &length);
			for (i = 0; i < length; i++) {
				len = sprintf(str_buf, "%d\n",
					raw_data[i] >= 0x400000 ?
					raw_data[i] - 0x800000 : raw_data[i]);
				size = bio_file_write(ekg_filp,
					0, str_buf, len);
			}
			vsm_driver_read_sram(VSM_SRAM_PPG2,
				raw_data, NULL, &length);
			for (i = 0; i < length; i += 2) {
				len = sprintf(str_buf, "%d, %d\n",
					raw_data[i] >= 0x400000 ?
						raw_data[i] - 0x800000 :
						raw_data[i],
					raw_data[i + 1] >= 0x400000 ?
						raw_data[i + 1] - 0x800000 :
						raw_data[i + 1]);
				size = bio_file_write(ppg_filp,
					0, str_buf, len);
			}
		} else {
			if (ekg_filp != NULL) {
				vfs_fsync(ekg_filp, 0);
				filp_close(ekg_filp, NULL);
				ekg_filp = NULL;
			}
			if (ppg_filp != NULL) {
				vfs_fsync(ppg_filp, 0);
				filp_close(ppg_filp, NULL);
				ppg_filp = NULL;
			}
		}
		mutex_unlock(&bio_data_collection_mutex);
	}
	return 0;
}

static void bio_test_init(void)
{
	static bool init_done;

	if (init_done == false) {
		test_info[EKG].write_counter = 0x33CC;
		test_info[EKG].read_counter = 0x33C0;
		test_info[EKG].upsram_rd_data = 0x33C8;
		test_info[EKG].raw_data_path = "/data/bio/ekg";
		test_info[EKG].filp = NULL;
		test_info[EKG].numOfData = 0;
		test_info[EKG].enBit = 0x18;

		test_info[PPG1].write_counter = 0x33DC;
		test_info[PPG1].read_counter = 0x33D0;
		test_info[PPG1].upsram_rd_data = 0x33D8;
		test_info[PPG1].raw_data_path = "/data/bio/ppg1";
		test_info[PPG1].filp = NULL;
		test_info[PPG1].numOfData = 0;
		test_info[PPG1].enBit = 0x124;

		test_info[PPG2].write_counter = 0x33EC;
		test_info[PPG2].read_counter = 0x33E0;
		test_info[PPG2].upsram_rd_data = 0x33E8;
		test_info[PPG2].raw_data_path = "/data/bio/ppg2";
		test_info[PPG2].filp = NULL;
		test_info[PPG2].numOfData = 0;
		test_info[PPG2].enBit = 0x144;

		bio_tsk[0] = kthread_create(bio_thread,
			(void *)&test_info[EKG], "EKG");
		bio_tsk[1] = kthread_create(bio_thread,
			(void *)&test_info[PPG1], "PPG1");
		bio_tsk[2] = kthread_create(bio_thread,
			(void *)&test_info[PPG2], "PPG2");
		wake_up_process(bio_tsk[0]);
		wake_up_process(bio_tsk[1]);
		wake_up_process(bio_tsk[2]);

		init_done = true;
	}
}

static void bio_stress_test_init(void)
{
	static bool init_done;

	if (init_done == false) {
		bio_tsk[3] = kthread_create(
			bio_thread_stress, NULL, "BIO_STRESS");
		wake_up_process(bio_tsk[3]);

		init_done = true;
	}
}

static struct file *bio_file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);

	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}

	return filp;
}

static int
bio_file_read_line(struct file *file, unsigned long long offset,
		    unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;
	int i = 0;

	oldfs = get_fs();
	set_fs(get_ds());

	while ((ret = vfs_read(file, data + i, 1, &(file->f_pos))) == 1) {
		if (data[i] == '\n') {
			data[i] = '\0';
			break;
		}
		i++;
	}

	set_fs(oldfs);

	return ret;
}

static int
bio_file_write(struct file *file, unsigned long long offset,
		unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;
	struct inode *inode = file->f_mapping->host;

	oldfs = get_fs();
	set_fs(get_ds());

	offset = i_size_read(inode);
	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);

	return ret;
}

static void insert_modify_setting(int addr, int value)
{
	int i;

	for (i = 0; i < mod_ary_len; i++) {
		if (VSM_SIGNAL_MODIFY_array[i].addr == addr) {
			VSM_SIGNAL_MODIFY_array[i].value = value;
			return;
		}
	}

	if (i == sizeof(VSM_SIGNAL_MODIFY_array)) {
		pr_err("modify array full\n");
	} else {
		VSM_SIGNAL_MODIFY_array[i].addr = addr;
		VSM_SIGNAL_MODIFY_array[i].value = value;
		mod_ary_len++;
	}
}

static void remove_modify_setting(int addr, int value)
{
	int i;

	for (i = 0; i < mod_ary_len; i++) {
		if (VSM_SIGNAL_MODIFY_array[i].addr == addr) {
			VSM_SIGNAL_MODIFY_array[i].addr =
				VSM_SIGNAL_MODIFY_array[
					mod_ary_len - 1].addr;
			VSM_SIGNAL_MODIFY_array[i].value =
				VSM_SIGNAL_MODIFY_array[
					mod_ary_len - 1].value;
			mod_ary_len--;
			return;
		}
	}
}

static void clear_modify_setting(void)
{
	mod_ary_len = 0;
}

static void update_new_init_setting(void)
{
	int i, j;

	if (mod_ary_len == 0) {
		new_init_array_len = 0;
		return;
	}

	memcpy(VSM_SIGNAL_NEW_INIT_array,
		VSM_SIGNAL_INIT_array, sizeof(VSM_SIGNAL_INIT_array));
	new_init_array_len = ARRAY_SIZE(VSM_SIGNAL_INIT_array);

	for (i = 0; i < mod_ary_len; i++) {
		for (j = 0; j < new_init_array_len; j++) {
			if (VSM_SIGNAL_MODIFY_array[i].addr ==
					VSM_SIGNAL_NEW_INIT_array[j].addr) {
				VSM_SIGNAL_NEW_INIT_array[j].value =
					VSM_SIGNAL_MODIFY_array[i].value;
				break;
			}
		}
		if (j == new_init_array_len) {
			VSM_SIGNAL_NEW_INIT_array[j].addr =
				VSM_SIGNAL_MODIFY_array[i].addr;
			VSM_SIGNAL_NEW_INIT_array[j].value =
				VSM_SIGNAL_MODIFY_array[i].value;
			new_init_array_len++;
		}
	}

}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char *chipinfo = "hello,mt6381";

	return snprintf(buf, strlen(chipinfo), "%s", chipinfo);
}

static ssize_t
store_trace_value(struct device_driver *ddri,
		   const char *buf, size_t count)
{
	int trace;

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&bio_trace, trace);
	else
		pr_err("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

static ssize_t
store_rstb_value(struct device_driver *ddri,
		  const char *buf, size_t count)
{
	int value = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &value);

	if (ret == 0) {
		if (value == 1)
			pinctrl_select_state(
				pinctrl_gpios, bio_pins_reset_high);
		else if (value == 0)
			pinctrl_select_state(pinctrl_gpios, bio_pins_reset_low);
		else
			pr_err("Wrong parameter, %d\n", value);
	} else
		pr_err("Wrong parameter, %s\n", buf);

	return count;
}

static ssize_t
store_io_value(struct device_driver *ddri,
		const char *buf, size_t count)
{
	u32 value[4] = {0};
	u32 address[4] = {0};
	struct bus_data_t data;
	int num = sscanf(buf, "%x %x %x %x %x %x %x %x ",
		&address[0], &value[0], &address[1],
		&value[1], &address[2], &value[2], &address[3], &value[3]);

	pr_notice("(%d, %x, %x, %x, %x, %x, %x, %x, %x)\n",
		num, address[0], value[0], address[1],
		value[1], address[2], value[2], address[3], value[3]);
	if (num == 1) {
		data.addr = address[0] >> 8;
		data.reg = address[0] & 0xFF;
		data.data_buf = (uint8_t *) &lastRead;
		data.length = sizeof(lastRead);
		vsm_driver_read_register(&data);

		/* mt2511_read((u16)address[0], lastRead); */
		lastAddress = address[0];
		pr_notice("address[0x04%x] = %x\n",
			address[0], *(u32 *)lastRead);
	} else if (num >= 2) {
		data.addr = address[0] >> 8;
		data.reg = address[0] & 0xFF;
		data.length = sizeof(value[0]);
		data.data_buf = (uint8_t *) &value[0];
		vsm_driver_write_register(&data);
		/* mt2511_write((u16)address[0], value[0]); */
		pr_notice("address[0x04%x] = %x\n", address[0], value[0]);

		if (num >= 4) {
			data.addr = address[1] >> 8;
			data.reg = address[1] & 0xFF;
			data.length = sizeof(value[1]);
			data.data_buf = (uint8_t *) &value[1];
			vsm_driver_write_register(&data);
			/* mt2511_write((u16)address[1], value[1]); */
			pr_notice("address[0x04%x] = %x\n",
				address[1], value[1]);
		}

		if (num >= 6) {
			data.addr = address[2] >> 8;
			data.reg = address[2] & 0xFF;
			data.length = sizeof(value[2]);
			data.data_buf = (uint8_t *) &value[2];
			vsm_driver_write_register(&data);
			/* mt2511_write((u16)address[2], value[2]); */
			pr_notice("address[0x04%x] = %x\n",
				address[2], value[2]);
		}

		if (num == 8) {
			data.addr = address[3] >> 8;
			data.reg = address[3] & 0xFF;
			data.length = sizeof(value[3]);
			data.data_buf = (uint8_t *) &value[3];
			vsm_driver_write_register(&data);
			/* mt2511_write((u16)address[3], value[3]); */
			pr_notice("address[0x04%x] = %x\n",
				address[3], value[3]);
		}
	} else
		pr_err("invalid format = '%s'\n", buf);

	return count;
}

static ssize_t show_io_value(struct device_driver *ddri, char *buf)
{

	return sprintf(buf, "address[%x] = %x\n",
		lastAddress, *(u32 *)lastRead);

}

static ssize_t
store_delay(struct device_driver *ddri,
	     const char *buf, size_t count)
{
	int delayTime = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &delayTime);

	pr_debug("(%d)\n", delayTime);
	if (0 == ret && 0 < delayTime)
		mdelay(delayTime);

	return count;
}

static ssize_t show_delay(struct device_driver *ddri, char *buf)
{

	return sprintf(buf, "usage : echo delayNum(ms in decimal) > delay\n\r");

}

static ssize_t
store_data(struct device_driver *ddri,
	     const char *buf, size_t count)
{
	int en = 0;
	int readData[3];
	int i;
	int num = sscanf(buf, "%d %d %d %d", &en,
		&readData[EKG], &readData[PPG1], &readData[PPG2]);
	unsigned int enBit = 0;
	int32_t len;
	struct signal_data_t *temp;
	uint32_t enable_data;
	struct bus_data_t data;
	/* u32 value; */
	struct signal_data_t reset_counter_array[] = {
		{0x3360, 0x0},
		{0x33D0, 0x60000000},
		{0x33E0, 0x60000000},
		{0x33C0, 0x60000000}
	};

	if (num != 4) {
		pr_err("Wrong parameters %d, %s\n", num, buf);
		return count;
	}

	bio_test_init();

	pr_debug("%d, %d, %d, %d\n",
		en, readData[EKG], readData[PPG1], readData[PPG2]);

	mutex_lock(&bio_data_collection_mutex);

	len = ARRAY_SIZE(reset_counter_array);
	temp = reset_counter_array;
	vsm_driver_write_signal(temp, len, &enable_data);
	/* reset R/W counter */
	/* disable PPG function and reset PPG1 and PPG2 write counters to 0 */
	/* mt2511_write(0x3360, 0x0); */
	/* reset PPG1 read counter to 0 */
	/* mt2511_write(0x33D0, 0x60000000); */
	/* reset PPG2 read counter to 0 */
	/* mt2511_write(0x33E0, 0x60000000); */
	/* reset EKG read counter to 0 */
	/* mt2511_write(0x33C0, 0x60000000); */

	for (i = 0; i < NUM_OF_TYPE; i++) {
		if (test_info[i].filp != NULL) {
			vfs_fsync(test_info[i].filp, 0);
			filp_close(test_info[i].filp, NULL);
			test_info[i].filp = NULL;
		}
	}

	for (i = 0; i < NUM_OF_TYPE; i++) {
		if (en & (1 << i)) {
			test_info[i].filp =
				bio_file_open(test_info[i].raw_data_path,
				O_CREAT | O_WRONLY | O_TRUNC, 0644);
			if (test_info[i].filp == NULL)
				pr_err("open %s fail\n",
					test_info[i].raw_data_path);

			test_info[i].numOfData = readData[i];
			enBit |= test_info[i].enBit;
		} else {
			test_info[i].numOfData = 0;
		}
	}
	data.addr = 0x33;
	data.reg = 0x60;
	data.length = sizeof(enBit);
	data.data_buf = (uint8_t *) &enBit;
	vsm_driver_write_register(&data);
	/* mt2511_write(0x3360, enBit); */
	mutex_unlock(&bio_data_collection_mutex);

	return count;
}

static ssize_t show_data(struct device_driver *ddri, char *buf)
{
	while ((test_info[EKG].filp != NULL) ||
		(test_info[PPG1].filp != NULL) ||
		(test_info[PPG2].filp != NULL))
		msleep(100);
	return 0;
}

static ssize_t
store_init(struct device_driver *ddri,
	    const char *buf, size_t count)
{
	int addr, value;
	int num = sscanf(buf, "%x %x", &addr, &value);

	pr_notice("addr = %x, value = %x\n", addr, value);
	if (num == 2) {
		if (addr == 0x2330) {
			set_AFE_TCTRL_CON2 = 1;
			AFE_TCTRL_CON2 = value;
		} else if (addr == 0x2334) {
			set_AFE_TCTRL_CON3 = 1;
			AFE_TCTRL_CON3 = value;
		} else
			insert_modify_setting(addr, value);
	} else if (num == 1) {
		if (addr == 0) {
			clear_modify_setting();
			set_AFE_TCTRL_CON2 = 0;
			set_AFE_TCTRL_CON3 = 0;
		} else if (addr == 0x2330)
			set_AFE_TCTRL_CON2 = 0;
		else if (addr == 0x2334)
			set_AFE_TCTRL_CON3 = 0;
		else
			remove_modify_setting(addr, value);
	}

	update_new_init_setting();

	return count;
}

static ssize_t show_init(struct device_driver *ddri, char *buf)
{
	int i;
	char tmp_buf[100];
	int len = 0;

	strcpy(buf, "Modify settings:\n");
	if (set_AFE_TCTRL_CON2 == 1) {
		sprintf(tmp_buf, "0x2330, 0x%08x\n", AFE_TCTRL_CON2);
		strcat(buf, tmp_buf);
	}
	if (set_AFE_TCTRL_CON3 == 1) {
		sprintf(tmp_buf, "0x2334, 0x%08x\n", AFE_TCTRL_CON3);
		strcat(buf, tmp_buf);
	}
	for (i = 0; i < mod_ary_len; i++) {
		sprintf(tmp_buf, "0x%x, 0x%08x\n",
			VSM_SIGNAL_MODIFY_array[i].addr,
			VSM_SIGNAL_MODIFY_array[i].value);
		strcat(buf, tmp_buf);
	}

	strcat(buf, "New init settings:\n");
	if (mod_ary_len != 0) {
		for (i = 0; i < new_init_array_len; i++) {
			sprintf(tmp_buf, "0x%x, 0x%08x\n",
				VSM_SIGNAL_NEW_INIT_array[i].addr,
				VSM_SIGNAL_NEW_INIT_array[i].value);
			strcat(buf, tmp_buf);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(VSM_SIGNAL_INIT_array); i++) {
			sprintf(tmp_buf, "0x%x, 0x%08x\n",
				VSM_SIGNAL_INIT_array[i].addr,
				VSM_SIGNAL_INIT_array[i].value);
			strcat(buf, tmp_buf);
		}
	}

	len = strlen(buf);

	return len;
}

static ssize_t
store_polling_delay(
		     struct device_driver *ddri,
		     const char *buf, size_t count)
{
	int delayTime = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &delayTime);

	pr_notice("(%d)\n", delayTime);
	if (0 == ret && 0 <= delayTime)
		polling_delay = delayTime;

	return count;
}

static ssize_t show_polling_delay(struct device_driver *ddri, char *buf)
{
	return sprintf(buf, "polling delay = %d\n", polling_delay);
}

static ssize_t
store_latency_test(
		    struct device_driver *ddri,
		    const char *buf, size_t count)
{
	int num = sscanf(buf, "%d %d %d", &latency_test_data.first_data,
		&latency_test_data.second_data, &latency_test_data.ekg_num);
	latency_test_data.ppg1_num = latency_test_data.ekg_num;
	latency_test_data.ppg2_num = latency_test_data.ekg_num;

	pr_notice("first_data = %d, second_data = %d, delay_num = %d\n",
		latency_test_data.first_data,
		latency_test_data.second_data, latency_test_data.ekg_num);

	if (num != 3) {
		latency_test_data.ekg_num = 0;
		latency_test_data.ppg1_num = 0;
		latency_test_data.ppg2_num = 0;
	}

	return count;
}

static ssize_t
store_cali(
	    struct device_driver *ddri,
	    const char *buf, size_t count)
{
	struct biometric_cali data;

	int num = sscanf(buf, "%d %d", &data.pga6, &data.ambdac5_5);

	if (num != 2)
		pr_err("Invalid parameter : %s\n", buf);
	else
		MT6381_WriteCalibration(&data);

	return count;
}

static ssize_t show_cali(struct device_driver *ddri, char *buf)
{
	return sprintf(buf, "pga6 = %d, ambdac5.5 = %d, dc_offset = %lld\n",
		pga6, ambdac5_5, dc_offset);
}

static ssize_t
store_stress(
	      struct device_driver *ddri,
	      const char *buf,
	      size_t count)
{
	int err;

	err = kstrtoint(buf, 10, &stress_test);

	if (err != 0) {
		pr_err("Wrong parameters %d, %s\n", err, buf);
		return count;
	}

	bio_stress_test_init();

	pr_debug("%d\n", stress_test);

	mutex_lock(&bio_data_collection_mutex);

	if (stress_test != 0) {
		vsm_driver_set_signal(VSM_SIGNAL_EKG);
		vsm_driver_set_signal(VSM_SIGNAL_PPG1);
		vsm_driver_set_signal(VSM_SIGNAL_PPG2);
		vsm_driver_set_led(VSM_SIGNAL_PPG2, true);
	} else {
		vsm_driver_disable_signal(VSM_SIGNAL_EKG);
		vsm_driver_disable_signal(VSM_SIGNAL_PPG1);
		vsm_driver_disable_signal(VSM_SIGNAL_PPG2);
	}

	mutex_unlock(&bio_data_collection_mutex);

	return count;
}

static ssize_t
store_offline_mode(
	       struct device_driver *ddri,
	       const char *buf,
	       size_t count)
{
	char file_name[MAX_FILE_LENGTH];
	int offline_mode;
	int num;

	num = sscanf(buf, "%d, %s", &offline_mode, file_name);

	if (num == 2 && offline_mode == 1) {
		offline_mode_en = true;
		strcpy(offline_mode_file_name, file_name);
	} else if ((num == 1 || num == 2) && offline_mode == 0) {
		offline_mode_en = false;
	} else
		pr_err("Wrong parameters, %s\n", buf);

	return count;
}

static ssize_t show_offline_mode(struct device_driver *ddri, char *buf)
{
	char raw_data_path[128];
	struct file *filp;

	strcpy(raw_data_path, "/data/bio/");
	strcat(raw_data_path, offline_mode_file_name);

	if (offline_mode_en) {
		filp = bio_file_open(raw_data_path, O_RDONLY, 0644);
		if (filp == NULL)
			strcpy(offline_mode_file_name, "File not exist");
		else
			filp_close(filp, NULL);
	}
	return sprintf(buf, "%d, %s\n",
		offline_mode_en, offline_mode_file_name);
}

static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(trace, 0644, NULL, store_trace_value);
static DRIVER_ATTR(rstb, 0644, NULL, store_rstb_value);
static DRIVER_ATTR(io, 0644, show_io_value, store_io_value);
static DRIVER_ATTR(delay, 0644, show_delay, store_delay);
static DRIVER_ATTR(data, 0644, show_data, store_data);
static DRIVER_ATTR(init, 0644, show_init, store_init);
static DRIVER_ATTR(polling_delay, 0644,
	show_polling_delay, store_polling_delay);
static DRIVER_ATTR(latency_test, 0644, NULL, store_latency_test);
static DRIVER_ATTR(cali, 0644, show_cali, store_cali);
static DRIVER_ATTR(stress, 0644, NULL, store_stress);
static DRIVER_ATTR(offline_mode, 0644, show_offline_mode, store_offline_mode);

static struct driver_attribute *mt6381_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_trace,	/*trace log */
	&driver_attr_rstb,	/* set rstb */
	&driver_attr_io,
	&driver_attr_delay,
	&driver_attr_data,
	&driver_attr_init,
	&driver_attr_polling_delay,
	&driver_attr_latency_test,
	&driver_attr_cali,
	&driver_attr_stress,
	&driver_attr_offline_mode,
};

static int mt6381_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(mt6381_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, mt6381_attr_list[idx]);
		if (err != 0) {
			pr_err("driver_create_file (%s) = %d\n",
				mt6381_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}

static int mt6381_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(mt6381_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, mt6381_attr_list[idx]);

	return err;
}


/* mt6381 spec relevant */
enum vsm_status_t
vsm_driver_read_register_batch(
				enum vsm_sram_type_t sram_type, u8 *buf,
				u16 length)
{
	int res;
	/* ///int i; */
	struct i2c_msg *msg = NULL;

	switch (sram_type) {
	case VSM_SRAM_EKG:
		msg = ekg_msg;
		break;
	case VSM_SRAM_PPG1:
		msg = ppg1_msg;
		break;
	case VSM_SRAM_PPG2:
		msg = ppg2_msg;
		break;
	default:
		return -1;
	}

	res = i2c_transfer(mt6381_i2c_client->adapter, msg, length * 2);
	if (res < 0)
		pr_err("mt6381 i2c read failed. errno:%d\n", res);
	else
		memcpy(buf, msg[1].buf, length * 4);

	return res;
}

enum vsm_status_t vsm_driver_read_register(struct bus_data_t *data)
{
	int32_t ret = 0, i = 0;

	if (data == NULL) {
		pr_debug("NULL data parameter\n");
		return VSM_STATUS_INVALID_PARAMETER;
	}

	for (i = 0; i < 5; i++) {
		ret = mt6381_i2c_write_read(data->addr, data->reg,
			data->data_buf, data->length);
		if (ret == VSM_STATUS_OK)
			break;
	}

	if (DBG_READ) {
		pr_debug("%s():addr 0x%x, reg 0x%x, len %d, value 0x%x",
			 __func__, data->addr, data->reg, data->length,
			 *(uint32_t *) data->data_buf);
	}

	if (ret < 0) {
		pr_err("vsm_driver_read_register error (%d)\r\n", ret);
		return VSM_STATUS_ERROR;
	} else {
		return VSM_STATUS_OK;
	}

	return ret;
}

enum vsm_status_t vsm_driver_write_register(struct bus_data_t *data)
{
	unsigned char txbuffer[MAX_WRITE_LENGTH * 2];
	unsigned char reg_addr = data->reg;
	unsigned char data_len = data->length;
	int32_t ret, i = 0;

	if (data == NULL) {
		pr_err("NULL data parameter\n");
		return VSM_STATUS_INVALID_PARAMETER;
	}

	if (DBG_WRITE) {
		pr_debug("%s():addr 0x%x, reg 0x%x, value 0x%x, len %d",
			 __func__, data->addr, reg_addr,
			 *(uint32_t *) (data->data_buf), data_len);
	}

	txbuffer[0] = reg_addr;
	memcpy(txbuffer + 1, data->data_buf, data_len);
	for (i = 0; i < 5; i++) {
		ret = mt6381_i2c_write(data->addr, txbuffer, data_len + 1);
		if (ret == (data_len + 1))
			break;

		pr_err("mt6381_i2c_write error(%d), reg_addr = %x, reg_data = %x\n",
			ret, reg_addr, *(uint32_t *) (data->data_buf));
	}

	if (ret < 0) {
		pr_err("I2C Trasmit error(%d)\n", ret);
		return VSM_STATUS_ERROR;
	} else {
		return VSM_STATUS_OK;
	}
}

static enum vsm_status_t vsm_driver_write_signal(
					  struct signal_data_t *reg_addr,
					  int32_t len,
					  uint32_t *enable_data)
{
	struct bus_data_t data;
	uint32_t data_buf;
	int32_t i = 0;
	enum vsm_status_t err = VSM_STATUS_OK;

	if (!reg_addr || !enable_data) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	for (i = 0; i < len; i++) {
		data.addr = ((reg_addr + i)->addr & 0xFF00) >> 8;
		data.reg = ((reg_addr + i)->addr & 0xFF);
		data.length = sizeof(data_buf);
		data.data_buf = (uint8_t *) &data_buf;

		/* process with combo signal */
		data_buf = (reg_addr + i)->value;

		if (inCali && ((reg_addr + i)->addr == 0x3318)) {
			data_buf &= ~(PPG_AMDAC2_MASK |
				PPG_AMDAC1_MASK | PPG_PGA_GAIN_MASK);
			data_buf |= (cali_ambdac_amb << PPG_AMDAC2_OFFSET);
			data_buf |= (cali_ambdac_led << PPG_AMDAC1_OFFSET);
			data_buf |= (cali_pga << PPG_PGA_GAIN_OFFSET);
			if (atomic_read(&bio_trace) != 0)
				pr_debug("0x3318 = %x, %d, %d, %d\n",
					data_buf,
					cali_ambdac_amb,
					cali_ambdac_led,
					cali_pga);
		} else if (inCali && (cali_ambdac_amb == 0) &&
			((reg_addr + i)->addr == 0x331C)) {
			data_buf &= ~(0x800);
		} else if (inCali && ((reg_addr + i)->addr == 0x332C)) {
			data_buf = 0;
			if (atomic_read(&bio_trace) != 0)
				pr_debug("0x332C = %x\n", data_buf);
		}

		data.length = sizeof(data_buf);
		err = vsm_driver_write_register(&data);

		if (((reg_addr + i)->addr == 0x3300) &&
			((reg_addr + i + 1)->addr == 0x3300))
			mdelay(50);

	}
	return err;
}

static enum vsm_status_t vsm_driver_set_signal(enum vsm_signal_t signal)
{
	enum vsm_status_t err = VSM_STATUS_OK;
	int32_t len;
	struct signal_data_t *temp;
	uint32_t enable_data;

	if (current_signal == 0) {
		/* reset digital*/
		if (!IS_ERR(pinctrl_gpios)) {
			pinctrl_select_state(
				pinctrl_gpios, bio_pins_reset_low);
			msleep(20);
			pinctrl_select_state(
				pinctrl_gpios, bio_pins_reset_high);
			msleep(20);
		}
		/* initial setting */
		if (new_init_array_len != 0) {
			len = new_init_array_len;
			temp = VSM_SIGNAL_NEW_INIT_array;
		} else {
			len = ARRAY_SIZE(VSM_SIGNAL_INIT_array);
			temp = VSM_SIGNAL_INIT_array;
		}
		vsm_driver_write_signal(temp, len, &enable_data);
		enable_time = sched_clock();
		pre_t[VSM_SRAM_EKG] = sched_clock();
		pre_t[VSM_SRAM_PPG1] = sched_clock();
		pre_t[VSM_SRAM_PPG2] = sched_clock();
		if (!inCali)
			ppg_control_init();
		data_dropped = false;
		numOfData[VSM_SRAM_EKG] = 0;
		numOfData[VSM_SRAM_PPG1] = 0;
		numOfData[VSM_SRAM_PPG2] = 0;
		numOfEKGDataNeedToDrop = 2;
		ppg1_buf2_len = 0;
		ppg2_buf2_len = 0;
		agc_ppg1_buf_len = 0;
		agc_ppg2_buf_len = 0;
		if (latency_test_data.ekg_num != 0) {
			latency_test_data.ppg1_num = latency_test_data.ekg_num;
			latency_test_data.ppg2_num = latency_test_data.ekg_num;
			latency_test_data.in_latency_test = true;
		} else
			latency_test_data.in_latency_test = false;
	}

	/* Turn on PPG1 led for finger on/off detection. */
	/* PPG2 led will be turned on later after finger detected. */
	if ((signal == VSM_SIGNAL_PPG1) || (signal == VSM_SIGNAL_PPG2))
		vsm_driver_set_led(VSM_SIGNAL_PPG1, true);

	current_signal |= signal;

	return err;
}

static enum vsm_status_t vsm_driver_set_signal_offline_mode(
	enum vsm_signal_t signal)
{
	int i;
	int index = driver_type_to_index(signal);
	int apk_type = driver_type_to_apk_type(signal);

	if (olm_info_p == NULL) {
		olm_info_p = kzalloc(
			sizeof(struct offline_mode_info), GFP_KERNEL);
		if (olm_info_p == NULL)
			return 0;

		memset(olm_info_p, 0, sizeof(struct offline_mode_info));

		olm_info_p->en_time = sched_clock();

		strcpy(olm_info_p->raw_data_path, "/data/bio/");
		strcat(olm_info_p->raw_data_path, offline_mode_file_name);
		for (i = 0; i < ARRAY_SIZE(olm_info_p->sensor); i++) {
			olm_info_p->sensor[i].filp =
				bio_file_open(olm_info_p->raw_data_path,
					O_RDONLY, 0644);
			if (olm_info_p->sensor[i].filp == NULL) {
				pr_err("open %s fail\n",
					olm_info_p->raw_data_path);
				return VSM_STATUS_ERROR;
			}
		}
	}

	if (index >= 0) {
		olm_info_p->sensor[index].apk_type = apk_type;
		olm_info_p->sensor[index].num_of_data = 0;
	}

	return VSM_STATUS_OK;
}

static enum vsm_status_t vsm_driver_set_read_counter(
	enum vsm_sram_type_t sram_type, uint32_t *counter)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;

	if (counter == NULL) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	switch (sram_type) {
	case VSM_SRAM_EKG:
		data.reg = SRAM_EKG_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG1:
		data.reg = SRAM_PPG1_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG2:
		data.reg = SRAM_PPG2_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_BISI:
		data.reg = SRAM_BISI_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_NUMBER:
	default:
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	*counter |= 0x60000000;
	data.addr = MT2511_SLAVE_II;
	data.data_buf = (uint8_t *) counter;
	data.length = sizeof(uint32_t);
	err = vsm_driver_write_register(&data);

	if (err != VSM_STATUS_OK)
		pr_err("vsm_driver_set_read_counter fail : %d\n", err);

	return err;
}

static enum vsm_status_t vsm_driver_get_read_counter(
	enum vsm_sram_type_t sram_type, uint32_t *counter)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;

	if (counter == NULL) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	switch (sram_type) {
	case VSM_SRAM_EKG:
		data.reg = SRAM_EKG_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG1:
		data.reg = SRAM_PPG1_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG2:
		data.reg = SRAM_PPG2_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_BISI:
		data.reg = SRAM_BISI_READ_COUNT_ADDR;
		break;

	case VSM_SRAM_NUMBER:
	default:
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	data.addr = MT2511_SLAVE_II;
	data.data_buf = (uint8_t *) counter;
	data.length = sizeof(uint32_t);
	err = vsm_driver_read_register(&data);

	if (err == VSM_STATUS_OK) {
		*counter =
		    ((*counter & 0x01ff0000) >> 16) >
		    VSM_SRAM_LEN ? 0 : ((*counter & 0x01ff0000) >> 16);
		if (DBG)
			pr_debug("vsm_driver_get_read_counter 0x%x \r\n",
				*counter);
	}
	return err;
}


static enum vsm_status_t
vsm_driver_write_counter(
			  enum vsm_sram_type_t sram_type,
			  uint32_t *write_counter)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t counter = 0;

	if (write_counter == NULL) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	switch (sram_type) {
	case VSM_SRAM_EKG:
		data.reg = SRAM_EKG_WRITE_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG1:
		data.reg = SRAM_PPG1_WRITE_COUNT_ADDR;
		break;

	case VSM_SRAM_PPG2:
		data.reg = SRAM_PPG2_WRITE_COUNT_ADDR;
		break;

	case VSM_SRAM_BISI:
		data.reg = SRAM_BISI_WRITE_COUNT_ADDR;
		break;

	case VSM_SRAM_NUMBER:
	default:
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	data.addr = MT2511_SLAVE_II;
	data.data_buf = (uint8_t *) &counter;
	data.length = sizeof(uint32_t);
	err = vsm_driver_read_register(&data);

	if (err == VSM_STATUS_OK) {
		counter = (counter & 0x01ff0000) >> 16;
		if (DBG)
			pr_debug("vsm_driver_write_counter 0x%x \r\n", counter);
	}
	*write_counter = counter;

	return err;
}

static enum vsm_status_t vsm_driver_read_sram(
				       enum vsm_sram_type_t sram_type,
				       uint32_t *data_buf, uint32_t *amb_buf,
				       u32 *len)
{
	int err = VSM_STATUS_OK;
	uint32_t temp;
	uint32_t read_counter = 0;
	uint32_t write_counter = 0;
	uint32_t amb_read_counter = 0;
	uint32_t sram_len;
	int64_t current_timestamp;
	static int64_t rate[3] = {0};
	uint32_t i;
	static uint32_t pre_amb_data;

	if (data_buf == NULL || len == NULL) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}
	/* 1. compute how many sram data */
	/* read counter */
	vsm_driver_get_read_counter(sram_type, &read_counter);
	do {
		temp = read_counter;
		vsm_driver_get_read_counter(sram_type, &read_counter);
	} while (temp != read_counter);

	/* write counter */
	vsm_driver_write_counter(sram_type, &write_counter);
	do {
		temp = write_counter;
		vsm_driver_write_counter(sram_type, &write_counter);
	} while (temp != write_counter);

	/* only get even number to prevent the reverse of ppg and ambient */
	/* compute sram length */
	sram_len =
	    (write_counter >=
	     read_counter) ? (write_counter - read_counter) :
	     (VSM_SRAM_LEN - read_counter + write_counter);
	sram_len = ((sram_len % 2) == 0) ? sram_len : sram_len - 1;

	*len = sram_len;

	current_timestamp = sched_clock();
	/* sram will wraparound after 375000000ns =
	 * VSM_SRAM_LEN * (1000000000 / 1024)
	 */
	if (((current_timestamp - pre_t[sram_type]) >= 350000000LL) ||
		(((current_timestamp - pre_t[sram_type]) >= 300000000LL) &&
		sram_len < 100))
		aee_kernel_warning("MT6381", "Data dropped!! %d, %lld, %d\n",
			sram_type,
			current_timestamp - pre_t[sram_type],
			sram_len);
	if (atomic_read(&bio_trace) != 0)
		pr_debug("Data read, %d, %lld, %d\n", sram_type,
			current_timestamp - pre_t[sram_type],
			sram_len);
	pre_t[sram_type] = current_timestamp;

	if (sram_len > 0) {
		if (sram_type == VSM_SRAM_EKG) {
			/* drop fisrt two garbage bytes
			 * of EKG data after enable
			 */
			while (numOfEKGDataNeedToDrop > 0 && sram_len > 0) {
				err = vsm_driver_read_register_batch(
					sram_type, (u8 *) data_buf, 1);
				if (err < 0) {
					err = VSM_STATUS_INVALID_PARAMETER;
					*len = sram_len = 0;
				}
				numOfEKGDataNeedToDrop--;
				sram_len--;
			}

			*len = sram_len;
			if (sram_len <= 0)
				return err;
		}
		numOfData[sram_type] += sram_len;
		rate[sram_type] = numOfData[sram_type] *
			1000000000 / (current_timestamp - enable_time);

		/* 2. get sram data to data_buf */
		/* get sram data */
		err = vsm_driver_read_register_batch(
			sram_type, (u8 *) data_buf, sram_len);
		if (err < 0) {
			err = VSM_STATUS_INVALID_PARAMETER;
			*len = sram_len = 0;
		}

		/* Read out ambient data */
		if (sram_type == VSM_SRAM_PPG2 && amb_buf != NULL) {
			amb_read_counter = read_counter;
			for (i = 0; i < sram_len; i++) {
				/* down sample from 512Hz to 16Hz */
				if (amb_read_counter % 64 == 0) {
					vsm_driver_set_read_counter(
						VSM_SRAM_PPG1,
						&amb_read_counter);
					vsm_driver_read_register_batch(
						VSM_SRAM_PPG1,
						(u8 *)&amb_buf[i], 1);
					pre_amb_data = amb_buf[i];

					vsm_driver_get_read_counter(
						VSM_SRAM_PPG1,
						&read_counter);
					do {
						temp = read_counter;
						vsm_driver_get_read_counter(
							VSM_SRAM_PPG1,
							&read_counter);
					} while (temp != read_counter);
				} else
					amb_buf[i] = pre_amb_data;
				amb_read_counter = (amb_read_counter + 1) %
					VSM_SRAM_LEN;
			}
		}

		if (atomic_read(&bio_trace) != 0) {
			pr_debug("%s, type: %d, len,%d,%d,%d,%lld\n",
				__func__, sram_type, sram_len,
				read_counter, write_counter, rate[sram_type]);
			pr_debug("delta: %lld, %lld, %lld, %lld\n",
				sched_clock() - current_timestamp,
				numOfData[0], numOfData[1], numOfData[2]);
		}
	} else {
		err = VSM_STATUS_INVALID_PARAMETER;
	}

	return err;
}

static enum vsm_status_t _vsm_driver_read_offline_mode_data(
	struct raw_data_info *info)
{
	ssize_t count;
	char buf[MAX_LENGTH_PER_LINE];
	char *pbuf;
	char *str;
	int result;
	enum vsm_status_t ret = VSM_STATUS_OK;

	do {
		pbuf = buf;
		memset(pbuf, 0, MAX_LENGTH_PER_LINE);
		count = bio_file_read_line(
			info->filp, 0, pbuf, MAX_LENGTH_PER_LINE);
		if (count > 0) {
			int err;
			int i;

			str = strsep(&pbuf, ",");
			if (str == NULL) {
				pr_err("Parsing fail, %s\n", pbuf);
				ret = VSM_STATUS_ERROR;
				break;
			}

			if (isdigit(str[0])) {
				err = kstrtoint(str, 10, &result);
				if (err != 0) {
					pr_err("Parsing fail %d, %s\n",
						err, str);
					ret = VSM_STATUS_ERROR;
					break;
				}
			} else
				continue;

			if (result != info->apk_type)
				continue;

			str = strsep(&pbuf, ","); /* read out SN */

			for (i = 0; i < NUM_OF_DATA_PER_LINE; i++) {
				str = strsep(&pbuf, ",");
				if (str == NULL) {
					pr_err("Parsing fail, %s\n", pbuf);
					ret = VSM_STATUS_ERROR;
					break;
				}
				err = kstrtoint(str, 10, &result);
				if (err != 0) {
					pr_err("Parsing fail %d, %s\n",
						err, pbuf);
					ret = VSM_STATUS_ERROR;
					break;
				}
				info->raw_data[i] = result;
			}

			break;
		}
	} while (count > 0);

	return ret;
}

static enum vsm_status_t vsm_driver_read_offline_mode_data(
	enum vsm_signal_t type, uint32_t *data_buf, u32 *len)
{
	int64_t current_time;
	int target_num;
	int index;

	index = driver_type_to_index(type);
	*len = 0;
	current_time = sched_clock();

	if (index < 0) {
		pr_err("Wrong type %d\n", type);
		return VSM_STATUS_INVALID_PARAMETER;
	}

	if (olm_info_p == NULL) {
		pr_err("olm_info_p == NULL\n");
		return VSM_STATUS_ERROR;
	}

	if (olm_info_p->sensor[index].filp == NULL) {
		pr_err("olm_info_p->sensor[index].filp == NULL\n");
		return VSM_STATUS_ERROR;
	}

	target_num = (current_time - olm_info_p->en_time) * 512 / 1000000000;

	for (; olm_info_p->sensor[index].num_of_data < target_num;
		olm_info_p->sensor[index].num_of_data++) {
		if ((olm_info_p->sensor[index].num_of_data %
			NUM_OF_DATA_PER_LINE) == 0) {
			/* read out one line data */
			_vsm_driver_read_offline_mode_data(
				&olm_info_p->sensor[index]);
		}

		data_buf[(*len)++] = olm_info_p->sensor[index].raw_data[
			olm_info_p->sensor[index].num_of_data %
			NUM_OF_DATA_PER_LINE];
	}

	return VSM_STATUS_OK;
}

enum vsm_status_t vsm_driver_update_register(void)
{
	uint32_t write_data;
	struct bus_data_t data;
	int err = VSM_STATUS_OK;

	data.addr = (UPDATE_COMMAND_ADDR & 0xFF00) >> 8;
	data.reg = (UPDATE_COMMAND_ADDR & 0xFF);
	write_data = (uint32_t) 0xFFFF0002;
	data.data_buf = (uint8_t *) &write_data;
	data.length = sizeof(write_data);

	err = vsm_driver_write_register(&data);
	if (err == VSM_STATUS_OK) {
		write_data = 0xFFFF0000;
		err = vsm_driver_write_register(&data);
	}
	return err;
}

enum vsm_status_t vsm_driver_set_tia_gain(
	enum vsm_signal_t ppg_type, enum vsm_tia_gain_t input)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t read_data;

	if (ppg_type == VSM_SIGNAL_PPG1 || ppg_type == VSM_SIGNAL_PPG2) {

		switch (ppg_type) {
		case VSM_SIGNAL_PPG1:
			data.reg = (PPG1_GAIN_ADDR & 0xFF);
			data.addr = (PPG1_GAIN_ADDR & 0xFF00) >> 8;
			break;
		case VSM_SIGNAL_PPG2:
			data.reg = (PPG2_GAIN_ADDR & 0xFF);
			data.addr = (PPG2_GAIN_ADDR & 0xFF00) >> 8;
			break;
		default:
			data.reg = (PPG1_GAIN_ADDR & 0xFF);
			data.addr = (PPG1_GAIN_ADDR & 0xFF00) >> 8;
			break;
		}

		data.data_buf = (uint8_t *) &read_data;
		data.length = sizeof(read_data);
		err = vsm_driver_read_register(&data);

		if (err == VSM_STATUS_OK) {
			if (ppg_type == VSM_SIGNAL_PPG1) {
				read_data &= ~PPG1_GAIN_MASK;
				read_data |= (input & 0x7) << PPG1_GAIN_OFFSET;
			} else if (ppg_type == VSM_SIGNAL_PPG2) {
				read_data &= ~PPG2_GAIN_MASK;
				read_data |= (input & 0x7) << PPG2_GAIN_OFFSET;
			}
			err = vsm_driver_write_register(&data);
			/* update register setting */
			vsm_driver_update_register();
		}
	} else {
		err = VSM_STATUS_INVALID_PARAMETER;
	}

	return err;
}

enum vsm_status_t vsm_driver_set_pga_gain(enum vsm_pga_gain_t input)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t read_data;

	data.reg = (PPG_PGA_GAIN_ADDR & 0xFF);
	data.addr = (PPG_PGA_GAIN_ADDR & 0xFF00) >> 8;
	data.data_buf = (uint8_t *) &read_data;
	data.length = sizeof(read_data);
	err = vsm_driver_read_register(&data);

	if (err == VSM_STATUS_OK) {
		if (input > VSM_PGA_GAIN_6)
			input = VSM_PGA_GAIN_6;
		read_data &= ~PPG_PGA_GAIN_MASK;
		read_data |= (input & 0x7) << PPG_PGA_GAIN_OFFSET;
		err = vsm_driver_write_register(&data);
		/* update register setting */
		vsm_driver_update_register();
	}

	return err;
}

enum vsm_status_t vsm_driver_set_led_current(
	enum vsm_led_type_t led_type,
	enum vsm_signal_t ppg_type, uint32_t input)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t read_data;

	if (led_type == VSM_LED_1 || led_type == VSM_LED_2) {
		switch (ppg_type) {
		case VSM_SIGNAL_PPG1:
			data.reg = (PPG1_CURR_ADDR & 0xFF);
			data.addr = (PPG1_CURR_ADDR & 0xFF00) >> 8;
			break;
		case VSM_SIGNAL_PPG2:
			data.reg = (PPG2_CURR_ADDR & 0xFF);
			data.addr = (PPG2_CURR_ADDR & 0xFF00) >> 8;
			break;
		default:
			data.reg = (PPG1_CURR_ADDR & 0xFF);
			data.addr = (PPG1_CURR_ADDR & 0xFF00) >> 8;
			break;
		}

		data.data_buf = (uint8_t *) &read_data;
		data.length = sizeof(read_data);
		err = vsm_driver_read_register(&data);

		if (err == VSM_STATUS_OK) {
			if (led_type == VSM_LED_1) {
				read_data &= ~PPG1_CURR_MASK;
				read_data |= (input & 0xFF) << PPG1_CURR_OFFSET;
			} else if (led_type == VSM_LED_2) {
				read_data &= ~PPG2_CURR_MASK;
				read_data |= (input & 0xFF) << PPG2_CURR_OFFSET;
			}
			err = vsm_driver_write_register(&data);
			/* update register setting */
			vsm_driver_update_register();
		}
	} else {
		err = VSM_STATUS_INVALID_PARAMETER;
	}

	return err;
}

int32_t vsm_driver_chip_version_get(void)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t read_data;

	if (vsm_chip_version == -1) {
		data.reg = (CHIP_VERSION_ADDR & 0xFF);
		data.addr = (CHIP_VERSION_ADDR & 0xFF00) >> 8;
		data.data_buf = (uint8_t *) &read_data;
		data.length = sizeof(read_data);
		err = vsm_driver_read_register(&data);
		if (err == VSM_STATUS_OK) {
			pr_notice("read back chip version:0x%x", read_data);
			if (read_data == 0x25110000)
				vsm_chip_version = CHIP_VERSION_E2;
			else if (read_data == 0xFFFFFFFF)
				vsm_chip_version = CHIP_VERSION_E1;
			else
				vsm_chip_version = CHIP_VERSION_UNKNOWN;
		} else {
			vsm_chip_version = CHIP_VERSION_UNKNOWN;
		}
	}

	return vsm_chip_version;
}

enum vsm_status_t vsm_driver_set_ambdac_current(
	enum vsm_ambdac_type_t ambdac_type,
	enum vsm_ambdac_current_t currentt)
{
	int err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t read_data;

	data.reg = (PPG_AMDAC_ADDR & 0xFF);
	data.addr = (PPG_AMDAC_ADDR & 0xFF00) >> 8;
	data.data_buf = (uint8_t *) &read_data;
	data.length = sizeof(read_data);
	err = vsm_driver_read_register(&data);

	if (err == VSM_STATUS_OK) {
		if (currentt > VSM_AMBDAC_CURR_06_MA)
			currentt = VSM_AMBDAC_CURR_06_MA;
		if (vsm_driver_chip_version_get() == CHIP_VERSION_E1) {
			read_data &= ~PPG_AMDAC_MASK;
			read_data |= (currentt & 0xF) << PPG_AMDAC_OFFSET;
			err = vsm_driver_write_register(&data);
		} else if (vsm_driver_chip_version_get() == CHIP_VERSION_E2) {
			if (ambdac_type == VSM_AMBDAC_1) {
				read_data &= ~PPG_AMDAC1_MASK;
				read_data |=
					(currentt & 0x7) << PPG_AMDAC1_OFFSET;
				err = vsm_driver_write_register(&data);
			} else if (ambdac_type == VSM_AMBDAC_2) {
				read_data &= ~PPG_AMDAC2_MASK;
				read_data |=
					(currentt & 0x7) << PPG_AMDAC2_OFFSET;
				err = vsm_driver_write_register(&data);
			}
		}
		/* update register setting */
		vsm_driver_update_register();
	}
	return err;
}

enum vsm_status_t vsm_driver_enable_signal(enum vsm_signal_t signal)
{
	enum vsm_status_t err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t enable_data, reg_data;

	/* {0x3360,0x00000187}   //Enable Mode[0:5]=[018:124:187:164:33C:3FF] */
	switch (signal) {
	case VSM_SIGNAL_EKG:
	case VSM_SIGNAL_EEG:
	case VSM_SIGNAL_EMG:
	case VSM_SIGNAL_GSR:
		enable_data = 0x18;
		break;
	case VSM_SIGNAL_PPG1:
	case VSM_SIGNAL_PPG1_512HZ:
		/* enable_data = 0x124; */
		enable_data = 0x124;
		break;
	case VSM_SIGNAL_PPG2:
		/* enable_data = 0x144; */
		enable_data = 0x144;
		break;
	case VSM_SIGNAL_BISI:
		/* enable_data = 0x187; */
		enable_data = 0x187;
		break;
	default:
		enable_data = 0x00;
		break;
	}
	data.addr = (DIGITAL_START_ADDR & 0xFF00) >> 8;
	data.reg = (DIGITAL_START_ADDR & 0xFF);
	data.data_buf = (uint8_t *) &reg_data;
	data.length = sizeof(reg_data);
	err = vsm_driver_read_register(&data);
	if (err == VSM_STATUS_OK) {
		reg_data |= (enable_data);
		err = vsm_driver_write_register(&data);
	}

	return err;
}

enum vsm_status_t vsm_driver_set_led(enum vsm_signal_t signal, bool enable)
{
	enum vsm_status_t err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t reg_data;

	if (signal == VSM_SIGNAL_PPG1 && ppg1_led_status != enable) {
		ppg1_led_status = enable;
		data.addr = (0x2330 & 0xFF00) >> 8;
		data.reg = (0x2330 & 0xFF);
		if (enable)
			reg_data = 0x00C10182;
		else
			reg_data = 0x0;
		data.length = sizeof(reg_data);
		data.data_buf = (uint8_t *) &reg_data;
		err = vsm_driver_write_register(&data);

		/* update register setting */
		vsm_driver_update_register();
	} else if (signal == VSM_SIGNAL_PPG2 && ppg2_led_status != enable) {
		ppg2_led_status = enable;
		data.addr = (0x2334 & 0xFF00) >> 8;
		data.reg = (0x2334 & 0xFF);
		if (enable)
			reg_data = 0x02430304;
		else
			reg_data = 0x0;
		data.length = sizeof(reg_data);
		data.data_buf = (uint8_t *) &reg_data;
		err = vsm_driver_write_register(&data);

		/* update register setting */
		vsm_driver_update_register();
	}

	return err;
}

static enum vsm_status_t vsm_driver_disable_signal(enum vsm_signal_t signal)
{
	int32_t len;
	struct signal_data_t *temp;
	uint32_t enable_data;
	enum vsm_status_t err = VSM_STATUS_OK;

	current_signal &= ~signal;

	if ((current_signal & VSM_SIGNAL_PPG1) == 0 &&
		(current_signal & VSM_SIGNAL_PPG2) == 0)
		vsm_driver_set_led(VSM_SIGNAL_PPG1, false);
	if (signal == VSM_SIGNAL_PPG2)
		vsm_driver_set_led(signal, false);

	if (current_signal == 0) {
		len = ARRAY_SIZE(VSM_SIGNAL_IDLE_array);
		temp = VSM_SIGNAL_IDLE_array;
		vsm_driver_write_signal(temp, len, &enable_data);
	}

	return err;
}

static enum vsm_status_t vsm_driver_disable_signal_offline_mode(
	enum vsm_signal_t signal)
{
	bool free_olm_info_p = true;
	int i;

	if (olm_info_p == NULL) {
		pr_err("olm_info_p is NULL\n");
		return VSM_STATUS_ERROR;
	}

	if (driver_type_to_index(signal) >= 0 &&
		olm_info_p->sensor[driver_type_to_index(signal)].filp != NULL) {
		filp_close(
			olm_info_p->sensor[driver_type_to_index(signal)].filp,
			NULL);
		olm_info_p->sensor[driver_type_to_index(signal)].filp = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(olm_info_p->sensor); i++) {
		if (olm_info_p->sensor[i].filp != NULL) {
			free_olm_info_p = false;
			break;
		}
	}

	if (free_olm_info_p == true) {
		olm_info_p->en_time = -1LL;
		kfree(olm_info_p);
		olm_info_p = NULL;
		offline_mode_en = false;
	}

	return VSM_STATUS_OK;
}

enum vsm_signal_t vsm_driver_reset_PPG_counter(void)
{
	enum vsm_status_t err = VSM_STATUS_OK;
	struct bus_data_t data;
	uint32_t /*enable_data, */ reg_data = 0;
	/* step 1: (disable PPG function and reset
	 * PPG1 and PPG2 write counters to 0)
	 */
	data.addr = (DIGITAL_START_ADDR & 0xFF00) >> 8;
	data.reg = (DIGITAL_START_ADDR & 0xFF);
	data.data_buf = (uint8_t *) &reg_data;
	data.length = sizeof(reg_data);
	err = vsm_driver_write_register(&data);
	if (err == VSM_STATUS_OK) {
		/* step 2: 0x33D0 = 0x2000_0000
		 * (reset PPG1 read counter to 0)
		 */
		data.reg = SRAM_PPG1_READ_COUNT_ADDR;
		reg_data = 0x60000000;
		err = vsm_driver_write_register(&data);
		if (err == VSM_STATUS_OK) {
			/* step 3: 0x33E0 = 0x2000_0000
			 * (reset PPG2 read counter to 0)
			 */
			data.reg = SRAM_PPG2_READ_COUNT_ADDR;
			reg_data = 0x60000000;
			err = vsm_driver_write_register(&data);
		}
	}
	return err;
}

static int MT6381_CaliReadSram(enum vsm_sram_type_t sram_type,
	int32_t *data_buf, uint32_t len)
{
	uint32_t temp;
	uint32_t read_counter = 0;
	uint32_t write_counter = 0;
	uint32_t sram_len;
	int err = VSM_STATUS_OK;

	/* read counter */
	vsm_driver_get_read_counter(sram_type, &read_counter);
	do {
		temp = read_counter;
		vsm_driver_get_read_counter(sram_type, &read_counter);
	} while (temp != read_counter);

	/* write counter */
	vsm_driver_write_counter(sram_type, &write_counter);
	do {
		temp = write_counter;
		vsm_driver_write_counter(sram_type, &write_counter);
	} while (temp != write_counter);

	sram_len =
	    (write_counter >=
	     read_counter) ? (write_counter - read_counter) :
	     (VSM_SRAM_LEN - read_counter + write_counter);
	sram_len = ((sram_len % 2) == 0) ? sram_len : sram_len - 1;
	sram_len = (sram_len > len) ? len : sram_len;

	if (sram_len > 0) {
		/* get sram data */
		err = vsm_driver_read_register_batch(
			sram_type, (u8 *) data_buf, sram_len);
		if (err < 0)
			pr_err("vsm_driver_read_register_batch fail, %d\n",
				err);
	}

	return sram_len;
}

static int MT6381_GetCalibrationData(int32_t *cali_sram1_buf,
	int32_t *cali_sram2_buf, uint16_t pga,
	uint16_t ambdac_amb, uint16_t ambdac_led, uint32_t len)
{
	uint32_t sram1_len = len;
	uint32_t sram2_len = len;
	uint32_t read_len = 0;

	inCali = true;

	cali_pga = pga;
	cali_ambdac_amb = ambdac_amb;
	cali_ambdac_led = ambdac_led;

	vsm_driver_set_signal(VSM_SIGNAL_PPG1);
	vsm_driver_set_signal(VSM_SIGNAL_PPG2);

	while ((cali_sram1_buf != NULL && sram1_len != 0) ||
		(cali_sram2_buf != NULL && sram2_len != 0)) {
		if (cali_sram1_buf != NULL && sram1_len != 0) {
			read_len = MT6381_CaliReadSram(VSM_SRAM_PPG1,
				&cali_sram1_buf[len - sram1_len], sram1_len);
			if (read_len > sram1_len) {
				pr_err("Wrong len, sram1_len = %d, read_len = %d\n",
					sram1_len, read_len);
				return -1;
			}
			sram1_len -= read_len;
		}
		if (cali_sram2_buf != NULL && sram2_len != 0) {
			read_len = MT6381_CaliReadSram(VSM_SRAM_PPG2,
				&cali_sram2_buf[len - sram2_len], sram2_len);
			if (read_len > sram2_len) {
				pr_err("Wrong len, sram2_len = %d, read_len = %d\n",
					sram2_len, read_len);
				return -1;
			}
			sram2_len -= read_len;
		}
	}

	inCali = false;
	vsm_driver_disable_signal(VSM_SIGNAL_PPG1);
	vsm_driver_disable_signal(VSM_SIGNAL_PPG2);

	return 0;
}

static int MT6381_DoCalibration(struct biometric_cali *cali)
{
	int i;
	int32_t *cali_sram_buf;
	int a = 0;
	int b = 0;
	int c = 0;
	int count = 0;
	int64_t sumOfambdac = 0;

	cali_sram_buf = kzalloc(sizeof(*cali_sram_buf) *
		CALI_DATA_LEN, GFP_KERNEL);
	if (cali_sram_buf == NULL)
		return -1;

	/* 1. set PGA=1, AMBDAC(AMB phase) = 0 to get AMB data A */
	MT6381_GetCalibrationData(cali_sram_buf, NULL, 0, 0, 7, CALI_DATA_LEN);
	for (i = CALI_DATA_STABLE_LEN; i < CALI_DATA_LEN; i += 2) {
		cali_sram_buf[i] = cali_sram_buf[i] >= 0x400000 ?
			cali_sram_buf[i] - 0x800000 : cali_sram_buf[i];
		a += cali_sram_buf[i];
		if (atomic_read(&bio_trace) != 0)
			pr_debug("[%d] = %d\n", i, cali_sram_buf[i]);
	}
	/* 2. set PGA=1, AMBDAC(AMB phase) = 1 to get AMB data B */
	MT6381_GetCalibrationData(cali_sram_buf, NULL, 0, 1, 7, CALI_DATA_LEN);
	for (i = CALI_DATA_STABLE_LEN; i < CALI_DATA_LEN; i += 2) {
		cali_sram_buf[i] = cali_sram_buf[i] >= 0x400000 ?
			cali_sram_buf[i] - 0x800000 : cali_sram_buf[i];
		b += cali_sram_buf[i];
		if (atomic_read(&bio_trace) != 0)
			pr_debug("[%d] = %d\n", i, cali_sram_buf[i]);
	}
	/* 3. set PGA=6, AMBDAC(AMB phase) = 1 to get AMB data C */
	MT6381_GetCalibrationData(cali_sram_buf, NULL, 6, 1, 7, CALI_DATA_LEN);
	for (i = CALI_DATA_STABLE_LEN; i < CALI_DATA_LEN; i += 2) {
		cali_sram_buf[i] = cali_sram_buf[i] >= 0x400000 ?
			cali_sram_buf[i] - 0x800000 : cali_sram_buf[i];
		c += cali_sram_buf[i];
		if (atomic_read(&bio_trace) != 0)
			pr_debug("[%d] = %d\n", i, cali_sram_buf[i]);
	}
	/* 4. PGA6 = (C - A) / ( B - A) */
	/* 5. set PGA=1, AMBDAC(AMB phase) = 1, AMBDAC(LED phase) = 7 */
	MT6381_GetCalibrationData(NULL, cali_sram_buf, 0, 1, 7, CALI_DATA_LEN);
	for (i = CALI_DATA_STABLE_LEN; i < CALI_DATA_LEN; i += 2) {
		cali_sram_buf[i] = cali_sram_buf[i] >= 0x400000 ?
			cali_sram_buf[i] - 0x800000 : cali_sram_buf[i];
		sumOfambdac += cali_sram_buf[i];
		count++;
		if (atomic_read(&bio_trace) != 0)
			pr_debug("[%d] = %d\n", i, cali_sram_buf[i]);
	}

	kfree(cali_sram_buf);

	cali->pga6 = (c - a) * 1000 / (b - a);
	cali->ambdac5_5 = sumOfambdac * -1000 / count;
	if (atomic_read(&bio_trace) != 0) {
		pr_debug("sumOfambdac = %lld, count = %d\n",
			sumOfambdac, count);
		pr_debug("pga6 = %d, ambdac5_5 = %d\n",
			cali->pga6, cali->ambdac5_5);
	}
	if (cali->pga6 < 6000 || cali->pga6 > 6060) {
		pr_err("pga fail, %d is not between 6000~6060.\n", cali->pga6);
		return -1;
	}
	if (cali->ambdac5_5 < 20957234 || cali->ambdac5_5 > 22296629) {
		pr_err("ambdac fail, %d is not between 20957234~22296629.\n",
			cali->ambdac5_5);
		return -1;
	}
	return 0;
}

static int MT6381_WriteCalibration(struct biometric_cali *cali)
{
	pga6 = cali->pga6;
	ambdac5_5 = cali->ambdac5_5;
	dc_offset = (long long)pga6 * (long long)ambdac5_5;
	dc_offset += 500000;
	do_div(dc_offset, 1000000);
	pr_notice("pga6 = %d, ambdac5_5 = %d, dc_offset = %lld\n",
		pga6, ambdac5_5, dc_offset);
	return 0;
}

static int MT6381_ResetCalibration(void)
{
	struct biometric_cali data;

	data.pga6 = DEFAULT_PGA6;
	data.ambdac5_5 = DEFAULT_AMBDAC5_5;
	MT6381_WriteCalibration(&data);

	return 0;
}

static int MT6381_ReadCalibration(struct biometric_cali *cali)
{
	cali->pga6 = pga6;
	cali->ambdac5_5 = ambdac5_5;
	pr_notice("pga6 = %d, ambdac5_5 = %d\n", pga6, ambdac5_5);
	return 0;
}

static int MT6381_FtmReadSram(
	enum vsm_sram_type_t sram_type, int32_t *data_buf, int len)
{
	uint32_t temp;
	uint32_t read_counter = 0;
	uint32_t write_counter = 0;
	uint32_t sram_len;
	int err = VSM_STATUS_OK;

	if (data_buf == NULL || len <= 0 || len >= VSM_SRAM_LEN) {
		err = VSM_STATUS_INVALID_PARAMETER;
		return err;
	}

	/* read counter */
	vsm_driver_get_read_counter(sram_type, &read_counter);
	do {
		temp = read_counter;
		vsm_driver_get_read_counter(sram_type, &read_counter);
	} while (temp != read_counter);

	/* write counter */
	vsm_driver_write_counter(sram_type, &write_counter);
	do {
		temp = write_counter;
		vsm_driver_write_counter(sram_type, &write_counter);
	} while (temp != write_counter);

	sram_len =
	    (write_counter >=
	     read_counter) ? (write_counter - read_counter) :
	     (VSM_SRAM_LEN - read_counter + write_counter);
	sram_len = ((sram_len % 2) == 0) ? sram_len : sram_len - 1;

	if (sram_len < len) {
		pr_err("data not ready\n");
		return -1;
	}

	read_counter = (write_counter + VSM_SRAM_LEN - len) % VSM_SRAM_LEN;

	/* get sram data */
	vsm_driver_set_read_counter(sram_type, &read_counter);
	err = vsm_driver_read_register_batch(sram_type, (u8 *) data_buf, len);
	if (err < 0) {
		pr_err("vsm_driver_read_register_batch fail, %d\n", err);
		return -1;
	}

	return 0;
}

static int MT6381_FtmStart(void)
{
	vsm_driver_set_signal(VSM_SIGNAL_PPG1);
	vsm_driver_set_signal(VSM_SIGNAL_PPG2);
	vsm_driver_set_signal(VSM_SIGNAL_EKG);
	vsm_driver_set_led(VSM_SIGNAL_PPG2, true);

	return 0;
}

static int MT6381_FtmEnd(void)
{
	vsm_driver_disable_signal(VSM_SIGNAL_PPG1);
	vsm_driver_disable_signal(VSM_SIGNAL_PPG2);
	vsm_driver_disable_signal(VSM_SIGNAL_EKG);

	return 0;
}

static int MT6381_GetTestData(struct biometric_test_data *data)
{
	int32_t ppg[2];

	msleep(20); /* wait for data ready */
	MT6381_FtmReadSram(VSM_SRAM_PPG2, ppg, 2);

	data->ppg_ir = ppg[0];
	data->ppg_r = ppg[1];

	MT6381_FtmReadSram(VSM_SRAM_EKG, &(data->ekg), 1);

	data->ppg_ir = data->ppg_ir >= 0x400000 ?
		data->ppg_ir - 0x800000 : data->ppg_ir;
	data->ppg_r  = data->ppg_r >= 0x400000 ?
		data->ppg_r - 0x800000 : data->ppg_r;
	data->ekg = data->ekg >= 0x400000 ? data->ekg - 0x800000 : data->ekg;
	if (atomic_read(&bio_trace) != 0)
		pr_debug("%d, %d, %d\n",
			data->ppg_ir, data->ppg_r, data->ekg);

	return 0;
}

static int MT6381_GetThreshold(struct biometric_threshold *threshold)
{
	threshold->ppg_ir_threshold = FTM_PPG_IR_THRESHOLD;
	threshold->ppg_r_threshold = FTM_PPG_R_THRESHOLD;
	threshold->ekg_threshold = FTM_PPG_EKG_THRESHOLD;
	return 0;
}

int mt6381_enable_ekg(int en)
{
	enum vsm_status_t err = VSM_STATUS_OK;

	pr_debug("%s en:%d\n", __func__, en);

	mutex_lock(&op_lock);
	if (en) {
		if (offline_mode_en == false)
			err = vsm_driver_set_signal(VSM_SIGNAL_EKG);
		else
			err = vsm_driver_set_signal_offline_mode(
				VSM_SIGNAL_EKG);
	} else {
		if (offline_mode_en == false)
			err = vsm_driver_disable_signal(VSM_SIGNAL_EKG);
		else
			err = vsm_driver_disable_signal_offline_mode(
				VSM_SIGNAL_EKG);
	}
	mutex_unlock(&op_lock);

	return err;
}

int mt6381_enable_ppg1(int en)
{
	enum vsm_status_t err = VSM_STATUS_OK;

	pr_debug("%s en:%d\n", __func__, en);

	mutex_lock(&op_lock);
	if (en) {
		if (offline_mode_en == false)
			err = vsm_driver_set_signal(VSM_SIGNAL_PPG1);
		else
			err = vsm_driver_set_signal_offline_mode(
				VSM_SIGNAL_PPG1);
	} else {
		if (offline_mode_en == false)
			err = vsm_driver_disable_signal(VSM_SIGNAL_PPG1);
		else
			err = vsm_driver_disable_signal_offline_mode(
				VSM_SIGNAL_PPG1);
	}
	mutex_unlock(&op_lock);

	return err;
}

int mt6381_enable_ppg2(int en)
{
	enum vsm_status_t err = VSM_STATUS_OK;

	pr_debug("%s en:%d\n", __func__, en);

	mutex_lock(&op_lock);
	if (en) {
		if (offline_mode_en == false)
			err = vsm_driver_set_signal(VSM_SIGNAL_PPG2);
		else
			err = vsm_driver_set_signal_offline_mode(
				VSM_SIGNAL_PPG2);
	} else {
		if (offline_mode_en == false)
			err = vsm_driver_disable_signal(VSM_SIGNAL_PPG2);
		else
			err = vsm_driver_disable_signal_offline_mode(
				VSM_SIGNAL_PPG2);
	}
	mutex_unlock(&op_lock);

	return err;
}

int mt6381_set_delay(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
	return 0;
}

int mt6381_get_data_ekg(int *raw_data, int *amb_data,
	int *agc_data, int8_t *status, u32 *length)
{
	int i;

	if (offline_mode_en == true) {
		mutex_lock(&op_lock);
		vsm_driver_read_offline_mode_data(
			VSM_SIGNAL_EKG, raw_data, length);
		mutex_unlock(&op_lock);

		for (i = 0; i < *length; i++) {
			/* When finger is not ready,
			 * ekg value should be saturated (0x3C0000).
			 */
			/* Set threshold to +/- 1000mV.
			 * 1000 * (2 ^ 23) / 4000 = 2097152
			 */
			if ((raw_data[i] > 2097152) || (raw_data[i] < -2097152))
				status[i] = SENSOR_STATUS_UNRELIABLE;
			else
				status[i] = SENSOR_STATUS_ACCURACY_HIGH;
		}

		return 0;
	}

	mutex_lock(&op_lock);
	if (current_signal & VSM_SIGNAL_EKG)
		vsm_driver_read_sram(VSM_SRAM_EKG, raw_data, NULL, length);
	else
		*length = 0;

	for (i = 0; i < *length; i++) {
		if (latency_test_data.in_latency_test) {
			/* verify only */
			if (latency_test_data.ekg_num > 0) {
				raw_data[i] = latency_test_data.first_data;
				latency_test_data.ekg_num--;
			} else
				raw_data[i] = latency_test_data.second_data;
		} else {
			if (raw_data[i] >= 0x400000)
				raw_data[i] = raw_data[i] - 0x800000;

			/* When finger is not ready,
			 * ekg value should be saturated (0x3C0000).
			 */
			/* Set threshold to +/- 1000mV.
			 * 1000 * (2 ^ 23) / 4000 = 2097152
			 */
			if ((raw_data[i] > 2097152) || (raw_data[i] < -2097152))
				status[i] = SENSOR_STATUS_UNRELIABLE;
			else
				status[i] = SENSOR_STATUS_ACCURACY_HIGH;
		}
	}

	mutex_unlock(&op_lock);
	return 0;
}

int mt6381_get_data_ppg1(int *raw_data, int *amb_data,
	int *agc_data, int8_t *status, u32 *length)
{
	int i;
	/* The sampling frequency of PPG input (Support 125Hz only). */
	int32_t ppg_control_fs = 16;
	/* The input configuration, default value is 1. */
	int32_t ppg_control_cfg = 1;
	/* The input source, default value is 1 (PPG channel 1). */
	int32_t ppg_control_src = 1;
	/* Input structure for the #ppg_control_process(). */
	struct ppg_control_t ppg1_control_input;
	int64_t numOfSRAMPPG2Data;
	u32 leddrv_con1;

	if (offline_mode_en == true) {
		mutex_lock(&op_lock);
		vsm_driver_read_offline_mode_data(
			VSM_SIGNAL_PPG1, raw_data, length);
		mutex_unlock(&op_lock);
		for (i = 0; i < *length; i++) {
			/* Only report finger on/off status by IR PPG */
			if ((raw_data[i] - dc_offset) < PPG_THRESHOLD)
				status[i] = SENSOR_STATUS_UNRELIABLE;
			else
				status[i] = SENSOR_STATUS_ACCURACY_HIGH;
		}
		return 0;
	}

	mutex_lock(&op_lock);
	numOfSRAMPPG2Data = numOfData[VSM_SRAM_PPG2];
	/* avoid to accumulate numOfData[] */
	if (current_signal & VSM_SIGNAL_PPG1) {
		mt6381_i2c_write_read(0x33, 0x2C, (u8 *)&leddrv_con1, 4);
		/* PPG1 data is stored in sram ppg2 as well */
		vsm_driver_read_sram(VSM_SRAM_PPG2,
			temp_buf, amb_temp_buf, length);
		for (i = 0; i < *length; i += 2) {
			temp_buf[i] = temp_buf[i] >= 0x400000 ?
				temp_buf[i] - 0x800000 : temp_buf[i];
			temp_buf[i + 1] = temp_buf[i + 1] >= 0x400000 ?
				temp_buf[i + 1] - 0x800000 : temp_buf[i + 1];
			amb_temp_buf[i] = amb_temp_buf[i] >= 0x400000 ?
				amb_temp_buf[i] - 0x800000 : amb_temp_buf[i];

			if (ppg1_buf2_len < VSM_SRAM_LEN) {
				ppg1_agc_buf[ppg1_buf2_len] = leddrv_con1;
				ppg1_amb_buf[ppg1_buf2_len] = amb_temp_buf[i];
				ppg1_buf2[ppg1_buf2_len++] = temp_buf[i];
			}
			if (ppg2_buf2_len < VSM_SRAM_LEN) {
				ppg2_agc_buf[ppg2_buf2_len] = leddrv_con1;
				ppg2_amb_buf[ppg2_buf2_len] = amb_temp_buf[i];
				ppg2_buf2[ppg2_buf2_len++] = temp_buf[i + 1];
			}

			/* downsample to 16hz for AGC */
			if ((numOfSRAMPPG2Data + i) % 64 == 0) {
				if (agc_ppg1_buf_len <
					ARRAY_SIZE(agc_ppg1_buf)) {
					agc_ppg1_buf[agc_ppg1_buf_len] =
						temp_buf[i];
					agc_ppg1_amb_buf[agc_ppg1_buf_len++] =
						amb_temp_buf[i];
				}
				if (agc_ppg2_buf_len <
					ARRAY_SIZE(agc_ppg2_buf)) {
					agc_ppg2_buf[agc_ppg2_buf_len] =
						temp_buf[i + 1];
					/* use ppg1 amb data */
					agc_ppg2_amb_buf[agc_ppg2_buf_len++] =
						amb_temp_buf[i];
				}
			}
		}

		if ((current_signal & VSM_SIGNAL_PPG2) && *length > 0) {
			if ((int)temp_buf[i - 2] < PPG_THRESHOLD)
				vsm_driver_set_led(VSM_SIGNAL_PPG2, false);
			else
				vsm_driver_set_led(VSM_SIGNAL_PPG2, true);
		}
	} else
		ppg1_buf2_len = 0;

	if (ppg1_buf2_len == VSM_SRAM_LEN)
		aee_kernel_warning("MT6381", "PPG1 data dropped\n");
	else if (ppg1_buf2_len > 0) {
		memcpy(raw_data, ppg1_buf2,
			ppg1_buf2_len * sizeof(ppg1_buf2[0]));
		memcpy(amb_data, ppg1_amb_buf,
			ppg1_buf2_len * sizeof(ppg1_buf2[0]));
		memcpy(agc_data, ppg1_agc_buf,
			ppg1_buf2_len * sizeof(ppg1_buf2[0]));

		for (i = 0; i < ppg1_buf2_len; i++) {
			/* Only report finger on/off status by IR PPG */
			if (raw_data[i] < PPG_THRESHOLD)
				status[i] = SENSOR_STATUS_UNRELIABLE;
			else
				status[i] = SENSOR_STATUS_ACCURACY_HIGH;

			raw_data[i] += dc_offset;
		}
	}

	*length = ppg1_buf2_len;
	ppg1_buf2_len = 0;

	/* used for latency verification */
	for (i = 0; i < *length; i++) {
		if (latency_test_data.in_latency_test) {
			/* verify only */
			if (latency_test_data.ppg1_num > 0) {
				raw_data[i] = latency_test_data.first_data;
				latency_test_data.ppg1_num--;
			} else
				raw_data[i] = latency_test_data.second_data;
		}
	}

	if (atomic_read(&bio_trace) != 0)
		for (i = 0; i < agc_ppg1_buf_len; i++)
			pr_debug("ppg1 = %d, amb = %d\n",
				agc_ppg1_buf[i], agc_ppg1_amb_buf[i]);
	ppg1_control_input.input = agc_ppg1_buf;
	ppg1_control_input.input_amb = agc_ppg1_amb_buf;
	ppg1_control_input.input_fs = ppg_control_fs;
	ppg1_control_input.input_length = agc_ppg1_buf_len;
	ppg1_control_input.input_config = ppg_control_cfg;
	ppg1_control_input.input_source = ppg_control_src;
	ppg_control_process(&ppg1_control_input);
	agc_ppg1_buf_len = 0;

	mutex_unlock(&op_lock);
	return 0;
}

int mt6381_get_data_ppg2(int *raw_data, int *amb_data,
	int *agc_data, int8_t *status, u32 *length)
{
	int i;
	/* The sampling frequency of PPG input (Support 125Hz only). */
	int32_t ppg_control_fs = 16;
	/* The input configuration, default value is 1. */
	int32_t ppg_control_cfg = 1;
	/* The input source, default value is 1 (PPG channel 1). */
	int32_t ppg_control_src = 2;
	/* Input structure for the #ppg_control_process(). */
	struct ppg_control_t ppg1_control_input;
	int64_t numOfSRAMPPG2Data;
	u32 leddrv_con1;

	if (offline_mode_en == true) {
		mutex_lock(&op_lock);
		vsm_driver_read_offline_mode_data(
			VSM_SIGNAL_PPG2, raw_data, length);
		mutex_unlock(&op_lock);
		return 0;
	}

	mutex_lock(&op_lock);
	numOfSRAMPPG2Data = numOfData[VSM_SRAM_PPG2];
	/* avoid to accumulate numOfData[] */
	if (current_signal & VSM_SIGNAL_PPG2) {
		mt6381_i2c_write_read(0x33, 0x2C, (u8 *)&leddrv_con1, 4);
		vsm_driver_read_sram(VSM_SRAM_PPG2,
			temp_buf, amb_temp_buf, length);
		for (i = 0; i < *length; i += 2) {
			temp_buf[i] = temp_buf[i] >= 0x400000 ?
				temp_buf[i] - 0x800000 : temp_buf[i];
			temp_buf[i + 1] = temp_buf[i + 1] >= 0x400000 ?
				temp_buf[i + 1] - 0x800000 : temp_buf[i + 1];
			amb_temp_buf[i] = amb_temp_buf[i] >= 0x400000 ?
				amb_temp_buf[i] - 0x800000 : amb_temp_buf[i];

			if (ppg1_buf2_len < VSM_SRAM_LEN) {
				ppg1_agc_buf[ppg1_buf2_len] = leddrv_con1;
				ppg1_amb_buf[ppg1_buf2_len] = amb_temp_buf[i];
				ppg1_buf2[ppg1_buf2_len++] = temp_buf[i];
			}
			if (ppg2_buf2_len < VSM_SRAM_LEN) {
				ppg2_agc_buf[ppg2_buf2_len] = leddrv_con1;
				ppg2_amb_buf[ppg2_buf2_len] = amb_temp_buf[i];
				ppg2_buf2[ppg2_buf2_len++] = temp_buf[i + 1];
			}

			/* downsample to 16hz for AGC */
			if ((numOfSRAMPPG2Data + i) % 64 == 0) {
				if (agc_ppg1_buf_len <
					ARRAY_SIZE(agc_ppg1_buf)) {
					agc_ppg1_buf[agc_ppg1_buf_len] =
						temp_buf[i];
					agc_ppg1_amb_buf[agc_ppg1_buf_len++] =
						amb_temp_buf[i];
				}
				if (agc_ppg2_buf_len <
					ARRAY_SIZE(agc_ppg2_buf)) {
					agc_ppg2_buf[agc_ppg2_buf_len] =
						temp_buf[i + 1];
					/* use ppg1 amb data */
					agc_ppg2_amb_buf[agc_ppg2_buf_len++] =
						amb_temp_buf[i];
				}
			}
		}

		if (*length > 0) {
			if ((int)temp_buf[i - 2] < PPG_THRESHOLD)
				vsm_driver_set_led(VSM_SIGNAL_PPG2, false);
			else
				vsm_driver_set_led(VSM_SIGNAL_PPG2, true);
		}
	} else
		ppg2_buf2_len = 0;

	if (ppg2_buf2_len == VSM_SRAM_LEN)
		aee_kernel_warning("MT6381", "PPG1 data dropped\n");
	else if (ppg2_buf2_len > 0) {
		memcpy(raw_data, ppg2_buf2,
			ppg2_buf2_len * sizeof(ppg2_buf2[0]));
		memcpy(amb_data, ppg2_amb_buf,
			ppg2_buf2_len * sizeof(ppg2_buf2[0]));
		memcpy(agc_data, ppg2_agc_buf,
			ppg2_buf2_len * sizeof(ppg2_buf2[0]));

		for (i = 0; i < ppg2_buf2_len; i++)
			raw_data[i] += dc_offset;
	}

	*length = ppg2_buf2_len;
	ppg2_buf2_len = 0;

	/* used for latency verification */
	for (i = 0; i < *length; i++) {
		if (latency_test_data.in_latency_test) {
			if (latency_test_data.ppg2_num > 0) {
				raw_data[i] = latency_test_data.first_data;
				latency_test_data.ppg2_num--;
			} else
				raw_data[i] = latency_test_data.second_data;
		}
	}

	if (atomic_read(&bio_trace) != 0)
		for (i = 0; i < agc_ppg2_buf_len; i++)
			pr_debug("ppg2 = %d, amb = %d\n",
				agc_ppg2_buf[i], agc_ppg2_amb_buf[i]);
	ppg1_control_input.input = agc_ppg2_buf;
	ppg1_control_input.input_amb = agc_ppg2_amb_buf;
	ppg1_control_input.input_fs = ppg_control_fs;
	ppg1_control_input.input_length = agc_ppg2_buf_len;
	ppg1_control_input.input_config = ppg_control_cfg;
	ppg1_control_input.input_source = ppg_control_src;
	ppg_control_process(&ppg1_control_input);
	agc_ppg2_buf_len = 0;

	mutex_unlock(&op_lock);

	return 0;
}

static int pin_init(void)
{
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
	int ret;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6381");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			pinctrl_gpios = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(pinctrl_gpios)) {
				ret = PTR_ERR(pinctrl_gpios);
				pr_err("%s can't find mt6381 pinctrl\n",
					__func__);
				return -1;
			}
		} else {
			pr_err("%s platform device is null\n", __func__);
		}
		/* it's normal that get "default" will failed */
		bio_pins_default = pinctrl_lookup_state(
			pinctrl_gpios, "default");
		if (IS_ERR(bio_pins_default)) {
			ret = PTR_ERR(bio_pins_default);
			pr_err("%s can't find mt6381 pinctrl default\n",
				__func__);
			/* return ret; */
		}
		bio_pins_reset_high = pinctrl_lookup_state(
			pinctrl_gpios, "reset_high");
		if (IS_ERR(bio_pins_reset_high)) {
			ret = PTR_ERR(bio_pins_reset_high);
			pr_err("%s can't find mt6381 pinctrl reset_high\n",
				__func__);
			return -1;
		}
		bio_pins_reset_low = pinctrl_lookup_state(
			pinctrl_gpios, "reset_low");
		if (IS_ERR(bio_pins_reset_low)) {
			ret = PTR_ERR(bio_pins_reset_low);
			pr_err("%s can't find mt6381 pinctrl reset_low\n",
				__func__);
			return -1;
		}
		bio_pins_pwd_high = pinctrl_lookup_state(
			pinctrl_gpios, "pwd_high");
		if (IS_ERR(bio_pins_pwd_high)) {
			ret = PTR_ERR(bio_pins_pwd_high);
			pr_err("%s can't find mt6381 pinctrl pwd_high\n",
				__func__);
			return -1;
		}
		bio_pins_pwd_low = pinctrl_lookup_state(
			pinctrl_gpios, "pwd_low");
		if (IS_ERR(bio_pins_pwd_low)) {
			ret = PTR_ERR(bio_pins_pwd_low);
			pr_err("%s can't find mt6381 pinctrl pwd_low\n",
				__func__);
			return -1;
		}
	} else {
		pr_err("Device Tree: can not find bio node!. %s\n",
			"Go to use old cust info");
		return -1;
	}

	return 0;
}


/**********************************************************************
 * Function Configuration
 **********************************************************************/
static int mt6381_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}
/*-------------------------------------------------------------------*/
static int mt6381_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*-------------------------------------------------------------------*/
static long mt6381_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	void __user *data;
	struct biometric_cali sensor_data;
	struct biometric_test_data test_data;
	struct biometric_threshold threshold;
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
			(void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		pr_err("access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case BIOMETRIC_IOCTL_INIT:
		break;
	case BIOMETRIC_IOCTL_DO_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MT6381_DoCalibration(&sensor_data);
		if (err < 0) {
			err = -EFAULT;
			break;
		}
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;
	case BIOMETRIC_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		err = MT6381_WriteCalibration(&sensor_data);
		break;
	case BIOMETRIC_IOCTL_CLR_CALI:
		err = MT6381_ResetCalibration();
		break;
	case BIOMETRIC_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MT6381_ReadCalibration(&sensor_data);
		if (err < 0)
			break;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;
	case BIOMETRIC_IOCTL_FTM_START:
		MT6381_FtmStart();
		break;
	case BIOMETRIC_IOCTL_FTM_END:
		MT6381_FtmEnd();
		break;
	case BIOMETRIC_IOCTL_FTM_GET_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MT6381_GetTestData(&test_data);
		if (err < 0)
			break;
		if (copy_to_user(data, &test_data, sizeof(test_data))) {
			err = -EFAULT;
			break;
		}
		break;
	case BIOMETRIC_IOCTL_FTM_GET_THRESHOLD:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MT6381_GetThreshold(&threshold);
		if (err < 0)
			break;
		if (copy_to_user(data, &threshold, sizeof(threshold))) {
			err = -EFAULT;
			break;
		}
		break;
	default:
		pr_err("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#ifdef CONFIG_COMPAT
static long mt6381_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_BIOMETRIC_IOCTL_INIT:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_INIT, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_DO_CALI:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_DO_CALI, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_SET_CALI:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_SET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_GET_CALI:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_GET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_CLR_CALI:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_CLR_CALI, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_FTM_START:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_FTM_START, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_FTM_END:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_FTM_END, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_FTM_GET_DATA:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_FTM_GET_DATA, (unsigned long)arg32);
		break;
	case COMPAT_BIOMETRIC_IOCTL_FTM_GET_THRESHOLD:
		err = file->f_op->unlocked_ioctl(file,
			BIOMETRIC_IOCTL_FTM_GET_THRESHOLD,
			(unsigned long)arg32);
		break;
	default:
		pr_err("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif

/*----------------------------------------------------------------------------*/
static const struct file_operations mt6381_fops = {
	.owner = THIS_MODULE,
	.open = mt6381_open,
	.release = mt6381_release,
	.unlocked_ioctl = mt6381_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mt6381_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mt6381_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "biometric",
	.fops = &mt6381_fops,
};

static int mt6381_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct biometric_control_path ctl = { 0 };
	struct biometric_data_path data = { 0 };
	u32 chip_id;

	mt6381_i2c_client = client;
	mt6381_i2c_write_read(mt6381_i2c_client->addr,
		0xac, (u8 *) &chip_id, 4);
	pr_debug("bio sensor inited. chip_id:%x\n", chip_id);
	if (chip_id != 0x25110000)
		goto exit;

	mt6381_init_i2c_msg(MT2511_SLAVE_II);

	err = misc_register(&mt6381_device);
	if (err) {
		pr_err("mt6381_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = mt6381_create_attr(
		&(mt6381_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	err = pin_init();
	if (err) {
		pr_err("pin_init fail\n");
		goto exit_create_attr_failed;
	}

	current_signal = 0;
	atomic_set(&bio_trace, 0);
	mutex_init(&op_lock);
	ppg1_buf2_len = 0;
	ppg2_buf2_len = 0;
	mod_ary_len = 0;
	new_init_array_len = 0;
	set_AFE_TCTRL_CON2 = 0;
	set_AFE_TCTRL_CON3 = 0;
	AFE_TCTRL_CON2 = 0;
	AFE_TCTRL_CON3 = 0;
	polling_delay = 50;
	MT6381_ResetCalibration();
	latency_test_data.in_latency_test = false;
	ppg1_led_status = false;
	ppg2_led_status = false;
	inCali = false;
	pr_debug("AGC version: %x\n", ppg_control_get_version());

	ctl.open_report_data = mt6381_enable_ekg;
	ctl.batch = mt6381_set_delay;
	err = biometric_register_control_path(&ctl, ID_EKG);
	if (err) {
		pr_err("register bio control path err\n");
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = mt6381_enable_ppg1;
	ctl.batch = mt6381_set_delay;
	err = biometric_register_control_path(&ctl, ID_PPG1);
	if (err) {
		pr_err("register bio control path err\n");
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = mt6381_enable_ppg2;
	ctl.batch = mt6381_set_delay;
	err = biometric_register_control_path(&ctl, ID_PPG2);
	if (err) {
		pr_err("register bio control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = mt6381_get_data_ekg;
	err = biometric_register_data_path(&data, ID_EKG);
	if (err) {
		pr_err("register bio data path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = mt6381_get_data_ppg1;
	err = biometric_register_data_path(&data, ID_PPG1);
	if (err) {
		pr_err("register bio data path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = mt6381_get_data_ppg2;
	err = biometric_register_data_path(&data, ID_PPG2);
	if (err) {
		pr_err("register bio data path err\n");
		goto exit_create_attr_failed;
	}

	biosensor_init_flag = 0;
	return 0;
exit_create_attr_failed:
	mt6381_delete_attr(client->dev.driver);
	misc_deregister(&mt6381_device);
exit_misc_device_register_failed:
exit:
	biosensor_init_flag = -1;
	return err;
}

static int mt6381_i2c_remove(struct i2c_client *client)
{
	misc_deregister(&mt6381_device);
	return 0;
}

static struct i2c_driver mt6381_i2c_driver = {
	.probe = mt6381_i2c_probe,
	.remove = mt6381_i2c_remove,

	.id_table = mt6381_i2c_id,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MT6381_DEV_NAME,
/* #ifdef CONFIG_PM_SLEEP */
#if 0
		   .pm = &lsm6ds3h_pm_ops,
#endif
#ifdef CONFIG_OF
			/* need add in dtsi first */
			.of_match_table = biometric_of_match,
#endif
	},
};

static int mt6381_local_init(void)
{
	if (i2c_add_driver(&mt6381_i2c_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (biosensor_init_flag == -1) {
		pr_err("%s init failed!\n", __func__);
		return -1;
	}
	return 0;
}

static int mt6381_local_remove(void)
{
	return 0;
}

static int __init MT6381_init(void)
{
	biometric_driver_add(&mt6381_init_info, ID_EKG);

	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit MT6381_exit(void)
{
}

/*----------------------------------------------------------------------------*/
module_init(MT6381_init);
module_exit(MT6381_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Liujiang Chen");
MODULE_DESCRIPTION("MT6381 driver");
MODULE_LICENSE("GPL");
