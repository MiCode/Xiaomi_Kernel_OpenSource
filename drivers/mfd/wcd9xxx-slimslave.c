/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>

#define WCD_SLIM_INVALID_SAMPLE_RATE(r) \
	((r/4000)%4)

struct wcd9xxx_slim_master_prop {
	u16 chanh;
	u16 grph;
	u32 ph1;
	struct slim_ch prop;
};

struct slim_session {
	struct completion sb_comp;
	void *handle;
};

struct wcd9xxx_master_cfg {
	struct wcd9xxx_slim_master_prop *slim_cfg;
	u16 sample_rate;
	u16 sample_size;
	u32 ref_count;
	struct slim_session slim_s;
	struct mutex lock;
};
static struct wcd9xxx_master_cfg slim_tx_master;

struct wcd9xxx_slim_sch {
	u16 rx_port_ch_reg_base;
	u16 port_tx_cfg_reg_base;
	u16 port_rx_cfg_reg_base;
};

static struct wcd9xxx_slim_sch sh_ch;

static int wcd9xxx_alloc_slim_sh_ch(struct wcd9xxx *wcd9xxx,
				    u8 wcd9xxx_pgd_la, u32 cnt,
				    struct wcd9xxx_ch *channels, u32 path);

static int wcd9xxx_dealloc_slim_sh_ch(struct slim_device *slim,
				      u32 cnt, struct wcd9xxx_ch *channels);

static int wcd9xxx_configure_ports(struct wcd9xxx *wcd9xxx)
{
	if (wcd9xxx->codec_type->slim_slave_type ==
	    WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA) {
		sh_ch.rx_port_ch_reg_base = 0x180;
		sh_ch.port_rx_cfg_reg_base = 0x040;
		sh_ch.port_tx_cfg_reg_base = 0x040;
	} else {
		sh_ch.rx_port_ch_reg_base =
			0x180 - (TAIKO_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS * 4);
		sh_ch.port_rx_cfg_reg_base =
			0x040 - TAIKO_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS;
		sh_ch.port_tx_cfg_reg_base = 0x050;
	}

	return 0;
}


int wcd9xxx_init_slimslave(struct wcd9xxx *wcd9xxx, u8 wcd9xxx_pgd_la,
			   unsigned int tx_num, unsigned int *tx_slot,
			   unsigned int rx_num, unsigned int *rx_slot)
{
	int ret = 0;
	int i;

	ret = wcd9xxx_configure_ports(wcd9xxx);
	if (ret) {
		pr_err("%s: Failed to configure register address offset\n",
		       __func__);
		goto err;
	}

	if (wcd9xxx->rx_chs) {
		wcd9xxx->num_rx_port = rx_num;
		for (i = 0; i < rx_num; i++) {
			wcd9xxx->rx_chs[i].ch_num = rx_slot[i];
			INIT_LIST_HEAD(&wcd9xxx->rx_chs[i].list);
		}
		ret = wcd9xxx_alloc_slim_sh_ch(wcd9xxx, wcd9xxx_pgd_la,
						wcd9xxx->num_rx_port,
						wcd9xxx->rx_chs,
						SLIM_SINK);
		if (ret) {
			pr_err("%s: Failed to alloc %d rx slimbus channels\n",
				__func__, wcd9xxx->num_rx_port);
			kfree(wcd9xxx->rx_chs);
			wcd9xxx->rx_chs = NULL;
			wcd9xxx->num_rx_port = 0;
		}
	} else {
		pr_err("Not able to allocate memory for %d slimbus rx ports\n",
			wcd9xxx->num_rx_port);
	}

	if (wcd9xxx->tx_chs) {
		wcd9xxx->num_tx_port = tx_num;
		for (i = 0; i < tx_num; i++) {
			wcd9xxx->tx_chs[i].ch_num = tx_slot[i];
			INIT_LIST_HEAD(&wcd9xxx->tx_chs[i].list);
		}
		ret = wcd9xxx_alloc_slim_sh_ch(wcd9xxx, wcd9xxx_pgd_la,
						wcd9xxx->num_tx_port,
						wcd9xxx->tx_chs,
						SLIM_SRC);
		if (ret) {
			pr_err("%s: Failed to alloc %d tx slimbus channels\n",
				__func__, wcd9xxx->num_tx_port);
			kfree(wcd9xxx->tx_chs);
			wcd9xxx->tx_chs = NULL;
			wcd9xxx->num_tx_port = 0;
		}
	} else {
		pr_err("Not able to allocate memory for %d slimbus tx ports\n",
			wcd9xxx->num_tx_port);
	}
	mutex_init(&slim_tx_master.lock);
	init_completion(&slim_tx_master.slim_s.sb_comp);
	return 0;
err:
	return ret;
}

