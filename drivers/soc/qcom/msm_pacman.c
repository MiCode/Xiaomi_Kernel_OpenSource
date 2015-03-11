/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define PACMAN_VERSION	"1.1"
#define CLASS_NAME "msm_pacman"
#define DEVICE_NAME "msm_pacman"
/* Enable pr_debug() logs */
/* #define DEBUG */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>

#include <soc/qcom/scm.h>
#include "msm_pacman_qmi_interface.h"

#define TEST_DRIVER_NAME_LEN 5

struct pacman_control_s {
	dev_t dev_num;
	struct class *dev_class;
	struct device *dev;
};
static struct pacman_control_s pacman_ctl;
static char test_string[TEST_DRIVER_NAME_LEN];

/******************************************************************************
* PACMan Configuration
******************************************************************************/
/* Number of QUPs supported */
#define NUM_QUPS 12

/* Bypass bus bind/unbind calls and return success */
/* #define DEBUG_BYPASS_BINDING */
/* Bypass TrustZone calls and return success */
/* #define DEBUG_BYPASS_TZ */
/* Bypass QMI calls and return success */
/* #define DEBUG_BYPASS_QMI */

/******************************************************************************
* Trust Zone API
******************************************************************************/
#ifdef DEBUG_BYPASS_TZ
static int tz_configure_blsp_ownership(u32 qup_id, u32 subsystem_id)
{
	return 0;
}
#else
static int tz_configure_blsp_ownership(u32 qup_id, u32 subsystem_id)
{
	int rc = 0;

	if (strcmp("test", test_string)) {
		struct scm_desc desc = {0};
		desc.arginfo = 2;
		desc.args[0] = qup_id;
		desc.args[1] = subsystem_id;

		rc = scm_call2(SCM_SIP_FNID(4, 3), &desc);
	}
	return rc;
}
#endif

/******************************************************************************
* State Machine Framework
******************************************************************************/
/* State Machine Input Events (edges) */
typedef enum state_machine_input_enum state_machine_input_t;

/* State Machine States (nodes are represented as function pointers) */
typedef struct state_machine_s state_machine_t;
typedef void (*state_machine_state_fn)(state_machine_t *sm,
					state_machine_input_t input,
					void *arg);
struct state_machine_s {
	state_machine_state_fn curr_state; /* Read only */
	state_machine_state_fn next_state; /* Update in state function */
	char *curr_state_string;           /* Read only */

	/* Pseudo v-table */
	void (*init)(state_machine_t *this);
	void (*run)(state_machine_t *this, state_machine_input_t input,
			void *arg);
};

/* State Machine Member Function Prototypes */
static void state_machine_init(state_machine_t *this);
static void state_machine_run(state_machine_t *this,
		state_machine_input_t input, void *arg);

/******************************************************************************
* State Machine Definition
******************************************************************************/
/* State Machine Input Events (edges) */
enum state_machine_input_enum {
	INIT = 0,
	ENABLE = 1,
	DISABLE = 2,
	SSR = 3
};

/* State Machine States (nodes) */
/* If adding a state, be sure to also update state_machine_run() strings */
static void pacman_state_init(state_machine_t *this,
			state_machine_input_t input, void *arg);
static void pacman_state_disabled(state_machine_t *this,
			state_machine_input_t input, void *arg);
static void pacman_state_enabling(state_machine_t *this,
			state_machine_input_t input, void *arg);
static void pacman_state_enabled(state_machine_t *this,
			state_machine_input_t input, void *arg);
static void pacman_state_disabling(state_machine_t *this,
			state_machine_input_t input, void *arg);

/* State Machine Member Functions */
static void state_machine_init(state_machine_t *this)
{
	pr_debug("%s\n", __func__);
	memset(this, '0', sizeof(state_machine_t));
	this->curr_state = pacman_state_init;
	this->next_state = pacman_state_init;
	this->curr_state_string = "INIT";

	/* Setup Pseudo V-Table */
	this->init = state_machine_init;
	this->run = state_machine_run;

	/* Run the state machine with the 'INIT' event */
	this->run(this, INIT, NULL);
}

static void state_machine_run(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	state_machine_state_fn tmp_state;

	pr_debug("%s: input %u\n", __func__, input);
	if (NULL == this || NULL == this->curr_state)
		return;

	/* Execute the current state with the given input event */
	(this->curr_state)(this, input, NULL);

	/* If the current state set this->next_state, process the transition */
	while (this->curr_state != this->next_state) {
		if (NULL == this->next_state)
			break;

		tmp_state = this->next_state;
		(this->next_state)(this, input, arg);

		/* Transition complete - update curr_state */
		this->curr_state = tmp_state;
		if (this->curr_state == pacman_state_init)
			this->curr_state_string = "STATE_INIT";
		else if (this->curr_state == pacman_state_disabled)
			this->curr_state_string = "STATE_DISABLED";
		else if (this->curr_state == pacman_state_enabling)
			this->curr_state_string = "STATE_ENABLING";
		else if (this->curr_state == pacman_state_enabled)
			this->curr_state_string = "STATE_ENABLED";
		else if (this->curr_state == pacman_state_disabling)
			this->curr_state_string = "STATE_DISABLING";
		else
			this->curr_state_string = "STATE_UNKNOWN";
	}
}

