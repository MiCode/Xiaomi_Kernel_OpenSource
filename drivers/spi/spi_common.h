#ifndef SPI_COMMON
#define SPI_COMMON

/* Macros for SPI opcode generation */
#define MAX_BUF_SZ                      16
#define SPI_OPCODE_PHYADDR_SHIFT        6
#define SPI_OPCODE_PHYADDR(a)           ((a) << SPI_OPCODE_PHYADDR_SHIFT)
#define SPI_OPCODE_RDWR_SHIFT           5
#define SPI_OPCODE_WR                   (1 << SPI_OPCODE_RDWR_SHIFT)
#define SPI_OPCODE_RD                   (0 << SPI_OPCODE_RDWR_SHIFT)
#define SPI_OPCODE_INC_SHIFT            4
#define SPI_OPCODE_AUTO_INC             (1 << SPI_OPCODE_INC_SHIFT)
#define SPI_OPCODE_NO_INC               (0 << SPI_OPCODE_INC_SHIFT)
#define SPI_OPCODE_RD_WAIT_SHIFT        2
#define SPI_OPCODE_RD_WAIT_MASK         (0x3 << SPI_OPCODE_RD_WAIT_SHIFT)
#define SPI_OPCODE_RD_WAIT_0            (0 << SPI_OPCODE_RD_WAIT_SHIFT)
#define SPI_OPCODE_RD_WAIT_2            (1 << SPI_OPCODE_RD_WAIT_SHIFT)
#define SPI_OPCODE_RD_WAIT_4            (2 << SPI_OPCODE_RD_WAIT_SHIFT)
#define SPI_OPCODE_RD_WAIT_6            (3 << SPI_OPCODE_RD_WAIT_SHIFT)
#define SPI_OPCODE_TZ_SZ_MASK           0x3
#define SPI_OPCODE_TX_SZ_8              (0)
#define SPI_OPCODE_TX_SZ_16             (1)
#define SPI_OPCODE_TX_SZ_32             (2)
#define SPI_OPCODE_TX_SZ_64             (3)

#define DEFAULT_DEVICE_ID            (0)

#define GET_BIT(x, bit)          ((x & (1<<(bit))) >> (bit))
#define SET_BIT_ENABLE(x, bit)   ((x) |= (1<<(bit)))
#define SET_BIT_DISABLE(x, bit)  ((x) &= (~(1<<(bit))))

#define ACD_PHY_LINK_LINKED 0x1
#define ACD_PHY_LINK_BUSY 0x9

#define  g_spi_id  3

enum TestMode_1G {
    TestMode_2 = 0,
    TestMode_4,
    TestMode_5,
    TestMode_6,
    TestMode_IB,
};

#endif /* SPI_COMMON */