int wcd9xxx_deinit_slimslave(struct wcd9xxx *wcd9xxx)
{
	if (wcd9xxx->num_rx_port) {
		wcd9xxx_dealloc_slim_sh_ch(wcd9xxx->slim,
					wcd9xxx->num_rx_port,
					wcd9xxx->rx_chs);
		wcd9xxx->num_rx_port = 0;
	}
	if (wcd9xxx->num_tx_port) {
		wcd9xxx_dealloc_slim_sh_ch(wcd9xxx->slim,
					wcd9xxx->num_tx_port,
					wcd9xxx->tx_chs);
		wcd9xxx->num_tx_port = 0;
	}
	return 0;
}


static int wcd9xxx_alloc_slim_sh_ch(struct wcd9xxx *wcd9xxx,
				    u8 wcd9xxx_pgd_la, u32 cnt,
				    struct wcd9xxx_ch *channels, u32 path)
{
	int ret = 0;
	u32 ch_idx ;

	/* The slimbus channel allocation seem take longer time
	 * so do the allocation up front to avoid delay in start of
	 * playback
	 */
	pr_debug("%s: pgd_la[%d]\n", __func__, wcd9xxx_pgd_la);
	for (ch_idx = 0; ch_idx < cnt; ch_idx++) {
		ret = slim_get_slaveport(wcd9xxx_pgd_la,
					channels[ch_idx].port,
					&channels[ch_idx].sph, path);
		pr_debug("%s: pgd_la[%d] channels[%d].port[%d]\n"
			"channels[%d].sph[%d] path[%d]\n",
			__func__, wcd9xxx_pgd_la, ch_idx,
			channels[ch_idx].port,
			ch_idx, channels[ch_idx].sph, path);
		if (ret < 0) {
			pr_err("%s: slave port failure id[%d] ret[%d]\n",
				__func__, channels[ch_idx].ch_num, ret);
			goto err;
		}

		ret = slim_query_ch(wcd9xxx->slim,
				    channels[ch_idx].ch_num,
				    &channels[ch_idx].ch_h);
		if (ret < 0) {
			pr_err("%s: slim_query_ch failed ch-num[%d] ret[%d]\n",
				__func__, channels[ch_idx].ch_num, ret);
			goto err;
		}
	}
err:
	return ret;
}

static int wcd9xxx_dealloc_slim_sh_ch(struct slim_device *slim,
			u32 cnt, struct wcd9xxx_ch *channels)
{
	int idx = 0;
	int ret = 0;
	/* slim_dealloc_ch */
	for (idx = 0; idx < cnt; idx++) {
		ret = slim_dealloc_ch(slim, channels[idx].ch_h);
		if (ret < 0) {
			pr_err("%s: slim_dealloc_ch fail ret[%d] ch_h[%d]\n",
				__func__, ret, channels[idx].ch_h);
		}
	}
	return ret;
}

