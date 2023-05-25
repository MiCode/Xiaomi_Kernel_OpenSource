#define LOG_TAG         "SFCtrl"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"

#define rSFCTRLv2_CMD_SEL               (0x0000)
#define rSFCTRLv2_FLASH_ADDR            (0x0004)
#define rSFCTRLv2_SRAM_ADDR             (0x0008)
#define rSFCTRLv2_DATA_LENGTH           (0x000C)
#define rSFCTRLv2_START_DEXC            (0x0010)
#define rSFCTRLv2_RELEASE_FLASH         (0x0014)
#define rSFCTRLv2_HW_STATE              (0x0018)
#define rSFCTRLv2_CRC_RESULT            (0x001C)
#define rSFCTRLv2_SRAM_CRC_START        (0x0020)
#define rSFCTRLv2_FLASH_CRC_START       (0x0022)
#define rSFCTRLv2_SF_BUSY               (0x0024)

/** Constants for register @ref rSFCTRLv2_CMD_SEL */
enum sfctrlv2_cmd {
    SFCTRLv2_CMD_FAST_READ = 0x01u,
    SFCTRLv2_CMD_SE = 0x02u,
    SFCTRLv2_CMD_BE = 0x03u,
    SFCTRLv2_CMD_PP = 0x04u,
    SFCTRLv2_CMD_RDSR = 0x05u,
    SFCTRLv2_CMD_RDID = 0x06u
};

/** SPI flash controller v2 operation flags. */
enum sfctrlv2_opflags {
    SFCTRLv2_OPFLAG_READ = BIT(0),
    SFCTRLv2_OPFLAG_SET_FLASH_ADDR = BIT(1),
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG = BIT(2),
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH = BIT(3),
    SFCTRLv2_OPFLAG_WAIT_WIP_CLR = BIT(4),

};

#define SFCTRLv2_CMD_RDID_FLAGS \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG)

#define SFCTRLv2_CMD_RDSR_FLAGS    \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG)

#define SFCTRLv2_CMD_SE_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_WAIT_WIP_CLR)

#define SFCTRLv2_CMD_BE_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_WAIT_WIP_CLR)

#define SFCTRLv2_CMD_PP_FLAGS \
    (SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG  | \
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH | \
    SFCTRLv2_OPFLAG_WAIT_WIP_CLR)

#define SFCTRLv2_CMD_FAST_READ_FLAGS  \
    (SFCTRLv2_OPFLAG_READ | \
    SFCTRLv2_OPFLAG_SET_FLASH_ADDR | \
    SFCTRLv2_OPFLAG_SRAM_DATA_XCHG  | \
    SFCTRLv2_OPFLAG_SET_DATA_LENGTH)

#define sfctrl_reg_addr(cts_dev, offset)  \
    ((cts_dev)->hwdata->sfctrl->reg_base + offset)

#define DEFINE_SFCTRL_REG_ACCESS_FUNC(access_type, data_type) \
    static inline int sfctrl_reg_ ## access_type(const struct cts_device *cts_dev, \
        u32 reg, data_type data) { \
        return cts_hw_reg_ ## access_type(cts_dev, sfctrl_reg_addr(cts_dev, reg), data); \
    }

DEFINE_SFCTRL_REG_ACCESS_FUNC(writeb, u8)
DEFINE_SFCTRL_REG_ACCESS_FUNC(writew, u16)
DEFINE_SFCTRL_REG_ACCESS_FUNC(writel, u32)
DEFINE_SFCTRL_REG_ACCESS_FUNC(readb, u8 *)
DEFINE_SFCTRL_REG_ACCESS_FUNC(readw, u16 *)
DEFINE_SFCTRL_REG_ACCESS_FUNC(readl, u32 *)

