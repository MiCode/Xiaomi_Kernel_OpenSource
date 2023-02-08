#define LOG_TAG         "Firmware"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_firmware.h"
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/namei.h>


#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#include "cts_builtin_firmware.h"
#define NUM_DRIVER_BUILTIN_FIRMWARE ARRAY_SIZE(cts_driver_builtin_firmwares)
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#define CTS_FIRMWARE_MULTI_SECTION_FILE_SIZE    (0x20000)
#define CTS_SECTION_ENABLE_FLAG                 (0x0000C35A)

enum cts_firmware_section_offset {
	CTS_FIRMWARE_SECTION_OFFSET = 0x00000000,
	CTS_FIRMWARE_CRC_SECTION_OFFSET = 0x17000,
	CTS_DDIPARAM_SECTION_OFFSET = 0x00019000,
	CTS_DDIPARAM_CRC_SECTION_OFFSET = 0x1B000,

};

struct cts_firmware_sect_info {
	const u8 *firmware_sect;
	size_t firmware_sect_size;
	const u8 *firmware_crc_sect;
	size_t firmware_crc_sect_size;
	u32 firmware_sect_crc;

	const u8 *ddiparam_sect;
	size_t ddiparam_sect_size;
	const u8 *ddiparam_crc_sect;
	size_t ddiparam_crc_sect_size;
	u32 ddiparam_sect_crc;

};

#define FIRMWARE_SECTION(firmware) \
	((firmware)->data)
#define FIRMWARE_CRC_SECTION(firmware) \
	((firmware)->data + CTS_FIRMWARE_CRC_SECTION_OFFSET)
#define DDIPARAM_SECTION(firmware) \
	((firmware)->data + CTS_DDIPARAM_SECTION_OFFSET)
#define DDIPARAM_CRC_SECTION(firmware) \
	((firmware)->data + CTS_DDIPARAM_CRC_SECTION_OFFSET)

#define FIRMWARE_SECTION_CRC(firmware) \
	(get_unaligned_le32(FIRMWARE_CRC_SECTION(firmware)))
#define FIRMWARE_SECTION_SIZE(firmware) \
	(get_unaligned_le32(FIRMWARE_CRC_SECTION(firmware) + 4))
#define FIRMWARE_SECTION_CRC_ENABLE(firmware) \
	(get_unaligned_le32(FIRMWARE_CRC_SECTION(firmware) + 8))
#define FIRMWARE_CRC_SECTION_SIZE   (12)

#define DDIPARAM_SECTION_ENABLE(firmware) \
	(get_unaligned_le32(DDIPARAM_CRC_SECTION(firmware)))
#define DDIPARAM_SECTION_CRC_ENABLE(firmware) \
	(get_unaligned_le32(DDIPARAM_CRC_SECTION(firmware) + 4))
#define DDIPARAM_SECTION_CRC(firmware) \
	(get_unaligned_le32(DDIPARAM_CRC_SECTION(firmware) + 8))
#define DDIPARAM_SECTION_SIZE(firmware) \
	(get_unaligned_le32(DDIPARAM_CRC_SECTION(firmware) + 12))
#define DDIPARAM_CRC_SECTION_SIZE   (17)

u16 cts_crc16(const u8 *data, size_t len)
{
	u16 crc = 0;

	const static u16 crc16_table[] = {
		0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
		0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
		0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
		0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
		0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
		0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
		0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
		0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
		0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
		0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
		0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
		0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
		0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
		0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
		0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
		0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
		0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
		0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
		0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
		0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
		0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
		0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
		0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
		0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
		0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
		0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
		0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
		0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
		0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
		0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
		0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
		0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202,
	};

	while (len) {
		crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data) & 0xFF];
		data++;
		len--;
	}

	return crc;
}

