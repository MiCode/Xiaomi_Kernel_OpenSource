#ifndef _MI_DMA_DIO_H
#define _MI_DMA_DIO_H

struct dma_dio_file_task;
struct dma_dio_file;

struct dma_dio_operations {
        struct dma_dio_file_task *(*dma_dio_declare_file_read)(struct dma_dio_file *dio_file);
        int (*dma_dio_gather_file_page)(struct dma_dio_file_task *dio_ftask,
                                         struct page *page);
        int (*dma_dio_end_file_read)(struct dma_dio_file_task *dio_ftask);
        size_t (*dma_dio_alloc_size)(struct dma_dio_file *dio_file);
};

#endif  /* _MI_DMA_DIO_H */