/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Ben Widawsky <ben@bwidawsk.net>
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/sysfs.h>
#include "intel_drv.h"
#include "i915_drv.h"
#include "intel_clrmgr.h"

#define dev_to_drm_minor(d) dev_get_drvdata((d))

#ifdef CONFIG_PM
static u32 calc_residency(struct drm_device *dev, const u32 reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u64 raw_time; /* 32b value may overflow during fixed point math */
	u64 units = 128ULL, div = 100000ULL, bias = 100ULL;
	u32 ret;

	if (!intel_enable_rc6(dev))
		return 0;

	intel_runtime_pm_get(dev_priv);

	/* On VLV and CHV, residency time is in CZ units rather than 1.28us */
	if (IS_VALLEYVIEW(dev)) {
		u32 reg, czcount_30ns;

		if (IS_CHERRYVIEW(dev))
			reg = CHV_CLK_CTL1;
		else
			reg = VLV_CLK_CTL2;

		czcount_30ns = I915_READ(reg) >> CLK_CTL2_CZCOUNT_30NS_SHIFT;

		if (!czcount_30ns) {
			WARN(!czcount_30ns, "bogus CZ count value");
			ret = 0;
			goto out;
		}

		units = 0;
		div = 1000000ULL;

		if (IS_CHERRYVIEW(dev)) {
			/* Special case for 320Mhz */
			if (czcount_30ns == 1) {
				div = 10000000ULL;
				units = 3125ULL;
			} else {
				/* chv counts are one less */
				czcount_30ns += 1;
			}
		}

		if (units == 0)
			units = DIV_ROUND_UP_ULL(30ULL * bias,
						 (u64)czcount_30ns);

		if (I915_READ(VLV_COUNTER_CONTROL) & VLV_COUNT_RANGE_HIGH)
			units <<= 8;

		div = div * bias;
	}

	raw_time = I915_READ(reg) * units;
	ret = DIV_ROUND_UP_ULL(raw_time, div);

out:
	intel_runtime_pm_put(dev_priv);
	return ret;
}

static ssize_t
show_forcewake(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	return snprintf(buf, PAGE_SIZE, "%x\n", dev_priv->uncore.forcewake_count);
}

static ssize_t
forcewake_store(struct device *kdev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;
	ssize_t ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	if (val)
		gen6_gt_force_wake_get(dev_priv, FORCEWAKE_ALL);
	else
		gen6_gt_force_wake_put(dev_priv, FORCEWAKE_ALL);

	return count;
}

static ssize_t
show_rc6_mask(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *dminor = dev_to_drm_minor(kdev);
	return snprintf(buf, PAGE_SIZE, "%x\n", intel_enable_rc6(dminor->dev));
}

static ssize_t
show_rc6_ms(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *dminor = dev_get_drvdata(kdev);
	u32 rc6_residency = calc_residency(dminor->dev, GEN6_GT_GFX_RC6);
	return snprintf(buf, PAGE_SIZE, "%u\n", rc6_residency);
}

static ssize_t
show_rc6p_ms(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *dminor = dev_to_drm_minor(kdev);
	u32 rc6p_residency = calc_residency(dminor->dev, GEN6_GT_GFX_RC6p);
	if (IS_VALLEYVIEW(dminor->dev))
		rc6p_residency = 0;
	return snprintf(buf, PAGE_SIZE, "%u\n", rc6p_residency);
}

static ssize_t
show_rc6pp_ms(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *dminor = dev_to_drm_minor(kdev);
	u32 rc6pp_residency = calc_residency(dminor->dev, GEN6_GT_GFX_RC6pp);
	if (IS_VALLEYVIEW(dminor->dev))
		rc6pp_residency = 0;
	return snprintf(buf, PAGE_SIZE, "%u\n", rc6pp_residency);
}

static DEVICE_ATTR(forcewake, S_IRUSR | S_IWUSR, show_forcewake,
		   forcewake_store);
static DEVICE_ATTR(rc6_enable, S_IRUGO, show_rc6_mask, NULL);
static DEVICE_ATTR(rc6_residency_ms, S_IRUGO, show_rc6_ms, NULL);
static DEVICE_ATTR(rc6p_residency_ms, S_IRUGO, show_rc6p_ms, NULL);
static DEVICE_ATTR(rc6pp_residency_ms, S_IRUGO, show_rc6pp_ms, NULL);

static struct attribute *rc6_attrs[] = {
	&dev_attr_forcewake.attr,
	&dev_attr_rc6_enable.attr,
	&dev_attr_rc6_residency_ms.attr,
	&dev_attr_rc6p_residency_ms.attr,
	&dev_attr_rc6pp_residency_ms.attr,
	NULL
};

static struct attribute_group rc6_attr_group = {
	.name = power_group_name,
	.attrs =  rc6_attrs
};
#endif

static int l3_access_valid(struct drm_device *dev, loff_t offset)
{
	if (!HAS_L3_DPF(dev))
		return -EPERM;

	if (offset % 4 != 0)
		return -EINVAL;

	if (offset >= GEN7_L3LOG_SIZE)
		return -ENXIO;

	return 0;
}

