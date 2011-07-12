/* Copyright (c) 2009, 2011 Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include <mach/pmic.h>

#include "smd_rpcrouter.h"

#define TRACE_PMIC 0

#if TRACE_PMIC
#define PMIC(x...) printk(KERN_INFO "[PMIC] " x)
#else
#define PMIC(x...) do {} while (0)
#endif


#define LIB_NULL_PROC 0
#define LIB_RPC_GLUE_CODE_INFO_REMOTE_PROC 1
#define LP_MODE_CONTROL_PROC 2
#define VREG_SET_LEVEL_PROC 3
#define VREG_PULL_DOWN_SWITCH_PROC 4
#define SECURE_MPP_CONFIG_DIGITAL_OUTPUT_PROC 5
#define SECURE_MPP_CONFIG_I_SINK_PROC 6
#define RTC_START_PROC 7
#define RTC_STOP_PROC 8
#define RTC_GET_TIME_PROC 9
#define RTC_ENABLE_ALARM_PROC 10
#define RTC_DISABLE_ALARM_PROC 11
#define RTC_GET_ALARM_TIME_PROC 12
#define RTC_GET_ALARM_STATUS_PROC 13
#define RTC_SET_TIME_ADJUST_PROC 14
#define RTC_GET_TIME_ADJUST_PROC 15
#define SET_LED_INTENSITY_PROC 16
#define FLASH_LED_SET_CURRENT_PROC 17
#define FLASH_LED_SET_MODE_PROC 18
#define FLASH_LED_SET_POLARITY_PROC 19
#define SPEAKER_CMD_PROC 20
#define SET_SPEAKER_GAIN_PROC 21
#define VIB_MOT_SET_VOLT_PROC 22
#define VIB_MOT_SET_MODE_PROC 23
#define VIB_MOT_SET_POLARITY_PROC 24
#define VID_EN_PROC 25
#define VID_IS_EN_PROC 26
#define VID_LOAD_DETECT_EN_PROC 27
#define MIC_EN_PROC 28
#define MIC_IS_EN_PROC 29
#define MIC_SET_VOLT_PROC 30
#define MIC_GET_VOLT_PROC 31
#define SPKR_EN_RIGHT_CHAN_PROC 32
#define SPKR_IS_RIGHT_CHAN_EN_PROC 33
#define SPKR_EN_LEFT_CHAN_PROC 34
#define SPKR_IS_LEFT_CHAN_EN_PROC 35
#define SET_SPKR_CONFIGURATION_PROC 36
#define GET_SPKR_CONFIGURATION_PROC 37
#define SPKR_GET_GAIN_PROC 38
#define SPKR_IS_EN_PROC 39
#define SPKR_EN_MUTE_PROC 40
#define SPKR_IS_MUTE_EN_PROC 41
#define SPKR_SET_DELAY_PROC 42
#define SPKR_GET_DELAY_PROC 43
#define SECURE_MPP_CONFIG_DIGITAL_INPUT_PROC 44
#define SET_SPEAKER_DELAY_PROC 45
#define SPEAKER_1K6_ZIN_ENABLE_PROC 46
#define SPKR_SET_MUX_HPF_CORNER_FREQ_PROC 47
#define SPKR_GET_MUX_HPF_CORNER_FREQ_PROC 48
#define SPKR_IS_RIGHT_LEFT_CHAN_ADDED_PROC 49
#define SPKR_EN_STEREO_PROC 50
#define SPKR_IS_STEREO_EN_PROC 51
#define SPKR_SELECT_USB_WITH_HPF_20HZ_PROC 52
#define SPKR_IS_USB_WITH_HPF_20HZ_PROC 53
#define SPKR_BYPASS_MUX_PROC 54
#define SPKR_IS_MUX_BYPASSED_PROC 55
#define SPKR_EN_HPF_PROC 56
#define SPKR_IS_HPF_EN_PROC 57
#define SPKR_EN_SINK_CURR_FROM_REF_VOLT_CIR_PROC 58
#define SPKR_IS_SINK_CURR_FROM_REF_VOLT_CIR_EN_PROC 59
#define SPKR_ADD_RIGHT_LEFT_CHAN_PROC 60
#define SPKR_SET_GAIN_PROC 61
#define SPKR_EN_PROC 62
#define HSED_SET_PERIOD_PROC 63
#define HSED_SET_HYSTERESIS_PROC 64
#define HSED_SET_CURRENT_THRESHOLD_PROC 65
#define HSED_ENABLE_PROC 66
#define HIGH_CURRENT_LED_SET_CURRENT_PROC 67
#define HIGH_CURRENT_LED_SET_POLARITY_PROC 68
#define HIGH_CURRENT_LED_SET_MODE_PROC 69
#define LP_FORCE_LPM_CONTROL_PROC 70
#define LOW_CURRENT_LED_SET_EXT_SIGNAL_PROC 71
#define LOW_CURRENT_LED_SET_CURRENT_PROC 72
#define SPKR_SET_VSEL_LDO_PROC 86
#define HP_SPKR_CTRL_AUX_GAIN_INPUT_PROC 87
#define HP_SPKR_MSTR_EN_PROC 88
#define SPKR_SET_BOOST_PROC 89
#define HP_SPKR_PRM_IN_EN_PROC 90
#define HP_SPKR_CTRL_PRM_GAIN_INPUT_PROC 91
#define HP_SPKR_MUTE_EN_PROC 92
#define SPKR_BYPASS_EN_PROC 93
#define HP_SPKR_AUX_IN_EN_PROC 94
#define XO_CORE_FORCE_ENABLE 96
#define GPIO_SET_CURRENT_SOURCE_PULLS_PROC 97
#define GPIO_SET_GPIO_DIRECTION_INPUT_PROC 98
#define GPIO_SET_EXT_PIN_CONFIG_PROC 99
#define GPIO_SET_GPIO_CONFIG_PROC 100
#define GPIO_CONFIG_DIGITAL_OUTPUT_PROC 101
#define GPIO_GET_GPIO_DIRECTION_PROC 102
#define GPIO_SET_SLEEP_CLK_CONFIG_PROC 103
#define GPIO_CONFIG_DIGITAL_INPUT_PROC 104
#define GPIO_SET_OUTPUT_BUFFER_CONFIGURATION_PROC 105
#define GPIO_SET_PROC 106
#define GPIO_CONFIG_MODE_SELECTION_PROC 107
#define GPIO_SET_INVERSION_CONFIGURATION_PROC 108
#define GPIO_SET_GPIO_DIRECTION_OUTPUT_PROC 109
#define GPIO_SET_SOURCE_CONFIGURATION_PROC 110
#define GPIO_GET_PROC 111
#define GPIO_SET_VOLTAGE_SOURCE_PROC 112
#define GPIO_SET_OUTPUT_BUFFER_DRIVE_STRENGTH_PROC 113

/* rpc related */
#define PMIC_RPC_TIMEOUT (5*HZ)

