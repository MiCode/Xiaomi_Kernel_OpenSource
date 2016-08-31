/*
 * drivers/video/tegra/dc/edid.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
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


#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>

#include "edid.h"

struct tegra_edid_pvt {
	struct kref			refcnt;
	struct tegra_edid_hdmi_eld	eld;
	bool				support_stereo;
	bool				support_underscan;
	bool				support_audio;
	int			        hdmi_vic_len;
	u8			        hdmi_vic[7];
	/* Note: dc_edid must remain the last member */
	struct tegra_dc_edid		dc_edid;
};

struct tegra_edid {
	struct i2c_client	*client;
	struct i2c_board_info	info;
	int			bus;

	struct tegra_edid_pvt	*data;

	struct mutex		lock;
};

#if defined(DEBUG) || defined(CONFIG_DEBUG_FS)
static int tegra_edid_show(struct seq_file *s, void *unused)
{
	struct tegra_edid *edid = s->private;
	struct tegra_dc_edid *data;
	u8 *buf;
	int i;

	data = tegra_edid_get_data(edid);
	if (!data) {
		seq_printf(s, "No EDID\n");
		return 0;
	}

	buf = data->buf;

	for (i = 0; i < data->len; i++) {
		if (i % 16 == 0)
			seq_printf(s, "edid[%03x] =", i);

		seq_printf(s, " %02x", buf[i]);

		if (i % 16 == 15)
			seq_printf(s, "\n");
	}

	tegra_edid_put_data(data);

	return 0;
}
#endif

#ifdef CONFIG_DEBUG_FS
static int tegra_edid_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_edid_show, inode->i_private);
}

