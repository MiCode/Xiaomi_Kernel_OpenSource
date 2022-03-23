/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <mt-plat/mtk_meminfo.h>

#include "mtk_vcorefs_manager.h"
#include "mtk_vcorefs_governor.h"
#include "mtk_spm_vcore_dvfs.h"
#include "mmdvfs_pmqos.h"

__weak char *spm_vcorefs_dump_dvfs_regs(char *p)
{
	return NULL;
}

static DEFINE_MUTEX(vcorefs_mutex);

#define DEFINE_ATTR_RO(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
}

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

struct vcorefs_profile {
	int plat_init_opp;
	bool init_done;
	bool autok_finish;
	bool autok_lock;
	bool dvfs_lock;
	bool dvfs_request;
	u32 kr_req_mask;
	u32 kr_log_mask;
};

static struct vcorefs_profile vcorefs_ctrl = {
	.plat_init_opp	= 0,
	.init_done	= 0,
#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) \
	|| defined(CONFIG_MACH_MT6771)
	.autok_finish   = 1,
#else
	.autok_finish   = 0,
#endif
	.autok_lock	= 0,
	.dvfs_lock	= 0,
	.dvfs_request   = 0,
	.kr_req_mask	= 0,
	.kr_log_mask	= (1U << KIR_GPU) | (1U << KIR_FBT) | (1U << KIR_PERF)
				| (1U << KIR_TLC),
};

/*
 * __nosavedata will not be restored after IPO-H boot
 */
static bool feature_en __nosavedata;

/*
 * For MET tool register handler
 */
static vcorefs_req_handler_t vcorefs_req_handler;

void vcorefs_register_req_notify(vcorefs_req_handler_t handler)
{
	vcorefs_req_handler = handler;
}
EXPORT_SYMBOL(vcorefs_register_req_notify);

int is_vcorefs_can_work(void)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	int r = 0;

	/* mutex_lock(&vcorefs_mutex); */

	if (pwrctrl->init_done && feature_en)
		r = 1;		/* ready to use vcorefs */
	else if (!is_vcorefs_feature_enable())
		r = -1;		/* not support vcorefs */
	else
		r = 0;		/* init not finish, please wait */

	/* mutex_unlock(&vcorefs_mutex); */

	return r;
}

bool is_vcorefs_request(void)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;

	return pwrctrl->dvfs_request;
}

int vcorefs_each_kicker_request(enum dvfs_kicker kicker)
{
	return kicker_table[kicker];
}

int spm_msdc_dvfs_setting(int msdc, bool enable)
{
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6759)
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;

	if (msdc != KIR_AUTOK_SDIO)
		return 0;

	pwrctrl->autok_finish = enable;

#if defined(CONFIG_MACH_MT6739)
	dvfsrc_msdc_autok_finish();
#endif

	vcorefs_crit("[%s] MSDC AUTOK FINISH\n", __func__);

#ifdef CONFIG_MTK_DCS
	dcs_full_init();
	vcorefs_crit("[%s] DCS FULL INIT FINISH\n", __func__);
#endif /* end of CONFIG_MTK_DCS */

	/* notify MM DVFS for msdc autok end */
	mmdvfs_prepare_action(MMDVFS_PREPARE_CALIBRATION_END);
#endif
	return 0;
}

__weak int spm_vcorefs_get_kicker_group(int kicker)
{
	return 1;
}

static int _get_dvfs_opp(struct vcorefs_profile *pwrctrl,
			enum dvfs_kicker kicker, enum dvfs_opp opp)
{
	unsigned int dvfs_opp = UINT_MAX;
	int i, group;
	char table[NUM_KICKER * 4 + 1];
	char *p = table;
	char *buff_end = table + (NUM_KICKER * 4 + 1);

	group = spm_vcorefs_get_kicker_group(kicker);

	for (i = 0; i < NUM_KICKER; i++) {
		if (kicker_table[i] < 0
			|| group != spm_vcorefs_get_kicker_group(i))
			continue;

		if (kicker_table[i] < dvfs_opp)
			dvfs_opp = kicker_table[i];
	}