/******************************************************************************
* QUP Framework (struct qup_instance passed as argument to state machine)
******************************************************************************/
enum subsystem {
	NONE = 0,
	APSS = 1,
	ADSP = 2
};

const char *subsystem_strings[] = {
	"NONE",
	"APSS",
	"ADSP"
};

struct qup_instance {
	int id;
	enum subsystem owner;
	enum subsystem next_owner;
	state_machine_t state_machine;
	struct device *bus_dev;
	struct device_driver *bus_drv;
};

/******************************************************************************
* PACMan Framework
******************************************************************************/
static void pacman_framework_init(void);
static void pacman_run(char *command_string);
static void pacman_dump(char *my_buff, size_t length);

/* Globals */
static DEFINE_MUTEX(pacman_mutex);
static struct qup_instance QUP_TABLE[NUM_QUPS];
static int PACMAN_ADSP_IN_SSR;

static void pacman_framework_init(void)
{
	int i;
	pr_debug("%s: Number of QUPs = %i\n", __func__, NUM_QUPS);

	/* Initialize Global QUP Table */
	for (i = 0; i < NUM_QUPS; i++) {
		QUP_TABLE[i].id = -1;	/* -1 Invalid */
		QUP_TABLE[i].owner = NONE;
		QUP_TABLE[i].next_owner = NONE;
		QUP_TABLE[i].bus_dev = NULL;
		QUP_TABLE[i].bus_drv = NULL;
		state_machine_init(&QUP_TABLE[i].state_machine);
	}

	mutex_init(&pacman_mutex);
}

/*
 * Expect format to be "<string bus_id> <string subsytem_name>"
 * i.e. "f9928000.i2c ADSP"
 * Bus IDs
 *   i2c: /sys/bus/platform/drivers/i2c-msm-v2/
 *   SPI: /sys/bus/platform/drivers/spi_qsd/
 */
static void pacman_run(char *command_string)
{
	char my_buff[32], bus_string[16], subsystem_string[8];
	char *str, *str_running;
	const char delimiters[] = " \n";
	struct device *bus_dev;
	enum subsystem subsystem;
	int qup_id;

	mutex_lock(&pacman_mutex);
	strlcpy(my_buff, command_string, sizeof(my_buff));
	my_buff[31] = '\0';
	str_running = (char *)&my_buff;

	pr_debug("%s: %s\n", __func__, (char *)&my_buff);

	/* Parse Bus ID */
	str = strsep(&str_running, delimiters);
	if (NULL == str) {
		pr_err("%s: ERROR strsep expected bus name\n",
				__func__);
		goto error;
	}
	strlcpy(bus_string, str, sizeof(bus_string));
	strlcpy(test_string, str, sizeof(test_string));

	/* Parse Subsystem Name */
	str = strsep(&str_running, delimiters);
	if (NULL == str) {
		pr_err("%s: ERROR strsep expected subsystem name\n",
				__func__);
		goto error;
	}
	strlcpy(subsystem_string, str, sizeof(subsystem_string));

	/* Parse Extra Arguments */
	str = strsep(&str_running, delimiters);
	if (NULL != str) {
		if (0 != strcmp("", str))
			pr_debug("%s: Extra arguments are being ignored\n",
					__func__);
	}

	/* Validate Bus Argument*/
	bus_dev = bus_find_device_by_name(&platform_bus_type, NULL, bus_string);
	if (!bus_dev) {
		pr_err("%s: ERROR invalid bus\n", __func__);
		goto error;
	}

	/* Validate Bus to QUP ID mapping exists in device tree */
	/* i.e. Device tree contains aliases{ qup5 = &i2c5; } */
	qup_id = of_alias_get_id(bus_dev->of_node, "qup");
	if (qup_id < 0) {
		pr_err("%s: ERROR QUP ID not found\n", __func__);
		goto error;
	}

	/* Validate Subsystem Argument*/
	if (0 == strcmp("APSS", subsystem_string))
		subsystem = APSS;
	else if (0 == strcmp("ADSP", subsystem_string))
		subsystem = ADSP;
	else if (0 == strcmp("SSR", subsystem_string))
		subsystem = NONE;
	else {
		pr_err("%s: ERROR invalid subsystem\n", __func__);
		goto error;
	}

	pr_info("%s: Bus=%s QUP=%i Subsystem=%s\n", __func__,
		bus_string, qup_id, subsystem_string);

	/* Flag QUP as valid upon first PACman invocation */
	QUP_TABLE[qup_id].id = qup_id;
	QUP_TABLE[qup_id].bus_dev = bus_dev;
	/* Prepare for state machine */
	QUP_TABLE[qup_id].next_owner = subsystem;

	/* Invoke state machine transition */
	switch (subsystem) {
	case APSS:
		QUP_TABLE[qup_id].state_machine.run(
				&QUP_TABLE[qup_id].state_machine,
				ENABLE, &QUP_TABLE[qup_id]);
		if (QUP_TABLE[qup_id].state_machine.curr_state ==
			pacman_state_enabled) {
			QUP_TABLE[qup_id].owner = APSS;
		} else {
			QUP_TABLE[qup_id].owner = NONE;
		}
		break;
	case ADSP:
		QUP_TABLE[qup_id].state_machine.run(
				&QUP_TABLE[qup_id].state_machine,
				DISABLE, &QUP_TABLE[qup_id]);
		if (QUP_TABLE[qup_id].state_machine.curr_state ==
			pacman_state_disabled) {
			QUP_TABLE[qup_id].owner = ADSP;
		} else {
			QUP_TABLE[qup_id].owner = NONE;
		}
		break;
	case NONE: /* SSR */
		pr_info("%s: SSR initiated on QUP %i\n", __func__, qup_id);
		QUP_TABLE[qup_id].state_machine.run(
				&QUP_TABLE[qup_id].state_machine,
				SSR, &QUP_TABLE[qup_id]);

		/* TODO: Notify client of SSR event via poll() implementation */
		/* Currently ownership is passed back after SSR */
		/* Do not update the table owner for now - used to detect SSR */
		/*
		if (QUP_TABLE[qup_id].state_machine.curr_state ==
			pacman_state_enabled) {
			QUP_TABLE[qup_id].owner = APSS;
		} else {
			QUP_TABLE[qup_id].owner = NONE;
		}
		*/
		break;
	default:
		pr_err("%s: ERROR owner not supported\n", __func__);
		goto error;
	}

error:
	mutex_unlock(&pacman_mutex);
	return;
}