/* Enable slimbus slave device for RX path */
int wcd9xxx_cfg_slim_sch_rx(struct wcd9xxx *wcd9xxx,
			    struct list_head *wcd9xxx_ch_list,
			    unsigned int rate, unsigned int bit_width,
			    u16 *grph)
{
	u8 ch_cnt = 0;
	u16 ch_h[SLIM_MAX_RX_PORTS] = {0};
	u8  payload = 0;
	u16 codec_port = 0;
	int ret;
	struct slim_ch prop;
	struct wcd9xxx_ch *rx;

	/* Configure slave interface device */

	list_for_each_entry(rx, wcd9xxx_ch_list, list) {
		payload |= 1 << rx->shift;
		ch_h[ch_cnt] = rx->ch_h;
		ch_cnt++;
		pr_debug("list ch->ch_h %d ch->sph %d\n", rx->ch_h, rx->sph);
	}
	pr_debug("%s: ch_cnt[%d] rate=%d WATER_MARK_VAL %d\n",
		 __func__, ch_cnt, rate, WATER_MARK_VAL);
	/* slim_define_ch api */
	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = bit_width;

	pr_debug("Before slim_define_ch:\n"
		 "ch_cnt %d,ch_h[0] %d ch_h[1] %d, grph %d\n",
		 ch_cnt, ch_h[0], ch_h[1], *grph);
	ret = slim_define_ch(wcd9xxx->slim, &prop, ch_h, ch_cnt,
			     true, grph);
	if (ret < 0) {
		pr_err("%s: slim_define_ch failed ret[%d]\n",
		       __func__, ret);
		goto err;
	}

	list_for_each_entry(rx, wcd9xxx_ch_list, list) {
		codec_port = rx->port;
		pr_debug("%s: codec_port %d rx 0x%p, payload %d\n"
			 "sh_ch.rx_port_ch_reg_base0 0x%x\n"
			 "sh_ch.port_rx_cfg_reg_base 0x%x\n",
			 __func__, codec_port, rx, payload,
			 sh_ch.rx_port_ch_reg_base,
			sh_ch.port_rx_cfg_reg_base);

		/* look for the valid port range and chose the
		 * payload accordingly
		 */
		/* write to interface device */
		ret = wcd9xxx_interface_reg_write(wcd9xxx,
				SB_PGD_RX_PORT_MULTI_CHANNEL_0(
				sh_ch.rx_port_ch_reg_base, codec_port),
				payload);

		if (ret < 0) {
			pr_err("%s:Intf-dev fail reg[%d] payload[%d] ret[%d]\n",
				__func__,
				SB_PGD_RX_PORT_MULTI_CHANNEL_0(
				sh_ch.rx_port_ch_reg_base, codec_port),
				payload, ret);
			goto err;
		}
		/* configure the slave port for water mark and enable*/
		ret = wcd9xxx_interface_reg_write(wcd9xxx,
				SB_PGD_PORT_CFG_BYTE_ADDR(
				sh_ch.port_rx_cfg_reg_base, codec_port),
				WATER_MARK_VAL);
		if (ret < 0) {
			pr_err("%s:watermark set failure for port[%d] ret[%d]",
				__func__, codec_port, ret);
		}

		ret = slim_connect_sink(wcd9xxx->slim, &rx->sph, 1, rx->ch_h);
		if (ret < 0) {
			pr_err("%s: slim_connect_sink failed ret[%d]\n",
				__func__, ret);
			goto err_close_slim_sch;
		}
	}
	/* slim_control_ch */
	ret = slim_control_ch(wcd9xxx->slim, *grph, SLIM_CH_ACTIVATE,
			      true);
	if (ret < 0) {
		pr_err("%s: slim_control_ch failed ret[%d]\n",
			__func__, ret);
		goto err_close_slim_sch;
	}
	return 0;

err_close_slim_sch:
	/*  release all acquired handles */
	wcd9xxx_close_slim_sch_rx(wcd9xxx, wcd9xxx_ch_list, *grph);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_cfg_slim_sch_rx);