static ssize_t
i915_l3_read(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct drm_minor *dminor = dev_to_drm_minor(dev);
	struct drm_device *drm_dev = dminor->dev;
	struct drm_i915_private *dev_priv = drm_dev->dev_private;
	int slice = (int)(uintptr_t)attr->private;
	int ret;

	count = round_down(count, 4);

	ret = l3_access_valid(drm_dev, offset);
	if (ret)
		return ret;

	count = min_t(size_t, GEN7_L3LOG_SIZE - offset, count);

	ret = i915_mutex_lock_interruptible(drm_dev);
	if (ret)
		return ret;

	if (dev_priv->l3_parity.remap_info[slice])
		memcpy(buf,
		       dev_priv->l3_parity.remap_info[slice] + (offset/4),
		       count);
	else
		memset(buf, 0, count);

	mutex_unlock(&drm_dev->struct_mutex);

	return count;
}

static ssize_t
i915_l3_write(struct file *filp, struct kobject *kobj,
	      struct bin_attribute *attr, char *buf,
	      loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct drm_minor *dminor = dev_to_drm_minor(dev);
	struct drm_device *drm_dev = dminor->dev;
	struct drm_i915_private *dev_priv = drm_dev->dev_private;
	struct intel_context *ctx;
	u32 *temp = NULL; /* Just here to make handling failures easy */
	int slice = (int)(uintptr_t)attr->private;
	int ret;

	if (!HAS_HW_CONTEXTS(drm_dev))
		return -ENXIO;

	ret = l3_access_valid(drm_dev, offset);
	if (ret)
		return ret;

	ret = i915_mutex_lock_interruptible(drm_dev);
	if (ret)
		return ret;

	if (!dev_priv->l3_parity.remap_info[slice]) {
		temp = kzalloc(GEN7_L3LOG_SIZE, GFP_KERNEL);
		if (!temp) {
			mutex_unlock(&drm_dev->struct_mutex);
			return -ENOMEM;
		}
	}

	ret = i915_gpu_idle(drm_dev);
	if (ret) {
		kfree(temp);
		mutex_unlock(&drm_dev->struct_mutex);
		return ret;
	}

	/* TODO: Ideally we really want a GPU reset here to make sure errors
	 * aren't propagated. Since I cannot find a stable way to reset the GPU
	 * at this point it is left as a TODO.
	*/
	if (temp)
		dev_priv->l3_parity.remap_info[slice] = temp;

	memcpy(dev_priv->l3_parity.remap_info[slice] + (offset/4), buf, count);

	/* NB: We defer the remapping until we switch to the context */
	list_for_each_entry(ctx, &dev_priv->context_list, link)
		ctx->remap_slice |= (1<<slice);

	mutex_unlock(&drm_dev->struct_mutex);

	return count;
}

static struct bin_attribute dpf_attrs = {
	.attr = {.name = "l3_parity", .mode = (S_IRUSR | S_IWUSR)},
	.size = GEN7_L3LOG_SIZE,
	.read = i915_l3_read,
	.write = i915_l3_write,
	.mmap = NULL,
	.private = (void *)0
};

static struct bin_attribute dpf_attrs_1 = {
	.attr = {.name = "l3_parity_slice_1", .mode = (S_IRUSR | S_IWUSR)},
	.size = GEN7_L3LOG_SIZE,
	.read = i915_l3_read,
	.write = i915_l3_write,
	.mmap = NULL,
	.private = (void *)1
};

static ssize_t gt_cur_freq_mhz_show(struct device *kdev,
				    struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	intel_runtime_pm_get(dev_priv);

	mutex_lock(&dev_priv->rps.hw_lock);
	if (IS_VALLEYVIEW(dev_priv->dev)) {
		u32 freq;
		freq = vlv_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS);
		ret = vlv_gpu_freq(dev_priv, (freq >> 8) & 0xff);
	} else {
		ret = dev_priv->rps.cur_freq * GT_FREQUENCY_MULTIPLIER;
	}
	mutex_unlock(&dev_priv->rps.hw_lock);

	intel_runtime_pm_put(dev_priv);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t vlv_rpe_freq_mhz_show(struct device *kdev,
				     struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			vlv_gpu_freq(dev_priv, dev_priv->rps.efficient_freq));
}

