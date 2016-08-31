/*
 * arch/arm/mach-tegra/tegra_bbc_proxy.c
 *
 * Copyright (C) 2013 NVIDIA Corporation. All rights reserved.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/edp.h>

#include <mach/isomgr.h>
#include <mach/latency_allowance.h>
#include <mach/tegra_bbc_proxy.h>
#include <mach/tegra_bbc_thermal.h>

#define MAX_MODEM_EDP_STATES 10

#define MAX_ISO_BW_REQ  1200000

struct tegra_bbc_proxy {
	struct edp_client *modem_boot_edp_client;
	struct edp_client modem_edp_client;
	unsigned int modem_edp_states[MAX_MODEM_EDP_STATES];
	int edp_client_registered;
	int edp_boot_client_registered;
	int edp_initialized;
	char *edp_manager_name;
	char *ap_name;
	unsigned int i_breach_ppm; /* percent time current exceeds i_thresh */
	unsigned int i_thresh_3g_adjperiod; /* 3g i_thresh adj period */
	unsigned int i_thresh_lte_adjperiod; /* lte i_thresh adj period */
	unsigned int threshold; /* current edp threshold value */
	unsigned int state; /* current edp state value */
	struct mutex edp_lock; /* lock for edp operations */

	tegra_isomgr_handle isomgr_handle;
	/* last iso settings with proxy driver */
	unsigned int last_bw;
	unsigned int last_ult;
	unsigned int margin; /* current iso margin bw request */
	unsigned int bw; /* current iso bandwidth request */
	unsigned int lt; /* current iso latency tolerance for emc frequency
			    switches */
	struct mutex iso_lock; /* lock for iso operations */

	struct regulator *sim0;
	struct regulator *sim1;
	struct regulator *rf1v7;
	struct regulator *rf2v65;
};


