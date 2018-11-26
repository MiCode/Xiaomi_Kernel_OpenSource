/*
 * Reference Driver for NextInput Sensor
 *
 * The GPL Deliverables are provided to Licensee under the terms
 * of the GNU General Public License version 2 (the "GPL") and
 * any use of such GPL Deliverables shall comply with the terms
 * and conditions of the GPL. A copy of the GPL is available
 * in the license txt file accompanying the Deliverables and
 * at http://www.gnu.org/licenses/gpl.txt
 *
 * Copyright (C) NextInput, Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 * All rights reserved
 *
 * 1. Redistribution in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 2. Neither the name of NextInput nor the names of the contributors
 *    may be used to endorse or promote products derived from
 *    the software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES INCLUDING BUT
 * NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef NI_AF3018_H
#define NI_AF3018_H

#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#ifndef DIC_VARIANT
#include "ni_af3018_command_set.h"
#endif
#include <linux/hrtimer.h>

#define OEM_PIXEL
#define DRIVER_VERSION                          4
#define DRIVER_REVISION                         2
#define DRIVER_BUILD                            2
#define NI_STATUS_OK                            0b01000000
#define MAX_RETRY_COUNT                         3
#define BOOTING_DELAY                           400
#define RESET_DELAY                             20
#define SLEEP_DELAY                             20

#define NUM_SENSORS					1
#define DEVICE_NAME			 		"ni_af3018"
#define DEVICE_TREE_NAME			"nif,ni_af3018"
#define NI_WRITE_PERMISSIONS		(S_IWUSR | S_IWGRP)

#define OEM_ID_VENDOR				0x0101
#define OEM_ID_PRODUCT				0x0101
#define OEM_ID_VERSION				0x0101
#define OEM_DATE					"MM DD YYYY"
#define OEM_TIME					 "HH:MM:SS"
#define OEM_MAX_AUTOCAL				 0xfff
#define OEM_MAX_INTERRUPT			 0xfff
#define OEM_ADCOUT_LEN 2
#define DEFAULT_INT_THRE 15

#define OEM_RELEASE_THRESHOLD		10
#define INPUT_DEVICE
#define DEVICE_INTERRUPT

struct ni_af3018_platform_data {
#ifdef DEVICE_INTERRUPT
	int irq_gpio;
	u32 irq_gpio_flags;
#endif
	int dummy;
};

#ifdef NI_MCU
struct ni_af3018_ts_fw_upgrade_info {
	char fw_path[256];
	u8 fw_force_upgrade;
	volatile u8 is_downloading;
	bool bootloadermode;
	u8 *fwdata_ptr;
	size_t fw_size;
};

struct ni_af3018_ts_fw_info {
	u8 fw_ver;
	u8 fw_rev;
	u8 fw_build;
	u8 ni_core_ver;
	u8 ni_core_rev;
	u8 ni_core_build;
	char buildDate[11];
	char buildTime[8];
	struct ni_af3018_ts_fw_upgrade_info fw_upgrade;
	u8 ic_fw_identifier[31];
};

enum {
	DOWNLOAD_COMPLETE = 0,
	UNDER_DOWNLOADING,
};
#endif

struct ni_af3018_ts_data {
	struct i2c_client *client;
#ifdef INPUT_DEVICE
	struct input_dev *input_dev;
#endif
#ifdef DEVICE_INTERRUPT
	u8 enableInterrupt;
	struct work_struct work_release_interrupt;
	int release_threshold[NUM_SENSORS];
#endif
	struct ni_af3018_platform_data *pdata;
#ifdef NI_MCU
	struct ni_af3018_ts_fw_info fw_info;
	struct work_struct work_fw_upgrade;
#endif
	u16 force[NUM_SENSORS];
	atomic_t device_init;
	volatile int curr_pwr_state;
	int curr_resume_state;
	u8 ic_init_err_cnt;
	u8 is_probed;
	struct delayed_work work_init;
	struct work_struct work_recover;
	struct delayed_work work_queue;
	struct notifier_block notif;
	int suspend_flag;
	struct hrtimer release_timer;
	int released;
};

enum {
	POWER_OFF = 0,
	POWER_ON,
	POWER_SLEEP,
	POWER_IDLE,
	POWER_RESUME,
	POWER_WAKE
};

enum {
	DEBUG_NONE          = 0,
	DEBUG_BASE_INFO     = (1U << 0),
	DEBUG_COMMAND       = (1U << 1),
	DEBUG_VERBOSE       = (1U << 2),
	DEBUG_DATA          = (1U << 3),
	DEBUG_HARDWARE      = (1U << 4),
};

/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/ni_af3018_ts/parameters/debug_mask
 */
static u32 ni_debug_mask = DEBUG_BASE_INFO;
module_param_named(debug_mask, ni_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int  ni_af3018_ic_init(struct ni_af3018_ts_data *ts);
static void ni_af3018_init_func(struct work_struct *work_init);
static int  ni_af3018_ts_get_data(struct i2c_client *client);
static int  ni_af3018_init_panel(struct i2c_client *client);
static int  ni_af3018_get_ic_info(struct ni_af3018_ts_data *ts);
static void *get_touch_handle(struct i2c_client *client);
/*static int  ni_af3018_i2c_write_mode (struct i2c_client *client, u8 mode); */
static int  ni_af3018_i2c_read(struct i2c_client *client, u8 reg, int len,
			       u8 *buf);
static int ni_af3018_i2c_write(struct i2c_client *client, u8 reg, int len,
			       u8 *buf);
static int ni_af3018_i2c_write_byte(struct i2c_client *client, u8 reg,
				    u8 data);
static int ni_af3018_i2c_modify_byte(struct i2c_client *client, u8 reg, u8 data,
				     u8 mask);
static int ni_af3018_i2c_modify_array(struct i2c_client *client, u8 reg, u8 data,
				      u8 mask, u8 offset);
#define LOGI(fmt, args...) \
	do {\
		if (unlikely(ni_debug_mask & DEBUG_BASE_INFO)) \
			printk(KERN_INFO "[NextInput] " fmt, ##args);\
	} while (0)

#define LOGC(fmt, args...) \
	do {\
		if (unlikely(ni_debug_mask & DEBUG_COMMAND)) \
			printk(KERN_INFO "[NextInput C] " fmt, ##args);\
	} while (0)

#define LOGE(fmt, args...) \
	do {\
		printk(KERN_ERR "[NextInput E] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);\
	} while (0)

#define LOGV(fmt, args...) \
	do {\
		if (unlikely(ni_debug_mask & DEBUG_VERBOSE)) \
			printk(KERN_ERR "[NextInput V] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);\
	} while (0)

#define LOGD(fmt, args...) \
	do {\
		if (unlikely(ni_debug_mask & DEBUG_DATA)) \
			printk(KERN_ERR "[NextInput D] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);\
	} while (0)

#define LOGH(fmt, args...) \
	do {\
		if (unlikely(ni_debug_mask & DEBUG_HARDWARE)) \
			printk(KERN_ERR "[NextInput H] [%s %d] " fmt, __FUNCTION__, __LINE__, ##args);\
	} while (0)

#endif