#define PMIC_PDEV_NAME	"rs00010001:00000000"
#define PMIC_RPC_PROG	0x30000061
#define PMIC_RPC_VER_1_1	0x00010001
#define PMIC_RPC_VER_2_1	0x00020001
#define PMIC_RPC_VER_3_1	0x00030001
#define PMIC_RPC_VER_5_1	0x00050001
#define PMIC_RPC_VER_6_1	0x00060001

/* error bit flags defined by modem side */
#define PM_ERR_FLAG__PAR1_OUT_OF_RANGE		(0x0001)
#define PM_ERR_FLAG__PAR2_OUT_OF_RANGE		(0x0002)
#define PM_ERR_FLAG__PAR3_OUT_OF_RANGE		(0x0004)
#define PM_ERR_FLAG__PAR4_OUT_OF_RANGE		(0x0008)
#define PM_ERR_FLAG__PAR5_OUT_OF_RANGE		(0x0010)

#define PM_ERR_FLAG__ALL_PARMS_OUT_OF_RANGE   	(0x001F) /* all 5 previous */

#define PM_ERR_FLAG__SBI_OPT_ERR		(0x0080)
#define PM_ERR_FLAG__FEATURE_NOT_SUPPORTED	(0x0100)

#define	PMIC_BUFF_SIZE		256

struct pmic_buf {
	char *start;		/* buffer start addr */
	char *end;		/* buffer end addr */
	int size;		/* buffer size */
	char *data;		/* payload begin addr */
	int len;		/* payload len */
};

static DEFINE_MUTEX(pmic_mtx);

struct pmic_ctrl {
	int inited;
	struct pmic_buf	tbuf;
	struct pmic_buf	rbuf;
	struct msm_rpc_endpoint *endpoint;
};

static struct pmic_ctrl pmic_ctrl = {
	.inited = -1,
};

/* Add newer versions at the top of array */
static const unsigned int rpc_vers[] = {
	PMIC_RPC_VER_6_1,
	PMIC_RPC_VER_5_1,
	PMIC_RPC_VER_3_1,
	PMIC_RPC_VER_2_1,
	PMIC_RPC_VER_1_1,
};

static int pmic_rpc_req_reply(struct pmic_buf *tbuf,
				struct pmic_buf *rbuf, int proc);
static int pmic_rpc_set_only(uint data0, uint data1, uint data2,
				uint data3, int num, int proc);
static int pmic_rpc_set_struct(int, uint, uint *data, uint size, int proc);
static int pmic_rpc_set_get(uint setdata, uint *getdata, int size, int proc);
static int pmic_rpc_get_only(uint *getdata, int size, int proc);

