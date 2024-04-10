#define LOG_TAG         "Core"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_sysfs.h"
#include "cts_firmware.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"
#include "cts_tcs.h"

#ifdef CONFIG_LCT_EARJACK_DETECT
static void cts_ts_earphone_plugin_work_func(struct work_struct *work);
DECLARE_WORK(cts_earphone_plugin_work, cts_ts_earphone_plugin_work_func);

void cts_ts_earphone_event_callback(void)
{
	schedule_work(&cts_earphone_plugin_work);
}
static void cts_ts_earphone_plugin_work_func(struct work_struct *work)
{
        struct chipone_ts_data *cts_data = g_cts_data; 
	struct cts_device *cts_dev = &cts_data->cts_dev;
	if (cts_dev->rtdata.suspended) {
		cts_err("earphone tp is suspend,can not be set\n");
		return;
	}
      cts_info("earphone_plugged_in=%d", g_touchscreen_earphone_pulgin.earphone_plugged_in);
    cts_tcs_set_earjack_plug(cts_dev, g_touchscreen_earphone_pulgin.earphone_plugged_in);  
	return;
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
static int cts_i2c_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8 buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%02x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2,
        addr, b);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writeb invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 1, retry, delay);
}

static int cts_i2c_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%04x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2,
        addr, w);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writew invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_i2c_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%08x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2,
        addr, l);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writel invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
}

static int cts_i2c_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
    int ret;
    u8 *data;
    size_t max_xfer_size;
    size_t payload_len;
    size_t xfer_len;

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x len: %zu",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2,
        addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    data = cts_plat_get_i2c_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2)
            put_unaligned_be16(addr, data);
        else if (cts_dev->rtdata.addr_width == 3)
            put_unaligned_be24(addr, data);
        else {
            cts_err("Writesb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_i2c_write(cts_dev->pdata,
                cts_dev->rtdata.slave_addr, data,
                xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c write failed %d", ret);
            return ret;
        }

        src += payload_len;
        len -= payload_len;
        addr += payload_len;
    }

    return 0;
}

static int cts_i2c_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

    cts_dbg("Readb from slave_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readb invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_i2c_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    u8 buff[2];

    cts_dbg("Readw from slave_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readw invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0)
        *w = get_unaligned_le16(buff);

    return ret;
}

static int cts_i2c_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    u8 buff[4];

    cts_dbg("Readl from slave_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readl invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0)
        *l = get_unaligned_le32(buff);

    return ret;
}

static int cts_i2c_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

    cts_dbg("Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2,
        addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2)
            put_unaligned_be16(addr, addr_buf);
        else if (cts_dev->rtdata.addr_width == 3)
            put_unaligned_be24(addr, addr_buf);
        else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_i2c_read(cts_dev->pdata,
                cts_dev->rtdata.slave_addr, addr_buf,
                cts_dev->rtdata.addr_width, dst,
                xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst += xfer_len;
        len -= xfer_len;
        addr += xfer_len;
    }

    return 0;
}
#else
static int cts_spi_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8 buff[8];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writeb invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 1, retry, delay);
    return 0;
}

static int cts_spi_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[8];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writew invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
    return 0;
}

static int cts_spi_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[8];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, buff);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, buff);
    else {
        cts_err("Writel invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
    return 0;
}

static int cts_spi_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
    int ret;
    u8 *data;
    size_t max_xfer_size;
    size_t payload_len;
    size_t xfer_len;

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    data = cts_plat_get_spi_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2)
            put_unaligned_be16(addr, data);
        else if (cts_dev->rtdata.addr_width == 3)
            put_unaligned_be24(addr, data);
        else {
            cts_err("Writesb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                data, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c write failed %d", ret);
            return ret;
        }

        src += payload_len;
        len -= payload_len;
        addr += payload_len;
    }
    return 0;
}

static int cts_spi_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readb invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_spi_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    u8 buff[2];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readw invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0)
        *w = get_unaligned_le16(buff);

    return ret;
}

static int cts_spi_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    u8 buff[4];

    if (cts_dev->rtdata.addr_width == 2)
        put_unaligned_be16(addr, addr_buf);
    else if (cts_dev->rtdata.addr_width == 3)
        put_unaligned_be24(addr, addr_buf);
    else {
        cts_err("Readl invalid address width %u", cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0)
        *l = get_unaligned_le32(buff);

    return ret;
}

static int cts_spi_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2)
            put_unaligned_be16(addr, addr_buf);
        else if (cts_dev->rtdata.addr_width == 3)
            put_unaligned_be24(addr, addr_buf);
        else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read(cts_dev->pdata,
                cts_dev->rtdata.slave_addr, addr_buf,
                cts_dev->rtdata.addr_width, dst,
                xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst += xfer_len;
        len -= xfer_len;
        addr += xfer_len;
    }
    return 0;
}

static int cts_spi_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay, int idle)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2)
            put_unaligned_be16(addr, addr_buf);
        else if (cts_dev->rtdata.addr_width == 3)
            put_unaligned_be24(addr, addr_buf);
        else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read_delay_idle(cts_dev->pdata,
                cts_dev->rtdata.slave_addr, addr_buf,
                cts_dev->rtdata.addr_width,
                dst, xfer_len, retry, delay, idle);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst += xfer_len;
        len -= xfer_len;
        addr += xfer_len;
    }
    return 0;
}

#endif /* CONFIG_CTS_I2C_HOST */

int cts_dev_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writeb(cts_dev, addr, b, retry, delay);
#else
    return cts_spi_writeb(cts_dev, addr, b, retry, delay);
#endif
}

static inline int cts_dev_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writew(cts_dev, addr, w, retry, delay);
#else
    return cts_spi_writew(cts_dev, addr, w, retry, delay);
#endif
}

static inline int cts_dev_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writel(cts_dev, addr, l, retry, delay);
#else
    return cts_spi_writel(cts_dev, addr, l, retry, delay);
#endif
}

static inline int cts_dev_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writesb(cts_dev, addr, src, len, retry, delay);
#else
    return cts_spi_writesb(cts_dev, addr, src, len, retry, delay);
#endif
}

int cts_dev_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readb(cts_dev, addr, b, retry, delay);
#else
    return cts_spi_readb(cts_dev, addr, b, retry, delay);
#endif
}

static inline int cts_dev_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readw(cts_dev, addr, w, retry, delay);
#else
    return cts_spi_readw(cts_dev, addr, w, retry, delay);
#endif
}

static inline int cts_dev_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readl(cts_dev, addr, l, retry, delay);
#else
    return cts_spi_readl(cts_dev, addr, l, retry, delay);
#endif
}

static inline int cts_dev_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else
    return cts_spi_readsb(cts_dev, addr, dst, len, retry, delay);
#endif
}

static inline int cts_dev_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay, int idle)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else
    return cts_spi_readsb_delay_idle(cts_dev, addr, dst, len, retry, delay,
            idle);
#endif
}

#ifdef CFG_CTS_UPDATE_CRCCHECK
int cts_sram_writesb_boot_crc_retry(const struct cts_device *cts_dev,
        size_t len, u32 crc, int retry)
{
    int ret = 0, retries;
    u32 addr[3];

    if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9922C) {
        addr[0] = 0x02bff0;
        addr[1] = 0x02bff8;
        addr[2] = 0x02bffc;
    } else {
        addr[0] = 0x01fff0;
        addr[1] = 0x01fff8;
        addr[2] = 0x01fffc;
    }

    retries = 0;
    do {
        ret = cts_dev_writel(cts_dev, addr[0], 0xCC33555A, 3, 1);
        if (ret != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        ret = cts_dev_writel(cts_dev, addr[1], crc, 3, 1);
        if (ret != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        ret = cts_dev_writel(cts_dev, addr[2], len, 3, 1);
        if (ret != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        break;
    } while (retries++ < retry);

    return ret;
}
#endif

static int cts_write_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    int i, ret;
    u8 buff[5];

    for (i = 0; i < len; i++) {
        put_unaligned_le32(addr, buff);
        buff[4] = *(u8 *) src;

        addr++;
        src++;

        ret = cts_dev_writesb(cts_dev, CTS_DEVICE_FW_REG_DEBUG_INTF,
                buff, 5, retry, delay);
        if (ret) {
            cts_err("Write rDEBUG_INTF len=5B failed %d", ret);
            return ret;
        }
    }

    return 0;
}

int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode)
        return cts_dev_writeb(cts_dev, addr, b, retry, delay);
    else
        return cts_write_sram_normal_mode(cts_dev, addr, &b, 1, retry, delay);
}

