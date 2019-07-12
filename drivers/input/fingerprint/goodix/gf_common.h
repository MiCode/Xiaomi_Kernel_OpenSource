#ifndef __GF_COMMON_H__
#define __GF_COMMON_H__

#include "gf_spi.h"

/****************Function prototypes*****************/
int  gf_spi_read_bytes(struct gf_dev *gf_dev, unsigned short addr,
		unsigned short data_len, unsigned char *rx_buf);

int  gf_spi_write_bytes(struct gf_dev *gf_dev, unsigned short addr,
		unsigned short data_len, unsigned char *tx_buf);

int  gf_spi_read_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short *value);

int  gf_spi_write_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short value);

int  gf_spi_read_data(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_read_data_bigendian(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_write_data(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_send_cmd(struct gf_dev *gf_dev, unsigned char *cmd, int len);
#endif
