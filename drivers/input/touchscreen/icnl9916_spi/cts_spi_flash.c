#define LOG_TAG         "Flash"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_strerror.h"

/* NOTE: double check command sets and memory organization when you add
 * more flash chips.  This current list focusses on newer chips, which
 * have been converging on command sets which including JEDEC ID.
 */
static const struct cts_flash cts_flashes[] = {
    /* Winbond */
    { "Winbond,W25Q10EW",
      0xEF6011, 256, 0x1000, 0x8000, 0x20000},
    { "Winbond,W25Q20EW",
      0xEF6012, 256, 0x1000, 0x8000, 0x40000},
    { "Winbond,W25Q40EW",
      0xEF6013, 256, 0x1000, 0x8000, 0x80000},
    { "Winbond,W25Q20BW",
      0xEF5012, 256, 0x1000, 0x8000, 0x40000},
    { "Winbond,W25Q40BW",
      0xEF5013, 256, 0x1000, 0x8000, 0x80000},

    /* Giga device */
    { "Giga,GD25LQ10B",
      0xC86011, 256, 0x1000, 0x8000, 0x20000},
    { "Giga,GD25LD20CEIGR",
      0xC86012, 256, 0x1000, 0x8000, 0x40000},
    { "Giga,GD25LD40CEIGR",
      0xC86013, 256, 0x1000, 0x8000, 0x80000},

    /* Macronix */
    { "Macronix,MX25U1001E",
      0xC22531,  32, 0x1000, 0x10000, 0x20000},
    { "Macronix,MX25R2035F",
      0xC22812, 256, 0x1000,  0x8000, 0x40000},
    { "Macronix,MX25R4035F",
      0xC22813, 256, 0x1000,  0x8000, 0x80000},

    /* Puya-Semi */
    { "Puya-Semi,P25Q20LUVHIR",
      0x856012, 256, 0x1000, 0x8000, 0x40000},
    { "Puya-Semi,P25Q40LUXHIR",
      0x856013, 256, 0x1000, 0x8000, 0x80000},
    { "Puya-Semi,P25Q11L",
      0x854011, 256, 0x1000, 0x8000, 0x20000},
    { "Puya-Semi,P25Q21L",
      0x854012, 256, 0x1000, 0x8000, 0x40000},
    { "Puya-Semi,P25T22L",
      0x854412, 256, 0x1000, 0x8000, 0x40000},


    /* Boya */
    { "Boya,BY25D10",
      0x684011, 256, 0x1000, 0x8000, 0x20000},

    /* EON */
    { "EoN,EN25S20A",
      0x1C3812, 256, 0x1000, 0x8000, 0x40000},

    /* XTXTECH */
    { "XTX,XT25Q01",
      0x0B6011, 256, 0x1000,      0, 0x20000},
    { "XTX,XT25Q02",
      0x0B6012, 256, 0x1000,      0, 0x40000},

    /* Xinxin-Semi */
    { "Xinxin-Semi,XM25QU20B",
      0x205012, 256, 0x1000, 0x8000, 0x40000},
    { "Xinxin-Semi,XM25QU40B",
      0x205013, 256, 0x1000, 0x8000, 0x80000},

    { "KangYong,XK25Q20T",
      0xEB6012, 256, 0x1000, 0x8000, 0x40000},
};

static const struct cts_flash *find_flash_by_jedec_id(u32 jedec_id)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_flashes); i++) {
        if (cts_flashes[i].jedec_id == jedec_id) {
            return &cts_flashes[i];
        }
    }

    return NULL;
}

static int probe_flash(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Probe flash");

    if (cts_dev->hwdata->sfctrl->ops->rdid != NULL) {
        u32 id;

        cts_info("Read JEDEC ID");

        ret = cts_dev->hwdata->sfctrl->ops->rdid(cts_dev, &id);
        if (ret) {
            cts_err("Read JEDEC ID failed %d(%s)",
                ret, cts_strerror(ret));
            return ret;
        }

        cts_dev->flash = find_flash_by_jedec_id(id);
        if (cts_dev->flash == NULL) {
            cts_err("Unknown JEDEC ID: %06x", id);
            return -ENODEV;
        }

        cts_info("Flash type: '%s'", cts_dev->flash->name);
        return 0;
    } else {
        cts_err("Probe flash with sfctrl->ops->rdid == NULL");
        return -ENOTSUPP;
    }
}

