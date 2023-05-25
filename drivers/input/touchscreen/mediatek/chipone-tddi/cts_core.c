#define LOG_TAG         "Core"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_firmware.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"

#ifdef CONFIG_CTS_I2C_HOST
static int cts_i2c_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%02x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, b);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writeb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 1, retry ,delay);
}

static int cts_i2c_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%04x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, w);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writew invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_i2c_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%08x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, l);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writel invalid address width %u",
            cts_dev->rtdata.addr_width);
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
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    data = cts_plat_get_i2c_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, data);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, data);
        } else {
            cts_err("Writesb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                data, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c write failed %d", ret);
            return ret;
        }

        src  += payload_len;
        len  -= payload_len;
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

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_i2c_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[2];

    cts_dbg("Readw from slave_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readw invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0) {
        *w = get_unaligned_le16(buff);
    }

    return ret;
}

static int cts_i2c_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[4];

    cts_dbg("Readl from slave_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readl invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0) {
        *l = get_unaligned_le32(buff);
    }

    return ret;
}

static int cts_i2c_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

    cts_dbg("Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }

    return 0;
}
#else
static int cts_spi_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8  buff[8];

//    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%02x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, b);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writeb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr, buff,
        cts_dev->rtdata.addr_width + 1, retry ,delay);
}

static int cts_spi_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8  buff[8];

//    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%04x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, w);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writew invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_spi_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8  buff[8];

//    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%08x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, l);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writel invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
}

static int cts_spi_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
#if 1
    int ret;
    u8 *data;
    size_t max_xfer_size;
    size_t payload_len;
    size_t xfer_len;

//    cts_dbg("Write to slave_addr: 0x%02x reg: 0x%0*x len: %zu",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    data = cts_plat_get_spi_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, data);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, data);
        } else {
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

        src  += payload_len;
        len  -= payload_len;
        addr += payload_len;
    }
#endif
    return 0;
}

static int cts_spi_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

//    cts_dbg("Readb from slave_addr: 0x%02x reg: 0x%0*x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_spi_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[2];

//    cts_dbg("Readw from slave_addr: 0x%02x reg: 0x%0*x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readw invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0) {
        *w = get_unaligned_le16(buff);
    }

    return ret;
}

static int cts_spi_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[4];

//    cts_dbg("Readl from slave_addr: 0x%02x reg: 0x%0*x",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readl invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0) {
        *l = get_unaligned_le32(buff);
    }

    return ret;
}

static int cts_spi_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

//    cts_dbg("Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
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

//    cts_dbg("Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read_delay_idle(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay, idle);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }
    return 0;
}

#endif /* CONFIG_CTS_I2C_HOST */

