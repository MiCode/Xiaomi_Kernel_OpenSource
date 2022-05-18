// SPDX-License-Identifier: GPL-2.0
/*
 * mddp_dev.c - MDDP device node API.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_if.h"
#include "mddp_sm.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#define MDDP_DEV_NAME                   "mddp"

struct mddp_dev_rb_t {
	struct mddp_dev_rb_t   *next;
	struct mddp_dev_rb_t   *prev;

	uint32_t                rb_len;
	void                   *rb_data;
};

struct mddp_dev_rb_head_t {
	struct mddp_dev_rb_t           *next;
	struct mddp_dev_rb_t           *prev;

	uint32_t                        cnt;
	spinlock_t                      locker;
	wait_queue_head_t               read_wq;
};

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------
#define MDDP_DEV_CLONE_COMM_HDR(_rsp, _req, _status) \
	do { \
		(_rsp)->mcode = (_req)->mcode; \
		(_rsp)->status = _status; \
		(_rsp)->app_type = (_req)->app_type; \
		(_rsp)->msg = (_req)->msg; \
	} while (0)

#define MDDP_SET_BUF_TERMIN(_buf, _len) \
	do { \
		_len = strlen(_buf); \
		if (_len > 1 && _buf[_len-1] == '\n') \
			_buf[_len-1] = '\0'; \
	} while (0)

#define MDDP_DSTATE_IS_VALID_ID(_id) (_id < MDDP_DSTATE_ID_NUM)
#define MDDP_DSTATE_IS_ACTIVATED() (mddp_dstate_activated_s)

//------------------------------------------------------------------------------
// Private prototype.
// -----------------------------------------------------------------------------
static int32_t mddp_dev_open(struct inode *inode,
	struct file *file);
static int32_t mddp_dev_close(struct inode *inode,
	struct file *file);
static ssize_t mddp_dev_read(struct file *file,
	char *buf, size_t count, loff_t *ppos);
static ssize_t mddp_dev_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
static long mddp_dev_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static long mddp_dev_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg);
#endif
static unsigned int mddp_dev_poll(struct file *fp,
	struct poll_table_struct *poll);

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static const struct file_operations mddp_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = &mddp_dev_open,
	.read           = &mddp_dev_read,
	.write          = &mddp_dev_write,
	.release        = &mddp_dev_close,
	.unlocked_ioctl = &mddp_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = &mddp_dev_compat_ioctl,
#endif
	.poll           = &mddp_dev_poll,
};

static atomic_t mddp_dev_open_ref_cnt_s;
static struct mddp_dev_rb_head_t mddp_hidl_rb_head_s;
static struct mddp_dev_rb_head_t mddp_dstate_rb_head_s;

#define MDDP_CMCMD_RSP_CNT (MDDP_CMCMD_RSP_END - MDDP_CMCMD_RSP_BEGIN)
static enum mddp_dev_evt_type_e
mddp_dev_rsp_status_mapping_s[MDDP_CMCMD_RSP_CNT][2] =  {
/* FAIL                             SUCCESS */
{MDDP_DEV_EVT_STOPPED_ERROR,        MDDP_DEV_EVT_SUPPORT_AVAILABLE},//ENABLE
{MDDP_DEV_EVT_NONE,                 MDDP_DEV_EVT_NONE},//DISABLE
{MDDP_DEV_EVT_STOPPED_ERROR,        MDDP_DEV_EVT_STARTED},//ACT
{MDDP_DEV_EVT_STOPPED_UNSUPPORTED,  MDDP_DEV_EVT_STOPPED_UNSUPPORTED},//DEACT
{MDDP_DEV_EVT_STOPPED_LIMIT_REACHED, MDDP_DEV_EVT_STOPPED_LIMIT_REACHED},//LIMIT
{MDDP_DEV_EVT_CONNECT_UPDATE,       MDDP_DEV_EVT_CONNECT_UPDATE},//CT_IND
{MDDP_DEV_EVT_WARNING_REACHED,      MDDP_DEV_EVT_WARNING_REACHED},
};
#undef MDDP_CMCMD_RSP_CNT