static int pmic_buf_init(void)
{
	struct pmic_ctrl *pm = &pmic_ctrl;

	memset(&pmic_ctrl, 0, sizeof(pmic_ctrl));

	pm->tbuf.start = kmalloc(PMIC_BUFF_SIZE, GFP_KERNEL);
	if (pm->tbuf.start == NULL) {
		printk(KERN_ERR "%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}

	pm->tbuf.data = pm->tbuf.start;
	pm->tbuf.size = PMIC_BUFF_SIZE;
	pm->tbuf.end = pm->tbuf.start + PMIC_BUFF_SIZE;
	pm->tbuf.len = 0;

	pm->rbuf.start = kmalloc(PMIC_BUFF_SIZE, GFP_KERNEL);
	if (pm->rbuf.start == NULL) {
		kfree(pm->tbuf.start);
		printk(KERN_ERR "%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}
	pm->rbuf.data = pm->rbuf.start;
	pm->rbuf.size = PMIC_BUFF_SIZE;
	pm->rbuf.end = pm->rbuf.start + PMIC_BUFF_SIZE;
	pm->rbuf.len = 0;

	pm->inited = 1;

	return 0;
}

static inline void pmic_buf_reserve(struct pmic_buf *bp, int len)
{
	bp->data += len;
	bp->len  += len;
}

static inline void pmic_buf_reset(struct pmic_buf *bp)
{
	bp->data = bp->start;
	bp->len = 0;
}

static int modem_to_linux_err(uint err)
{
	if (err == 0)
		return 0;

	if (err & PM_ERR_FLAG__ALL_PARMS_OUT_OF_RANGE)
		return -EINVAL;	/* PM_ERR_FLAG__PAR[1..5]_OUT_OF_RANGE */

	if (err & PM_ERR_FLAG__SBI_OPT_ERR)
		return -EIO;

	if (err & PM_ERR_FLAG__FEATURE_NOT_SUPPORTED)
		return -ENOSYS;

	return -EPERM;
}

static int pmic_put_tx_data(struct pmic_buf *tp, uint datav)
{
	uint *lp;

	if ((tp->size - tp->len) < sizeof(datav)) {
		printk(KERN_ERR "%s: OVERFLOW size=%d len=%d\n",
					__func__, tp->size, tp->len);
		return -1;
	}

	lp = (uint *)tp->data;
	*lp = cpu_to_be32(datav);
	tp->data += sizeof(datav);
	tp->len += sizeof(datav);

	return sizeof(datav);
}

static int pmic_pull_rx_data(struct pmic_buf *rp, uint *datap)
{
	uint *lp;

	if (rp->len < sizeof(*datap)) {
		printk(KERN_ERR "%s: UNDERRUN len=%d\n", __func__, rp->len);
		return -1;
	}
	lp = (uint *)rp->data;
	*datap = be32_to_cpu(*lp);
	rp->data += sizeof(*datap);
	rp->len -= sizeof(*datap);

	return sizeof(*datap);
}


/*
 *
 *   +-------------------+
 *   |  PROC cmd layer   |
 *   +-------------------+
 *   |     RPC layer     |
 *   +-------------------+
 *
 * 1) network byte order
 * 2) RPC request header(40 bytes) and RPC reply header (24 bytes)
 * 3) each transaction consists of a request and reply
 * 3) PROC (comamnd) layer has its own sub-protocol defined
 * 4) sub-protocol can be grouped to follwoing 7 cases:
 *  	a) set one argument, no get
 * 	b) set two argument, no get
 * 	c) set three argument, no get
 * 	d) set a struct, no get
 * 	e) set a argument followed by a struct, no get
 * 	f) set a argument, get a argument
 * 	g) no set, get either a argument or a struct
 */

/**
 * pmic_rpc_req_reply() - send request and wait for reply
 * @tbuf:	buffer contains arguments
 * @rbuf:	buffer to be filled with arguments at reply
 * @proc:	command/request id
 *
 * This function send request to modem and wait until reply received
 */
static int pmic_rpc_req_reply(struct pmic_buf *tbuf, struct pmic_buf *rbuf,
	int	proc)
{
	struct pmic_ctrl *pm = &pmic_ctrl;
	int	ans, len, i;


	if ((pm->endpoint == NULL) || IS_ERR(pm->endpoint)) {
		for (i = 0; i < ARRAY_SIZE(rpc_vers); i++) {
			pm->endpoint = msm_rpc_connect_compatible(PMIC_RPC_PROG,
					rpc_vers[i], 0);

			if (IS_ERR(pm->endpoint)) {
				ans  = PTR_ERR(pm->endpoint);
				printk(KERN_ERR "%s: init rpc failed! ans = %d"
						" for 0x%x version, fallback\n",
						__func__, ans, rpc_vers[i]);
			} else {
				printk(KERN_DEBUG "%s: successfully connected"
					" to 0x%x rpc version\n",
					 __func__, rpc_vers[i]);
				break;
			}
		}
	}

	if (IS_ERR(pm->endpoint)) {
		ans  = PTR_ERR(pm->endpoint);
		return ans;
	}

	/*
	* data is point to next available space at this moment,
	* move it back to beginning of request header and increase
	* the length
	*/
	tbuf->data = tbuf->start;

	len = msm_rpc_call_reply(pm->endpoint, proc,
				tbuf->data, tbuf->len,
				rbuf->data, rbuf->size,
				PMIC_RPC_TIMEOUT);

	if (len <= 0) {
		printk(KERN_ERR "%s: rpc failed! len = %d\n", __func__, len);
		pm->endpoint = NULL;	/* re-connect later ? */
		return len;
	}

	rbuf->len = len;
	/* strip off rpc_reply_hdr */
	rbuf->data += sizeof(struct rpc_reply_hdr);
	rbuf->len -= sizeof(struct rpc_reply_hdr);

	return rbuf->len;
}

/**
 * pmic_rpc_set_only() - set arguments and no get
 * @data0:	first argumrnt
 * @data1:	second argument
 * @data2:	third argument
 * @data3:	fourth argument
 * @num:	number of argument
 * @proc:	command/request id
 *
 * This function covers case a, b, and c
 */
static int pmic_rpc_set_only(uint data0, uint data1, uint data2, uint data3,
		int num, int proc)
{
	struct pmic_ctrl *pm = &pmic_ctrl;
	struct pmic_buf	*tp;
	struct pmic_buf	*rp;
	int	stat;


	if (mutex_lock_interruptible(&pmic_mtx))
		return -ERESTARTSYS;

	if (pm->inited <= 0) {
		stat = pmic_buf_init();
		if (stat < 0) {
			mutex_unlock(&pmic_mtx);
			return stat;
		}
	}

	tp = &pm->tbuf;
	rp = &pm->rbuf;

	pmic_buf_reset(tp);
	pmic_buf_reserve(tp, sizeof(struct rpc_request_hdr));
	pmic_buf_reset(rp);

	if (num > 0)
		pmic_put_tx_data(tp, data0);

	if (num > 1)
		pmic_put_tx_data(tp, data1);

	if (num > 2)
		pmic_put_tx_data(tp, data2);

	if (num > 3)
		pmic_put_tx_data(tp, data3);

	stat = pmic_rpc_req_reply(tp, rp, proc);
	if (stat < 0) {
		mutex_unlock(&pmic_mtx);
		return stat;
	}

	pmic_pull_rx_data(rp, &stat);	/* result from server */

	mutex_unlock(&pmic_mtx);

	return modem_to_linux_err(stat);
}

/**
 * pmic_rpc_set_struct() - set the whole struct
 * @xflag:	indicates an extra argument
 * @xdata:	the extra argument
 * @*data:	starting address of struct
 * @size:	size of struct
 * @proc:	command/request id
 *
 * This fucntion covers case d and e
 */
static int pmic_rpc_set_struct(int xflag, uint xdata, uint *data, uint size,
	int proc)
{
	struct pmic_ctrl *pm = &pmic_ctrl;
	struct pmic_buf *tp;
	struct pmic_buf	*rp;
	int	i, stat, more_data;


	if (mutex_lock_interruptible(&pmic_mtx))
		return -ERESTARTSYS;

	if (pm->inited <= 0) {
		stat = pmic_buf_init();
		if (stat < 0) {
			mutex_unlock(&pmic_mtx);
			return stat;
		}
	}

	tp = &pm->tbuf;
	rp = &pm->rbuf;

	pmic_buf_reset(tp);
	pmic_buf_reserve(tp, sizeof(struct rpc_request_hdr));
	pmic_buf_reset(rp);

	if (xflag)
		pmic_put_tx_data(tp, xdata);

	more_data = 1; 		/* tell server there have more data followed */
	pmic_put_tx_data(tp, more_data);

	size >>= 2;
	for (i = 0; i < size; i++) {
		pmic_put_tx_data(tp, *data);
		data++;
	}

	stat = pmic_rpc_req_reply(tp, rp, proc);
	if (stat < 0) {
		mutex_unlock(&pmic_mtx);
		return stat;
	}

	pmic_pull_rx_data(rp, &stat);	/* result from server */

	mutex_unlock(&pmic_mtx);

	return modem_to_linux_err(stat);
}

/**
 * pmic_rpc_set_get() - set one argument and get one argument
 * @setdata:	set argument
 * @*getdata:	memory to store argumnet
 * @size:	size of memory
 * @proc:	command/request id
 *
 * This function covers case f
 */
static int pmic_rpc_set_get(uint setdata, uint *getdata, int size, int proc)
{
	struct pmic_ctrl *pm = &pmic_ctrl;
	struct pmic_buf	*tp;
	struct pmic_buf	*rp;
	unsigned int *lp;
	int i, stat, more_data;


	if (mutex_lock_interruptible(&pmic_mtx))
		return -ERESTARTSYS;

	if (pm->inited <= 0) {
		stat = pmic_buf_init();
		if (stat < 0) {
			mutex_unlock(&pmic_mtx);
			return stat;
		}
	}

	tp = &pm->tbuf;
	rp = &pm->rbuf;

	pmic_buf_reset(tp);
	pmic_buf_reserve(tp, sizeof(struct rpc_request_hdr));
	pmic_buf_reset(rp);

	pmic_put_tx_data(tp, setdata);

	/*
	* more_data = TRUE to ask server reply with requested datum
	* otherwise, server will reply without datum
	*/
	more_data = (getdata != NULL);
	pmic_put_tx_data(tp, more_data);

	stat = pmic_rpc_req_reply(tp, rp, proc);
	if (stat < 0) {
		mutex_unlock(&pmic_mtx);
		return stat;
	}

	pmic_pull_rx_data(rp, &stat);		/* result from server */
	pmic_pull_rx_data(rp, &more_data);

	if (more_data) { 				/* more data followed */
		size >>= 2;
		lp = getdata;
		for (i = 0; i < size; i++) {
			if (pmic_pull_rx_data(rp, lp++) < 0)
				break;	/* not supposed to happen */
		}
	}

	mutex_unlock(&pmic_mtx);

	return modem_to_linux_err(stat);
}

/**
 * pmic_rpc_get_only() - get one or more than one arguments
 * @*getdata:	memory to store arguments
 * @size:	size of mmory
 * @proc:	command/request id
 *
 * This function covers case g
 */
static int pmic_rpc_get_only(uint *getdata, int size, int proc)
{
	struct pmic_ctrl *pm = &pmic_ctrl;
	struct pmic_buf *tp;
	struct pmic_buf *rp;
	unsigned int *lp;
	int	i, stat, more_data;


	if (mutex_lock_interruptible(&pmic_mtx))
		return -ERESTARTSYS;

	if (pm->inited <= 0) {
		stat = pmic_buf_init();
		if (stat < 0) {
			mutex_unlock(&pmic_mtx);
			return stat;
		}
	}

	tp = &pm->tbuf;
	rp = &pm->rbuf;

	pmic_buf_reset(tp);
	pmic_buf_reserve(tp, sizeof(struct rpc_request_hdr));
	pmic_buf_reset(rp);

	/*
	* more_data = TRUE to ask server reply with requested datum
	* otherwise, server will reply without datum
	*/
	more_data = (getdata != NULL);
	pmic_put_tx_data(tp, more_data);

	stat = pmic_rpc_req_reply(tp, rp, proc);
	if (stat < 0) {
		mutex_unlock(&pmic_mtx);
		return stat;
	}

	pmic_pull_rx_data(rp, &stat);		/* result from server */
	pmic_pull_rx_data(rp, &more_data);

	if (more_data) { 				/* more data followed */
		size >>= 2;
		lp = getdata;
		for (i = 0; i < size; i++) {
			if (pmic_pull_rx_data(rp, lp++) < 0)
				break;	/* not supposed to happen */
		}
	}

	mutex_unlock(&pmic_mtx);

	return modem_to_linux_err(stat);
}


int pmic_lp_mode_control(enum switch_cmd cmd, enum vreg_lp_id id)
{
	return pmic_rpc_set_only(cmd, id, 0, 0, 2, LP_MODE_CONTROL_PROC);
}
EXPORT_SYMBOL(pmic_lp_mode_control);

int pmic_vreg_set_level(enum vreg_id vreg, int level)
{
	return pmic_rpc_set_only(vreg, level, 0, 0, 2, VREG_SET_LEVEL_PROC);
}
EXPORT_SYMBOL(pmic_vreg_set_level);

int pmic_vreg_pull_down_switch(enum switch_cmd cmd, enum vreg_pdown_id id)
{
	return pmic_rpc_set_only(cmd, id, 0, 0, 2, VREG_PULL_DOWN_SWITCH_PROC);
}
EXPORT_SYMBOL(pmic_vreg_pull_down_switch);

int pmic_secure_mpp_control_digital_output(enum mpp_which which,
	enum mpp_dlogic_level level,
	enum mpp_dlogic_out_ctrl out)
{
	return pmic_rpc_set_only(which, level, out, 0, 3,
				SECURE_MPP_CONFIG_DIGITAL_OUTPUT_PROC);
}
EXPORT_SYMBOL(pmic_secure_mpp_control_digital_output);

int pmic_secure_mpp_config_i_sink(enum mpp_which which,
				enum mpp_i_sink_level level,
				enum mpp_i_sink_switch onoff)
{
	return pmic_rpc_set_only(which, level, onoff, 0, 3,
				SECURE_MPP_CONFIG_I_SINK_PROC);
}
EXPORT_SYMBOL(pmic_secure_mpp_config_i_sink);

int pmic_secure_mpp_config_digital_input(enum mpp_which which,
	enum mpp_dlogic_level level,
	enum mpp_dlogic_in_dbus dbus)
{
	return pmic_rpc_set_only(which, level, dbus, 0, 3,
				SECURE_MPP_CONFIG_DIGITAL_INPUT_PROC);
}
EXPORT_SYMBOL(pmic_secure_mpp_config_digital_input);

int pmic_rtc_start(struct rtc_time *time)
{
	return pmic_rpc_set_struct(0, 0, (uint *)time, sizeof(*time),
				RTC_START_PROC);
}
EXPORT_SYMBOL(pmic_rtc_start);

int pmic_rtc_stop(void)
{
	return pmic_rpc_set_only(0, 0, 0, 0, 0, RTC_STOP_PROC);
}
EXPORT_SYMBOL(pmic_rtc_stop);

int pmic_rtc_get_time(struct rtc_time *time)
{
	return pmic_rpc_get_only((uint *)time, sizeof(*time),
				RTC_GET_TIME_PROC);
}
EXPORT_SYMBOL(pmic_rtc_get_time);

int pmic_rtc_enable_alarm(enum rtc_alarm alarm,
	struct rtc_time *time)
{
	return pmic_rpc_set_struct(1, alarm, (uint *)time, sizeof(*time),
				RTC_ENABLE_ALARM_PROC);
}
EXPORT_SYMBOL(pmic_rtc_enable_alarm);

int pmic_rtc_disable_alarm(enum rtc_alarm alarm)
{
	return pmic_rpc_set_only(alarm, 0, 0, 0, 1, RTC_DISABLE_ALARM_PROC);
}
EXPORT_SYMBOL(pmic_rtc_disable_alarm);

int pmic_rtc_get_alarm_time(enum rtc_alarm	alarm,
	struct rtc_time *time)
{
	return pmic_rpc_set_get(alarm, (uint *)time, sizeof(*time),
				RTC_GET_ALARM_TIME_PROC);
}
EXPORT_SYMBOL(pmic_rtc_get_alarm_time);

int pmic_rtc_get_alarm_status(uint *status)
{
	return pmic_rpc_get_only(status, sizeof(*status),
				RTC_GET_ALARM_STATUS_PROC);
}
EXPORT_SYMBOL(pmic_rtc_get_alarm_status);

int pmic_rtc_set_time_adjust(uint adjust)
{
	return pmic_rpc_set_only(adjust, 0, 0, 0, 1,
				RTC_SET_TIME_ADJUST_PROC);
}
EXPORT_SYMBOL(pmic_rtc_set_time_adjust);

int pmic_rtc_get_time_adjust(uint *adjust)
{
	return pmic_rpc_get_only(adjust, sizeof(*adjust),
				RTC_GET_TIME_ADJUST_PROC);
}
EXPORT_SYMBOL(pmic_rtc_get_time_adjust);

/*
 * generic speaker
 */
int pmic_speaker_cmd(const enum spkr_cmd cmd)
{
	return pmic_rpc_set_only(cmd, 0, 0, 0, 1, SPEAKER_CMD_PROC);
}
EXPORT_SYMBOL(pmic_speaker_cmd);

int pmic_set_spkr_configuration(struct spkr_config_mode	*cfg)
{
	return pmic_rpc_set_struct(0, 0, (uint *)cfg, sizeof(*cfg),
				SET_SPKR_CONFIGURATION_PROC);
}
EXPORT_SYMBOL(pmic_set_spkr_configuration);

int pmic_get_spkr_configuration(struct spkr_config_mode *cfg)
{
	return pmic_rpc_get_only((uint *)cfg, sizeof(*cfg),
				GET_SPKR_CONFIGURATION_PROC);
}
EXPORT_SYMBOL(pmic_get_spkr_configuration);

int pmic_spkr_en_right_chan(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, SPKR_EN_RIGHT_CHAN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_right_chan);

int pmic_spkr_is_right_chan_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_RIGHT_CHAN_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_right_chan_en);

int pmic_spkr_en_left_chan(uint	enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, SPKR_EN_LEFT_CHAN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_left_chan);

int pmic_spkr_is_left_chan_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_LEFT_CHAN_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_left_chan_en);

int pmic_set_speaker_gain(enum spkr_gain gain)
{
	return pmic_rpc_set_only(gain, 0, 0, 0, 1, SET_SPEAKER_GAIN_PROC);
}
EXPORT_SYMBOL(pmic_set_speaker_gain);

int pmic_set_speaker_delay(enum spkr_dly delay)
{
	return pmic_rpc_set_only(delay, 0, 0, 0, 1, SET_SPEAKER_DELAY_PROC);
}
EXPORT_SYMBOL(pmic_set_speaker_delay);

int pmic_speaker_1k6_zin_enable(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1,
				SPEAKER_1K6_ZIN_ENABLE_PROC);
}
EXPORT_SYMBOL(pmic_speaker_1k6_zin_enable);

int pmic_spkr_set_mux_hpf_corner_freq(enum spkr_hpf_corner_freq	freq)
{
	return pmic_rpc_set_only(freq, 0, 0, 0, 1,
				SPKR_SET_MUX_HPF_CORNER_FREQ_PROC);
}
EXPORT_SYMBOL(pmic_spkr_set_mux_hpf_corner_freq);

int pmic_spkr_get_mux_hpf_corner_freq(enum spkr_hpf_corner_freq	*freq)
{
	return pmic_rpc_get_only(freq, sizeof(*freq),
				SPKR_GET_MUX_HPF_CORNER_FREQ_PROC);
}
EXPORT_SYMBOL(pmic_spkr_get_mux_hpf_corner_freq);

int pmic_spkr_select_usb_with_hpf_20hz(uint	enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1,
				SPKR_SELECT_USB_WITH_HPF_20HZ_PROC);
}
EXPORT_SYMBOL(pmic_spkr_select_usb_with_hpf_20hz);

