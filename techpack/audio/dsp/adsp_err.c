// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <dsp/apr_audio-v2.h>

#ifdef AUDIO_FORCE_RESTART_ADSP_TEMP
#include <soc/qcom/subsystem_restart.h>
#include <linux/version.h>
#endif

/* ERROR STRING */
/* Success. The operation completed with no errors. */
#define ADSP_EOK_STR          "ADSP_EOK"
/* General failure. */
#define ADSP_EFAILED_STR      "ADSP_EFAILED"
/* Bad operation parameter. */
#define ADSP_EBADPARAM_STR    "ADSP_EBADPARAM"
/* Unsupported routine or operation. */
#define ADSP_EUNSUPPORTED_STR "ADSP_EUNSUPPORTED"
/* Unsupported version. */
#define ADSP_EVERSION_STR     "ADSP_EVERSION"
/* Unexpected problem encountered. */
#define ADSP_EUNEXPECTED_STR  "ADSP_EUNEXPECTED"
/* Unhandled problem occurred. */
#define ADSP_EPANIC_STR       "ADSP_EPANIC"
/* Unable to allocate resource. */
#define ADSP_ENORESOURCE_STR  "ADSP_ENORESOURCE"
/* Invalid handle. */
#define ADSP_EHANDLE_STR      "ADSP_EHANDLE"
/* Operation is already processed. */
#define ADSP_EALREADY_STR     "ADSP_EALREADY"
/* Operation is not ready to be processed. */
#define ADSP_ENOTREADY_STR    "ADSP_ENOTREADY"
/* Operation is pending completion. */
#define ADSP_EPENDING_STR     "ADSP_EPENDING"
/* Operation could not be accepted or processed. */
#define ADSP_EBUSY_STR        "ADSP_EBUSY"
/* Operation aborted due to an error. */
#define ADSP_EABORTED_STR     "ADSP_EABORTED"
/* Operation preempted by a higher priority. */
#define ADSP_EPREEMPTED_STR   "ADSP_EPREEMPTED"
/* Operation requests intervention to complete. */
#define ADSP_ECONTINUE_STR    "ADSP_ECONTINUE"
/* Operation requests immediate intervention to complete. */
#define ADSP_EIMMEDIATE_STR   "ADSP_EIMMEDIATE"
/* Operation is not implemented. */
#define ADSP_ENOTIMPL_STR     "ADSP_ENOTIMPL"
/* Operation needs more data or resources. */
#define ADSP_ENEEDMORE_STR    "ADSP_ENEEDMORE"
/* Operation does not have memory. */
#define ADSP_ENOMEMORY_STR    "ADSP_ENOMEMORY"
/* Item does not exist. */
#define ADSP_ENOTEXIST_STR    "ADSP_ENOTEXIST"
/* Unexpected error code. */
#define ADSP_ERR_MAX_STR      "ADSP_ERR_MAX"

#if IS_ENABLED(CONFIG_SND_SOC_QDSP_DEBUG)
static bool adsp_err_panic;

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_adsp_err;

static ssize_t adsp_err_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char cmd;

	if (copy_from_user(&cmd, ubuf, 1))
		return -EFAULT;

	if (cmd == '0')
		adsp_err_panic = false;
	else
		adsp_err_panic = true;

	return cnt;
}

static const struct file_operations adsp_err_debug_ops = {
	.write = adsp_err_debug_write,
};
#endif
#endif

struct adsp_err_code {
	int		lnx_err_code;
	char	*adsp_err_str;
};