#define EDP_INT_ATTR(field)						\
static ssize_t								\
field ## _show(struct device *pdev, struct device_attribute *attr,      \
	       char *buf)						\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(pdev);		\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	return sprintf(buf, "%d\n", bbc->field);			\
}									\
static DEVICE_ATTR(field, S_IRUSR, field ## _show, NULL);

EDP_INT_ATTR(i_breach_ppm);
EDP_INT_ATTR(i_thresh_3g_adjperiod);
EDP_INT_ATTR(i_thresh_lte_adjperiod);

static ssize_t i_max_store(struct device *pdev, struct device_attribute *attr,
			   const char *buff, size_t size)
{
	char *s, *state_i_max, buf[50];
	unsigned int states[MAX_MODEM_EDP_STATES];
	unsigned int num_states = 0;
	int ret;

	/* retrieve max current for supported states */
	strlcpy(buf, buff, sizeof(buf));
	s = strim(buf);
	while (s && (num_states < MAX_MODEM_EDP_STATES)) {
		state_i_max = strsep(&s, ",");
		ret = kstrtoul(state_i_max, 10,
			(unsigned long *)&states[num_states]);
		if (ret) {
			dev_err(pdev, "invalid bbc state-current setting\n");
			goto done;
		}
		num_states++;
	}

	if (s && (num_states >= MAX_MODEM_EDP_STATES)) {
		dev_err(pdev, "number of bbc EDP states exceeded max\n");
		ret = -EINVAL;
		goto done;
	}

	ret = tegra_bbc_proxy_edp_register(pdev, num_states, states);
	if (ret)
		goto done;

	ret = size;
done:
	return ret;
}
static DEVICE_ATTR(i_max, S_IWUSR, NULL, i_max_store);

int tegra_bbc_proxy_edp_register(struct device *dev, u32 num_states,
				u32 *states)
{
	struct edp_manager *mgr;
	struct edp_client *ap;
	int ret;
	int i;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	mutex_lock(&bbc->edp_lock);

	/* client should only be registered once per modem boot */
	if (bbc->edp_client_registered) {
		dev_err(dev, "bbc edp client already registered\n");
		ret = -EBUSY;
		goto done;
	}

	memset(bbc->modem_edp_states, 0, sizeof(bbc->modem_edp_states));
	memset(&bbc->modem_edp_client, 0, sizeof(bbc->modem_edp_client));

	/* retrieve max current for supported states */
	for (i = 0; i < num_states; i++) {
		bbc->modem_edp_states[i] = *states;
		states++;
	}

	strncpy(bbc->modem_edp_client.name, "bbc", EDP_NAME_LEN);
	bbc->modem_edp_client.name[EDP_NAME_LEN - 1] = '\0';
	bbc->modem_edp_client.states = bbc->modem_edp_states;
	bbc->modem_edp_client.num_states = num_states;
	bbc->modem_edp_client.e0_index = 0;
	bbc->modem_edp_client.max_borrowers = 1;
	bbc->modem_edp_client.priority = EDP_MAX_PRIO;

	mgr = edp_get_manager(bbc->edp_manager_name);
	if (!mgr) {
		dev_err(dev, "can't get edp manager\n");
		ret = -EINVAL;
		goto done;
	}

	/* unregister modem_boot_client */
	ret = edp_unregister_client(bbc->modem_boot_edp_client);
	if (ret) {
		dev_err(dev, "unable to register bbc boot edp client\n");
		goto done;
	}

	bbc->edp_boot_client_registered = 0;

	/* register modem client */
	ret = edp_register_client(mgr, &bbc->modem_edp_client);
	if (ret) {
		dev_err(dev, "unable to register bbc edp client\n");
		goto done;
	}

	bbc->edp_client_registered = 1;

	ap = edp_get_client(bbc->ap_name);
	if (!ap) {
		dev_err(dev, "can't get ap client\n");
		goto done;
	}

	ret = edp_register_loan(&bbc->modem_edp_client, ap);
	if (ret && ret != -EEXIST) {
		dev_err(dev, "unable to register bbc loan to ap\n");
		goto done;
	}

done:
	mutex_unlock(&bbc->edp_lock);
	return ret;
}
EXPORT_SYMBOL(tegra_bbc_proxy_edp_register);

static int bbc_edp_request_unlocked(struct device *dev, u32 mode, u32 state,
				u32 threshold)
{
	int ret;
	struct edp_client *c;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	if (!bbc->edp_client_registered)
		return -EINVAL;

	if (state != bbc->state) {
		c = &bbc->modem_edp_client;
		if (state >= c->num_states)
			return -EINVAL;

		ret = edp_update_client_request(c, state, NULL);
		if (ret) {
			dev_err(dev, "state update to %u failed\n", state);
			return ret;
		}
		bbc->state = state;
	}

	if (threshold != bbc->threshold) {
		ret = edp_update_loan_threshold(&bbc->modem_edp_client,
						threshold);
		if (ret) {
			dev_err(dev, "threshold update to %u failed\n",
				threshold);
			return ret;
		}
		bbc->threshold = threshold;
	}

	return 0;
}

int tegra_bbc_proxy_edp_request(struct device *dev, u32 mode, u32 state,
				u32 threshold)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	mutex_lock(&bbc->edp_lock);
	ret = bbc_edp_request_unlocked(dev, mode, state, threshold);
	mutex_unlock(&bbc->edp_lock);

	return ret;
}
EXPORT_SYMBOL(tegra_bbc_proxy_edp_request);

static ssize_t request_store(struct device *pdev, struct device_attribute *attr,
			     const char *buff, size_t size)
{
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(pdev);
	unsigned int id;
	int ret;

	if (sscanf(buff, "%u", &id) != 1)
		return -EINVAL;

	if (!bbc->edp_client_registered)
		return -EINVAL;

	mutex_lock(&bbc->edp_lock);
	ret = bbc_edp_request_unlocked(pdev, 0, id, bbc->threshold);
	mutex_unlock(&bbc->edp_lock);

	return ret ? ret : size;
}
static DEVICE_ATTR(request, S_IWUSR, NULL, request_store);

