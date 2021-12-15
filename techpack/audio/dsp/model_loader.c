/**
 * Copyright 2020 Mi
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <dsp/model_loader.h>
#include <dsp/q6afe-v2.h>
#include <dsp/msm_audio_ion.h>

struct qdsp_data_state g_qdsp_data_state;
struct afe_audio_client  *g_client = NULL;
struct voice_sound_model g_sm = {0};


int voice_snd_model_buf_free(struct afe_audio_client *client, struct voice_sound_model *sm)
{
	int rc = 0;

	mutex_lock(&client->cmd_lock);
	pr_debug("%s: endter\n",__func__);
	if (NULL == sm || NULL == client)
		return -EFAULT;

	if (sm->mem_map_handle != 0) {
		rc = afe_cmd_memory_unmap(client->mem_map_handle);
		if (rc)
			pr_err("%s: CMD Memory_unmap_regions failed %d\n",
				__func__, rc);
	}
	if(sm->data) {
		msm_audio_ion_free(sm->dma_buf);
	}
	sm->dma_buf = NULL;
	sm->data = NULL;
	sm->mem_map_handle = 0;
	sm->phys = 0;
	mutex_unlock(&client->cmd_lock);

	pr_debug("%s: exit\n",__func__);
	return rc;
}

/**
 * voice_snd_model_buf_alloc -
 *       Allocate memory for voice model
 *
 * @client: afe client handle
 * @sm: sound model param info
 * @len: size of sound model
 *
 * Returns 0 on success or error on failure
 */
int voice_snd_model_buf_alloc(struct afe_audio_client *client,  struct voice_sound_model *sm, size_t len)
{
	size_t total_mem = 0;
	int rc = -EINVAL;

	mutex_lock(&client->cmd_lock);

	pr_debug("%s: enter\n",__func__);

	if (NULL == sm || NULL == client)
		return -EFAULT;

	if (!sm->data) {
		sm->size = len;
		total_mem = PAGE_ALIGN(len);
		pr_debug("%s: voice sm size %zd Total mem %zd\n",
				 __func__, len, total_mem);
		rc = msm_audio_ion_alloc(&sm->dma_buf, total_mem,
						&sm->phys, &len, &sm->data);
		if (rc) {
			pr_err("%s: Audio ION alloc is failed, rc = %d size, %zd\n",
				__func__, rc, total_mem);
			goto fail;
		}
	} else {
		pr_err("%s: voice sound model busy\n", __func__);
		rc = -EBUSY;
		goto fail;
	}

	rc = afe_memory_map(sm->phys, len, client);
	if (rc) {
		pr_err("fail to map memory to DSP\n");
		goto fail;
	}
	sm->mem_map_handle = client->mem_map_handle;
	pr_debug(" mamp handle %x, address  %x\n",sm->mem_map_handle, sm->phys);
	mutex_unlock(&client->cmd_lock);

	return rc;

fail:
	mutex_unlock(&client->cmd_lock);
	voice_snd_model_buf_free(client, sm);
	return rc;
}

int voice_send_snd_model(struct model_info *model, struct afe_audio_client *client, struct voice_sound_model *sm) {
	int ret = 0;

	pr_debug("%s: endter\n",__func__);

	if (!sm || !model || !sm->data || !model->data || !client)
		return -EFAULT;

	if (copy_from_user(sm->data, model->data, model->len)) {
		pr_err("%s :Failed to copy audio from user buffer\n",
			__func__);
		ret = -EFAULT;
		return ret;
	}

	ret = afe_send_data(sm->phys, client->mem_map_handle, model->len);
	if (ret < 0) {
		pr_err("%s :Failed to send buffer\n",
			__func__);
	}
	return ret;
}

int qdsp_voice_model_data_get(struct snd_kcontrol *kcontrol,
                                  struct snd_ctl_elem_value *ucontrol) {
        memset(ucontrol->value.bytes.data, 0, sizeof(struct model_info));
        ucontrol->value.bytes.data[0] = (char)g_qdsp_data_state.voice_model_state;
        return 0;
}

int qdsp_voice_model_data_put(struct snd_kcontrol *kcontrol,
                                  struct snd_ctl_elem_value *ucontrol) {
        struct model_info  info;

	memcpy(&info, (struct model_info*)ucontrol->value.bytes.data, sizeof(struct model_info));
        pr_debug("%s: data ptr is %x, data len is %d\n", __func__,info.data, info.len);

	if (!g_client)
		g_client = q6afe_audio_client_alloc(NULL);
	if (!g_client) {
		pr_debug("%s: Could not allocate memory\n", __func__);
		return -ENOMEM;
	}

        voice_snd_model_buf_alloc(g_client, &g_sm, info.len);
        voice_send_snd_model(&info, g_client, &g_sm);
	voice_snd_model_buf_free(g_client, &g_sm);

	if (g_client) {
		q6afe_audio_client_free(g_client);
		g_client = NULL;
	}

        return 0;
}

static const struct snd_kcontrol_new qdsp_send_data_mixer_controls[] = {
        SND_SOC_BYTES_EXT("Qdsp voice model Data",
        sizeof(struct model_info),
        qdsp_voice_model_data_get,
        qdsp_voice_model_data_put),
};

unsigned int send_data_add_component_controls(void *component)
{
        const unsigned int num_controls =
                ARRAY_SIZE(qdsp_send_data_mixer_controls);
        if (component != NULL) {
                snd_soc_add_component_controls(
                        (struct snd_soc_component *)component,
                        qdsp_send_data_mixer_controls,
                        num_controls);
        } else {
                pr_err("pointer is NULL");
        }

        return num_controls;
}

EXPORT_SYMBOL(send_data_add_component_controls);