u32 cts_crc32(const u8 *data, size_t len)
{
	const static u32 crc32_table[] = {
		0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC,
		    0x17C56B6B,
		0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6,
		    0x2B4BCB61,
		0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD, 0x4C11DB70,
		    0x48D0C6C7,
		0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2,
		    0x52568B75,
		0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014,
		    0x7DDC5DA3,
		0x709F7B7A, 0x745E66CD, 0x9823B6E0, 0x9CE2AB57, 0x91A18D8E,
		    0x95609039,
		0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58,
		    0xBAEA46EF,
		0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA,
		    0xA06C0B5D,
		0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C,
		    0xC3F706FB,
		0xCEB42022, 0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46,
		    0xFF79A6F1,
		0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D, 0x34867077,
		    0x30476DC0,
		0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5,
		    0x2AC12072,
		0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13,
		    0x054BF6A4,
		0x0808D07D, 0x0CC9CDCA, 0x7897AB07, 0x7C56B6B0, 0x71159069,
		    0x75D48DDE,
		0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF,
		    0x5A5E5B08,
		0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D,
		    0x40D816BA,
		0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B,
		    0xBB60ADFC,
		0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698, 0x832F1041,
		    0x87EE0DF6,
		0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A, 0xE0B41DE7,
		    0xE4750050,
		0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055,
		    0xFEF34DE2,
		0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683,
		    0xD1799B34,
		0xDC3ABDED, 0xD8FBA05A, 0x690CE0EE, 0x6DCDFD59, 0x608EDB80,
		    0x644FC637,
		0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 0x4F040D56,
		    0x4BC510E1,
		0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4,
		    0x51435D53,
		0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42,
		    0x32D850F5,
		0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48,
		    0x0E56F0FF,
		0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623, 0xF12F560E,
		    0xF5EE4BB9,
		0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC,
		    0xEF68060B,
		0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A,
		    0xC0E2D0DD,
		0xCDA1F604, 0xC960EBB3, 0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610,
		    0xB07DABA7,
		0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6,
		    0x9FF77D71,
		0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74,
		    0x857130C3,
		0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645,
		    0x4A4FFBF2,
		0x470CDD2B, 0x43CDC09C, 0x7B827D21, 0x7F436096, 0x7200464F,
		    0x76C15BF8,
		0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24, 0x119B4BE9,
		    0x155A565E,
		0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B,
		    0x0FDC1BEC,
		0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D,
		    0x2056CD3A,
		0x2D15EBE3, 0x29D4F654, 0xC5A92679, 0xC1683BCE, 0xCC2B1D17,
		    0xC8EA00A0,
		0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1,
		    0xE760D676,
		0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673,
		    0xFDE69BC4,
		0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5,
		    0x9E7D9662,
		0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF,
		    0xA2F33668,
		0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
	};

	u32 crc = 0;

	while (len) {
		crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data) & 0xFF];

		data++;
		len--;
	}

	return crc;
}

static int calc_crc_in_flash(const struct cts_device *cts_dev,
			     u32 flash_addr, size_t size, u32 *crc)
{
	return cts_dev->hwdata->sfctrl->ops->calc_flash_crc(cts_dev,
							    flash_addr, size,
							    crc);
}

static bool is_multi_section_firmware(const struct cts_firmware *firmware)
{
	return (firmware->size == CTS_FIRMWARE_MULTI_SECTION_FILE_SIZE);
}

static bool is_single_section_firmware(const struct cts_firmware *firmware)
{
	return !(is_multi_section_firmware(firmware));
}

static bool is_firmware_size_valid(const struct cts_firmware *firmware)
{
	return (firmware->size > 0x102 &&
		firmware->size <= CTS_FIRMWARE_MULTI_SECTION_FILE_SIZE);
}

static bool is_multi_section_firmware_valid(const struct cts_firmware *firmware)
{
	u32 crc;

	crc =
	    cts_crc32(FIRMWARE_SECTION(firmware),
		      FIRMWARE_SECTION_SIZE(firmware));
	if (crc != FIRMWARE_SECTION_CRC(firmware)) {
		cts_err("Firmware-section crc mismatch crc-section %08x != %08x"
			", File maybe broken!!!", crc,
			FIRMWARE_SECTION_CRC(firmware));
		return false;
	}

	if (DDIPARAM_SECTION_ENABLE(firmware) == CTS_SECTION_ENABLE_FLAG) {
		crc = cts_crc32(DDIPARAM_SECTION(firmware),
				DDIPARAM_SECTION_SIZE(firmware));
		if (crc != DDIPARAM_SECTION_CRC(firmware)) {
			cts_err
			    ("DDIParam-section crc mismatch crc-section %08x != %08x"
			     ", File maybe broken!!!", crc,
			     DDIPARAM_SECTION_CRC(firmware));
			return false;
		}
	} else
		cts_info("DDIParam-section is NOT enabled");

	return true;
}

static bool is_firmware_valid(const struct cts_firmware *firmware)
{
	if (firmware && firmware->data && is_firmware_size_valid(firmware)) {
		if (is_single_section_firmware(firmware) ||
		    is_multi_section_firmware_valid(firmware))
			return true;
	}

	return false;
}

static void parse_single_section_firmware(const struct cts_firmware *firmware,
					  struct cts_firmware_sect_info *info)
{
	static u8 crc_sect[12];

	info->firmware_sect = firmware->data;
	info->firmware_sect_size = firmware->size;
	info->firmware_sect_crc = cts_crc32(firmware->data, firmware->size);