static ssize_t threshold_store(struct device *pdev,
			       struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(pdev);
	unsigned int tv;
	int ret;

	if (sscanf(buff, "%u", &tv) != 1)
		return -EINVAL;

	mutex_lock(&bbc->edp_lock);
	ret = bbc_edp_request_unlocked(pdev, 0, bbc->state, tv);
	mutex_unlock(&bbc->edp_lock);

	return ret ? ret : size;
}
static DEVICE_ATTR(threshold, S_IWUSR, NULL, threshold_store);

static struct device_attribute *edp_attributes[] = {
	&dev_attr_i_breach_ppm,
	&dev_attr_i_thresh_3g_adjperiod,
	&dev_attr_i_thresh_lte_adjperiod,
	&dev_attr_i_max,
	&dev_attr_request,
	&dev_attr_threshold,
	NULL
};

static ssize_t la_bbcr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int la_val;

	if (sscanf(buf, "%u", &la_val) != 1)
		return -EINVAL;
	if (la_val > (MAX_ISO_BW_REQ / 1000))
		return -EINVAL;

	tegra_set_latency_allowance(TEGRA_LA_BBCR, la_val);

	return size;
}

static ssize_t la_bbcw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int la_val;

	if (sscanf(buf, "%u", &la_val) != 1)
		return -EINVAL;
	if (la_val > (MAX_ISO_BW_REQ / 1000))
		return -EINVAL;

	tegra_set_latency_allowance(TEGRA_LA_BBCW, la_val);

	return size;
}

static int bbc_bw_request_unlocked(struct device *dev, u32 mode, u32 bw,
				u32 lt, u32 margin)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	if (bw > MAX_ISO_BW_REQ)
		return -EINVAL;

	if (margin > MAX_ISO_BW_REQ)
		return -EINVAL;

	if (margin != bbc->margin) {
		ret = tegra_isomgr_set_margin(TEGRA_ISO_CLIENT_BBC_0,
			margin, true);
		if (ret) {
			dev_err(dev, "can't margin for bbc bw\n");
			return ret;
		}

		bbc->margin = margin;
	}

	if ((bw != bbc->bw) || (lt != bbc->lt)) {
		ret = tegra_isomgr_reserve(bbc->isomgr_handle, bw, lt);
		if (!ret) {
			dev_err(dev, "can't reserve iso bw\n");
			return ret;
		}
		bbc->bw = bw;

		ret = tegra_isomgr_realize(bbc->isomgr_handle);
		if (!ret) {
			dev_err(dev, "can't realize iso bw\n");
			return ret;
		}
		bbc->lt = lt;

		tegra_set_latency_allowance(TEGRA_LA_BBCR, bw / 1000);
		tegra_set_latency_allowance(TEGRA_LA_BBCW, bw / 1000);
	}

	return 0;
}

int tegra_bbc_proxy_bw_request(struct device *dev, u32 mode, u32 bw, u32 lt,
			u32 margin)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	mutex_lock(&bbc->iso_lock);
	bbc->last_bw = bw;
	bbc->last_ult = lt;

	ret = bbc_bw_request_unlocked(dev, mode, bw, lt, margin);
	mutex_unlock(&bbc->iso_lock);

	return ret;
}
EXPORT_SYMBOL(tegra_bbc_proxy_bw_request);

int tegra_bbc_proxy_restore_iso(struct device *dev)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	if (!bbc)
		return -EINVAL;

	mutex_lock(&bbc->iso_lock);
	ret = bbc_bw_request_unlocked(dev, 0, bbc->last_bw,
					bbc->last_ult, bbc->margin);
	mutex_unlock(&bbc->iso_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_bbc_proxy_restore_iso);

int tegra_bbc_proxy_clear_iso(struct device *dev)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	if (!bbc)
		return -EINVAL;

	mutex_lock(&bbc->iso_lock);
	ret = bbc_bw_request_unlocked(dev, 0, 0,
					1000, bbc->margin);
	mutex_unlock(&bbc->iso_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_bbc_proxy_clear_iso);