static inline int cts_dev_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writeb(cts_dev, addr, b, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writeb(cts_dev, addr, b, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writew(cts_dev, addr, w, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writew(cts_dev, addr, w, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writel(cts_dev, addr, l, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writel(cts_dev, addr, l, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writesb(cts_dev, addr, src, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writesb(cts_dev, addr, src, len, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readb(cts_dev, addr, b, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readb(cts_dev, addr, b, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readw(cts_dev, addr, w, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readw(cts_dev, addr, w, retry, delay);;
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readl(cts_dev, addr, l, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readl(cts_dev, addr, l, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readsb(cts_dev, addr, dst, len, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay, int idle)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readsb_delay_idle(cts_dev, addr, dst, len, retry, delay, idle);
#endif /* CONFIG_CTS_I2C_HOST */
}


#ifdef CFG_CTS_UPDATE_CRCCHECK
int cts_sram_writesb_boot_crc_retry(const struct cts_device *cts_dev,
        size_t len, u32 crc, int retry)
{
    int ret = 0, retries;

    retries = 0;
    do {
        if ((ret = cts_dev_writel(cts_dev, 0x015ff0, 0xCC33555A, 3, 1)) != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

       if ((ret = cts_dev_writel(cts_dev, 0x08fffc, crc, 3, 1)) != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        if ((ret = cts_dev_writel(cts_dev, 0x08fff8, len, 3, 1)) != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        break;
    }while (retries++ < retry);

    return ret;
}
#endif

static int cts_write_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    int i, ret;
    u8    buff[5];

    for (i = 0; i < len; i++) {
        put_unaligned_le32(addr, buff);
        buff[4] = *(u8 *)src;

        addr++;
        src++;

        ret = cts_dev_writesb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, buff, 5, retry, delay);
        if (ret) {
            cts_err("Write rDEBUG_INTF len=5B failed %d",
                    ret);
            return ret;
        }
    }

    return 0;
}

int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writeb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, &b, 1, retry, delay);
    }
}

int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writew(cts_dev, addr, w, retry, delay);
    } else {
        put_unaligned_le16(w, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
    }
}

int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[4];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writel(cts_dev, addr, l, retry, delay);
    } else {
        put_unaligned_le32(l, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
    }
}

int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writesb(cts_dev, addr, src, len, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, src, len, retry, delay);
    }
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

    retries = 0;
    do {
        u32 crc_sram;

        retries++;

        if ((ret = cts_sram_writesb(cts_dev, 0, src, len)) != 0) {
            cts_err("SRAM writesb failed %d", ret);
            continue;
        }

        if ((ret = cts_calc_sram_crc(cts_dev, 0, len, &crc_sram)) != 0) {
            cts_err("Get CRC for sram writesb failed %d retries %d",
                ret, retries);
            continue;
        }

        if (crc == crc_sram) {
            return 0;
        }

        cts_err("Check CRC for sram writesb mismatch %x != %x retries %d",
                crc, crc_sram, retries);
        ret = -EFAULT;
    }while (retries < retry);

    return ret;
}

static int cts_read_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        ret = cts_dev_writel(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, addr, retry, delay);
        if (ret) {
            cts_err("Write addr to rDEBUG_INTF failed %d", ret);
            return ret;
        }

        ret = cts_dev_readb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF + 4, (u8 *)dst, retry, delay);
        if (ret) {
            cts_err("Read value from rDEBUG_INTF + 4 failed %d",
                ret);
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
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, b, 1, retry, delay);
    }
}

int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readw(cts_dev, addr, w, retry, delay);
    } else {
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

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readl(cts_dev, addr, l, retry, delay);
    } else {
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
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readsb(cts_dev, addr, dst, len, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, dst, len, retry, delay);
    }
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
        cts_err("Writesb to fw reg 0x%04x under program mode", reg_addr);
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

    return cts_dev_readsb_delay_idle(cts_dev, reg_addr, dst, len, retry, delay, idle);
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

const static struct cts_sfctrl icnl9911_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (80 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9911s_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (64 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9911c_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (64 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

#define CTS_DEV_HW_REG_DDI_REG_CTRL     (0x3002Cu)

static int icnl9911_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8  access_flag;

    cts_info("ICNL9911 %s access ddi reg", enable ? "enable" : "disable");

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

    ret = cts_hw_reg_writew(cts_dev, 0x3DFF0, enable ? 0x4BB4 : 0xB44B);
    if (ret) {
        cts_err("Write password failed %d");
        goto disable_access_ddi_reg;
    }

    return 0;

disable_access_ddi_reg: {
        int r;

        access_flag = enable ? (access_flag & (~0x01)) : (access_flag | 0x01);
        r = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
        if (r) {
            cts_err("");
        }
    }

    return ret;
}

static int icnl9911s_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8  access_flag;

    cts_info("ICNL9911S %s access ddi reg", enable ? "enable" : "disable");

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

    ret = cts_hw_reg_writeb(cts_dev, 0x30074, enable ? 1 : 0);
    if (ret) {
        cts_err("Write 0x30074 failed %d", access_flag, ret);
        return ret;
    }

    ret = cts_hw_reg_writew(cts_dev, 0x3DFF0, enable ? 0x595A : 0x5A5A);
    if (ret) {
        cts_err("Write password to F0 failed %d");
        return ret;
    }
    ret = cts_hw_reg_writew(cts_dev, 0x3DFF4, enable ? 0xA6A5 : 0x5A5A);
    if (ret) {
        cts_err("Write password to F1 failed %d");
        return ret;
    }

    return 0;
}

const static struct cts_device_hwdata cts_device_hwdatas[] = {
    {
        .name = "ICNL9911",
        .hwid = CTS_DEV_HWID_ICNL9911,
        .fwid = CTS_DEV_FWID_ICNL9911,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 80 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911_sfctrl,
        .enable_access_ddi_reg = icnl9911_set_access_ddi_reg,
    },
    {
        .name = "ICNL9911S",
        .hwid = CTS_DEV_HWID_ICNL9911S,
        .fwid = CTS_DEV_FWID_ICNL9911S,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 64 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911s_sfctrl,
        .enable_access_ddi_reg = icnl9911s_set_access_ddi_reg,
    },
    {
        .name = "ICNL9911C",
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 64 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911c_sfctrl,
        .enable_access_ddi_reg = icnl9911s_set_access_ddi_reg,
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

    rt_mutex_lock(&cts_dev->pdata->dev_lock);
}

void cts_unlock_device(const struct cts_device *cts_dev)
{
    cts_dbg("### Un-Lock ###");

    rt_mutex_unlock(&cts_dev->pdata->dev_lock);
}

int cts_set_work_mode(const struct cts_device *cts_dev, u8 mode)
{
    cts_info("Set work mode to %u", mode);

    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_work_mode(const struct cts_device *cts_dev, u8 *mode)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_firmware_version(const struct cts_device *cts_dev, u16 *version)
{
    int ret = cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_VERSION, version);

    if (ret) {
        *version = 0;
    } else {
        *version = be16_to_cpup(version);
    }

    return ret;
}

int cts_get_ddi_version(const struct cts_device *cts_dev, u8 *version)
{
    int ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_DDI_VERSION, version);

    if (ret) {
        *version = 0;
    }
    return ret;
}

int cts_get_lib_version(const struct cts_device *cts_dev, u16 *lib_version)
{
    u8  main_version, sub_version;
    int ret;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FW_LIB_MAIN_VERSION, &main_version);
    if (ret) {
        cts_err("Get fw lib main version failed %d", ret);
        return ret;
    }

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FW_LIB_SUB_VERSION, &sub_version);
    if (ret) {
        cts_err("Get fw lib sub version failed %d", ret);
        return ret;
    }

    *lib_version = (main_version << 8) + sub_version;
    return 0;
}


int cts_get_data_ready_flag(const struct cts_device *cts_dev, u8 *flag)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, flag);
}

int cts_clr_data_ready_flag(const struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, 0);
}

int cts_send_command(const struct cts_device *cts_dev, u8 cmd)
{
    if (cts_dev->rtdata.program_mode) {
        cts_warn("Send command %u while chip in program mode", cmd);
        return -ENODEV;
    }

    return cts_fw_reg_writeb_retry(cts_dev, CTS_DEVICE_FW_REG_CMD, cmd, 3, 0);
}

static int cts_get_touchinfo(const struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info)
{
    cts_dbg("Get touch info");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get touch info in program mode");
        return -ENODEV;
    }

    if (cts_dev->rtdata.suspended) {
        cts_warn("Get touch info while is suspended");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO,
            touch_info, sizeof(*touch_info));
}

int cts_get_panel_param(const struct cts_device *cts_dev,
        void *param, size_t size)
{
    cts_info("Get panel parameter");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get panel parameter in program mode");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_set_panel_param(const struct cts_device *cts_dev,
        const void *param, size_t size)
{
    cts_info("Set panel parameter");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Set panel parameter in program mode");
        return -ENODEV;
    }
    return cts_fw_reg_writesb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_get_x_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_X_RESOLUTION, resolution);
}

int cts_get_y_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_Y_RESOLUTION, resolution);
}

int cts_get_num_rows(const struct cts_device *cts_dev, u8 *num_rows)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_TX, num_rows);
}

int cts_get_num_cols(const struct cts_device *cts_dev, u8 *num_cols)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_RX, num_cols);
}

#define CTS_DEV_FW_ESD_PROTECTION_ON    (3)
#define CTS_DEV_FW_ESD_PROTECTION_OFF   (1)