int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[2];

    if (cts_dev->rtdata.program_mode)
        return cts_dev_writew(cts_dev, addr, w, retry, delay);
    else {
        put_unaligned_le16(w, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
    }
}

int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[4];

    if (cts_dev->rtdata.program_mode)
        return cts_dev_writel(cts_dev, addr, l, retry, delay);
    else {
        put_unaligned_le32(l, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
    }
}

int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode)
        return cts_dev_writesb(cts_dev, addr, src, len, retry, delay);
    else
        return cts_write_sram_normal_mode(cts_dev, addr, src, len, retry, delay);
}

static int cts_calc_sram_crc(const struct cts_device *cts_dev,
        u32 sram_addr, size_t size, u32 *crc)
{
    cts_info("Calc crc from sram 0x%06x size %zu", sram_addr, size);

    return cts_dev->hwdata->sfctrl->ops->calc_sram_crc(cts_dev,
            sram_addr, size, crc);
}

int cts_sram_writesb_check_crc_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, u32 crc, int retry)
{
    int ret, retries;
    u32 fw_crc;
    u8 tdata[16];
    u8 magic_num[] = { 0x5A, 0x55, 0x33, 0xCC, 0x00, 0x00, 0x00, 0x00 };

    retries = 0;
    do {
        u32 crc_sram;

        retries++;

        ret = cts_sram_writesb(cts_dev, 0, src, len);
        if (ret != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        fw_crc = cts_crc32(src, len);
        cts_info("fw crc: 0x%04x", fw_crc);
        memcpy(tdata, magic_num, 8);
        memcpy(tdata + 8, &fw_crc, 4);
        memcpy(tdata + 12, &len, 4);

        ret = cts_sram_writesb_boot_crc_retry(cts_dev, len, fw_crc, 3);
        if (ret != 0) {
            cts_err("cts_hw_reg_writesb_retry %d", ret);
            continue;
        }

        ret = cts_calc_sram_crc(cts_dev, 0, len, &crc_sram);
        if (ret != 0) {
            cts_err("Get CRC for sram writesb failed %d retries %d",
                ret, retries);
            continue;
        }

        if (crc == crc_sram)
            return 0;

        cts_err("Check CRC for sram writesb mismatch %x != %x retries %d",
            crc, crc_sram, retries);
        ret = -EFAULT;
    } while (retries < retry);

    return ret;
}

int cts_read_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        ret = cts_dev_writel(cts_dev, CTS_DEVICE_FW_REG_DEBUG_INTF,
            addr, retry, delay);
        if (ret) {
            cts_err("Write addr to rDEBUG_INTF failed %d", ret);
            return ret;
        }

        ret = cts_dev_readb(cts_dev, CTS_DEVICE_FW_REG_DEBUG_INTF + 4,
                (u8 *) dst, retry, delay);
        if (ret) {
            cts_err("Read value from rDEBUG_INTF + 4 failed %d", ret);
            return ret;
        }

        addr++;
        dst++;
    }

    return 0;
}

int cts_sram_readb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode)
        return cts_dev_readb(cts_dev, addr, b, retry, delay);
    else
        return cts_read_sram_normal_mode(cts_dev, addr, b, 1, retry, delay);
}

int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8 buff[2];

    if (cts_dev->rtdata.program_mode)
        return cts_dev_readw(cts_dev, addr, w, retry, delay);
    else {
        ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
        if (ret) {
            cts_err("SRAM readw in normal mode failed %d", ret);
            return ret;
        }

        *w = get_unaligned_le16(buff);
        return 0;
    }
}

int cts_sram_readl_retry(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8 buff[4];

    if (cts_dev->rtdata.program_mode)
        return cts_dev_readl(cts_dev, addr, l, retry, delay);
    else {
        ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
        if (ret) {
            cts_err("SRAM readl in normal mode failed %d", ret);
            return ret;
        }

        *l = get_unaligned_le32(buff);

        return 0;
    }
}

int cts_sram_readsb_retry(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode)
        return cts_dev_readsb(cts_dev, addr, dst, len, retry, delay);
    else
        return cts_read_sram_normal_mode(cts_dev, addr, dst, len, retry, delay);
}

int cts_fw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writeb to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writeb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writew to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writew(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writel to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writel(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writesb to fw reg 0x%04x under program mode",
            reg_addr);
        return -ENODEV;
    }

    return cts_dev_writesb(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_fw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readb from fw reg under program mode");
        return -ENODEV;
    }

    return cts_dev_readb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readw from fw reg under program mode");
        return -ENODEV;
    }

    return cts_dev_readw(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readl from fw reg under program mode");
        return -ENODEV;
    }

    return cts_dev_readl(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readsb from fw reg under program mode");
        return -ENODEV;
    }

    return cts_dev_readsb(cts_dev, reg_addr, dst, len, retry, delay);
}

int cts_fw_reg_readsb_retry_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay, int idle)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readsb from fw reg under program mode");
        return -ENODEV;
    }

    return cts_dev_readsb_delay_idle(cts_dev, reg_addr, dst, len, retry,
            delay, idle);
}

int cts_hw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    return cts_sram_writeb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    return cts_sram_writew_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    return cts_sram_writel_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    return cts_sram_writesb_retry(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_hw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    return cts_sram_readb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    return cts_sram_readw_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    return cts_sram_readl_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    return cts_sram_readsb_retry(cts_dev, reg_addr, dst, len, retry, delay);
}

static int icnl9916_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8 access_flag;

    cts_info("ICNL9916 %s access ddi reg", enable ? "enable" : "disable");

    ret = cts_hw_reg_readb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, &access_flag);
    if (ret) {
        cts_err("Read HW_REG_DDI_REG_CTRL failed %d", ret);
        return ret;
    }

    access_flag = enable ? (access_flag | 0x01) : (access_flag & (~0x01));
    ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
    if (ret) {
        cts_err("Write HW_REG_DDI_REG_CTRL %02x failed %d", access_flag, ret);
        return ret;
    }

    ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_HPPROG, enable ? 1 : 0);
    if (ret) {
        cts_err("Write CTS_DEV_HW_REG_HPPROG failed %d", ret);
        return ret;
    }

    ret = cts_hw_reg_writew(cts_dev, 0x3DFF0, enable ? 0x595A : 0x5A5A);
    if (ret) {
        cts_err("Write password to F0 failed %d", ret);
        return ret;
    }
    ret = cts_hw_reg_writew(cts_dev, 0x3DFF4, enable ? 0xA6A5 : 0x5A5A);
    if (ret) {
        cts_err("Write password to F1 failed %d", ret);
        return ret;
    }

    return 0;
}

const static struct cts_sfctrl icnl9916_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = 96 * 1024,
    .xchg_sram_size = 32 * 1024,    /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9916c_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = 96 * 1024,
    .xchg_sram_size = 32 * 1024,    /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9922_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = 96 * 1024,
    .xchg_sram_size = 32 * 1024,    /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9922c_sfctrl = {
    .reg_base = 0x74000,
    .xchg_sram_base = 0x24000,
    .xchg_sram_size = 16 * 1024,    /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9951_sfctrl = {
    .reg_base = 0x74000,
    .xchg_sram_base = 160 * 1024,
    .xchg_sram_size = 64 * 1024,    /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_device_hwdata cts_device_hwdatas[] = {
    {
        .name = "ICNL9916",
        .hwid = CTS_DEV_HWID_ICNL9916,
        .fwid = CTS_DEV_FWID_ICNL9916,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 96 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9916_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },
    {
        .name = "ICNL9916C",
        .hwid = CTS_DEV_HWID_ICNL9916C,
        .fwid = CTS_DEV_FWID_ICNL9916C,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 96 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9916c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9922",
        .hwid = CTS_DEV_HWID_ICNL9922,
        .fwid = CTS_DEV_FWID_ICNL9922,
        .num_row = 36,
        .num_col = 18,
        .sram_size = 96 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9922_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9922C",
        .hwid = CTS_DEV_HWID_ICNL9922C,
        .fwid = CTS_DEV_FWID_ICNL9922C,
        .num_row = 36,
        .num_col = 18,
        .sram_size = 96 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9922c_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    },

    {
        .name = "ICNL9951",
        .hwid = CTS_DEV_HWID_ICNL9951,
        .fwid = CTS_DEV_FWID_ICNL9951,
        .num_row = 36,
        .num_col = 22,
        .sram_size = 128 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9951_sfctrl,
        .enable_access_ddi_reg = icnl9916_set_access_ddi_reg,
    }
};

static int cts_init_device_hwdata(struct cts_device *cts_dev,
        u32 hwid, u16 fwid)
{
    int i;

    cts_info("Init hardware data hwid: %06x fwid: %04x", hwid, fwid);

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (hwid == cts_device_hwdatas[i].hwid ||
        fwid == cts_device_hwdatas[i].fwid) {
            cts_dev->hwdata = &cts_device_hwdatas[i];
            return 0;
        }
    }

    return -EINVAL;
}

void cts_lock_device(const struct cts_device *cts_dev)
{
    cts_dbg("*** Lock ***");

    mutex_lock(&cts_dev->pdata->dev_lock);
}

void cts_unlock_device(const struct cts_device *cts_dev)
{
    cts_dbg("### Un-Lock ###");

    mutex_unlock(&cts_dev->pdata->dev_lock);
}

int cts_send_command(const struct cts_device *cts_dev, u8 cmd)
{
    cts_info("Send command 0x%02x", cmd);

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Send command %u while chip in program mode", cmd);
        return -ENODEV;
    }

    return cts_fw_reg_writeb_retry(cts_dev, CTS_DEVICE_FW_REG_CMD, cmd, 3, 0);
}