static ssize_t iso_res_realize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	unsigned int bw;
	unsigned int ult = 4;
	char buff[50];
	char *val, *s;
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	strlcpy(buff, buf, sizeof(buff));
	s = strim(buff);

	/* first param is bw */
	val = strsep(&s, ",");
	ret = kstrtouint(val, 10, &bw);
	if (ret) {
		pr_err("invalid bw setting\n");
		return -EINVAL;
	}

	/* second param is latency */
	if (s) {
		ret = kstrtouint(s, 10, &ult);
		if (ret) {
			pr_err("invalid latency setting\n");
			return -EINVAL;
		}
	}

	mutex_lock(&bbc->iso_lock);
	bbc->last_bw = bw;
	bbc->last_ult = ult;

	ret = bbc_bw_request_unlocked(dev, 0, bw, ult, bbc->margin);
	mutex_unlock(&bbc->iso_lock);

	return ret ? ret : size;
}

static ssize_t iso_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	unsigned int bw;
	int ret;

	if (sscanf(buf, "%u", &bw) != 1)
		return -EINVAL;

	ret = tegra_bbc_proxy_bw_register(dev, bw);

	return ret ? ret : size;
}

int tegra_bbc_proxy_bw_register(struct device *dev, u32 bw)
{
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	mutex_lock(&bbc->iso_lock);

	if (bbc->isomgr_handle)
		tegra_isomgr_unregister(bbc->isomgr_handle);

	bbc->isomgr_handle = tegra_isomgr_register(TEGRA_ISO_CLIENT_BBC_0,
		bw, NULL, NULL);
	ret = PTR_RET(bbc->isomgr_handle);
	if (ret)
		dev_err(dev, "error registering bbc with isomgr\n");

	ret = tegra_isomgr_set_margin(TEGRA_ISO_CLIENT_BBC_0,
		bw, true);
	if (ret)
		dev_err(dev, "can't margin for bbc bw\n");
	else
		bbc->margin = bw;

	mutex_unlock(&bbc->iso_lock);

	return ret;
}
EXPORT_SYMBOL(tegra_bbc_proxy_bw_register);

static ssize_t iso_margin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	unsigned int margin;
	int ret;
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &margin) != 1)
		return -EINVAL;

	mutex_lock(&bbc->iso_lock);
	ret = bbc_bw_request_unlocked(dev, 0, bbc->bw, bbc->lt, margin);
	mutex_unlock(&bbc->iso_lock);

	return ret ? ret : size;
}


static DEVICE_ATTR(la_bbcr, S_IWUSR, NULL, la_bbcr_store);
static DEVICE_ATTR(la_bbcw, S_IWUSR, NULL, la_bbcw_store);
static DEVICE_ATTR(iso_reserve_realize, S_IWUSR, NULL, iso_res_realize_store);
static DEVICE_ATTR(iso_register, S_IWUSR, NULL, iso_register_store);
static DEVICE_ATTR(iso_margin, S_IWUSR, NULL, iso_margin_store);


static struct device_attribute *mc_attributes[] = {
	&dev_attr_la_bbcr,
	&dev_attr_la_bbcw,
	&dev_attr_iso_reserve_realize,
	&dev_attr_iso_register,
	&dev_attr_iso_margin,
	NULL
};


#define REG_ATTR(field)							\
static ssize_t								\
field ## _show_state(struct device *dev, struct device_attribute *attr,	\
		     char *buf)						\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	if (regulator_is_enabled(bbc->field))				\
		return sprintf(buf, "enabled\n");			\
	else								\
		return sprintf(buf, "disabled\n");			\
}									\
									\