static void pacman_dump(char *my_buff, size_t length)
{
	char my_working_buff[32];
	int rc, i;

	pr_debug("%s\n", __func__);

	if (NULL == my_buff || length < 256) {
		pr_debug("%s: ERROR input buffer too small\n", __func__);
		return;
	}

	mutex_lock(&pacman_mutex);

	/* Parse QUP table and dump state*/
	my_buff[0] = '\0';

	for (i = 0; i < NUM_QUPS; i++) {
		/* Check if enabled for dumping */
		if (QUP_TABLE[i].id >= 0) {
			rc = snprintf((char *)(&my_working_buff),
				sizeof(my_working_buff),
				"%u %s %s\n", QUP_TABLE[i].id,
				subsystem_strings[QUP_TABLE[i].owner],
				QUP_TABLE[i].state_machine.curr_state_string);
			strlcat(my_buff, (char *)&my_working_buff, length);
			/* Check for overflow */
			if ((length - strlen(my_buff) - 1) <
				sizeof(my_working_buff)) {
				pr_debug("%s Overflow Detected\n",
						__func__);
				break;
			}
		}
	}

	mutex_unlock(&pacman_mutex);
}

/******************************************************************************
* QMI Framework
******************************************************************************/
/* Configuration */
#define PACMAN_QMI_TIMEOUT_MS 5000
#define PACMAN_QMI_ADSP_SERVICE_ID 771
#define PACMAN_QMI_ADSP_SERVICE_VERSION 1
#define PACMAN_QMI_ADSP_SERVICE_INSTANCE_ID 1

/* QMI Client Port Handles */
static struct qmi_handle *pacman_qmi_client_port_adsp;

/* TODO: Scale worker threads for other subsystems by passing the QMI
 * port handle from the callback function to the worker thread
 */
/* QMI Worker Threads */
static void pacman_qmi_client_notify_arrive_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_qmi_client_notify_arrive,
				pacman_qmi_client_notify_arrive_worker);
static void pacman_qmi_client_notify_exit_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_qmi_client_notify_exit,
				pacman_qmi_client_notify_exit_worker);
static void pacman_qmi_client_port_notify_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_qmi_client_port_notify,
		pacman_qmi_client_port_notify_worker);
static struct workqueue_struct *pacman_qmi_notification_workqueue;
static struct workqueue_struct *pacman_qmi_rx_workqueue;


/* Prototypes */
static int pacman_qmi_init(void);
static void pacman_qmi_deinit(void);
static int pacman_qmi_client_notify_cb(struct notifier_block *this,
					unsigned long code, void *cmd);
static void pacman_qmi_client_port_notify_cb(struct qmi_handle *handle,
						enum qmi_event_type event,
						void *notify_priv);