static int cts_get_touchinfo(struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info)
{
    cts_dbg("Get touch info");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get touch info in program mode");
        return -ENODEV;
    }

    return cts_tcs_get_touchinfo(cts_dev, touch_info);
}

static int cts_get_dev_boot_mode(const struct cts_device *cts_dev,
        u8 *boot_mode)
{
    int ret;

    if (cts_dev->rtdata.program_mode)
        ret = cts_hw_reg_readb_retry(cts_dev, CTS_DEV_HW_REG_CURRENT_MODE,
                boot_mode, 5, 10);
    else
        ret = cts_tcs_read_hw_reg(cts_dev, CTS_DEV_HW_REG_CURRENT_MODE,
                boot_mode, 1);

    if (ret) {
        cts_err("Read boot mode failed %d", ret);
        return ret;
    }

    *boot_mode &= CTS_DEV_BOOT_MODE_MASK;

    cts_info("Curr dev boot mode: %u(%s)", *boot_mode,
        cts_dev_boot_mode2str(*boot_mode));
    return 0;
}

static int cts_set_dev_boot_mode(const struct cts_device *cts_dev, u8 boot_mode)
{
    int ret;

    cts_info("Set dev boot mode to %u(%s)", boot_mode,
        cts_dev_boot_mode2str(boot_mode));

    ret = cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_BOOT_MODE,
            boot_mode, 5, 5);
    if (ret) {
        cts_err("Write hw register BOOT_MODE failed %d", ret);
        return ret;
    }

    return 0;
}