int cts_get_dev_esd_protection(struct cts_device *cts_dev, bool *enable)
{
    int ret;
    u8  val;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_ESD_PROTECTION, &val);
    if (ret == 0) {
        *enable = (val == CTS_DEV_FW_ESD_PROTECTION_ON);
    }

    return ret;
}

int cts_set_dev_esd_protection(struct cts_device *cts_dev, bool enable)
{
    cts_info("%s ESD protection",  enable ? "Enable" : "Disable");

    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_ESD_PROTECTION,
        enable ? CTS_DEV_FW_ESD_PROTECTION_ON : CTS_DEV_FW_ESD_PROTECTION_OFF);
}

#ifdef CONFIG_CTS_LEGACY_TOOL
int cts_enable_get_rawdata(const struct cts_device *cts_dev)
{
    cts_info("Enable get raw/diff data");
    return cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_RAWDATA);
}

int cts_disable_get_rawdata(const struct cts_device *cts_dev)
{
    cts_info("Disable get raw/diff data");
    return cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_RAWDATA);
}

static void tsdata_flip_x(void *tsdata, u8 fw_rows, u8 fw_cols)
{
    u8 r, c;
    u16 *data;

    data = (u16 *)tsdata;
    for (r = 0; r < fw_rows; r++) {
        for (c = 0; c < fw_cols / 2; c++) {
            swap(data[r * fw_cols + c],
                 data[r * fw_cols + wrap(fw_cols, c)]);
        }
    }
}

static void tsdata_flip_y(void *tsdata, u8 fw_rows, u8 fw_cols)
{
    u8 r, c;
    u16 *data;

    data = (u16 *)tsdata;
    for (r = 0; r < fw_rows / 2; r++) {
        for (c = 0; c < fw_cols; c++) {
            swap(data[r * fw_cols + c],
                 data[wrap(fw_rows, r) * fw_cols + c]);
        }
    }
}

