#include <linux/kernel.h>

#define LOG_TAG                    "TCS"

#include "cts_config.h"
#include "cts_firmware.h"
#include "cts_platform.h"

#define _CTS_TCS_C_
#include "cts_tcs.h"
#undef  _CTS_TCS_C_

#define TEST_RESULT_BUFFER_SIZE(cts_dev) \
	(cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

#define RAWDATA_BUFFER_SIZE(cts_dev) \
	(cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

#define DIFFDATA_BUFFER_SIZE(cts_dev) \
	((cts_dev->fwdata.rows + 2) * (cts_dev->fwdata.cols + 2) * 2)

/* raw touch info without data */
#define TOUCH_INFO_SIZ                      (112)
/* tcs reply tail: (errcode + cmd + crc) */
#define TCS_REPLY_TAIL_SIZ                  (sizeof(u8) + sizeof(u16) + sizeof(u16))

#define INT_DATA_VALID_SIZ					(62)
#define INT_DATA_INFO_SIZ                   (64)
#define INT_DATA_TYPE_LEN_SIZ               (4)

static u8 dump_flag = 1;

void dump_spi(const char *prefix, u8 *data, size_t datalen)
{
	u8 str[1024];
	int offset = 0;
	int i;

	if (!dump_flag)
		return;

	offset += snprintf(str + offset, sizeof(str) - offset, "%s", prefix);
	for (i = 0; i < datalen; i++) {
		offset += snprintf(str + offset, sizeof(str) - offset, " %02x", data[i]);
	}
	cts_err("%s", str);
}

static int cts_tcs_spi_xtrans(const struct cts_device *cts_dev, u8 *tx,
	size_t txlen, u8 *rx, size_t rxlen)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer xfer[2];
	struct chipone_ts_data *cts_data = container_of(cts_dev,
		struct chipone_ts_data, cts_dev);
	u16 crc16_recv, crc16_calc;

	memset(&xfer[0], 0, sizeof(struct spi_transfer));
	xfer[0].delay_usecs = 0;
	xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
	xfer[0].tx_buf = tx;
	xfer[0].rx_buf = NULL;
	xfer[0].len = txlen;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 0);
#endif
	ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 1);
#endif
	if (ret < 0) {
		cts_err("spi_sync xfer[0] failed: %d", ret);
		return ret;
	}
	udelay(100);

	memset(&xfer[1], 0, sizeof(struct spi_transfer));
	xfer[1].delay_usecs = 0;
	xfer[1].speed_hz = cts_dev->pdata->spi_speed * 1000u;
	xfer[1].tx_buf = NULL;
	xfer[1].rx_buf = rx;
	xfer[1].len = rxlen;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[1], &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 0);
#endif
	ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 1);
#endif
	if (ret < 0) {
		cts_err("spi_sync xfer[1] failed: %d", ret);
		return ret;
	}

	crc16_recv = (rx[rxlen - 1] << 8) | rx[rxlen - 2];
	crc16_calc = cts_crc16(rx, rxlen - 2);
	if (crc16_recv != crc16_calc) {
		cts_err("xtrans crc error: recv %04x != %04x calc", crc16_recv,
			crc16_calc);
		return -EIO;
	}
	udelay(100);

	return 0;
}

static int cts_tcs_spi_xtrans_1_cs(const struct cts_device *cts_dev, u8 *tx,
	size_t txlen, u8 *rx, size_t rxlen)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer xfer[1];
	struct chipone_ts_data *cts_data = container_of(cts_dev,
		struct chipone_ts_data, cts_dev);

	memset(&xfer[0], 0, sizeof(struct spi_transfer));
	xfer[0].delay_usecs = 0;
	xfer[0].speed_hz = cts_dev->pdata->spi_speed * 1000u;
	xfer[0].tx_buf = tx;
	xfer[0].rx_buf = rx;
	xfer[0].len = txlen > rxlen ? txlen : rxlen;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 0);
#endif
	ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 1);
#endif
	if (ret < 0) {
		cts_err("spi_sync xfer[0] failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cts_tcs_spi_read_pack(u8 *tx, TcsCmdValue_t *tcv, u16 rdatalen)
{
	tcs_tx_head *txhdr = (tcs_tx_head *) tx;
	int packlen = 0;
	u16 crc16;

	txhdr->addr = TCS_RD_ADDR;
	txhdr->cmd	= (tcv->baseFlag << 15) | (tcv->isRead << 14) |
		(tcv->classID << 8) | (tcv->cmdID << 0);
	txhdr->datlen = rdatalen;
	crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_tx_head, crc16));
	txhdr->crc16 = crc16;
	packlen += sizeof(tcs_tx_head);

	return packlen;
}

static int cts_tcs_spi_write_pack(u8 *tx, TcsCmdValue_t *tcv,
	u8 *wdata, u16 wdatalen)
{
	tcs_tx_head *txhdr = (tcs_tx_head *) tx;
	int packlen = 0;
	u16 crc16;

	txhdr->addr = TCS_WR_ADDR;
	txhdr->cmd = (tcv->baseFlag << 15) | (tcv->isWrite << 13) |
		(tcv->classID << 8) | (tcv->cmdID << 0);
	txhdr->datlen = wdatalen;
	crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_tx_head, crc16));
	txhdr->crc16 = crc16;
	packlen += sizeof(tcs_tx_head);

	if (wdatalen > 0) {
		memcpy(tx + sizeof(tcs_tx_head), wdata, wdatalen);
		crc16 = cts_crc16(wdata, wdatalen);
		*(tx + sizeof(tcs_tx_head) + wdatalen) = ((crc16 >> 0) & 0xFF);
		*(tx + sizeof(tcs_tx_head) + wdatalen + 1) = ((crc16 >> 8) & 0xFF);
		packlen += wdatalen + sizeof(crc16);
	}

	return packlen;
}

int cts_tcs_spi_read(const struct cts_device *cts_dev,
	enum TcsCmdIndex cmdIdx, u8 *rdata, size_t rdatalen)
{
	// static u8 tx[INT_DATA_MAX_SIZ];
	// static u8 rx[INT_DATA_MAX_SIZ];
	int txlen;