static int cts_init_fwdata(struct cts_device *cts_dev)
{

    struct cts_device_fwdata *fwdata = &cts_dev->fwdata;
    int ret;
    u8 tp_info_buf[64] = {0};
    cts_info("Init firmware data");

    if (cts_dev->rtdata.program_mode) {
        cts_err("Init firmware data while in program mode");
        return -EINVAL;
    }

    memset(fwdata, 0, sizeof(*fwdata));

    ret = cts_tcs_get_fw_ver(cts_dev, &fwdata->version);
    if (ret < 0) {
        cts_err("get_fw_ver failed");
        return -EINVAL;
    }
    sprintf(tp_info_buf, "[Vendor]huaxing,[FW]0x%02x,[IC]icnl9916\n", (fwdata->version)&0x00ff);
	update_lct_tp_info(tp_info_buf, NULL);
    ret = cts_tcs_get_lib_ver(cts_dev, &fwdata->lib_version);
    if (ret < 0) {
        cts_err("get_lib_ver failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_ddi_ver(cts_dev, &fwdata->ddi_version);
    if (ret < 0) {
        cts_err("get_ddi_ver failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_res_x(cts_dev, &fwdata->res_x);
    if (ret < 0) {
        cts_err("get_res_x failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_res_y(cts_dev, &fwdata->res_y);
    if (ret < 0) {
        cts_err("get_res_y failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_rows(cts_dev, &fwdata->rows);
    if (ret < 0) {
        cts_err("get_rows failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_cols(cts_dev, &fwdata->cols);
    if (ret < 0) {
        cts_err("get_cols failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_flip_x(cts_dev, &fwdata->flip_x);
    if (ret < 0) {
        cts_err("get_flip_x failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_flip_y(cts_dev, &fwdata->flip_y);
    if (ret < 0) {
        cts_err("get_flip_y failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_swap_axes(cts_dev, &fwdata->swap_axes);
    if (ret < 0) {
        cts_err("get_swap_axes failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_int_mode(cts_dev, &fwdata->int_mode);
    if (ret < 0) {
        cts_err("get_int_mode failed");
        return -EINVAL;
    }
    ret = cts_tcs_get_int_keep_time(cts_dev, &fwdata->int_keep_time);
    if (ret < 0) {
        cts_err("get_int_keep_time failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_rawdata_target(cts_dev, &fwdata->rawdata_target);
    if (ret < 0) {
        cts_err("get_rawdata_target failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_esd_method(cts_dev, &fwdata->esd_method);
    if (ret < 0) {
        cts_err("get_esd_method failed");
        return -EINVAL;
    }

    ret = cts_tcs_get_has_int_data(cts_dev,
            &fwdata->has_int_data);
    if (ret < 0) {
        cts_err("get_has_int_data failed: %d", ret);
        return -EINVAL;
    }

    if (fwdata->has_int_data) {
        ret = cts_tcs_get_int_data_method(cts_dev,
                &fwdata->int_data_method);
        if (ret < 0) {
            cts_err("get_int_data_method failed: %d", ret);
            return -EINVAL;
        }
        if (fwdata->int_data_method >= INT_DATA_METHOD_CNT) {
            cts_err("Invalid int data method: %d",
                fwdata->int_data_method);
            return -EINVAL;
        }

        ret = cts_tcs_get_int_data_types(cts_dev,
                &fwdata->int_data_types);
        if (ret < 0) {
            cts_err("get_int_data_types failed: %d", ret);
            return -EINVAL;
        }
        fwdata->int_data_types &= INT_DATA_TYPE_MASK;

        ret = cts_tcs_calc_int_data_size(cts_dev);
        if (ret < 0) {
            cts_err("calc_int_data_size failed: %d", ret);
            return -EINVAL;
        }
    }

    cts_err("fwver: %04x", fwdata->version);
    cts_err("libver: %04x", fwdata->lib_version);
    cts_err("ddi_version: %02x", fwdata->ddi_version);
    cts_err("res_x: %d", fwdata->res_x);
    cts_err("res_y: %d", fwdata->res_y);
    cts_err("rows: %d", fwdata->rows);
    cts_err("cols: %d", fwdata->cols);
    cts_err("flip_x: %d", fwdata->flip_x);
    cts_err("flip_y: %d", fwdata->flip_y);
    cts_err("swap_axes: %d", fwdata->swap_axes);
    cts_err("int_mode: %d", fwdata->int_mode);
    cts_err("int_keep_time: %d", fwdata->int_keep_time);
    cts_err("rawdata_target: %d", fwdata->rawdata_target);
    cts_err("esd_method: %d", fwdata->esd_method);
    cts_err("has_int_data: %d", fwdata->has_int_data);
    cts_err("int_data_method: %d", fwdata->int_data_method);
    cts_err("int_data_types: %d", fwdata->int_data_types);
    cts_err("int_data_size: %ld", fwdata->int_data_size);

    return 0;
}

#ifdef CFG_CTS_FW_LOG_REDIRECT
void cts_show_fw_log(struct cts_device *cts_dev)
{
    u8 len, max_len;
    int ret;
    u8 *data;

    max_len = cts_plat_get_max_fw_log_size(cts_dev->pdata);
    data = cts_plat_get_fw_log_buf(cts_dev->pdata, max_len);
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO + 1, &len, 1);
    if (ret) {
        cts_err("Get i2c print buf len error");
        return;
    }
    if (len >= max_len)
        len = max_len - 1;

    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO + 2, data, len);
    if (ret) {
        cts_err("Get i2c print buf error");
        return;
    }
    data[len] = '\0';
    printk("CTS-FW_LOG %s", data);
    cts_fw_log_show_finish(cts_dev);
}
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
static bool cts_is_proximity_enable(struct cts_device *cts_dev)
{
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &cts_dev->rtdata.firmware_status;
    return !!status->proximity;
}

static int cts_enable_proximity_mode(struct cts_device *cts_dev)
{
    return cts_tcs_set_proximity_mode(cts_dev, 1);
}
/*
static int cts_exit_proximity_mode(struct cts_device *cts_dev)
{
    return cts_tcs_set_proximity_mode(cts_dev, 0);
}*/

static void cts_handle_proximity_event(bool status)
{
    cts_info("Handler proximity '%s' event", status ? "Near" : "Far");
}
#endif

int cts_irq_handler(struct cts_device *cts_dev)
{
    struct cts_device_touch_info *touch_info;
    u8 pwrmode = 3;
    int ret;

    cts_dbg("Enter IRQ handler");

    if (cts_dev->rtdata.program_mode) {
        cts_err("IRQ triggered in program mode");
        return -EINVAL;
    }

    touch_info = &cts_dev->rtdata.touch_info;
    ret = cts_get_touchinfo(cts_dev, touch_info);
    if (ret) {
        cts_err("Get touch info failed %d", ret);
        return ret;
    }

    if (unlikely(cts_dev->rtdata.suspended)) {
#ifdef CFG_CTS_GESTURE
        if (cts_dev->rtdata.gesture_wakeup_enabled) {
            struct cts_device_gesture_info *gesture_info;

            gesture_info = &cts_dev->rtdata.gesture_info;
            memcpy(gesture_info, touch_info, sizeof(struct cts_device_gesture_info));

            ret = cts_plat_process_gesture_info(cts_dev->pdata, gesture_info);

            if (cts_dev->fwdata.int_data_method != INT_DATA_METHOD_HOST) {
                if (ret)
                    cts_err("Process gesture info failed %d", ret);

                ret = cts_tcs_set_pwr_mode(cts_dev, pwrmode);
                if (ret)
                    cts_warn("Reenter suspend with gesture wakeup failed %d", ret);

                if (cts_tcs_clr_gstr_ready_flag(cts_dev))
                    cts_err("Clear gesture ready flag failed");
            }
        } else {
            cts_warn("IRQ triggered while device suspended "
                "without gesture wakeup enable");
        }
#endif /* CFG_CTS_GESTURE */
    } else {
        cts_dbg("Touch info: vkey_state %x, num_msg %u",
            touch_info->vkey_state, touch_info->num_msg);

#ifdef CFG_CTS_PALM_DETECT
        if (CFG_CTS_PALM_ID == touch_info->vkey_state) {
            cts_report_palm_event(cts_dev->pdata);
            cts_plat_release_all_touch(cts_dev->pdata);
            return 0;
        }
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
        if (cts_is_proximity_enable(cts_dev)) {
            cts_dbg("proximity status:%d", cts_dev->rtdata.proximity_status);
            if (!cts_dev->rtdata.proximity_status
            && touch_info->vkey_state == CTS_CMD_PROXIMITY_STATUS) {
                cts_dev->rtdata.proximity_num++;
                if (cts_dev->rtdata.proximity_num == 4) {
                    cts_dev->rtdata.proximity_num = 0;
                    cts_dev->rtdata.proximity_status = true;
                    cts_handle_proximity_event(true);
                    if(ps_send_touch_event_probe == true){
	    if(tp_ps_send_touch_event!=NULL){
		(*tp_ps_send_touch_event)(0);
			}
		}
		    return 0;
                }
            } else if (cts_dev->rtdata.proximity_status
            && (touch_info->vkey_state == 0)) {
                cts_dev->rtdata.proximity_num = 0;
                cts_dev->rtdata.proximity_status = false;
                cts_handle_proximity_event(false);
                if(ps_send_touch_event_probe == true){
	    if(tp_ps_send_touch_event!=NULL){
		(*tp_ps_send_touch_event)(1);
			}
		}
		cts_plat_release_all_touch(cts_dev->pdata);
                return 0;
            }
        }
#endif

        ret = cts_plat_process_touch_msg(cts_dev->pdata, touch_info->msgs,
                touch_info->num_msg);
        if (ret) {
            cts_err("Process touch msg failed %d", ret);
            return ret;
        }
#ifdef CONFIG_CTS_VIRTUALKEY
        ret = cts_plat_process_vkey(cts_dev->pdata, touch_info->vkey_state);
        if (ret) {
            cts_err("Process vkey failed %d", ret);
            return ret;
        }
#endif
    }

    return 0;
}

bool cts_is_device_suspended(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.suspended;
}

int cts_suspend_device(struct cts_device *cts_dev)
{
    int ret;
    u8 buf;

    cts_info("Suspend device");

/* Disable this check for sleep/gesture switch */
/*
    if (cts_dev->rtdata.suspended) {
        cts_warn("Suspend device while already suspended");
        return 0;
    }
*/
#ifdef CONFIG_CTS_TP_PROXIMITY
 if (cts_is_proximity_enable(cts_dev)){
        cts_info("Proximity is enable in suspend");
        return 0;
 }
#endif
    if (cts_dev->rtdata.program_mode) {
        cts_info("Quit programming mode before suspend");
        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Failed to exit program mode before suspend:%d", ret);
            return ret;
        }
    }

    cts_info("Set suspend mode:%s",
        cts_dev->rtdata.gesture_wakeup_enabled ? "gesture" : "sleep");

    buf = cts_dev->rtdata.gesture_wakeup_enabled ? 3 : 2;
    ret = cts_tcs_set_pwr_mode(cts_dev, buf);
    if (ret) {
        cts_err("Suspend device failed %d", ret);
        return ret;
    }

    cts_info("Device suspended ...");
    cts_dev->rtdata.suspended = true;

    return 0;
}

int cts_resume_device(struct cts_device *cts_dev)
{
    u8 data[4] = {'R', 'S', 'T', '!'};
    int retries = 3;
    int ret = 0;

    cts_info("Resume device");
#ifdef CONFIG_CTS_TP_PROXIMITY
    if (cts_is_proximity_enable(cts_dev)) {
        cts_enable_proximity_mode(cts_dev);
        cts_info("Proximity is enable in resume");
        if (cts_is_charger_exist(cts_dev)) {
            int r = cts_set_dev_charger_attached(cts_dev, true);
            if (r)
                cts_err("Set dev charger attached failed %d", r);
        }
        ret = cts_plat_disable_irq_wake(g_cts_data->pdata);
        if (ret) {
            cts_err("Disable IRQ wake failed %d", ret);
        }
       if (!cts_dev->rtdata.suspended) {
		cts_err("suspend is false, return");
		return 0;
            }
    }
#endif
    /* Check whether device is in normal mode */
    while (--retries >= 0) {
#ifdef CFG_CTS_HAS_RESET_PIN
        ret = cts_tcs_write_hw_reg(cts_dev, CTS_DEV_HW_REG_SET_RESET,
                data, sizeof(data));
        if (ret) {
            cts_err("write RST failed");
        }
        cts_reset_device(cts_dev);
#endif
        cts_set_normal_addr(cts_dev);

        if (cts_plat_is_normal_mode(cts_dev->pdata)) {
            break;
        }
    }

    if (retries < 0) {
        const struct cts_firmware *firmware = NULL;

        cts_info("Need update firmware when resume");

#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
        if (cts_dev->config_fw_name[0] != '\0') {
            firmware = cts_request_firmware_from_fs(cts_dev, cts_dev->config_fw_name);
        }
#else
        firmware = cts_request_firmware(cts_dev, cts_dev->hwdata->hwid,
            cts_dev->hwdata->fwid, 0);
#endif
        if (firmware) {
            ret = cts_update_firmware(cts_dev, firmware, false);
            cts_release_firmware(firmware);

            if (ret) {
                cts_err("Update default firmware failed %d", ret);
                goto err_set_program_mode;
            }
        } else {
            cts_err("Request default firmware failed %d, "
                "please update manually!!", ret);

            goto err_set_program_mode;
        }
    }
#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r)
            cts_err("Set dev charger attached failed %d", r);
    }
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev earjack attached failed %d", r);
        }
    }
#endif

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev))
        cts_enter_glove_mode(cts_dev);
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev))
        cts_enable_fw_log_redirect(cts_dev);
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
    if (cts_is_proximity_enable(cts_dev)) {
        cts_enable_proximity_mode(cts_dev);
        cts_info("after update firmware, enable proximity mode.");
    }
#endif

    cts_dev->rtdata.suspended = false;
    return 0;

err_set_program_mode:
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.slave_addr = CTS_DEV_PROGRAM_MODE_I2CADDR;
    cts_dev->rtdata.addr_width = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;

    return ret;
}

bool cts_is_device_program_mode(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.program_mode;
}

static inline void cts_init_rtdata_with_normal_mode(struct cts_device *cts_dev)
{
    memset(&cts_dev->rtdata, 0, sizeof(cts_dev->rtdata));

    cts_set_normal_addr(cts_dev);
    cts_dev->rtdata.suspended = false;
    cts_dev->rtdata.updating = false;
    cts_dev->rtdata.testing = false;
    cts_dev->rtdata.fw_log_redirect_enabled = false;
    cts_dev->rtdata.glove_mode_enabled = false;
    cts_dev->rtdata.esd_count = 0;
#ifdef CONFIG_CTS_TP_PROXIMITY
    cts_dev->rtdata.proximity_num = 0;
    cts_dev->rtdata.proximity_status = false;
#endif
}

#ifdef FIX_DEFAULT_ENABLE_DRW
extern int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len, u8 *tx_buffer, u8 *rx_buffer);

static int cts_disable_drw(struct cts_device *cts_dev)
{
    int ret;

    u8 txbuf[] = {
        0x60,
        0x07, 0x80, 0x34,
        0x00, 0x00, 0x04,
        0xdd, 0xe8,
        0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,
        0xdf, 0xfc,
        0x00, 0x00, 0x00
    };
    u8 rxbuf[] = {
        0x60,
        0x07, 0x80, 0x34,
        0x00, 0x00, 0x04,
        0xdd, 0xe8,
        0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00,
        0xdf, 0xfc,
        0x00, 0x00, 0x00
    };

    ret = cts_spi_send_recv(cts_dev->pdata, sizeof(txbuf), txbuf, rxbuf);
    if (ret)
        cts_err("disable drw ret=%d", ret);
    return ret;
}
#endif

int cts_enter_program_mode(struct cts_device *cts_dev)
{
    const static u8 magic_num[] = { 0xCC, 0x33, 0x55, 0x5A };
    u8 boot_mode;
    int ret;

    cts_info("Enter program mode");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Enter program mode while alredy in");
        /* return 0; */
    }

#ifdef CONFIG_CTS_I2C_HOST
    ret = cts_plat_i2c_write(cts_dev->pdata,
            CTS_DEV_PROGRAM_MODE_I2CADDR, magic_num, 4, 5, 10);
    if (ret) {
        cts_err("Write magic number to i2c_dev: 0x%02x failed %d",
            CTS_DEV_PROGRAM_MODE_I2CADDR, ret);
        return ret;
    }

    cts_set_program_addr(cts_dev);
    /* Write i2c deglitch register */
    ret = cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_I2C_CFG, 0x0F, 5, 1);
    if (ret)
        cts_err("Write i2c deglitch register failed\n");
#else
    cts_set_program_addr(cts_dev);
    ret = cts_spi_send_recv(cts_dev->pdata, sizeof(magic_num), (u8 *)magic_num, NULL);
    if (ret) {
        cts_err("Write magic number to i2c_dev: 0x%02x failed %d",
            CTS_DEV_PROGRAM_MODE_SPIADDR, ret);
        return ret;
    }
#endif /* CONFIG_CTS_I2C_HOST */

    cts_dev->rtdata.program_mode = true;
    mdelay(5);

#ifdef FIX_DEFAULT_ENABLE_DRW
    cts_disable_drw(cts_dev);
#endif

    ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
    if (ret) {
        cts_err("Read BOOT_MODE failed %d", ret);
        return ret;
    }
    /* Note: the following CTS_DEV_BOOT_MODE_TCH_PRG_9916
    is used by both ICNL9916 and ICNL9916C */
#ifdef CONFIG_CTS_I2C_HOST
    if ((boot_mode == CTS_DEV_BOOT_MODE_TCH_PRG_9916) ||
    (boot_mode == CTS_DEV_BOOT_MODE_I2C_PRG_9911C))
#else
    if ((boot_mode == CTS_DEV_BOOT_MODE_TCH_PRG_9916) ||
    (boot_mode == CTS_DEV_BOOT_MODE_SPI_PRG_9911C))
#endif
    {
        return 0;
    }
    cts_err("BOOT_MODE readback %u != I2C/SPI PROMGRAM mode", boot_mode);
    return -EFAULT;
}

const char *cts_dev_boot_mode2str(u8 boot_mode)
{
    switch (boot_mode) {
    case CTS_DEV_BOOT_MODE_IDLE:
        return "IDLE-BOOT";
    case CTS_DEV_BOOT_MODE_FLASH:
        return "FLASH-BOOT";
    case CTS_DEV_BOOT_MODE_SRAM:
        return "SRAM-BOOT";
        /* case CTS_DEV_BOOT_MODE_I2C_PRG_9911C: */
    case CTS_DEV_BOOT_MODE_TCH_PRG_9916:
        return "I2C-PRG-BOOT/TCH-PRG-BOOT";
    case CTS_DEV_BOOT_MODE_DDI_PRG:
        return "DDI-PRG-BOOT";
    case CTS_DEV_BOOT_MODE_SPI_PRG_9911C:
        return "SPI-PROG-BOOT/INVALID-BOOT";
    default:
        return "INVALID";
    }
}

int cts_enter_normal_mode(struct cts_device *cts_dev)
{
    int ret = 0;
    u8 boot_mode;
    int retries;
    u16 fwid = CTS_DEV_FWID_INVALID;
    u8 auto_boot = 0;
    u8 first_boot = 1;

    cts_info("Enter normal mode");

    if (!cts_dev->rtdata.program_mode) {
        cts_warn("Enter normal mode while already in");
        return 0;
    }

    if (cts_dev->rtdata.has_flash)
        auto_boot = 1;

#ifdef CFG_CTS_UPDATE_CRCCHECK
    auto_boot = 1;
#endif
    for (retries = 5; retries >= 0; retries--) {
        if (first_boot == 1 || auto_boot == 0) {
            cts_set_program_addr(cts_dev);
            ret = cts_set_dev_boot_mode(cts_dev, CTS_DEV_BOOT_MODE_SRAM);
            if (ret) {
                cts_err("Set BOOT_MODE to SRAM failed %d,"
                    "try to reset device", ret);
            }

            mdelay(30);
        }
        first_boot = 0;

        cts_set_normal_addr(cts_dev);

        ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
        if (ret)
            cts_err("Get BOOT_MODE failed %d", ret);
        if (boot_mode != CTS_DEV_BOOT_MODE_SRAM)
            cts_err("Curr boot mode %u(%s) != SRAM_BOOT",
                boot_mode, cts_dev_boot_mode2str(boot_mode));
        else
            break;

        ret = cts_tcs_get_fw_id(cts_dev, &fwid);
        if (ret)
            cts_err("Get firmware id failed %d, retries %d", ret, retries);
        else {
            cts_info("Get firmware id: 0x%02x", fwid);
            break;
        }
        cts_reset_device(cts_dev);
    }
    if (retries >= 0) {
        ret = cts_init_fwdata(cts_dev);
        if (ret) {
            cts_err("Device init firmware data failed %d", ret);
            return ret;
        }
        return 0;
    }
    cts_set_program_addr(cts_dev);
    return ret;
}

bool cts_is_device_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->enabled;
}

int cts_start_device(struct cts_device *cts_dev)
{
#if defined(CONFIG_CTS_ESD_PROTECTION) || \
    defined(CONFIG_CTS_CHARGER_DETECT) || \
    defined(CONFIG_CTS_EARJACK_DETECT)
    struct chipone_ts_data *cts_data =
    container_of(cts_dev, struct chipone_ts_data, cts_dev);
#endif
    int ret;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_enable_heartbeat_mechanism(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_enable_esd_protection(cts_data);
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_start_charger_detect(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_start_earjack_detect(cts_data);
#endif
#ifdef CONFIG_CTS_TP_WORK_IRQ
	if(cts_irq_status){
		if ((ret = cts_plat_enable_irq(cts_dev->pdata)) < 0) {
			cts_err("Enable IRQ failed %d", ret);
			return ret;
		}
	}else{
		cts_info("Disable Touch irq");
	}
#else
        if ((ret = cts_plat_enable_irq(cts_dev->pdata)) < 0) {
	cts_err("Enable IRQ failed %d", ret);
        return ret;
    }
#endif
    cts_dev->enabled = true;

#ifdef CONFIG_LCT_EARJACK_DETECT
        if (g_touchscreen_earphone_pulgin.valid)
                g_touchscreen_earphone_pulgin.event_callback();
#endif
    cts_info("Start device successfully");

    return 0;
}

int cts_stop_device(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
    int ret;

    cts_info("Stop device...");
    
#ifdef CONFIG_CTS_TP_PROXIMITY
 if (cts_is_proximity_enable(cts_dev)){
        ret = cts_plat_enable_irq_wake(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ wake failed %d", ret);
        }
        cts_info("Proximity is enable in stop");
        return 0;
 }
#endif
    if (!cts_is_device_enabled(cts_dev)) {
        cts_warn("Stop device while halted");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        cts_warn("Stop device while firmware updating, please try again");
        return -EAGAIN;
    }
#ifdef CONFIG_CTS_TP_WORK_IRQ
	if(cts_irq_status){
		if ((ret = cts_plat_disable_irq(cts_dev->pdata)) < 0) {
			cts_err("Disable IRQ failed %d", ret);
			return ret;
		}
	}else{
		cts_info("Disable Touch irq");
	}
#else
	if ((ret = cts_plat_disable_irq(cts_dev->pdata)) < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }
#endif
    cts_dev->enabled = false;

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_disable_heartbeat_mechanism(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_disable_esd_protection(cts_data);
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_stop_charger_detect(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_stop_earjack_detect(cts_data);
#endif

#ifndef CONFIG_GENERIC_HARDIRQS
    if (work_pending(&cts_data->pdata->ts_irq_work)) {
        cts_warn("IRQ work is pending .... flush it");
        flush_work(&cts_data->pdata->ts_irq_work);
    } else
        cts_info("None IRQ work is pending");
#endif
    cts_info("Flush workqueue...");
    if (!cts_dev->rtdata.update_work_run)
        flush_workqueue(cts_data->workqueue);

    ret = cts_plat_release_all_touch(cts_dev->pdata);
    if (ret) {
        cts_err("Release all touch failed %d", ret);
        return ret;
    }
#ifdef CONFIG_CTS_VIRTUALKEY
    ret = cts_plat_release_all_vkey(cts_dev->pdata);
    if (ret) {
        cts_err("Release all vkey failed %d", ret);
        return ret;
    }
#endif

    return 0;
}

#ifdef CONFIG_CTS_ESD_PROTECTION
int cts_start_device_esdrecover(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

    ret = cts_plat_enable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Enable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = true;

    cts_info("Start device successfully");

    return 0;
}

int cts_stop_device_esdrecover(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
    int ret;

    cts_info("Stop device...");

    if (!cts_is_device_enabled(cts_dev)) {
        cts_warn("Stop device while halted");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        cts_warn("Stop device while firmware updating, please try again");
        return -EAGAIN;
    }

    ret = cts_plat_disable_irq(cts_dev->pdata);
    if (ret < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = false;

    flush_workqueue(cts_data->workqueue);

    ret = cts_plat_release_all_touch(cts_dev->pdata);
    if (ret) {
        cts_err("Release all touch failed %d", ret);
        return ret;
    }
#ifdef CONFIG_CTS_VIRTUALKEY
    ret = cts_plat_release_all_vkey(cts_dev->pdata);
    if (ret) {
        cts_err("Release all vkey failed %d", ret);
        return ret;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

    return 0;
}
#endif

bool cts_is_fwid_valid(u16 fwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].fwid == fwid)
            return true;
    }

    return false;
}

static bool cts_is_hwid_valid(u32 hwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].hwid == hwid)
            return true;
    }

    return false;
}

int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid)
{
    int ret;

    cts_info("Get device hardware id");

    if (cts_dev->hwdata) {
        *hwid = cts_dev->hwdata->hwid;
        return 0;
    }

    cts_info("Device hardware data not initialized, try to read from register");

    if (!cts_dev->rtdata.program_mode) {
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            goto err_out;
        }
    }

    ret = cts_hw_reg_readl_retry(cts_dev, CTS_DEV_HW_REG_HARDWARE_ID,
        hwid, 5, 0);
    if (ret)
        goto err_out;

    *hwid = le32_to_cpu(*hwid);
    *hwid &= 0XFFFFFFF0;
    cts_info("Device hardware id: %04x", *hwid);

    if (!cts_is_hwid_valid(*hwid)) {
        cts_warn("Device hardware id %04x invalid", *hwid);
        ret = -EINVAL;
        goto err_out;
    }

    return 0;

err_out:
    *hwid = CTS_DEV_HWID_INVALID;
    return ret;
}

int cts_probe_device(struct cts_device *cts_dev)
{
    int ret, retries = 0;
    u16 fwid = CTS_DEV_FWID_INVALID;
    u32 hwid = CTS_DEV_HWID_INVALID;

    cts_dev->fwdata.version = 0;

    cts_info("Probe device");

    cts_init_rtdata_with_normal_mode(cts_dev);
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        cts_warn("Normal mode spi addr is offline");
    } else {
        ret = cts_tcs_get_fw_id(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d", ret);
        } else {
            cts_info("Firmware id: 0x%04x", fwid);
            ret = cts_tcs_get_fw_ver(cts_dev, &cts_dev->fwdata.version);
            if (ret) {
                cts_err("Read firmware version failed %d", ret);
                cts_dev->fwdata.version = 0;
            } else {
                cts_info("Firmware version: 0x%04x", cts_dev->fwdata.version);
            }
            goto init_hwdata;
        }
    }

read_hwid:
    ret = cts_get_hwid(cts_dev, &hwid);
    if (ret || hwid == CTS_DEV_HWID_INVALID) {
        retries++;

        cts_err("Get hardware id failed %d retries %d", ret, retries);

        if (retries < 3) {
            cts_reset_device(cts_dev);
            goto read_hwid;
        } else
            return -ENODEV;
    }

init_hwdata:
    ret = cts_init_device_hwdata(cts_dev, hwid, fwid);
    if (ret) {
        cts_err("Device hwid: %06x fwid: %04x not found", hwid, fwid);
        return -ENODEV;
    }

    cts_info("Touch info size:%zu", sizeof(struct cts_device_touch_info));

    return ret;
}

#ifdef CFG_CTS_GESTURE
void cts_enable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Enable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = true;
}

void cts_disable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Disable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = false;
}

bool cts_is_gesture_wakeup_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.gesture_wakeup_enabled;
}

int cts_get_gesture_info(struct cts_device *cts_dev, void *gesture_info)
{
    int ret;

    cts_info("Get gesture info");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get gesture info in program mode");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.suspended) {
        cts_warn("Get gesture info while not suspended");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.gesture_wakeup_enabled) {
        cts_warn("Get gesture info while gesture wakeup not enabled");
        return -ENODEV;
    }

    ret = cts_tcs_get_gestureinfo(cts_dev, gesture_info);
    if (ret) {
        cts_err("Read gesture info header failed %d", ret);
        return ret;
    }

    return 0;
}
#endif /* CFG_CTS_GESTURE */

int cts_set_int_data_types(struct cts_device *cts_dev, u16 types)
{
    int ret;
    u16 realtypes = types & INT_DATA_TYPE_MASK;

    cts_info("Set int data types: %#06x, mask to %#06x", types, realtypes);

    ret = cts_tcs_set_int_data_types(cts_dev, realtypes);
    if (ret) {
        cts_err("Set int data type failed: %d", ret);
        return -EIO;
    }
    cts_dev->fwdata.int_data_types = realtypes;

    cts_tcs_calc_int_data_size(cts_dev);

    return ret;
}

int cts_set_int_data_method(struct cts_device *cts_dev, u8 method)
{
    int ret = 0;

    cts_info("Set int data method: %d", method);

    if (method >= INT_DATA_METHOD_CNT) {
        cts_err("Invalid int data method");
        return -EINVAL;
    }

    ret = cts_tcs_set_int_data_method(cts_dev, method);
    if (ret) {
        cts_err("Set int data method failed: %d", ret);
        return -EIO;
    }
    cts_dev->fwdata.int_data_method = method;

    cts_tcs_calc_int_data_size(cts_dev);

    return ret;
}

#ifdef CONFIG_CTS_ESD_PROTECTION
static void cts_esd_protection_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    int ret = 0;

    cts_dbg("ESD protection work");
    cts_data = container_of(work, struct chipone_ts_data, esd_work.work);
    cts_lock_device(&cts_data->cts_dev);
    if (!cts_plat_is_normal_mode(cts_data->pdata)) {
        cts_data->esd_check_fail_cnt++;
        /*reset chip next time */
        if ((cts_data->esd_check_fail_cnt % 2) == 0) {
            cts_err("ESD protection read normal mode failed, reset chip!");
            ret = cts_reset_device(&cts_data->cts_dev);
            if (ret)
                cts_err("ESD protection reset chip failed %d", ret);
        }
    } else {
        cts_data->esd_check_fail_cnt = 0;
    }

    if (cts_data->esd_check_fail_cnt >= CFG_CTS_ESD_FAILED_CONFIRM_CNT) {
        const struct cts_firmware *firmware;

        cts_warn("ESD protection check failed, update firmware!!!");
        cts_stop_device_esdrecover(&cts_data->cts_dev);
        firmware = cts_request_firmware(&cts_data->cts_dev,
            cts_data->cts_dev.hwdata->hwid, cts_data->cts_dev.hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(&cts_data->cts_dev, firmware, false);
            cts_release_firmware(firmware);

            if (ret)
                cts_err("Update default firmware failed %d", ret);
        } else
            cts_err("Request default firmware failed %d, "
                "please update manually!!", ret);

        cts_start_device_esdrecover(&cts_data->cts_dev);
        cts_data->esd_check_fail_cnt = 0;
    }
    queue_delayed_work(cts_data->esd_workqueue, &cts_data->esd_work,
        CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);

    cts_unlock_device(&cts_data->cts_dev);
}

void cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && !cts_data->esd_enabled) {
        cts_info("ESD protection enable");

        cts_data->esd_enabled = true;
        cts_data->esd_check_fail_cnt = 0;
        queue_delayed_work(cts_data->esd_workqueue,
                &cts_data->esd_work,
                CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);
    }
}

void cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && cts_data->esd_enabled) {
        cts_info("ESD protection disable");

        cts_data->esd_enabled = false;
        //cancel_delayed_work(&cts_data->esd_work);
        //flush_workqueue(cts_data->esd_workqueue);
        cancel_delayed_work_sync(&cts_data->esd_work);
    }
}

void cts_init_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("Init ESD protection");

    INIT_DELAYED_WORK(&cts_data->esd_work, cts_esd_protection_work);

    cts_data->esd_enabled = false;
    cts_data->esd_check_fail_cnt = 0;
}

void cts_deinit_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("De-Init ESD protection");

    if (cts_data->esd_workqueue && cts_data->esd_enabled) {
        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
    }
}
#endif /* CONFIG_CTS_ESD_PROTECTION */


#ifdef CFG_CTS_HEARTBEAT_MECHANISM
void cts_show_touch_debug_msg(void *debug)
{
    struct cts_touch_debug_msg *msg = (struct cts_touch_debug_msg *)debug;
    struct cts_firmware_status *status = (struct cts_firmware_status *)
            &msg->firmware_status;

    cts_info("FW_Ver      : 0x%02X", msg->fw_version);
    cts_info("Reset_Flag  : 0x%08X", msg->reset_flag);

    cts_info("Charger     : %d", status->charger);
    cts_info("Proxmty     : %d", status->proximity);
    cts_info("Earjack     : %d", status->earjack);
    cts_info("Knuckle     : %d", status->knuckle);
    cts_info("Glove       : %d", status->glove);
    cts_info("Pocket      : %d", status->pocket);
    cts_info("Game        : %d", status->game);

    cts_info("High_Temp   : %d", status->high_temperature);
    cts_info("Low_Temp    : %d", status->low_temperature);
    cts_info("Bend        : %d", status->bend_mode);
    cts_info("Palm        : %d", status->palm_status);
    cts_info("Noise       : %d", status->noise_mode);
    cts_info("Base_Trace  : %d", status->base_trace);
    cts_info("Water       : %d", status->water_mode);
    cts_info("Ground      : %d", status->ground);

    cts_info("Proxmity_Sts: 0x%02x", msg->proximity);

    cts_info("Work_Mode   : %d", msg->work_mode);
    cts_info("Power_Mode  : %d", msg->power_mode);
    cts_info("Freq        : %d", msg->curr_freq);
    cts_info("Esd_0a      : %d", msg->esd_0a_status);
    cts_info("Fw_Esd      : %d", msg->fw_esd_status);
    cts_info("Landscape   : %d", msg->landscape_mode);

    cts_info("Freq_Noise  : %d", msg->curr_freq_noise);
    cts_info("Max_Diff    : %d", msg->max_diff);
    cts_info("Row         : %d", msg->max_diff_row);
    cts_info("Col         : %d", msg->max_diff_col);
    cts_info("Max_Neg     : -%d", msg->max_neg_diff);
    cts_info("Neg_Row     : %d", msg->max_neg_diff_row);
    cts_info("Neg_Col     : %d", msg->max_neg_diff_col);
}