int cts_get_rawdata(const struct cts_device *cts_dev, void *buf)
{
    int i, ret;
    u8 ready;
    u8 retries = 5;

    cts_info("Get rawdata");
    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            cts_err("Get data ready flag failed %d", ret);
            goto get_raw_exit;
        }
        if (ready) {
            break;
        }
    }
    if (i == 1000) {
        ret = -ENODEV;
        goto get_raw_exit;
    }
    do {
        ret = cts_fw_reg_readsb_delay_idle(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA,
            buf,cts_dev->fwdata.rows*cts_dev->fwdata.cols*2, 500);
        if (ret) {
            cts_err("Read rawdata failed %d", ret);
        }
    } while (--retries > 0 && ret != 0);

    if (cts_dev->fwdata.flip_x) {
        tsdata_flip_x(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }
    if (cts_dev->fwdata.flip_y) {
        tsdata_flip_y(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }

    if (cts_clr_data_ready_flag(cts_dev)) {
        cts_err("Clear data ready flag failed");
        ret = -ENODEV;
    }
get_raw_exit:
    return ret;
}

int cts_get_diffdata(const struct cts_device *cts_dev, void *buf)
{
    int i, j, ret;
    u8 ready;
    u8 retries = 5;
    u8 *cache_buf;

    cts_info("Get diffdata");
    cache_buf = kzalloc(
      (cts_dev->fwdata.rows + 2) * (cts_dev->fwdata.cols + 2) * 2, GFP_KERNEL);
    if (cache_buf == NULL) {
        cts_err("Get diffdata: malloc error");
        ret = -ENOMEM;
        goto get_diff_exit;
    }
    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            cts_err("Get data ready flag failed %d", ret);
            goto get_diff_free_buf;
        }
        if (ready) {
            break;
        }
    }
    if (i == 1000) {
        ret = -ENODEV;
        goto get_diff_free_buf;
    }
    do {
        ret = cts_fw_reg_readsb_delay_idle(cts_dev, CTS_DEVICE_FW_REG_DIFF_DATA,
            cache_buf,(cts_dev->fwdata.rows+2)*(cts_dev->fwdata.cols+2)*2, 500);
        if (ret) {
            cts_err("Read diffdata failed %d", ret);
        }
    } while (--retries > 0 && ret != 0);

    for (i = 0; i < cts_dev->fwdata.rows; i++) {
        for (j = 0; j < cts_dev->fwdata.cols; j++) {
            ((u8 *)buf)[2 * (i * cts_dev->fwdata.cols + j)]  =
                cache_buf[2 * ((i+1)*(cts_dev->fwdata.cols+2)+j+1)];
            ((u8 *)buf)[2*(i*cts_dev->fwdata.cols + j)+1] =
                cache_buf[2*((i+1)*(cts_dev->fwdata.cols+2)+j+1)+1];
        }
    }

    if (cts_dev->fwdata.flip_x) {
        tsdata_flip_x(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }
    if (cts_dev->fwdata.flip_y) {
        tsdata_flip_y(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }

    if (cts_clr_data_ready_flag(cts_dev)) {
        cts_err("Clear data ready flag failed");
        ret = -ENODEV;
    }
get_diff_free_buf:
    kfree(cache_buf);
get_diff_exit:
    return ret;
}
#endif /* CONFIG_CTS_LEGACY_TOOL */

static int cts_get_dev_boot_mode(const struct cts_device *cts_dev,
        u8 *boot_mode)
{
    int ret;

    ret = cts_hw_reg_readb_retry(cts_dev,
        CTS_DEV_HW_REG_CURRENT_MODE, boot_mode, 5, 10);
    if (ret) {
        cts_err("Read boot mode failed %d", ret);
        return ret;
    }

    *boot_mode &= CTS_DEV_BOOT_MODE_MASK;

    cts_info("Curr dev boot mode: %u(%s)", *boot_mode,
        cts_dev_boot_mode2str(*boot_mode));
    return 0;
}

static int cts_set_dev_boot_mode(const struct cts_device *cts_dev,
        u8 boot_mode)
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
    u8  val;
    int ret;

    cts_info("Init firmware data");

    if (cts_dev->rtdata.program_mode) {
        cts_err("Init firmware data while in program mode");
        return -EINVAL;
    }

    ret = cts_get_firmware_version(cts_dev, &fwdata->version);
    if (ret) {
        cts_err("Read firmware version failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %04x", "Firmware version", fwdata->version);

    ret = cts_get_lib_version(cts_dev, &fwdata->lib_version);
    if (ret) {
        cts_err("Read firmware Lib version failed %d", ret);
    }
    cts_info("  %-24s: v%x.%x", "Fimrware lib verion",
        (u8)(fwdata->lib_version >> 8),
        (u8)(fwdata->lib_version));

    ret = cts_get_ddi_version(cts_dev, &fwdata->ddi_version);
    if (ret) {
        cts_err("Read ddi version failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %02x", "DDI init code verion", fwdata->ddi_version);

    ret = cts_get_x_resolution(cts_dev, &fwdata->res_x);
    if (ret) {
        cts_err("Read firmware X resoltion failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %u", "X resolution", fwdata->res_x);

    ret = cts_get_y_resolution(cts_dev, &fwdata->res_y);
    if (ret) {
        cts_err("Read firmware Y resolution failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %u", "Y resolution", fwdata->res_y);

    ret = cts_get_num_rows(cts_dev, &fwdata->rows);
    if (ret) {
        cts_err("Read firmware num TX failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %u", "Num rows", fwdata->rows);

    ret = cts_get_num_cols(cts_dev, &fwdata->cols);
    if (ret) {
        cts_err("Read firmware num RX failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %u", "Num cols", fwdata->cols);

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &val);
    if (ret) {
        cts_err("Read FW_REG_FLIP_X/Y failed %d", ret);
        return ret;
    }
    cts_dev->fwdata.flip_x = !!(val & BIT(2));
    cts_dev->fwdata.flip_y = !!(val & BIT(3));
    cts_info("  %-24s: %s", "Flip X",
        cts_dev->fwdata.flip_x ? "True" : "Flase");
    cts_info("  %-24s: %s", "Flip Y",
        cts_dev->fwdata.flip_y ? "True" : "Flase");

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_SWAP_AXES, &val);
    if (ret) {
        cts_err("Read FW_REG_SWAP_AXES failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %s", "Swap axes",
        cts_dev->fwdata.swap_axes ? "True" : "Flase");

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_INT_MODE, &fwdata->int_mode);
    if (ret) {
        cts_err("Read firmware Int mode failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %s", "Int polarity",
        (fwdata->int_mode == 0) ? "LOW" : "HIGH");

    ret = cts_fw_reg_readw(cts_dev,
        CTS_DEVICE_FW_REG_INT_KEEP_TIME, &fwdata->int_keep_time);
    if (ret) {
        cts_err("Read firmware Int keep time failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %d", "Int keep time", fwdata->int_keep_time);

    ret = cts_fw_reg_readw(cts_dev,
        CTS_DEVICE_FW_REG_RAWDATA_TARGET, &fwdata->rawdata_target);
    if (ret) {
        cts_err("Read firmware Raw dest value failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %d", "Raw target value", fwdata->rawdata_target);


    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_ESD_PROTECTION, &fwdata->esd_method);
    if (ret) {
        cts_err("Read firmware Esd method failed %d", ret);
        return ret;
    }
    cts_info("  %-24s: %d", "Esd method", fwdata->esd_method);

#ifdef CONFIG_CTS_EARJACK_DETECT
    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_EARJACK_DETECT_SUPP, &val);
    if (ret) {
        cts_err("Read firmware earjack detect support failed %d", ret);
        return ret;
    }
    fwdata->supp_headphone_cable_reject = !!(val & BIT(0));
    cts_info("  %-24s: %s", "Headphone cable reject",
        fwdata->supp_headphone_cable_reject ? "True" : "False");
#endif /* CONFIG_CTS_EARJACK_DETECT */

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
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO+1, &len, 1);
    if (ret) {
        cts_err("Get i2c print buf len error");
        return;
    }
    if (len >= max_len) {
        len = max_len - 1;
    }
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO+2, data, len);
    if (ret) {
        cts_err("Get i2c print buf error");
        return;
    }
    data[len] = '\0';
    printk("CTS-FW_LOG %s", data);
    cts_fw_log_show_finish(cts_dev);
}
#endif

int cts_irq_handler(struct cts_device *cts_dev)
{
    int ret;

    cts_dbg("Enter IRQ handler");

    if (cts_dev->rtdata.program_mode) {
        cts_err("IRQ triggered in program mode");
        return -EINVAL;
    }

    if (unlikely(cts_dev->rtdata.suspended)) {
#ifdef CFG_CTS_GESTURE
        if (cts_dev->rtdata.gesture_wakeup_enabled) {
            struct cts_device_gesture_info gesture_info;

            cts_info("Get gesture info");
            ret = cts_get_gesture_info(cts_dev,
                    &gesture_info, CFG_CTS_GESTURE_REPORT_TRACE);
            if (ret) {
                cts_warn("Get gesture info failed %d", ret);
                //return ret;
            }

            /** - Issure another suspend with gesture wakeup command to device
             * after get gesture info.
             */
            cts_info("Set device enter gesture mode");
            cts_send_command(cts_dev, CTS_CMD_SUSPEND_WITH_GESTURE);

            ret = cts_plat_process_gesture_info(cts_dev->pdata, &gesture_info);
            if (ret) {
                cts_err("Process gesture info failed %d", ret);
                return ret;
            }
        } else {
            cts_warn("IRQ triggered while device suspended "
                    "without gesture wakeup enable");
        }
#endif /* CFG_CTS_GESTURE */
    } else {
        struct cts_device_touch_info *touch_info;

        touch_info = &cts_dev->pdata->touch_info;
#ifdef CFG_CTS_FW_LOG_REDIRECT
        ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO, touch_info, 1);
        if (ret) {
            cts_err("Get vkey_state failed %d", ret);
            return ret;
        }

        if (touch_info->vkey_state == CTS_FW_LOG_REDIRECT_SIGN) {
            if (cts_is_fw_log_redirect(cts_dev)) {
                cts_show_fw_log(cts_dev);
            }
            return 0;
        }
#endif /* CFG_CTS_FW_LOG_REDIRECT */
        ret = cts_get_touchinfo(cts_dev, touch_info);
        if (ret) {
            cts_err("Get touch info failed %d", ret);
            return ret;
        }

        cts_dbg("Touch info: vkey_state %x, num_msg %u",
            touch_info->vkey_state, touch_info->num_msg);

        ret = cts_plat_process_touch_msg(cts_dev->pdata,
            touch_info->msgs, touch_info->num_msg);
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
#endif /* CONFIG_CTS_VIRTUALKEY */
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

    cts_info("Suspend device");

    if (cts_dev->rtdata.suspended) {
        cts_warn("Suspend device while already suspended");
        return 0;
    }
    if (cts_dev->rtdata.program_mode) {
        cts_info("Quit programming mode before suspend");
        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
              cts_err("Failed to exit program mode before suspend:%d", ret);
              return ret;
        }
    }
    ret = cts_send_command(cts_dev,
        cts_dev->rtdata.gesture_wakeup_enabled ?
            CTS_CMD_SUSPEND_WITH_GESTURE : CTS_CMD_SUSPEND);

    if (ret){
        cts_err("Suspend device failed %d", ret);

        return ret;
    }

    cts_info("Device suspended ...");
    cts_dev->rtdata.suspended = true;

    return 0;
}

int cts_resume_device(struct cts_device *cts_dev)
{
    int ret = 0;
    int retries = 3;

    cts_info("Resume device");

    /* Check whether device is in normal mode */
    while (--retries >= 0) {
#ifdef CFG_CTS_HAS_RESET_PIN
        cts_plat_reset_device(cts_dev->pdata);
#endif /* CFG_CTS_HAS_RESET_PIN */
        cts_set_normal_addr(cts_dev);
#ifdef CONFIG_CTS_I2C_HOST
        if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR))
#else
        if (cts_plat_is_normal_mode(cts_dev->pdata))
#endif
        {
            break;
        }
    }

    if (retries < 0) {
        const struct cts_firmware *firmware;

        cts_info("Need update firmware when resume");
        firmware = cts_request_firmware(cts_dev,cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(cts_dev, firmware, true);
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
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_dev->fwdata.supp_headphone_cable_reject &&
        cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev earjack attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_dev->rtdata.suspended = false;
    return 0;

err_set_program_mode:
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.slave_addr   = CTS_DEV_PROGRAM_MODE_I2CADDR;
    cts_dev->rtdata.addr_width   = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;

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
    cts_dev->rtdata.suspended       = false;
    cts_dev->rtdata.updating        = false;
    cts_dev->rtdata.testing         = false;
    cts_dev->rtdata.fw_log_redirect_enabled  = false;
    cts_dev->rtdata.glove_mode_enabled = false;
}

int cts_enter_program_mode(struct cts_device *cts_dev)
{
    const static u8 magic_num[] = {0xCC, 0x33, 0x55, 0x5A};
    u8  boot_mode;
    int ret;

    cts_info("Enter program mode");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Enter program mode while alredy in");
        //return 0;
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
    ret = cts_hw_reg_writeb_retry(cts_dev, 0x37001, 0x0F, 5, 1);
    if (ret) {
        cts_err("Write i2c deglitch register failed\n");
        // FALL through
    }
#else
    cts_set_program_addr(cts_dev);
    cts_plat_reset_device(cts_dev->pdata);
    ret = cts_plat_spi_write(cts_dev->pdata,
            0xcc, &magic_num[1], 3, 5, 10);
    if (ret) {
        cts_err("Write magic number to i2c_dev: 0x%02x failed %d",
            CTS_DEV_PROGRAM_MODE_SPIADDR, ret);
        return ret;
    }
#endif /* CONFIG_CTS_I2C_HOST */
    ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
    if (ret) {
        cts_err("Read BOOT_MODE failed %d", ret);
        return ret;
    }

#ifdef CONFIG_CTS_I2C_HOST
    if (boot_mode != CTS_DEV_BOOT_MODE_I2C_PROGRAM)
#else
    if (boot_mode != CTS_DEV_BOOT_MODE_SPI_PROGRAM)
#endif
    {
        cts_err("BOOT_MODE readback %u != I2C/SPI PROMGRAM mode", boot_mode);
        return -EFAULT;
    }

    return 0;
}

const char *cts_dev_boot_mode2str(u8 boot_mode)
{
#define case_boot_mode(mode) \
    case CTS_DEV_BOOT_MODE_ ## mode: return #mode "-BOOT"

    switch (boot_mode) {
        case_boot_mode(FLASH);
        case_boot_mode(I2C_PROGRAM);
        case_boot_mode(SRAM);
        case_boot_mode(SPI_PROGRAM);
        default: return "INVALID";
    }

#undef case_boot_mode
}

int cts_enter_normal_mode(struct cts_device *cts_dev)
{
    int ret = 0;
    u8  boot_mode;
    int retries;
    u16  fwid = CTS_DEV_FWID_INVALID;
    u8  auto_boot = 0;
    u8  first_boot = 1;

    cts_info("Enter normal mode");

    if (!cts_dev->rtdata.program_mode) {
        cts_warn("Enter normal mode while already in");
        return 0;
    }

    if (cts_dev->rtdata.has_flash) {
        auto_boot = 1;
    }
#ifdef CFG_CTS_UPDATE_CRCCHECK
    if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911S ||
        cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911C) {
        auto_boot = 1;
    }
#endif
    for(retries = 5; retries >= 0; retries--) {
        if (first_boot == 1 || auto_boot == 0) {
            cts_set_program_addr(cts_dev);
            ret = cts_set_dev_boot_mode(cts_dev, CTS_DEV_BOOT_MODE_SRAM);
            if (ret) {
                cts_err("Set BOOT_MODE to SRAM failed %d, try to reset device", ret);
            }
            mdelay(30);
        }
        first_boot = 0;
#ifdef CONFIG_CTS_I2C_HOST
        if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR)) {
            cts_set_normal_addr(cts_dev);
        }
#else
        cts_set_normal_addr(cts_dev);
#endif
        ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
        if (ret) {
            cts_err("Get BOOT_MODE failed %d", ret);
        }
        if (boot_mode != CTS_DEV_BOOT_MODE_SRAM) {
            cts_err("Curr boot mode %u(%s) != SRAM_BOOT",
                boot_mode, cts_dev_boot_mode2str(boot_mode));
        }
        else {
            break;
        }
        ret = cts_get_fwid(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d, retries %d", ret, retries);
        } else {
            if (fwid == CTS_DEV_FWID_ICNL9911 || fwid == CTS_DEV_FWID_ICNL9911S)
            {
                cts_info("Get firmware id successful 0x%02x", fwid);
                break;
            }
        }
        cts_plat_reset_device(cts_dev->pdata);
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
#if defined(CONFIG_CTS_ESD_PROTECTION) || defined(CONFIG_CTS_CHARGER_DETECT) || defined(CONFIG_CTS_EARJACK_DETECT)
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
#endif
    int ret;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_enable_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_start_charger_detect(cts_data);
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_dev->fwdata.supp_headphone_cable_reject) {
        cts_start_earjack_detect(cts_data);
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */

    if ((ret = cts_plat_enable_irq(cts_dev->pdata)) < 0) {
        cts_err("Enable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = true;

    cts_info("Start device successfully");

    return 0;
}

int cts_stop_device(struct cts_device *cts_dev)
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

    if ((ret = cts_plat_disable_irq(cts_dev->pdata)) < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = false;

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_disable_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_stop_charger_detect(cts_data);
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_dev->fwdata.supp_headphone_cable_reject) {
        cts_stop_earjack_detect(cts_data);
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */

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

#ifdef CONFIG_CTS_ESD_PROTECTION
int cts_start_device_esdrecover(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

    if ((ret = cts_plat_enable_irq(cts_dev->pdata)) < 0) {
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

    if ((ret = cts_plat_disable_irq(cts_dev->pdata)) < 0) {
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
        if (cts_device_hwdatas[i].fwid == fwid) {
            return true;
        }
    }

    return false;
}

static bool cts_is_hwid_valid(u32 hwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].hwid == hwid) {
            return true;
        }
    }

    return false;
}

int cts_get_fwid(struct cts_device *cts_dev, u16 *fwid)
{
    int ret;

    cts_info("Get device firmware id");

    if (cts_dev->hwdata) {
        *fwid = cts_dev->hwdata->fwid;
        return 0;
    }

    if (cts_dev->rtdata.program_mode) {
        cts_err("Get device firmware id while in program mode");
        ret = -ENODEV;
        goto err_out;
    }

    ret = cts_fw_reg_readw_retry(cts_dev,
            CTS_DEVICE_FW_REG_CHIP_TYPE, fwid, 5, 1);
    if (ret) {
        goto err_out;
    }

    *fwid = be16_to_cpu(*fwid);

    cts_info("Device firmware id: %04x", *fwid);

    if (!cts_is_fwid_valid(*fwid)) {
        cts_warn("Get invalid firmware id %04x", *fwid);
        ret = -EINVAL;
        goto err_out;
    }

    return 0;

err_out:
    *fwid = CTS_DEV_FWID_INVALID;
    return ret;
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

    ret = cts_hw_reg_readl_retry(cts_dev, CTS_DEV_HW_REG_HARDWARE_ID, hwid, 5, 0);
    if (ret) {
        goto err_out;
    }

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
    int  ret, retries = 0;
    u16  fwid = CTS_DEV_FWID_INVALID;
    u32  hwid = CTS_DEV_HWID_INVALID;
    u16  device_fw_ver = 0;
    //const struct cts_firmware *firmware = NULL;

    cts_info("Probe device");

read_fwid:
#ifdef CONFIG_CTS_I2C_HOST
    if (!cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR)) {
        cts_warn("Normal mode i2c addr is offline");
    } else
#else
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        cts_warn("Normal mode spi addr is offline");
    } else