static struct adsp_err_code adsp_err_code_info[ADSP_ERR_MAX+1] = {
	{ 0, ADSP_EOK_STR},
	{ -ENOTRECOVERABLE, ADSP_EFAILED_STR},
	{ -EINVAL, ADSP_EBADPARAM_STR},
	{ -EOPNOTSUPP, ADSP_EUNSUPPORTED_STR},
	{ -ENOPROTOOPT, ADSP_EVERSION_STR},
	{ -ENOTRECOVERABLE, ADSP_EUNEXPECTED_STR},
	{ -ENOTRECOVERABLE, ADSP_EPANIC_STR},
	{ -ENOSPC, ADSP_ENORESOURCE_STR},
	{ -EBADR, ADSP_EHANDLE_STR},
	{ -EALREADY, ADSP_EALREADY_STR},
	{ -EPERM, ADSP_ENOTREADY_STR},
	{ -EINPROGRESS, ADSP_EPENDING_STR},
	{ -EBUSY, ADSP_EBUSY_STR},
	{ -ECANCELED, ADSP_EABORTED_STR},
	{ -EAGAIN, ADSP_EPREEMPTED_STR},
	{ -EAGAIN, ADSP_ECONTINUE_STR},
	{ -EAGAIN, ADSP_EIMMEDIATE_STR},
	{ -EAGAIN, ADSP_ENOTIMPL_STR},
	{ -ENODATA, ADSP_ENEEDMORE_STR},
	{ -EADV, ADSP_ERR_MAX_STR},
	{ -ENOMEM, ADSP_ENOMEMORY_STR},
	{ -ENODEV, ADSP_ENOTEXIST_STR},
	{ -EADV, ADSP_ERR_MAX_STR},
};

#if IS_ENABLED(CONFIG_SND_SOC_QDSP_DEBUG)
static inline void adsp_err_check_panic(u32 adsp_error)
{
	if (adsp_err_panic && adsp_error != ADSP_EALREADY)
		panic("%s: encounter adsp_err=0x%x\n", __func__, adsp_error);
}
#else
static inline void adsp_err_check_panic(u32 adsp_error) {}
#endif

#ifdef AUDIO_FORCE_RESTART_ADSP_TEMP
#define ADSP_ERR_LIMITED_COUNT		(15)
#define ADSP_ERR_LIMITED_TIME		(1)
static int err_count = 0;
static long long err_total_count = 0;
static __kernel_time_t last_time = 0;
static void adsp_err_check_restart(u32 adsp_error)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
	struct timespec64 curtime;
#else
	struct timeval curtime;
#endif
	pr_err("%s: DSP returned error adsp_err = 0x%x total = %lld\n", __func__, adsp_error, err_total_count);

	if (adsp_error == ADSP_ENEEDMORE || adsp_error == ADSP_ENOMEMORY) {
		err_count++;
		err_total_count++;
		pr_err("%s: DSP returned error ADSP_ENEEDMORE or ADSP_ENOMEMORY adsp_err=0x%x\n",
			__func__, adsp_error);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
		ktime_get_real_ts64(&curtime);
#else
		do_gettimeofday(&curtime);
#endif
		pr_err("%s: err_count = %d [%lld - %lld = %lld]\n", __func__,
			err_count, curtime.tv_sec, last_time, curtime.tv_sec - last_time);

		if ((err_count >= ADSP_ERR_LIMITED_COUNT) &&
					(curtime.tv_sec - last_time <= ADSP_ERR_LIMITED_TIME)) {
			err_count = 0;
			pr_err("%s: DSP returned error more than limited, restart now !\n", __func__);
			subsystem_restart("adsp");
		}

		last_time = curtime.tv_sec;

		if (err_count >= ADSP_ERR_LIMITED_COUNT) {
			pr_err("%s: DSP returned error more than limited and err_count to 0!\n", __func__);
			err_count = 0;
		}
	}
}
#else
static void adsp_err_check_restart(u32 adsp_error) {}
#endif

int adsp_err_get_lnx_err_code(u32 adsp_error)
{
	adsp_err_check_panic(adsp_error);
	adsp_err_check_restart(adsp_error);

	if (adsp_error > ADSP_ERR_MAX)
		return adsp_err_code_info[ADSP_ERR_MAX].lnx_err_code;
	else
		return adsp_err_code_info[adsp_error].lnx_err_code;
}

char *adsp_err_get_err_str(u32 adsp_error)
{
	if (adsp_error > ADSP_ERR_MAX)
		return adsp_err_code_info[ADSP_ERR_MAX].adsp_err_str;
	else
		return adsp_err_code_info[adsp_error].adsp_err_str;
}

#if IS_ENABLED(CONFIG_SND_SOC_QDSP_DEBUG) && defined(CONFIG_DEBUG_FS)
int __init adsp_err_init(void)
{


	debugfs_adsp_err = debugfs_create_file("msm_adsp_audio_debug",
					       S_IFREG | 0444, NULL, NULL,
					       &adsp_err_debug_ops);

	return 0;
}
#else
int __init adsp_err_init(void) { return 0; }

#endif

void adsp_err_exit(void)
{
	return;
}