/** Make sure sector addr is sector aligned && < flash total size */
static int erase_sector_retry(const struct cts_device *cts_dev,
        u32 sector_addr, int retry)
{
    int ret, retries;

    cts_info("  Erase sector 0x%06x", sector_addr);

    retries = 0;
    do {
        retries++;
        ret = cts_dev->hwdata->sfctrl->ops->se(cts_dev, sector_addr);
        if (ret) {
            cts_err("Erase sector 0x%06x failed %d(%s) retries %d",
                sector_addr, ret, cts_strerror(ret), retries);
            continue;
        }
    } while (retries < retry);

    return ret;
}

/** Make sure sector addr is sector aligned && < flash total size */
static inline int erase_sector(const struct cts_device *cts_dev,
        u32 sector_addr)
{
    return erase_sector_retry(cts_dev, sector_addr, CTS_FLASH_ERASE_DEFAULT_RETRY);
}

/** Make sure block addr is block aligned && < flash total size */
static int erase_block_retry(const struct cts_device *cts_dev,
        u32 block_addr, int retry)
{
    int ret, retries;

    cts_info("  Erase block  0x%06x", block_addr);

    retries = 0;
    do {
        retries++;

        ret = cts_dev->hwdata->sfctrl->ops->be(cts_dev, block_addr);
        if (ret) {
            cts_err("Erase block 0x%06x failed %d(%s) retries %d",
                block_addr, ret, cts_strerror(ret), retries);
            continue;
        }
    } while (retries < retry);

    return ret;
}

/** Make sure block addr is block aligned && < flash total size */
static inline int erase_block(const struct cts_device *cts_dev,
        u32 block_addr)
{
    return erase_block_retry(cts_dev, block_addr,
        CTS_FLASH_ERASE_DEFAULT_RETRY);
}

int cts_prepare_flash_operation(struct cts_device *cts_dev)
{
    int ret;
    u8  diva = 0;
    bool program_mode = cts_is_device_program_mode(cts_dev);
    u32 hwid;
    bool enabled = cts_is_device_enabled(cts_dev);

    cts_info("Prepare for flash operation");

    if (!program_mode) {
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d(%s)",
                ret, cts_strerror(ret));
            goto err_start_device;
        }
    }

    ret = cts_hw_reg_readb_retry(cts_dev, CTS_DEV_HW_REG_CLK_DIV_CFG, &diva, 5, 0);
    if (ret) {
        cts_warn("Read DIVA failed %d(%s)", ret, cts_strerror(ret));
    } else {
        cts_dbg("Device DIVA = %d", diva);
    }

    if (ret == 0 && diva == 0x0C) {
        cts_info("DIVA is ready already");
    } else {
        int retries;

        /* Set HCLK to 10MHz */
        hwid = cts_dev->hwdata->hwid;
        if (hwid == CTS_DEV_HWID_ICNL9911 || hwid == CTS_DEV_HWID_ICNL9911C \
         || hwid == CTS_DEV_HWID_ICNL9951) {
            ret = cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_CLK_DIV_CFG, 0x0C, 5, 0);
            if (ret) {
                cts_err("Write DIVA failed %d(%s)",
                    ret, cts_strerror(ret));
                goto err_enter_normal_mode;
            }
         }    

        /* Reset SFCTL */
        ret = cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_RESET_CONFIG, 0xFB, 5, 0);
        if (ret) {
            cts_err("Reset sfctl failed %d(%s)",
                ret, cts_strerror(ret));
            goto err_enter_normal_mode;
        }

        retries = 0;
        do {
            u8 state;

            ret = cts_hw_reg_readb_relaxed(cts_dev, CTS_DEV_HW_REG_HW_STATUS, &state);
            if (ret == 0 && (state & 0x40) != 0)
                goto init_flash;

            mdelay(2);
        } while (++retries < 1000);

        cts_warn("Wait SFCTRL ready failed %d(%s)",
            ret, cts_strerror(ret));
        // Go through and try
    }

init_flash:
    if (cts_dev->flash == NULL) {
        cts_info("Flash is not initialized, try to probe...");
        if ((ret = probe_flash(cts_dev)) != 0) {
            cts_dev->rtdata.has_flash = false;
            cts_warn("Probe flash failed %d(%s)",
                ret, cts_strerror(ret));
            return 0;
        }
    }
    cts_dev->rtdata.has_flash = true;
    return 0;