void cts_heartbeat_mechanism_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_device_touch_info info;
    const struct cts_firmware *firmware;
    u8 curr_status;
    int ret = 0;

    cts_data = container_of(work, struct chipone_ts_data, heart_work.work);
    cts_dev = &cts_data->cts_dev;

    cts_dbg("heartbeat mechanism work");

    cts_lock_device(cts_dev);
    ret = cts_tcs_get_touch_status(cts_dev);
    if (ret) {
        cts_err("get touch status failed");

        cts_reset_device(cts_dev);

        if (++cts_dev->rtdata.esd_count < 3)
            goto end;

        firmware = cts_request_firmware(cts_dev, cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(&cts_data->cts_dev, firmware, false);
            if (ret)
                cts_err("Update default firmware failed %d", ret);
            cts_release_firmware(firmware);
        }
        cts_tcs_reinit_fw_status(cts_dev);
        goto end;
    }

    memcpy(&info, cts_dev->int_data, sizeof(info));

    curr_status = info.debug_msg.firmware_status;

    cts_dev->rtdata.esd_count = 0;

    /* 9922C reset_flag:0xFFFFFF; 9922 9916 9916C reset_flag:0xFFFF */
    if ((info.debug_msg.reset_flag & 0xFFFFFF) != 0xFFFFFF
        || (info.debug_msg.reset_flag & 0xFFFF) != 0xFFFF) {
        cts_err("heartbeat reset flag:0x%08x error", info.debug_msg.reset_flag);
        cts_show_touch_debug_msg(&info.debug_msg);
    }

    if (curr_status != cts_dev->rtdata.firmware_status) {
        cts_err("firmware status error");
        cts_show_touch_debug_msg(&info.debug_msg);
        cts_tcs_reinit_fw_status(cts_dev);
    }