static ssize_t								\
field ## _store_state(struct device *dev, struct device_attribute *attr,\
		      const char *buf, size_t count)			\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	if (sysfs_streq(buf, "enabled\n") || sysfs_streq(buf, "1"))	\
		regulator_enable(bbc->field);				\
	else if (sysfs_streq(buf, "disabled\n") ||			\
		 sysfs_streq(buf, "0"))					\
		regulator_disable(bbc->field);				\
									\
	return count;							\
}									\
static DEVICE_ATTR(field ## _state, 0644,				\
		   field ## _show_state, field ## _store_state);	\
									\
static ssize_t								\
field ## _show_microvolts(struct device *dev,				\
			  struct device_attribute *attr, char *buf)	\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	return sprintf(buf, "%d\n", regulator_get_voltage(bbc->field));	\
}									\
									\
static ssize_t								\
field ## _store_microvolts(struct device *dev,				\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
	int value;							\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	if (sscanf(buf, "%d", &value) != 1)				\
		return -EINVAL;						\
									\
	if (value)							\
		regulator_set_voltage(bbc->field, value, value);	\
									\
	return count;							\
}									\
static DEVICE_ATTR(field ## _microvolts, 0644,				\
		   field ## _show_microvolts,				\
		   field ## _store_microvolts);				\
									\
static ssize_t								\
field ## _show_mode(struct device *dev, struct device_attribute *attr,	\
		     char *buf)						\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	switch (regulator_get_mode(bbc->field)) {			\
	case REGULATOR_MODE_FAST:					\
		return sprintf(buf, "fast\n");				\
	case REGULATOR_MODE_NORMAL:					\
		return sprintf(buf, "normal\n");			\
	case REGULATOR_MODE_IDLE:					\
		return sprintf(buf, "idle\n");				\
	case REGULATOR_MODE_STANDBY:					\
		return sprintf(buf, "standby\n");			\
	}								\
	return sprintf(buf, "unknown\n");				\
}									\
									\