#endif
    {
        cts_init_rtdata_with_normal_mode(cts_dev);
        ret = cts_get_fwid(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d, retries %d", ret, retries);
        } else {
            ret = cts_fw_reg_readw_retry(cts_dev,
                    CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
            if (ret) {
                cts_err("Read firmware version failed %d", ret);
                device_fw_ver = 0;
                // TODO: Handle this error
            } else {
                device_fw_ver = be16_to_cpu(device_fw_ver);
                cts_info("Device firmware version: %04x", device_fw_ver);
            }
            goto init_hwdata;
        }
    }

    /** - Try to read hardware id,
        it will enter program mode as normal */
    ret = cts_get_hwid(cts_dev, &hwid);
    if (ret || hwid == CTS_DEV_HWID_INVALID) {
        retries++;

        cts_err("Get hardware id failed %d retries %d", ret, retries);

        if (retries < 3) {
            cts_plat_reset_device(cts_dev->pdata);
            goto read_fwid;
        } else {
            return -ENODEV;
        }
    }

init_hwdata:
    ret = cts_init_device_hwdata(cts_dev, hwid, fwid);
    if (ret) {
        cts_err("Device hwid: %06x fwid: %04x not found", hwid, fwid);
        return -ENODEV;
    }

#if 0
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
    cts_warn("Force update firmware");
    firmware = cts_request_firmware(CTS_DEV_HWID_ANY, CTS_DEV_FWID_ANY, 0);
#else /* CFG_CTS_FIRMWARE_FORCE_UPDATE */
    firmware = cts_request_firmware(hwid, fwid, device_fw_ver);
#endif /* CFG_CTS_FIRMWARE_FORCE_UPDATE */


    retries = 0;
update_firmware:
    if (firmware) {
        ++retries;
        ret = cts_update_firmware(cts_dev, firmware, true);
        if (ret) {
            cts_err("Update firmware failed %d retries %d", ret, retries);

            if (retries < 3) {
                cts_plat_reset_device(cts_dev->pdata);
                goto update_firmware;
            } else {
                cts_release_firmware(firmware);
                return ret;
            }
        } else {
            cts_release_firmware(firmware);
        }
    } else {
        if (fwid == CTS_DEV_FWID_INVALID) {
            /* Device without firmware running && no updatable firmware found */
            return -ENODEV;
        } else {
            ret = cts_init_fwdata(cts_dev);
            if (ret) {
                cts_err("Device init firmware data failed %d", ret);
                return ret;
            }
        }
    }
#endif

    return 0;
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

int cts_get_gesture_info(const struct cts_device *cts_dev,
        void *gesture_info, bool trace_point)
{
    int    ret;

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

    ret = cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_GESTURE_INFO, gesture_info, 2);
    if(ret) {
        cts_err("Read gesture info header failed %d", ret);
        return ret;
    }

    if (trace_point) {
        ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_GESTURE_INFO + 2,
                gesture_info + 2,
                (((u8 *)gesture_info))[1] * sizeof(struct cts_device_gesture_point));
        if(ret) {
            cts_err("Read gesture trace points failed %d", ret);
            return ret;
        }
    }

    return 0;
}
#endif /* CFG_CTS_GESTURE */