end:
    queue_delayed_work(cts_data->heart_workqueue,
            &cts_data->heart_work, msecs_to_jiffies(2000));
    cts_unlock_device(cts_dev);
}
void cts_enable_heartbeat_mechanism(struct chipone_ts_data *cts_data)
{
    queue_delayed_work(cts_data->heart_workqueue,
            &cts_data->heart_work, msecs_to_jiffies(2000));
}
void cts_disable_heartbeat_mechanism(struct chipone_ts_data *cts_data)
{
    cancel_delayed_work_sync(&cts_data->heart_work);
}
#endif

#ifdef CONFIG_CTS_GLOVE
int cts_enter_glove_mode(const struct cts_device *cts_dev)
{
    int ret;

    cts_info("Enter glove mode");

    ret = cts_tcs_set_glove_mode(cts_dev, 1);
    if (ret)
        cts_err("Enable Glove mode err");
    else
        cts_dev->rtdata.glove_mode_enabled = true;

    return ret;
}

int cts_exit_glove_mode(const struct cts_device *cts_dev)
{
    cts_info("Exit glove mode");

    ret = cts_tcs_set_glove_mode(cts_dev, 0);
    if (ret)
        cts_err("Exit Glove mode err");
    else
        cts_dev->rtdata.glove_mode_enabled = false;

    return ret;
}

int cts_is_glove_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.glove_mode_enabled;
}

#endif /* CONFIG_CTS_GLOVE */