	put_unaligned_le32(info->firmware_sect_crc, crc_sect);
	put_unaligned_le32(info->firmware_sect_size, crc_sect + 4);
	put_unaligned_le32(~0x0000C35A, crc_sect + 8);	/* Enable CRC check */
	info->firmware_crc_sect = crc_sect;
	info->firmware_crc_sect_size = sizeof(crc_sect);
}

static void parse_multi_section_firmware(const struct cts_firmware *firmware,
					 struct cts_firmware_sect_info *info)
{
	info->firmware_sect = FIRMWARE_SECTION(firmware);
	info->firmware_sect_size = FIRMWARE_SECTION_SIZE(firmware);
	info->firmware_crc_sect = FIRMWARE_CRC_SECTION(firmware);
	info->firmware_crc_sect_size = FIRMWARE_CRC_SECTION_SIZE;
	info->firmware_sect_crc = FIRMWARE_SECTION_CRC(firmware);

	if (DDIPARAM_SECTION_ENABLE(firmware) == CTS_SECTION_ENABLE_FLAG) {
		info->ddiparam_sect = DDIPARAM_SECTION(firmware);
		info->ddiparam_sect_size = DDIPARAM_SECTION_SIZE(firmware);
		info->ddiparam_crc_sect = DDIPARAM_CRC_SECTION(firmware);
		info->ddiparam_crc_sect_size = DDIPARAM_CRC_SECTION_SIZE;
		info->ddiparam_sect_crc = DDIPARAM_SECTION_CRC(firmware);
	}
}

static int parse_firmware(const struct cts_firmware *firmware,
			  struct cts_firmware_sect_info *info)
{
	memset(info, 0, sizeof(*info));

	if (is_multi_section_firmware(firmware))
		parse_multi_section_firmware(firmware, info);
	else
		parse_single_section_firmware(firmware, info);

	cts_info("  Firmware section size: %zu", info->firmware_sect_size);
	if (info->ddiparam_crc_sect)
		cts_info("  DDIParam section size: %zu",
			 info->ddiparam_sect_size);

	return 0;
}

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#ifdef CONFIG_CTS_SYSFS
int cts_get_num_driver_builtin_firmware(void)
{
	return NUM_DRIVER_BUILTIN_FIRMWARE;
}

const struct cts_firmware *cts_request_driver_builtin_firmware_by_name(const char *name)
{
	const struct cts_firmware *firmware;
	int i;

	cts_info("Request driver builtin by name '%s'", name);

	firmware = cts_driver_builtin_firmwares;
	for (i = 0; i < NUM_DRIVER_BUILTIN_FIRMWARE; i++, firmware++) {
		if (strcmp(firmware->name, name) == 0) {
			if (is_firmware_valid(firmware)) {
				cts_info("Found driver builtin '%s' "
					 "hwid: %06x fwid: %04x size: %zu ver: %04x",
					 firmware->name, firmware->hwid,
					 firmware->fwid, firmware->size,
					 FIRMWARE_VERSION(firmware));
				return firmware;
			}

			cts_warn("Found driver builtin '%s' "
				 "hwid: %06x fwid: %04x size: %zu invalid",
				 firmware->name, firmware->hwid, firmware->hwid,
				 firmware->size);
		}
	}

	return NULL;
}

const struct cts_firmware *cts_request_driver_builtin_firmware_by_index(
	u32 index)
{
	const struct cts_firmware *firmware;

	cts_info("Request driver builtin by index %u", index);

	if (index < NUM_DRIVER_BUILTIN_FIRMWARE) {
		firmware = cts_driver_builtin_firmwares + index;
		if (is_firmware_valid(firmware)) {
			cts_info("Found driver builtin '%s' "
				 "hwid: %06x fwid: %04x size: %zu ver: %04x",
				 firmware->name, firmware->hwid, firmware->fwid,
				 firmware->size, FIRMWARE_VERSION(firmware));
			return firmware;
		}
		cts_warn("Found driver builtin '%s' "
			 "hwid: %06x fwid: %04x size: %zu INVALID",
			 firmware->name, firmware->hwid, firmware->hwid,
			 firmware->size);
	} else
		cts_warn("Request driver builtin by index %u too large >= %zu",
			 index, NUM_DRIVER_BUILTIN_FIRMWARE);

	return NULL;
}
#endif /* CONFIG_CTS_SYSFS */