static const struct file_operations tegra_edid_debug_fops = {
	.open		= tegra_edid_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void tegra_edid_debug_add(struct tegra_edid *edid)
{
	char name[] = "edidX";

	snprintf(name, sizeof(name), "edid%1d", edid->bus);
	debugfs_create_file(name, S_IRUGO, NULL, edid, &tegra_edid_debug_fops);
}
#else
void tegra_edid_debug_add(struct tegra_edid *edid)
{
}
#endif

#ifdef DEBUG
static char tegra_edid_dump_buff[16 * 1024];

static void tegra_edid_dump(struct tegra_edid *edid)
{
	struct seq_file s;
	int i;
	char c;

	memset(&s, 0x0, sizeof(s));

	s.buf = tegra_edid_dump_buff;
	s.size = sizeof(tegra_edid_dump_buff);
	s.private = edid;

	tegra_edid_show(&s, NULL);

	i = 0;
	while (i < s.count ) {
		if ((s.count - i) > 256) {
			c = s.buf[i + 256];
			s.buf[i + 256] = 0;
			printk("%s", s.buf + i);
			s.buf[i + 256] = c;
		} else {
			printk("%s", s.buf + i);
		}
		i += 256;
	}
}
#else
static void tegra_edid_dump(struct tegra_edid *edid)
{
}
#endif

int tegra_edid_read_block(struct tegra_edid *edid, int block, u8 *data)
{
	u8 block_buf[] = {block >> 1};
	u8 cmd_buf[] = {(block & 0x1) * 128};
	int status;
	u8 checksum = 0;
	u8 i;
	struct i2c_msg msg[] = {
		{
			.addr = 0x30,
			.flags = 0,
			.len = 1,
			.buf = block_buf,
		},
		{
			.addr = 0x50,
			.flags = 0,
			.len = 1,
			.buf = cmd_buf,
		},
		{
			.addr = 0x50,
			.flags = I2C_M_RD,
			.len = 128,
			.buf = data,
		}};
	struct i2c_msg *m;
	int msg_len;

	if (block > 1) {
		msg_len = 3;
		m = msg;
	} else {
		msg_len = 2;
		m = &msg[1];
	}

	status = i2c_transfer(edid->client->adapter, m, msg_len);

	if (status < 0)
		return status;

	if (status != msg_len)
		return -EIO;

	for (i = 0; i < 128; i++)
		checksum += data[i];
	if (checksum != 0) {
		pr_err("%s: checksum failed\n", __func__);
		return -EIO;
	}

	return 0;
}

int tegra_edid_parse_ext_block(const u8 *raw, int idx,
			       struct tegra_edid_pvt *edid)
{
	const u8 *ptr;
	u8 tmp;
	u8 code;
	int len;
	int i;
	bool basic_audio = false;

	if (!edid) {
		pr_err("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	edid->support_audio = 0;
	ptr = &raw[0];

	/* If CEA 861 block get info for eld struct */
	if (ptr) {
		if (*ptr <= 3)
			edid->eld.eld_ver = 0x02;
		edid->eld.cea_edid_ver = ptr[1];

		/* check for basic audio support in CEA 861 block */
		if(raw[3] & (1<<6)) {
			/* For basic audio, set spk_alloc to Left+Right.
			 * If there is a Speaker Alloc block this will
			 * get over written with that value */
			basic_audio = true;
			edid->support_audio = 1;
		}
	}

	if (raw[3] & 0x80)
		edid->support_underscan = 1;
	else
		edid->support_underscan = 0;

	ptr = &raw[4];

	while (ptr < &raw[idx]) {
		tmp = *ptr;
		len = tmp & 0x1f;

		/* HDMI Specification v1.4a, section 8.3.2:
		 * see Table 8-16 for HDMI VSDB format.
		 * data blocks have tags in top 3 bits:
		 * tag code 2: video data block
		 * tag code 3: vendor specific data block
		 */
		code = (tmp >> 5) & 0x7;
		switch (code) {
		case 1:
		{
			edid->eld.sad_count = len;
			edid->eld.conn_type = 0x00;
			edid->eld.support_hdcp = 0x00;
			for (i = 0; (i < len) && (i < ELD_MAX_SAD); i ++)
				edid->eld.sad[i] = ptr[i + 1];
			len++;
			ptr += len; /* adding the header */
			/* Got an audio data block so enable audio */
			if(basic_audio == true)
				edid->eld.spk_alloc = 1;
			break;
		}
		/* case 2 is commented out for now */
		case 3:
		{
			int j = 0;

			if ((ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {
				edid->eld.port_id[0] = ptr[4];
				edid->eld.port_id[1] = ptr[5];
			}
			if ((len >= 8) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {
				j = 8;
				tmp = ptr[j++];
				/* HDMI_Video_present? */
				if (tmp & 0x20) {
					/* Latency_Fields_present? */
					if (tmp & 0x80)
						j += 2;
					/* I_Latency_Fields_present? */
					if (tmp & 0x40)
						j += 2;
					/* 3D_present? */
					if (j <= len && (ptr[j] & 0x80))
						edid->support_stereo = 1;
					/* HDMI_VIC_LEN */
					if (++j <= len && (ptr[j] & 0xe0)) {
						int k = 0;
						edid->hdmi_vic_len = ptr[j] >> 5;
						for (k = 0; k < edid->hdmi_vic_len; k++)
						    edid->hdmi_vic[k] = ptr[j+k+1];
					}
				}
			}
			if ((len > 5) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {

				edid->eld.support_ai = (ptr[6] & 0x80);
			}

			if ((len > 9) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {

				edid->eld.aud_synch_delay = ptr[10];
			}
			len++;
			ptr += len; /* adding the header */
			break;
		}
		case 4:
		{
			edid->eld.spk_alloc = ptr[1];
			len++;
			ptr += len; /* adding the header */
			break;
		}
		default:
			len++; /* len does not include header */
			ptr += len;
			break;
		}
	}

	return 0;
}

int tegra_edid_mode_support_stereo(struct fb_videomode *mode)
{
	if (!mode)
		return 0;

	if (mode->xres == 1280 &&
		mode->yres == 720 &&
		((mode->refresh == 60) || (mode->refresh == 50)))
		return 1;

	if (mode->xres == 1920 && mode->yres == 1080 && mode->refresh == 24)
		return 1;

	return 0;
}

static void data_release(struct kref *ref)
{
	struct tegra_edid_pvt *data =
		container_of(ref, struct tegra_edid_pvt, refcnt);
	vfree(data);
}

int tegra_edid_get_monspecs_test(struct tegra_edid *edid,
			struct fb_monspecs *specs, unsigned char *edid_ptr)
{
	int i, j, ret;
	int extension_blocks;
	struct tegra_edid_pvt *new_data, *old_data;
	u8 *data;

	new_data = vmalloc(SZ_32K + sizeof(struct tegra_edid_pvt));
	if (!new_data)
		return -ENOMEM;

	kref_init(&new_data->refcnt);

	new_data->support_stereo = 0;
	new_data->support_underscan = 0;

	data = new_data->dc_edid.buf;
	memcpy(data, edid_ptr, 128);

	memset(specs, 0x0, sizeof(struct fb_monspecs));
	memset(&new_data->eld, 0x0, sizeof(new_data->eld));
	fb_edid_to_monspecs(data, specs);
	if (specs->modedb == NULL) {
		ret = -EINVAL;
		goto fail;
	}

	memcpy(new_data->eld.monitor_name, specs->monitor,
					sizeof(specs->monitor));

	new_data->eld.mnl = strlen(new_data->eld.monitor_name) + 1;
	new_data->eld.product_id[0] = data[0x8];
	new_data->eld.product_id[1] = data[0x9];
	new_data->eld.manufacture_id[0] = data[0xA];
	new_data->eld.manufacture_id[1] = data[0xB];

	extension_blocks = data[0x7e];
	for (i = 1; i <= extension_blocks; i++) {
		memcpy(data+128, edid_ptr+128, 128);

		if (data[i * 128] == 0x2) {
			fb_edid_add_monspecs(data + i * 128, specs);

			tegra_edid_parse_ext_block(data + i * 128,
					data[i * 128 + 2], new_data);

			if (new_data->support_stereo) {
				for (j = 0; j < specs->modedb_len; j++) {
					if (tegra_edid_mode_support_stereo(
						&specs->modedb[j]))
						specs->modedb[j].vmode |=
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
						FB_VMODE_STEREO_FRAME_PACK;
#else
						FB_VMODE_STEREO_LEFT_RIGHT;
#endif
				}
			}
		}
	}

	new_data->dc_edid.len = i * 128;

	mutex_lock(&edid->lock);
	old_data = edid->data;
	edid->data = new_data;
	mutex_unlock(&edid->lock);

	if (old_data)
		kref_put(&old_data->refcnt, data_release);

	tegra_edid_dump(edid);
	return 0;
fail:
	vfree(new_data);
	return ret;
}

int tegra_edid_get_monspecs(struct tegra_edid *edid, struct fb_monspecs *specs)
{
	int i;
	int j;
	int ret;
	int extension_blocks;
	struct tegra_edid_pvt *new_data, *old_data;
	u8 *data;

	new_data = vmalloc(SZ_32K + sizeof(struct tegra_edid_pvt));
	if (!new_data)
		return -ENOMEM;

	kref_init(&new_data->refcnt);

	new_data->support_stereo = 0;

	data = new_data->dc_edid.buf;

	ret = tegra_edid_read_block(edid, 0, data);
	if (ret)
		goto fail;

	memset(specs, 0x0, sizeof(struct fb_monspecs));
	memset(&new_data->eld, 0x0, sizeof(new_data->eld));
	fb_edid_to_monspecs(data, specs);
	if (specs->modedb == NULL) {
		ret = -EINVAL;
		goto fail;
	}
	memcpy(new_data->eld.monitor_name, specs->monitor, sizeof(specs->monitor));
	new_data->eld.mnl = strlen(new_data->eld.monitor_name) + 1;
	new_data->eld.product_id[0] = data[0x8];
	new_data->eld.product_id[1] = data[0x9];
	new_data->eld.manufacture_id[0] = data[0xA];
	new_data->eld.manufacture_id[1] = data[0xB];

	extension_blocks = data[0x7e];

	for (i = 1; i <= extension_blocks; i++) {
		ret = tegra_edid_read_block(edid, i, data + i * 128);
		if (ret < 0)
			goto fail;

		if (data[i * 128] == 0x2) {
			fb_edid_add_monspecs(data + i * 128, specs);

			tegra_edid_parse_ext_block(data + i * 128,
					data[i * 128 + 2], new_data);

			if (new_data->support_stereo) {
				for (j = 0; j < specs->modedb_len; j++) {
					if (tegra_edid_mode_support_stereo(
						&specs->modedb[j]))
						specs->modedb[j].vmode |=
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
						FB_VMODE_STEREO_FRAME_PACK;
#else
						FB_VMODE_STEREO_LEFT_RIGHT;
#endif
				}
			}

			if (new_data->hdmi_vic_len > 0) {
				int k;
				int l = specs->modedb_len;
				struct fb_videomode *m;
				m = kzalloc((specs->modedb_len + new_data->hdmi_vic_len) *
				    sizeof(struct fb_videomode), GFP_KERNEL);
				if (!m)
				    break;
				memcpy(m, specs->modedb, specs->modedb_len *
				        sizeof(struct fb_videomode));
				for (k = 0; k < new_data->hdmi_vic_len; k++) {
				    unsigned vic = new_data->hdmi_vic[k];
				    if (vic >= HDMI_EXT_MODEDB_SIZE) {
				        pr_warning("Unsupported HDMI VIC %d, ignoring\n", vic);
				        continue;
				    }
				    memcpy(&m[l], &hdmi_ext_modes[vic], sizeof(m[l]));
				    l++;
				}
				kfree(specs->modedb);
				specs->modedb = m;
				specs->modedb_len = specs->modedb_len + new_data->hdmi_vic_len;
			}
		}
	}

	new_data->dc_edid.len = i * 128;

	mutex_lock(&edid->lock);
	old_data = edid->data;
	edid->data = new_data;
	mutex_unlock(&edid->lock);

	if (old_data)
		kref_put(&old_data->refcnt, data_release);

	tegra_edid_dump(edid);
	return 0;

fail:
	vfree(new_data);
	return ret;
}

int tegra_edid_audio_supported(struct tegra_edid *edid)
{
	if ((!edid) || (!edid->data))
		return 0;

	return edid->data->support_audio;
}

int tegra_edid_underscan_supported(struct tegra_edid *edid)
{
	if ((!edid) || (!edid->data))
		return 0;

	return edid->data->support_underscan;
}

int tegra_edid_get_eld(struct tegra_edid *edid, struct tegra_edid_hdmi_eld *elddata)
{
	if (!elddata || !edid->data)
		return -EFAULT;

	memcpy(elddata,&edid->data->eld,sizeof(struct tegra_edid_hdmi_eld));

	return 0;
}

struct tegra_edid *tegra_edid_create(int bus)
{
	struct tegra_edid *edid;
	struct i2c_adapter *adapter;
	int err;

	edid = kzalloc(sizeof(struct tegra_edid), GFP_KERNEL);
	if (!edid)
		return ERR_PTR(-ENOMEM);

	mutex_init(&edid->lock);
	strlcpy(edid->info.type, "tegra_edid", sizeof(edid->info.type));
	edid->bus = bus;
	edid->info.addr = 0x50;
	edid->info.platform_data = edid;

	adapter = i2c_get_adapter(bus);
	if (!adapter) {
		pr_err("can't get adpater for bus %d\n", bus);
		err = -EBUSY;
		goto free_edid;
	}

	edid->client = i2c_new_device(adapter, &edid->info);
	i2c_put_adapter(adapter);

	if (!edid->client) {
		pr_err("can't create new device\n");
		err = -EBUSY;
		goto free_edid;
	}

	tegra_edid_debug_add(edid);

	return edid;

free_edid:
	kfree(edid);

	return ERR_PTR(err);
}

void tegra_edid_destroy(struct tegra_edid *edid)
{
	i2c_release_client(edid->client);
	if (edid->data)
		kref_put(&edid->data->refcnt, data_release);
	kfree(edid);
}

struct tegra_dc_edid *tegra_edid_get_data(struct tegra_edid *edid)
{
	struct tegra_edid_pvt *data;

	mutex_lock(&edid->lock);
	data = edid->data;
	if (data)
		kref_get(&data->refcnt);
	mutex_unlock(&edid->lock);

	return data ? &data->dc_edid : NULL;
}

void tegra_edid_put_data(struct tegra_dc_edid *data)
{
	struct tegra_edid_pvt *pvt;

	if (!data)
		return;

	pvt = container_of(data, struct tegra_edid_pvt, dc_edid);

	kref_put(&pvt->refcnt, data_release);
}

static const struct i2c_device_id tegra_edid_id[] = {
        { "tegra_edid", 0 },
        { }
};

MODULE_DEVICE_TABLE(i2c, tegra_edid_id);

static struct i2c_driver tegra_edid_driver = {
        .id_table = tegra_edid_id,
        .driver = {
                .name = "tegra_edid",
        },
};

static int __init tegra_edid_init(void)
{
        return i2c_add_driver(&tegra_edid_driver);
}

static void __exit tegra_edid_exit(void)
{
        i2c_del_driver(&tegra_edid_driver);
}

module_init(tegra_edid_init);
module_exit(tegra_edid_exit);