err_enter_normal_mode:
    if (!program_mode) {
        int r = cts_enter_normal_mode(cts_dev);
        if (r) {
            cts_err("Enter normal mode failed %d(%s)",
                r, cts_strerror(r));
        }
    }
err_start_device:
    if (enabled) {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d(%s)",
                r, cts_strerror(r));
        }
    }

    return ret;
}

int cts_post_flash_operation(struct cts_device *cts_dev)
{
    u32 hwid;
    cts_info("Post flash operation");

    hwid = cts_dev->hwdata->hwid;
    if (hwid == CTS_DEV_HWID_ICNL9911 || hwid == CTS_DEV_HWID_ICNL9911C \
      ||hwid == CTS_DEV_HWID_ICNL9951) {
        return cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_CLK_DIV_CFG, 4, 5, 0);
    }
    return 0;    
}

int cts_read_flash_retry(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size, int retry)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Read from 0x%06x size %zu", flash_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->read == NULL) {
        cts_err("Read not supported");
        return -ENOTSUPP;
    }

    if (flash_addr > flash->total_size) {
        cts_err("Read from 0x%06x > flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Read actually from 0x%06x size %zu", flash_addr, size);

    while (size) {
        size_t l;

        l = min(sfctrl->xchg_sram_size, size);

        ret = sfctrl->ops->read(cts_dev, flash_addr, dst, l);
        if(ret < 0) {
            cts_err("Read from 0x%06x size %zu failed %d(%s)",
                flash_addr, size, ret, cts_strerror(ret));
            return ret;
        }

        dst += l;
        size -= l;
        flash_addr += l;
    }

    return 0;
}

int cts_read_flash_to_sram_retry(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size, int retry)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret, retries;

    cts_info("Read from 0x%06x to sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->read_to_sram == NULL) {
        cts_err("Read to sram not supported");
        return -ENOTSUPP;
    }

    if (flash_addr > flash->total_size) {
        cts_err("Read to sram from 0x%06x > flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Read to sram actually from 0x%06x size %zu",
        flash_addr, size);

    retries = 0;
    do {
        retries++;
        ret = sfctrl->ops->read_to_sram(cts_dev,
           flash_addr, sram_addr, size);
        if(ret) {
            cts_err("Read from 0x%06x to sram 0x%06x size %zu "
                    "failed %d(%s) retries %d",
                flash_addr, sram_addr, size,
                ret, cts_strerror(ret), retries);
            continue;
        }
    } while (retries < retry);

    return ret;
}

int cts_read_flash_to_sram_check_crc_retry(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size, u32 crc, int retry)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret, retries;

    cts_info("Read from 0x%06x to sram 0x%06x size %zu with crc check",
        flash_addr, sram_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->read_to_sram == NULL ||
        sfctrl->ops->calc_sram_crc == NULL) {
        cts_err("Read to sram check crc not supported");
        return -ENOTSUPP;
    }

    if (flash_addr > flash->total_size) {
        cts_err("Read to sram from 0x%06x > flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Read to sram actually from 0x%06x size %zu with crc check",
        flash_addr, size);

    retries = 0;
    do {
        u32 crc_sram;

        retries++;

        ret = sfctrl->ops->read_to_sram(cts_dev,
            flash_addr, sram_addr, size);
        if(ret) {
            cts_err("Read from 0x%06x to sram 0x%06x size %zu "
                    "failed %d(%s) retries %d",
                flash_addr, sram_addr, size,
                ret, cts_strerror(ret), retries);
            continue;
        }

        ret = sfctrl->ops->calc_sram_crc(cts_dev,
            sram_addr, size, &crc_sram);
        if (ret) {
            cts_err("Get crc for read "
                    "from 0x%06x to sram 0x%06x size %zu "
                    "failed %d(%s) retries %d",
                flash_addr, sram_addr, size,
                ret, cts_strerror(ret), retries);
            continue;
        }

        if (crc == crc_sram) {
            return 0;
        }

        cts_err("Check crc for read "
                "from 0x%06x to sram 0x%06x size %zu "
                "mismatch %x != %x, retries %d",
            flash_addr, sram_addr, size, crc, crc_sram, retries);
        ret = -EFAULT;
    } while (retries < retry);

    return ret;
}