	txlen = cts_tcs_spi_read_pack(cts_dev->tx, TcsCmdValue + cmdIdx,
		rdatalen);
	/* dump_spi(">> ", cts_dev->tx, txlen); */
	cts_tcs_spi_xtrans(cts_dev, cts_dev->tx, txlen, cts_dev->rx,
		rdatalen + sizeof(tcs_rx_tail));
	/* dump_spi("<< ", cts_dev->rx, rdatalen + sizeof(tcs_rx_tail)); */
    /**
     * if (cts_dev->rx[rdatalen] != 0) {
     *     cts_err("spi_sync failed: err=%d", cts_dev->rx[rdatalen]);
     * }
     */
	memcpy(rdata, cts_dev->rx, rdatalen);

	return 0;
}

int cts_tcs_spi_read_1_cs(const struct cts_device *cts_dev,
	enum TcsCmdIndex cmdIdx, u8 *rdata, size_t rdatalen)
{
	// static u8 tx[INT_DATA_MAX_SIZ];
	// static u8 rx[INT_DATA_MAX_SIZ];
	int txlen;
	u16 crc16_recv, crc16_calc;

	txlen = cts_tcs_spi_read_pack(cts_dev->tx, TcsCmdValue + cmdIdx,
		rdatalen);
	/* dump_spi(">> ", cts_dev->tx, txlen); */
	cts_tcs_spi_xtrans_1_cs(cts_dev, cts_dev->tx, txlen, cts_dev->rx,
		rdatalen);
	/* dump_spi("<< ", cts_dev->rx, rdatalen + sizeof(tcs_rx_tail)); */
    /**
     * if (cts_dev->rx[rdatalen] != 0) {
     *     cts_err("spi_sync failed: err=%d", cts_dev->rx[rdatalen]);
     * }
     */
	crc16_recv = (cts_dev->rx[rdatalen - 1] << 8) | cts_dev->rx[rdatalen - 2];
	crc16_calc = cts_crc16(cts_dev->rx, rdatalen - 2);
	if (crc16_recv != crc16_calc) {
		cts_err("Get int data crc error: recv %04x != %04x calc",
			crc16_recv, crc16_calc);
		return -EIO;
	}

	memcpy(rdata, cts_dev->rx, rdatalen);

	return 0;
}

int cts_tcs_spi_write(const struct cts_device *cts_dev,
	enum TcsCmdIndex cmdIdx, u8 *wdata, size_t wdatalen)
{
	// static u8 tx[2048];
	// static u8 rx[2048];
	int txlen;

	txlen = cts_tcs_spi_write_pack(cts_dev->tx, TcsCmdValue + cmdIdx,
		wdata, wdatalen);
	/* dump_spi(">> ", cts_dev->tx, txlen); */
	cts_tcs_spi_xtrans(cts_dev, cts_dev->tx, txlen, cts_dev->rx,
		sizeof(tcs_rx_tail));
	/* dump_spi("<< ", cts_dev->rx, wdatalen + sizeof(tcs_rx_tail)); */
	return cts_dev->rx[0];
}

int cts_tcs_get_fw_ver(const struct cts_device *cts_dev, u16 *fwver)
{
	int ret = -1;
	u8 buf[4];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_FW_VER_RO,
		buf, sizeof(buf));
	if (!ret) {
		*fwver = buf[0] | (buf[1] << 8);
		return 0;
	}
	return ret;
}

int cts_tcs_get_lib_ver(const struct cts_device *cts_dev, u16 *libver)
{
	int ret = -1;
	u8 buf[4];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_FW_VER_RO, buf,
		sizeof(buf));
	if (!ret) {
		*libver = buf[2] | (buf[3] << 8);
		return 0;
	}
	return ret;
}

int cts_tcs_get_fw_id(const struct cts_device *cts_dev, u16 *fwid)
{
	int ret = -1;
	u8 buf[4];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_CHIP_FW_ID_RO,
		buf, sizeof(buf));
	if (!ret) {
		*fwid = buf[0] | (buf[1] << 8);
		return 0;
	}

	return ret;
}

int cts_tcs_get_ddi_ver(const struct cts_device *cts_dev, u8 *ddiver)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DDI_CODE_VER_RO,
		buf, sizeof(buf));
	if (!ret) {
		*ddiver = buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_res_x(const struct cts_device *cts_dev, u16 *res_x)
{
	int ret = -1;
	u8 buf[10];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_TOUCH_XY_INFO_RO,
		buf, sizeof(buf));
	if (!ret) {
		*res_x = buf[0] | (buf[1] << 8);
		return 0;
	}
	return ret;
}

int cts_tcs_get_res_y(const struct cts_device *cts_dev, u16 *res_y)
{
	int ret = -1;
	u8 buf[10];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_TOUCH_XY_INFO_RO,
		buf, sizeof(buf));
	if (!ret) {
		*res_y = buf[2] | (buf[3] << 8);
		return 0;
	}
	return ret;
}

int cts_tcs_get_rows(const struct cts_device *cts_dev, u8 *rows)
{
	int ret = -1;
	u8 buf[10];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_TOUCH_XY_INFO_RO,
		buf, sizeof(buf));
	if (!ret) {
		*rows = buf[5];
		return 0;
	}
	return ret;
}

int cts_tcs_get_cols(const struct cts_device *cts_dev, u8 *cols)
{
	int ret = -1;
	u8 buf[10];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_TOUCH_XY_INFO_RO,
		buf, sizeof(buf));
	if (!ret) {
		*cols = buf[4];
		return 0;
	}
	return ret;
}