#define sfctrl_write_reg_check_ret(access_type, reg, val) \
    do { \
        int ret; \
        cts_dbg("Write " #reg " to 0x%x", val); \
        ret = sfctrl_reg_ ## access_type(cts_dev, reg, val); \
        if (ret) { \
            cts_err("Write " #reg " failed %d", ret); \
            return ret; \
        } \
    } while (0)

#define sfctrl_read_reg_check_ret(access_type, reg, val) \
    do { \
        int ret; \
        cts_dbg("Read " #reg ""); \
        ret = sfctrl_reg_ ## access_type(cts_dev, reg, val); \
        if (ret) { \
            cts_err("Read " #reg " failed %d", ret); \
            return ret; \
        } \
    } while (0)

static int wait_sfctrl_xfer_comp(const struct cts_device *cts_dev)
{
    int retries = 0;
    u8  status;

    do {
        sfctrl_read_reg_check_ret(readb, rSFCTRLv2_SF_BUSY, &status);
        if(status == 0) {
            return 0;
        }

        mdelay(1);
    }  while(status && retries++ < 1000);

    return -ETIMEDOUT;
}

static int sfctrlv2_rdsr(const struct cts_device *cts_dev, u8 *status)
{
#define RDSR_XCHG_SRAM_ADDR     (cts_dev->hwdata->sram_size - 1)

    int ret;

    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_CMD_SEL, SFCTRLv2_CMD_RDSR);
    sfctrl_write_reg_check_ret(writel, rSFCTRLv2_SRAM_ADDR, RDSR_XCHG_SRAM_ADDR);
    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_START_DEXC, 1);

    if((ret = wait_sfctrl_xfer_comp(cts_dev)) != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    ret = cts_sram_readb(cts_dev, RDSR_XCHG_SRAM_ADDR, status);
    if (ret) {
        cts_err("Read exchange sram failed %d", ret);
        return ret;
    }
#undef RDSR_XCHG_SRAM_ADDR

    return 0;
}

static int wait_flash_wip_clear(const struct cts_device *cts_dev)
{
#define FLASH_SR_WIP        BIT(0)      /*!< Write in progress */

    int ret, retries = 0;
    u8 status;

    do {
        ret = sfctrlv2_rdsr(cts_dev, &status);
        if (ret) {
            cts_err("Read flash status register failed %d", ret);
            return ret;
        }

        if (status & FLASH_SR_WIP) {
            mdelay(1);
        } else {
            return 0;
        }
    } while (status & FLASH_SR_WIP && retries++ < 1000);

    return -ETIMEDOUT;
#undef FLASH_SR_WIP
}

static int sfctrlv2_transfer(const struct cts_device *cts_dev,
        u8 cmd, void *data, u32 flash_addr, u32 sram_addr, size_t size, u32 flags)
{
    int ret;

    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_CMD_SEL, cmd);

    if (flags & SFCTRLv2_OPFLAG_SRAM_DATA_XCHG) {
        sfctrl_write_reg_check_ret(writel, rSFCTRLv2_SRAM_ADDR, sram_addr);

        /** - Write data to exchange SRAM if operation is write and
         *        data != NULL(data not in SRAM) */
        if ((!(flags & SFCTRLv2_OPFLAG_READ)) && data) {
            ret = cts_sram_writesb(cts_dev, sram_addr, data, size);
            if (ret) {
                cts_err("Write data to exchange sram 0x%06x size %zu failed %d",
                    sram_addr, size, ret);
                return ret;
            }
        }
    }

    if (flags & SFCTRLv2_OPFLAG_SET_FLASH_ADDR) {
        sfctrl_write_reg_check_ret(writel, rSFCTRLv2_FLASH_ADDR, flash_addr);
    }
    if (flags & SFCTRLv2_OPFLAG_SET_DATA_LENGTH) {
        sfctrl_write_reg_check_ret(writel, rSFCTRLv2_DATA_LENGTH, (u32)size);
    }

    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_START_DEXC, 1);

    if((ret = wait_sfctrl_xfer_comp(cts_dev)) != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    if (flags & SFCTRLv2_OPFLAG_WAIT_WIP_CLR &&
        (ret = wait_flash_wip_clear(cts_dev)) != 0) {
        cts_err("Wait WIP clear failed %d", ret);
        return ret;
    }

    if ((flags & SFCTRLv2_OPFLAG_READ) && data) {
        ret = cts_sram_readsb(cts_dev, sram_addr, data, size);
        if (ret) {
            cts_err("Read data from exchange sram 0x%06x size %zu failed %d",
                sram_addr, size, ret);
            return ret;
        }
    }

    return 0;
}

static int sfctrlv2_rdid(const struct cts_device *cts_dev, u32 *id)
{
    int ret;
    u8  buf[4];

    ret = sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_RDID, buf,
            0, cts_dev->hwdata->sfctrl->xchg_sram_base, 3,
            SFCTRLv2_CMD_RDID_FLAGS);
    *id = ret ? 0 : get_unaligned_be24(buf);

    return ret;
}

static int sfctrlv2_se(const struct cts_device *cts_dev, u32 sector_addr)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_SE, NULL,
                sector_addr, 0, 0, SFCTRLv2_CMD_SE_FLAGS);
}

