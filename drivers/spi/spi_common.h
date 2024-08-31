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

int diagnosis_sysfs_init(struct spi_driver spidev_driver);
int spidev_open_kern(void);
int spidev_release_kern(void);
int spidev_message_kern(struct spi_ioc_transfer *u_xfers, unsigned n_xfers);

enum TestMode {
    TestMode_None = 0,
    TestMode_1,
    TestMode_2,
    TestMode_4 = 4,
    TestMode_5,
    TestMode_6,
    TestMode_IB,
    TestMode_TVCO,
};

#define VLAN_33 33
#define VLAN_35 35
#define VLAN_36 36
#define VLAN_45 45
#define VLAN_65 65
#define VLAN_100 100
#define VLAN_110 110

#define VLAN_PRI_0 0
#define VLAN_PRI_1 1
#define VLAN_PRI_2 2
#define VLAN_PRI_3 3
#define VLAN_PRI_4 4

#endif /* SPI_COMMON */