int pmic_spkr_is_usb_with_hpf_20hz(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_USB_WITH_HPF_20HZ_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_usb_with_hpf_20hz);

int pmic_spkr_bypass_mux(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, SPKR_BYPASS_MUX_PROC);
}
EXPORT_SYMBOL(pmic_spkr_bypass_mux);

int pmic_spkr_is_mux_bypassed(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_MUX_BYPASSED_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_mux_bypassed);

int pmic_spkr_en_hpf(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, SPKR_EN_HPF_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_hpf);

int pmic_spkr_is_hpf_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_HPF_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_hpf_en);

int pmic_spkr_en_sink_curr_from_ref_volt_cir(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1,
				SPKR_EN_SINK_CURR_FROM_REF_VOLT_CIR_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_sink_curr_from_ref_volt_cir);

int pmic_spkr_is_sink_curr_from_ref_volt_cir_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_SINK_CURR_FROM_REF_VOLT_CIR_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_sink_curr_from_ref_volt_cir_en);

/*
 * 	speaker indexed by left_right
 */
int pmic_spkr_en(enum spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2, SPKR_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en);

int pmic_spkr_is_en(enum spkr_left_right left_right, uint *enabled)
{
	return pmic_rpc_set_get(left_right, enabled, sizeof(*enabled),
				SPKR_IS_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_en);

int pmic_spkr_set_gain(enum spkr_left_right left_right, enum spkr_gain gain)
{
	return pmic_rpc_set_only(left_right, gain, 0, 0, 2, SPKR_SET_GAIN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_set_gain);

int pmic_spkr_get_gain(enum spkr_left_right left_right, enum spkr_gain *gain)
{
	return pmic_rpc_set_get(left_right, gain, sizeof(*gain),
				SPKR_GET_GAIN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_get_gain);

int pmic_spkr_set_delay(enum spkr_left_right left_right, enum spkr_dly delay)
{
	return pmic_rpc_set_only(left_right, delay, 0, 0, 2,
				SPKR_SET_DELAY_PROC);
}
EXPORT_SYMBOL(pmic_spkr_set_delay);

int pmic_spkr_get_delay(enum spkr_left_right left_right, enum spkr_dly *delay)
{
	return pmic_rpc_set_get(left_right, delay, sizeof(*delay),
				SPKR_GET_DELAY_PROC);
}
EXPORT_SYMBOL(pmic_spkr_get_delay);

int pmic_spkr_en_mute(enum spkr_left_right left_right, uint enabled)
{
	return pmic_rpc_set_only(left_right, enabled, 0, 0, 2,
				SPKR_EN_MUTE_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_mute);

int pmic_spkr_is_mute_en(enum spkr_left_right left_right, uint *enabled)
{
	return pmic_rpc_set_get(left_right, enabled, sizeof(*enabled),
				SPKR_IS_MUTE_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_mute_en);

int pmic_spkr_set_vsel_ldo(enum spkr_left_right left_right,
					enum spkr_ldo_v_sel vlt_cntrl)
{
	return pmic_rpc_set_only(left_right, vlt_cntrl, 0, 0, 2,
			SPKR_SET_VSEL_LDO_PROC);
}
EXPORT_SYMBOL(pmic_spkr_set_vsel_ldo);

int pmic_spkr_set_boost(enum spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			SPKR_SET_BOOST_PROC);
}
EXPORT_SYMBOL(pmic_spkr_set_boost);

int pmic_spkr_bypass_en(enum spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			SPKR_BYPASS_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_bypass_en);

/*
 * 	mic
 */
int pmic_mic_en(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, MIC_EN_PROC);
}
EXPORT_SYMBOL(pmic_mic_en);

int pmic_mic_is_en(uint	*enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled), MIC_IS_EN_PROC);
}
EXPORT_SYMBOL(pmic_mic_is_en);

int pmic_mic_set_volt(enum mic_volt vol)
{
	return pmic_rpc_set_only(vol, 0, 0, 0, 1, MIC_SET_VOLT_PROC);
}
EXPORT_SYMBOL(pmic_mic_set_volt);

int pmic_mic_get_volt(enum mic_volt *voltage)
{
	return pmic_rpc_get_only(voltage, sizeof(*voltage), MIC_GET_VOLT_PROC);
}
EXPORT_SYMBOL(pmic_mic_get_volt);

int pmic_vib_mot_set_volt(uint vol)
{
	return pmic_rpc_set_only(vol, 0, 0, 0, 1, VIB_MOT_SET_VOLT_PROC);
}
EXPORT_SYMBOL(pmic_vib_mot_set_volt);

int pmic_vib_mot_set_mode(enum pm_vib_mot_mode mode)
{
	return pmic_rpc_set_only(mode, 0, 0, 0, 1, VIB_MOT_SET_MODE_PROC);
}
EXPORT_SYMBOL(pmic_vib_mot_set_mode);

int pmic_vib_mot_set_polarity(enum pm_vib_mot_pol pol)
{
	return pmic_rpc_set_only(pol, 0, 0, 0, 1, VIB_MOT_SET_POLARITY_PROC);
}
EXPORT_SYMBOL(pmic_vib_mot_set_polarity);

int pmic_vid_en(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, VID_EN_PROC);
}
EXPORT_SYMBOL(pmic_vid_en);