uint32_t mddp_debug_log_class_s = MDDP_LC_ALL;
uint32_t mddp_debug_log_level_s = MDDP_LL_DEFAULT;
static bool mddp_dstate_activated_s;

//------------------------------------------------------------------------------
// Function Prototype.
// -----------------------------------------------------------------------------
static struct mddp_dev_rb_t *mddp_query_dstate(
		struct mddp_dev_rb_head_t *list, uint32_t seq);
static struct mddp_dev_rb_t *mddp_dequeue_dstate(
		struct mddp_dev_rb_head_t *list);
static void mddp_clear_dstate(struct mddp_dev_rb_head_t *list);

//------------------------------------------------------------------------------
// Sysfs APIs
//------------------------------------------------------------------------------
static ssize_t
debug_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
			"debug_log_class(%x), debug_log_level(%x)\n",
			mddp_debug_log_class_s, mddp_debug_log_level_s);
}

static ssize_t
debug_log_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	uint32_t                lv;
	uint32_t                class;
	unsigned long           value;

	if (!kstrtoul(buf, 0, &value)) {
		class = (value & MDDP_DEBUG_LOG_CLASS_MASK) >> 4;
		if (MDDP_IS_VALID_LOG_CLASS(class))
			mddp_debug_log_class_s = class;

		lv = value & MDDP_DEBUG_LOG_LV_MASK;
		if (MDDP_IS_VALID_LOG_LEVEL(lv))
			mddp_debug_log_level_s = lv;
	}
	return count;
}
static DEVICE_ATTR_RW(debug_log);

static ssize_t
version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "MDDP version(%d)\n",
				 __MDDP_VERSION__);
}
static DEVICE_ATTR_RO(version);

static ssize_t
state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t          *app;
	uint32_t                    type;
	uint8_t                     idx;
	uint32_t                    ret_num = 0;
	uint32_t                    seq = 0;
	struct mddp_dev_rb_head_t  *list = &mddp_dstate_rb_head_s;
	struct mddp_dev_rb_t       *entry;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);
		ret_num += scnprintf(buf + ret_num, PAGE_SIZE - ret_num,
					"type(%d), state(%d)\n",
					app->type, app->state);
		ret_num += scnprintf(buf + ret_num, PAGE_SIZE - ret_num,
				"drv_reg(%d), feature(%d)\n",
				app->drv_reg, atomic_read(&app->feature));
		ret_num += scnprintf(buf + ret_num, PAGE_SIZE - ret_num,
				"abnormal(%x), reset_cnt(%d)\n",
				app->abnormal_flags, app->reset_cnt);

		// NG. Failed to fill-in data!
		if (ret_num <= 0)
			return scnprintf(buf, PAGE_SIZE,
				"%s: Failed to fill-in data!\n", __func__);
	}

	/*
	 * Detailed state.
	 */
	entry = mddp_query_dstate(list, seq);
	while (entry) {
		ret_num += scnprintf(buf + ret_num, PAGE_SIZE - ret_num,
				"%s\n",
				((struct mddp_dstate_t *)entry->rb_data)->str);

		seq += 1;
		entry = mddp_query_dstate(list, seq);
	}

	// OK.
	return ret_num;
}

static ssize_t
state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	unsigned long           value;

	if (!kstrtoul(buf, 0, &value)) {
		if (value == MDDP_DETAILED_STATE_ENABLE) {
			mddp_dstate_activated_s = true;
			mddp_enqueue_dstate(MDDP_DSTATE_ID_START);
		} else  if (value == MDDP_DETAILED_STATE_DISABLE) {
			mddp_enqueue_dstate(MDDP_DSTATE_ID_STOP);
			mddp_dstate_activated_s = false;
		}
	}

	return count;
}
static DEVICE_ATTR_RW(state);