	if (group == 1) {
		/* if have no request, set to init OPP */
		if (dvfs_opp == UINT_MAX) {
			dvfs_opp = pwrctrl->plat_init_opp;
			pwrctrl->dvfs_request = false;
		} else {
			pwrctrl->dvfs_request = true;
		}
	}

	for (i = 0; i < NUM_KICKER; i++)
		p += snprintf(p, buff_end-p, "%d, ", kicker_table[i]);

	vcorefs_crit_mask(log_mask(), kicker,
		"kicker: %s, opp: %d, dvfs_opp: %d, sw_opp: %d, kr opp: %s\n",
		governor_get_kicker_name(kicker), opp, dvfs_opp,
		vcorefs_get_sw_opp(), table);

	return dvfs_opp;
}

static int kicker_request_compare(enum dvfs_kicker kicker, enum dvfs_opp opp)
{
	/* compare kicker table opp with request opp (except SYSFS) */
	if (opp == kicker_table[kicker] && kicker != KIR_SYSFS
		&& kicker != KIR_SYSFSX) {
		vcorefs_crit_mask(log_mask(), kicker,
				"opp no change, kr_tb: %d, kr: %d, opp: %d\n",
				kicker_table[kicker], kicker, opp);
		return -1;
	}

	kicker_table[kicker] = opp;

	return 0;
}

static int kicker_request_mask(struct vcorefs_profile *pwrctrl,
				enum dvfs_kicker kicker, enum dvfs_opp opp)
{
	if (pwrctrl->kr_req_mask & (1U << kicker)) {
		if (opp < 0)
			kicker_table[kicker] = opp;

		vcorefs_crit_mask(log_mask(), kicker,
				"mask request, mask: 0x%x, kr: %d, opp: %d\n",
				pwrctrl->kr_req_mask, kicker, opp);
		return -1;
	}

	return 0;
}

/*
 * AutoK related API
 */
static int vcorefs_autok_lock_dvfs(bool lock)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->autok_lock = lock;
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

static int vcorefs_autok_set_vcore(int kicker, enum dvfs_opp opp)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	struct kicker_config krconf;
	int r = 0;

	if (opp >= NUM_OPP || !pwrctrl->autok_lock) {
		vcorefs_crit_mask(log_mask(), kicker,
			"[AUTOK] SET VCORE FAIL, opp: %d, autok_lock: %d\n",
			opp, pwrctrl->autok_lock);
		return -1;
	}

	mutex_lock(&vcorefs_mutex);
	krconf.kicker = kicker;
	krconf.opp = opp;
	krconf.dvfs_opp = opp;

	vcorefs_crit_mask(log_mask(), kicker,
		"[AUTOK] kicker: %s, opp: %d, dvfs_opp: %d, sw_opp: %d\n",
		governor_get_kicker_name(krconf.kicker), krconf.opp,
		krconf.dvfs_opp, vcorefs_get_sw_opp());

	r = kick_dvfs_by_opp_index(&krconf);
	mutex_unlock(&vcorefs_mutex);

	return r;
}

static inline void vcorefs_footprint(int kicker, int opp)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	u32 val;

	if (opp == OPP_UNREQ)
		opp = 0xf;

	/* record last kicker and opp */
	val = aee_rr_curr_vcore_dvfs_opp();
	val &= ~(0xff000000);
	val |= ((kicker << 28) | (opp << 24));
	aee_rr_rec_vcore_dvfs_opp(val);
#endif
}