int pmic_vid_is_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled), VID_IS_EN_PROC);
}
EXPORT_SYMBOL(pmic_vid_is_en);

int pmic_vid_load_detect_en(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, VID_LOAD_DETECT_EN_PROC);
}
EXPORT_SYMBOL(pmic_vid_load_detect_en);

int pmic_set_led_intensity(enum ledtype type, int level)
{
	return pmic_rpc_set_only(type, level, 0, 0, 2, SET_LED_INTENSITY_PROC);
}
EXPORT_SYMBOL(pmic_set_led_intensity);

int pmic_flash_led_set_current(const uint16_t milliamps)
{
	return pmic_rpc_set_only(milliamps, 0, 0, 0, 1,
				FLASH_LED_SET_CURRENT_PROC);
}
EXPORT_SYMBOL(pmic_flash_led_set_current);

int pmic_flash_led_set_mode(enum flash_led_mode mode)
{
	return pmic_rpc_set_only((int)mode, 0, 0, 0, 1,
				FLASH_LED_SET_MODE_PROC);
}
EXPORT_SYMBOL(pmic_flash_led_set_mode);

int pmic_flash_led_set_polarity(enum flash_led_pol pol)
{
	return pmic_rpc_set_only((int)pol, 0, 0, 0, 1,
				FLASH_LED_SET_POLARITY_PROC);
}
EXPORT_SYMBOL(pmic_flash_led_set_polarity);