static int pacman_qmi_send_sync_ready_msg(struct qmi_handle *qmi_port_handle,
						int qup_id);
static int pacman_qmi_send_sync_take_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id);
static int pacman_qmi_send_sync_give_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id);

/* QMI Callbacks */
static int pacman_qmi_client_notify_cb(struct notifier_block *this,
					unsigned long code, void *cmd)
{
	int rc;

	switch (code) {
	case QMI_SERVER_ARRIVE:
		pr_debug("%s: QMI_SERVER_ARRIVE\n", __func__);
		rc = queue_delayed_work(pacman_qmi_notification_workqueue,
				&work_qmi_client_notify_arrive, 0);
		if (rc != 1)
			pr_err("%s ERROR QMI packet dropped\n", __func__);
		break;
	case QMI_SERVER_EXIT:
		pr_debug("%s: QMI_SERVER_EXIT\n", __func__);
		rc = queue_delayed_work(pacman_qmi_notification_workqueue,
				&work_qmi_client_notify_exit, 0);
		if (rc != 1)
			pr_err("%s ERROR QMI packet dropped\n", __func__);
		break;
	default:
		pr_err("%s: Unknown code %u\n", __func__, (unsigned)code);
		break;
	}

	return 0;
}

static void pacman_qmi_client_port_notify_cb(struct qmi_handle *handle,
						enum qmi_event_type event,
						void *notify_priv)
{
	int rc;

	pr_debug("%s\n", __func__);

	switch (event) {
	case QMI_RECV_MSG:
		rc = queue_delayed_work(pacman_qmi_rx_workqueue,
					&work_qmi_client_port_notify, 0);
		if (rc != 1)
			pr_err("%s ERROR QMI packet dropped\n", __func__);
		break;
	default:
		pr_err("%s: Unknown event %i\n", __func__, event);
		break;
	}
}

/* QMI Worker Threads */
static void pacman_qmi_client_notify_arrive_worker(struct work_struct *w_s)
{
	int rc, i;
	char buff[32];

	pr_debug("%s\n", __func__);

	/* Create a Local client port for QMI communication */
	pacman_qmi_client_port_adsp = qmi_handle_create(
				pacman_qmi_client_port_notify_cb, NULL);
	if (!pacman_qmi_client_port_adsp) {
		pr_err("%s: qmi_handle_create failed\n", __func__);
		return;
	}

	rc = qmi_connect_to_service(pacman_qmi_client_port_adsp,
					PACMAN_QMI_ADSP_SERVICE_ID,
					PACMAN_QMI_ADSP_SERVICE_VERSION,
					PACMAN_QMI_ADSP_SERVICE_INSTANCE_ID);

	if (rc < 0) {
		pr_err("%s: qmi_connect_to_service failed rc=%i\n",
			__func__, rc);
		qmi_handle_destroy(pacman_qmi_client_port_adsp);
		pacman_qmi_client_port_adsp = NULL;
		return;
	}

	/* Re-establish ADSP ownership if SSR occurred */
	/* Note: Notification work queue serializes use of
	 * IN_SSR flag and SSR
	 */
	if (PACMAN_ADSP_IN_SSR) {
		/* pacman_run("<QUP_ID> ADSP"); */
		for (i = 0; i < NUM_QUPS; i++) {
			if (QUP_TABLE[i].owner == ADSP) {
				snprintf((char *)&buff, sizeof(buff),
					"%i ADSP", i);
				msleep(2000); /* Wait for ADSP to be fully up */
				pacman_run((char *)&buff);
			}
		}

		PACMAN_ADSP_IN_SSR = 0;
	}
}

static void pacman_qmi_client_notify_exit_worker(struct work_struct *w_s)
{
	int i;
	char buff[12];

	pr_debug("%s\n", __func__);
	PACMAN_ADSP_IN_SSR = 1;

	/* Inject a SSR Event into PACMan for each registered QUP */
	for (i = 0; i < NUM_QUPS; i++) {
		if (QUP_TABLE[i].owner == ADSP) {
			snprintf((char *)&buff, sizeof(buff), "%i SSR", i);
			pacman_run((char *)&buff);
		}
	}
}

static void pacman_qmi_client_port_notify_worker(struct work_struct *work)
{
	int rc;

	pr_debug("%s\n", __func__);

	do {
		rc = qmi_recv_msg(pacman_qmi_client_port_adsp);
	} while (0 == rc);

	if (rc != -ENOMSG)
		pr_err("%s: Error receiving QMI message\n", __func__);
}

static struct notifier_block pacman_qmi_notifier_block = {
	.notifier_call = pacman_qmi_client_notify_cb,
};