static const struct cts_firmware *cts_request_newer_driver_builtin_firmware(
	u32 hwid, u16 fwid, u16 device_fw_ver)
{
#define MATCH_HWID(firmware, hwid) \
	((hwid) == CTS_DEV_HWID_ANY || (firmware)->hwid == (hwid))
#define MATCH_FWID(firmware, fwid) \
	((fwid) == CTS_DEV_FWID_ANY || (firmware)->fwid == (fwid))

	const struct cts_firmware *firmware = NULL;
	int i;

	cts_info
	    ("Request driver builtin if match hwid: %06x fwid: %04x && ver > %04x",
	     hwid, fwid, device_fw_ver);

	firmware = cts_driver_builtin_firmwares;
	for (i = 0; i < ARRAY_SIZE(cts_driver_builtin_firmwares);
	     i++, firmware++) {
		if (MATCH_HWID(firmware, hwid) && MATCH_FWID(firmware, fwid)) {
			if (!is_firmware_valid(firmware)) {
				cts_err("Found driver builtin '%s' "
					"hwid: %06x fwid: %04x data: %p size: %zu INVALID",
					firmware->name, firmware->hwid,
					firmware->fwid, firmware->data,
					firmware->size);
				continue;
			}

			cts_info("Found matched driver builtin '%s' "
				 "hwid: %06x fwid: %04x size: %zu ver: %04x",
				 firmware->name, firmware->hwid, firmware->fwid,
				 firmware->size, FIRMWARE_VERSION(firmware));

			/* if(FIRMWARE_VERSION(firmware) > device_fw_ver) { */
			if (1) {
				cts_info("Found newer driver builtin '%s' "
					 "hwid: %06x fwid: %04x size: %zu ver: %04x > %04x",
					 firmware->name, firmware->hwid,
					 firmware->fwid, firmware->size,
					 FIRMWARE_VERSION(firmware),
					 device_fw_ver);
				return firmware;
			}
		}
	}

	cts_info("No newer driver builtin found");

	return NULL;

#undef MATCH_HWID
#undef MATCH_FWID
}
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
bool is_filesystem_mounted(const char *filepath)
{
#ifdef CFG_CTS_FOR_GKI
	cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
	return false;
#else
	struct path root_path;
	struct path path;
	int ret;

	ret = kern_path("/", LOOKUP_FOLLOW, &root_path);
	if (ret)
		return false;

	ret = kern_path(filepath, LOOKUP_FOLLOW, &path);
	if (ret)
		goto err_put_root_path;

	if (path.mnt->mnt_sb == root_path.mnt->mnt_sb)
		/* not mounted */
		ret = false;
	else
		ret = true;

	path_put(&path);
err_put_root_path:
	path_put(&root_path);

	return !!ret;
#endif
}

static int cts_request_firmware_full_filepath(struct cts_firmware *firmware,
	const char *filepath, int curr_version)
{
#ifdef CFG_CTS_FOR_GKI
	cts_info("%s(): some functions are forbiddon with GKI Version! Please use request_firmware!", __func__);
	return -EPERM;
#else
	struct file *file;
	u16 version;
	u8 buff[2];
	loff_t pos = 0;
	int read_size;
	int ret = 0;

	file = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(file)) {
		cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
		return PTR_ERR(file);
	}

	firmware->size = file_inode(file)->i_size;
	if (!is_firmware_size_valid(firmware)) {
		cts_info("File '%s' size: %zu invalid", filepath,
			 firmware->size);
		ret = -EINVAL;
		goto err_close_file;
	}

	pos = FIRMWARE_VERSION_OFFSET;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	read_size = kernel_read(file, buff, 2, &pos);
#else
	read_size = kernel_read(file, pos, buff, 2);
#endif
	if (read_size < 0) {
		cts_err("Read version from offset 0x100 failed");
		ret = -EIO;
		goto err_close_file;
	}
	version = get_unaligned_le16(buff);

	if (version <= curr_version) {
		cts_info("File '%s' size: %zu version: %04x <= %04x",
			 filepath, firmware->size, version, curr_version);
		ret = -EINVAL;
		goto err_close_file;
	}

	cts_info("File '%s' size: %zu version: %04x",
		 filepath, firmware->size, version);

	firmware->data = (u8 *) vmalloc(firmware->size);
	if (firmware->data == NULL) {
		cts_err("Request form fs alloc firmware data failed");
		ret = -ENOMEM;
		goto err_close_file;
	}

	pos = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	read_size = kernel_read(file, firmware->data, firmware->size, &pos);
#else
	read_size = kernel_read(file, pos, firmware->data, firmware->size);
#endif
	if (read_size < 0 || read_size != firmware->size) {
		cts_err("Request from fs read whole file failed %d", read_size);
		ret = -EIO;
		goto err_free_firmware_data;
	}

	{
		int r = filp_close(file, NULL);

		if (r)
			cts_warn("Close file '%s' failed %d", filepath, r);
	}

	return 0;

err_free_firmware_data:
	vfree(firmware->data);
	firmware->data = NULL;

err_close_file:{
		int r = filp_close(file, NULL);

		if (r)
			cts_warn("Close file '%s' failed %d", filepath, r);
	}

	return ret;