int vcorefs_request_dvfs_opp(enum dvfs_kicker kicker, enum dvfs_opp opp)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	struct kicker_config krconf;
	int r;
	bool autok_r, autok_lock;
	u32 autok_kir_group = AUTOK_KIR_GROUP;

	if (!feature_en || !pwrctrl->init_done) {
		vcorefs_crit_mask(log_mask(), kicker,
			"feature_en: %d, init_done: %d, kr: %d, opp: %d\n",
			feature_en, pwrctrl->init_done, kicker, opp);
		return -1;
	}

	/* other kicker need waiting msdc autok finish */
	if (!((1U << kicker) & autok_kir_group)) {
		if (pwrctrl->autok_finish == false) {
			vcorefs_crit_mask(log_mask(), kicker,
						"MSDC AUTOK NOT FINISH\n");
			return -1;
		}
	}

	autok_r = governor_autok_check(kicker);
	if (autok_r) {
		autok_lock = governor_autok_lock_check(kicker, opp);

		if (autok_lock) {
			vcorefs_autok_lock_dvfs(autok_lock);
			vcorefs_autok_set_vcore(kicker, opp);
		} else {
#if defined(CONFIG_MACH_MT6771)
			vcorefs_autok_set_vcore(kicker, opp);
			vcorefs_autok_lock_dvfs(autok_lock);
#else
			vcorefs_autok_set_vcore(KIR_SYSFS,
					_get_dvfs_opp(pwrctrl, kicker, opp));
			vcorefs_autok_lock_dvfs(autok_lock);
#endif
		}
		return 0;
	}

	if (kicker != KIR_SYSFSX && (pwrctrl->autok_lock
		|| pwrctrl->dvfs_lock)) {
		vcorefs_crit_mask(log_mask(), kicker,
			"autok_lock: %d, dvfs_lock: %d, kr: %d, opp: %d\n",
			pwrctrl->autok_lock, pwrctrl->dvfs_lock, kicker, opp);
		return -1;
	}

	if (kicker_request_mask(pwrctrl, kicker, opp))
		return -1;

	if (kicker_request_compare(kicker, opp))
		return 0;

	mutex_lock(&vcorefs_mutex);

	krconf.kicker = kicker;
	krconf.opp = opp;
	krconf.dvfs_opp = _get_dvfs_opp(pwrctrl, kicker, opp);

	vcorefs_footprint(kicker, opp);

	if (vcorefs_req_handler)
		vcorefs_req_handler(kicker, opp);

	r = kick_dvfs_by_opp_index(&krconf);

	vcorefs_footprint(0xf, 0xf);

	mutex_unlock(&vcorefs_mutex);

	return r;
}

void vcorefs_drv_init(int plat_init_opp)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	int i;

	mutex_lock(&vcorefs_mutex);
	for (i = 0; i < NUM_KICKER; i++)
		kicker_table[i] = -1;

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_vcore_dvfs_opp(0xffffffff);
#endif

	pwrctrl->plat_init_opp = plat_init_opp;
	pwrctrl->init_done = true;
	feature_en = true;

#if defined(CONFIG_MTK_QOS_SUPPORT)
		pwrctrl->kr_req_mask = (1 << NUM_KICKER) - 1;
#endif

	mutex_unlock(&vcorefs_mutex);

	vcorefs_crit("[%s] done\n", __func__);

	governor_autok_manager();
}

static char *vcorefs_get_kicker_info(char *p)
{
	int i;
	char *buff_end = p + PAGE_SIZE;

	for (i = 0; i < NUM_KICKER; i++)
		p += snprintf(p, buff_end - p,
			"[%s] opp: %d\n",
			governor_get_kicker_name(i), kicker_table[i]);

	return p;
}

u32 log_mask(void)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;

	return pwrctrl->kr_log_mask;
}

static ssize_t vcore_debug_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	char *p = buf;
	char *buff_end = buf + PAGE_SIZE;

	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "[feature_en   ]: %d(%d)(%d)\n",
			feature_en, pwrctrl->autok_lock, pwrctrl->dvfs_lock);
	p += snprintf(p, buff_end - p, "[plat_init_opp]: %d\n",
			pwrctrl->plat_init_opp);
	p += snprintf(p, buff_end - p, "[init_done    ]: %d\n",
			pwrctrl->init_done);
	p += snprintf(p, buff_end - p, "[autok_finish ]: %d\n",
			pwrctrl->autok_finish);
	p += snprintf(p, buff_end - p, "[kr_req_mask  ]: 0x%x\n",
			pwrctrl->kr_req_mask);
	p += snprintf(p, buff_end - p, "[kr_log_mask  ]: 0x%x\n",
			pwrctrl->kr_log_mask);
	p += snprintf(p, buff_end - p, "\n");

	p = governor_get_dvfs_info(p);
	p += snprintf(p, buff_end - p, "\n");

	p = vcorefs_get_kicker_info(p);
	p += snprintf(p, buff_end - p, "\n");

