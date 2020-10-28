/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ilitek_v3.h"

struct touch_bus_info {
	struct i2c_driver bus_driver;
	struct ilitek_hwif_info *hwif;
};

struct ilitek_ts_data *ilits;

#define RW_SYNC 	0
#define R_ONLY		1
#define W_ONLY		2

#if I2C_DMA_TRANSFER
static unsigned char *ilitek_dma_va;
static dma_addr_t ilitek_dma_pa;

#define DMA_VA_BUFFER   4096

static int dma_i2c_alloc(struct i2c_client *client)
{
	if (client != NULL) {
		client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		ilitek_dma_va = (u8 *)dmam_alloc_coherent(client->dev, DMA_VA_BUFFER, &ilitek_dma_pa, GFP_KERNEL);
		if (ERR_ALLOC_MEM(ilitek_dma_va)) {
			ILI_ERR("Allocate DMA I2C Buffer failed\n");
			return -ENOMEM;
		}

		memset(ilitek_dma_va, 0, DMA_VA_BUFFER);
		client->ext_flag |= I2C_DMA_FLAG;
		return 0;
	}

	ILI_ERR("client is NULL, allocated dma i2c failed\n");
	return -ENODEV;
}
#endif

static int core_i2c_write(void *buf, int len)
{
	int ret = 0;
	u8 *txbuf = (u8 *)buf;
	u8 check_sum = 0;
	u8 *mpbuf = NULL;

	struct i2c_msg msgs[] = {
		{
		 .addr = ilits->i2c->addr,
		 .flags = 0,	/* write flag. */
		 .len = len,
		 .buf = txbuf,
		 },
	};

#if I2C_DMA_TRANSFER
	if (len > 8) {
		msgs[0].addr = (ilits->client->addr & I2C_MASK_FLAG);
		msgs[0].ext_flag = (ilits->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		memcpy(ilitek_dma_va, txbuf, len);
		msgs[0].buf = (u8 *)ilitek_dma_pa;
	}
#endif


	/*
	 * NOTE: If TP driver is doing MP test and commanding 0xF1 to FW, we add a checksum
	 * to the last index and plus 1 with size.
	 */
	if (atomic_read(&ilits->mp_stat) && txbuf[0] == P5_X_SET_CDC_INIT) {
		check_sum = ili_calc_packet_checksum(txbuf, len);
		mpbuf = kcalloc(len + 1, sizeof(u8), GFP_KERNEL);
		if (ERR_ALLOC_MEM(mpbuf)) {
			ILI_ERR("Failed to allocate mpbuf mem\n");
			return -ENOMEM;
		}
		ipio_memcpy(mpbuf, txbuf, len, msgs[0].len);
		mpbuf[len] = check_sum;
		msgs[0].buf = mpbuf;
		ili_dump_data(mpbuf, 8, len+1, 0, "mp cdc cmd");
		msgs[0].len = len + 1;
	}

	if (i2c_transfer(ilits->i2c->adapter, msgs, 1) != 1)
		ret = -1;

	ipio_kfree((void **)&mpbuf);
	return ret;
}

static int core_i2c_read(void *buf, int len)
{
	int ret;
	u8 *rxbuf = (u8 *)buf;

	struct i2c_msg msgs[] = {
		{
		 .addr = ilits->i2c->addr,
		 .flags = I2C_M_RD,	/* read flag. */
		 .len = len,
		 .buf = rxbuf,
		 },
	};


#if I2C_DMA_TRANSFER
	if (len > 8) {
		msgs[0].addr = (ilits->client->addr & I2C_MASK_FLAG);
		msgs[0].ext_flag = (ilits->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		msgs[0].buf = (u8 *)ilitek_dma_pa;
	} else {
		msgs[0].buf = rxbuf;
	}
#endif


	ret = i2c_transfer(ilits->i2c->adapter, msgs, 1);

#if I2C_DMA_TRANSFER
	if (len > 8)
		memcpy(rxbuf, ilitek_dma_va, len);
#endif

	/*
	 * If i2c_transfer is ok (must return 1 because only sends one msg),
	 * return #bytes transferred, else error code.
	 */
	return (ret == 1) ? len : ret;
}

static int ilitek_i2c_write(void *buf, int len)
{
	int ret = 0;

	if (len == 0) {
		ILI_ERR("i2c write len is invalid\n");
		return -EINVAL;
	}

	ret = core_i2c_write(buf, len);
	if (ret < 0) {
		if (atomic_read(&ilits->tp_reset) == START) {
			ret = 0;
			goto out;
		}
		ILI_ERR("i2c write error, ret = %d\n", ret);
	}

out:
	return ret;
}

static int ilitek_i2c_read(void *buf, int len)
{
	int ret = 0;

	if (len == 0) {
		ILI_ERR("i2c read len is invalid\n");
		return -EINVAL;
	}

	ret = core_i2c_read(buf, len);
	if (ret < 0) {
		if (atomic_read(&ilits->tp_reset) == START) {
			ret = 0;
			goto out;
		}
		ILI_ERR("i2c read error, ret = %d\n", ret);
	}

out:
	return ret;
}

static int ili_i2c_pll_clk_wakeup(void)
{
	int ret = 0;
	u8 wakeup = 0xA3;
#if (PLL_CLK_WAKEUP_TP_RESUME == ENABLE)
	if (ilits->pll_clk_wakeup == true) {
#else
	if ((ilits->pll_clk_wakeup == true) && (ilits->tp_suspend == true)) {
#endif
		ret = ilitek_i2c_write(&wakeup, sizeof(wakeup));
		if (ret < 0)
			ILI_ERR("i2c write dummy cmd error\n");
	}
	return ret;
}

static int ili_i2c_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool spi_irq, bool i2c_irq)
{
	int ret = 0, operate = -1;

	if (wlen > 0) {
		if (ERR_ALLOC_MEM(txbuf)) {
			ILI_ERR("txbuf is null\n");
			return -ENOMEM;
		}
	}

	if (rlen > 0) {
		if (ERR_ALLOC_MEM(rxbuf)) {
			ILI_ERR("rxbuf is null\n");
			return -ENOMEM;
		}
	}

	if (wlen > 0 && rlen > 0)
		operate = RW_SYNC;
	else if (wlen > 0 && !rlen)
		operate = W_ONLY;
	else
		operate = R_ONLY;

	if (ilits->int_pulse)
		ilits->detect_int_stat = ili_ic_check_int_pulse;
	else
		ilits->detect_int_stat = ili_ic_check_int_level;

	if (i2c_irq)
		atomic_set(&ilits->cmd_int_check, ENABLE);

	switch (operate) {
	case RW_SYNC:
		ret = ilitek_i2c_write(txbuf, wlen);
		if (ret < 0) {
			ILI_ERR("i2c-wrapper write error\n");
			break;
		}

		if (i2c_irq) {
			if (ilits->detect_int_stat(false) < 0) {
				ILI_ERR("ERROR! Check INT timeout\n");
				ret = -ETIME;
				break;
			}
		} else {
			msleep(1);
		}

		ret = ilitek_i2c_read(rxbuf, rlen);
		if (ret < 0) {
			ILI_ERR("i2c-wrapper read error\n");
			break;
		}
		break;
	case W_ONLY:
		ret = ili_i2c_pll_clk_wakeup();
		if (ret < 0) {
			ILI_ERR("i2c-wrapper dummy cmd write error\n");
		}
		ret = ilitek_i2c_write(txbuf, wlen);
		if (ret < 0) {
			ILI_ERR("i2c-wrapper write error\n");
			break;
		}
		break;
	case R_ONLY:
		if (i2c_irq) {
			if (ilits->detect_int_stat(false) < 0) {
				ILI_ERR("ERROR! Check INT timeout\n");
				ret = -ETIME;
				break;
			}
		}

		ret = ilitek_i2c_read(rxbuf, rlen);
		if (ret < 0) {
			ILI_ERR("i2c-wrapper read error\n");
			break;
		}
		break;
	default:
		ILI_ERR("Unknown ts-i2c operation\n");
		ret = -EINVAL;
		break;
	}

	if (i2c_irq)
		atomic_set(&ilits->cmd_int_check, DISABLE);

	return ret;
}

int ili_core_spi_setup(int num)
{
	ILI_ERR("Not support this interface\n");
	return 0;
}

static int ilitek_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct touch_bus_info *info =
		container_of(to_i2c_driver(i2c->dev.driver),
			struct touch_bus_info, bus_driver);

	ILI_INFO("ilitek i2c probe\n");

	if (!i2c) {
		ILI_ERR("i2c client is NULL\n");
		return -ENODEV;
	}

	if (i2c->addr != TDDI_I2C_ADDR) {
		i2c->addr = TDDI_I2C_ADDR;
		ILI_INFO("i2c addr doesn't be set up, use default : 0x%x\n", i2c->addr);
	}

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		ILI_ERR("i2c functions are not supported!\n");
		return -ENODEV;
	}

	ilits = devm_kzalloc(&i2c->dev, sizeof(struct ilitek_ts_data), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits)) {
		ILI_ERR("Failed to allocate ts memory, %ld\n", PTR_ERR(ilits));
		return -ENOMEM;
	}

	/* Used for receiving touch data only, do not mix up with others. */
	ilits->tr_buf = kzalloc(TR_BUF_SIZE, GFP_ATOMIC);
	if (ERR_ALLOC_MEM(ilits->tr_buf)) {
		ILI_ERR("failed to allocate touch report buffer\n");
		return -ENOMEM;
	}

	ilits->gcoord = kzalloc(sizeof(struct gesture_coordinate), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits->gcoord)) {
		ILI_ERR("Failed to allocate gresture coordinate buffer\n");
		return -ENOMEM;
	}

	ilits->i2c = i2c;
	ilits->spi = NULL;
	ilits->dev = &i2c->dev;
	ilits->hwif = info->hwif;
	ilits->phys = "I2C";
	ilits->wrapper = ili_i2c_wrapper;
	ilits->detect_int_stat = ili_ic_check_int_pulse;
	ilits->int_pulse = true;
	ilits->mp_retry = false;