static ssize_t								\
field ## _store_mode(struct device *dev, struct device_attribute *attr,	\
		      const char *buf, size_t count)			\
{									\
	struct tegra_bbc_proxy *bbc = dev_get_drvdata(dev);		\
	int mode = 0;							\
									\
	if (!bbc)							\
		return -EAGAIN;						\
									\
	if (sysfs_streq(buf, "fast\n"))					\
		mode = REGULATOR_MODE_FAST;				\
	else if (sysfs_streq(buf, "normal\n"))				\
		mode = REGULATOR_MODE_NORMAL;				\
	else if (sysfs_streq(buf, "idle\n"))				\
		mode = REGULATOR_MODE_IDLE;				\
	else if (sysfs_streq(buf, "standby\n"))				\
		mode = REGULATOR_MODE_STANDBY;				\
	else								\
		return -EINVAL;						\
									\
	if (regulator_set_mode(bbc->field, mode))			\
		return -EINVAL;						\
									\
	return count;							\
}									\
static DEVICE_ATTR(field ## _mode, 0644,				\
		   field ## _show_mode, field ## _store_mode);

REG_ATTR(sim0);
REG_ATTR(sim1);
REG_ATTR(rf1v7);
REG_ATTR(rf2v65);

static struct device_attribute *sim_attributes[] = {
	&dev_attr_sim0_state,
	&dev_attr_sim0_microvolts,
	&dev_attr_sim0_mode,
	&dev_attr_sim1_state,
	&dev_attr_sim1_microvolts,
	&dev_attr_sim1_mode,
	NULL
};

static struct device_attribute *rf_attributes[] = {
	&dev_attr_rf1v7_state,
	&dev_attr_rf1v7_microvolts,
	&dev_attr_rf1v7_mode,
	&dev_attr_rf2v65_state,
	&dev_attr_rf2v65_microvolts,
	&dev_attr_rf2v65_mode,
	NULL
};

static int tegra_bbc_proxy_probe(struct platform_device *pdev)
{
	struct tegra_bbc_proxy_platform_data *pdata = pdev->dev.platform_data;
	struct tegra_bbc_proxy *bbc;
	struct edp_manager *mgr;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int ret = 0;

	/* check for platform data */
	if (!pdata) {
		dev_err(&pdev->dev, "platform data not available\n");
		return -ENODEV;
	}

	bbc = kzalloc(sizeof(struct tegra_bbc_proxy), GFP_KERNEL);
	if (!bbc) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	if (pdata->modem_boot_edp_client && pdata->edp_manager_name) {
		mutex_init(&bbc->edp_lock);

		/* register bbc boot client */
		bbc->edp_manager_name = pdata->edp_manager_name;
		mgr = edp_get_manager(pdata->edp_manager_name);
		if (!mgr) {
			dev_err(&pdev->dev, "can't get edp manager\n");
			goto error;
		}

		bbc->modem_boot_edp_client = pdata->modem_boot_edp_client;
		ret = edp_register_client(mgr, bbc->modem_boot_edp_client);
		if (ret) {
			dev_err(&pdev->dev,
				"unable to register bbc boot edp client\n");
			goto error;
		}

		/* request E0 */
		ret = edp_update_client_request(bbc->modem_boot_edp_client,
						0, NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"unable to set e0 state\n");
			goto edp_req_error;
		}

		bbc->edp_boot_client_registered = 1;

		bbc->i_breach_ppm = pdata->i_breach_ppm;
		bbc->i_thresh_3g_adjperiod = pdata->i_thresh_3g_adjperiod;
		bbc->i_thresh_lte_adjperiod = pdata->i_thresh_lte_adjperiod;

		attrs = edp_attributes;
		while ((attr = *attrs++)) {
			ret = device_create_file(&pdev->dev, attr);
			if (ret) {
				dev_err(&pdev->dev,
					"can't create sysfs file\n");
				goto edp_req_error;
			}
		}

		bbc->edp_initialized = 1;
		bbc->ap_name = pdata->ap_name;
	}

	mutex_init(&bbc->iso_lock);

	bbc->isomgr_handle = tegra_isomgr_register(TEGRA_ISO_CLIENT_BBC_0,
		MAX_ISO_BW_REQ, NULL, NULL);
	if (!bbc->isomgr_handle)
		goto iso_error;

	tegra_set_latency_allowance(TEGRA_LA_BBCLLR, 640);

	/* statically margin for bbc bw */
	ret = tegra_isomgr_set_margin(TEGRA_ISO_CLIENT_BBC_0,
		MAX_ISO_BW_REQ, true);
	if (ret)
		dev_err(&pdev->dev, "can't margin for bbc bw\n");
	else
		bbc->margin = MAX_ISO_BW_REQ;

	/* thermal zones from bbc */
	tegra_bbc_thermal_init();

	attrs = mc_attributes;
	while ((attr = *attrs++)) {
		ret = device_create_file(&pdev->dev, attr);
		if (ret) {
			dev_err(&pdev->dev, "can't create sysfs file\n");
			goto mc_error;
		}
	}

	bbc->sim0 = regulator_get(&pdev->dev, "vddio_sim0");
	if (IS_ERR(bbc->sim0)) {
		dev_err(&pdev->dev, "vddio_sim0 regulator get failed\n");
		bbc->sim0 = NULL;
		goto sim_error;
	}

	bbc->sim1 = regulator_get(&pdev->dev, "vddio_sim1");
	if (IS_ERR(bbc->sim1)) {
		dev_err(&pdev->dev, "vddio_sim1 regulator get failed\n");
		bbc->sim1 = NULL;
		goto sim_error;
	}

	attrs = sim_attributes;
	while ((attr = *attrs++)) {
		ret = device_create_file(&pdev->dev, attr);
		if (ret) {
			dev_err(&pdev->dev, "can't create sysfs file\n");
			goto sim_error;
		}
	}

	bbc->rf1v7 = regulator_get(&pdev->dev, "vdd_1v7_rf");
	if (IS_ERR(bbc->rf1v7)) {
		dev_info(&pdev->dev,
			 "vdd_1v7_rf regulator not available\n");
		bbc->rf1v7 = NULL;
	}

	bbc->rf2v65 = regulator_get(&pdev->dev, "vdd_2v65_rf");
	if (IS_ERR(bbc->rf2v65)) {
		dev_info(&pdev->dev,
			 "vdd_2v65_rf regulator not available\n");
		bbc->rf2v65 = NULL;
	}

	if (bbc->rf1v7 && bbc->rf2v65) {
		attrs = rf_attributes;
		while ((attr = *attrs++)) {
			ret = device_create_file(&pdev->dev, attr);
			if (ret) {
				dev_err(&pdev->dev,
					"can't create sysfs file\n");
				goto rf_error;
			}
		}
	}

	dev_set_drvdata(&pdev->dev, bbc);

	return 0;

rf_error:
	regulator_put(bbc->rf1v7);
	regulator_put(bbc->rf2v65);
sim_error:
	regulator_put(bbc->sim0);
	regulator_put(bbc->sim1);

	attrs = mc_attributes;
	while ((attr = *attrs++))
		device_remove_file(&pdev->dev, attr);

mc_error:
	tegra_isomgr_unregister(bbc->isomgr_handle);

iso_error:
	if (bbc->edp_initialized) {
		attrs = edp_attributes;
		while ((attr = *attrs++))
			device_remove_file(&pdev->dev, attr);
	}

edp_req_error:
	if (bbc->edp_boot_client_registered)
		edp_unregister_client(bbc->modem_boot_edp_client);

error:
	kfree(bbc);

	return ret;
}