static int pacman_qmi_init(void)
{
	int rc;

	pr_debug("%s\n", __func__);
	pacman_qmi_client_port_adsp = NULL;
	pacman_qmi_notification_workqueue =
		create_singlethread_workqueue("pacman_qmi_notification_wq");
	pacman_qmi_rx_workqueue =
		create_singlethread_workqueue("pacman_qmi_rx_wq");
	if (!pacman_qmi_notification_workqueue || !pacman_qmi_rx_workqueue) {
		pr_err("%s: create_singlethread_workqueue failed\n",
			__func__);
		return -EFAULT;
	}

	/* Register for ADSP PACMan Service */
	rc = qmi_svc_event_notifier_register(PACMAN_QMI_ADSP_SERVICE_ID,
					PACMAN_QMI_ADSP_SERVICE_VERSION,
					PACMAN_QMI_ADSP_SERVICE_INSTANCE_ID,
					&pacman_qmi_notifier_block);
	if (rc < 0) {
		pr_err("%s: qmi_svc_event_notifier_register failed rc=%i\n",
				__func__, rc);
		destroy_workqueue(pacman_qmi_notification_workqueue);
		destroy_workqueue(pacman_qmi_rx_workqueue);
		return rc;
	}

	return 0;
}

static void pacman_qmi_deinit(void)
{
	pr_debug("%s\n", __func__);

	/* Unregister for ADSP PACMan Service */
	qmi_handle_destroy(pacman_qmi_client_port_adsp);
	pacman_qmi_client_port_adsp = NULL;
	qmi_svc_event_notifier_unregister(PACMAN_QMI_ADSP_SERVICE_ID,
					PACMAN_QMI_ADSP_SERVICE_VERSION,
					PACMAN_QMI_ADSP_SERVICE_INSTANCE_ID,
					&pacman_qmi_notifier_block);

	if (pacman_qmi_notification_workqueue) {
		flush_workqueue(pacman_qmi_notification_workqueue);
		destroy_workqueue(pacman_qmi_notification_workqueue);
	}
	if (pacman_qmi_rx_workqueue) {
		flush_workqueue(pacman_qmi_rx_workqueue);
		destroy_workqueue(pacman_qmi_rx_workqueue);
	}
}