static ssize_t
wh_statistic_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback)
		return app->sysfs_callback(app,
				MDDP_SYSFS_CMD_STATISTIC_READ, buf, 0);

	return scnprintf(buf, PAGE_SIZE,
				"Cannot change WH mode, mddp-wh config(%d)\n",
				app->is_config);
}
static DEVICE_ATTR_RO(wh_statistic);

static ssize_t
wh_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback)
		return app->sysfs_callback(app,
				MDDP_SYSFS_CMD_ENABLE_READ, buf, 0);

	return scnprintf(buf, PAGE_SIZE,
				"Cannot change WH mode, mddp-wh config(%d)\n",
				app->is_config);
}

static ssize_t
wh_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback) {
		// OK.
		app->sysfs_callback(app, MDDP_SYSFS_CMD_ENABLE_WRITE,
				    (char *)buf, count);
		return count;
	}

	// NG. Failed to configure!
	return count;
}
static DEVICE_ATTR_RW(wh_enable);

#ifdef MDDP_EM_SUPPORT
#define EM_CMD_BUF_SZ 32
static uint8_t em_cmd_buf[EM_CMD_BUF_SZ];
static int32_t em_cmd_app = -1;
static int32_t em_cmd_status;

#define EM_CMD_RESET() (em_cmd_app = -1)

static ssize_t
em_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "staus:%d, cmd_buf:%s\n",
			em_cmd_status, em_cmd_buf);
}

static ssize_t
em_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct mddp_app_t      *app;
	const char             *delim = " ";
	char                   *token;
	char                   *strsep_buf_p;
	unsigned int            str_len;

	str_len = strlen(buf);

	snprintf(em_cmd_buf, EM_CMD_BUF_SZ, "%.*s",
			(int)min(count, sizeof(em_cmd_buf) - 1), buf);
	strsep_buf_p = em_cmd_buf;
	MDDP_SET_BUF_TERMIN(em_cmd_buf, str_len);

	token = strsep(&strsep_buf_p, delim);
	if (token == NULL) {
		em_cmd_status = -EINVAL;
		goto input_param_error;
	}

	if (kstrtoint(token, 10, &em_cmd_app))
		return -EINVAL;

	if (em_cmd_app != MDDP_APP_TYPE_WH) {
		em_cmd_status = -EPERM;
		goto not_support_error;
	}

	app = mddp_get_app_inst(em_cmd_app);
	if (app->sysfs_callback) {
		// OK.
		snprintf(em_cmd_buf, EM_CMD_BUF_SZ, "%.*s",
				(int)min(count, sizeof(em_cmd_buf) - 1), buf);
		MDDP_SET_BUF_TERMIN(em_cmd_buf, str_len);
		em_cmd_status = app->sysfs_callback(app,
						MDDP_SYSFS_EM_CMD_TEST_WRITE,
						em_cmd_buf, strlen(em_cmd_buf));
		return count;
	}

	// NG. Failed to configure!
	em_cmd_status = -ERANGE;
	snprintf(em_cmd_buf, EM_CMD_BUF_SZ, "%.*s",
			(int)min(count, sizeof(em_cmd_buf) - 1), buf);
	return count;

input_param_error:
not_support_error:
	EM_CMD_RESET();
	snprintf(em_cmd_buf, EM_CMD_BUF_SZ, "%.*s",
			(int)min(count, sizeof(em_cmd_buf) - 1), buf);
	return count;
}
static DEVICE_ATTR_RW(em_test);
#endif /* MDDP_EM_SUPPORT */

static struct attribute *mddp_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_state.attr,
	&dev_attr_wh_statistic.attr,
	&dev_attr_debug_log.attr,

	&dev_attr_wh_enable.attr,
#ifdef MDDP_EM_SUPPORT
	&dev_attr_em_test.attr,
