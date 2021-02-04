/**
 * ${ANDROID_BUILD_TOP}/vendor/focaltech/src/base/focaltech/ff_spi.h
 *
 * Copyright (C) 2014-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
**/

#ifndef __FF_SPI_H__
#define __FF_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Init ff_spi component.
 *
 * @return
 *  ff_err_t code.
 */
 int ff_spi_init(void);

/*
 * De-init ff_spi component.
 */
int ff_spi_free(void);

/*
 * Configurate the SPI speed.
 *
 * @params
 *  bps: bits per second.
 *
 * @return
 *  ff_err_t code.
 */
int ff_spi_config_speed(int bps);

/*
 * Writes data to SPI bus.
 *
 * @params
 *  tx_buf: TX data buffer to be written.
 *  tx_len: TX data length.
 *
 * @return
 *  ff_err_t code.
 */
int ff_spi_write_buf(const void *tx_buf, int tx_len);

/*
 * Writes command data to SPI bus and then reads data from SPI bus.
 *
 * @params
 *  tx_buf: TX data buffer to be written.
 *  tx_len: TX data length.
 *  rx_buf: RX data buffer to be read.
 *  rx_len: RX data length.
 *
 * @return
 *  ff_err_t code.
 */
int ff_spi_write_then_read_buf(const void *tx_buf, int tx_len, void *rx_buf, int rx_len);

#ifdef __cplusplus
}
#endif

#endif /* __FF_SPI_H__ */