#ifdef CONFIG_CTS_CHARGER_DETECT
bool cts_is_charger_exist(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data;
    bool attached = false;
    int ret;

    cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

    ret = cts_is_charger_attached(cts_data, &attached);
    if (ret)
        cts_err("Get charger state failed %d", ret);

    cts_dev->rtdata.charger_exist = attached;

    return attached;
}

int cts_set_dev_charger_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;
    u8 buf;

    cts_info("Set dev charger %s", attached ? "ATTACHED" : "DETATCHED");
    buf = attached ? 1 : 0;

    ret = cts_tcs_set_charger_plug(cts_dev, buf);
    if (ret)
        cts_err("Set failed %d", ret);

    return ret;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
bool cts_is_earjack_exist(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data;
    bool attached = false;
    int  ret;

    cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

    ret = cts_is_earjack_attached(cts_data, &attached);
    if (ret) {
        cts_err("Get earjack state failed %d", ret);
    }

    return attached;
}

int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;
    u8 buf;

    cts_info("Set dev earjack %s", attached ? "ATTACHED" : "DETATCHED");
    buf = attached ? 1 : 0;
    ret = cts_tcs_set_earjack_plug(cts_dev, buf);
    if (ret)
        cts_err("Set failed %d", ret);

    return ret;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

int cts_enable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Fw log redirect enable");
    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_FW_LOG_REDIRECT);
    if (ret)
        cts_err("Send CTS_CMD_ENABLE_FW_LOG_REDIRECT failed %d", ret);
    else
        cts_dev->rtdata.fw_log_redirect_enabled = true;

    return 0;
}

int cts_disable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Fw log redirect disable");
    ret = cts_send_command(cts_dev, CTS_CMD_DISABLE_FW_LOG_REDIRECT);
    if (ret)
        cts_err("Send CTS_CMD_DISABLE_FW_LOG_REDIRECT failed %d", ret);
    else
        cts_dev->rtdata.fw_log_redirect_enabled = false;

    return 0;
}

bool cts_is_fw_log_redirect(struct cts_device *cts_dev)
{
    return cts_dev->rtdata.fw_log_redirect_enabled;
}

int cts_fw_log_show_finish(struct cts_device *cts_dev)
{
    int ret;

    ret = cts_send_command(cts_dev, CTS_CMD_FW_LOG_SHOW_FINISH);
    if (ret)
        cts_err("Send CTS_CMD_FW_LOG_SHOW_FINISH failed %d", ret);

    return ret;
}

void cts_firmware_upgrade_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    const struct cts_firmware *firmware;
    int retries;
    int ret = 0;

    cts_info("Firmware upgrade work");

    cts_data = container_of(work, struct chipone_ts_data, fw_upgrade_work.work);
    cts_dev = &cts_data->cts_dev;

    cts_lock_device(cts_dev);
    cts_dev->rtdata.update_work_run = true;
    firmware = cts_request_firmware(cts_dev, cts_dev->hwdata->hwid,
            CTS_DEV_FWID_ANY, cts_dev->fwdata.version);
    if (firmware == NULL) {
        cts_warn("Request firmware failed");
        /* With flash, do not update. */
        cts_set_program_addr(cts_dev);
        cts_enter_normal_mode(cts_dev);
        goto end;
    }

    retries = 0;
    do {
        ret = cts_update_firmware(cts_dev, firmware, false);
        if (ret) {
            cts_err("Update firmware failed times: %d", retries);
            cts_reset_device(cts_dev);
        } else
            break;
    } while (++retries < 3);

    cts_release_firmware(firmware);

end:
#ifdef CONFIG_LCT_EARJACK_DETECT
    if (!IS_ERR_OR_NULL(g_touchscreen_earphone_pulgin.event_callback)) {
        g_touchscreen_earphone_pulgin.valid = true;
    }
#endif
    if (ret == 0)
        cts_start_device(cts_dev);

    cts_dev->rtdata.update_work_run = false;
    cts_unlock_device(cts_dev);
}

int cts_reset_device(struct cts_device *cts_dev)
{
    cts_info("Reset device");

    return cts_tcs_reset_device(cts_dev);
}

static struct file *cts_log_filp;
static int cts_log_to_file_level;
extern int cts_write_file(struct file *filp, const void *data, size_t size);

static char *cts_log_buffer;
static int cts_log_buf_size;
static int cts_log_buf_wr_size;

static bool cts_log_redirect;

extern int cts_mkdir_for_file(const char *filepath, umode_t mode);

int cts_start_driver_log_redirect(const char *filepath, bool append_to_file,
    char *log_buffer, int log_buf_size, int log_level)
{
#define START_BANNER \
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    int ret = 0;

    cts_info("Start driver log redirect");

    cts_log_to_file_level = log_level;

    if (log_buffer && log_buf_size) {
        cts_info(" - Start driver log to buffer: %p size: %d level: %d",
            log_buffer, log_buf_size, log_level);
        cts_log_buffer = log_buffer;
        cts_log_buf_size = log_buf_size;
        cts_log_buf_wr_size = 0;
    }

    if (filepath && filepath[0]) {
        cts_info(" - Start driver log to file  : '%s' level: %d",
            filepath, log_level);
#ifdef CFG_CTS_FOR_GKI
        cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
#else
        cts_log_filp = filp_open(filepath,
            O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
            S_IRUGO | S_IWUGO);
        if (IS_ERR(cts_log_filp)) {
            ret = PTR_ERR(cts_log_filp);
            cts_log_filp = NULL;
            cts_err("Open file '%s' for driver log failed %d",
                filepath, ret);
        } else {
            cts_write_file(cts_log_filp, START_BANNER, strlen(START_BANNER));
        }
#endif
    }

    cts_log_redirect = true;

    return ret;
#undef START_BANNER
}

void cts_stop_driver_log_redirect(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
#ifndef CFG_CTS_FOR_GKI
    int ret;
#endif

    cts_log_redirect = false;

    cts_info("Stop driver log redirect");

    if (cts_log_filp) {
        cts_info(" - Stop driver log to file");

        cts_write_file(cts_log_filp, END_BANNER, strlen(END_BANNER));
#ifndef CFG_CTS_FOR_GKI
        ret = filp_close(cts_log_filp, NULL);
        if (ret) {
            cts_err("Close driver log file failed %d", ret);
        }
#endif
        cts_log_filp = NULL;
    }

    if (cts_log_buffer) {
        cts_info(" - Stop driver log to buffer");

        cts_log_buffer = NULL;
        cts_log_buf_size = 0;
        cts_log_buf_wr_size = 0;
    }

#undef END_BANNER
}

int cts_get_driver_log_redirect_size(void)
{
    if (cts_log_redirect && cts_log_buffer && cts_log_buf_wr_size) {
        return cts_log_buf_wr_size;
    } else {
        return 0;
    }
}

void cts_log(int level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    if (cts_log_redirect) {
        if (cts_log_buffer &&
            cts_log_buf_wr_size < cts_log_buf_size &&
            level <= cts_log_to_file_level) {
            cts_log_buf_wr_size += vscnprintf(cts_log_buffer + cts_log_buf_wr_size,
                cts_log_buf_size - cts_log_buf_wr_size, fmt, args);
        }

        if (cts_log_filp != NULL && level <= cts_log_to_file_level) {
            char buf[512];
            int count = vscnprintf(buf, sizeof(buf), fmt, args);

            cts_write_file(cts_log_filp, buf, count);
        }
    }

    if (level < CTS_DRIVER_LOG_DEBUG || cts_show_debug_log) {
        vprintk(fmt, args);
    }

    va_end(args);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/* From lib/kstrtox.c */
/**
 * kstrtobool - convert common user inputs into boolean values
 * @s: input string
 * @res: result
 *
 * This routine returns 0 iff the first character is one of 'Yy1Nn0', or
 * [oO][NnFf] for "on" and "off". Otherwise it will return -EINVAL.  Value
 * pointed to by res is updated upon finding a match.
 */
int kstrtobool(const char *s, bool *res)
{
    if (!s)
        return -EINVAL;

    switch (s[0]) {
    case 'y':
    case 'Y':
    case '1':
        *res = true;
        return 0;
    case 'n':
    case 'N':
    case '0':
        *res = false;
        return 0;
    case 'o':
    case 'O':
        switch (s[1]) {
        case 'n':
        case 'N':
            *res = true;
            return 0;
        case 'f':
        case 'F':
            *res = false;
            return 0;
        default:
            break;
        }
    default:
        break;
    }

    return -EINVAL;
}
#endif