int cts_tcs_get_flip_x(const struct cts_device *cts_dev, bool *flip_x)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_COORD_FLIP_X_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*flip_x = !!buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_flip_y(const struct cts_device *cts_dev, bool *flip_y)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_COORD_FLIP_Y_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*flip_y = !!buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_swap_axes(const struct cts_device *cts_dev, bool *swap_axes)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_COORD_SWAP_AXES_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*swap_axes = !!buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_int_mode(const struct cts_device *cts_dev, u8 *int_mode)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_INT_MODE_RW,
		buf, sizeof(buf));
	if (!ret) {
		*int_mode = buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_int_keep_time(const struct cts_device *cts_dev,
	u16 *int_keep_time)
{
	int ret = -1;
	u8 buf[2];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_INT_KEEP_TIME_RW,
		buf, sizeof(buf));
	if (!ret) {
		*int_keep_time = (buf[0] | (buf[1] << 8));
		return 0;
	}
	return ret;

}

int cts_tcs_get_rawdata_target(const struct cts_device *cts_dev,
	u16 *rawdata_target)
{
	int ret = -1;
	u8 buf[2];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_CNEG_OPTIONS_RW,
		buf, sizeof(buf));
	if (!ret) {
		*rawdata_target = (buf[0] | (buf[1] << 8));
		return 0;
	}
	return ret;

}

int cts_tcs_get_esd_method(const struct cts_device *cts_dev, u8 *esd_method)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_DDI_ESD_OPTIONS_RW,
		buf, sizeof(buf));
	if (!ret) {
		*esd_method = buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_esd_protection(const struct cts_device *cts_dev,
	u8 *esd_protection)
{
	int ret = -1;
	u8 buf[4];

	buf[0] = 0x01;
	buf[1] = 0x56;
	buf[2] = 0x81;
	buf[3] = 0x00;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		esd_protection, sizeof(u8));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_get_data_ready_flag(const struct cts_device *cts_dev, u8 *ready)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DAT_RDY_FLAG_RW,
		buf, sizeof(buf));
	if (!ret) {
		*ready = buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_clr_gstr_ready_flag(const struct cts_device *cts_dev)
{
	int ret;
	u8 ready = 0;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_GSTR_DAT_RDY_FLAG_GSTR_RW,
		&ready, sizeof(ready));
	return ret;
}

int cts_tcs_clr_data_ready_flag(const struct cts_device *cts_dev)
{
	int ret;
	u8 ready = 0;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_DAT_RDY_FLAG_RW,
		&ready, sizeof(ready));
	return ret;
}

int cts_tcs_enable_get_rawdata(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[1];

	buf[0] = 0x01;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_disable_get_rawdata(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[1];

	buf[0] = 0x00;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_enable_get_cneg(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[1];

	buf[0] = 0x01;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_CNEG_RD_EN_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_disable_get_cneg(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[1];

	buf[0] = 0x00;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_CNEG_RD_EN_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_is_cneg_ready(const struct cts_device *cts_dev, u8 *ready)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_CNEG_RDY_FLAG_RW,
		buf, sizeof(buf));
	if (!ret) {
		*ready = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_quit_guesture_mode(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[1];

	buf[0] = 0x00;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_MNT_FORCE_EXIT_MNT_WO,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_get_rawdata(const struct cts_device *cts_dev, u8 *buf)
{
	int ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_RAW_RO, buf,
		RAWDATA_BUFFER_SIZE(cts_dev));

	return ret;
}

int cts_tcs_get_diffdata(const struct cts_device *cts_dev, u8 *buf)
{
	int ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_DIFF_RO, buf,
		DIFFDATA_BUFFER_SIZE(cts_dev));

	return ret;
}

int cts_tcs_get_basedata(const struct cts_device *cts_dev, u8 *buf)
{
	int ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_BASE_RO, buf,
		DIFFDATA_BUFFER_SIZE(cts_dev));

	return ret;
}

int cts_tcs_get_cneg(const struct cts_device *cts_dev, u8 *buf, size_t size)
{
	int ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_CNEG_RO, buf, size);

	return ret;

}

int cts_tcs_read_hw_reg(const struct cts_device *cts_dev, u32 addr,
			u8 *regbuf, size_t size)
{
	int ret;
	u8 buf[4];

	buf[0] = 1;
	buf[1] = ((addr >> 0) & 0xFF);
	buf[2] = ((addr >> 8) & 0xFF);
	buf[3] = ((addr >> 16) & 0xFF);

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_READ_START_RO,
		regbuf, size);
	if (ret != 0)
		return ret;

	return 0;
}

int cts_tcs_write_hw_reg(const struct cts_device *cts_dev, u32 addr,
	u8 *regbuf, size_t size)
{
	int ret;
	u8 *buf;

	buf = kmalloc(size + 6, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	buf[0] = ((size >> 0) & 0xFF);
	buf[1] = ((size >> 8) & 0xFF);
	buf[2] = ((addr >> 0) & 0xFF);
	buf[3] = ((addr >> 8) & 0xFF);
	buf[4] = ((addr >> 16) & 0xFF);
	buf[5] = 0x00;
	memcpy(buf + 6, regbuf, size);

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO,
		buf, size + 6);
	if (ret != 0) {
		kfree(buf);
		return ret;
	}

	kfree(buf);

	return ret;

}

int cts_tcs_read_ddi_reg(const struct cts_device *cts_dev, u32 addr,
	u8 *regbuf, size_t size)
{
	int ret;
	u8 buf[2];

	buf[0] = 2;
	buf[1] = addr;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_TP_DATA_READ_START_RO,
		regbuf, size);
	if (ret != 0)
		return ret;

	return 0;
}

int cts_tcs_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
	u8 *regbuf, size_t size)
{
	int ret;
	u8 *buf;

	buf = kmalloc(size + 6, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	buf[0] = ((size >> 0) & 0xFF);
	buf[1] = ((size >> 8) & 0xFF);
	buf[2] = addr;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	memcpy(buf + 6, regbuf, size);

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO,
		buf, size + 6);
	if (ret != 0) {
		kfree(buf);
		return ret;
	}

	kfree(buf);

	return ret;

}

static int cts_tcs_reg_read_pack(u8 *tx, uint16_t cmd, u16 rdatalen)
{
	tcs_tx_head *txhdr = (tcs_tx_head *) tx;
	int packlen = 0;
	u16 crc16;

	txhdr->addr = TCS_RD_ADDR;
	txhdr->cmd = cmd;
	txhdr->datlen = rdatalen;
	crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_tx_head, crc16));
	txhdr->crc16 = crc16;
	packlen += sizeof(tcs_tx_head);

	return packlen;
}