#endif

	NULL,
};
ATTRIBUTE_GROUPS(mddp);

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static inline void __mddp_dev_insert(struct mddp_dev_rb_t *new,
		struct mddp_dev_rb_t *prev,
		struct mddp_dev_rb_t *next,
		struct mddp_dev_rb_head_t *list)
{
	new->next = next;
	new->prev = prev;
	next->prev = prev->next = new;
	list->cnt++;
}

static inline void __mddp_dev_rb_unlink(struct mddp_dev_rb_t *entry,
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t           *next;
	struct mddp_dev_rb_t           *prev;

	list->cnt--;
	next = entry->next;
	prev = entry->prev;
	entry->next = entry->prev = NULL;
	next->prev = prev;
	prev->next = next;
}

static void mddp_dev_rb_enqueue_tail(struct mddp_dev_rb_head_t *list,
		struct mddp_dev_rb_t *new)
{
	unsigned long flags;

	spin_lock(&list->locker);
	__mddp_dev_insert(new, list->prev, (struct mddp_dev_rb_t *) list, list);
	spin_unlock(&list->locker);

	spin_lock_irqsave(&list->read_wq.lock, flags);
	wake_up_all_locked(&list->read_wq);
	spin_unlock_irqrestore(&list->read_wq.lock, flags);
}

static struct mddp_dev_rb_t *mddp_dev_rb_peek(
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t *entry;

	entry = list->next;

	if (entry == (struct mddp_dev_rb_t *)list)
		entry = NULL;

	return entry;
}

static struct mddp_dev_rb_t *mddp_dev_rb_query(
		struct mddp_dev_rb_head_t *list, uint32_t seq)
{
	struct mddp_dev_rb_t     *entry = NULL;
	uint32_t                  cnt = 0;

	spin_lock(&list->locker);

	entry = mddp_dev_rb_peek(list);
	while (entry) {
		if (seq == cnt)
			break;

		entry = entry->next;
		if (entry == (struct mddp_dev_rb_t *)list) {
			entry = NULL;
			break;
		}

		cnt += 1;
	}

	spin_unlock(&list->locker);

	return entry;
}

static struct mddp_dev_rb_t *mddp_dev_rb_dequeue(
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t     *entry = NULL;

	spin_lock(&list->locker);

	entry = mddp_dev_rb_peek(list);
	if (entry)
		__mddp_dev_rb_unlink(entry, list);

	spin_unlock(&list->locker);

	return entry;
}

static bool mddp_dev_rb_queue_empty(struct mddp_dev_rb_head_t *list)
{
	return list->next == (struct mddp_dev_rb_t *)list;
}

static struct mddp_dev_rb_t *mddp_query_dstate(
		struct mddp_dev_rb_head_t *list, uint32_t seq)
{
	return mddp_dev_rb_query(list, seq);
}

static struct mddp_dev_rb_t *mddp_dequeue_dstate(
		struct mddp_dev_rb_head_t *list)
{
	return mddp_dev_rb_dequeue(list);
}

static void mddp_clear_dstate(
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t           *entry;

	entry = mddp_dequeue_dstate(list);
	while (entry) {
		kfree(entry->rb_data);
		kfree(entry);

		entry = mddp_dequeue_dstate(list);
	}
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
void mddp_dev_list_init(struct mddp_dev_rb_head_t *list)
{
	spin_lock_init(&list->locker);
	list->cnt = 0;
	list->prev = list->next = (struct mddp_dev_rb_t *)list;

	init_waitqueue_head(&list->read_wq);
}

struct miscdevice mddp_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MDDP_DEV_NAME,
	.fops = &mddp_dev_fops,
	.groups = mddp_groups,
};


int32_t mddp_dev_init(void)
{
	atomic_set(&mddp_dev_open_ref_cnt_s, 0);

	/*
	 * Ring buffer init.
	 */
	mddp_dev_list_init(&mddp_hidl_rb_head_s);
	mddp_dev_list_init(&mddp_dstate_rb_head_s);

	/*
	 * Create device node.
	 */
	if (misc_register(&mddp_dev) < 0)
		return -1;

	/*
	 * Detailed state init.
	 */
	mddp_dstate_activated_s = false;

	return 0;
}