#if I2C_DMA_TRANSFER
	if (dma_i2c_alloc(ilits->i2c) < 0)
		ILI_ERR("Failed to alllocate DMA mem %ld\n", PTR_ERR(ilits->i2c));
#endif

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ilits->tp_data_mode = AP_MODE;

	if (TDDI_RST_BIND)
		ilits->reset = TP_IC_WHOLE_RST;
	else
		ilits->reset = TP_HW_RST_ONLY;

	ilits->rst_edge_delay = 100;
	ilits->fw_open = FILP_OPEN;
	ilits->fw_upgrade_mode = UPGRADE_FLASH;
	ilits->mp_move_code = ili_move_mp_code_flash;
	ilits->gesture_move_code = ili_move_gesture_code_flash;
	ilits->esd_recover = ili_wq_esd_i2c_check;
	ilits->ges_recover = ili_touch_esd_gesture_flash;
	ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ilits->gesture_demo_ctrl = DISABLE;
	ilits->wtd_ctrl = OFF;
	ilits->report = ENABLE;
	ilits->netlink = DISABLE;
	ilits->dnp = DISABLE;
	ilits->irq_tirgger_type = IRQF_TRIGGER_FALLING;
	ilits->info_from_hex = DISABLE;
	ilits->wait_int_timeout = AP_INT_TIMEOUT;