int cts_tcs_read_reg(const struct cts_device *cts_dev, uint16_t cmd,
	u8 *rbuf, size_t rlen)
{
	static u8 tx[2048];
	static u8 rx[10];

	int txlen = cts_tcs_reg_read_pack(tx, cmd, rlen);

	cts_tcs_spi_xtrans(cts_dev, tx, txlen, rx,
		rlen + sizeof(tcs_rx_tail));

	memcpy(rbuf, rx, rlen);

	return 0;
}

static int cts_tcs_reg_write_pack(u8 *tx, uint16_t cmd,
	u8 *wdata, u16 wdatalen)
{
	tcs_tx_head *txhdr = (tcs_tx_head *) tx;
	int packlen = 0;
	u16 crc16;

	txhdr->addr = TCS_WR_ADDR;
	txhdr->cmd = cmd;
	txhdr->datlen = wdatalen;
	crc16 = cts_crc16((const u8 *)txhdr, offsetof(tcs_tx_head, crc16));
	txhdr->crc16 = crc16;
	packlen += sizeof(tcs_tx_head);

	if (wdatalen > 0) {
		memcpy(tx + sizeof(tcs_tx_head), wdata, wdatalen);
		crc16 = cts_crc16(wdata, wdatalen);
		*(tx + sizeof(tcs_tx_head) + wdatalen) = ((crc16 >> 0) & 0xFF);
		*(tx + sizeof(tcs_tx_head) + wdatalen + 1) = ((crc16 >> 8) & 0xFF);
		packlen += wdatalen + sizeof(crc16);
	}

	return packlen;
}

int cts_tcs_write_reg(const struct cts_device *cts_dev, uint16_t cmd,
	u8 *wbuf, size_t wlen)
{
	static u8 rx[2048];
	static u8 tx[2048];
	int txlen = cts_tcs_reg_write_pack(tx, cmd, wbuf, wlen);

	cts_tcs_spi_xtrans(cts_dev, tx, txlen + wlen, rx,
		wlen + sizeof(tcs_rx_tail));
	return 0;
}

int cts_tcs_read_fw_reg(const struct cts_device *cts_dev, u32 addr,
	u8 *regbuf, size_t size)
{
	int ret;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_READ_RO, regbuf, size);
	if (ret != 0)
		return ret;

	return 0;
}

int cts_tcs_write_fw_reg(const struct cts_device *cts_dev, u32 addr,
	u8 *regbuf, size_t size)
{
	int ret;
	u8 buf[4];

	buf[0] = 0x01;
	buf[1] = ((addr >> 0) & 0xFF);
	buf[2] = ((addr >> 8) & 0xFF);
	buf[3] = ((addr >> 16) & 0xFF);

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
		regbuf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_calc_int_data_size(struct cts_device *cts_dev)
{
#define INT_DATA_TYPE_U8_SIZ		\
	(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u8))
#define INT_DATA_TYPE_U16_SIZ		\
	(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * sizeof(u16))

	int data_size = TOUCH_INFO_SIZ + TCS_REPLY_TAIL_SIZ;
	u16 data_types = cts_dev->fwdata.int_data_types;
	u8 data_method = cts_dev->fwdata.int_data_method;

	if (data_method == INT_DATA_METHOD_NONE) {
		cts_dev->fwdata.int_data_size = data_size;
		return 0;
	} else if (data_method == INT_DATA_METHOD_DEBUG) {
		data_size += INT_DATA_INFO_SIZ;
		data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		cts_dev->fwdata.int_data_size = data_size;
		return 0;
	}

	cts_info("data_method:%d, data_type:%d", data_method, data_types);
	if (data_types != INT_DATA_TYPE_NONE) {
		data_size += INT_DATA_INFO_SIZ;
		if ((data_types & INT_DATA_TYPE_RAWDATA)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		}
		if ((data_types & INT_DATA_TYPE_MANUAL_DIFF)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		}
		if ((data_types & INT_DATA_TYPE_REAL_DIFF)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		}
		if ((data_types & INT_DATA_TYPE_NOISE_DIFF)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		}
		if ((data_types & INT_DATA_TYPE_BASEDATA)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U16_SIZ);
		}
		if ((data_types & INT_DATA_TYPE_CNEGDATA)) {
			data_size += (INT_DATA_TYPE_LEN_SIZ + INT_DATA_TYPE_U8_SIZ);
		}
	}

	cts_info("data_size: %d", data_size);
	cts_dev->fwdata.int_data_size = data_size;
	return 0;
}

static int cts_tcs_polling_data(const struct cts_device *cts_dev,
	u8 *buf, size_t size)
{
	int ret = -1;
	int retries = 100;
	u8 ready = 0;
	size_t data_size = cts_dev->fwdata.int_data_size;

	if (!data_size)
		data_size = TOUCH_INFO_SIZ + TCS_REPLY_TAIL_SIZ;

	do {
		ret = cts_tcs_get_data_ready_flag(cts_dev, &ready);
		if (!ret && ready)
			break;
		mdelay(10);
	} while (!ready && --retries);
	cts_info("get data rdy, retries left %d", retries);

	if (!ready) {
		cts_err("time out wait for data rdy");
		return -EIO;
	}

	retries = 3;
	do {
		ret = cts_tcs_spi_read_1_cs(cts_dev, TP_STD_CMD_GET_DATA_BY_POLLING_RO,
			cts_dev->rtdata.int_data, data_size);
		mdelay(1);
		if (cts_tcs_clr_data_ready_flag(cts_dev))
			cts_err("Clear data ready flag failed");
	} while (ret && --retries);

	return ret;
}

static int cts_tcs_polling_test_data(const struct cts_device *cts_dev,
	u8 *buf, size_t size)
{
	int ret = -EIO, retries = 5;
	int offset = TOUCH_INFO_SIZ + INT_DATA_INFO_SIZ + INT_DATA_TYPE_LEN_SIZ;

	while (retries--) {
		ret = cts_polling_data(cts_dev, buf, size);
		if (!ret) {
			memcpy(buf, cts_dev->rtdata.int_data + offset, size);
			break;
		}
	}

	return ret;
}