#ifdef DEBUG_BYPASS_QMI
static int pacman_qmi_send_sync_ready_msg(struct qmi_handle *qmi_port_handle,
						int qup_id)
{
	return 0;
}
#else
static int pacman_qmi_send_sync_ready_msg(struct qmi_handle *qmi_port_handle,
						int qup_id)
{
	struct qupm_ready_req_msg_v01 req_msg;
	struct qupm_ready_resp_msg_v01 resp_msg;
	struct msg_desc req_desc, resp_desc;
	int rc;

	pr_debug("%s\n", __func__);

	if (NULL == qmi_port_handle || qup_id < 0 || qup_id >= NUM_QUPS) {
		pr_err("%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	req_desc.max_msg_len = QUPM_READY_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_QUPM_READY_REQ_V01;
	req_desc.ei_array = qupm_ready_req_msg_v01_ei;
	req_msg.qup_id = qup_id;
	req_msg.flags_valid = 0;
	req_msg.flags = 0;

	resp_desc.max_msg_len = QUPM_READY_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_QUPM_READY_RESP_V01;
	resp_desc.ei_array = qupm_ready_resp_msg_v01_ei;

	rc = qmi_send_req_wait(qmi_port_handle, &req_desc,
				&req_msg, sizeof(req_msg),
				&resp_desc, &resp_msg, sizeof(resp_msg),
				PACMAN_QMI_TIMEOUT_MS);
	if (rc < 0) {
		pr_err("%s: qmi_send_req_wait failed rc=%i\n", __func__, rc);
		goto error;
	}

	pr_debug("%s: QMI Response result=%hu error=%hu\n",
			__func__, resp_msg.resp.result, resp_msg.resp.error);

	return (uint16_t)resp_msg.resp.result;

error:
	return rc;
}
#endif

#ifdef DEBUG_BYPASS_QMI
static int pacman_qmi_send_sync_take_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id)
{
	return 0;
}
#else
static int pacman_qmi_send_sync_take_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id)
{
	struct qupm_take_ownership_req_msg_v01 req_msg;
	struct qupm_take_ownership_resp_msg_v01 resp_msg;
	struct msg_desc req_desc, resp_desc;
	int rc;

	pr_debug("%s\n", __func__);

	if (NULL == qmi_port_handle || qup_id < 0 || qup_id >= NUM_QUPS) {
		pr_err("%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	req_desc.max_msg_len = QUPM_TAKE_OWNERSHIP_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_QUPM_TAKE_OWNERSHIP_REQ_V01;
	req_desc.ei_array = qupm_take_ownership_req_msg_v01_ei;
	req_msg.qup_id = qup_id;
	req_msg.flags_valid = 0;
	req_msg.flags = 0;

	resp_desc.max_msg_len = QUPM_TAKE_OWNERSHIP_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_QUPM_TAKE_OWNERSHIP_RESP_V01;
	resp_desc.ei_array = qupm_take_ownership_resp_msg_v01_ei;

	rc = qmi_send_req_wait(qmi_port_handle, &req_desc,
				&req_msg, sizeof(req_msg),
				&resp_desc, &resp_msg, sizeof(resp_msg),
				PACMAN_QMI_TIMEOUT_MS);
	if (rc < 0) {
		pr_err("%s: qmi_send_req_wait failed rc=%i\n", __func__, rc);
		goto error;
	}

	pr_debug("%s: QMI Response result=%hu error=%hu\n",
			__func__, resp_msg.resp.result, resp_msg.resp.error);

	return (uint16_t)resp_msg.resp.result;

error:
	return rc;
}
#endif


#ifdef DEBUG_BYPASS_QMI
static int pacman_qmi_send_sync_give_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id)
{
	return 0;
}
#else
static int pacman_qmi_send_sync_give_ownership_msg(
					struct qmi_handle *qmi_port_handle,
					int qup_id)
{
	struct qupm_give_ownership_req_msg_v01 req_msg;
	struct qupm_give_ownership_resp_msg_v01 resp_msg;
	struct msg_desc req_desc, resp_desc;
	int rc;

	pr_debug("%s\n", __func__);

	if (NULL == qmi_port_handle || qup_id < 0 || qup_id >= NUM_QUPS) {
		pr_err("%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	req_desc.max_msg_len = QUPM_GIVE_OWNERSHIP_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_QUPM_GIVE_OWNERSHIP_REQ_V01;
	req_desc.ei_array = qupm_give_ownership_req_msg_v01_ei;
	req_msg.qup_id = qup_id;
	req_msg.flags_valid = 0;
	req_msg.flags = 0;

	resp_desc.max_msg_len = QUPM_GIVE_OWNERSHIP_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_QUPM_GIVE_OWNERSHIP_RESP_V01;
	resp_desc.ei_array = qupm_give_ownership_resp_msg_v01_ei;

	rc = qmi_send_req_wait(qmi_port_handle, &req_desc,
				&req_msg, sizeof(req_msg),
				&resp_desc, &resp_msg, sizeof(resp_msg),
				PACMAN_QMI_TIMEOUT_MS);
	if (rc < 0) {
		pr_err("%s: qmi_send_req_wait failed rc=%i\n", __func__, rc);
		goto error;
	}

	pr_debug("%s: QMI Response result=%hu error=%hu\n",
			__func__, resp_msg.resp.result, resp_msg.resp.error);

	return (uint16_t)resp_msg.resp.result;

error:
	return rc;
}
#endif

/******************************************************************************
* State Machine States
******************************************************************************/
static void pacman_state_init(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	pr_debug("%s: input %u\n", __func__, input);

	/* Verify expected events while in state */
	if (input != INIT || NULL == this->curr_state) {
		pr_err("%s: ERROR with input %u\n", __func__, input);
		return;
	}

	/* For PACMan running on APSS, we want the default
	 * state to be enabled
	 */
	if (input == INIT) {
		this->next_state = pacman_state_enabled;
		return;
	}

	this->next_state = pacman_state_disabled;
}

static void pacman_state_disabled(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	pr_debug("%s: input %u\n", __func__, input);

	/* Verify expected events while in state */
	if (input != INIT && input != DISABLE && input != ENABLE &&
		input != SSR) {
		pr_err("%s: ERROR with input %u\n", __func__, input);
		return;
	}

	/* Check what state we transitioned from */
	if (this->curr_state == pacman_state_enabling) {
		pr_err("%s: pacman_state_enabling failed to execute\n",
					__func__);
		return;
	}

	if (input == ENABLE || input == SSR)
		this->next_state = pacman_state_enabling;
}

static void pacman_state_enabled(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	pr_debug("%s: input %u\n", __func__, input);

	/* Verify expected events while in state */
	if (input != INIT && input != DISABLE && input != ENABLE &&
		input != SSR) {
		pr_err("%s: ERROR with input %u\n", __func__, input);
		return;
	}

	/* Check what state we transitioned from */
	if (this->curr_state == pacman_state_disabling) {
		pr_err("%s: pacman_state_disabling failed to execute\n",
				__func__);
		return;
	}

	if (input == DISABLE)
		this->next_state = pacman_state_disabling;
}

static void pacman_state_enabling(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	int rc;
	struct qup_instance	*qup_instance = (struct qup_instance *)arg;

	pr_debug("%s: input %u\n", __func__, input);

	/* Verify expected events while in state */
	if (input != ENABLE && input != SSR) {
		pr_err("%s: ERROR with input %u\n", __func__, input);
		goto general_error;
	}

	/* Verify inputs */
	if (NULL == qup_instance) {
		pr_err("%s: ERROR QUP configuration invalid\n",
				__func__);
		goto general_error;
	}

	pr_debug("%s: QUP ID=%i Subsystem=%s\n", __func__,
		qup_instance->id,
		subsystem_strings[qup_instance->next_owner]);

	/* Do not attempt to communicate with ADSP if we have a SSR event */
	if (input == SSR)
		goto ssr_event;

	/* APSS should be the requested owner */
	if (qup_instance->next_owner != APSS) {
		pr_err("%s: ERROR support for APSS Only\n", __func__);
		goto general_error;
	}
	/* Check if ADSP is OK to release ownership */
	rc = pacman_qmi_send_sync_ready_msg(pacman_qmi_client_port_adsp,
					qup_instance->id);
	if (rc) {
		pr_err("%s: ERROR pacman_qmi_send_sync_ready_msg %i\n",
				__func__, rc);
		goto general_error;
	}
	/* APSS needs to be given ownership */
	rc = pacman_qmi_send_sync_give_ownership_msg(
						pacman_qmi_client_port_adsp,
						qup_instance->id);
	if (rc) {
		pr_err("%s: ERROR send_sync_give_ownership_msg %i\n",
				__func__, rc);
		goto general_error;
	}
ssr_event:
	/* Call into TrustZone */
	rc = tz_configure_blsp_ownership(qup_instance->id, APSS);
	if (rc) {
		pr_err("%s: ERROR calling into TZ %i\n", __func__, rc);
		goto ownership_cleanup;
	}
	/* Enable the bus and all its peripherals by binding the bus driver */
#ifndef DEBUG_BYPASS_BINDING
	rc = driver_attach(qup_instance->bus_drv);
#endif
	if (rc) {
		pr_err("%s: ERROR calling driver_enable %i\n",
				__func__, rc);
		goto tz_cleanup;
	}
	/* State transition successful */
	this->next_state = pacman_state_enabled;
	return;

	/* APSS is master, so the following is a true critical error */
tz_cleanup:
ownership_cleanup:
general_error:
	pr_err("%s: CRITICAL ERROR\n", __func__);
	return;
}

static void pacman_state_disabling(state_machine_t *this,
			state_machine_input_t input, void *arg)
{
	int rc;
	struct qup_instance	*qup_instance = (struct qup_instance *)arg;

	pr_debug("%s: input %u\n", __func__, input);

	/* Verify expected events while in state */
	if (input != DISABLE) {
		pr_err("%s: ERROR with input %u\n", __func__, input);
		goto general_error;
	}

	/* Verify inputs */
	if (NULL == qup_instance) {
		pr_err("%s: ERROR - QUP configuration invalid\n",
				__func__);
		goto general_error;
	}

	pr_debug("%s: QUP ID=%i Subsystem=%s\n", __func__,
				qup_instance->id,
				subsystem_strings[qup_instance->next_owner]);

	/* ADSP should be the owner */
	if (qup_instance->next_owner != ADSP) {
		pr_err("%s: ERROR Support for ADSP Only\n", __func__);
		goto general_error;
	}
	/* Check if ADSP is OK to take ownership */
	rc = pacman_qmi_send_sync_ready_msg(pacman_qmi_client_port_adsp,
						qup_instance->id);
	if (rc) {
		pr_err("%s: ERROR pacman_qmi_send_sync_ready_msg %i\n",
				__func__, rc);
		goto general_error;
	}
	/* Save the current bus device->device_driver for restoring later */
	qup_instance->bus_drv = qup_instance->bus_dev->driver;
	/* Disable the bus and peripherals by unbindng the bus */
#ifndef DEBUG_BYPASS_BINDING
	pr_debug("%s: Unbinding the bus\n", __func__);
	device_release_driver(qup_instance->bus_dev);
	/* Unfortunately no return code to check here for a successful unbind */
	/* If a peripheral does not unbind successfully, it may hang here */
	pr_debug("%s: Unbinding the bus complete\n", __func__);
#endif
	/* Call into TrustZone */
	rc = tz_configure_blsp_ownership(qup_instance->id,
					qup_instance->next_owner);
	if (rc) {
		pr_err("%s: ERROR calling into TZ %i\n", __func__, rc);
		goto driver_disable_cleanup;
	}
	/* Transfer ownership to subsystem */
	rc = pacman_qmi_send_sync_take_ownership_msg(
						pacman_qmi_client_port_adsp,
						qup_instance->id);
	if (rc) {
		pr_err("%s: ERROR send_sync_take_ownership_msg %i\n",
				__func__, rc);
		goto tz_cleanup;
	}
	/* State transition successful */
	this->next_state = pacman_state_disabled;
	return;

tz_cleanup:
	rc = tz_configure_blsp_ownership(qup_instance->id, APSS);
	if (rc)
		pr_err("%s: CRITICAL ERROR calling into TZ %i\n",
				__func__, rc);
driver_disable_cleanup:
#ifndef DEBUG_BYPASS_BINDING
	rc = driver_attach(qup_instance->bus_drv);
	if (rc)
		pr_err("%s: CRITICAL ERROR driver_enable %i\n",
				__func__, rc);
#endif
general_error:
	this->next_state = pacman_state_enabled;
	return;
}

/******************************************************************************
* Linux Driver
******************************************************************************/
static int __init pacman_init(void);
static void __exit pacman_exit(void);
static int pacman_probe(struct platform_device *pdev);
static int pacman_remove(struct platform_device *pdev);
static int pacman_open(struct inode *ip, struct file *fp);
static int pacman_close(struct inode *ip, struct file *fp);
static ssize_t pacman_read(struct file *fp, char __user *buffer,
				size_t length, loff_t *offset);
static ssize_t pacman_write(struct file *fp, const char *buffer,
				size_t length, loff_t *offset);

static int pacman_open(struct inode *ip, struct file *fp)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int pacman_close(struct inode *ip, struct file *fp)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static ssize_t pacman_read(struct file *fp, char __user *buffer,
				size_t length, loff_t *offset)
{
	char my_buff[256];
	int rc;

	pr_debug("%s\n", __func__);
	if (*offset == 0) {
		*offset = 1;
		pacman_dump((char *)&my_buff, sizeof(my_buff));
		if (strlen(my_buff) < length) {
			rc = copy_to_user(buffer, my_buff, strlen(my_buff));
			pr_debug("%s:\n%s\n", __func__, (char *)&my_buff);
		}
		return strlen(my_buff);
	} else {
		return 0;
	}
}

static ssize_t pacman_write(struct file *fp, const char *buffer,
				size_t length, loff_t *offset)
{
	char my_buff[64];
	int rc;

	pr_debug("%s\n", __func__);
	rc = copy_from_user(my_buff, buffer, min(length, sizeof(my_buff)));
	my_buff[63] = '\0';
	pacman_run((char *)&my_buff);
	return length;
}

const struct file_operations pacman_fops = {
	.owner = THIS_MODULE,
	.open = pacman_open,
	.release = pacman_close,
	.read = pacman_read,
	.write = pacman_write
};

static int pacman_probe(struct platform_device *pdev)
{
	pr_info("%s: %s version %s\n", __func__, DEVICE_NAME, PACMAN_VERSION);

	pacman_ctl.dev_num = register_chrdev(0, DEVICE_NAME, &pacman_fops);
	if (pacman_ctl.dev_num < 0) {
		pr_err("%s: register_chrdev failed\n", __func__);
		goto register_chrdev_err;
	}

	pacman_ctl.dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(pacman_ctl.dev_class)) {
		pr_err("%s: class_create failed\n", __func__);
		goto class_create_err;
	}

	pacman_ctl.dev = device_create(pacman_ctl.dev_class, NULL,
					MKDEV(pacman_ctl.dev_num, 0),
					&pacman_ctl, DEVICE_NAME);
	if (IS_ERR(pacman_ctl.dev)) {
		pr_err("%s: device_create failed\n", __func__);
		goto device_create_err;
	}

	return 0;

device_create_err:
	class_destroy(pacman_ctl.dev_class);
class_create_err:
	unregister_chrdev(pacman_ctl.dev_num, DEVICE_NAME);
register_chrdev_err:
	return -ENODEV;
}

static int pacman_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	device_unregister(pacman_ctl.dev);
	device_destroy(pacman_ctl.dev_class, MKDEV(pacman_ctl.dev_num, 0));
	class_destroy(pacman_ctl.dev_class);
	unregister_chrdev(pacman_ctl.dev_num, DEVICE_NAME);
	return 0;
}

static const struct of_device_id pacman_dt_match[] = {
	{.compatible = "qcom,msm-pacman"},
	{},
};
MODULE_DEVICE_TABLE(of, pacman_dt_match);

static struct platform_driver pacman_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = pacman_dt_match,
	},
	.probe = pacman_probe,
	.remove = pacman_remove,
};

static int __init pacman_init(void)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	rc = platform_driver_register(&pacman_driver);
	if (rc) {
		pr_err("%s: ERROR Failed to register driver\n", __func__);
		return rc;
	}
	pacman_framework_init();
	rc = pacman_qmi_init();
	if (rc) {
		pr_err("%s: ERROR pacman_qmi_init failed\n", __func__);
		return rc;
	}

	return 0;
}

static void __exit pacman_exit(void)
{
	pr_info("%s\n", __func__);
	pacman_qmi_deinit();
	platform_driver_unregister(&pacman_driver);
}

MODULE_DESCRIPTION("Peripheral Access Control Manager (PACMan)");
MODULE_LICENSE("GPL v2");
module_init(pacman_init);
module_exit(pacman_exit);
