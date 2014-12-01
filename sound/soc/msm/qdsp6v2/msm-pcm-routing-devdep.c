/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/module.h>
#include <sound/hwdep.h>
#include <sound/devdep_params.h>
#include <sound/msm-dts-eagle.h>

#include "msm-pcm-routing-devdep.h"
#include "msm-ds2-dap-config.h"

#ifdef CONFIG_SND_HWDEP
static int msm_pcm_routing_hwdep_open(struct snd_hwdep *hw, struct file *file)
{
	pr_debug("%s\n", __func__);
	msm_ds2_dap_update_port_parameters(hw, file, true);
	return 0;
}

static int msm_pcm_routing_hwdep_release(struct snd_hwdep *hw,
					 struct file *file)
{
	pr_debug("%s\n", __func__);
	msm_ds2_dap_update_port_parameters(hw, file, false);
	return 0;
}

static int msm_pcm_routing_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
				       unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	pr_debug("%s:cmd %x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM:
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM:
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND:
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE:
		msm_pcm_routing_acquire_lock();
		ret = msm_ds2_dap_ioctl(hw, file, cmd, argp);
		msm_pcm_routing_release_lock();
		break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER:
		ret = msm_ds2_dap_ioctl(hw, file, cmd, argp);
		break;
	case DTS_EAGLE_IOCTL_GET_CACHE_SIZE:
	case DTS_EAGLE_IOCTL_SET_CACHE_SIZE:
	case DTS_EAGLE_IOCTL_GET_PARAM:
	case DTS_EAGLE_IOCTL_SET_PARAM:
	case DTS_EAGLE_IOCTL_SET_CACHE_BLOCK:
	case DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE:
	case DTS_EAGLE_IOCTL_GET_LICENSE:
	case DTS_EAGLE_IOCTL_SET_LICENSE:
	case DTS_EAGLE_IOCTL_SEND_LICENSE:
	case DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS:
		ret = msm_dts_eagle_ioctl(cmd, arg);
		if (ret == -EPERM) {
			pr_err("%s called with invalid control 0x%X\n",
				__func__, cmd);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("%s called with invalid control 0x%X\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

void msm_pcm_routing_hwdep_free(struct snd_pcm *pcm)
{
	pr_debug("%s\n", __func__);
	msm_dts_eagle_pcm_free(pcm);
}

#ifdef CONFIG_COMPAT
static int msm_pcm_routing_hwdep_compat_ioctl(struct snd_hwdep *hw,
					      struct file *file,
					      unsigned int cmd,
					      unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	pr_debug("%s:cmd %x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM32:
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM32:
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND32:
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE32:
		msm_pcm_routing_acquire_lock();
		ret = msm_ds2_dap_compat_ioctl(hw, file, cmd, argp);
		msm_pcm_routing_release_lock();
		break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER32:
		ret = msm_ds2_dap_compat_ioctl(hw, file, cmd, argp);
		break;
	case DTS_EAGLE_IOCTL_GET_CACHE_SIZE32:
	case DTS_EAGLE_IOCTL_SET_CACHE_SIZE32:
	case DTS_EAGLE_IOCTL_GET_PARAM32:
	case DTS_EAGLE_IOCTL_SET_PARAM32:
	case DTS_EAGLE_IOCTL_SET_CACHE_BLOCK32:
	case DTS_EAGLE_IOCTL_SET_ACTIVE_DEVICE32:
	case DTS_EAGLE_IOCTL_GET_LICENSE32:
	case DTS_EAGLE_IOCTL_SET_LICENSE32:
	case DTS_EAGLE_IOCTL_SEND_LICENSE32:
	case DTS_EAGLE_IOCTL_SET_VOLUME_COMMANDS32:
		ret = msm_dts_eagle_compat_ioctl(cmd, arg);
		if (ret == -EPERM) {
			pr_err("%s called with invalid control 0x%X\n",
				__func__, cmd);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("%s called with invalid control 0x%X\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif

int msm_pcm_routing_hwdep_new(struct snd_soc_pcm_runtime *runtime,
			      struct msm_pcm_routing_bdai_data *msm_bedais)
{
	struct snd_hwdep *hwdep;
	struct snd_soc_dai_link *dai_link = runtime->dai_link;
	int rc;

	if (dai_link->be_id < 0 ||
		dai_link->be_id >= MSM_BACKEND_DAI_MAX) {
		pr_err("%s:be_id %d invalid index\n",
			__func__, dai_link->be_id);
		return -EINVAL;
	}
	pr_debug("%s be_id %d\n", __func__, dai_link->be_id);
	rc = snd_hwdep_new(runtime->card->snd_card,
			   msm_bedais[dai_link->be_id].name,
			   dai_link->be_id, &hwdep);
	if (hwdep == NULL) {
		pr_err("%s: hwdep intf failed to create %s- hwdep NULL\n",
			__func__, msm_bedais[dai_link->be_id].name);
		return rc;
	}
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: hwdep intf failed to create %s rc %d\n", __func__,
			msm_bedais[dai_link->be_id].name, rc);
		return rc;
	}

	hwdep->iface = SNDRV_HWDEP_IFACE_AUDIO_BE;
	hwdep->private_data = &msm_bedais[dai_link->be_id];
	hwdep->ops.open = msm_pcm_routing_hwdep_open;
	hwdep->ops.ioctl = msm_pcm_routing_hwdep_ioctl;
	hwdep->ops.release = msm_pcm_routing_hwdep_release;
#ifdef CONFIG_COMPAT
	hwdep->ops.ioctl_compat = msm_pcm_routing_hwdep_compat_ioctl;
#endif
	return msm_dts_eagle_pcm_new(runtime);
}
#endif