int cts_program_flash(const struct cts_device *cts_dev,
        u32 flash_addr, const void *src, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Program to 0x%06x size %zu", flash_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->program == NULL) {
        cts_err("Program not supported");
        return -ENOTSUPP;
    }

    if (flash_addr >= flash->total_size) {
        cts_err("Program from 0x%06x >= flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Program actually to 0x%06x size %zu", flash_addr, size);

    while (size) {
        size_t l, offset;

        l = min(flash->page_size, size);
        offset = flash_addr & (flash->page_size - 1);

        if (offset) {
            l = min(flash->page_size - offset, l);
        }

        ret = sfctrl->ops->program(cts_dev, flash_addr, src, l);
        if(ret) {
            cts_err("Program to 0x%06x size %zu failed %d(%s)",
                flash_addr, l, ret, cts_strerror(ret));
            return ret;
        }

        src += l;
        size -= l;
        flash_addr += l;
    }

    return 0;
}

int cts_program_flash_from_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Program to 0x%06x from sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL ||
        sfctrl->ops == NULL ||
        sfctrl->ops->program_from_sram == NULL) {
        cts_err("Program from sram not supported");
        return -ENOTSUPP;
    }

    if (flash_addr >= flash->total_size) {
        cts_err("Program from 0x%06x >= flash size 0x%06zx",
            flash_addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - flash_addr);

    cts_info("Program actually to 0x%06x from sram 0x%06x size %zu",
        flash_addr, sram_addr, size);

    while (size) {
        size_t l, offset;

        l = min(flash->page_size, size);
        offset = flash_addr & (flash->page_size - 1);

        if (offset) {
            l = min(flash->page_size - offset, l);
        }

        ret = sfctrl->ops->program_from_sram(cts_dev,
            flash_addr, sram_addr, l);
        if(ret) {
            cts_err("Program to 0x%06x from sram 0x%06x size %zu "
                   "failed %d(%s)",
                flash_addr, sram_addr, l, ret, cts_strerror(ret));
            return ret;
        }

        size -= l;
        flash_addr += l;
        sram_addr  += l;
    }

    return 0;
}

int cts_erase_flash(const struct cts_device *cts_dev, u32 addr, size_t size)
{
    const struct cts_sfctrl *sfctrl;
    const struct cts_flash *flash;
    int ret;

    cts_info("Erase from 0x%06x size %zu", addr, size);

    sfctrl = cts_dev->hwdata->sfctrl;
    flash  = cts_dev->flash;

    if (flash == NULL ||
        sfctrl == NULL || sfctrl->ops == NULL ||
        sfctrl->ops->se == NULL || sfctrl->ops->be == NULL ||
        flash == NULL) {
        cts_err("Oops");
        return -EINVAL;
    }

    /* Addr and size MUST sector aligned */
    addr = rounddown(addr, flash->sector_size);
    size = roundup(size, flash->sector_size);

    if (addr > flash->total_size) {
        cts_err("Erase from 0x%06x > flash size 0x%06zx",
            addr, flash->total_size);
        return -EINVAL;
    }
    size = min(size, flash->total_size - addr);

    cts_info("Erase actually from 0x%06x size %zu", addr, size);

    if (flash->block_size) {
        while (addr != ALIGN(addr, flash->block_size) &&
               size >= flash->sector_size) {
            ret = erase_sector(cts_dev, addr);
            if (ret) {
                cts_err("Erase sector 0x%06x size 0x%04zx failed %d(%s)",
                    addr, flash->sector_size, ret, cts_strerror(ret));
                return ret;
            }
            addr += flash->sector_size;
            size -= flash->sector_size;
        }

        while (size >= flash->block_size) {
            ret = erase_block(cts_dev, addr);
            if (ret) {
                cts_err("Erase block 0x%06x size 0x%04zx failed %d(%s)",
                    addr, flash->block_size, ret, cts_strerror(ret));
                return ret;
            }
            addr += flash->block_size;
            size -= flash->block_size;
        }
    }

    while (size >= flash->sector_size) {
        ret = erase_sector(cts_dev, addr);
        if (ret) {
            cts_err("Erase sector 0x%06x size 0x%04zx failed %d(%s)",
                addr, flash->sector_size, ret, cts_strerror(ret));
            return ret;
        }
        addr += flash->sector_size;
        size -= flash->sector_size;
    }

    return 0;
}

