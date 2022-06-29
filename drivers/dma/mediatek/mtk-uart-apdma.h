/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Mediatek Inc.
 */

#ifndef MTK_UART_APDMA_H
#define MTK_UART_APDMA_H

#define KERNEL_mtk_save_uart_apdma_reg    mtk_save_uart_apdma_reg
#define KERNEL_mtk_uart_apdma_data_dump   mtk_uart_apdma_data_dump

void mtk_save_uart_apdma_reg(struct dma_chan *chan, unsigned int *reg_buf);
void mtk_uart_apdma_data_dump(struct dma_chan *chan);


#endif /* MTK_UART_APDMA_H */