#ifdef CONFIG_MTK_RAM_CONSOLE
	p += snprintf(p, buff_end - p, "[aee]vcore_dvfs_opp   : 0x%x\n",
			aee_rr_curr_vcore_dvfs_opp());
	p += snprintf(p, buff_end - p, "[aee]vcore_dvfs_status: 0x%x\n",
			aee_rr_curr_vcore_dvfs_status());
	p += snprintf(p, buff_end - p, "\n");
#endif

	p = spm_vcorefs_dump_dvfs_regs(p);

	return p - buf;
}

static ssize_t vcore_debug_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	struct vcorefs_profile *pwrctrl = &vcorefs_ctrl;
	struct kicker_config krconf;
	int kicker, val, r;
	char cmd[32];

	r = governor_debug_store(buf);
	if (!r)
		return count;

	if (sscanf(buf, "%31s %d", cmd, &val) != 2)
		return -EPERM;

	/* no log when DRAM HQA stress (0xFFFF)*/
	if ((pwrctrl->kr_log_mask & 0xFFFF) != 65535)
		vcorefs_crit("vcore_debug: cmd: %s, val: %d\n", cmd, val);

	if (!strcmp(cmd, "feature_en")) {
		mutex_lock(&vcorefs_mutex);
		if (val && is_vcorefs_feature_enable() && (!feature_en)) {
			feature_en = 1;
		} else if ((!val) && feature_en) {
			krconf.kicker = KIR_SYSFS;
			krconf.opp = OPP_0;
			krconf.dvfs_opp = OPP_0;

			r = kick_dvfs_by_opp_index(&krconf);
			feature_en = 0;
		}
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "kr_req_mask")) {
		mutex_lock(&vcorefs_mutex);
		pwrctrl->kr_req_mask = val;
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "kr_log_mask")) {
		mutex_lock(&vcorefs_mutex);
		pwrctrl->kr_log_mask = val;
		mutex_unlock(&vcorefs_mutex);
#if defined(CONFIG_MACH_MT6775)
	} else if (!strcmp(cmd, "force")) {
		mutex_lock(&vcorefs_mutex);
		dvfsrc_force_opp(val);
		mutex_unlock(&vcorefs_mutex);
#endif
	}  else {
		/* set kicker opp and do DVFS */
		kicker = vcorefs_output_kicker_id(cmd);
		if (kicker != -1) {
			if (kicker == KIR_SYSFSX) {
				if (val != OPP_UNREQ)
					pwrctrl->dvfs_lock = true;
				else
					pwrctrl->dvfs_lock = false;
			}
			r = vcorefs_request_dvfs_opp(kicker, val);
		}
	}

	return count;
}

static ssize_t opp_table_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	vcorefs_update_opp_table();
	p = vcorefs_get_opp_table_info(p);

	return p - buf;
}

static ssize_t opp_num_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	int num = vcorefs_get_num_opp();
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;

	p += snprintf(p, buff_end - p, "%d\n", num);

	return p - buf;
}

DEFINE_ATTR_RW(vcore_debug);
DEFINE_ATTR_RO(opp_table);
DEFINE_ATTR_RO(opp_num);

static struct attribute *vcorefs_attrs[] = {
	__ATTR_OF(opp_table),
	__ATTR_OF(vcore_debug),
	__ATTR_OF(opp_num),
	NULL,
};

static struct attribute_group vcorefs_attr_group = {
	.name = "vcorefs",
	.attrs = vcorefs_attrs,
};

int init_vcorefs_sysfs(void)
{
	int r;

	r = sysfs_create_group(power_kobj, &vcorefs_attr_group);

	return r;
}