static int __exit tegra_bbc_proxy_remove(struct platform_device *pdev)
{
	struct tegra_bbc_proxy *bbc = platform_get_drvdata(pdev);
	struct device_attribute **attrs;
	struct device_attribute *attr;


	if (bbc->rf1v7 && bbc->rf2v65) {
		attrs = rf_attributes;
		while ((attr = *attrs++))
			device_remove_file(&pdev->dev, attr);
		regulator_put(bbc->rf1v7);
		regulator_put(bbc->rf2v65);
	}

	attrs = sim_attributes;
	while ((attr = *attrs++))
		device_remove_file(&pdev->dev, attr);

	regulator_put(bbc->sim0);
	regulator_put(bbc->sim1);

	attrs = mc_attributes;
	while ((attr = *attrs++))
		device_remove_file(&pdev->dev, attr);

	tegra_isomgr_unregister(bbc->isomgr_handle);

	if (bbc->edp_initialized) {
		attrs = edp_attributes;
		while ((attr = *attrs++))
			device_remove_file(&pdev->dev, attr);
	}

	if (bbc->edp_boot_client_registered)
		edp_unregister_client(bbc->modem_boot_edp_client);

	if (bbc->edp_client_registered)
		edp_unregister_client(&bbc->modem_edp_client);

	kfree(bbc);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_bbc_proxy_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	return 0;
}

static int tegra_bbc_proxy_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

static struct platform_driver tegra_bbc_proxy_driver = {
	.driver = {
		.name = "tegra_bbc_proxy",
		.owner = THIS_MODULE,
	},
	.probe = tegra_bbc_proxy_probe,
	.remove = tegra_bbc_proxy_remove,
#ifdef CONFIG_PM
	.suspend = tegra_bbc_proxy_suspend,
	.resume = tegra_bbc_proxy_resume,
#endif
};

static int __init tegra_bbc_proxy_init(void)
{
	return platform_driver_register(&tegra_bbc_proxy_driver);
}

static void __exit tegra_bbc_proxy_exit(void)
{
	platform_driver_unregister(&tegra_bbc_proxy_driver);
}

module_init(tegra_bbc_proxy_init);
module_exit(tegra_bbc_proxy_exit);

MODULE_DESCRIPTION("Tegra T148 BBC Proxy Module");
MODULE_LICENSE("GPL");