void mddp_dev_uninit(void)
{
	/*
	 * Release CHAR device node.
	 */
	misc_deregister(&mddp_dev);

	mddp_clear_dstate(&mddp_hidl_rb_head_s);
	mddp_clear_dstate(&mddp_dstate_rb_head_s);
}

void mddp_dev_response(enum mddp_app_type_e type,
		enum mddp_ctrl_msg_e msg, bool is_success,
		uint8_t *data, uint32_t data_len)
{
	struct mddp_dev_rb_head_t      *list = &mddp_hidl_rb_head_s;
	struct mddp_dev_rb_t           *entry;
	struct mddp_dev_rsp_common_t   *dev_rsp;
	uint16_t                        status;
	uint32_t                        rsp_idx;

	if (msg < MDDP_CMCMD_RSP_BEGIN || msg >= MDDP_CMCMD_RSP_END) {
		MDDP_C_LOG(MDDP_LL_NOTICE,
				"%s: invalid rsp msg(%d) in type(%d)!\n",
				__func__, msg, type);
		return;
	}

	rsp_idx = (msg - MDDP_CMCMD_RSP_BEGIN);
	status = mddp_dev_rsp_status_mapping_s[rsp_idx][is_success];

	if (status == MDDP_DEV_EVT_NONE) {
		// No response to upper module.
		MDDP_C_LOG(MDDP_LL_NOTICE,
				"%s: No RSP, type(%d), msg(%d), is_success(%d).\n",
				__func__, type, msg, is_success);
		return;
	}

	dev_rsp = kmalloc(sizeof(struct mddp_dev_rsp_common_t) + data_len,
				GFP_ATOMIC);
	if (unlikely(!dev_rsp))
		return;

	dev_rsp->mcode = MDDP_CTRL_MSG_MCODE;
	dev_rsp->status = status;
	dev_rsp->app_type = type;
	dev_rsp->msg = msg;
	dev_rsp->data_len = data_len;
	if (data_len > 0)
		memcpy(dev_rsp->data, data, data_len);

	entry = kmalloc(sizeof(struct mddp_dev_rb_t), GFP_ATOMIC);
	if (unlikely(!entry)) {
		kfree(dev_rsp);
		return;
	}

	entry->rb_len = sizeof(struct mddp_dev_rsp_common_t) + data_len;
	entry->rb_data = dev_rsp;
	mddp_dev_rb_enqueue_tail(list, entry);
}