#endif
}

static int cts_wrap_request_firmware(struct cts_firmware *firmware,
	const char *name, struct device *device, int curr_version)
{
	int ret;

	ret = request_firmware(&firmware->fw, name, device);
	if (ret) {
		cts_err("Could not load firmware from %s: %d", name, ret);
		return ret;
	}

	/* Map firmware structure to cts_firmware */
	firmware->data = (u8 *) firmware->fw->data;
	firmware->size = firmware->fw->size;

	/* Check firmware */
	if (!is_firmware_valid(firmware)) {
		cts_err("Request firmware file INVALID, size %zu",
			firmware->size);
		ret = -EINVAL;
		goto err_release_firmware;
	}

	/* Check firmware version */
	if (curr_version > 0 && curr_version >= FIRMWARE_VERSION(firmware)) {
		cts_info("Request firmware file with version 0x%04x < 0x%04x",
			 FIRMWARE_VERSION(firmware), curr_version);
		ret = -EINVAL;
		goto err_release_firmware;
	}

	return 0;

err_release_firmware:
	release_firmware(firmware->fw);

	return ret;
}

const struct cts_firmware *cts_request_newer_firmware_from_fs(
	const struct cts_device *cts_dev, const char *filepath, u16 curr_version)
{
	struct cts_firmware *firmware = NULL;
	int ret = 0;

	cts_info("Request from file '%s' if version > %04x",
		 filepath, curr_version);

	if (filepath == NULL || filepath[0] == '\0') {
		cts_err("Request from file path INVALID %p", filepath);
		return NULL;
	}

	firmware = (struct cts_firmware *)kzalloc(sizeof(*firmware), GFP_KERNEL);
	if (firmware == NULL) {
		cts_err("Request from file alloc struct cts_firmware failed");
		return NULL;
	}

	if (strchr(filepath, '/') != NULL) {	/* Full path */
		cts_info("Filepath is fullpath, direct read it out");
		ret = cts_request_firmware_full_filepath(firmware, filepath, curr_version);
	} else {		/* ONLY file name is givin */
		cts_info("Filepath is only filename, use request_firmware()");
		ret = cts_wrap_request_firmware(firmware, filepath,
			&cts_dev->pdata->ts_input_dev->dev, curr_version);
	}

	if (ret) {
		cts_err("Request from file '%s' failed %d", filepath, ret);
		kfree(firmware);
		return NULL;
	} else {
		return firmware;
	}
}

const struct cts_firmware *cts_request_firmware_from_fs(
	const struct cts_device	*cts_dev, const char *filepath)
{
	cts_info("Request from file '%s'", filepath);

	return cts_request_newer_firmware_from_fs(cts_dev, filepath, 0);
}

int cts_update_firmware_from_file(struct cts_device *cts_dev,
				  const char *filepath, bool to_flash)
{
	const struct cts_firmware *firmware;
	int ret;

	cts_info("Update from file '%s' to %s", filepath,
		 to_flash ? "flash" : "sram");

	firmware = cts_request_firmware_from_fs(cts_dev, filepath);
	if (firmware == NULL) {
		cts_err("Request from file '%s' failed", filepath);
		return -EFAULT;
	}

	ret = cts_update_firmware(cts_dev, firmware, to_flash);
	if (ret) {
		cts_err("Update to %s from file failed %d",
			to_flash ? "flash" : "sram", ret);
		goto err_release_firmware;
	}

	cts_info("Update from file success");

err_release_firmware:
	cts_release_firmware(firmware);

	return ret;
}
#endif /*CFG_CTS_FIRMWARE_IN_FS */