int pmic_spkr_add_right_left_chan(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1,
				SPKR_ADD_RIGHT_LEFT_CHAN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_add_right_left_chan);

int pmic_spkr_is_right_left_chan_added(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_RIGHT_LEFT_CHAN_ADDED_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_right_left_chan_added);

int pmic_spkr_en_stereo(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, SPKR_EN_STEREO_PROC);
}
EXPORT_SYMBOL(pmic_spkr_en_stereo);

int pmic_spkr_is_stereo_en(uint *enabled)
{
	return pmic_rpc_get_only(enabled, sizeof(*enabled),
				SPKR_IS_STEREO_EN_PROC);
}
EXPORT_SYMBOL(pmic_spkr_is_stereo_en);

int pmic_hsed_set_period(
	enum hsed_controller controller,
	enum hsed_period_pre_div period_pre_div,
	enum hsed_period_time period_time
)
{
	return pmic_rpc_set_only(controller, period_pre_div, period_time, 0,
				 3,
				 HSED_SET_PERIOD_PROC);
}
EXPORT_SYMBOL(pmic_hsed_set_period);

int pmic_hsed_set_hysteresis(
	enum hsed_controller controller,
	enum hsed_hyst_pre_div hyst_pre_div,
	enum hsed_hyst_time hyst_time
)
{
	return pmic_rpc_set_only(controller, hyst_pre_div, hyst_time, 0,
				 3,
				 HSED_SET_HYSTERESIS_PROC);
}
EXPORT_SYMBOL(pmic_hsed_set_hysteresis);