static ssize_t gt_max_freq_mhz_show(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	mutex_lock(&dev_priv->rps.hw_lock);
	if (IS_VALLEYVIEW(dev_priv->dev))
		ret = vlv_gpu_freq(dev_priv, dev_priv->rps.max_freq_softlimit);
	else
		ret = dev_priv->rps.max_freq_softlimit * GT_FREQUENCY_MULTIPLIER;
	mutex_unlock(&dev_priv->rps.hw_lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t gt_max_freq_mhz_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;
	ssize_t ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	mutex_lock(&dev_priv->rps.hw_lock);

	if (IS_VALLEYVIEW(dev_priv->dev))
		val = vlv_freq_opcode(dev_priv, val);
	else
		val /= GT_FREQUENCY_MULTIPLIER;

	if (val < dev_priv->rps.min_freq ||
	    val > dev_priv->rps.max_freq ||
	    val < dev_priv->rps.min_freq_softlimit) {
		mutex_unlock(&dev_priv->rps.hw_lock);
		return -EINVAL;
	}

	if (val > dev_priv->rps.rp0_freq)
		DRM_DEBUG("User requested overclocking to %d\n",
			  val * GT_FREQUENCY_MULTIPLIER);

	dev_priv->rps.max_freq_softlimit = val;

	if (dev_priv->rps.cur_freq > val) {
		if (IS_VALLEYVIEW(dev))
			valleyview_set_rps(dev, val);
		else
			gen6_set_rps(dev, val);
	} else if (!IS_VALLEYVIEW(dev)) {
		/* We still need gen6_set_rps to process the new max_delay and
		 * update the interrupt limits even though frequency request is
		 * unchanged. */
		gen6_set_rps(dev, dev_priv->rps.cur_freq);
	}

	mutex_unlock(&dev_priv->rps.hw_lock);

	return count;
}

static ssize_t gt_min_freq_mhz_show(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	mutex_lock(&dev_priv->rps.hw_lock);
	if (IS_VALLEYVIEW(dev_priv->dev))
		ret = vlv_gpu_freq(dev_priv, dev_priv->rps.min_freq_softlimit);
	else
		ret = dev_priv->rps.min_freq_softlimit * GT_FREQUENCY_MULTIPLIER;
	mutex_unlock(&dev_priv->rps.hw_lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t gt_min_freq_mhz_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;
	ssize_t ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	mutex_lock(&dev_priv->rps.hw_lock);

	if (IS_VALLEYVIEW(dev))
		val = vlv_freq_opcode(dev_priv, val);
	else
		val /= GT_FREQUENCY_MULTIPLIER;

	if (val < dev_priv->rps.min_freq ||
	    val > dev_priv->rps.max_freq ||
	    val > dev_priv->rps.max_freq_softlimit) {
		mutex_unlock(&dev_priv->rps.hw_lock);
		return -EINVAL;
	}

	dev_priv->rps.min_freq_softlimit = val;

	if (dev_priv->rps.cur_freq < val) {
		if (IS_VALLEYVIEW(dev))
			valleyview_set_rps(dev, val);
		else
			gen6_set_rps(dev, val);
	} else if (!IS_VALLEYVIEW(dev)) {
		/* We still need gen6_set_rps to process the new min_delay and
		 * update the interrupt limits even though frequency request is
		 * unchanged. */
		gen6_set_rps(dev, dev_priv->rps.cur_freq);
	}

	mutex_unlock(&dev_priv->rps.hw_lock);

	return count;

}

static DEVICE_ATTR(gt_cur_freq_mhz, S_IRUGO, gt_cur_freq_mhz_show, NULL);
static DEVICE_ATTR(gt_max_freq_mhz, S_IRUGO | S_IWUSR, gt_max_freq_mhz_show, gt_max_freq_mhz_store);
static DEVICE_ATTR(gt_min_freq_mhz, S_IRUGO | S_IWUSR, gt_min_freq_mhz_show, gt_min_freq_mhz_store);

static DEVICE_ATTR(vlv_rpe_freq_mhz, S_IRUGO, vlv_rpe_freq_mhz_show, NULL);

static ssize_t gt_rp_mhz_show(struct device *kdev, struct device_attribute *attr, char *buf);
static DEVICE_ATTR(gt_RP0_freq_mhz, S_IRUGO, gt_rp_mhz_show, NULL);
static DEVICE_ATTR(gt_RP1_freq_mhz, S_IRUGO, gt_rp_mhz_show, NULL);
static DEVICE_ATTR(gt_RPn_freq_mhz, S_IRUGO, gt_rp_mhz_show, NULL);

/* For now we have a static number of RP states */
static ssize_t gt_rp_mhz_show(struct device *kdev, struct device_attribute *attr, char *buf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val, rp_state_cap;
	ssize_t ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;
	intel_runtime_pm_get(dev_priv);
	rp_state_cap = I915_READ(GEN6_RP_STATE_CAP);
	intel_runtime_pm_put(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	if (attr == &dev_attr_gt_RP0_freq_mhz) {
		if (IS_VALLEYVIEW(dev))
			val = vlv_gpu_freq(dev_priv, dev_priv->rps.rp0_freq);
		else
			val = ((rp_state_cap & 0x0000ff) >> 0) * GT_FREQUENCY_MULTIPLIER;
	} else if (attr == &dev_attr_gt_RP1_freq_mhz) {
		if (IS_VALLEYVIEW(dev))
			val = vlv_gpu_freq(dev_priv, dev_priv->rps.rp1_freq);
		else
			val = ((rp_state_cap & 0x00ff00) >> 8) * GT_FREQUENCY_MULTIPLIER;
	} else if (attr == &dev_attr_gt_RPn_freq_mhz) {
		if (IS_VALLEYVIEW(dev))
			val = vlv_gpu_freq(dev_priv, dev_priv->rps.min_freq);
		else
			val = ((rp_state_cap & 0xff0000) >> 16) * GT_FREQUENCY_MULTIPLIER;
	} else {
		BUG();
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thaw_show(struct device *kdev, struct device_attribute *attr,
			 char *buf)
{
	return 0;
}
static DEVICE_ATTR(thaw, S_IRUGO, thaw_show, NULL);

static ssize_t gamma_adjust_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	int ret = 0;
	int bytes_count = 0;
	char *buf = NULL;
	int pipe = 0;
	int crtc_id = -1;
	int bytes_read = 0;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_crtc *crtc = NULL;
	struct drm_mode_object *obj;

	/* Validate input */
	if (!count) {
		DRM_ERROR("Gamma adjust: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		ret = -EINVAL;
		goto EXIT;
	}
	bytes_count = count;

	/* Parse data and read the crtc_id */
	ret = parse_clrmgr_input(&crtc_id, buf,
		CRTC_ID_TOKEN_COUNT, &bytes_count);
	if (ret < CRTC_ID_TOKEN_COUNT) {
		DRM_ERROR("CRTC_ID loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("CRTC_ID loading done\n");

	obj = drm_mode_object_find(dev, crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", crtc_id);
		ret = -EINVAL;
		goto EXIT;
	}
	crtc = obj_to_crtc(obj);
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	pipe = to_intel_crtc(crtc)->pipe;
	bytes_read += bytes_count;
	bytes_count = count - bytes_read;
	if (bytes_count > 0) {

		/* Parse data and load the gamma  table */
		ret = parse_clrmgr_input(gamma_softlut[pipe], buf+bytes_read,
			GAMMA_CORRECT_MAX_COUNT, &bytes_count);
		if (ret < GAMMA_CORRECT_MAX_COUNT) {
			DRM_ERROR("Gamma table loading failed\n");
			goto EXIT;
		} else
			DRM_DEBUG("Gamma table loading done\n");
	}
	bytes_read += bytes_count;
	bytes_count = count - bytes_read;
	if (bytes_count > 0) {

		/* Parse data and load the gcmax table */
		ret = parse_clrmgr_input(gcmax_softlut[pipe], buf+bytes_read,
				GC_MAX_COUNT, &bytes_count);

		if (ret < GC_MAX_COUNT)
			DRM_ERROR("GCMAX table loading failed\n");
		else
			DRM_DEBUG("GCMAX table loading done\n");
	}

EXIT:
	kfree(buf);
	if (ret < 0)
		return ret;

	return count;
}


static ssize_t csc_enable_show(struct device *kdev,
		struct device_attribute *attr, char *ubuf)
{
	int len = 0;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	len = scnprintf(ubuf, PAGE_SIZE, "Pipe 0: %s\nPipe 1: %s\n",
		dev_priv->csc_enabled[0] ? "Enabled" : "Disabled",
		dev_priv->csc_enabled[1] ? "Enabled" : "Disabled");

	return len;
}

static ssize_t csc_enable_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	int ret = 0;
	char *buf = NULL;
	int pipe = 0;
	int req_state = 0;
	int bytes_read = 0;
	int bytes_count = count;
	int crtc_id = -1;
	struct drm_mode_object *obj = NULL;
	struct drm_crtc *crtc = NULL;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;

	/* Validate input */
	if (!count) {
		DRM_ERROR("CSC enable: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		ret = -EINVAL;
		goto EXIT;
	}

	/* Parse data and load the crtc_id */
	ret = parse_clrmgr_input(&crtc_id, buf,
		CRTC_ID_TOKEN_COUNT, &bytes_count);
	if (ret < CRTC_ID_TOKEN_COUNT) {
		DRM_ERROR("CRTC_ID_TOKEN loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("CRTC_ID_TOKEN loading done\n");

	obj = drm_mode_object_find(dev, crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", crtc_id);
		ret = -EINVAL;
		goto EXIT;
	}
	crtc = obj_to_crtc(obj);
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	pipe = to_intel_crtc(crtc)->pipe;
	bytes_read += bytes_count;
	bytes_count = count - bytes_read;
	if (bytes_count > 0) {

		/* Parse data and load the gamma  table */
		ret = parse_clrmgr_input(&req_state, buf+bytes_read,
			ENABLE_TOKEN_MAX_COUNT, &bytes_count);
		if (ret < ENABLE_TOKEN_MAX_COUNT) {
			DRM_ERROR("Enable-token loading failed\n");
			goto EXIT;
		} else
			DRM_DEBUG("Enable-token loading done\n");
	} else {
		DRM_ERROR("Enable-token loading failed\n");
		ret = -EINVAL;
		goto EXIT;
	}
	DRM_DEBUG_KMS("req_state:%d\n", req_state);


	/* if CSC enabled, apply CSC correction */
	if (req_state) {
		if (do_intel_enable_csc(dev,
				(void *) csc_softlut[pipe], crtc)) {
			DRM_ERROR("CSC correction failed\n");
			ret = -EINVAL;
		} else
			ret = count;
	} else {
		/* Disable CSC on this CRTC */
		do_intel_disable_csc(dev, crtc);
		ret = count;
	}

EXIT:
	kfree(buf);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t csc_adjust_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	int bytes_count = count;
	int ret = 0;
	char *buf = NULL;
	int bytes_read = 0;
	int pipe = 0;
	int crtc_id = -1;
	struct drm_crtc *crtc = NULL;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_mode_object *obj = NULL;

	if (!count) {
		DRM_ERROR("CSC adjust: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		ret = -EINVAL;
		goto EXIT;
	}

	/* Parse data and load the crtc_id */
	ret = parse_clrmgr_input(&crtc_id, buf,
		CRTC_ID_TOKEN_COUNT, &bytes_count);
	if (ret < CRTC_ID_TOKEN_COUNT) {
		DRM_ERROR("CONNECTOR_TYPE_TOKEN loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("CONNECTOR_TYPE_TOKEN loading done\n");

	obj = drm_mode_object_find(dev, crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", crtc_id);
		ret = -EINVAL;
		goto EXIT;
	}
	crtc = obj_to_crtc(obj);
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	pipe = to_intel_crtc(crtc)->pipe;
	bytes_read += bytes_count;
	bytes_count = count - bytes_read;
	if (bytes_count > 0) {

		/* Parse data and load the csc  table */
		ret = parse_clrmgr_input(csc_softlut[pipe], buf+bytes_read,
			CSC_MAX_COEFF_COUNT, &bytes_count);
		if (ret < CSC_MAX_COEFF_COUNT)
			DRM_ERROR("CSC table loading failed\n");
		else
			DRM_DEBUG("CSC table loading done\n");
	}
EXIT:
	kfree(buf);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t gamma_enable_show(struct device *kdev,
		struct device_attribute *attr,  char *ubuf)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int len = 0;

	len = scnprintf(ubuf, PAGE_SIZE, "Pipe 0: %s\nPipe 1: %s\n",
		dev_priv->gamma_enabled[0] ? "Enabled" : "Disabled",
		dev_priv->gamma_enabled[1] ? "Enabled" : "Disabled");

	return len;
}

static ssize_t gamma_enable_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	int ret = 0;
	struct drm_crtc *crtc = NULL;
	char *buf = NULL;
	int bytes_read = 0;
	int bytes_count = 0;
	int crtc_id = -1;
	int req_state = 0;
	int pipe = 0;
	struct drm_mode_object *obj;

	/* Validate input */
	if (!count) {
		DRM_ERROR("Gamma adjust: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		ret = -EINVAL;
		goto EXIT;
	}

	bytes_count = count;

	/* Parse data and load the crtc_id */
	ret = parse_clrmgr_input(&crtc_id, buf,
		CRTC_ID_TOKEN_COUNT, &bytes_count);
	if (ret < CRTC_ID_TOKEN_COUNT) {
		DRM_ERROR("CRTC_ID loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("CRTC_ID loading done\n");

	obj = drm_mode_object_find(dev, crtc_id, DRM_MODE_OBJECT_CRTC);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown CRTC ID %d\n", crtc_id);
		ret = -EINVAL;
		goto EXIT;
	}

	crtc = obj_to_crtc(obj);
	pipe = to_intel_crtc(crtc)->pipe;
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	bytes_read += bytes_count;
	bytes_count = count - bytes_read;
	if (bytes_count > 0) {

		/* Parse data and load the gamma  table */
		ret = parse_clrmgr_input(&req_state, buf+bytes_read,
			ENABLE_TOKEN_MAX_COUNT, &bytes_count);
		if (ret < ENABLE_TOKEN_MAX_COUNT) {
			DRM_ERROR("Enable-token loading failed\n");
			goto EXIT;
		} else
			DRM_DEBUG("Enable-token loading done\n");
	} else {
		DRM_ERROR("Enable-token loading failed\n");
		ret = -EINVAL;
		goto EXIT;
	}

	/* if gamma enabled, apply gamma correction on PIPE */
	if (req_state) {
		if (intel_crtc_enable_gamma(crtc,
				pipe ? PIPEB : PIPEA)) {
			DRM_ERROR("Apply gamma correction failed\n");
			ret = -EINVAL;
		} else
			ret = count;
	} else {
		/* Disable gamma on this plane */
		intel_crtc_disable_gamma(crtc,
				pipe ? PIPEB : PIPEA);
		ret = count;
	}

EXIT:
	kfree(buf);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t cb_adjust_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	int ret = 0;
	int bytes_count = count;
	struct cont_brightlut *cb_ptr = NULL;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	char *buf = NULL;

	/* Validate input */
	if (!count) {
		DRM_ERROR("Contrast Brightness: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		kfree(buf);
		return -EINVAL;
	}

	cb_ptr = kzalloc(sizeof(struct cont_brightlut), GFP_KERNEL);
	if (!cb_ptr) {
		DRM_ERROR("Contrast Brightness adjust: insufficient memory\n");
		kfree(buf);
		return -ENOMEM;
	}

	/* Parse input data */
	ret = parse_clrmgr_input((uint *)cb_ptr, buf, CB_MAX_COEFF_COUNT,
			&bytes_count);
	if (ret < CB_MAX_COEFF_COUNT) {
		DRM_ERROR("Contrast Brightness loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("Contrast Brightness loading done\n");

	if (cb_ptr->sprite_no < SPRITEA || cb_ptr->sprite_no > SPRITED ||
			cb_ptr->sprite_no == PLANEB) {
		DRM_ERROR("Sprite value out of range. Enter 2,3, 5 or 6\n");
		goto EXIT;
	}

	DRM_DEBUG("sprite = %d Val=0x%x,\n", cb_ptr->sprite_no, cb_ptr->val);

	if (intel_sprite_cb_adjust(dev_priv, cb_ptr))
		DRM_ERROR("Contrast Brightness update failed\n");

EXIT:
	kfree(buf);
	kfree(cb_ptr);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t hs_adjust_store(struct device *kdev,
				     struct device_attribute *attr,
				     const char *ubuf, size_t count)
{
	int ret = count;
	int bytes_count = count;
	struct hue_saturationlut *hs_ptr = NULL;
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	char *buf = NULL;

	/* Validate input */
	if (!count) {
		DRM_ERROR("Hue Saturation: insufficient data\n");
		return -EINVAL;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf) {
		DRM_ERROR("Gamma adjust: insufficient memory\n");
		return -ENOMEM;
	}

	/* Get the data */
	if (!strncpy(buf, ubuf, count)) {
		DRM_ERROR("Gamma adjust: copy failed\n");
		kfree(buf);
		return -EINVAL;
	}

	hs_ptr = kzalloc(sizeof(struct hue_saturationlut), GFP_KERNEL);
	if (!hs_ptr) {
		DRM_ERROR("Hue Saturation adjust: insufficient memory\n");
		kfree(buf);
		return -ENOMEM;
	}

	/* Parse input data */
	ret = parse_clrmgr_input((uint *)hs_ptr, buf, HS_MAX_COEFF_COUNT,
			&bytes_count);
	if (ret < HS_MAX_COEFF_COUNT) {
		DRM_ERROR("Hue Saturation loading failed\n");
		goto EXIT;
	} else
		DRM_DEBUG("Hue Saturation loading done\n");

	if (hs_ptr->sprite_no < SPRITEA || hs_ptr->sprite_no > SPRITED ||
			hs_ptr->sprite_no == PLANEB) {
		DRM_ERROR("sprite = %d Val=0x%x,\n", hs_ptr->sprite_no,
					hs_ptr->val);
		goto EXIT;
	}

	DRM_DEBUG("sprite = %d Val=0x%x,\n", hs_ptr->sprite_no, hs_ptr->val);

	if (intel_sprite_hs_adjust(dev_priv, hs_ptr))
		DRM_ERROR("Hue Saturation update failed\n");

EXIT:
	kfree(buf);
	kfree(hs_ptr);

	if (ret < 0)
		return ret;

	return count;
}


static DEVICE_ATTR(gamma_enable, S_IRUGO | S_IWUSR, gamma_enable_show,
						gamma_enable_store);
static DEVICE_ATTR(gamma_adjust, S_IWUSR, NULL, gamma_adjust_store);
static DEVICE_ATTR(csc_enable, S_IRUGO | S_IWUSR, csc_enable_show,
						csc_enable_store);
static DEVICE_ATTR(csc_adjust, S_IWUSR, NULL, csc_adjust_store);
static DEVICE_ATTR(cb_adjust, S_IWUSR, NULL, cb_adjust_store);
static DEVICE_ATTR(hs_adjust, S_IWUSR, NULL, hs_adjust_store);

static const struct attribute *gen6_attrs[] = {
	&dev_attr_gt_cur_freq_mhz.attr,
	&dev_attr_gt_max_freq_mhz.attr,
	&dev_attr_gt_min_freq_mhz.attr,
	&dev_attr_gt_RP0_freq_mhz.attr,
	&dev_attr_gt_RP1_freq_mhz.attr,
	&dev_attr_gt_RPn_freq_mhz.attr,
	&dev_attr_thaw.attr,
	NULL,
};

static const struct attribute *vlv_attrs[] = {
	&dev_attr_gt_cur_freq_mhz.attr,
	&dev_attr_gt_max_freq_mhz.attr,
	&dev_attr_gt_min_freq_mhz.attr,
	&dev_attr_gt_RP0_freq_mhz.attr,
	&dev_attr_gt_RP1_freq_mhz.attr,
	&dev_attr_gt_RPn_freq_mhz.attr,
	&dev_attr_vlv_rpe_freq_mhz.attr,
	&dev_attr_thaw.attr,
	&dev_attr_gamma_enable.attr,
	&dev_attr_gamma_adjust.attr,
	&dev_attr_csc_enable.attr,
	&dev_attr_csc_adjust.attr,
	&dev_attr_cb_adjust.attr,
	&dev_attr_hs_adjust.attr,
	NULL,
};

static ssize_t error_state_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr, char *buf,
				loff_t off, size_t count)
{

	struct device *kdev = container_of(kobj, struct device, kobj);
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct i915_error_state_file_priv error_priv;
	struct drm_i915_error_state_buf error_str;
	ssize_t ret_count = 0;
	int ret;

	memset(&error_priv, 0, sizeof(error_priv));

	ret = i915_error_state_buf_init(&error_str, count, off);
	if (ret)
		return ret;

	error_priv.dev = dev;
	i915_error_state_get(dev, &error_priv);

	ret = i915_error_state_to_str(&error_str, &error_priv);
	if (ret)
		goto out;

	ret_count = count < error_str.bytes ? count : error_str.bytes;

	memcpy(buf, error_str.buf, ret_count);
out:
	i915_error_state_put(&error_priv);
	i915_error_state_buf_release(&error_str);

	return ret ?: ret_count;
}

static ssize_t error_state_write(struct file *file, struct kobject *kobj,
				 struct bin_attribute *attr, char *buf,
				 loff_t off, size_t count)
{
	struct device *kdev = container_of(kobj, struct device, kobj);
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	int ret;

	DRM_DEBUG_DRIVER("Resetting error state\n");

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	i915_destroy_error_state(dev);
	mutex_unlock(&dev->struct_mutex);

	return count;
}

static ssize_t i915_gem_clients_state_read(struct file *filp,
				struct kobject *memtrack_kobj,
				struct bin_attribute *attr,
				char *buf, loff_t off, size_t count)
{
	struct kobject *kobj = memtrack_kobj->parent;
	struct device *kdev = container_of(kobj, struct device, kobj);
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct drm_i915_error_state_buf error_str;
	ssize_t ret_count = 0;
	int ret;

	ret = i915_error_state_buf_init(&error_str, count, off);
	if (ret)
		return ret;

	ret = i915_get_drm_clients_info(&error_str, dev);
	if (ret)
		goto out;

	ret_count = count < error_str.bytes ? count : error_str.bytes;

	memcpy(buf, error_str.buf, ret_count);
out:
	i915_error_state_buf_release(&error_str);

	return ret ?: ret_count;
}

#define GEM_OBJ_STAT_BUF_SIZE (4*1024) /* 4KB */
#define GEM_OBJ_STAT_BUF_SIZE_MAX (1024*1024) /* 1MB */

struct i915_gem_file_attr_priv {
	char tgid_str[16];
	struct pid *tgid;
	struct drm_i915_error_state_buf buf;
};

static ssize_t i915_gem_read_objects(struct file *filp,
				struct kobject *memtrack_kobj,
				struct bin_attribute *attr,
				char *buf, loff_t off, size_t count)
{
	struct kobject *kobj = memtrack_kobj->parent;
	struct device *kdev = container_of(kobj, struct device, kobj);
	struct drm_minor *minor = dev_to_drm_minor(kdev);
	struct drm_device *dev = minor->dev;
	struct i915_gem_file_attr_priv *attr_priv;
	struct pid *tgid;
	ssize_t ret_count = 0;
	long bytes_available;
	int ret = 0, buf_size = GEM_OBJ_STAT_BUF_SIZE;
	unsigned long timeout = msecs_to_jiffies(500) + 1;

	/*
	 * There may arise a scenario where syfs file entry is being removed,
	 * and may race against sysfs read. Sysfs file remove function would
	 * have taken the drm_global_mutex and would wait for read to finish,
	 * which is again waiting to acquire drm_global_mutex, leading to
	 * deadlock. To avoid this, use mutex_trylock here with a timeout.
	 */
	while (!mutex_trylock(&drm_global_mutex) && --timeout)
		schedule_timeout_killable(1);
	if (timeout == 0) {
		DRM_DEBUG_DRIVER("Unable to acquire drm global mutex.\n");
		return -EBUSY;
	}

	if (!attr || !attr->private) {
		DRM_ERROR("attr | attr->private pointer is NULL\n");
		return -EINVAL;
	}
	attr_priv = attr->private;
	tgid = attr_priv->tgid;

	if (off && !attr_priv->buf.buf) {
		ret = -EINVAL;
		DRM_ERROR(
			"Buf not allocated during read with non-zero offset\n");
		goto out;
	}

	if (off == 0) {
retry:
		if (!attr_priv->buf.buf) {
			ret = i915_obj_state_buf_init(&attr_priv->buf,
				buf_size);
			if (ret) {
				DRM_ERROR(
					"obj state buf init failed. buf_size=%d\n",
					buf_size);
				goto out;
			}
		} else {
			/* Reset the buf parameters before filling data */
			attr_priv->buf.pos = 0;
			attr_priv->buf.bytes = 0;
		}

		/* Read the gfx device stats */
		ret = i915_gem_get_obj_info(&attr_priv->buf, dev, tgid);
		if (ret)
			goto out;

		ret = i915_error_ok(&attr_priv->buf);
		if (ret) {
			ret = 0;
			goto copy_data;
		}
		if (buf_size >= GEM_OBJ_STAT_BUF_SIZE_MAX) {
			DRM_DEBUG_DRIVER("obj stat buf size limit reached\n");
			ret = -ENOMEM;
			goto out;
		} else {
			/* Try to reallocate buf of larger size */
			i915_error_state_buf_release(&attr_priv->buf);
			buf_size *= 2;

			ret = i915_obj_state_buf_init(&attr_priv->buf,
						buf_size);
			if (ret) {
				DRM_ERROR(
					"obj stat buf init failed. buf_size=%d\n",
					buf_size);
				goto out;
			}
			goto retry;
		}
	}
copy_data:

	bytes_available = (long)attr_priv->buf.bytes - (long)off;

	if (bytes_available > 0) {
		ret_count = count < bytes_available ? count : bytes_available;
		memcpy(buf, attr_priv->buf.buf + off, ret_count);
	} else
		ret_count = 0;

out:
	mutex_unlock(&drm_global_mutex);

	return ret ?: ret_count;
}

int i915_gem_create_sysfs_file_entry(struct drm_device *dev,
					struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_gem_file_attr_priv *attr_priv;
	struct bin_attribute *obj_attr;
	struct drm_file *file_local;
	int ret;

	if (!i915.memtrack_debug)
		return 0;

	/*
	 * Check for multiple drm files having same tgid. If found, copy the
	 * bin attribute into the new file priv. Otherwise allocate a new
	 * copy of bin attribute, and create its corresponding sysfs file.
	 */
	mutex_lock(&dev->struct_mutex);
	list_for_each_entry(file_local, &dev->filelist, lhead) {
		struct drm_i915_file_private *file_priv_local =
				file_local->driver_priv;

		if (file_priv->tgid == file_priv_local->tgid) {
			file_priv->obj_attr = file_priv_local->obj_attr;
			mutex_unlock(&dev->struct_mutex);
			return 0;
		}
	}
	mutex_unlock(&dev->struct_mutex);

	obj_attr = kzalloc(sizeof(*obj_attr), GFP_KERNEL);
	if (!obj_attr) {
		DRM_ERROR("Alloc failed. Out of memory\n");
		ret = -ENOMEM;
		goto out;
	}

	attr_priv = kzalloc(sizeof(*attr_priv), GFP_KERNEL);
	if (!attr_priv) {
		DRM_ERROR("Alloc failed. Out of memory\n");
		ret = -ENOMEM;
		goto out_obj_attr;
	}

	snprintf(attr_priv->tgid_str, 16, "%d", task_tgid_nr(current));
	obj_attr->attr.name = attr_priv->tgid_str;
	obj_attr->attr.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	obj_attr->size = 0;
	obj_attr->read = i915_gem_read_objects;

	attr_priv->tgid = file_priv->tgid;
	obj_attr->private = attr_priv;

	ret = sysfs_create_bin_file(&dev_priv->memtrack_kobj,
				   obj_attr);
	if (ret) {
		DRM_ERROR(
			"sysfs tgid file setup failed. tgid=%d, process:%s, ret:%d\n",
			pid_nr(file_priv->tgid), file_priv->process_name, ret);

		goto out_attr_priv;
	}

	file_priv->obj_attr = obj_attr;
	return 0;

out_attr_priv:
	kfree(attr_priv);
out_obj_attr:
	kfree(obj_attr);
out:
	return ret;
}

void i915_gem_remove_sysfs_file_entry(struct drm_device *dev,
			struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	struct drm_file *file_local;
	int open_count = 0;

	if (!i915.memtrack_debug)
		return;

	/*
	 * Check if drm file being removed is the last one for that
	 * particular tgid. If so, remove the corresponding sysfs
	 * file entry also
	 */
	list_for_each_entry(file_local, &dev->filelist, lhead) {
		struct drm_i915_file_private *file_priv_local =
				file_local->driver_priv;

		if (pid_nr(file_priv->tgid) == pid_nr(file_priv_local->tgid))
			open_count++;
	}

	WARN_ON(open_count == 0);

	if (open_count == 1) {
		struct i915_gem_file_attr_priv *attr_priv;

		if (WARN_ON(file_priv->obj_attr == NULL))
			return;
		attr_priv = file_priv->obj_attr->private;

		sysfs_remove_bin_file(&dev_priv->memtrack_kobj,
				file_priv->obj_attr);

		i915_error_state_buf_release(&attr_priv->buf);
		kfree(file_priv->obj_attr->private);
		kfree(file_priv->obj_attr);
	}
}

static struct bin_attribute error_state_attr = {
	.attr.name = "error",
	.attr.mode = S_IRUSR | S_IWUSR,
	.size = 0,
	.read = error_state_read,
	.write = error_state_write,
};

static struct bin_attribute i915_gem_client_state_attr = {
	.attr.name = "i915_gem_meminfo",
	.attr.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
	.size = 0,
	.read = i915_gem_clients_state_read,
};

static struct attribute *memtrack_kobj_attrs[] = {NULL};

static struct kobj_type memtrack_kobj_type = {
	.release = NULL,
	.sysfs_ops = NULL,
	.default_attrs = memtrack_kobj_attrs,
};

void i915_setup_sysfs(struct drm_device *dev)
{
	int ret;

#ifdef CONFIG_PM
	if (INTEL_INFO(dev)->gen >= 6) {
		ret = sysfs_merge_group(&dev->primary->kdev->kobj,
					&rc6_attr_group);
		if (ret)
			DRM_ERROR("RC6 residency sysfs setup failed\n");
	}
#endif
	if (HAS_L3_DPF(dev)) {
		ret = device_create_bin_file(dev->primary->kdev, &dpf_attrs);
		if (ret)
			DRM_ERROR("l3 parity sysfs setup failed\n");

		if (NUM_L3_SLICES(dev) > 1) {
			ret = device_create_bin_file(dev->primary->kdev,
						     &dpf_attrs_1);
			if (ret)
				DRM_ERROR("l3 parity slice 1 setup failed\n");
		}
	}

	ret = 0;
	if (IS_VALLEYVIEW(dev))
		ret = sysfs_create_files(&dev->primary->kdev->kobj, vlv_attrs);
	else if (INTEL_INFO(dev)->gen >= 6)
		ret = sysfs_create_files(&dev->primary->kdev->kobj, gen6_attrs);
	if (ret)
		DRM_ERROR("RPS sysfs setup failed\n");

	ret = sysfs_create_bin_file(&dev->primary->kdev->kobj,
				    &error_state_attr);
	if (ret)
		DRM_ERROR("error_state sysfs setup failed\n");

	if (i915.memtrack_debug) {
		struct drm_i915_private *dev_priv = dev->dev_private;

		/*
		 * Create the gfx_memtrack directory for memtrack sysfs files
		 */
		ret = kobject_init_and_add(
			&dev_priv->memtrack_kobj, &memtrack_kobj_type,
			&dev->primary->kdev->kobj, "gfx_memtrack");
		if (unlikely(ret != 0)) {
			DRM_ERROR(
				"i915 sysfs setup memtrack directory failed\n"
				);
			kobject_put(&dev_priv->memtrack_kobj);
		} else {
			ret = sysfs_create_bin_file(&dev_priv->memtrack_kobj,
					    &i915_gem_client_state_attr);
			if (ret)
				DRM_ERROR(
					  "i915_gem_client_state sysfs setup failed\n"
				);
		}
	}
}

void i915_teardown_sysfs(struct drm_device *dev)
{
	sysfs_remove_bin_file(&dev->primary->kdev->kobj, &error_state_attr);
	if (IS_VALLEYVIEW(dev))
		sysfs_remove_files(&dev->primary->kdev->kobj, vlv_attrs);
	else
		sysfs_remove_files(&dev->primary->kdev->kobj, gen6_attrs);
	device_remove_bin_file(dev->primary->kdev,  &dpf_attrs_1);
	device_remove_bin_file(dev->primary->kdev,  &dpf_attrs);
#ifdef CONFIG_PM
	sysfs_unmerge_group(&dev->primary->kdev->kobj, &rc6_attr_group);
#endif
	if (i915.memtrack_debug) {
		struct drm_i915_private *dev_priv = dev->dev_private;

		sysfs_remove_bin_file(&dev_priv->memtrack_kobj,
					&i915_gem_client_state_attr);
		kobject_del(&dev_priv->memtrack_kobj);
		kobject_put(&dev_priv->memtrack_kobj);
	}
}