const struct cts_firmware *cts_request_firmware(const struct cts_device *cts_dev,
	u32 hwid, u16 fwid, u16 curr_firmware_ver)
{
	const struct cts_firmware *firmware_builtin = NULL;
	const struct cts_firmware *firmware_from_file = NULL;

	if (hwid == CTS_DEV_HWID_INVALID)
		hwid = CTS_DEV_HWID_ANY;
	if (fwid == CTS_DEV_FWID_INVALID)
		fwid = CTS_DEV_FWID_ANY;

	cts_info("Request newer if match hwid: %06x fwid: %04x && ver > %04x",
		 hwid, fwid, curr_firmware_ver);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
	firmware_builtin = cts_request_newer_driver_builtin_firmware(
		hwid, fwid,curr_firmware_ver);
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
#ifdef CFG_CTS_FOR_GKI
	/* Check firmware in file system when probe only when build to .ko */
	if (hwid == CTS_DEV_HWID_ICNL9911C) {
		firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
			cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9911,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			CFG_CTS_FIRMWARE_FILENAME_9911,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
	} else if (hwid == CTS_DEV_HWID_ICNL9916) {
		firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
			cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9916,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			CFG_CTS_FIRMWARE_FILENAME_9916,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
	} else if (hwid == CTS_DEV_HWID_ICNL9916C) {
		firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
			cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9916C,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			CFG_CTS_FIRMWARE_FILENAME_9916C,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
	} else if (hwid == CTS_DEV_HWID_ICNL9922) {
		firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
			cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9922,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			CFG_CTS_FIRMWARE_FILENAME_9922,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
			firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
	}
#else
	/* Check firmware in file system when probe only when build to .ko */
	if (hwid == CTS_DEV_HWID_ICNL9911C) {
		if (is_filesystem_mounted(CFG_CTS_FIRMWARE_FILEPATH_9911)) {
			firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
				cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9911,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				CFG_CTS_FIRMWARE_FILENAME_9911,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
		}
	} else if (hwid == CTS_DEV_HWID_ICNL9916) {
		if (is_filesystem_mounted(CFG_CTS_FIRMWARE_FILEPATH_9916)) {
			firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
				cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9916,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				CFG_CTS_FIRMWARE_FILENAME_9916,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
		}
	} else if (hwid == CTS_DEV_HWID_ICNL9916C) {
		if (is_filesystem_mounted(CFG_CTS_FIRMWARE_FILEPATH_9916C)) {
			firmware_from_file =
			    cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
				cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9916C,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				CFG_CTS_FIRMWARE_FILENAME_9916C,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				firmware_builtin ?  FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
		}
	} else if (hwid == CTS_DEV_HWID_ICNL9922) {
		if (is_filesystem_mounted(CFG_CTS_FIRMWARE_FILEPATH_9922)) {
			firmware_from_file = cts_request_newer_firmware_from_fs(cts_dev,
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
				cts_dev->config_fw_name[0] ? cts_dev->config_fw_name : CFG_CTS_FIRMWARE_FILENAME_9922,
#else /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				CFG_CTS_FIRMWARE_FILENAME_9922,
#endif /* CFG_CTS_FW_UPDATE_FILE_LOAD */
				firmware_builtin ? FIRMWARE_VERSION(firmware_builtin) : curr_firmware_ver);
		}
	}
#endif /* CFG_CTS_FOR_GKI */
#endif /* CFG_CTS_FIRMWARE_IN_FS */

	return firmware_from_file ? firmware_from_file : firmware_builtin;
}

void cts_release_firmware(const struct cts_firmware *firmware)
{
	cts_info("Release firmware %p", firmware);

	if (firmware) {
		if (firmware->fw) {
			/* Use request_firmware() get struct firmware * */
			cts_info("Release firmware from request_firmware");
			release_firmware(firmware->fw);
			kfree(firmware);
		} else if (firmware->name == NULL) {
			/* Direct read from file */
			cts_info("Release firmware from direct-load");
			vfree(firmware->data);
			kfree(firmware);
		} else
			/* Builtin firmware with non-NULL name, no need to free */
			cts_info("Release firmware from driver built-in");
	}
}

static int validate_flash_data(const struct cts_device *cts_dev,
	u32 flash_addr, const u8 *data, size_t size, u8 *buf, bool calc_crc, u32 crc)
{
	int ret, i;
	bool free_data = false;
	u32 crc_flash;

	cts_info("Validate flash data from 0x%06x size %zu by %s",
		 flash_addr, size, calc_crc ? "check-crc" : "direct-readback");

	if (calc_crc) {
		ret = calc_crc_in_flash(cts_dev, flash_addr, size, &crc_flash);
		if (ret) {
			cts_err("Calc data in flash from 0x%06x size %zu crc failed %d, "
				"try to validate by direct readback", flash_addr, size, ret);
			/* FALL through by direct compare data read back */
		} else {
			if (crc_flash != crc) {
				cts_err("Crc in flash from 0x%06x size %zu mismatch 0x%08x != 0x%08x",
					flash_addr, size, crc_flash, crc);
				/* FALL through by direct compare data read back */
			} else {
				cts_info("Flash data crc correct");
				return 0;
			}
		}
	}

	if (buf == NULL) {
		buf = (u8 *) kmalloc(size, GFP_KERNEL);
		if (buf == NULL) {
			cts_err("Validate flash data allocate mem failed");
			return -ENOMEM;
		}

		free_data = true;
	}

	ret = cts_read_flash(cts_dev, flash_addr, buf, size);
	if (ret) {
		cts_err("Read flash from 0x%06x size %zu failed %d", flash_addr, size, ret);
		goto err_free_buf;
	}

	for (i = 0; i < size; i++) {
		if (buf[i] != data[i]) {
			if (ret == 0) {
				cts_err("Flash data from 0x%06x size %zu first bytes diff:", flash_addr, size);
			}

			if (ret < 100) {
				cts_err("  0x%06x: %02x %02x", i, buf[i], data[i]);
			} else if (ret == 100) {
				cts_err("  ...");
			}
			ret++;
		}
	}

err_free_buf:
	if (free_data)
		kfree(buf);

	return ret;
}