/* Enable slimbus slave device for RX path */
int wcd9xxx_cfg_slim_sch_tx(struct wcd9xxx *wcd9xxx,
			    struct list_head *wcd9xxx_ch_list,
			    unsigned int rate, unsigned int bit_width,
			    u16 *grph)
{
	u16 ch_cnt = 0;
	u16 payload = 0;
	u16 ch_h[SLIM_MAX_TX_PORTS] = {0};
	u16 codec_port;
	int ret = 0;
	struct wcd9xxx_ch *tx;

	struct slim_ch prop;

	list_for_each_entry(tx, wcd9xxx_ch_list, list) {
		payload |= 1 << tx->shift;
		ch_h[ch_cnt] = tx->ch_h;
		ch_cnt++;
	}

	/* slim_define_ch api */
	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;
	ret = slim_define_ch(wcd9xxx->slim, &prop, ch_h, ch_cnt,
			     true, grph);
	if (ret < 0) {
		pr_err("%s: slim_define_ch failed ret[%d]\n",
		       __func__, ret);
		goto err;
	}

	pr_debug("%s: ch_cnt[%d] rate[%d]\n", __func__, ch_cnt, rate);
	list_for_each_entry(tx, wcd9xxx_ch_list, list) {
		codec_port = tx->port;
		pr_debug("%s: codec_port %d tx 0x%p, payload 0x%x\n",
			 __func__, codec_port, tx, payload);
		/* write to interface device */
		ret = wcd9xxx_interface_reg_write(wcd9xxx,
				SB_PGD_TX_PORT_MULTI_CHANNEL_0(codec_port),
				payload & 0x00FF);
		if (ret < 0) {
			pr_err("%s:Intf-dev fail reg[%d] payload[%d] ret[%d]\n",
				__func__,
				SB_PGD_TX_PORT_MULTI_CHANNEL_0(codec_port),
				payload, ret);
			goto err;
		}
		/* ports 8,9 */
		ret = wcd9xxx_interface_reg_write(wcd9xxx,
				SB_PGD_TX_PORT_MULTI_CHANNEL_1(codec_port),
				(payload & 0xFF00)>>8);
		if (ret < 0) {
			pr_err("%s:Intf-dev fail reg[%d] payload[%d] ret[%d]\n",
				__func__,
				SB_PGD_TX_PORT_MULTI_CHANNEL_1(codec_port),
				payload, ret);
			goto err;
		}
		/* configure the slave port for water mark and enable*/
		ret = wcd9xxx_interface_reg_write(wcd9xxx,
				SB_PGD_PORT_CFG_BYTE_ADDR(
				sh_ch.port_tx_cfg_reg_base, codec_port),
				WATER_MARK_VAL);
		if (ret < 0) {
			pr_err("%s:watermark set failure for port[%d] ret[%d]",
				__func__, codec_port, ret);
		}

		ret = slim_connect_src(wcd9xxx->slim, tx->sph, tx->ch_h);

		if (ret < 0) {
			pr_err("%s: slim_connect_src failed ret[%d]\n",
			       __func__, ret);
			goto err;
		}
	}
	/* slim_control_ch */
	ret = slim_control_ch(wcd9xxx->slim, *grph, SLIM_CH_ACTIVATE,
			      true);
	if (ret < 0) {
		pr_err("%s: slim_control_ch failed ret[%d]\n",
			__func__, ret);
		goto err;
	}
	return 0;
err:
	/* release all acquired handles */
	wcd9xxx_close_slim_sch_tx(wcd9xxx, wcd9xxx_ch_list, *grph);
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_cfg_slim_sch_tx);

int wcd9xxx_close_slim_sch_rx(struct wcd9xxx *wcd9xxx,
			      struct list_head *wcd9xxx_ch_list, u16 grph)
{
	u32 sph[SLIM_MAX_RX_PORTS] = {0};
	int ch_cnt = 0 ;
	int ret = 0;
	struct wcd9xxx_ch *rx;