#ifdef CONFIG_CTS_ESD_PROTECTION
static void cts_esd_protection_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    int ret;

    cts_info("ESD protection work");
    cts_data = container_of(work, struct chipone_ts_data, esd_work.work);
    cts_lock_device(&cts_data->cts_dev);
#ifdef CONFIG_CTS_I2C_HOST
    if (!cts_plat_is_i2c_online(cts_data->pdata, CTS_DEV_NORMAL_MODE_I2CADDR))
#else
    if (!cts_plat_is_normal_mode(cts_data->pdata))
#endif
    {
        cts_data->esd_check_fail_cnt++;
        /*reset chip next time*/
        if ((cts_data->esd_check_fail_cnt % 2) == 0) {
            cts_err("ESD protection read normal mode failed, reset chip!");
            ret = cts_plat_reset_device(cts_data->pdata);
            if (ret) {
                cts_err("ESD protection reset chip failed %d", ret);
            }
        }
    } else {
        cts_data->esd_check_fail_cnt = 0;
    }

    if (cts_data->esd_check_fail_cnt >= CFG_CTS_ESD_FAILED_CONFIRM_CNT) {
        const struct cts_firmware *firmware;

        cts_warn("ESD protection check failed, update firmware!!!");
        cts_stop_device_esdrecover(&cts_data->cts_dev);
        firmware = cts_request_firmware(cts_data->cts_dev.hwdata->hwid,
                cts_data->cts_dev.hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(&cts_data->cts_dev, firmware, true);
            cts_release_firmware(firmware);

            if (ret) {
                cts_err("Update default firmware failed %d", ret);
            }
        } else {
            cts_err("Request default firmware failed %d, "
                    "please update manually!!", ret);
        }
        cts_start_device_esdrecover(&cts_data->cts_dev);
        cts_data->esd_check_fail_cnt = 0;
    }
    queue_delayed_work(cts_data->esd_workqueue,
        &cts_data->esd_work, CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);

    cts_unlock_device(&cts_data->cts_dev);
}

void cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && !cts_data->esd_enabled) {
        cts_info("ESD protection enable");

        cts_data->esd_enabled = true;
        cts_data->esd_check_fail_cnt = 0;
        queue_delayed_work(cts_data->esd_workqueue,
            &cts_data->esd_work, CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);
    }
}

void cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->esd_workqueue && cts_data->esd_enabled) {
        cts_info("ESD protection disable");

        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
        flush_workqueue(cts_data->esd_workqueue);
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

#ifdef CONFIG_CTS_GLOVE
int cts_enter_glove_mode(const struct cts_device *cts_dev)
{
    cts_info("Enter glove mode");

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GLOVE_MODE, 1);
    if (ret) {
        cts_err("Enable Glove mode err");
    }
    else {
        cts_dev->rtdata.glove_mode_enabled = true;
    }
    return ret;
}

int cts_exit_glove_mode(const struct cts_device *cts_dev)
{
    cts_info("Exit glove mode");

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GLOVE_MODE, 0);
    if (ret) {
        cts_err("Exit Glove mode err");
    }
    else {
        cts_dev->rtdata.glove_mode_enabled = false;
    }
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
    int  ret;

    cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

    ret = cts_is_charger_attached(cts_data, &attached);
    if (ret) {
        cts_err("Get charger state failed %d", ret);
    }

    cts_dev->rtdata.charger_exist = attached;

    return attached;
}

int cts_set_dev_charger_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;

    cts_info("Set dev charger %s", attached ? "ATTACHED" : "DETATCHED");
    ret = cts_send_command(cts_dev,
        attached ? CTS_CMD_CHARGER_ATTACHED : CTS_CMD_CHARGER_DETACHED);
    if (ret) {
        if (ret) {
            cts_err("Send CMD_CHARGER_%s failed %d",
                attached ? "ATTACHED" : "DETACHED", ret);
        }
    }

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

    cts_info("Set dev earjack %s", attached ? "ATTACHED" : "DETATCHED");
    ret = cts_send_command(cts_dev,
        attached ? CTS_CMD_EARJACK_ATTACHED : CTS_CMD_EARJACK_DETACHED);
    if (ret) {
        cts_err("Send CMD_EARJACK_%s failed %d",
            attached ? "ATTACHED" : "DETACHED", ret);
    }

    return ret;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

int cts_enable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Fw log redirect enable");
    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_FW_LOG_REDIRECT);
    if (ret) {
        cts_err("Send CTS_CMD_ENABLE_FW_LOG_REDIRECT failed %d", ret);
    } else {
        cts_dev->rtdata.fw_log_redirect_enabled = true;
    }
    return ret;
}

int cts_disable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Fw log redirect disable");
    ret = cts_send_command(cts_dev, CTS_CMD_DISABLE_FW_LOG_REDIRECT);
    if (ret) {
        cts_err("Send CTS_CMD_DISABLE_FW_LOG_REDIRECT failed %d", ret);
    } else {
        cts_dev->rtdata.fw_log_redirect_enabled = false;
    }
    return ret;
}

bool cts_is_fw_log_redirect(struct cts_device *cts_dev)
{
    return cts_dev->rtdata.fw_log_redirect_enabled;
}

int cts_fw_log_show_finish(struct cts_device *cts_dev)
{
    int ret;

    ret = cts_send_command(cts_dev, CTS_CMD_FW_LOG_SHOW_FINISH);
    if (ret) {
        cts_err("Send CTS_CMD_FW_LOG_SHOW_FINISH failed %d", ret);
    }

    return ret;
}