static int polling_data(struct cts_device *cts_dev, u8 *buf, size_t size,
	enum int_data_type type)
{
	u8 old_int_data_method;
	u16 old_int_data_types;
	int ret = -EIO, retries = 5;
	int offset = TOUCH_INFO_SIZ + INT_DATA_INFO_SIZ + INT_DATA_TYPE_LEN_SIZ;

	old_int_data_types = cts_dev->fwdata.int_data_types;
	old_int_data_method = cts_dev->fwdata.int_data_method;

	cts_set_int_data_types(cts_dev, type);
	cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);

	while (retries--) {
		ret = cts_polling_data(cts_dev, buf, size);
		if (!ret) {
			memcpy(buf, cts_dev->rtdata.int_data + offset, size);
			break;
		}
	}

	cts_set_int_data_method(cts_dev, old_int_data_method);
	cts_set_int_data_types(cts_dev, old_int_data_types);

	return ret;
}

int cts_test_polling_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
    int ret = -EIO, retries = 5;
    int offset = TOUCH_INFO_SIZ + INT_DATA_INFO_SIZ + INT_DATA_TYPE_LEN_SIZ;

    while (retries--) {
        ret = cts_polling_data(cts_dev, buf, size);
        if (!ret) {
			if (cts_dev->rtdata.int_data[TOUCH_INFO_SIZ + INT_DATA_VALID_SIZ]) {
	            memcpy(buf, cts_dev->rtdata.int_data + offset, size);
	            break;
			}
        }
    }

    return ret;
}

int cts_tcs_top_get_rawdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_RAWDATA);
}

int cts_tcs_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf,
	size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_MANUAL_DIFF);
}

int cts_tcs_top_get_real_diff(struct cts_device *cts_dev, u8 *buf, size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_REAL_DIFF);
}

int cts_tcs_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf,
	size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_NOISE_DIFF);
}

int cts_tcs_top_get_basedata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_BASEDATA);
}

int cts_tcs_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf, size_t size)
{
	return polling_data(cts_dev, buf, size, INT_DATA_TYPE_CNEGDATA);
}

static int cts_tcs_tp_reset(const struct cts_device *cts_dev)
{
	int ret;
	u8 buf[] = { 0x01, 0xfe };

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_RESET_WO,
		buf, sizeof(buf));
	if (ret != 0)
		return ret;

	return ret;
}

int cts_tcs_reset_device(const struct cts_device *cts_dev)
{
	bool use_soft_tp_reset;
	int ret;

	/*
	  ICNL9916: hw reset, use_soft_tp_reset = false;
	  ICNL9922: sw reset, use_soft_tp_reset = true;
	*/
	if (!cts_dev->hwdata)
		use_soft_tp_reset = false;
	else if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916)
		use_soft_tp_reset = false;
	else if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916C)
		use_soft_tp_reset = false;
	else if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9922)
		use_soft_tp_reset = true;
	else
		use_soft_tp_reset = false;

	cts_info("use_soft_tp_reset=%d", use_soft_tp_reset);
	if (use_soft_tp_reset) {
		/* normal */
		cts_info("tp reset in normal mode");
		ret = cts_tcs_tp_reset(cts_dev);
		if (!ret) {
			mdelay(40);
			return 0;
		}
		/* program */
		cts_info("tp reset in program mode");
		ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_RESET_CONFIG, 0xfd);
		if (!ret) {
			mdelay(40);
			return 0;
		}
		return ret;
	} else
		return cts_plat_reset_device(cts_dev->pdata);
}

int cts_tcs_set_int_test(const struct cts_device *cts_dev, u8 enable)
{
    int ret;

    ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_INT_TEST_EN_RW, &enable,
		sizeof(enable));
    if (!ret) {
	    return 0;
    }

    return -1;
}

int cts_tcs_set_int_pin(const struct cts_device *cts_dev, u8 high)
{
    int ret;

    ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_SET_INT_PIN_RW, &high,
		sizeof(high));
    if (!ret) {
	    return 0;
    }

    return -1;
}

int cts_tcs_get_module_id(const struct cts_device *cts_dev, u32 *modId)
{
    int ret;
    u8 buf[4];

    ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_INFO_MODULE_ID_RO,
		buf, sizeof(buf));
    if (!ret) {
	    *modId = *(u32 *)buf;
	    return 0;
    }
    return -1;
}


int cts_tcs_get_gestureinfo(const struct cts_device *cts_dev,
	struct cts_device_gesture_info *gesture_info)
{
    int ret;
    size_t size = sizeof(*gesture_info) + TCS_REPLY_TAIL_SIZ;

    ret = cts_tcs_spi_read_1_cs(cts_dev, TP_STD_CMD_TP_DATA_COORDINATES_RO,
		cts_dev->rtdata.int_data, size);
    if (cts_tcs_clr_gstr_ready_flag(cts_dev)) {
        cts_err("Clear gesture ready flag failed");
    }
    if (ret < 0) {
        cts_err("Get gesture info failed: ret=%d", ret);
        return ret;
    }
    if (!ret) {
        memcpy(gesture_info, cts_dev->rtdata.int_data, sizeof(*gesture_info));
    }

    return ret;
}

#ifdef CFG_DUMP_INT_DATA
extern void cts_dump_int_tsdata(const u8 *data, size_t size);
#endif

int cts_tcs_get_touchinfo(struct cts_device *cts_dev,
	struct cts_device_touch_info *touch_info)
{
    int ret = -1;
    size_t size = cts_dev->fwdata.int_data_size;
    u8 method = cts_dev->fwdata.int_data_method;

    if (!size)
        size = TOUCH_INFO_SIZ;

    memset(touch_info, 0, sizeof(*touch_info));

    ret = cts_tcs_spi_read_1_cs(cts_dev, TP_STD_CMD_TP_DATA_COORDINATES_RO,
		cts_dev->rtdata.int_data, size);
    if (ret) {
        cts_err("tcs_spi_read_1_cs failed");
        return ret;
    }

