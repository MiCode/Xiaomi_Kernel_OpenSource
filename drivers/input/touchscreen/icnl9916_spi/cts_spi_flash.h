#ifndef CTS_SPI_FLASH_H
#define CTS_SPI_FLASH_H

struct cts_flash {
    const char *name;
    u32 jedec_id;        /* Device ID by command 0x9F */
    size_t page_size;    /* Page size by command 0x02 */
    size_t sector_size;    /* Sector size by command 0x20 */

/* Block size by command 0x52,
 * if 0 means block erase NOT supported
 */
    size_t block_size;
    size_t total_size;

};

#define CTS_FLASH_READ_DEFAULT_RETRY        (3)
#define CTS_FLASH_ERASE_DEFAULT_RETRY       (3)

struct cts_device;

extern int cts_prepare_flash_operation(struct cts_device *cts_dev);
extern int cts_post_flash_operation(struct cts_device *cts_dev);

extern int cts_read_flash_retry(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size, int retry);
static inline int cts_read_flash(const struct cts_device *cts_dev,
        u32 flash_addr, void *dst, size_t size)
{
    return cts_read_flash_retry(cts_dev, flash_addr, dst, size,
            CTS_FLASH_READ_DEFAULT_RETRY);
}

extern int cts_read_flash_to_sram_retry(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size, int retry);
static inline int cts_read_flash_to_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size)
{
    return cts_read_flash_to_sram_retry(cts_dev,
            flash_addr, sram_addr, size, CTS_FLASH_READ_DEFAULT_RETRY);
}

extern int cts_read_flash_to_sram_check_crc_retry(const struct cts_device
        *cts_dev, u32 flash_addr, u32 sram_addr, size_t size, u32 crc, int retry);
static inline int cts_read_flash_to_sram_check_crc(const struct cts_device
        *cts_dev, u32 flash_addr, u32 sram_addr, size_t size, u32 crc)
{
    return cts_read_flash_to_sram_check_crc_retry(cts_dev,
            flash_addr, sram_addr, size, crc, CTS_FLASH_READ_DEFAULT_RETRY);
}

extern int cts_erase_flash(const struct cts_device *cts_dev, u32 addr,
        size_t size);

extern int cts_program_flash(const struct cts_device *cts_dev,
        u32 flash_addr, const void *src, size_t size);
extern int cts_program_flash_from_sram(const struct cts_device *cts_dev,
        u32 flash_addr, u32 sram_addr, size_t size);

#endif /* CTS_SPI_FLASH_H */