#if ENABLE_GESTURE
	ilits->gesture = DISABLE;
	ilits->ges_sym.double_tap = DOUBLE_TAP;
	ilits->ges_sym.alphabet_line_2_top = ALPHABET_LINE_2_TOP;
	ilits->ges_sym.alphabet_line_2_bottom = ALPHABET_LINE_2_BOTTOM;
	ilits->ges_sym.alphabet_line_2_left = ALPHABET_LINE_2_LEFT;
	ilits->ges_sym.alphabet_line_2_right = ALPHABET_LINE_2_RIGHT;
	ilits->ges_sym.alphabet_m = ALPHABET_M;
	ilits->ges_sym.alphabet_w = ALPHABET_W;
	ilits->ges_sym.alphabet_c = ALPHABET_C;
	ilits->ges_sym.alphabet_E = ALPHABET_E;
	ilits->ges_sym.alphabet_V = ALPHABET_V;
	ilits->ges_sym.alphabet_O = ALPHABET_O;
	ilits->ges_sym.alphabet_S = ALPHABET_S;
	ilits->ges_sym.alphabet_Z = ALPHABET_Z;
	ilits->ges_sym.alphabet_V_down = ALPHABET_V_DOWN;
	ilits->ges_sym.alphabet_V_left = ALPHABET_V_LEFT;
	ilits->ges_sym.alphabet_V_right = ALPHABET_V_RIGHT;
	ilits->ges_sym.alphabet_two_line_2_bottom = ALPHABET_TWO_LINE_2_BOTTOM;
	ilits->ges_sym.alphabet_F = ALPHABET_F;
	ilits->ges_sym.alphabet_AT = ALPHABET_AT;
#endif

	return info->hwif->plat_probe();
}

static int ilitek_i2c_remove(struct i2c_client *i2c)
{
	ILI_INFO();
	return 0;
}

static const struct i2c_device_id tp_i2c_id[] = {
	{TDDI_DEV_ID, 0},
	{},
};

int ili_interface_dev_init(struct ilitek_hwif_info *hwif)
{
	struct touch_bus_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ILI_ERR("faied to allocate i2c_driver\n");
		return -ENOMEM;
	}

	if (hwif->bus_type != BUS_I2C) {
		ILI_ERR("Not I2C dev\n");
		ipio_kfree((void **)&info);
		return -EINVAL;
	}

	hwif->info = info;

	info->bus_driver.driver.name = hwif->name;
	info->bus_driver.driver.owner = hwif->owner;
	info->bus_driver.driver.of_match_table = hwif->of_match_table;
	info->bus_driver.driver.pm = hwif->pm;

	info->bus_driver.probe = ilitek_i2c_probe;
	info->bus_driver.remove = ilitek_i2c_remove;
	info->bus_driver.id_table = tp_i2c_id;

	info->hwif = hwif;
	return i2c_add_driver(&info->bus_driver);
}

void ili_interface_dev_exit(struct ilitek_ts_data *ts)
{
	struct touch_bus_info *info = (struct touch_bus_info *)ilits->hwif->info;

	ILI_INFO("remove i2c dev\n");
	i2c_del_driver(&info->bus_driver);
	ipio_kfree((void **)&info);
}
