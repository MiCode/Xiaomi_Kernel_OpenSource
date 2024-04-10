#ifndef CTS_FIRMWARE_H
#define CTS_FIRMWARE_H

#include "cts_config.h"
#include <linux/firmware.h>
#include <linux/vmalloc.h>

struct cts_firmware {
    const char *name;    /* MUST set to non-NULL if driver builtin firmware */
    u32 hwid;
    u16 fwid;

    u8 *data;
    size_t size;
    const struct firmware *fw;
};

#define FIRMWARE_VERSION_OFFSET     0x100
#define FIRMWARE_VERSION(firmware)  \
    get_unaligned_le16((firmware)->data + FIRMWARE_VERSION_OFFSET)

struct cts_device;

extern const struct cts_firmware *cts_request_firmware(
    const struct cts_device *cts_dev, u32 hwid, u16 fwid, u16 device_fw_ver);
extern void cts_release_firmware(const struct cts_firmware *firmware);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
extern int cts_get_num_driver_builtin_firmware(void);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(
        const char *name);
extern const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
        u32 index);
#else /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
static inline int cts_get_num_driver_builtin_firmware(void)
{
    return 0;
}

static inline const struct cts_firmware
*cts_request_driver_builtin_firmware_by_name(const char *name)
{
    return NULL;
}

static inline const struct cts_firmware
*cts_request_driver_builtin_firmware_by_index(u32 index)
{
    return NULL;
}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
extern bool cts_is_firmware_updating(const struct cts_device *cts_dev);

extern int cts_update_firmware(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, bool to_flash);

#ifdef CFG_CTS_FIRMWARE_IN_FS
extern const struct cts_firmware *cts_request_firmware_from_fs(
    const struct cts_device *cts_dev, const char *filepath);
extern int cts_update_firmware_from_file(struct cts_device *cts_dev,
    const char *filepath, bool to_flash);

#endif /* CFG_CTS_FIRMWARE_IN_FS */

extern u16 cts_crc16(const u8 *data, size_t len);
extern u32 cts_crc32(const u8 *data, size_t len);

#endif /* CTS_FIRMWARE_H */