static int cts_program_firmware(const struct cts_device *cts_dev,
	const struct cts_firmware_sect_info *firmware_info)
{
	int ret;
	u8 crc_sect_buf[FIRMWARE_CRC_SECTION_SIZE];

	cts_info("Program firmware size %zu",
		 firmware_info->firmware_sect_size);

	/* Write firmware to SRAM for easy enter normal mode after update */
	cts_info("Write firmware section to sram");
	ret = cts_sram_writesb_check_crc_retry(cts_dev, 0,
					       firmware_info->firmware_sect,
					       firmware_info->firmware_sect_size,
					       firmware_info->firmware_sect_crc,
					       3);
	if (ret) {
		cts_err("Write firmware section to sram failed %d", ret);
		return ret;
	}

	ret = cts_program_flash_from_sram(cts_dev,
					  4, 4,
					  firmware_info->firmware_sect_size -
					  4);
	if (ret) {
		cts_err("Program firmware section from sram failed %d", ret);
		return ret;
	}

	ret = cts_program_flash(cts_dev,
				CTS_FIRMWARE_CRC_SECTION_OFFSET,
				firmware_info->firmware_crc_sect,
				firmware_info->firmware_crc_sect_size);
	if (ret) {
		cts_err("Program firmware crc section failed %d", ret);
		return ret;
	}

	ret = validate_flash_data(cts_dev,
				  CTS_FIRMWARE_CRC_SECTION_OFFSET,
				  firmware_info->firmware_crc_sect,
				  firmware_info->firmware_crc_sect_size,
				  crc_sect_buf, false, 0);
	if (ret) {
		cts_err("Validate Firmware-CRC section failed %d", ret);
		return ret;
	}

	ret = cts_program_flash(cts_dev, 0, firmware_info->firmware_sect, 4);
	if (ret) {
		cts_err("Program firmware section fist 4bytes failed %d", ret);
		return ret;
	}

	ret = validate_flash_data(cts_dev,
				  CTS_FIRMWARE_SECTION_OFFSET,
				  firmware_info->firmware_sect,
				  firmware_info->firmware_sect_size, NULL, true,
				  firmware_info->firmware_sect_crc);
	if (ret) {
		cts_err("Validate firmware section failed %d", ret);
		return ret;
	}

	return 0;
}

static int cts_program_ddiparam(const struct cts_device *cts_dev,
				const struct cts_firmware_sect_info
				*firmware_info)
{
	int ret;
	u8 crc_sect_buf[DDIPARAM_CRC_SECTION_SIZE];

	cts_info("Program DDIParam size: %zu",
		 firmware_info->ddiparam_sect_size);

	ret = cts_program_flash(cts_dev, CTS_DDIPARAM_SECTION_OFFSET,
				firmware_info->ddiparam_sect,
				firmware_info->ddiparam_sect_size);
	if (ret) {
		cts_err("Program DDIParam section failed %d", ret);
		return ret;
	}

	ret = validate_flash_data(cts_dev,
				  CTS_DDIPARAM_SECTION_OFFSET,
				  firmware_info->ddiparam_sect,
				  firmware_info->ddiparam_sect_size, NULL, true,
				  firmware_info->ddiparam_sect_crc);
	if (ret) {
		cts_err("Validate DDIParam section failed %d", ret);
		return ret;
	}

	ret = cts_program_flash(cts_dev, CTS_DDIPARAM_CRC_SECTION_OFFSET,
				firmware_info->ddiparam_crc_sect,
				firmware_info->ddiparam_crc_sect_size);
	if (ret) {
		cts_err("Program DDIParam-CRC section failed %d", ret);
		return ret;
	}

	ret = validate_flash_data(cts_dev,
				  CTS_DDIPARAM_CRC_SECTION_OFFSET,
				  firmware_info->ddiparam_crc_sect,
				  firmware_info->ddiparam_crc_sect_size,
				  crc_sect_buf, false, 0);
	if (ret) {
		cts_err("Validate DDIParam-CRC section failed %d", ret);
		return ret;
	}

	return 0;
}

