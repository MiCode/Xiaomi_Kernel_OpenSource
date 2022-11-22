// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include "scp_audio_ipi.h"
#include "scp.h"
#include "mtk-scp-audio-base.h"
#include "mtk-scp-audio-pcm.h"
#include "mtk-scp-audio-mem-control.h"

#define SND_SCP_AUDIO_DTS_SIZE (4)
#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)
static struct mtk_scp_audio_base *local_scp_audio;

static const struct snd_pcm_hardware scp_audio_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = MTK_PCM_FORMATS,
	.rates = MTK_PCM_RATES,
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 4096,
	.buffer_bytes_max = 4 * 48 * 1024,
	.fifo_size = 0,
};

int set_scp_audio_base(struct mtk_scp_audio_base *pScpAudio)
{
	if (pScpAudio == NULL)
		pr_info("%s pScpAudio == NULL", __func__);

	local_scp_audio = pScpAudio;
	return 0;
}

void *get_scp_audio_base(void)
{
	if (local_scp_audio == NULL)
		pr_warn("%s local_scp_audio == NULL", __func__);
	return local_scp_audio;
}
EXPORT_SYMBOL(get_scp_audio_base);

/* user-space event notify */
static int scp_user_event_notify(struct notifier_block *nb,
				 unsigned long event, void *ptr)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct device *dev = scp_audio->dev;
	int ret = 0;

	if (!dev)
		return NOTIFY_DONE;

	switch (event) {
	case SCP_EVENT_STOP:
		pr_info("%s(), SCP_EVENT_STOP\n", __func__);
		ret = kobject_uevent(&dev->kobj, KOBJ_OFFLINE);
		break;
	case SCP_EVENT_READY:
		pr_info("%s(), SCP_EVENT_READY\n", __func__);
		ret = kobject_uevent(&dev->kobj, KOBJ_ONLINE);
		break;
	default:
		pr_info("%s, ignore event %lu", __func__, event);
		break;
	}

	if (ret)
		pr_info("%s, uevnet(%lu) fail, ret %d", __func__, event, ret);

	return NOTIFY_OK;
}

struct notifier_block scp_uevent_notifier = {
	.notifier_call = scp_user_event_notify,
};

static struct notifier_block scp_audio_recover_notifier = {
	.notifier_call = scp_audio_pcm_recover_event,
};

static int scp_audio_dev_probe(struct platform_device *pdev)
{
	struct mtk_scp_audio_base *scp_audio;
	struct scp_audio_task_attr task_attr;
	int ret = 0;
	struct device *dev;

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "scp_spk_process_enable",
					 (unsigned int *)&task_attr,
					 SND_SCP_AUDIO_DTS_SIZE);
	if (ret != 0) {
		pr_info("%s cannot get spkProcess attr\n", __func__);
		init_scp_spk_process_enable(0);
	} else {
		pr_info("%s enable=0x%x, 0x%x 0x%x 0x%x\n", __func__,
			task_attr.default_enable,
			task_attr.dl_memif,
			task_attr.ul_memif,
			task_attr.ref_memif);
		init_scp_spk_process_enable(task_attr.default_enable);
	}

	scp_audio = devm_kzalloc(&pdev->dev, sizeof(struct mtk_scp_audio_base),
				 GFP_KERNEL);
	if (!scp_audio)
		return -ENOMEM;

	scp_audio->dl_memif = task_attr.dl_memif;
	scp_audio->ul_memif = task_attr.ul_memif;
	scp_audio->ref_memif = task_attr.ref_memif;

	/*  register scp audio dai driver*/
	scp_audio_dai_register(pdev, scp_audio);
	scp_audio->scp_audio_hardware = &scp_audio_hardware;

	scp_audio->dev = &pdev->dev;
	dev = scp_audio->dev;

	scp_audio->request_dram_resource = scp_audio_dram_request;
	scp_audio->release_dram_resource = scp_audio_dram_release;

	scp_audio->ipi_ops.ipi_message_callback = scp_audio_pcm_ipi_recv;
	scp_audio->ipi_ops.ipi_handler = scp_aud_ipi_handler;

	platform_set_drvdata(pdev, scp_audio);
	pm_runtime_enable(&pdev->dev);
	set_scp_audio_base(scp_audio);
	mtk_scp_audio_init_mem();

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "snd_scp_audio");
		pdev->name = pdev->dev.kobj.name;
		pr_info("dev name %s\n", pdev->name);
	}

	ret = snd_soc_register_component(&pdev->dev,
					 &mtk_scp_audio_pcm_platform,
					 scp_audio->dai_drivers,
					 scp_audio->num_dai_drivers);
	if (ret) {
		dev_info(&pdev->dev, "%s() err_platform: %d\n", __func__, ret);
		return -1;
	}

	scp_A_register_notify(&scp_audio_recover_notifier);
	scp_A_register_notify(&scp_uevent_notifier);

	pr_info("%s done, ret:%d\n", __func__, ret);
	return 0;
}

static const struct of_device_id scp_audio_dt_match[] = {
	{ .compatible = "mediatek,snd_scp_audio", },
	{},
};
MODULE_DEVICE_TABLE(of, scp_audio_dt_match);

static struct platform_driver scp_audio_driver = {
	.driver = {
		   .name = "snd_scp_audio",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_audio_dt_match,
	},
	.probe = scp_audio_dev_probe,
};

module_platform_driver(scp_audio_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC platform driver for scp audio");
MODULE_AUTHOR("Zhixiong Wang <zhixiong.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