#define MDDP_CURR_TIME_STR_SZ 32
void mddp_enqueue_dstate(enum mddp_dstate_id_e id, ...)
{
	struct mddp_dev_rb_head_t  *list = &mddp_dstate_rb_head_s;
	struct mddp_dev_rb_t       *entry;
	struct mddp_dstate_t       *dstat;
	struct rtc_time             rt;
	struct timespec64           ts;
	char                        curr_time_str[MDDP_CURR_TIME_STR_SZ];
	va_list                     ap;
	int                         ip;
	int                         port;
	unsigned long long          rx;
	unsigned long long          tx;

	if (!MDDP_DSTATE_IS_VALID_ID(id) || !MDDP_DSTATE_IS_ACTIVATED())
		return;

	dstat = kzalloc(sizeof(struct mddp_dstate_t), GFP_ATOMIC);
	if (unlikely(!dstat))
		return;

	entry = kzalloc(sizeof(struct mddp_dev_rb_t), GFP_ATOMIC);
	if (unlikely(!entry)) {
		kfree(dstat);
		return;
	}

	// Generate current time string.
	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &rt);
	snprintf(curr_time_str, MDDP_CURR_TIME_STR_SZ,
			"%d%02d%02d %02d:%02d:%02d.%09ld UTC",
			rt.tm_year + 1900, rt.tm_mon + 1, rt.tm_mday,
			rt.tm_hour, rt.tm_min, rt.tm_sec, ts.tv_nsec);

	// Generate detailed state message.
	dstat->id = id;
	va_start(ap, id);
	switch (id) {
	case MDDP_DSTATE_ID_START:
		mddp_clear_dstate(list);
		snprintf(dstat->str, MDDP_DSTATE_STR_SZ,
				mddp_dstate_temp_s[id].str, curr_time_str);
		break;

	case MDDP_DSTATE_ID_STOP:
	case MDDP_DSTATE_ID_SUSPEND_TAG:
	case MDDP_DSTATE_ID_RESUME_TAG:
		snprintf(dstat->str, MDDP_DSTATE_STR_SZ,
				mddp_dstate_temp_s[id].str, curr_time_str);
		break;

	case MDDP_DSTATE_ID_NEW_TAG:
		ip = va_arg(ap, int);
		port = va_arg(ap, int);
		snprintf(dstat->str, MDDP_DSTATE_STR_SZ,
				mddp_dstate_temp_s[id].str, curr_time_str,
				ip, port);
		break;

	case MDDP_DSTATE_ID_GET_OFFLOAD_STATS:
		rx = va_arg(ap, unsigned long long);
		tx = va_arg(ap, unsigned long long);
		snprintf(dstat->str, MDDP_DSTATE_STR_SZ,
				mddp_dstate_temp_s[id].str, curr_time_str,
				rx, tx);
		break;

	default:
		break;
	}
	va_end(ap);

	entry->rb_len = sizeof(struct mddp_dstate_t);
	entry->rb_data = dstat;
	mddp_dev_rb_enqueue_tail(list, entry);
}

//------------------------------------------------------------------------------
// Device node functins.
//------------------------------------------------------------------------------
static int32_t mddp_dev_open(struct inode *inode, struct file *file)
{
	MDDP_C_LOG(MDDP_LL_INFO, "%s: IOCTL dev open.\n", __func__);

	if (atomic_read(&mddp_dev_open_ref_cnt_s))
		return -EBUSY;

	atomic_inc(&mddp_dev_open_ref_cnt_s);

	return 0;
}

static int32_t mddp_dev_close(struct inode *inode, struct file *file)
{
	MDDP_C_LOG(MDDP_LL_INFO, "%s: IOCTL dev close.\n", __func__);

	atomic_dec(&mddp_dev_open_ref_cnt_s);

	return 0;
}

static ssize_t mddp_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int32_t                         ret = 0;
	uint32_t                        len = 0;
	struct mddp_dev_rb_head_t      *list = &mddp_hidl_rb_head_s;
	struct mddp_dev_rb_t           *entry;

	/*
	 * READ: MDDP send data to upper module.
	 */
	if (mddp_dev_rb_queue_empty(list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			spin_lock_irq(&list->read_wq.lock);
			ret = wait_event_interruptible_locked_irq(
				list->read_wq,
				!mddp_dev_rb_queue_empty(list));
			spin_unlock_irq(&list->read_wq.lock);

			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	MDDP_C_LOG(MDDP_LL_DEBUG,
			"%s: IOCTL dev read, count(%zu).\n", __func__, count);
	entry = mddp_dev_rb_peek(list);

	if (!entry) {
		len = 0;
		goto exit;
	}
	len = entry->rb_len;

	if (count >= entry->rb_len) {
		if (copy_to_user(buf, entry->rb_data, entry->rb_len)) {
			MDDP_C_LOG(MDDP_LL_WARN, "%s: copy_to_user fail!\n",
					__func__);
			ret = -EFAULT;
		}

		entry = mddp_dev_rb_dequeue(list);
		if (entry == NULL) {
			MDDP_C_LOG(MDDP_LL_WARN,
					"%s: unexpected dequeue fail!\n",
					__func__);
			ret = -EFAULT;
			goto exit;
		}
		kfree(entry->rb_data);
		kfree(entry);
	} else {
		ret = -ENOBUFS;
		goto exit;
	}

exit:
	return ret ? ret : len;
}

static ssize_t mddp_dev_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	/*
	 * Not support WRITE.
	 */
	MDDP_C_LOG(MDDP_LL_DEBUG, "%s: Receive\n", __func__);

	return count;
}

