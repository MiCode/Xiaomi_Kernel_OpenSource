#ifndef __UAPI_WCD_SPI_AC_PARAMS_H__
#define __UAPI_WCD_SPI_AC_PARAMS_H__

#include <linux/types.h>

#define WCD_SPI_AC_CMD_CONC_BEGIN	0x01
#define WCD_SPI_AC_CMD_CONC_END		0x02
#define WCD_SPI_AC_CMD_BUF_DATA		0x03

#define WCD_SPI_AC_MAX_BUFFERS		2
#define WCD_SPI_AC_MAX_CH_PER_BUF	8

#define WCD_SPI_AC_CLIENT_CDEV_NAME	"wcd-spi-ac-client"
#define WCD_SPI_AC_PROCFS_DIR_NAME	"wcd-spi-ac"
#define WCD_SPI_AC_PROCFS_STATE_NAME	"svc-state"

/*
 * wcd_spi_ac_buf_data:
 *	Buffer address for one buffer. Should have data
 *	for all the channels. If channels are unused, the
 *	value must be NULL.
 *
 * @addr:
 *	Address where each channel of the buffer starts.
 */
struct wcd_spi_ac_buf_data {
	__u32 addr[WCD_SPI_AC_MAX_CH_PER_BUF];
} __packed;

/*
 * wcd_spi_ac_write_cmd:
 *	Data sent to the driver's write interface should
 *	be packed in this format.
 *
 * @cmd_type:
 *	Indicates the type of command that is sent. Should
 *	be one of the valid commands defined with
 *	WCD_SPI_AC_CMD_*
 * @payload:
 *	No payload for:
 *		WCD_SPI_AC_CMD_CONC_BEGIN
 *		WCD_SPI_AC_CMD_CONC_END
 *	Upto WCD_SPI_AC_MAX_BUFFERS of type
 *	struct wcd_spi_ac_buf_data for:
 *		WCD_SPI_AC_CMD_BUF_DATA
 */
struct wcd_spi_ac_write_cmd {
	__u32 cmd_type;
	__u8 payload[0];
} __packed;

#endif /* end of __UAPI_WCD_SPI_AC_PARAMS_H__ */