    memcpy(touch_info, cts_dev->rtdata.int_data, sizeof(*touch_info));

    if (method == INT_DATA_METHOD_HOST) {
#ifdef CFG_DUMP_INT_DATA
        if (cts_dev->rtdata.dumping) {
            if ((--cts_dev->rtdata.dump_cnt) == 0)
                cts_dev->rtdata.dumping = false;
            cts_dump_int_tsdata(cts_dev->rtdata.int_data, size - 5);
        }
#endif
        udelay(200);
    }

    return ret;
}


int cts_tcs_get_workmode(const struct cts_device *cts_dev, u8 *workmode)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_CURRENT_WORKMODE_RO,
		buf, sizeof(buf));
	if (!ret) {
		*workmode = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_workmode(const struct cts_device *cts_dev, u8 workmode)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_WORK_MODE_RW,
		&workmode, sizeof(workmode));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_set_openshort_mode(const struct cts_device *cts_dev, u8 mode)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_OPENSHORT_MODE_SEL_RW, &mode,
		sizeof(mode));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_set_tx_vol(const struct cts_device *cts_dev, u8 txvol)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_VSTIM_LVL_RW, &txvol,
		sizeof(txvol));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_is_enabled_get_rawdata(const struct cts_device *cts_dev,
	u8 *enabled)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW,
		buf, sizeof(buf));
	if (!ret) {
		*enabled = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_short_test_type(const struct cts_device *cts_dev,
	u8 short_type)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_OPENSHORT_SHORT_SEL_RW,
		&short_type, sizeof(short_type));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_is_openshort_enabled(const struct cts_device *cts_dev,
	u8 *enabled)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_OPENSHORT_EN_RW, buf,
		sizeof(buf));
	if (!ret) {
		*enabled = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_openshort_enable(const struct cts_device *cts_dev,
	u8 enable)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_OPENSHORT_EN_RW, &enable,
		sizeof(enable));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_set_esd_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_DDI_ESD_EN_RW, &enable,
		sizeof(enable));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_is_cneg_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
	int ret = -1;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_CNEG_EN_RW, enabled,
		sizeof(*enabled));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_is_mnt_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
	int ret = -1;

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_MNT_EN_RW, enabled,
		sizeof(*enabled));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_set_cneg_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_CNEG_EN_RW, &enable,
		sizeof(enable));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_set_mnt_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_MNT_EN_RW, &enable,
		sizeof(enable));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_is_display_on(const struct cts_device *cts_dev, u8 *display_on)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*display_on = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode)
{
	int ret;
	u8 buf[1];

	buf[0] = pwr_mode;
	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_PWR_STATE_RW,
		buf, sizeof(buf));
	return ret;
}

int cts_tcs_set_display_on(const struct cts_device *cts_dev, u8 display_on)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
		&display_on, sizeof(display_on));
	if (!ret)
		return 0;

	return ret;
}


int cts_tcs_set_charger_plug(const struct cts_device *cts_dev, u8 set)
{
	int ret;
	u8 buf[1];

	buf[0] = set ? 1 : 0;
	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_CHARGER_PLUGIN_RW,
		buf, sizeof(buf));
	if (!ret) {
		return 0;
	}

	return ret;
}

int cts_tcs_get_charger_plug(const struct cts_device *cts_dev, u8 *isset)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_CHARGER_PLUGIN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*isset = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_earjack_plug(const struct cts_device *cts_dev, u8 set)
{
	int ret;
	u8 buf[1];

	buf[0] = set ? 1 : 0;
	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_EP_PLUGIN_RW,
		buf, sizeof(buf));
	if (!ret) {
		return 0;
	}

	return ret;
}

int cts_tcs_get_earjack_plug(const struct cts_device *cts_dev, u8 *isset)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_EP_PLUGIN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*isset = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_panel_direction(const struct cts_device *cts_dev,
	u8 direction)
{
	int ret;
	u8 buf[1] = { direction };

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_PANEL_DIRECTION_RW,
		buf, sizeof(buf));
	if (!ret) {
		return 0;
	}

	return ret;
}

int cts_tcs_get_panel_direction(const struct cts_device *cts_dev,
	u8 *direction)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_PANEL_DIRECTION_RW,
		buf, sizeof(buf));
	if (!ret) {
		*direction = buf[0];
		return 0;
	}

	return ret;
}

int cts_tcs_set_game_mode(const struct cts_device *cts_dev, u8 enable)
{
	int ret;
	u8 buf[1] = { enable };

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_GAME_MODE_RW,
		buf, sizeof(buf));
	if (!ret) {
		return 0;
	}

	return ret;
}

int cts_tcs_get_game_mode(const struct cts_device *cts_dev, u8 *enabled)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_GAME_MODE_RW,
		buf, sizeof(buf));
	if (!ret) {
		*enabled = buf[0];
		return 0;
	}

	return ret;
}

/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
int cts_tcs_set_pocket_palm_mode(const struct cts_device *cts_dev, u8 enable)
{
	int ret;
	u8 buf[1] = { enable };

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_POCKET_MODE_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		return 0;
	}

	return ret;
}

int cts_tcs_get_pocket_palm_mode(const struct cts_device *cts_dev, u8 *enabled)
{
	int ret;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_POCKET_MODE_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*enabled = buf[0];
		return 0;
	}

	return ret;
}
/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/