int cts_update_firmware(struct cts_device *cts_dev,
			const struct cts_firmware *firmware, bool to_flash)
{
	struct cts_firmware_sect_info firmware_info;
	int ret, retries;
	
#ifdef CONFIG_CTS_I2C_HOST
	to_flash = true;
#else
	to_flash = false;
#endif

	cts_info("Update firmware to %s ver: %04x size: %zu",
		 to_flash ? "flash" : "sram",
		 FIRMWARE_VERSION(firmware), firmware->size);

	if (parse_firmware(firmware, &firmware_info)) {
		cts_err("Parse firmware failed");
		return -EINVAL;
	}

	cts_dev->rtdata.updating = true;

	ret = cts_enter_program_mode(cts_dev);
	if (ret) {
		cts_err("Device enter program mode failed %d", ret);
		goto out;
	}

	if (!to_flash || !cts_dev->rtdata.has_flash) {
		cts_info("Write firmware section to sram size %zu",
			 firmware_info.firmware_sect_size);
		ret = cts_sram_writesb_check_crc_retry(cts_dev, 0,
			firmware_info.firmware_sect, firmware_info.firmware_sect_size,
			firmware_info.firmware_sect_crc, 3);
		if (ret)
			cts_err("Write firmware section to sram failed %d", ret);
#ifdef CFG_CTS_UPDATE_CRCCHECK
		if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911S ||
		    cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911C ||
		    cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916 ||
		    cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916C ||
		    cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9922)
			cts_sram_writesb_boot_crc_retry(cts_dev,
				firmware_info.firmware_sect_size,
				firmware_info.firmware_sect_crc, 3);
#endif
		goto out;
	}

	ret = cts_prepare_flash_operation(cts_dev);
	if (ret)
		cts_warn("Prepare flash operation failed %d", ret);
		/* Go through and try */

	retries = 0;
	do {
		retries++;

		ret = cts_erase_flash(cts_dev, CTS_FIRMWARE_CRC_SECTION_OFFSET,
			firmware_info.firmware_crc_sect_size);
		if (ret) {
			cts_err("Erase firmware crc section failed %d retries %d",
				ret, retries);
			continue;
		}

		ret = cts_erase_flash(cts_dev, 0, firmware_info.firmware_sect_size);
		if (ret) {
			cts_err("Erase firmware section failed %d retries %d",
				ret, retries);
			continue;
		}

		ret = cts_program_firmware(cts_dev, &firmware_info);
		if (ret) {
			cts_err
			    ("Program firmware & crc section failed %d retries %d",
			     ret, retries);
		}
	} while (ret && retries < 3);

	if (ret == 0 && firmware_info.ddiparam_sect_size != 0) {
		retries = 0;
		do {
			retries++;

			ret = cts_erase_flash(cts_dev, CTS_DDIPARAM_CRC_SECTION_OFFSET,
				firmware_info.ddiparam_crc_sect_size);
			if (ret) {
				cts_err("Erase DDIParam crc secction failed %d, retries %d",
					ret, retries);
				continue;
			}

			ret = cts_erase_flash(cts_dev, CTS_DDIPARAM_SECTION_OFFSET,
				firmware_info.ddiparam_sect_size);
			if (ret) {
				cts_err("Erase DDIParam section failed %d, retries %d",
					ret, retries);
				continue;
			}

			ret = cts_program_ddiparam(cts_dev, &firmware_info);
			if (ret) {
				cts_err("Program DDIParam & crc section failed %d retries %d",
					ret, retries);
			}
		} while (ret && retries < 3);
	}

	cts_post_flash_operation(cts_dev);

out:
	cts_dev->rtdata.updating = false;

	if (ret == 0) {
		if (firmware_info.firmware_sect_size <=
		    cts_dev->hwdata->sfctrl->xchg_sram_base) {
			ret = cts_enter_normal_mode(cts_dev);
			if (ret) {
				cts_err("Enter normal mode failed %d", ret);
				/* return ret; */
			}
		}
	}

#ifdef CONFIG_CTS_CHARGER_DETECT
	if (cts_is_charger_exist(cts_dev)) {
		int r = cts_set_dev_charger_attached(cts_dev, true);

		if (r)
			cts_err("Set dev charger attached failed %d", r);
	}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
	if (cts_is_earjack_exist(cts_dev)) {
		int r = cts_set_dev_earjack_attached(cts_dev, true);
		if (r)
			cts_err("Set dev earjack attached failed %d", r);
	}
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_GLOVE
	if (cts_is_glove_enabled(cts_dev))
		cts_enter_glove_mode(cts_dev);
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
	if (cts_is_fw_log_redirect(cts_dev))
		cts_enable_fw_log_redirect(cts_dev);
#endif

	return ret;
}

bool cts_is_firmware_updating(const struct cts_device *cts_dev)
{
	return cts_dev->rtdata.updating;
}
