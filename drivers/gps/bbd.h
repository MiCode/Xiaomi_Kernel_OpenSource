/*
 * Copyright 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#ifndef __BBD_H__
#define __BBD_H__

#pragma pack(4)
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;

union long_union_t {
	unsigned char uc[sizeof(unsigned long)];
	unsigned long ul;
};

union short_union_t {
	unsigned char  uc[sizeof(unsigned short)];
	unsigned short us;
};
#pragma pack()


#define BBD_DEVICE_MAJOR	239
enum {
	BBD_MINOR_SHMD	    = 0,
	BBD_MINOR_SENSOR    = 1,
	BBD_MINOR_CONTROL   = 2,
	BBD_MINOR_PATCH     = 3,

	BBD_DEVICE_INDEX
};

#define BBD_MAX_DATA_SIZE	 4096  /* max data size for transition  */

#define BBD_CTRL_RESET_REQ	"BBD:RESET_REQ"
#define ESW_CTRL_READY		"ESW:READY"
#define ESW_CTRL_NOTREADY	"ESW:NOTREADY"
#define ESW_CTRL_CRASHED	"ESW:CRASHED"
#define BBD_CTRL_DEBUG_ON       "BBD:DEBUG=1"
#define BBD_CTRL_DEBUG_OFF      "BBD:DEBUG=0"
#define SSP_DEBUG_ON		"SSP:DEBUG=1"
#define SSP_DEBUG_OFF		"SSP:DEBUG=0"
#define SSI_DEBUG_ON		"SSI:DEBUG=1"
#define SSI_DEBUG_OFF		"SSI:DEBUG=0"
#define PZC_DEBUG_ON		"PZC:DEBUG=1"
#define PZC_DEBUG_OFF		"PZC:DEBUG=0"
#define RNG_DEBUG_ON		"RNG:DEBUG=1"
#define RNG_DEBUG_OFF		"RNG:DEBUG=0"
#define BBD_CTRL_SSI_PATCH_BEGIN	"SSI:PatchBegin"
#define BBD_CTRL_SSI_PATCH_END		"SSI:PatchEnd"
#define GPSD_SENSOR_ON		"GPSD:SENSOR_ON"
#define GPSD_SENSOR_OFF		"GPSD:SENSOR_OFF"


#define HSI_RNGDMA_RX_BASE_ADDR       0x40104040
#define HSI_RNGDMA_RX_SW_ADDR_OFFSET  0x40104050
#define HSI_RNGDMA_TX_BASE_ADDR       0x40104060
#define HSI_RNGDMA_TX_SW_ADDR_OFFSET  0x40104070
#define HSI_CTRL                      0x40104090
#define HSI_ADL_ABR_CONTROL           0x401040a0


/** callback for incoming data from 477x to senser hub driver **/
typedef struct {
	int (*on_packet)(void *ssh_data, const char *buf, size_t size);
	int (*on_packet_alarm)(void *ssh_data);
	int (*on_control)(void *ssh_data, const char *str_ctrl);
	int (*on_mcu_ready)(void *ssh_data, bool ready);
} bbd_callbacks;

extern void	bbd_register(void *ext_data, bbd_callbacks *pcallbacks);
extern ssize_t	bbd_send_packet(unsigned char *buf, size_t size);
extern ssize_t	bbd_pull_packet(unsigned char *buf, size_t size,
				unsigned int timeout_ms);
extern int	bbd_mcu_reset(void);
extern int	bbd_init(struct device *dev, bool legacy_patch);
extern void bbd_parse_asic_data(unsigned char *pucData,
					unsigned short usLen,
					void (*to_gpsd)(unsigned char *packet,
					unsigned short len,
					void *priv),
					void *priv);


#ifdef CONFIG_BCM_GPS_SPI_DRIVER
extern void bcm477x_debug_info(const char *buf);
#endif
#endif /* __BBD_H__ */