int cts_tcs_read_sram_normal_mode(const struct cts_device *cts_dev,
	u32 addr, void *dst, size_t len, int retry, int delay)
{
	struct spi_message msg;
	struct spi_transfer xfer[2];
	struct chipone_ts_data *cts_data = container_of(cts_dev,
		struct chipone_ts_data, cts_dev);
	u16 crc;
	int ret;
	u8 tx[ALIGN(128, 4)];
	u8 rx[ALIGN(128, 4)];
	u8 tx1[ALIGN(128, 4)];

	/* Write add */
	tx[0] = CTS_DEV_NORMAL_MODE_SPIADDR;
	tx[1] = 0x01;
	tx[2] = 0x21;
	tx[3] = 0x04;
	tx[4] = 0x00;
	crc = cts_crc16(tx, 5);
	tx[5] = ((crc >> 0) & 0xFF);
	tx[6] = ((crc >> 8) & 0xFF);
	tx[7] = 0x01;
	tx[8] = ((addr >> 0) & 0xFF);
	tx[9] = ((addr >> 8) & 0xFF);
	tx[10] = ((addr >> 16) & 0xFF);
	crc = cts_crc16(&tx[7], 4);
	tx[11] = ((crc >> 0) & 0xFF);
	tx[12] = ((crc >> 8) & 0xFF);

	memset(&xfer[0], 0, sizeof(struct spi_transfer));
	xfer[0].delay_usecs = 0,
	xfer[0].speed_hz = cts_data->pdata->spi_speed * 1000u,
	xfer[0].tx_buf = tx,
	xfer[0].rx_buf = rx,
	xfer[0].len = 13,
    /**
     * xfer[0].tx_dma = 0,
     * xfer[0].rx_dma = 0,
     */
	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 0);
#endif
	ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 1);
#endif
	if (ret)
		cts_err("spi sync 1 failed %d", ret);
	udelay(100);

	memset(rx, 0, sizeof(rx));
	memset(&xfer[1], 0, sizeof(struct spi_transfer));
	xfer[1].delay_usecs = 0,
	xfer[1].speed_hz = cts_data->pdata->spi_speed * 1000u,
	xfer[1].tx_buf = tx1,
	xfer[1].rx_buf = rx,
	xfer[1].len = 5,
    /**
     * xfer[0].tx_dma = 0,
     * xfer[0].rx_dma = 0,
     */
	spi_message_init(&msg);
	spi_message_add_tail(&xfer[1], &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 0);
#endif
	ret = spi_sync(cts_data->spi_client, &msg);
#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(cts_dev->pdata, 1);
#endif
	if (ret)
		cts_err("spi sync 2 failed %d", ret);
	udelay(100);

	memset(tx, 0, sizeof(tx));
	memset(tx1, 0, sizeof(tx1));
	memset(rx, 0, sizeof(rx));
	/* Read data */
	tx[0] = CTS_DEV_NORMAL_MODE_SPIADDR | 0x01;
	tx[1] = 0x02;
	tx[2] = 0x41;
	tx[3] = ((len >> 0) & 0xFF);
	tx[4] = ((len >> 8) & 0xFF);
	crc = cts_crc16(tx, 5);
	tx[5] = ((crc >> 0) & 0xFF);
	tx[6] = ((crc >> 8) & 0xFF);

	memset(&xfer[0], 0, sizeof(struct spi_transfer));
	xfer[0].delay_usecs = 0,
	xfer[0].speed_hz = cts_data->pdata->spi_speed * 1000u,
	xfer[0].tx_buf = tx,
	xfer[0].rx_buf = rx,
	xfer[0].len = 7,
    /**
     * xfer[0].tx_dma = 0,
     * xfer[0].rx_dma = 0,
     */
	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	ret = spi_sync(cts_data->spi_client, &msg);
	if (ret)
		cts_err("spi sync 3 failed %d", ret);
	udelay(100);

	memset(rx, 0, sizeof(rx));
	memset(&xfer[1], 0, sizeof(struct spi_transfer));
	xfer[1].delay_usecs = 0,
	xfer[1].speed_hz = cts_data->pdata->spi_speed * 1000u,
	xfer[1].tx_buf = tx1,
	xfer[1].rx_buf = rx,
	xfer[1].len = len + 5,
    /**
     * xfer[0].tx_dma = 0,
     * xfer[0].rx_dma = 0,
     */
	spi_message_init(&msg);
	spi_message_add_tail(&xfer[1], &msg);
	ret = spi_sync(cts_data->spi_client, &msg);
	if (ret)
		cts_err("spi sync 4 failed %d", ret);
	udelay(100);

	memcpy(dst, rx, len);

	return ret;
}

int cts_tcs_init_int_data(struct cts_device *cts_dev)
{
	if (!cts_dev->rtdata.int_data) {
		cts_dev->rtdata.int_data = kmalloc(INT_DATA_MAX_SIZ, GFP_KERNEL);
		if (!cts_dev->rtdata.int_data) {
			cts_err("Malloc for int_data failed");
			return -ENOMEM;
		}
		return 0;
	}

	return 0;
}

int cts_tcs_deinit_int_data(struct cts_device *cts_dev)
{
    if (cts_dev->rtdata.int_data) {
	    kfree(cts_dev->rtdata.int_data);
		cts_dev->rtdata.int_data = NULL;
    }

	return 0;
}

int cts_tcs_get_has_int_data(const struct cts_device *cts_dev,
	bool *has_int_data)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO,
		buf, sizeof(buf));
	if (!ret) {
		*has_int_data = !!buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_get_int_data_types(const struct cts_device *cts_dev,
	u16 *int_data_types)
{
	int ret = -1;
	u8 buf[2];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
		buf, sizeof(buf));
	if (!ret) {
		*int_data_types = buf[0] | (buf[1] << 8);
		return 0;
	}
	return ret;
}

int cts_tcs_set_int_data_types(const struct cts_device *cts_dev,
	u16 int_data_types)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
		(u8 *) &int_data_types, sizeof(int_data_types));
	if (!ret)
		return 0;

	return ret;
}

int cts_tcs_get_int_data_method(const struct cts_device *cts_dev,
	u8 *int_data_method)
{
	int ret = -1;
	u8 buf[1];

	ret = cts_tcs_spi_read(cts_dev, TP_STD_CMD_SYS_STS_DATA_CAPTURE_EN_RW,
		buf, sizeof(buf));
	if (!ret) {
		*int_data_method = buf[0];
		return 0;
	}
	return ret;
}

int cts_tcs_set_int_data_method(const struct cts_device *cts_dev,
	u8 int_data_method)
{
	int ret = -1;

	ret = cts_tcs_spi_write(cts_dev, TP_STD_CMD_SYS_STS_DATA_CAPTURE_EN_RW,
		&int_data_method, sizeof(int_data_method));
	if (!ret)
		return 0;

	return ret;
}