static int sfctrlv2_be(const struct cts_device *cts_dev, u32 block_addr)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_BE, NULL,
                block_addr, 0, 0, SFCTRLv2_CMD_BE_FLAGS);
}

static int sfctrlv2_pp(const struct cts_device *cts_dev,
        u32 flash_addr, const void *src, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_PP, (void *)src,
                flash_addr, cts_dev->hwdata->sfctrl->xchg_sram_base, size,
                SFCTRLv2_CMD_PP_FLAGS);
}

static int sfctrlv2_program_flash_from_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t len)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_PP, NULL,
                flash_addr, sram_addr, len, SFCTRLv2_CMD_PP_FLAGS);
}

static int sfctrlv2_read(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_FAST_READ, dst,
            flash_addr, cts_dev->hwdata->sfctrl->xchg_sram_base, size,
            SFCTRLv2_CMD_FAST_READ_FLAGS);
}

static int sfctrlv2_read_flash_to_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size)
{
    return sfctrlv2_transfer(cts_dev, SFCTRLv2_CMD_FAST_READ, NULL,
            flash_addr, sram_addr, size, SFCTRLv2_CMD_FAST_READ_FLAGS);
}

int sfctrlv2_calc_sram_crc(const struct cts_device *cts_dev,
        u32 sram_addr, size_t size, u32 *crc)
{
    int ret;

    sfctrl_write_reg_check_ret(writel, rSFCTRLv2_SRAM_ADDR, sram_addr);
    sfctrl_write_reg_check_ret(writel, rSFCTRLv2_DATA_LENGTH, (u32)size);
    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_SRAM_CRC_START, 1);

    if((ret = wait_sfctrl_xfer_comp(cts_dev)) != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    sfctrl_read_reg_check_ret(readl, rSFCTRLv2_CRC_RESULT, crc);

    return 0;
}

int sfctrlv2_calc_flash_crc(const struct cts_device *cts_dev,
        u32 flash_addr, size_t size, u32 *crc)
{
    int ret;

    cts_info("Calc crc from flash 0x%06x size %zu", flash_addr, size);

    sfctrl_write_reg_check_ret(writel, rSFCTRLv2_FLASH_ADDR, flash_addr);
    sfctrl_write_reg_check_ret(writel, rSFCTRLv2_DATA_LENGTH, (u32)size);
    sfctrl_write_reg_check_ret(writeb, rSFCTRLv2_FLASH_CRC_START, 1);

    if((ret = wait_sfctrl_xfer_comp(cts_dev)) != 0) {
        cts_err("Wait sfctrl ready failed %d", ret);
        return ret;
    }

    sfctrl_read_reg_check_ret(readl, rSFCTRLv2_CRC_RESULT, crc);

    return 0;
}

const struct cts_sfctrl_ops cts_sfctrlv2_ops = {
    .rdid = sfctrlv2_rdid,
    .se = sfctrlv2_se,
    .be = sfctrlv2_be,
    .read = sfctrlv2_read,
    .read_to_sram = sfctrlv2_read_flash_to_sram,
    .program = sfctrlv2_pp,
    .program_from_sram = sfctrlv2_program_flash_from_sram,
    .calc_sram_crc = sfctrlv2_calc_sram_crc,
    .calc_flash_crc = sfctrlv2_calc_flash_crc,
};