int cts_get_compensate_cap(struct cts_device *cts_dev, u8 *cap)
{
    int i, ret;
    u8  auto_calib_comp_cap_enabled;

    if (cts_dev == NULL || cap == NULL) {
        cts_err("Get compensate cap with cts_dev(%p) or cap(%p) = NULL",
            cts_dev, cap);
        return -EINVAL;
    }

    if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911 &&
        cts_dev->fwdata.lib_version < 0x0500) {
        cts_err("ICNL9911 lib version 0x%04x < v5.0 "
                "NOT supported get compensate cap",
                cts_dev->fwdata.lib_version);
        return -ENOTSUPP;
    }

    cts_info("Get compensate cap");

    /* Check whether auto calibrate compensate cap enabled */
    cts_info(" - Get auto calib comp cap enable");
    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_ENABLE,
        &auto_calib_comp_cap_enabled);
    if (ret) {
        cts_err("Get auto calib comp cap enable failed %d", ret);
        return ret;
    }

    /* Wait auto calibrate compensate cap done if enabled */
    if (auto_calib_comp_cap_enabled) {
        u8 auto_calib_comp_cap_done;

        cts_info(" - Wait auto calib comp cap done...");

        i = 0;
        do {
            ret = cts_fw_reg_readb(cts_dev,
                CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_DONE,
                &auto_calib_comp_cap_done);
            if (ret) {
                cts_err("Get auto calib comp cap done failed %d", ret);
            } else {
                if (auto_calib_comp_cap_done) {
                    goto enable_read_compensate_cap;
                }
            }

            mdelay(5);
        } while (++i < 100);

        cts_err("Wait auto calib comp cap done timeout");
        return -ETIMEDOUT;
    }

enable_read_compensate_cap:
    cts_info(" - Enable read comp cap");
    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_CNEG);
    if (ret) {
        cts_err("Enable read comp cap failed %d",ret);
        return ret;
    }

    /* Wait compensate cap ready */
    cts_info(" - Wait comp cap ready...");
    i = 0;
    do {
        u8 ready;

        mdelay(5);

        ret = cts_fw_reg_readb(cts_dev,
            CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY, &ready);
        if (ret) {
            cts_err("Read comp cap ready failed %d", ret);
        } else {
            if (ready) {
                goto read_compensate_cap;
            }
        }
    } while (++i < 100);

    cts_err("Wait comp cap ready timeout");
    return -ETIMEDOUT;

read_compensate_cap:
    /* Use hardware row & col here */
    cts_info(" - Read comp cap");
    ret = cts_fw_reg_readsb_delay_idle(cts_dev,
        CTS_DEVICE_FW_REG_COMPENSATE_CAP, cap,
        cts_dev->hwdata->num_row * cts_dev->hwdata->num_col,
        500);
    if (ret) {
        cts_err("Read comp cap failed %d",ret);
        // Fall through to disable read compensate cap
    }

    cts_info(" - Clear comp cap ready");
    i = 0;
    do {
        int r;
        u8  ready;

        r = cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_CNEG);
        if (r) {
            cts_err("Send cmd DISABLE_READ_CNEG failed %d", r);
            continue;
        }

        mdelay(5);
        r = cts_fw_reg_readb(cts_dev,
                CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY, &ready);
        if (r) {
            cts_err("Re-Check comp cap ready failed %d", r);
            continue;
        }

        if (ready) {
            cts_warn("Comp cap ready is still set");
            continue;
        } else {
            return ret;
        }
    }while (++i < 100);

    cts_warn("Clr comp cap ready failed, try to do reset!");

    /* Try to do hardware reset */
    cts_plat_reset_device(cts_dev->pdata);

#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_dev->fwdata.supp_headphone_cable_reject &&
        cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev earjack attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        int r = cts_enter_glove_mode(cts_dev);
        if (r) {
            cts_err("Enter dev glove mode failed %d", r);
        }
    }
#endif /* CONFIG_CTS_GLOVE */

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        int r = cts_enable_fw_log_redirect(cts_dev);
        if (r) {
            cts_err("Enable fw log redirect failed %d", r);
        }
    }
#endif /* CONFIG_CTS_GLOVE */

    return ret;
}

static struct file *cts_log_filp = NULL;
static int cts_log_to_file_level = 0;
extern int cts_write_file(struct file *filp, const void *data, size_t size);

static char *cts_log_buffer = NULL;
static int cts_log_buf_size = 0;
static int cts_log_buf_wr_size = 0;

static bool cts_log_redirect = false;

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
    }

    cts_log_redirect = true;

    return ret;
#undef START_BANNER
}

void cts_stop_driver_log_redirect(void)
{
#define END_BANNER \
        "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"

    cts_log_redirect = false;

    cts_info("Stop driver log redirect");

    if (cts_log_filp) {
        int ret;

        cts_info(" - Stop driver log to file");

        cts_write_file(cts_log_filp, END_BANNER, strlen(END_BANNER));
        ret = filp_close(cts_log_filp, NULL);
        if (ret) {
            cts_err("Close driver log file failed %d", ret);
        }
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
            level <= cts_log_to_file_level)
        {
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

void cts_firmware_upgrade_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    const struct cts_firmware *firmware;
    int retries;
    int ret;

    cts_info("Firmware upgrade work");

    cts_data = container_of(work, struct chipone_ts_data,
        fw_upgrade_work.work);
    cts_dev = &cts_data->cts_dev;

    firmware = cts_request_firmware(cts_dev, cts_dev->hwdata->hwid,
        CTS_DEV_FWID_ANY, cts_dev->fwdata.version);
    if (firmware == NULL) {
        cts_err("Request firmware failed");
        return ;
    }

    retries = 0;
    do {
        ret = cts_update_firmware(cts_dev, firmware, true);
        if (ret) {
            cts_err("Update firmware failed %d retries %d", ret,
                retries);

            cts_plat_reset_device(cts_dev->pdata);
        } else {
            break;
        }
    } while (++retries < 3);

    cts_release_firmware(firmware);
    
    if (ret == 0) {
        cts_start_device(cts_dev);
    }
}