int pmic_hsed_set_current_threshold(
	enum hsed_controller controller,
	enum hsed_switch switch_hsed,
	uint32_t current_threshold
)
{
	return pmic_rpc_set_only(controller, switch_hsed, current_threshold, 0,
				 3,
				 HSED_SET_CURRENT_THRESHOLD_PROC);
}
EXPORT_SYMBOL(pmic_hsed_set_current_threshold);

int pmic_hsed_enable(
	enum hsed_controller controller,
	enum hsed_enable enable_hsed
)
{
	return pmic_rpc_set_only(controller, enable_hsed, 0, 0,
				 2,
				 HSED_ENABLE_PROC);
}
EXPORT_SYMBOL(pmic_hsed_enable);

int pmic_high_current_led_set_current(enum high_current_led led,
		uint16_t milliamps)
{
	return pmic_rpc_set_only(led, milliamps, 0, 0,
			2,
			HIGH_CURRENT_LED_SET_CURRENT_PROC);
}
EXPORT_SYMBOL(pmic_high_current_led_set_current);

int pmic_high_current_led_set_polarity(enum high_current_led led,
		enum flash_led_pol polarity)
{
	return pmic_rpc_set_only(led, polarity, 0, 0,
			2,
			HIGH_CURRENT_LED_SET_POLARITY_PROC);
}
EXPORT_SYMBOL(pmic_high_current_led_set_polarity);