static long mddp_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mddp_dev_req_common_t            dev_req;
	struct mddp_dev_rsp_common_t           *dev_rsp;
	long                                    ret = 0;
	uint32_t                                data_len;
	uint8_t                                 buf[MDDP_MAX_GET_BUF_SZ] = {0};
	uint32_t                                buf_len = MDDP_MAX_GET_BUF_SZ;
	struct mddp_dev_req_set_ct_value_t     *ct_req;

	memset(&dev_req, 0, sizeof(dev_req));
	/*
	 * NG. copy_from_user fail!
	 */
	if (copy_from_user(&dev_req, (void __user *)arg,
			sizeof(struct mddp_dev_req_common_t))) {
		MDDP_C_LOG(MDDP_LL_WARN,
				"%s: copy_from_user failed!\n", __func__);
		ret = -EFAULT;
		goto ioctl_error;
	}

	/*
	 * NG. MCODE check fail!
	 */
	if (dev_req.mcode != MDDP_CTRL_MSG_MCODE) {
		MDDP_C_LOG(MDDP_LL_WARN,
				"%s: MCODE(%d) wrong!\n",
				__func__, dev_req.mcode);
		ret = -EINVAL;
		goto ioctl_error;
	}

	data_len = dev_req.data_len;
	/*
	 * OK. IOCTL command dispatch.
	 */
	switch (dev_req.msg) {
	case MDDP_CMCMD_ENABLE_REQ:
		ret = mddp_on_enable(dev_req.app_type);
		break;

	case MDDP_CMCMD_DISABLE_REQ:
		ret = mddp_on_disable(dev_req.app_type);
		break;

	case MDDP_CMCMD_ACT_REQ:
		if (data_len == sizeof(struct mddp_dev_req_act_t)) {
			struct mddp_dev_req_act_t *from, *to;

			to = (struct mddp_dev_req_act_t *) &buf;
			from = (struct mddp_dev_req_act_t *)
				&(((struct mddp_dev_req_common_t *)arg)->data);
			ret = copy_from_user(to, from, data_len);

			if (ret == 0) {
				/* OK */
				to->ul_dev_name[IFNAMSIZ - 1] = 0;
				to->dl_dev_name[IFNAMSIZ - 1] = 0;
				ret = mddp_on_activate(dev_req.app_type,
						to->ul_dev_name,
						to->dl_dev_name);
				break;
			}
		}
		/* NG */
		MDDP_C_LOG(MDDP_LL_ERR,
				"%s: ACT fail, data_len(%d), ret(%ld)!\n",
				__func__, data_len, ret);

		break;

	case MDDP_CMCMD_DEACT_REQ:
		ret = mddp_on_deactivate(dev_req.app_type);
		break;

	case MDDP_CMCMD_GET_OFFLOAD_STATS_REQ:
		ret = mddp_on_get_offload_stats(dev_req.app_type,
				buf, &buf_len);
		MDDP_C_LOG(MDDP_LL_DEBUG,
				"%s: ret(%ld), type(%d), buf(%p), len(%u)\n",
				__func__, ret, dev_req.app_type, buf, buf_len);
		MDDP_C_LOG(MDDP_LL_NOTICE,
			"%s: get_offload_stats, rx(%llu), tx(%llu).\n",
			__func__,
			((struct mddp_u_data_stats_t *)buf)->total_rx_bytes,
			((struct mddp_u_data_stats_t *)buf)->total_tx_bytes);

		if (!ret) {
			dev_rsp = kmalloc(
				sizeof(struct mddp_dev_rsp_common_t) + buf_len,
				GFP_ATOMIC);

			if (dev_rsp == NULL) {
				ret = -ENOMEM;
				goto ioctl_error;
			}

			MDDP_DEV_CLONE_COMM_HDR(dev_rsp, &dev_req, 0);
			dev_rsp->data_len = buf_len;
			memcpy(dev_rsp->data, &buf, buf_len);
			ret = (copy_to_user((void *)arg, dev_rsp,
				sizeof(struct mddp_dev_rsp_common_t) +
				buf_len))
				? -EFAULT : 0;
			kfree(dev_rsp);

			mddp_enqueue_dstate(MDDP_DSTATE_ID_GET_OFFLOAD_STATS,
				((struct mddp_u_data_stats_t *)buf)->
					total_rx_bytes,
				((struct mddp_u_data_stats_t *)buf)->
					total_tx_bytes);
		}
		break;

	case MDDP_CMCMD_SET_DATA_LIMIT_REQ:
		if (data_len == sizeof(struct mddp_dev_req_set_data_limit_t)) {
			struct mddp_dev_req_set_data_limit_t *from, *to;

			to = (struct mddp_dev_req_set_data_limit_t *) &buf;
			from = (struct mddp_dev_req_set_data_limit_t *)
				&(((struct mddp_dev_req_common_t *)arg)->data);
			ret = copy_from_user(to, from, data_len);

			if (ret == 0) {
				to->ul_dev_name[IFNAMSIZ - 1] = 0;
				ret = mddp_on_set_data_limit(dev_req.app_type, buf, data_len);
			}
		}
		break;

	case MDDP_CMCMD_SET_CT_VALUE_REQ:
		if (data_len !=
			sizeof(struct mddp_dev_req_set_ct_value_t)) {
			MDDP_C_LOG(MDDP_LL_WARN,
					"%s: arg_len(%u) of command(%u) is not expected!\n",
					__func__,
					dev_req.data_len, dev_req.msg);
			ret = -EINVAL;
			break;
		}


		ct_req = (struct mddp_dev_req_set_ct_value_t *)
			&(((struct mddp_dev_req_common_t *)arg)->data);
		buf_len = sizeof(struct mddp_dev_req_set_ct_value_t);
		ret = copy_from_user((char *)&buf, (char *)ct_req, buf_len);

		if (ret == 0)
			ret = mddp_on_set_ct_value(dev_req.app_type,
					buf, buf_len);
		else
			MDDP_C_LOG(MDDP_LL_WARN,
					"%s: failed to copy_from_user, buf_len(%u), ret(%ld)!\n",
					__func__, buf_len, ret);

		break;

	case MDDP_CMCMD_SET_WARNING_AND_DATA_LIMIT_REQ:
		if (data_len == sizeof(struct mddp_dev_req_set_warning_and_data_limit_t)) {
			struct mddp_dev_req_set_warning_and_data_limit_t *from, *to;

			to = (struct mddp_dev_req_set_warning_and_data_limit_t *) &buf;
			from = (struct mddp_dev_req_set_warning_and_data_limit_t *)
				&(((struct mddp_dev_req_common_t *)arg)->data);
			ret = copy_from_user(to, from, data_len);

			if (ret == 0) {
				to->ul_dev_name[IFNAMSIZ - 1] = 0;
				ret = mddp_on_set_warning_and_data_limit(dev_req.app_type, buf,
									 data_len);
			}
		}
		break;

	default:
		MDDP_C_LOG(MDDP_LL_WARN, "%s: Invalid command(%d)!\n",
				__func__, dev_req.msg);
		ret = -EINVAL;
		break;
	}

ioctl_error:
	MDDP_C_LOG(MDDP_LL_INFO,
			"%s: cmd(%d) app_type(%d), ret (%ld).\n",
			__func__, dev_req.msg, dev_req.app_type, ret);
	return ret;
}

#ifdef CONFIG_COMPAT
static long mddp_dev_compat_ioctl(struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	return 0;
}
#endif

static unsigned int mddp_dev_poll(struct file *fp, struct poll_table_struct *poll)
{
	return 0;
}