	list_for_each_entry(rx, wcd9xxx_ch_list, list)
		sph[ch_cnt++] = rx->sph;

	pr_debug("%s ch_cht %d, sph[0] %d sph[1] %d\n", __func__, ch_cnt,
		sph[0], sph[1]);

	/* slim_control_ch (REMOVE) */
	pr_debug("%s before slim_control_ch grph %d\n", __func__, grph);
	ret = slim_control_ch(wcd9xxx->slim, grph, SLIM_CH_REMOVE, true);
	if (ret < 0) {
		pr_err("%s: slim_control_ch failed ret[%d]\n", __func__, ret);
		goto err;
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_close_slim_sch_rx);

int wcd9xxx_close_slim_sch_tx(struct wcd9xxx *wcd9xxx,
			      struct list_head *wcd9xxx_ch_list,
			      u16 grph)
{
	u32 sph[SLIM_MAX_TX_PORTS] = {0};
	int ret = 0;
	int ch_cnt = 0 ;
	struct wcd9xxx_ch *tx;

	pr_debug("%s\n", __func__);
	list_for_each_entry(tx, wcd9xxx_ch_list, list)
		sph[ch_cnt++] = tx->sph;

	pr_debug("%s ch_cht %d, sph[0] %d sph[1] %d\n",
		__func__, ch_cnt, sph[0], sph[1]);
	/* slim_control_ch (REMOVE) */
	ret = slim_control_ch(wcd9xxx->slim, grph, SLIM_CH_REMOVE, true);
	if (ret < 0) {
		pr_err("%s: slim_control_ch failed ret[%d]\n",
			__func__, ret);
		goto err;
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_close_slim_sch_tx);

int wcd9xxx_get_slave_port(unsigned int ch_num)
{
	int ret = 0;

	ret = (ch_num - BASE_CH_NUM);
	pr_debug("%s: ch_num[%d] slave port[%d]\n", __func__, ch_num, ret);
	if (ret < 0) {
		pr_err("%s: Error:- Invalid slave port found = %d\n",
			__func__, ret);
		return -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_get_slave_port);

int wcd9xxx_disconnect_port(struct wcd9xxx *wcd9xxx,
			    struct list_head *wcd9xxx_ch_list, u16 grph)
{
	u32 sph[SLIM_MAX_TX_PORTS + SLIM_MAX_RX_PORTS] = {0};
	int ch_cnt = 0 ;
	int ret = 0;
	struct wcd9xxx_ch *slim_ch;

	list_for_each_entry(slim_ch, wcd9xxx_ch_list, list)
		sph[ch_cnt++] = slim_ch->sph;

	/* slim_disconnect_port */
	ret = slim_disconnect_ports(wcd9xxx->slim, sph, ch_cnt);
	if (ret < 0) {
		pr_err("%s: slim_disconnect_ports failed ret[%d]\n",
			__func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_disconnect_port);

/* This function is called with mutex acquired */
int wcd9xxx_rx_vport_validation(u32 port_id,
				struct list_head *codec_dai_list)
{
	struct wcd9xxx_ch *ch;
	int ret = 0;

	pr_debug("%s: port_id %u\n", __func__, port_id);

	list_for_each_entry(ch,
		codec_dai_list, list) {
		pr_debug("%s: ch->port %u\n", __func__, ch->port);
		if (ch->port == port_id) {
			ret = -EINVAL;
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_rx_vport_validation);


/* This function is called with mutex acquired */
int wcd9xxx_tx_vport_validation(u32 table, u32 port_id,
				struct wcd9xxx_codec_dai_data *codec_dai,
				u32 num_codec_dais)
{
	struct wcd9xxx_ch *ch;
	int ret = 0;
	u32 index;
	unsigned long vtable = table;
	u32 size = sizeof(table) * BITS_PER_BYTE;

	pr_debug("%s: vtable 0x%lx port_id %u size %d\n", __func__,
		 vtable, port_id, size);
	for_each_set_bit(index, &vtable, size) {
		if (index < num_codec_dais) {
			list_for_each_entry(ch,
					&codec_dai[index].wcd9xxx_ch_list,
					list) {
				pr_debug("%s: index %u ch->port %u vtable 0x%lx\n",
						__func__, index, ch->port,
						vtable);
				if (ch->port == port_id) {
					pr_err("%s: TX%u is used by AIF%u_CAP Mixer\n",
							__func__, port_id + 1,
							(index + 1)/2);
					ret = -EINVAL;
					break;
				}
			}
		} else {
			pr_err("%s: Invalid index %d of codec dai",
					__func__, index);
			ret = -EINVAL;
		}
		if (ret)
			break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(wcd9xxx_tx_vport_validation);

int wcd9xxx_slim_ch_master_open(struct wcd9xxx *wcd9xxx,
		u16 rate, u16 bit_sz, void **handle, u16 slim_channel)
{
	int rc = 0;
	struct wcd9xxx_master_cfg *tx_master;
	struct wcd9xxx_slim_master_prop *slim_cfg;
	struct slim_ch *prop;

	pr_debug("%s: rate 0x%x bit_sz 0x%x\n",
		 __func__, rate, bit_sz);

	if (wcd9xxx == NULL || handle == NULL) {
		pr_err("%s: Invalid params, wcd9xxx(%p) handle(%p)\n",
			__func__, wcd9xxx, handle);
		return -EINVAL;
	}

	if (WCD_SLIM_INVALID_SAMPLE_RATE(rate)) {
		pr_err("%s: Invalid sample rate 0x%x\n",
			__func__, rate);
		return -EINVAL;
	}

	tx_master = &slim_tx_master;
	tx_master->ref_count++;
	if (tx_master->ref_count > 1) {
		pr_err("%s: Slim channel already open, ref_count = %u\n",
			__func__, tx_master->ref_count);
		return -EINVAL;
	}
	slim_cfg = kzalloc(sizeof(struct wcd9xxx_slim_master_prop),
			   GFP_KERNEL);
	if (slim_cfg == NULL) {
		pr_err("%s:Memory allocation for slim_cfg fail\n",
		       __func__);
		return -ENOMEM;
	}

	mutex_lock(&tx_master->lock);
	tx_master->sample_rate = rate;
	tx_master->sample_size = bit_sz;
	tx_master->slim_cfg = slim_cfg;
	prop = &slim_cfg->prop;
	prop->prot = SLIM_AUTO_ISO;
	prop->baser = SLIM_RATE_4000HZ;
	prop->dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop->auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop->ratem = tx_master->sample_rate/4000;
	prop->sampleszbits = bit_sz;
	rc = slim_query_ch(wcd9xxx->slim, slim_channel,
			   &(slim_cfg->chanh));
	if (rc) {
		pr_err("%s:Err query ch ret:%d, chanh:0x%x\n", __func__ ,
			rc, slim_cfg->chanh);
		goto fail;
	}
	rc = slim_define_ch(wcd9xxx->slim, &(slim_cfg->prop),
			   &(slim_cfg->chanh), 1,
			   true, &(slim_cfg->grph));
	if (rc) {
		pr_err("%s:Err slim_define_ch ch ret:%d, grph:0x%x\n",
		       __func__, rc, slim_cfg->grph);
		goto fail;
	}
	rc = slim_alloc_mgrports(wcd9xxx->slim,
				 SLIM_REQ_DEFAULT, 1,
				 &(slim_cfg->ph1),
				 sizeof(slim_cfg->ph1));
	if (rc) {
		pr_err("%s:alloc mgr port:ret:%d\n", __func__, rc);
		goto fail;
	}
	*handle = (struct wcd9xxx_master_cfg *)tx_master;
	tx_master->slim_s.handle = *handle;
	init_completion(&tx_master->slim_s.sb_comp);
	pr_debug("%s: Handle %p slim_cfg->ph1 %x slim grp handle %x\n"
		 "chanh %x\n", __func__, tx_master->slim_s.handle,
		 tx_master->slim_cfg->ph1, tx_master->slim_cfg->grph,
		 tx_master->slim_cfg->chanh);
	mutex_unlock(&tx_master->lock);
	pr_debug("%s: Handle %p slim_cfg->ph1 %x slim grp\n"
		 "handle %x chanh %x ref count %x\n",
		 __func__, tx_master->slim_s.handle,
		 tx_master->slim_cfg->ph1,
		 tx_master->slim_cfg->grph,
		 tx_master->slim_cfg->chanh,
		 tx_master->ref_count);
	return 0;
fail:
	mutex_unlock(&tx_master->lock);
	kfree(slim_cfg);
	slim_control_ch(wcd9xxx->slim, slim_cfg->grph, SLIM_CH_REMOVE, true);
return rc;
}
EXPORT_SYMBOL(wcd9xxx_slim_ch_master_open);

int wcd9xxx_slim_ch_master_close(struct wcd9xxx *wcd9xxx, void **handle)
{

	int rc = 0, err = 0;
	struct wcd9xxx_master_cfg *tx_master;
	struct wcd9xxx_slim_master_prop *slim_cfg;

	if (wcd9xxx == NULL || handle == NULL) {
		pr_err("%s: Invalid params, wcd9xxx(%p) handle(%p)\n",
			__func__, wcd9xxx, handle);
		return -EINVAL;
	}

	tx_master = &slim_tx_master;
	if (*handle != tx_master->slim_s.handle) {
		pr_err("%s: handle(%p) not matching slim_hdl(%p)\n",
			__func__, *handle, tx_master->slim_s.handle);
		return -EINVAL;
	}

	mutex_lock(&tx_master->lock);
	slim_cfg = tx_master->slim_cfg;
	rc = slim_control_ch(wcd9xxx->slim, slim_cfg->grph,
			     SLIM_CH_REMOVE, true);
	if (rc) {
		pr_err("%s:dealloc mgrport returned :%d\n",
		       __func__, rc);
		err = rc;
	}
	rc  = slim_dealloc_mgrports(wcd9xxx->slim, &slim_cfg->ph1, 1);
	if (rc) {
		pr_err("%s:dealloc mgrport returned :%d\n",
			__func__, rc);
		if (!err)
			err = rc;
	}
	rc = slim_dealloc_ch(wcd9xxx->slim, slim_cfg->chanh);
	if (rc) {
		pr_err("%s:dealloc ch ret:%d, chanh:0x%x\n",
		       __func__, rc, slim_cfg->chanh);
		if (!err)
			err = rc;
	}
	if (err) {
		rc = err;
		goto fail;
	}
	tx_master->slim_s.handle = NULL;
	tx_master->ref_count--;
	*handle = NULL;
fail:
	mutex_unlock(&tx_master->lock);
	kfree(tx_master->slim_cfg);
	pr_err("%s: rc = %x", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(wcd9xxx_slim_ch_master_close);

int wcd9xxx_slim_ch_master_status(struct wcd9xxx *wcd9xxx, void *handle,
				  phys_addr_t phys, u32 *len)
{
	int rc = 0;
	struct wcd9xxx_master_cfg *tx_master;
	struct wcd9xxx_slim_master_prop *slim_cfg;
	struct completion *sb_comp;

	if (wcd9xxx == NULL || len == NULL) {
		pr_err("%s: Invlaid len/wcd9xxx pointer\n",
		       __func__);
		return -EINVAL;
	}
	tx_master = &slim_tx_master;
	if (handle != tx_master->slim_s.handle) {
		pr_err("%s: handle(%p) not matching slim_hdl(%p)\n",
			__func__, handle, tx_master->slim_s.handle);
		return -EINVAL;
	}
	mutex_lock(&tx_master->lock);
	slim_cfg = tx_master->slim_cfg;
	sb_comp = &tx_master->slim_s.sb_comp;
	rc = wait_for_completion_timeout(sb_comp, (2 * (HZ/10)));
	rc = slim_port_get_xfer_status(wcd9xxx->slim, slim_cfg->ph1,
				       &phys, len);
	if (rc || *len == 0) {
		pr_err("%s: Get Xfer status rc %x, len %x\n",
		       __func__, rc, *(len));
	}
	mutex_unlock(&tx_master->lock);
return rc;
}
EXPORT_SYMBOL(wcd9xxx_slim_ch_master_status);

int wcd9xxx_slim_ch_master_enable_read(struct wcd9xxx *wcd9xxx, void *handle)
{
	int rc = 0;
	struct wcd9xxx_master_cfg *tx_master;
	struct wcd9xxx_slim_master_prop *slim_cfg;
	pr_debug("%s:handle = %p\n", __func__, handle);

	if (wcd9xxx == NULL || handle == NULL) {
		pr_err("%s: Invalid params, wcd9xxx(%p) handle(%p)\n",
			__func__, wcd9xxx, handle);
		return -EINVAL;
	}

	tx_master = &slim_tx_master;
	if (handle != tx_master->slim_s.handle) {
		pr_err("%s: handle(%p) not matching slim_hdl(%p)\n",
			__func__, handle, tx_master->slim_s.handle);
		return -EINVAL;
	}
	mutex_lock(&tx_master->lock);
	slim_cfg = tx_master->slim_cfg;
	rc = slim_connect_sink(wcd9xxx->slim,
			       &slim_cfg->ph1, 1 ,
			       slim_cfg->chanh);
	if (rc) {
		pr_err("%s:connect src ret:%d\n", __func__, rc);
		goto error_exit;
	}
	rc = slim_control_ch(wcd9xxx->slim, slim_cfg->grph,
			     SLIM_CH_ACTIVATE, true);
	if (rc) {
		pr_err("%s:activate ch ret:%d\n", __func__, rc);
		goto error_exit;
	}
	mutex_unlock(&tx_master->lock);
	return 0;
error_exit:
	mutex_unlock(&tx_master->lock);
	/*Client has to close if error, do not clean up here*/
	return rc;
}
EXPORT_SYMBOL(wcd9xxx_slim_ch_master_enable_read);

int wcd9xxx_slim_ch_master_read(struct wcd9xxx *wcd9xxx, void *handle,
				phys_addr_t phys, u8 *mem,
				u32 read_len)
{
	int rc = 0;
	struct wcd9xxx_master_cfg *tx_master;
	struct wcd9xxx_slim_master_prop *slim_cfg;
	struct completion *sb_comp;

	pr_debug("%s: handle %p len %x\n",
		  __func__, handle, read_len);

	if (wcd9xxx == NULL || handle == NULL) {
		pr_err("%s: Invlaid handle/wcd9xxx pointer\n", __func__);
		return -EINVAL;
	}

	tx_master = &slim_tx_master;
	if (handle != tx_master->slim_s.handle) {
		pr_err("%s: handle(%p) not matching slim_hdl(%p)\n",
			__func__, handle, tx_master->slim_s.handle);
		return -EINVAL;
	}
	mutex_lock(&tx_master->lock);
	slim_cfg = tx_master->slim_cfg;
	sb_comp = &tx_master->slim_s.sb_comp;
	rc = slim_port_xfer(wcd9xxx->slim, slim_cfg->ph1,
			    phys, read_len, sb_comp);
	if (rc) {
		pr_err("%s:Slimbus master read failure rc %d\n",
		       __func__, rc);
	}
	mutex_unlock(&tx_master->lock);
	return rc;
}
EXPORT_SYMBOL(wcd9xxx_slim_ch_master_read);