int pmic_high_current_led_set_mode(enum high_current_led led,
		enum flash_led_mode mode)
{
	return pmic_rpc_set_only(led, mode, 0, 0,
			2,
			HIGH_CURRENT_LED_SET_MODE_PROC);
}
EXPORT_SYMBOL(pmic_high_current_led_set_mode);

int pmic_lp_force_lpm_control(enum switch_cmd cmd,
		enum vreg_lpm_id vreg)
{
	return pmic_rpc_set_only(cmd, vreg, 0, 0,
			2,
			LP_FORCE_LPM_CONTROL_PROC);
}
EXPORT_SYMBOL(pmic_lp_force_lpm_control);

int pmic_low_current_led_set_ext_signal(enum low_current_led led,
		enum ext_signal sig)
{
	return pmic_rpc_set_only(led, sig, 0, 0,
			2,
			LOW_CURRENT_LED_SET_EXT_SIGNAL_PROC);
}
EXPORT_SYMBOL(pmic_low_current_led_set_ext_signal);

int pmic_low_current_led_set_current(enum low_current_led led,
		uint16_t milliamps)
{
	return pmic_rpc_set_only(led, milliamps, 0, 0,
			2,
			LOW_CURRENT_LED_SET_CURRENT_PROC);
}
EXPORT_SYMBOL(pmic_low_current_led_set_current);

/*
 * Head phone speaker
 */
int pmic_hp_spkr_mstr_en(enum hp_spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			HP_SPKR_MSTR_EN_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_mstr_en);

int pmic_hp_spkr_mute_en(enum hp_spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			HP_SPKR_MUTE_EN_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_mute_en);

int pmic_hp_spkr_prm_in_en(enum hp_spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			HP_SPKR_PRM_IN_EN_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_prm_in_en);

int pmic_hp_spkr_aux_in_en(enum hp_spkr_left_right left_right, uint enable)
{
	return pmic_rpc_set_only(left_right, enable, 0, 0, 2,
			HP_SPKR_AUX_IN_EN_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_aux_in_en);

int pmic_hp_spkr_ctrl_prm_gain_input(enum hp_spkr_left_right left_right,
							uint prm_gain_ctl)
{
	return pmic_rpc_set_only(left_right, prm_gain_ctl, 0, 0, 2,
			HP_SPKR_CTRL_PRM_GAIN_INPUT_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_ctrl_prm_gain_input);

int pmic_hp_spkr_ctrl_aux_gain_input(enum hp_spkr_left_right left_right,
							uint aux_gain_ctl)
{
	return pmic_rpc_set_only(left_right, aux_gain_ctl, 0, 0, 2,
			HP_SPKR_CTRL_AUX_GAIN_INPUT_PROC);
}
EXPORT_SYMBOL(pmic_hp_spkr_ctrl_aux_gain_input);

int pmic_xo_core_force_enable(uint enable)
{
	return pmic_rpc_set_only(enable, 0, 0, 0, 1, XO_CORE_FORCE_ENABLE);
}
EXPORT_SYMBOL(pmic_xo_core_force_enable);

int pmic_gpio_direction_input(unsigned gpio)
{
	return pmic_rpc_set_only(gpio, 0, 0, 0, 1,
			GPIO_SET_GPIO_DIRECTION_INPUT_PROC);
}
EXPORT_SYMBOL(pmic_gpio_direction_input);

int pmic_gpio_direction_output(unsigned gpio)
{
	return pmic_rpc_set_only(gpio, 0, 0, 0, 1,
			GPIO_SET_GPIO_DIRECTION_OUTPUT_PROC);
}
EXPORT_SYMBOL(pmic_gpio_direction_output);

int pmic_gpio_set_value(unsigned gpio, int value)
{
	return pmic_rpc_set_only(gpio, value, 0, 0, 2, GPIO_SET_PROC);
}
EXPORT_SYMBOL(pmic_gpio_set_value);

int pmic_gpio_get_value(unsigned gpio)
{
	uint value;
	int ret;

	ret = pmic_rpc_set_get(gpio, &value, sizeof(value), GPIO_GET_PROC);
	if (ret < 0)
		return ret;
	return value ? 1 : 0;
}
EXPORT_SYMBOL(pmic_gpio_get_value);

int pmic_gpio_get_direction(unsigned gpio)
{
	enum pmic_direction_mode dir;
	int ret;

	ret = pmic_rpc_set_get(gpio, &dir, sizeof(dir),
			GPIO_GET_GPIO_DIRECTION_PROC);
	if (ret < 0)
		return ret;
	return dir;
}
EXPORT_SYMBOL(pmic_gpio_get_direction);

int pmic_gpio_config(struct pm8xxx_gpio_rpc_cfg *param)
{
	return pmic_rpc_set_struct(0, 0, (uint *)param, sizeof(*param),
			GPIO_SET_GPIO_CONFIG_PROC);
}
EXPORT_SYMBOL(pmic_gpio_config);