struct cts_dev_ops tcs_ops = {
	.get_fw_ver				= cts_tcs_get_fw_ver,
	.get_lib_ver			= cts_tcs_get_lib_ver,
	.get_ddi_ver			= cts_tcs_get_ddi_ver,
	.get_res_x				= cts_tcs_get_res_x,
	.get_res_y				= cts_tcs_get_res_y,
	.get_rows				= cts_tcs_get_rows,
	.get_cols				= cts_tcs_get_cols,
	.get_flip_x				= cts_tcs_get_flip_x,
	.get_flip_y				= cts_tcs_get_flip_y,
	.get_swap_axes			= cts_tcs_get_swap_axes,
	.get_int_mode			= cts_tcs_get_int_mode,
	.get_int_keep_time		= cts_tcs_get_int_keep_time,
	.get_rawdata_target		= cts_tcs_get_rawdata_target,
	.get_esd_method			= cts_tcs_get_esd_method,
    .get_gestureinfo		= cts_tcs_get_gestureinfo,
	.get_touchinfo			= cts_tcs_get_touchinfo,
	.get_esd_protection		= cts_tcs_get_esd_protection,
	.get_data_ready_flag	= cts_tcs_get_data_ready_flag,
	.clr_data_ready_flag	= cts_tcs_clr_data_ready_flag,
	.enable_get_rawdata		= cts_tcs_enable_get_rawdata,
	.is_enabled_get_rawdata	= cts_tcs_is_enabled_get_rawdata,
	.disable_get_rawdata	= cts_tcs_disable_get_rawdata,
	.enable_get_cneg		= cts_tcs_enable_get_cneg,
	.disable_get_cneg		= cts_tcs_disable_get_cneg,
	.is_cneg_ready			= cts_tcs_is_cneg_ready,
	.quit_guesture_mode		= cts_tcs_quit_guesture_mode,
	.get_rawdata			= cts_tcs_get_rawdata,
	.get_diffdata			= cts_tcs_get_diffdata,
	.get_basedata			= cts_tcs_get_basedata,
	.get_cneg				= cts_tcs_get_cneg,
	.read_hw_reg			= cts_tcs_read_hw_reg,
	.write_hw_reg			= cts_tcs_write_hw_reg,
	.read_ddi_reg			= cts_tcs_read_ddi_reg,
	.write_ddi_reg			= cts_tcs_write_ddi_reg,
	.read_fw_reg			= cts_tcs_read_fw_reg,
	.write_fw_reg			= cts_tcs_write_fw_reg,
	.read_reg				= cts_tcs_read_reg,
	.write_reg				= cts_tcs_write_reg,
	.get_fw_id				= cts_tcs_get_fw_id,
	.get_workmode			= cts_tcs_get_workmode,
	.set_workmode			= cts_tcs_set_workmode,
	.set_openshort_mode		= cts_tcs_set_openshort_mode,
	.set_tx_vol				= cts_tcs_set_tx_vol,
	.set_short_test_type	= cts_tcs_set_short_test_type,
	.set_openshort_enable	= cts_tcs_set_openshort_enable,
	.is_openshort_enabled	= cts_tcs_is_openshort_enabled,
	.set_esd_enable			= cts_tcs_set_esd_enable,
	.set_cneg_enable		= cts_tcs_set_cneg_enable,
	.set_mnt_enable			= cts_tcs_set_mnt_enable,
	.is_display_on			= cts_tcs_is_display_on,
	.set_display_on			= cts_tcs_set_display_on,
	.is_cneg_enabled		= cts_tcs_is_cneg_enabled,
	.is_mnt_enabled			= cts_tcs_is_mnt_enabled,
	.set_pwr_mode			= cts_tcs_set_pwr_mode,
	.read_sram_normal_mode	= cts_tcs_read_sram_normal_mode,
	.init_int_data			= cts_tcs_init_int_data,
	.deinit_int_data		= cts_tcs_deinit_int_data,
	.get_has_int_data		= cts_tcs_get_has_int_data,
	.get_int_data_types		= cts_tcs_get_int_data_types,
	.set_int_data_types		= cts_tcs_set_int_data_types,
	.get_int_data_method	= cts_tcs_get_int_data_method,
	.set_int_data_method	= cts_tcs_set_int_data_method,
	.calc_int_data_size		= cts_tcs_calc_int_data_size,
	.polling_data			= cts_tcs_polling_data,
	.polling_test_data		= cts_tcs_polling_test_data,

	.top_get_rawdata		= cts_tcs_top_get_rawdata,
	.top_get_manual_diff	= cts_tcs_top_get_manual_diff,
	.top_get_real_diff		= cts_tcs_top_get_real_diff,
	.top_get_noise_diff		= cts_tcs_top_get_noise_diff,
	.top_get_basedata		= cts_tcs_top_get_basedata,
	.top_get_cnegdata		= cts_tcs_top_get_cnegdata,

	.reset_device			= cts_tcs_reset_device,

	.set_int_test			= cts_tcs_set_int_test,
	.set_int_pin			= cts_tcs_set_int_pin,
	.get_module_id			= cts_tcs_get_module_id,
	.set_charger_plug		= cts_tcs_set_charger_plug,
	.get_charger_plug		= cts_tcs_get_charger_plug,
	.set_earjack_plug		= cts_tcs_set_earjack_plug,
	.get_earjack_plug		= cts_tcs_get_earjack_plug,
	.set_panel_direction	= cts_tcs_set_panel_direction,
	.get_panel_direction	= cts_tcs_get_panel_direction,
	.set_game_mode			= cts_tcs_set_game_mode,
	.get_game_mode			= cts_tcs_get_game_mode,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
	.set_pocket_palm_mode	= cts_tcs_set_pocket_palm_mode,
	.get_pocket_palm_mode	= cts_tcs_get_pocket_palm_mode,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/
	.spi_xtrans				= cts_tcs_spi_xtrans,
};
