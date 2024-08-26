/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : ese_driver.h
 * Description: Source file for tms ese driver
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 *******************************************************************************************/
#ifndef _TMS_ESE_THN31_H_
#define _TMS_ESE_THN31_H_

#include "ese_common.h"

/*********** PART1: Define Area ***********/
#define ESE_DEVICE               "tms,ese"

#define ESE_MAX_BUFFER_SIZE      (4096)
#define ESE_CMD_RSP_TIMEOUT_MS   (2000)
#define ESE_MAGIC                (0xEA)
#define ESE_SET_STATE            _IOW(ESE_MAGIC, 0x01, long)
#define ESE_SPI_CLK_CONTROL      _IOW(ESE_MAGIC, 0xFF, long)  /* MTK TEE set SE SPI clock on/off */

enum ese_ioctl_request_table {
    ESE_POWER_OFF     = 11,  /* ESE power off with ven low */
    ESE_POWER_ON      = 10,  /* ESE power on with ven high */
    ESE_COS_DWNLD_OFF = 12,  /* ESE firmware download gpio low */
    ESE_COS_DWNLD_ON  = 13,  /* ESE firmware download gpio high */
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
struct mtk_spi {
    void __iomem                       *base;
    void __iomem                       *peri_regs;
    u32                                state;
    int                                pad_num;
    u32                                *pad_sel;
    struct clk                         *parent_clk, *sel_clk, *spi_clk;
    struct spi_transfer                *cur_transfer;
    u32                                xfer_len;
    struct scatterlist                 *tx_sgl, *rx_sgl;
    u32                                tx_sgl_len, rx_sgl_len;
    const struct mtk_spi_compatible    *dev_comp;
    u32                                dram_8gb_offset;
};
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(4, 14, 0))
struct mtk_spi {
    void __iomem                       *base;
    u32                                state;
    int                                pad_num;
    u32                                *pad_sel;
    struct clk                         *parent_clk, *sel_clk, *spi_clk;
    struct spi_transfer                *cur_transfer;
    u32                                xfer_len;
    u32                                num_xfered;
    struct scatterlist                 *tx_sgl, *rx_sgl;
    u32                                tx_sgl_len, rx_sgl_len;
    const struct mtk_spi_compatible    *dev_comp;
};
#else
struct mtk_spi {
    void __iomem                       *base;
    u32                                state;
    int                                pad_num;
    u32                                *pad_sel;
    struct clk                         *parent_clk, *sel_clk, *spi_clk, *spare_clk;
    struct                             spi_transfer *cur_transfer;
    u32                                xfer_len;
    u32                                num_xfered;
    struct scatterlist                 *tx_sgl, *rx_sgl;
    u32                                tx_sgl_len, rx_sgl_len;
    const struct mtk_spi_compatible    *dev_comp;
};
#endif

/*********** PART2: Struct Area ***********/

/*********** PART3: Function or variables for other files ***********/
//extern void mt_spi_enable_master_clk(struct spi_device *spidev);
//extern void mt_spi_disable_master_clk(struct spi_device *spidev);
#endif /* _TMS_ESE_THN31_H_ */