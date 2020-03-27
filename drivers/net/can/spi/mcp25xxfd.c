// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * CAN bus driver for Microchip 25XXFD CAN Controller with SPI Interface
 *
 * Copyright 2017 Martin Sperl <kernel@xxxxxxxxxxxxxxxx>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Based on Microchip MCP251x CAN controller driver written by
 * David Vrabel, Copyright 2006 Arcom Control Systems Ltd.
 *
 */

#include <linux/can/core.h>
#include <linux/can/dev.h>
#include <linux/can/led.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/freezer.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>

#define DEVICE_NAME "mcp25xxfd"

/* device description and rational:
 *
 * the mcp25xxfd is a CanFD controller that also supports can2.0 only
 * modes.
 * It is connected via spi to the host and requires at minimum a single
 * irq line in addition to the SPI lines - it is not mentioned explicitly
 * in the documentation but in principle SPI 3-wire should be possible.
 *
 * The clock connected is typically 4MHz, 20MHz or 40MHz.
 * For the 4MHz clock the controller contains 10x PLL circuitry.
 *
 * The controller itself has 2KB or ECC-SRAM for data.
 * It also has 32 FIFOs (of up to 32 CAN-frames).
 * There are 4 Fifo types which can get configured:
 * * TEF - Transmission Event Fifo - which consumes FIFO 0
 *   even if it is not configured
 * * Tansmission Queue - for up to 32 Frames.
 *   this queue reorders CAN frames to get transmitted following the
 *   typical CAN dominant/recessive rules on the can bus itself.
 *   This FIFO is optional.
 * * TX FIFO: generic TX fifos that can contain arbitrary data
 *   and which come with a configurable priority for transmission
 *   It is also possible to have the Controller automatically trigger
 *   a transfer when a Filter Rule for a RTR frame matches.
 *   Each of these fifos in principle can get configured for distinct
 *   dlc sizes (8 thru 64 bytes)
 * * RX FIFO: generic RX fifo which is filled via filter-rules.
 *   Each of these fifos in principle can get configured for distinct
 *   dlc sizes (8 thru 64 bytes)
 *   Unfortunately there is no filter rule that would allow triggering
 *   on different frame sizes, so for all practical purposes the
 *   RX fifos have to be of the same size (unless one wants to experience
 *   lost data).
 * When a Can Frame is transmitted fromthe TX Queue or an individual
 * TX FIFO then a small TEF Frame can get added to the TEF FIFO queue
 * to log the Transmission of the frame - this includes ID, Flags
 * (including a custom identifier/index) .
 *
 * The controller provides an optional free running counter with a divider
 * for timestamping of RX frames as well as for TEF entries.
 *
 * Driver Implementation details and rational:
 * * The whole driver has been designed to give best performance
 *   and as little packet loss as possible with 1MHZ Can frames with DLC=0
 *   on small/slow devices like the Raspberry Pi 1
 * * This means that some optimizations for full duplex communication
 *   have been implemented to avoid CPU introduced latencies
 *   (especially for spi_write_then_read cases) - this only applies to
 *   4 wire SPI busses.
 * * Due to the fact that the TXQ does reorder Can-Frames we do not make
 *   use of it to avoid unexpected behaviour (say when running a
 *   firmware upgrade via Can)
 * * this means we use individual TX-fifos with a given priority and
 *   we have to wait until all the TX fifos have been transmitted before
 *   we can restart the networking queue to avoid reordering the frames on
 *   the Can bus itself.
 *   Still we can transmit a transmit only Duty-cycle of 66% to 90% on the
 *   Can bus (at 1MHz).
 *   The scaling factors here are:
 *   * Can bus speed - lower Speeds increase Duty-cycle
 *   * SPI Clock Rate - higher speeds increase duty-cycle
 *   * CPU speed + SPI implementation - reduces latencies between transfers
 * * There is a module parameter that allows the modification of the
 *   number of tx_fifos, which is by default 7.
 * * The driver offers some module parameters that allow to control the use
 *   of some optimizations (prefer reading more data than necessary instead
 *   of multiple SPI transfers - the idea here is that this way we may
 *   allow the SPI-controller to use DMA instead of programmed IO to
 *   limit latencies/number of interrupts)
 *   When we have to read multiple RX frames in CanFD mode:
 *   * we allow reading all 64 bytes of payload even if DLC <=8
 *     this mode is used in Can2.0 only mode by default and can not get
 *     disabled (SRAM Reads have to be a multiple of 4 bytes anyway)
 *   * Releasing/freeing the RX queue requires writing of 1 byte per fifo.
 *     unfortunately these 32-bit registers are not ajacent to each other,
 *     so that for 2 consecutive RX Frames instead of writing 1 byte per
 *     fifo (with protocol overhead of 2 bytes - so a total of 6 bytes in
 *     2 transfers) we transmit 13 bytes (with a protocol overhead of 2 -
 *     so a total of 15 bytes)
 *     This optimization is only enabled by a module parameter.
 * * we use TEF + time stamping to record the transmitted frames
 *   including their timestamp - we use this to order TX and RX frames
 *   when submitting them to the network stack.
 * * due to the inability to "filter" based on DLC sizes we have to use
 *   a common FIFO size. This is 8 bytes for Can2.0 and 64 bytes for CanFD.
 * * the driver tries to detect the Controller only by reading registers,
 *   but there are circumstances (e.g. after a crashed driver) where we
 *   have to "blindly" configure the clock rate to get the controller to
 *   respond correctly.
 * * There is one situation where the controller will require a full POR
 *   (total power off) to recover from a bad Clock configuration.
 *   This happens when the wrong clock is configured in the device tree
 *   (say 4MHz are configured, while 20 or 40MHz are used)
 *   in such a situation the driver tries to enable the PLL, which will
 *   never synchronize and the controller becomes unresponsive to further
 *   spi requests until a POR.
 */

#define MCP25XXFD_OST_DELAY_MS		3
#define MCP25XXFD_MIN_CLOCK_FREQUENCY	1000000
#define MCP25XXFD_MAX_CLOCK_FREQUENCY	40000000
#define MCP25XXFD_PLL_MULTIPLIER	10
#define MCP25XXFD_AUTO_PLL_MAX_CLOCK_FREQUENCY				\
	(MCP25XXFD_MAX_CLOCK_FREQUENCY / MCP25XXFD_PLL_MULTIPLIER)
#define MCP25XXFD_SCLK_DIVIDER		2

#define MCP25XXFD_OSC_POLLING_JIFFIES	(HZ / 2)

#define TX_ECHO_SKB_MAX	32

#define INSTRUCTION_RESET		0x0000
#define INSTRUCTION_READ		0x3000
#define INSTRUCTION_WRITE		0x2000
#define INSTRUCTION_READ_CRC		0xB000
#define INSTRUCTION_WRITE_CRC		0xA000
#define INSTRUCTION_WRITE_SAVE		0xC000

#define ADDRESS_MASK			0x0fff

#define MCP25XXFD_SFR_BASE(x)		(0xE00 + (x))
#define MCP25XXFD_OSC			MCP25XXFD_SFR_BASE(0x00)
#  define MCP25XXFD_OSC_PLLEN		BIT(0)
#  define MCP25XXFD_OSC_OSCDIS		BIT(2)
#  define MCP25XXFD_OSC_SCLKDIV		BIT(4)
#  define MCP25XXFD_OSC_CLKODIV_BITS	2
#  define MCP25XXFD_OSC_CLKODIV_SHIFT	5
#  define MCP25XXFD_OSC_CLKODIV_MASK			\
	GENMASK(MCP25XXFD_OSC_CLKODIV_SHIFT		\
		+ MCP25XXFD_OSC_CLKODIV_BITS - 1,	\
		MCP25XXFD_OSC_CLKODIV_SHIFT)
#  define MCP25XXFD_OSC_CLKODIV_10	3
#  define MCP25XXFD_OSC_CLKODIV_4	2
#  define MCP25XXFD_OSC_CLKODIV_2	1
#  define MCP25XXFD_OSC_CLKODIV_1	0
#  define MCP25XXFD_OSC_PLLRDY		BIT(8)
#  define MCP25XXFD_OSC_OSCRDY		BIT(10)
#  define MCP25XXFD_OSC_SCLKRDY		BIT(12)
#define MCP25XXFD_IOCON			MCP25XXFD_SFR_BASE(0x04)
#  define MCP25XXFD_IOCON_TRIS0		BIT(0)
#  define MCP25XXFD_IOCON_TRIS1		BIT(1)
#  define MCP25XXFD_IOCON_XSTBYEN	BIT(6)
#  define MCP25XXFD_IOCON_LAT0		BIT(8)
#  define MCP25XXFD_IOCON_LAT1		BIT(9)
#  define MCP25XXFD_IOCON_GPIO0		BIT(16)
#  define MCP25XXFD_IOCON_GPIO1		BIT(17)
#  define MCP25XXFD_IOCON_PM0		BIT(24)
#  define MCP25XXFD_IOCON_PM1		BIT(25)
#  define MCP25XXFD_IOCON_TXCANOD	BIT(28)
#  define MCP25XXFD_IOCON_SOF		BIT(29)
#  define MCP25XXFD_IOCON_INTOD		BIT(29)
#define MCP25XXFD_CRC			MCP25XXFD_SFR_BASE(0x08)
#  define MCP25XXFD_CRC_MASK		GENMASK(15, 0)
#  define MCP25XXFD_CRC_CRCERRIE	BIT(16)
#  define MCP25XXFD_CRC_FERRIE		BIT(17)
#  define MCP25XXFD_CRC_CRCERRIF	BIT(24)
#  define MCP25XXFD_CRC_FERRIF		BIT(25)
#define MCP25XXFD_ECCCON		MCP25XXFD_SFR_BASE(0x0C)
#  define MCP25XXFD_ECCCON_ECCEN	BIT(0)
#  define MCP25XXFD_ECCCON_SECIE	BIT(1)
#  define MCP25XXFD_ECCCON_DEDIE	BIT(2)
#  define MCP25XXFD_ECCCON_PARITY_BITS 6
#  define MCP25XXFD_ECCCON_PARITY_SHIFT 8
#  define MCP25XXFD_ECCCON_PARITY_MASK			\
	GENMASK(MCP25XXFD_ECCCON_PARITY_SHIFT		\
		+ MCP25XXFD_ECCCON_PARITY_BITS - 1,	\
		MCP25XXFD_ECCCON_PARITY_SHIFT)
#define MCP25XXFD_ECCSTAT		MCP25XXFD_SFR_BASE(0x10)
#  define MCP25XXFD_ECCSTAT_SECIF	BIT(1)
#  define MCP25XXFD_ECCSTAT_DEDIF	BIT(2)
#  define MCP25XXFD_ECCSTAT_ERRADDR_SHIFT 8
#  define MCP25XXFD_ECCSTAT_ERRADDR_MASK	      \
	GENMASK(MCP25XXFD_ECCSTAT_ERRADDR_SHIFT + 11, \
		MCP25XXFD_ECCSTAT_ERRADDR_SHIFT)

#define CAN_SFR_BASE(x)			(0x000 + (x))
#define CAN_CON				CAN_SFR_BASE(0x00)
#  define CAN_CON_DNCNT_BITS		5
#  define CAN_CON_DNCNT_SHIFT		0
#  define CAN_CON_DNCNT_MASK					\
	GENMASK(CAN_CON_DNCNT_SHIFT + CAN_CON_DNCNT_BITS - 1,	\
		CAN_CON_DNCNT_SHIFT)
#  define CAN_CON_ISOCRCEN		BIT(5)
#  define CAN_CON_PXEDIS		BIT(6)
#  define CAN_CON_WAKFIL		BIT(8)
#  define CAN_CON_WFT_BITS		2
#  define CAN_CON_WFT_SHIFT		9
#  define CAN_CON_WFT_MASK					\
	GENMASK(CAN_CON_WFT_SHIFT + CAN_CON_WFT_BITS - 1,	\
		CAN_CON_WFT_SHIFT)
#  define CAN_CON_BUSY			BIT(11)
#  define CAN_CON_BRSDIS		BIT(12)
#  define CAN_CON_RTXAT			BIT(16)
#  define CAN_CON_ESIGM			BIT(17)
#  define CAN_CON_SERR2LOM		BIT(18)
#  define CAN_CON_STEF			BIT(19)
#  define CAN_CON_TXQEN			BIT(20)
#  define CAN_CON_OPMODE_BITS		3
#  define CAN_CON_OPMOD_SHIFT		21
#  define CAN_CON_OPMOD_MASK					\
	GENMASK(CAN_CON_OPMOD_SHIFT + CAN_CON_OPMODE_BITS - 1,	\
		CAN_CON_OPMOD_SHIFT)
#  define CAN_CON_REQOP_BITS		3
#  define CAN_CON_REQOP_SHIFT		24
#  define CAN_CON_REQOP_MASK					\
	GENMASK(CAN_CON_REQOP_SHIFT + CAN_CON_REQOP_BITS - 1,	\
		CAN_CON_REQOP_SHIFT)
#    define CAN_CON_MODE_MIXED			0
#    define CAN_CON_MODE_SLEEP			1
#    define CAN_CON_MODE_INTERNAL_LOOPBACK	2
#    define CAN_CON_MODE_LISTENONLY		3
#    define CAN_CON_MODE_CONFIG			4
#    define CAN_CON_MODE_EXTERNAL_LOOPBACK	5
#    define CAN_CON_MODE_CAN2_0			6
#    define CAN_CON_MODE_RESTRICTED		7
#  define CAN_CON_ABAT			BIT(27)
#  define CAN_CON_TXBWS_BITS		3
#  define CAN_CON_TXBWS_SHIFT		28
#  define CAN_CON_TXBWS_MASK					\
	GENMASK(CAN_CON_TXBWS_SHIFT + CAN_CON_TXBWS_BITS - 1,	\
		CAN_CON_TXBWS_SHIFT)
#  define CAN_CON_DEFAULT				\
	(CAN_CON_ISOCRCEN |				\
	 CAN_CON_PXEDIS |				\
	 CAN_CON_WAKFIL |				\
	 (3 << CAN_CON_WFT_SHIFT) |			\
	 CAN_CON_STEF |					\
	 CAN_CON_TXQEN |				\
	 (CAN_CON_MODE_CONFIG << CAN_CON_OPMOD_SHIFT) |	\
	 (CAN_CON_MODE_CONFIG << CAN_CON_REQOP_SHIFT))
#  define CAN_CON_DEFAULT_MASK	\
	(CAN_CON_DNCNT_MASK |	\
	 CAN_CON_ISOCRCEN |	\
	 CAN_CON_PXEDIS |	\
	 CAN_CON_WAKFIL |	\
	 CAN_CON_WFT_MASK |	\
	 CAN_CON_BRSDIS |	\
	 CAN_CON_RTXAT |	\
	 CAN_CON_ESIGM |	\
	 CAN_CON_SERR2LOM |	\
	 CAN_CON_STEF |		\
	 CAN_CON_TXQEN |	\
	 CAN_CON_OPMOD_MASK |	\
	 CAN_CON_REQOP_MASK |	\
	 CAN_CON_ABAT |		\
	 CAN_CON_TXBWS_MASK)
#define CAN_NBTCFG			CAN_SFR_BASE(0x04)
#  define CAN_NBTCFG_SJW_BITS		7
#  define CAN_NBTCFG_SJW_SHIFT		0
#  define CAN_NBTCFG_SJW_MASK					\
	GENMASK(CAN_NBTCFG_SJW_SHIFT + CAN_NBTCFG_SJW_BITS - 1, \
		CAN_NBTCFG_SJW_SHIFT)
#  define CAN_NBTCFG_TSEG2_BITS		7
#  define CAN_NBTCFG_TSEG2_SHIFT	8
#  define CAN_NBTCFG_TSEG2_MASK					    \
	GENMASK(CAN_NBTCFG_TSEG2_SHIFT + CAN_NBTCFG_TSEG2_BITS - 1, \
		CAN_NBTCFG_TSEG2_SHIFT)
#  define CAN_NBTCFG_TSEG1_BITS		8
#  define CAN_NBTCFG_TSEG1_SHIFT	16
#  define CAN_NBTCFG_TSEG1_MASK					    \
	GENMASK(CAN_NBTCFG_TSEG1_SHIFT + CAN_NBTCFG_TSEG1_BITS - 1, \
		CAN_NBTCFG_TSEG1_SHIFT)
#  define CAN_NBTCFG_BRP_BITS		8
#  define CAN_NBTCFG_BRP_SHIFT		24
#  define CAN_NBTCFG_BRP_MASK					\
	GENMASK(CAN_NBTCFG_BRP_SHIFT + CAN_NBTCFG_BRP_BITS - 1, \
		CAN_NBTCFG_BRP_SHIFT)
#define CAN_DBTCFG			CAN_SFR_BASE(0x08)
#  define CAN_DBTCFG_SJW_BITS		4
#  define CAN_DBTCFG_SJW_SHIFT		0
#  define CAN_DBTCFG_SJW_MASK					\
	GENMASK(CAN_DBTCFG_SJW_SHIFT + CAN_DBTCFG_SJW_BITS - 1, \
		CAN_DBTCFG_SJW_SHIFT)
#  define CAN_DBTCFG_TSEG2_BITS		4
#  define CAN_DBTCFG_TSEG2_SHIFT	8
#  define CAN_DBTCFG_TSEG2_MASK					    \
	GENMASK(CAN_DBTCFG_TSEG2_SHIFT + CAN_DBTCFG_TSEG2_BITS - 1, \
		CAN_DBTCFG_TSEG2_SHIFT)
#  define CAN_DBTCFG_TSEG1_BITS		5
#  define CAN_DBTCFG_TSEG1_SHIFT	16
#  define CAN_DBTCFG_TSEG1_MASK					    \
	GENMASK(CAN_DBTCFG_TSEG1_SHIFT + CAN_DBTCFG_TSEG1_BITS - 1, \
		CAN_DBTCFG_TSEG1_SHIFT)
#  define CAN_DBTCFG_BRP_BITS		8
#  define CAN_DBTCFG_BRP_SHIFT		24
#  define CAN_DBTCFG_BRP_MASK					\
	GENMASK(CAN_DBTCFG_BRP_SHIFT + CAN_DBTCFG_BRP_BITS - 1, \
		CAN_DBTCFG_BRP_SHIFT)
#define CAN_TDC				CAN_SFR_BASE(0x0C)
#  define CAN_TDC_TDCV_BITS		5
#  define CAN_TDC_TDCV_SHIFT		0
#  define CAN_TDC_TDCV_MASK					\
	GENMASK(CAN_TDC_TDCV_SHIFT + CAN_TDC_TDCV_BITS - 1, \
		CAN_TDC_TDCV_SHIFT)
#  define CAN_TDC_TDCO_BITS		5
#  define CAN_TDC_TDCO_SHIFT		8
#  define CAN_TDC_TDCO_MASK					\
	GENMASK(CAN_TDC_TDCO_SHIFT + CAN_TDC_TDCO_BITS - 1, \
		CAN_TDC_TDCO_SHIFT)
#  define CAN_TDC_TDCMOD_BITS		2
#  define CAN_TDC_TDCMOD_SHIFT		16
#  define CAN_TDC_TDCMOD_MASK					\
	GENMASK(CAN_TDC_TDCMOD_SHIFT + CAN_TDC_TDCMOD_BITS - 1, \
		CAN_TDC_TDCMOD_SHIFT)
#  define CAN_TDC_TDCMOD_DISABLED	0
#  define CAN_TDC_TDCMOD_MANUAL		1
#  define CAN_TDC_TDCMOD_AUTO		2
#  define CAN_TDC_SID11EN		BIT(24)
#  define CAN_TDC_EDGFLTEN		BIT(25)
#define CAN_TBC				CAN_SFR_BASE(0x10)
#define CAN_TSCON			CAN_SFR_BASE(0x14)
#  define CAN_TSCON_TBCPRE_BITS		10
#  define CAN_TSCON_TBCPRE_SHIFT	0
#  define CAN_TSCON_TBCPRE_MASK					    \
	GENMASK(CAN_TSCON_TBCPRE_SHIFT + CAN_TSCON_TBCPRE_BITS - 1, \
		CAN_TSCON_TBCPRE_SHIFT)
#  define CAN_TSCON_TBCEN		BIT(16)
#  define CAN_TSCON_TSEOF		BIT(17)
#  define CAN_TSCON_TSRES		BIT(18)
#define CAN_VEC				CAN_SFR_BASE(0x18)
#  define CAN_VEC_ICODE_BITS		7
#  define CAN_VEC_ICODE_SHIFT		0
#  define CAN_VEC_ICODE_MASK					    \
	GENMASK(CAN_VEC_ICODE_SHIFT + CAN_VEC_ICODE_BITS - 1,	    \
		CAN_VEC_ICODE_SHIFT)
#  define CAN_VEC_FILHIT_BITS		5
#  define CAN_VEC_FILHIT_SHIFT		8
#  define CAN_VEC_FILHIT_MASK					\
	GENMASK(CAN_VEC_FILHIT_SHIFT + CAN_VEC_FILHIT_BITS - 1, \
		CAN_VEC_FILHIT_SHIFT)
#  define CAN_VEC_TXCODE_BITS		7
#  define CAN_VEC_TXCODE_SHIFT		16
#  define CAN_VEC_TXCODE_MASK					\
	GENMASK(CAN_VEC_TXCODE_SHIFT + CAN_VEC_TXCODE_BITS - 1, \
		CAN_VEC_TXCODE_SHIFT)
#  define CAN_VEC_RXCODE_BITS		7
#  define CAN_VEC_RXCODE_SHIFT		24
#  define CAN_VEC_RXCODE_MASK					\
	GENMASK(CAN_VEC_RXCODE_SHIFT + CAN_VEC_RXCODE_BITS - 1, \
		CAN_VEC_RXCODE_SHIFT)
#define CAN_INT				CAN_SFR_BASE(0x1C)
#  define CAN_INT_IF_SHIFT		0
#  define CAN_INT_TXIF			BIT(0)
#  define CAN_INT_RXIF			BIT(1)
#  define CAN_INT_TBCIF			BIT(2)
#  define CAN_INT_MODIF			BIT(3)
#  define CAN_INT_TEFIF			BIT(4)
#  define CAN_INT_ECCIF			BIT(8)
#  define CAN_INT_SPICRCIF		BIT(9)
#  define CAN_INT_TXATIF		BIT(10)
#  define CAN_INT_RXOVIF		BIT(11)
#  define CAN_INT_SERRIF		BIT(12)
#  define CAN_INT_CERRIF		BIT(13)
#  define CAN_INT_WAKIF			BIT(14)
#  define CAN_INT_IVMIF			BIT(15)
#  define CAN_INT_IF_MASK		\
	(CAN_INT_TXIF |			\
	 CAN_INT_RXIF |			\
	 CAN_INT_TBCIF	|		\
	 CAN_INT_MODIF	|		\
	 CAN_INT_TEFIF	|		\
	 CAN_INT_ECCIF	|		\
	 CAN_INT_SPICRCIF |		\
	 CAN_INT_TXATIF |		\
	 CAN_INT_RXOVIF |		\
	 CAN_INT_CERRIF |		\
	 CAN_INT_SERRIF |		\
	 CAN_INT_WAKEIF |		\
	 CAN_INT_IVMIF)
#  define CAN_INT_IE_SHIFT		16
#  define CAN_INT_TXIE			(CAN_INT_TXIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_RXIE			(CAN_INT_RXIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_TBCIE			(CAN_INT_TBCIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_MODIE			(CAN_INT_MODIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_TEFIE			(CAN_INT_TEFIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_ECCIE			(CAN_INT_ECCIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_SPICRCIE		\
	(CAN_INT_SPICRCIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_TXATIE		(CAN_INT_TXATIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_RXOVIE		(CAN_INT_RXOVIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_CERRIE		(CAN_INT_CERRIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_SERRIE		(CAN_INT_SERRIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_WAKIE			(CAN_INT_WAKIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_IVMIE			(CAN_INT_IVMIF << CAN_INT_IE_SHIFT)
#  define CAN_INT_IE_MASK		\
	(CAN_INT_TXIE |			\
	 CAN_INT_RXIE |			\
	 CAN_INT_TBCIE	|		\
	 CAN_INT_MODIE	|		\
	 CAN_INT_TEFIE	|		\
	 CAN_INT_ECCIE	|		\
	 CAN_INT_SPICRCIE |		\
	 CAN_INT_TXATIE |		\
	 CAN_INT_RXOVIE |		\
	 CAN_INT_CERRIE |		\
	 CAN_INT_SERRIE |		\
	 CAN_INT_WAKEIE |		\
	 CAN_INT_IVMIE)
#define CAN_RXIF			CAN_SFR_BASE(0x20)
#define CAN_TXIF			CAN_SFR_BASE(0x24)
#define CAN_RXOVIF			CAN_SFR_BASE(0x28)
#define CAN_TXATIF			CAN_SFR_BASE(0x2C)
#define CAN_TXREQ			CAN_SFR_BASE(0x30)
#define CAN_TREC			CAN_SFR_BASE(0x34)
#  define CAN_TREC_REC_BITS		8
#  define CAN_TREC_REC_SHIFT		0
#  define CAN_TREC_REC_MASK				    \
	GENMASK(CAN_TREC_REC_SHIFT + CAN_TREC_REC_BITS - 1, \
		CAN_TREC_REC_SHIFT)
#  define CAN_TREC_TEC_BITS		8
#  define CAN_TREC_TEC_SHIFT		8
#  define CAN_TREC_TEC_MASK				    \
	GENMASK(CAN_TREC_TEC_SHIFT + CAN_TREC_TEC_BITS - 1, \
		CAN_TREC_TEC_SHIFT)
#  define CAN_TREC_EWARN		BIT(16)
#  define CAN_TREC_RXWARN		BIT(17)
#  define CAN_TREC_TXWARN		BIT(18)
#  define CAN_TREC_RXBP			BIT(19)
#  define CAN_TREC_TXBP			BIT(20)
#  define CAN_TREC_TXBO			BIT(21)
#define CAN_BDIAG0			CAN_SFR_BASE(0x38)
#  define CAN_BDIAG0_NRERRCNT_BITS	8
#  define CAN_BDIAG0_NRERRCNT_SHIFT	0
#  define CAN_BDIAG0_NRERRCNT_MASK				\
	GENMASK(CAN_BDIAG0_NRERRCNT_SHIFT + CAN_BDIAG0_NRERRCNT_BITS - 1, \
		CAN_BDIAG0_NRERRCNT_SHIFT)
#  define CAN_BDIAG0_NTERRCNT_BITS	8
#  define CAN_BDIAG0_NTERRCNT_SHIFT	8
#  define CAN_BDIAG0_NTERRCNT_MASK					\
	GENMASK(CAN_BDIAG0_NTERRCNT_SHIFT + CAN_BDIAG0_NTERRCNT_BITS - 1, \
		CAN_BDIAG0_NTERRCNT_SHIFT)
#  define CAN_BDIAG0_DRERRCNT_BITS	8
#  define CAN_BDIAG0_DRERRCNT_SHIFT	16
#  define CAN_BDIAG0_DRERRCNT_MASK					\
	GENMASK(CAN_BDIAG0_DRERRCNT_SHIFT + CAN_BDIAG0_DRERRCNT_BITS - 1, \
		CAN_BDIAG0_DRERRCNT_SHIFT)
#  define CAN_BDIAG0_DTERRCNT_BITS	8
#  define CAN_BDIAG0_DTERRCNT_SHIFT	24
#  define CAN_BDIAG0_DTERRCNT_MASK					\
	GENMASK(CAN_BDIAG0_DTERRCNT_SHIFT + CAN_BDIAG0_DTERRCNT_BITS - 1, \
		CAN_BDIAG0_DTERRCNT_SHIFT)
#define CAN_BDIAG1			CAN_SFR_BASE(0x3C)
#  define CAN_BDIAG1_EFMSGCNT_BITS	16
#  define CAN_BDIAG1_EFMSGCNT_SHIFT	0
#  define CAN_BDIAG1_EFMSGCNT_MASK					\
	GENMASK(CAN_BDIAG1_EFMSGCNT_SHIFT + CAN_BDIAG1_EFMSGCNT_BITS - 1, \
		CAN_BDIAG1_EFMSGCNT_SHIFT)
#  define CAN_BDIAG1_NBIT0ERR		BIT(16)
#  define CAN_BDIAG1_NBIT1ERR		BIT(17)
#  define CAN_BDIAG1_NACKERR		BIT(18)
#  define CAN_BDIAG1_NSTUFERR		BIT(19)
#  define CAN_BDIAG1_NFORMERR		BIT(20)
#  define CAN_BDIAG1_NCRCERR		BIT(21)
#  define CAN_BDIAG1_TXBOERR		BIT(23)
#  define CAN_BDIAG1_DBIT0ERR		BIT(24)
#  define CAN_BDIAG1_DBIT1ERR		BIT(25)
#  define CAN_BDIAG1_DFORMERR		BIT(27)
#  define CAN_BDIAG1_DSTUFERR		BIT(28)
#  define CAN_BDIAG1_DCRCERR		BIT(29)
#  define CAN_BDIAG1_ESI		BIT(30)
#  define CAN_BDIAG1_DLCMM		BIT(31)
#define CAN_TEFCON			CAN_SFR_BASE(0x40)
#  define CAN_TEFCON_TEFNEIE		BIT(0)
#  define CAN_TEFCON_TEFHIE		BIT(1)
#  define CAN_TEFCON_TEFFIE		BIT(2)
#  define CAN_TEFCON_TEFOVIE		BIT(3)
#  define CAN_TEFCON_TEFTSEN		BIT(5)
#  define CAN_TEFCON_UINC		BIT(8)
#  define CAN_TEFCON_FRESET		BIT(10)
#  define CAN_TEFCON_FSIZE_BITS		5
#  define CAN_TEFCON_FSIZE_SHIFT	24
#  define CAN_TEFCON_FSIZE_MASK					    \
	GENMASK(CAN_TEFCON_FSIZE_SHIFT + CAN_TEFCON_FSIZE_BITS - 1, \
		CAN_TEFCON_FSIZE_SHIFT)
#define CAN_TEFSTA			CAN_SFR_BASE(0x44)
#  define CAN_TEFSTA_TEFNEIF		BIT(0)
#  define CAN_TEFSTA_TEFHIF		BIT(1)
#  define CAN_TEFSTA_TEFFIF		BIT(2)
#  define CAN_TEFSTA_TEVOVIF		BIT(3)
#define CAN_TEFUA			CAN_SFR_BASE(0x48)
#define CAN_RESERVED			CAN_SFR_BASE(0x4C)
#define CAN_TXQCON			CAN_SFR_BASE(0x50)
#  define CAN_TXQCON_TXQNIE		BIT(0)
#  define CAN_TXQCON_TXQEIE		BIT(2)
#  define CAN_TXQCON_TXATIE		BIT(4)
#  define CAN_TXQCON_TXEN		BIT(7)
#  define CAN_TXQCON_UINC		BIT(8)
#  define CAN_TXQCON_TXREQ		BIT(9)
#  define CAN_TXQCON_FRESET		BIT(10)
#  define CAN_TXQCON_TXPRI_BITS		5
#  define CAN_TXQCON_TXPRI_SHIFT	16
#  define CAN_TXQCON_TXPRI_MASK					    \
	GENMASK(CAN_TXQCON_TXPRI_SHIFT + CAN_TXQCON_TXPRI_BITS - 1, \
		CAN_TXQCON_TXPRI_SHIFT)
#  define CAN_TXQCON_TXAT_BITS		2
#  define CAN_TXQCON_TXAT_SHIFT		21
#  define CAN_TXQCON_TXAT_MASK					    \
	GENMASK(CAN_TXQCON_TXAT_SHIFT + CAN_TXQCON_TXAT_BITS - 1, \
		CAN_TXQCON_TXAT_SHIFT)
#  define CAN_TXQCON_FSIZE_BITS		5
#  define CAN_TXQCON_FSIZE_SHIFT	24
#  define CAN_TXQCON_FSIZE_MASK					    \
	GENMASK(CAN_TXQCON_FSIZE_SHIFT + CAN_TXQCON_FSIZE_BITS - 1, \
		CAN_TXQCON_FSIZE_SHIFT)
#  define CAN_TXQCON_PLSIZE_BITS	3
#  define CAN_TXQCON_PLSIZE_SHIFT	29
#  define CAN_TXQCON_PLSIZE_MASK				      \
	GENMASK(CAN_TXQCON_PLSIZE_SHIFT + CAN_TXQCON_PLSIZE_BITS - 1, \
		CAN_TXQCON_PLSIZE_SHIFT)
#    define CAN_TXQCON_PLSIZE_8		0
#    define CAN_TXQCON_PLSIZE_12	1
#    define CAN_TXQCON_PLSIZE_16	2
#    define CAN_TXQCON_PLSIZE_20	3
#    define CAN_TXQCON_PLSIZE_24	4
#    define CAN_TXQCON_PLSIZE_32	5
#    define CAN_TXQCON_PLSIZE_48	6
#    define CAN_TXQCON_PLSIZE_64	7

#define CAN_TXQSTA			CAN_SFR_BASE(0x54)
#  define CAN_TXQSTA_TXQNIF		BIT(0)
#  define CAN_TXQSTA_TXQEIF		BIT(2)
#  define CAN_TXQSTA_TXATIF		BIT(4)
#  define CAN_TXQSTA_TXERR		BIT(5)
#  define CAN_TXQSTA_TXLARB		BIT(6)
#  define CAN_TXQSTA_TXABT		BIT(7)
#  define CAN_TXQSTA_TXQCI_BITS		5
#  define CAN_TXQSTA_TXQCI_SHIFT	8
#  define CAN_TXQSTA_TXQCI_MASK					    \
	GENMASK(CAN_TXQSTA_TXQCI_SHIFT + CAN_TXQSTA_TXQCI_BITS - 1, \
		CAN_TXQSTA_TXQCI_SHIFT)

#define CAN_TXQUA			CAN_SFR_BASE(0x58)
#define CAN_FIFOCON(x)			CAN_SFR_BASE(0x5C + 12 * ((x) - 1))
#define CAN_FIFOCON_TFNRFNIE		BIT(0)
#define CAN_FIFOCON_TFHRFHIE		BIT(1)
#define CAN_FIFOCON_TFERFFIE		BIT(2)
#define CAN_FIFOCON_RXOVIE		BIT(3)
#define CAN_FIFOCON_TXATIE		BIT(4)
#define CAN_FIFOCON_RXTSEN		BIT(5)
#define CAN_FIFOCON_RTREN		BIT(6)
#define CAN_FIFOCON_TXEN		BIT(7)
#define CAN_FIFOCON_UINC		BIT(8)
#define CAN_FIFOCON_TXREQ		BIT(9)
#define CAN_FIFOCON_FRESET		BIT(10)
#  define CAN_FIFOCON_TXPRI_BITS	5
#  define CAN_FIFOCON_TXPRI_SHIFT	16
#  define CAN_FIFOCON_TXPRI_MASK					\
	GENMASK(CAN_FIFOCON_TXPRI_SHIFT + CAN_FIFOCON_TXPRI_BITS - 1,	\
		CAN_FIFOCON_TXPRI_SHIFT)
#  define CAN_FIFOCON_TXAT_BITS		2
#  define CAN_FIFOCON_TXAT_SHIFT	21
#  define CAN_FIFOCON_TXAT_MASK					    \
	GENMASK(CAN_FIFOCON_TXAT_SHIFT + CAN_FIFOCON_TXAT_BITS - 1, \
		CAN_FIFOCON_TXAT_SHIFT)
#  define CAN_FIFOCON_TXAT_ONE_SHOT	0
#  define CAN_FIFOCON_TXAT_THREE_SHOT	1
#  define CAN_FIFOCON_TXAT_UNLIMITED	2
#  define CAN_FIFOCON_FSIZE_BITS	5
#  define CAN_FIFOCON_FSIZE_SHIFT	24
#  define CAN_FIFOCON_FSIZE_MASK					\
	GENMASK(CAN_FIFOCON_FSIZE_SHIFT + CAN_FIFOCON_FSIZE_BITS - 1,	\
		CAN_FIFOCON_FSIZE_SHIFT)
#  define CAN_FIFOCON_PLSIZE_BITS	3
#  define CAN_FIFOCON_PLSIZE_SHIFT	29
#  define CAN_FIFOCON_PLSIZE_MASK					\
	GENMASK(CAN_FIFOCON_PLSIZE_SHIFT + CAN_FIFOCON_PLSIZE_BITS - 1, \
		CAN_FIFOCON_PLSIZE_SHIFT)
#define CAN_FIFOSTA(x)			CAN_SFR_BASE(0x60 + 12 * ((x) - 1))
#  define CAN_FIFOSTA_TFNRFNIF		BIT(0)
#  define CAN_FIFOSTA_TFHRFHIF		BIT(1)
#  define CAN_FIFOSTA_TFERFFIF		BIT(2)
#  define CAN_FIFOSTA_RXOVIF		BIT(3)
#  define CAN_FIFOSTA_TXATIF		BIT(4)
#  define CAN_FIFOSTA_TXERR		BIT(5)
#  define CAN_FIFOSTA_TXLARB		BIT(6)
#  define CAN_FIFOSTA_TXABT		BIT(7)
#  define CAN_FIFOSTA_FIFOCI_BITS	5
#  define CAN_FIFOSTA_FIFOCI_SHIFT	8
#  define CAN_FIFOSTA_FIFOCI_MASK					\
	GENMASK(CAN_FIFOSTA_FIFOCI_SHIFT + CAN_FIFOSTA_FIFOCI_BITS - 1, \
		CAN_FIFOSTA_FIFOCI_SHIFT)
#define CAN_FIFOUA(x)			CAN_SFR_BASE(0x64 + 12 * ((x) - 1))
#define CAN_FLTCON(x)			CAN_SFR_BASE(0x1D0 + ((x) & 0x1c))
#  define CAN_FILCON_SHIFT(x)		(((x) & 3) * 8)
#  define CAN_FILCON_BITS(x)		CAN_FILCON_BITS_
#  define CAN_FILCON_BITS_		4
	/* avoid macro reuse warning, so do not use GENMASK as above */
#  define CAN_FILCON_MASK(x)					\
	(GENMASK(CAN_FILCON_BITS_ - 1, 0) << CAN_FILCON_SHIFT(x))
#  define CAN_FIFOCON_FLTEN(x)		BIT(7 + CAN_FILCON_SHIFT(x))
#define CAN_FLTOBJ(x)			CAN_SFR_BASE(0x1F0 + 8 * (x))
#  define CAN_FILOBJ_SID_BITS		11
#  define CAN_FILOBJ_SID_SHIFT		0
#  define CAN_FILOBJ_SID_MASK					\
	GENMASK(CAN_FILOBJ_SID_SHIFT + CAN_FILOBJ_SID_BITS - 1, \
		CAN_FILOBJ_SID_SHIFT)
#  define CAN_FILOBJ_EID_BITS		18
#  define CAN_FILOBJ_EID_SHIFT		12
#  define CAN_FILOBJ_EID_MASK					\
	GENMASK(CAN_FILOBJ_EID_SHIFT + CAN_FILOBJ_EID_BITS - 1, \
		CAN_FILOBJ_EID_SHIFT)
#  define CAN_FILOBJ_SID11		BIT(29)
#  define CAN_FILOBJ_EXIDE		BIT(30)
#define CAN_FLTMASK(x)			CAN_SFR_BASE(0x1F4 + 8 * (x))
#  define CAN_FILMASK_MSID_BITS		11
#  define CAN_FILMASK_MSID_SHIFT	0
#  define CAN_FILMASK_MSID_MASK					\
	GENMASK(CAN_FILMASK_MSID_SHIFT + CAN_FILMASK_MSID_BITS - 1, \
		CAN_FILMASK_MSID_SHIFT)
#  define CAN_FILMASK_MEID_BITS		18
#  define CAN_FILMASK_MEID_SHIFT	12
#  define CAN_FILMASK_MEID_MASK					\
	GENMASK(CAN_FILMASK_MEID_SHIFT + CAN_FILMASK_MEID_BITS - 1, \
		CAN_FILMASK_MEID_SHIFT)
#  define CAN_FILMASK_MSID11		BIT(29)
#  define CAN_FILMASK_MIDE		BIT(30)

#define CAN_OBJ_ID_SID_BITS		11
#define CAN_OBJ_ID_SID_SHIFT		0
#define CAN_OBJ_ID_SID_MASK					\
	GENMASK(CAN_OBJ_ID_SID_SHIFT + CAN_OBJ_ID_SID_BITS - 1, \
		CAN_OBJ_ID_SID_SHIFT)
#define CAN_OBJ_ID_EID_BITS		18
#define CAN_OBJ_ID_EID_SHIFT		11
#define CAN_OBJ_ID_EID_MASK					\
	GENMASK(CAN_OBJ_ID_EID_SHIFT + CAN_OBJ_ID_EID_BITS - 1, \
		CAN_OBJ_ID_EID_SHIFT)
#define CAN_OBJ_ID_SID_BIT11		BIT(29)

#define CAN_OBJ_FLAGS_DLC_BITS		4
#define CAN_OBJ_FLAGS_DLC_SHIFT		0
#define CAN_OBJ_FLAGS_DLC_MASK					      \
	GENMASK(CAN_OBJ_FLAGS_DLC_SHIFT + CAN_OBJ_FLAGS_DLC_BITS - 1, \
		CAN_OBJ_FLAGS_DLC_SHIFT)
#define CAN_OBJ_FLAGS_IDE		BIT(4)
#define CAN_OBJ_FLAGS_RTR		BIT(5)
#define CAN_OBJ_FLAGS_BRS		BIT(6)
#define CAN_OBJ_FLAGS_FDF		BIT(7)
#define CAN_OBJ_FLAGS_ESI		BIT(8)
#define CAN_OBJ_FLAGS_SEQ_BITS		7
#define CAN_OBJ_FLAGS_SEQ_SHIFT		9
#define CAN_OBJ_FLAGS_SEQ_MASK					      \
	GENMASK(CAN_OBJ_FLAGS_SEQ_SHIFT + CAN_OBJ_FLAGS_SEQ_BITS - 1, \
		CAN_OBJ_FLAGS_SEQ_SHIFT)
#define CAN_OBJ_FLAGS_FILHIT_BITS	11
#define CAN_OBJ_FLAGS_FILHIT_SHIFT	5
#define CAN_OBJ_FLAGS_FILHIT_MASK				      \
	GENMASK(CAN_FLAGS_FILHIT_SHIFT + CAN_FLAGS_FILHIT_BITS - 1, \
		CAN_FLAGS_FILHIT_SHIFT)

#define CAN_OBJ_FLAGS_CUSTOM_ISTEF	BIT(31)

#define MCP25XXFD_BUFFER_TXRX_SIZE 2048

static const char * const mcp25xxfd_mode_names[] = {
	[CAN_CON_MODE_MIXED] = "can2.0+canfd",
	[CAN_CON_MODE_SLEEP] = "sleep",
	[CAN_CON_MODE_INTERNAL_LOOPBACK] = "internal loopback",
	[CAN_CON_MODE_LISTENONLY] = "listen only",
	[CAN_CON_MODE_CONFIG] = "config",
	[CAN_CON_MODE_EXTERNAL_LOOPBACK] = "external loopback",
	[CAN_CON_MODE_CAN2_0] = "can2.0",
	[CAN_CON_MODE_RESTRICTED] = "restricted"
};

struct mcp25xxfd_obj {
	u32 id;
	u32 flags;
};

struct mcp25xxfd_obj_tx {
	struct mcp25xxfd_obj header;
	u32 data[];
};

static void mcp25xxfd_obj_to_le(struct mcp25xxfd_obj *obj)
{
	obj->id = cpu_to_le32(obj->id);
	obj->flags = cpu_to_le32(obj->flags);
}

struct mcp25xxfd_obj_ts {
	u32 id;
	u32 flags;
	u32 ts;
};

struct mcp25xxfd_obj_tef {
	struct mcp25xxfd_obj_ts header;
};

struct mcp25xxfd_obj_rx {
	struct mcp25xxfd_obj_ts header;
	u8 data[];
};

static void mcp25xxfd_obj_ts_from_le(struct mcp25xxfd_obj_ts *obj)
{
	obj->id = le32_to_cpu(obj->id);
	obj->flags = le32_to_cpu(obj->flags);
	obj->ts = le32_to_cpu(obj->ts);
}

#define FIFO_DATA(x)			(0x400 + (x))
#define FIFO_DATA_SIZE			0x800

static const struct can_bittiming_const mcp25xxfd_nominal_bittiming_const = {
	.name		= DEVICE_NAME,
	.tseg1_min	= 2,
	.tseg1_max	= BIT(CAN_NBTCFG_TSEG1_BITS),
	.tseg2_min	= 1,
	.tseg2_max	= BIT(CAN_NBTCFG_TSEG2_BITS),
	.sjw_max	= BIT(CAN_NBTCFG_SJW_BITS),
	.brp_min	= 1,
	.brp_max	= BIT(CAN_NBTCFG_BRP_BITS),
	.brp_inc	= 1,
};

static const struct can_bittiming_const mcp25xxfd_data_bittiming_const = {
	.name		= DEVICE_NAME,
	.tseg1_min	= 1,
	.tseg1_max	= BIT(CAN_DBTCFG_TSEG1_BITS),
	.tseg2_min	= 1,
	.tseg2_max	= BIT(CAN_DBTCFG_TSEG2_BITS),
	.sjw_max	= BIT(CAN_DBTCFG_SJW_BITS),
	.brp_min	= 1,
	.brp_max	= BIT(CAN_DBTCFG_BRP_BITS),
	.brp_inc	= 1,
};

enum mcp25xxfd_model {
	CAN_MCP2517FD	= 0x2517,
};

enum mcp25xxfd_gpio_mode {
	gpio_mode_int		= 0,
	gpio_mode_standby	= MCP25XXFD_IOCON_XSTBYEN,
	gpio_mode_out_low	= MCP25XXFD_IOCON_PM0,
	gpio_mode_out_high	= MCP25XXFD_IOCON_PM0 | MCP25XXFD_IOCON_LAT0,
	gpio_mode_in		= MCP25XXFD_IOCON_PM0 | MCP25XXFD_IOCON_TRIS0
};

struct mcp25xxfd_trigger_tx_message {
	struct spi_message msg;
	struct spi_transfer fill_xfer;
	struct spi_transfer trigger_xfer;
	int fifo;
	char fill_cmd[2];
	char fill_obj[sizeof(struct mcp25xxfd_obj_tx)];
	char fill_data[64];
	char trigger_cmd[2];
	char trigger_data;
};

struct mcp25xxfd_read_fifo_info {
	struct mcp25xxfd_obj_ts *rxb[32];
	int rx_count;
};

struct mcp25xxfd_priv {
	struct can_priv	   can;
	struct net_device *net;
	struct spi_device *spi;
	struct regulator *power;
	struct regulator *transceiver;
	struct clk *clk;

	struct mutex clk_user_lock; /* lock for enabling/disabling the clock */
	int clk_user_mask;
#define MCP25XXFD_CLK_USER_CAN BIT(0)
#define MCP25XXFD_CLK_USER_GPIO0 BIT(1)
#define MCP25XXFD_CLK_USER_GPIO1 BIT(2)

	struct dentry *debugfs_dir;

#ifdef CONFIG_GPIOLIB
	struct gpio_chip gpio;
#endif

	/* the actual model of the mcp25xxfd */
	enum mcp25xxfd_model model;

	struct {
		/* clock configuration */
		bool clock_pll;
		bool clock_div2;
		int  clock_odiv;

		/* GPIO configuration */
		bool gpio_opendrain;
	} config;

	/* the distinct spi_speeds to use for spi communication */
	u32 spi_setup_speed_hz;
	u32 spi_speed_hz;

	/* fifo info */
	struct {
		/* define payload size and mode */
		int payload_size;
		u32 payload_mode;

		/* TEF addresses - start, end and current */
		u32 tef_fifos;
		u32 tef_address_start;
		u32 tef_address_end;
		u32 tef_address;

		/* address in mcp25xxfd-Fifo RAM of each fifo */
		u32 fifo_address[32];

		/* infos on tx-fifos */
		u32 tx_fifos;
		u32 tx_fifo_start;
		u32 tx_fifo_mask; /* bitmask of which fifo is a tx fifo */
		u32 tx_submitted_mask;
		u32 tx_pending_mask;
		u32 tx_pending_mask_in_irq;
		u32 tx_processed_mask;

		/* info on rx_fifos */
		u32 rx_fifos;
		u32 rx_fifo_depth;
		u32 rx_fifo_start;
		u32 rx_fifo_mask;  /* bitmask of which fifo is a rx fifo */

		/* memory image of FIFO RAM on mcp25xxfd */
		u8 fifo_data[MCP25XXFD_BUFFER_TXRX_SIZE];

	} fifos;

	/* structure with active fifos that need to get fed to the system */
	struct mcp25xxfd_read_fifo_info queued_fifos;

	/* statistics */
	struct {
		/* number of calls to the irq handler */
		u64 irq_calls;
		/* number of loops inside the irq handler */
		u64 irq_loops;

		/* interrupt handler state and statistics */
		u32 irq_state;
#define IRQ_STATE_NEVER_RUN 0
#define IRQ_STATE_RUNNING 1
#define IRQ_STATE_HANDLED 2
		/* stats on number of rx overflows */
		u64 rx_overflow;
		/* statistics of FIFO usage */
		u64 fifo_usage[32];

		/* message abort counter */
		u64 rx_mab;
		u64 tx_mab;

		/* message counter fd */
		u64 rx_fd_count;
		u64 tx_fd_count;

		/* message counter fd bit rate switch */
		u64 rx_brs_count;
		u64 tx_brs_count;

		/* interrupt counter */
		u64 int_ivm_count;
		u64 int_wake_count;
		u64 int_cerr_count;
		u64 int_serr_count;
		u64 int_rxov_count;
		u64 int_txat_count;
		u64 int_spicrc_count;
		u64 int_ecc_count;
		u64 int_tef_count;
		u64 int_mod_count;
		u64 int_tbc_count;
		u64 int_rx_count;
		u64 int_tx_count;

		/* dlc statistics */
		u64 rx_dlc_usage[16];
		u64 tx_dlc_usage[16];
	} stats;

	/* the current status of the mcp25xxfd */
	struct {
		u32 intf;
		/* ASSERT(CAN_INT + 4 == CAN_RXIF) */
		u32 rxif;
		/* ASSERT(CAN_RXIF + 4 == CAN_TXIF) */
		u32 txif;
		/* ASSERT(CAN_TXIF + 4 == CAN_RXOVIF) */
		u32 rxovif;
		/* ASSERT(CAN_RXOVIF + 4 == CAN_TXATIF) */
		u32 txatif;
		/* ASSERT(CAN_TXATIF + 4 == CAN_TXREQ) */
		u32 txreq;
		/* ASSERT(CAN_TXREQ + 4 == CAN_TREC) */
		u32 trec;
		/* ASSERT(CAN_TREC + 4 == CAN_BDIAG0) */
		u32 bdiag0;
		/* ASSERT(CAN_BDIAG0 + 4 == CAN_BDIAG1) */
		u32 bdiag1;
	} status;

	/* configuration registers */
	struct {
		u32 osc;
		u32 ecccon;
		u32 con;
		u32 iocon;
		u32 tdc;
		u32 tscon;
		u32 tefcon;
		u32 nbtcfg;
		u32 dbtcfg;
	} regs;

	/* interrupt handler signaling */
	int force_quit;
	int after_suspend;
#define AFTER_SUSPEND_UP 1
#define AFTER_SUSPEND_DOWN 2
#define AFTER_SUSPEND_POWER 4
#define AFTER_SUSPEND_RESTART 8
	int restart_tx;

	/* interrupt flags during irq handling */
	u32 bdiag1_clear_mask;
	u32 bdiag1_clear_value;

	/* composit error id and dataduring irq handling */
	u32 can_err_id;
	u32 can_err_data[8];

	/* the current mode */
	u32 active_can_mode;
	u32 new_state;

	/* status of the tx_queue enabled/disabled */
	u32 tx_queue_status;
#define TX_QUEUE_STATUS_INIT		0
#define TX_QUEUE_STATUS_RUNNING		1
#define TX_QUEUE_STATUS_NEEDS_START	2
#define TX_QUEUE_STATUS_STOPPED		3

	/* spi-tx/rx buffers for efficient transfers
	 * used during setup and irq
	 */
	struct mutex spi_rxtx_lock;
	u8 spi_tx[MCP25XXFD_BUFFER_TXRX_SIZE];
	u8 spi_rx[MCP25XXFD_BUFFER_TXRX_SIZE];

	/* structure for transmit fifo spi_messages */
	struct mcp25xxfd_trigger_tx_message *spi_transmit_fifos;
};

/* module parameters */
bool use_bulk_release_fifos;
bool use_complete_fdfifo_read;
unsigned int tx_fifos;
unsigned int bw_sharing_log2bits;
bool three_shot;

/* spi sync helper */

/* wrapper arround spi_sync, that sets speed_hz */
static int mcp25xxfd_sync_transfer(struct spi_device *spi,
				   struct spi_transfer *xfer,
				   unsigned int xfers,
				   int speed_hz)
{
	int i;

	for (i = 0; i < xfers; i++)
		xfer[i].speed_hz = speed_hz;

	return spi_sync_transfer(spi, xfer, xfers);
}

/* an optimization of spi_write_then_read that merges the transfers */
static int mcp25xxfd_write_then_read(struct spi_device *spi,
				     const void *tx_buf,
				     unsigned int tx_len,
				     void *rx_buf,
				     unsigned int rx_len,
				     int speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct spi_transfer xfer[2];
	u8 single_reg_data_tx[6];
	u8 single_reg_data_rx[6];
	int ret;

	memset(xfer, 0, sizeof(xfer));

	/* when using a halfduplex controller or to big for buffer */
	if ((spi->master->flags & SPI_MASTER_HALF_DUPLEX) ||
	    (tx_len + rx_len > sizeof(priv->spi_tx))) {
		xfer[0].tx_buf = tx_buf;
		xfer[0].len = tx_len;

		xfer[1].rx_buf = rx_buf;
		xfer[1].len = rx_len;

		return mcp25xxfd_sync_transfer(spi, xfer, 2, speed_hz);
	}

	/* full duplex optimization */
	xfer[0].len = tx_len + rx_len;
	if (xfer[0].len > sizeof(single_reg_data_tx)) {
		mutex_lock(&priv->spi_rxtx_lock);
		xfer[0].tx_buf = priv->spi_tx;
		xfer[0].rx_buf = priv->spi_rx;
	} else {
		xfer[0].tx_buf = single_reg_data_tx;
		xfer[0].rx_buf = single_reg_data_rx;
	}

	/* copy and clean */
	memcpy((u8 *)xfer[0].tx_buf, tx_buf, tx_len);
	memset((u8 *)xfer[0].tx_buf + tx_len, 0, rx_len);

	ret = mcp25xxfd_sync_transfer(spi, xfer, 1, speed_hz);
	if (!ret)
		memcpy(rx_buf, xfer[0].rx_buf + tx_len, rx_len);

	if (xfer[0].len > sizeof(single_reg_data_tx))
		mutex_unlock(&priv->spi_rxtx_lock);

	return ret;
}

/* simple spi_write wrapper with speed_hz */
static int mcp25xxfd_write(struct spi_device *spi,
			   const void *tx_buf,
			   unsigned int tx_len,
			   int speed_hz)
{
	struct spi_transfer xfer;

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = tx_buf;
	xfer.len = tx_len;

	return mcp25xxfd_sync_transfer(spi, &xfer, 1, speed_hz);
}

/* spi_sync wrapper similar to spi_write_then_read that optimizes transfers */
static int mcp25xxfd_write_then_write(struct spi_device *spi,
				      const void *tx_buf,
				      unsigned int tx_len,
				      const void *tx2_buf,
				      unsigned int tx2_len,
				      int speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct spi_transfer xfer;
	u8 single_reg_data[6];
	int ret;

	if (tx_len + tx2_len > MCP25XXFD_BUFFER_TXRX_SIZE)
		return -EINVAL;

	memset(&xfer, 0, sizeof(xfer));

	xfer.len = tx_len + tx2_len;
	if (xfer.len > sizeof(single_reg_data)) {
		mutex_lock(&priv->spi_rxtx_lock);
		xfer.tx_buf = priv->spi_tx;
	} else {
		xfer.tx_buf = single_reg_data;
	}

	memcpy((u8 *)xfer.tx_buf, tx_buf, tx_len);
	memcpy((u8 *)xfer.tx_buf + tx_len, tx2_buf, tx2_len);

	ret = mcp25xxfd_sync_transfer(spi, &xfer, 1, speed_hz);

	if (xfer.len > sizeof(single_reg_data))
		mutex_unlock(&priv->spi_rxtx_lock);

	return ret;
}

/* mcp25xxfd spi command/protocol helper */

static void mcp25xxfd_calc_cmd_addr(u16 cmd, u16 addr, u8 *data)
{
	cmd = cmd | (addr & ADDRESS_MASK);

	data[0] = (cmd >> 8) & 0xff;
	data[1] = (cmd >> 0) & 0xff;
}

static int mcp25xxfd_cmd_reset(struct spi_device *spi, u32 speed_hz)
{
	u8 cmd[2];

	mcp25xxfd_calc_cmd_addr(INSTRUCTION_RESET, 0, cmd);

	/* write the reset command */
	return mcp25xxfd_write(spi, cmd, 2, speed_hz);
}

/* read multiple bytes, transform some registers */
static int mcp25xxfd_cmd_readn(struct spi_device *spi, u32 reg,
			       void *data, int n, u32 speed_hz)
{
	u8 cmd[2];
	int ret;

	mcp25xxfd_calc_cmd_addr(INSTRUCTION_READ, reg, cmd);

	ret = mcp25xxfd_write_then_read(spi, &cmd, 2, data, n, speed_hz);
	if (ret)
		return ret;

	return 0;
}

static int mcp25xxfd_convert_to_cpu(u32 *data, int n)
{
	int i;

	for (i = 0; i < n; i++)
		data[i] = le32_to_cpu(data[i]);

	return 0;
}

static int mcp25xxfd_first_byte(u32 mask)
{
	return (mask & 0x0000ffff) ?
		((mask & 0x000000ff) ? 0 : 1) :
		((mask & 0x00ff0000) ? 2 : 3);
}

static int mcp25xxfd_last_byte(u32 mask)
{
	return (mask & 0xffff0000) ?
		((mask & 0xff000000) ? 3 : 2) :
		((mask & 0x0000ff00) ? 1 : 0);
}

/* read a register, but we are only interrested in a few bytes */
static int mcp25xxfd_cmd_read_mask(struct spi_device *spi, u32 reg,
				   u32 *data, u32 mask, u32 speed_hz)
{
	int first_byte, last_byte, len_byte;
	int ret;

	/* check that at least one bit is set */
	if (!mask)
		return -EINVAL;

	/* calculate first and last byte used */
	first_byte = mcp25xxfd_first_byte(mask);
	last_byte = mcp25xxfd_last_byte(mask);
	len_byte = last_byte - first_byte + 1;

	/* do a partial read */
	*data = 0;
	ret = mcp25xxfd_cmd_readn(spi, reg + first_byte,
				  ((void *)data + first_byte), len_byte,
				  speed_hz);
	if (ret)
		return ret;

	return mcp25xxfd_convert_to_cpu(data, 1);
}

static int mcp25xxfd_cmd_read(struct spi_device *spi, u32 reg, u32 *data,
			      u32 speed_hz)
{
	return mcp25xxfd_cmd_read_mask(spi, reg, data, -1, speed_hz);
}

/* read a register, but we are only interrested in a few bytes */
static int mcp25xxfd_cmd_write_mask(struct spi_device *spi, u32 reg,
				    u32 data, u32 mask, u32 speed_hz)
{
	int first_byte, last_byte, len_byte;
	u8 cmd[2];

	/* check that at least one bit is set */
	if (!mask)
		return -EINVAL;

	/* calculate first and last byte used */
	first_byte = mcp25xxfd_first_byte(mask);
	last_byte = mcp25xxfd_last_byte(mask);
	len_byte = last_byte - first_byte + 1;

	/* prepare buffer */
	mcp25xxfd_calc_cmd_addr(INSTRUCTION_WRITE, reg + first_byte, cmd);
	data = cpu_to_le32(data);

	return mcp25xxfd_write_then_write(spi,
					  cmd, sizeof(cmd),
					  ((void *)&data + first_byte),
					  len_byte,
					  speed_hz);
}

static int mcp25xxfd_cmd_write(struct spi_device *spi, u32 reg, u32 data,
			       u32 speed_hz)
{
	return mcp25xxfd_cmd_write_mask(spi, reg, data, -1, speed_hz);
}

static int mcp25xxfd_cmd_writen(struct spi_device *spi, u32 reg,
				void *data, int n, u32 speed_hz)
{
	u8 cmd[2];
	int ret;

	mcp25xxfd_calc_cmd_addr(INSTRUCTION_WRITE, reg, cmd);

	ret = mcp25xxfd_write_then_write(spi, &cmd, 2, data, n, speed_hz);
	if (ret)
		return ret;

	return 0;
}

static int mcp25xxfd_clean_sram(struct spi_device *spi, u32 speed_hz)
{
	u8 buffer[256];
	int i;
	int ret;

	memset(buffer, 0, sizeof(buffer));

	for (i = 0; i < FIFO_DATA_SIZE; i += sizeof(buffer)) {
		ret = mcp25xxfd_cmd_writen(spi, FIFO_DATA(i),
					   buffer, sizeof(buffer),
					   speed_hz);
		if (ret)
			return ret;
	}

	return 0;
}

/* mcp25xxfd opmode helper functions */

static int mcp25xxfd_get_opmode(struct spi_device *spi,
				int *mode,
				int speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int ret;

	/* read the mode */
	ret = mcp25xxfd_cmd_read_mask(spi,
				      CAN_CON,
				      &priv->regs.con,
				      CAN_CON_OPMOD_MASK,
				      speed_hz);
	if (ret)
		return ret;
	/* calculate the mode */
	*mode = (priv->regs.con & CAN_CON_OPMOD_MASK) >>
		CAN_CON_OPMOD_SHIFT;

	/* and assign to active mode as well */
	priv->active_can_mode = *mode;

	return 0;
}

static int mcp25xxfd_set_opmode(struct spi_device *spi, int mode,
				int speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 val = priv->regs.con & ~CAN_CON_REQOP_MASK;

	/* regs.con also contains the effective register */
	priv->regs.con = val |
		(mode << CAN_CON_REQOP_SHIFT) |
		(mode << CAN_CON_OPMOD_SHIFT);
	priv->active_can_mode = mode;

	/* if the opmode is sleep then the oscilator will be disabled
	 * and also not ready
	 */
	if (mode == CAN_CON_MODE_SLEEP) {
		priv->regs.osc &= ~(MCP25XXFD_OSC_OSCRDY |
				    MCP25XXFD_OSC_PLLRDY |
				    MCP25XXFD_OSC_SCLKRDY);
		priv->regs.osc |= MCP25XXFD_OSC_OSCDIS;
	}

	/* but only write the relevant section */
	return mcp25xxfd_cmd_write_mask(spi, CAN_CON,
					priv->regs.con,
					CAN_CON_REQOP_MASK,
					speed_hz);
}

static int mcp25xxfd_set_normal_opmode(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int mode;

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		mode = CAN_CON_MODE_EXTERNAL_LOOPBACK;
	else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		mode = CAN_CON_MODE_LISTENONLY;
	else if (priv->can.ctrlmode & CAN_CTRLMODE_FD)
		mode = CAN_CON_MODE_MIXED;
	else
		mode = CAN_CON_MODE_CAN2_0;

	return mcp25xxfd_set_opmode(spi, mode, priv->spi_setup_speed_hz);
}

/* clock helper */
static int mcp25xxfd_wake_from_sleep(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 waitfor = MCP25XXFD_OSC_OSCRDY;
	u32 mask = waitfor | MCP25XXFD_OSC_OSCDIS;
	unsigned long timeout;
	int ret;

	/* write clock with OSCDIS cleared*/
	priv->regs.osc &= ~MCP25XXFD_OSC_OSCDIS;
	ret = mcp25xxfd_cmd_write(spi, MCP25XXFD_OSC,
				  priv->regs.osc,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* wait for synced pll/osc/sclk */
	timeout = jiffies + MCP25XXFD_OSC_POLLING_JIFFIES;
	while (time_before_eq(jiffies, timeout)) {
		ret = mcp25xxfd_cmd_read(spi, MCP25XXFD_OSC,
					 &priv->regs.osc,
					 priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		if ((priv->regs.osc & mask) == waitfor) {
			priv->active_can_mode = CAN_CON_MODE_CONFIG;
			return 0;
		}
		/* wait some time */
		msleep(100);
	}

	dev_err(&spi->dev,
		"Clock did not enable within the timeout period\n");
	return -ETIMEDOUT;
}

static int mcp25xxfd_hw_check_clock(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 val;
	int ret;

	/* read the osc register and check if it matches
	 * what we have on record
	 */
	ret = mcp25xxfd_cmd_read(spi, MCP25XXFD_OSC,
				 &val,
				 priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	if (val == priv->regs.osc)
		return 0;

	/* ignore all those ready bits on second try */
	if ((val & 0xff) == (priv->regs.osc & 0xff)) {
		dev_info(&spi->dev,
			 "The oscillator register value %08x does not match what we expect: %08x - it is still reasonable, but please investigate\n",
			val, priv->regs.osc);
		return 0;
	}

	dev_err(&spi->dev,
		"The oscillator register value %08x does not match what we expect: %08x\n",
		val, priv->regs.osc);

	return -ENODEV;
}

static int mcp25xxfd_start_clock(struct spi_device *spi, int requestor_mask)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int ret = 0;

	mutex_lock(&priv->clk_user_lock);

	priv->clk_user_mask |= requestor_mask;

	if (priv->clk_user_mask != requestor_mask)
		goto out;

	/* check that the controller clock register
	 * is what it is supposed to be
	 */
	ret = mcp25xxfd_hw_check_clock(spi);
	if (ret)
		goto out;

	/* and we start the clock */
	if (!IS_ERR(priv->clk))
		ret = clk_prepare_enable(priv->clk);

	/* we wake from sleep */
	if (priv->active_can_mode == CAN_CON_MODE_SLEEP)
		ret = mcp25xxfd_wake_from_sleep(spi);

out:
	mutex_unlock(&priv->clk_user_lock);

	return ret;
}

static int mcp25xxfd_stop_clock(struct spi_device *spi, int requestor_mask)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	mutex_lock(&priv->clk_user_lock);

	priv->clk_user_mask &= ~requestor_mask;

	if (!priv->clk_user_mask)
		goto out;

	/* put us into sleep mode */
	mcp25xxfd_set_opmode(spi, CAN_CON_MODE_SLEEP,
			     priv->spi_setup_speed_hz);

	/* and we stop the clock */
	if (!IS_ERR(priv->clk))
		clk_disable_unprepare(priv->clk);

out:
	mutex_unlock(&priv->clk_user_lock);

	return 0;
}

/* mcp25xxfd GPIO helper functions */
#ifdef CONFIG_GPIOLIB

enum mcp25xxfd_gpio_pins {
	MCP25XXFD_GPIO_GPIO0 = 0,
	MCP25XXFD_GPIO_GPIO1 = 1,
};

static int mcp25xxfd_gpio_request(struct gpio_chip *chip,
				  unsigned int offset)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	int clock_requestor = offset ?
		MCP25XXFD_CLK_USER_GPIO1 : MCP25XXFD_CLK_USER_GPIO0;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return -EINVAL;

	mcp25xxfd_start_clock(priv->spi, clock_requestor);

	return 0;
}

static void mcp25xxfd_gpio_free(struct gpio_chip *chip,
				unsigned int offset)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	int clock_requestor = offset ?
		MCP25XXFD_CLK_USER_GPIO1 : MCP25XXFD_CLK_USER_GPIO0;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return;

	mcp25xxfd_stop_clock(priv->spi, clock_requestor);
}

static int mcp25xxfd_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	u32 mask = (offset) ? MCP25XXFD_IOCON_GPIO1 : MCP25XXFD_IOCON_GPIO0;
	int ret;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return -EINVAL;

	/* read the relevant gpio Latch */
	ret = mcp25xxfd_cmd_read_mask(priv->spi, MCP25XXFD_IOCON,
				      &priv->regs.iocon, mask,
				      priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* return the match */
	return priv->regs.iocon & mask;
}

static void mcp25xxfd_gpio_set(struct gpio_chip *chip, unsigned int offset,
			       int value)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	u32 mask = (offset) ? MCP25XXFD_IOCON_LAT1 : MCP25XXFD_IOCON_LAT0;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return;

	/* update in memory representation with the corresponding value */
	if (value)
		priv->regs.iocon |= mask;
	else
		priv->regs.iocon &= ~mask;

	mcp25xxfd_cmd_write_mask(priv->spi, MCP25XXFD_IOCON,
				 priv->regs.iocon, mask,
				 priv->spi_setup_speed_hz);
}

static int mcp25xxfd_gpio_direction_input(struct gpio_chip *chip,
					  unsigned int offset)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	u32 mask_tri = (offset) ?
		MCP25XXFD_IOCON_TRIS1 : MCP25XXFD_IOCON_TRIS0;
	u32 mask_stby = (offset) ?
		0 : MCP25XXFD_IOCON_XSTBYEN;
	u32 mask_pm = (offset) ?
		MCP25XXFD_IOCON_PM1 : MCP25XXFD_IOCON_PM0;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return -EINVAL;

	/* set the mask */
	priv->regs.iocon |= mask_tri | mask_pm;

	/* clear stby */
	priv->regs.iocon &= ~mask_stby;

	return mcp25xxfd_cmd_write_mask(priv->spi, MCP25XXFD_IOCON,
					priv->regs.iocon,
					mask_tri | mask_stby | mask_pm,
					priv->spi_setup_speed_hz);
}

static int mcp25xxfd_gpio_direction_output(struct gpio_chip *chip,
					   unsigned int offset, int value)
{
	struct mcp25xxfd_priv *priv = gpiochip_get_data(chip);
	u32 mask_tri = (offset) ?
		MCP25XXFD_IOCON_TRIS1 : MCP25XXFD_IOCON_TRIS0;
	u32 mask_lat = (offset) ?
		MCP25XXFD_IOCON_LAT1 : MCP25XXFD_IOCON_LAT0;
	u32 mask_pm = (offset) ?
		MCP25XXFD_IOCON_PM1 : MCP25XXFD_IOCON_PM0;
	u32 mask_stby = (offset) ?
		0 : MCP25XXFD_IOCON_XSTBYEN;

	/* only handle gpio 0/1 */
	if (offset > 1)
		return -EINVAL;

	/* clear the tristate bit and also clear stby */
	priv->regs.iocon &= ~(mask_tri | mask_stby);

	/* set GPIO mode */
	priv->regs.iocon |= mask_pm;

	/* set the value */
	if (value)
		priv->regs.iocon |= mask_lat;
	else
		priv->regs.iocon &= ~mask_lat;

	return mcp25xxfd_cmd_write_mask(priv->spi, MCP25XXFD_IOCON,
					priv->regs.iocon,
					mask_tri | mask_lat |
					mask_pm | mask_stby,
					priv->spi_setup_speed_hz);
}

static int mcp25xxfd_gpio_setup(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* gpiochip only handles GPIO0 and GPIO1 */
	priv->gpio.owner		= THIS_MODULE;
	priv->gpio.parent		= &spi->dev;
	priv->gpio.label		= dev_name(&spi->dev);
	priv->gpio.direction_input	= mcp25xxfd_gpio_direction_input;
	priv->gpio.get			= mcp25xxfd_gpio_get;
	priv->gpio.direction_output	= mcp25xxfd_gpio_direction_output;
	priv->gpio.set			= mcp25xxfd_gpio_set;
	priv->gpio.request		= mcp25xxfd_gpio_request;
	priv->gpio.free			= mcp25xxfd_gpio_free;
	priv->gpio.base			= -1;
	priv->gpio.ngpio		= 2;
	priv->gpio.can_sleep		= 1;

	return devm_gpiochip_add_data(&spi->dev, &priv->gpio, priv);
}

#else

static int mcp25xxfd_gpio_setup(struct spi_device *spi)
{
	return 0;
}

#endif

/* ideally these would be defined in uapi/linux/can.h */
#define CAN_EFF_SID_SHIFT		(CAN_EFF_ID_BITS - CAN_SFF_ID_BITS)
#define CAN_EFF_SID_BITS		CAN_SFF_ID_BITS
#define CAN_EFF_SID_MASK				      \
	GENMASK(CAN_EFF_SID_SHIFT + CAN_EFF_SID_BITS - 1,     \
		CAN_EFF_SID_SHIFT)
#define CAN_EFF_EID_SHIFT		0
#define CAN_EFF_EID_BITS		CAN_EFF_SID_SHIFT
#define CAN_EFF_EID_MASK				      \
	GENMASK(CAN_EFF_EID_SHIFT + CAN_EFF_EID_BITS - 1,     \
		CAN_EFF_EID_SHIFT)

static void mcp25xxfd_canid_to_mcpid(u32 can_id, u32 *id, u32 *flags)
{
	if (can_id & CAN_EFF_FLAG) {
		int sid = (can_id & CAN_EFF_SID_MASK) >> CAN_EFF_SID_SHIFT;
		int eid = (can_id & CAN_EFF_EID_MASK) >> CAN_EFF_EID_SHIFT;
		*id = (eid << CAN_OBJ_ID_EID_SHIFT) |
			(sid << CAN_OBJ_ID_SID_SHIFT);
		*flags = CAN_OBJ_FLAGS_IDE;
	} else {
		*id = can_id & CAN_SFF_MASK;
		*flags = 0;
	}

	*flags |= (can_id & CAN_RTR_FLAG) ? CAN_OBJ_FLAGS_RTR : 0;
}

static void mcp25xxfd_mcpid_to_canid(u32 mcpid, u32 mcpflags, u32 *id)
{
	u32 sid = (mcpid & CAN_OBJ_ID_SID_MASK) >> CAN_OBJ_ID_SID_SHIFT;
	u32 eid = (mcpid & CAN_OBJ_ID_EID_MASK) >> CAN_OBJ_ID_EID_SHIFT;

	if (mcpflags & CAN_OBJ_FLAGS_IDE) {
		*id = (eid << CAN_EFF_EID_SHIFT) |
			(sid << CAN_EFF_SID_SHIFT) |
			CAN_EFF_FLAG;
	} else {
		*id = sid;
	}

	*id |= (mcpflags & CAN_OBJ_FLAGS_RTR) ? CAN_RTR_FLAG : 0;
}

static void __mcp25xxfd_stop_queue(struct net_device *net,
				   unsigned int id)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);

	if (priv->tx_queue_status >= TX_QUEUE_STATUS_STOPPED)
		dev_warn(&priv->spi->dev,
			 "tx-queue is already stopped by: %i\n",
			 priv->tx_queue_status);

	priv->tx_queue_status = id ? id : TX_QUEUE_STATUS_STOPPED;
	netif_stop_queue(priv->net);
}

/* helper to identify who is stopping the queue by line number */
#define mcp25xxfd_stop_queue(spi) \
	__mcp25xxfd_stop_queue(spi, __LINE__)

static void mcp25xxfd_wake_queue(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* nothing should be left pending /in flight now... */
	priv->fifos.tx_pending_mask = 0;
	priv->fifos.tx_submitted_mask = 0;
	priv->fifos.tx_processed_mask = 0;
	priv->tx_queue_status = TX_QUEUE_STATUS_RUNNING;

	/* wake queue now */
	netif_wake_queue(priv->net);
}

/* CAN transmit related*/

static void mcp25xxfd_mark_tx_pending(void *context)
{
	struct mcp25xxfd_trigger_tx_message *txm = context;
	struct spi_device *spi = txm->msg.spi;
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* only here or in the irq handler this value is changed,
	 * so there is no race condition and it does not require locking
	 * serialization happens via spi_pump_message
	 */
	priv->fifos.tx_pending_mask |= BIT(txm->fifo);
}

static int mcp25xxfd_fill_spi_transmit_fifos(struct mcp25xxfd_priv *priv)
{
	struct mcp25xxfd_trigger_tx_message *txm;
	int i, fifo;
	const u32 trigger = CAN_FIFOCON_TXREQ | CAN_FIFOCON_UINC;
	const int first_byte = mcp25xxfd_first_byte(trigger);
	u32 fifo_address;

	priv->spi_transmit_fifos = kcalloc(priv->fifos.tx_fifos,
					   sizeof(*priv->spi_transmit_fifos),
					   GFP_KERNEL | GFP_DMA);
	if (!priv->spi_transmit_fifos)
		return -ENOMEM;

	for (i = 0; i < priv->fifos.tx_fifos; i++) {
		fifo = priv->fifos.tx_fifo_start + i;
		txm = &priv->spi_transmit_fifos[i];
		fifo_address = priv->fifos.fifo_address[fifo];
		/* prepare the message */
		spi_message_init(&txm->msg);
		txm->msg.complete = mcp25xxfd_mark_tx_pending;
		txm->msg.context = txm;
		txm->fifo = fifo;
		/* the payload itself */
		txm->fill_xfer.speed_hz = priv->spi_speed_hz;
		txm->fill_xfer.tx_buf = txm->fill_cmd;
		txm->fill_xfer.len = 2;
		txm->fill_xfer.cs_change = true;
		mcp25xxfd_calc_cmd_addr(INSTRUCTION_WRITE,
					FIFO_DATA(fifo_address),
					txm->fill_cmd);
		spi_message_add_tail(&txm->fill_xfer, &txm->msg);
		/* the trigger command */
		txm->trigger_xfer.speed_hz = priv->spi_speed_hz;
		txm->trigger_xfer.tx_buf = txm->trigger_cmd;
		txm->trigger_xfer.len = 3;
		mcp25xxfd_calc_cmd_addr(INSTRUCTION_WRITE,
					CAN_FIFOCON(fifo) + first_byte,
					txm->trigger_cmd);
		txm->trigger_data = trigger >> (8 * first_byte);
		spi_message_add_tail(&txm->trigger_xfer, &txm->msg);
	}

	return 0;
}

static int mcp25xxfd_transmit_message_common(struct spi_device *spi,
					     int fifo,
					     struct mcp25xxfd_obj_tx *obj,
					     int len,
					     u8 *data)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_trigger_tx_message *txm =
		&priv->spi_transmit_fifos[fifo - priv->fifos.tx_fifo_start];
	int ret;

	/* add fifo as seq */
	obj->header.flags |= fifo << CAN_OBJ_FLAGS_SEQ_SHIFT;

	/* transform to le32 */
	mcp25xxfd_obj_to_le(&obj->header);

	/* fill in details */
	memcpy(txm->fill_obj, obj, sizeof(struct mcp25xxfd_obj_tx));
	memset(txm->fill_data, 0, priv->fifos.payload_size);
	memcpy(txm->fill_data, data, len);

	/* transfers to FIFO RAM has to be multiple of 4 */
	txm->fill_xfer.len =
		2 + sizeof(struct mcp25xxfd_obj_tx) + ALIGN(len, 4);

	/* and transmit asyncroniously */
	ret = spi_async(spi, &txm->msg);
	if (ret)
		return NETDEV_TX_BUSY;

	return NETDEV_TX_OK;
}

static int mcp25xxfd_transmit_fdmessage(struct spi_device *spi, int fifo,
					struct canfd_frame *frame)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_obj_tx obj;
	int dlc = can_len2dlc(frame->len);
	u32 flags;

	frame->len = can_dlc2len(dlc);

	mcp25xxfd_canid_to_mcpid(frame->can_id, &obj.header.id, &flags);

	flags |= dlc << CAN_OBJ_FLAGS_DLC_SHIFT;
	flags |= (frame->can_id & CAN_EFF_FLAG) ? CAN_OBJ_FLAGS_IDE : 0;
	flags |= (frame->can_id & CAN_RTR_FLAG) ? CAN_OBJ_FLAGS_RTR : 0;
	if (frame->flags & CANFD_BRS) {
		flags |= CAN_OBJ_FLAGS_BRS;
		priv->stats.tx_brs_count++;
	}
	flags |= (frame->flags & CANFD_ESI) ? CAN_OBJ_FLAGS_ESI : 0;
	flags |= CAN_OBJ_FLAGS_FDF;

	priv->stats.tx_fd_count++;
	priv->stats.tx_dlc_usage[dlc]++;

	obj.header.flags = flags;

	return mcp25xxfd_transmit_message_common(spi, fifo, &obj,
						 frame->len, frame->data);
}

static int mcp25xxfd_transmit_message(struct spi_device *spi, int fifo,
				      struct can_frame *frame)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_obj_tx obj;
	u32 flags;

	if (frame->can_dlc > 8)
		frame->can_dlc = 8;

	priv->stats.tx_dlc_usage[frame->can_dlc]++;

	mcp25xxfd_canid_to_mcpid(frame->can_id, &obj.header.id, &flags);

	flags |= frame->can_dlc << CAN_OBJ_FLAGS_DLC_SHIFT;
	flags |= (frame->can_id & CAN_EFF_FLAG) ? CAN_OBJ_FLAGS_IDE : 0;
	flags |= (frame->can_id & CAN_RTR_FLAG) ? CAN_OBJ_FLAGS_RTR : 0;

	obj.header.flags = flags;

	return mcp25xxfd_transmit_message_common(spi, fifo, &obj,
						 frame->can_dlc, frame->data);
}

static bool mcp25xxfd_is_last_txfifo(struct spi_device *spi,
				     int fifo)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	return (fifo ==
		(priv->fifos.tx_fifo_start + priv->fifos.tx_fifos - 1));
}

static netdev_tx_t mcp25xxfd_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;
	u32 pending_mask;
	int fifo;
	int ret;

	if (can_dropped_invalid_skb(net, skb))
		return NETDEV_TX_OK;

	if (priv->can.state == CAN_STATE_BUS_OFF) {
		mcp25xxfd_stop_queue(priv->net);
		return NETDEV_TX_BUSY;
	}

	/* get effective mask */
	pending_mask = priv->fifos.tx_pending_mask |
		priv->fifos.tx_submitted_mask;

	/* decide on fifo to assign */
	if (pending_mask)
		fifo = fls(pending_mask);
	else
		fifo = priv->fifos.tx_fifo_start;

	/* handle error - this should not happen... */
	if (fifo >= priv->fifos.tx_fifo_start + priv->fifos.tx_fifos) {
		dev_err(&spi->dev,
			"reached tx-fifo %i, which is not valid\n",
			fifo);
		return NETDEV_TX_BUSY;
	}

	/* if we are the last one, then stop the queue */
	if (mcp25xxfd_is_last_txfifo(spi, fifo))
		mcp25xxfd_stop_queue(priv->net);

	/* mark as submitted */
	priv->fifos.tx_submitted_mask |= BIT(fifo);
	priv->stats.fifo_usage[fifo]++;

	/* now process it for real */
	if (can_is_canfd_skb(skb))
		ret = mcp25xxfd_transmit_fdmessage(spi, fifo,
						   (struct canfd_frame *)
						   skb->data);
	else
		ret = mcp25xxfd_transmit_message(spi, fifo,
						 (struct can_frame *)
						 skb->data);

	/* keep it for reference until the message really got transmitted */
	if (ret == NETDEV_TX_OK)
		can_put_echo_skb(skb, priv->net, fifo);

	return ret;
}

/* CAN RX Related */

static int mcp25xxfd_can_transform_rx_fd(struct spi_device *spi,
					 struct mcp25xxfd_obj_rx *rx)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct canfd_frame *frame;
	struct sk_buff *skb;
	u32 flags = rx->header.flags;
	int dlc;

	/* allocate the skb buffer */
	skb = alloc_canfd_skb(priv->net, &frame);
	if (!skb) {
		dev_err(&spi->dev, "cannot allocate RX skb\n");
		priv->net->stats.rx_dropped++;
		return -ENOMEM;
	}

	mcp25xxfd_mcpid_to_canid(rx->header.id, flags, &frame->can_id);
	frame->flags |= (flags & CAN_OBJ_FLAGS_BRS) ? CANFD_BRS : 0;
	frame->flags |= (flags & CAN_OBJ_FLAGS_ESI) ? CANFD_ESI : 0;

	dlc = (flags & CAN_OBJ_FLAGS_DLC_MASK) >> CAN_OBJ_FLAGS_DLC_SHIFT;
	if (dlc > 15)
		dlc = 15;

	frame->len = can_dlc2len(dlc);

	memcpy(frame->data, rx->data, frame->len);

	priv->stats.rx_fd_count++;
	priv->net->stats.rx_packets++;
	priv->net->stats.rx_bytes += frame->len;
	if (rx->header.flags & CAN_OBJ_FLAGS_BRS)
		priv->stats.rx_brs_count++;
	priv->stats.rx_dlc_usage[dlc]++;

	can_led_event(priv->net, CAN_LED_EVENT_RX);

	netif_rx_ni(skb);

	return 0;
}

static int mcp25xxfd_can_transform_rx_normal(struct spi_device *spi,
					     struct mcp25xxfd_obj_rx *rx)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct sk_buff *skb;
	struct can_frame *frame;
	u32 flags = rx->header.flags;
	int len;
	int dlc;

	/* allocate the skb buffer */
	skb = alloc_can_skb(priv->net, &frame);
	if (!skb) {
		dev_err(&spi->dev, "cannot allocate RX skb\n");
		priv->net->stats.rx_dropped++;
		return -ENOMEM;
	}

	mcp25xxfd_mcpid_to_canid(rx->header.id, flags, &frame->can_id);

	dlc = (flags & CAN_OBJ_FLAGS_DLC_MASK) >> CAN_OBJ_FLAGS_DLC_SHIFT;
	if (dlc > 15)
		dlc = 15;

	frame->can_dlc = dlc;

	len = can_dlc2len(frame->can_dlc);

	memcpy(frame->data, rx->data, len);

	priv->net->stats.rx_packets++;
	priv->net->stats.rx_bytes += len;
	priv->stats.rx_dlc_usage[dlc]++;

	can_led_event(priv->net, CAN_LED_EVENT_RX);

	netif_rx_ni(skb);

	return 0;
}

static int mcp25xxfd_process_queued_rx(struct spi_device *spi,
				       struct mcp25xxfd_obj_ts *obj)
{
	struct mcp25xxfd_obj_rx *rx = container_of(obj,
						   struct mcp25xxfd_obj_rx,
						   header);

	if (obj->flags & CAN_OBJ_FLAGS_FDF)
		return mcp25xxfd_can_transform_rx_fd(spi, rx);
	else
		return mcp25xxfd_can_transform_rx_normal(spi, rx);
}

static int mcp25xxfd_normal_release_fifos(struct spi_device *spi,
					  int start, int end)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int ret;

	/* release each fifo in a separate transfer */
	for (; start < end ; start++) {
		ret = mcp25xxfd_cmd_write_mask(spi, CAN_FIFOCON(start),
					       CAN_FIFOCON_UINC,
					       CAN_FIFOCON_UINC,
					       priv->spi_speed_hz);
		if (ret)
			return ret;
	}

	return 0;
}

/* unfortunately the CAN_FIFOCON are not directly consecutive
 * so the optimization of "clearing all in one spi_transfer"
 * would produce an overhead of 11 unnecessary bytes/fifo
 * - transferring 14 (2 cmd + 12 data) bytes
 * instead of just 3 (2 + 1).
 * On some slower systems this may still be beneficial,
 * but it is not good enough for the generic case.
 * On a Raspberry Pi CM the timings for clearing 3 fifos
 * (at 12.5MHz SPI clock speed) are:
 * * normal:
 *   * 3 spi transfers
 *   * 9 bytes total
 *   * 36.74us from first CS low to last CS high
 *   * individual CS: 9.14us, 5.74us and 5.16us
 *   * 77.02us from CS up of fifo transfer to last release CS up
 * * bulk:
 *   * 1 spi transfer
 *   * 27 bytes total
 *   * 29.06us CS Low
 *   * 78.28us from CS up of fifo transfer to last release CS up
 * this obviously varies with SPI_clock speed
 * - the slower the clock the less efficient the optimization.
 * similarly the faster the CPU (and bigger the code cache) the
 * less effcient the optimization - the above case is border line.
 */

#define FIFOCON_SPACING (CAN_FIFOCON(1) - CAN_FIFOCON(0))
#define FIFOCON_SPACINGW (FIFOCON_SPACING / sizeof(u32))

static int mcp25xxfd_bulk_release_fifos(struct spi_device *spi,
					int start, int end)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int i;
	int ret;

	/* calculate start address and length */
	int fifos = end - start;
	int first_byte = mcp25xxfd_first_byte(CAN_FIFOCON_UINC);
	int addr = CAN_FIFOCON(start);
	int len = 1 + (fifos - 1) * FIFOCON_SPACING;

	/* the worsted case buffer */
	u32 buf[32 * FIFOCON_SPACINGW], base;

	base = (priv->fifos.payload_mode << CAN_FIFOCON_PLSIZE_SHIFT) |
		((priv->fifos.rx_fifo_depth - 1) << CAN_FIFOCON_FSIZE_SHIFT) |
		CAN_FIFOCON_RXTSEN | /* RX timestamps */
		CAN_FIFOCON_UINC |
		CAN_FIFOCON_TFERFFIE | /* FIFO Full */
		CAN_FIFOCON_TFHRFHIE | /* FIFO Half Full*/
		CAN_FIFOCON_TFNRFNIE; /* FIFO not empty */

	memset(buf, 0, sizeof(buf));
	for (i = 0; i < end - start ; i++) {
		if (i == priv->fifos.rx_fifos - 1)
			base |= CAN_FIFOCON_RXOVIE;
		buf[FIFOCON_SPACINGW * i] = cpu_to_le32(base);
	}

	ret = mcp25xxfd_cmd_writen(spi, addr + first_byte,
				   (u8 *)buf + first_byte,
				   len,
				   priv->spi_speed_hz);
	if (ret)
		return ret;

	return 0;
}

/* queued FIFO handling for release to system */

static void mcp25xxfd_clear_queued_fifos(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* prepare rfi - mostly used for sorting */
	priv->queued_fifos.rx_count = 0;
}

static void mcp25xxfd_addto_queued_fifos(struct spi_device *spi,
					 struct mcp25xxfd_obj_ts *obj)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_read_fifo_info *rfi = &priv->queued_fifos;

	/* timestamps must ignore the highest byte, so we shift it,
	 * so that it still compares correctly
	 */
	obj->ts <<= 8;

	/* add pointer to queued array-list */
	rfi->rxb[rfi->rx_count] = obj;
	rfi->rx_count++;
}

static int mcp25xxfd_process_queued_tef(struct spi_device *spi,
					struct mcp25xxfd_obj_ts *obj)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_obj_tef *tef = container_of(obj,
						     struct mcp25xxfd_obj_tef,
						     header);
	int dlc = (obj->flags & CAN_OBJ_FLAGS_DLC_MASK)
		>> CAN_OBJ_FLAGS_DLC_SHIFT;
	int fifo = (tef->header.flags & CAN_OBJ_FLAGS_SEQ_MASK) >>
		CAN_OBJ_FLAGS_SEQ_SHIFT;

	if (dlc > 15)
		dlc = 15;

	/* update counters */
	priv->net->stats.tx_packets++;
	priv->net->stats.tx_bytes += can_dlc2len(dlc);
	if (obj->flags & CAN_OBJ_FLAGS_FDF)
		priv->stats.tx_fd_count++;
	if (obj->flags & CAN_OBJ_FLAGS_BRS)
		priv->stats.tx_brs_count++;
	priv->stats.tx_dlc_usage[dlc]++;

	/* release it */
	can_get_echo_skb(priv->net, fifo);

	can_led_event(priv->net, CAN_LED_EVENT_TX);

	return 0;
}

static int mcp25xxfd_compare_obj_ts(const void *a, const void *b)
{
	const struct mcp25xxfd_obj_ts * const *rxa = a;
	const struct mcp25xxfd_obj_ts * const *rxb = b;
	/* using signed here to handle rollover correctly */
	s32 ats = (*rxa)->ts;
	s32 bts = (*rxb)->ts;

	if (ats < bts)
		return -EINVAL;
	if (ats > bts)
		return 1;
	return 0;
}

static int mcp25xxfd_process_queued_fifos(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_read_fifo_info *rfi = &priv->queued_fifos;
	int i;
	int ret;

	/* sort the fifos (rx and TEF) by receive timestamp */
	sort(rfi->rxb, rfi->rx_count, sizeof(struct mcp25xxfd_obj_ts *),
	     mcp25xxfd_compare_obj_ts, NULL);

	/* process the recived fifos */
	for (i = 0; i < rfi->rx_count ; i++) {
		if (rfi->rxb[i]->flags & CAN_OBJ_FLAGS_CUSTOM_ISTEF)
			ret = mcp25xxfd_process_queued_tef(spi, rfi->rxb[i]);
		else
			ret = mcp25xxfd_process_queued_rx(spi, rfi->rxb[i]);
		if (ret)
			return ret;
	}

	/* clear queued fifos */
	mcp25xxfd_clear_queued_fifos(spi);

	return 0;
}

static int mcp25xxfd_transform_rx(struct spi_device *spi,
				  struct mcp25xxfd_obj_rx *rx)
{
	int dlc;

	/* transform the data to system byte order */
	mcp25xxfd_obj_ts_from_le(&rx->header);

	/* add the object to the list */
	mcp25xxfd_addto_queued_fifos(spi, &rx->header);

	/* calc length and return it */
	dlc = (rx->header.flags & CAN_OBJ_FLAGS_DLC_MASK)
		>> CAN_OBJ_FLAGS_DLC_SHIFT;
	return can_dlc2len(dlc);
}

/* read_fifo implementations
 *
 * read_fifos is a simple implementation, that:
 *   * loops all fifos
 *     * read header + some data-bytes (8)
 *     * read rest of data-bytes bytes
 *     * release fifo
 *   for 3 can frames dlc<=8 to read here we have:
 *     * 6 spi transfers
 *     * 75 bytes (= 3 * (2 + 12 + 8) bytes + 3 * 3 bytes)
 *   for 3 canfd frames dlc>8 to read here we have:
 *     * 9 spi transfers
 *     * 81 (= 3 * (2 + 12 + 8 + 2) bytes + 3 * 3 bytes) + 3 * extra payload
 *     this only transfers the required size of bytes on the spi bus.
 *
 * bulk_read_fifos is an optimization that is most practical for
 * Can2.0 busses, but may also be practical for CanFD busses that
 * have a high average payload data size.
 *
 * It will read all of the fifo data in a single spi_transfer:
 *   * read all fifos in one go (as long as these are ajacent to each other)
 *   * loop all fifos
 *     * release fifo
 *   for 3 can2.0 frames to read here we have:
 *     * 4 spi transfers
 *     * 71 bytes (= 2 + 3 * (12 + 8) bytes + 3 * 3 bytes)
 *   for 3 canfd frames to read here we have:
 *     * 4 spi transfers
 *     * 230 bytes (= 2 + 3 * (12 + 64) bytes)
 *     obviously this reads way too many bytes for framesizes <=32 bytes,
 *     but it avoids the overhead on the CPU side and may even trigger
 *     DMA transfers due to the high byte count, which release CPU cycles.
 *
 * This optimization will also be efficient for cases where a high
 * percentage of canFD frames has a dlc-size > 8.
 * This mode is used for Can2.0 configured busses.
 *
 * For now this option can get forced for CanFD via a module parameter.
 * In the future there may be some heuristics that could trigger a usage
 * of this mode as well in some circumstances.
 *
 * Note: there is a second optimization for release fifo as well,
 *       but it is not as efficient as this optimization for the
 *       non-CanFD case - see mcp25xxfd_bulk_release_fifos
 */

static int mcp25xxfd_read_fifos(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int fifo_header_size = sizeof(struct mcp25xxfd_obj_rx);
	int fifo_min_payload_size = 8;
	int fifo_min_size = fifo_header_size + fifo_min_payload_size;
	int fifo_max_payload_size =
		((priv->can.ctrlmode & CAN_CTRLMODE_FD) ? 64 : 8);
	u32 mask = priv->status.rxif;
	struct mcp25xxfd_obj_rx *rx;
	int i, len;
	int ret;
	u32 fifo_address;
	u8 *data;

	/* read all the "open" segments in big chunks */
	for (i = priv->fifos.rx_fifo_start + priv->fifos.rx_fifos - 1;
	     i >= priv->fifos.rx_fifo_start;
	     i--) {
		if (!(mask & BIT(i)))
			continue;
		/* the fifo to fill */
		rx = (struct mcp25xxfd_obj_rx *)
			(priv->fifos.fifo_data + priv->fifos.fifo_address[i]);
		/* read the minimal payload */
		fifo_address = priv->fifos.fifo_address[i];
		ret = mcp25xxfd_cmd_readn(spi,
					  FIFO_DATA(fifo_address),
					  rx,
					  fifo_min_size,
					  priv->spi_speed_hz);
		if (ret)
			return ret;
		/* process fifo stats and get length */
		len = min_t(int, mcp25xxfd_transform_rx(spi, rx),
			    fifo_max_payload_size);

		/* read extra payload if needed */
		if (len > fifo_min_payload_size) {
			data = &rx->data[fifo_min_payload_size];
			ret = mcp25xxfd_cmd_readn(spi,
						  FIFO_DATA(fifo_address +
							    fifo_min_size),
						  data,
						  len - fifo_min_payload_size,
						  priv->spi_speed_hz);
			if (ret)
				return ret;
		}
		/* release fifo */
		ret = mcp25xxfd_normal_release_fifos(spi, i, i + 1);
		if (ret)
			return ret;
		/* increment fifo_usage */
		priv->stats.fifo_usage[i]++;
	}

	return 0;
}

static int mcp25xxfd_bulk_read_fifo_range(struct spi_device *spi,
					  int start, int end)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	const int fifo_header_size = sizeof(struct mcp25xxfd_obj_rx);
	const int fifo_max_payload_size = priv->fifos.payload_size;
	const int fifo_max_size = fifo_header_size + fifo_max_payload_size;
	struct mcp25xxfd_obj_rx *rx;
	int i;
	int ret;

	/* now we got start and end, so read the range */
	ret = mcp25xxfd_cmd_readn(spi,
				  FIFO_DATA(priv->fifos.fifo_address[start]),
				  priv->fifos.fifo_data +
				  priv->fifos.fifo_address[start],
				  (end - start) * fifo_max_size,
				  priv->spi_speed_hz);
	if (ret)
		return ret;

	/* clear all the fifos in range */
	if (use_bulk_release_fifos)
		ret = mcp25xxfd_bulk_release_fifos(spi, start, end);
	else
		ret = mcp25xxfd_normal_release_fifos(spi, start, end);
	if (ret)
		return ret;

	/* preprocess data */
	for (i = start; i < end ; i++) {
		/* store the fifo to process */
		rx = (struct mcp25xxfd_obj_rx *)
			(priv->fifos.fifo_data + priv->fifos.fifo_address[i]);
		/* process fifo stats */
		mcp25xxfd_transform_rx(spi, rx);
		/* increment usage */
		priv->stats.fifo_usage[i]++;
	}

	return 0;
}

static int mcp25xxfd_bulk_read_fifos(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 mask = priv->status.rxif;
	int i, start, end;
	int ret;

	/* find blocks of set bits top down */
	for (i = priv->fifos.rx_fifo_start + priv->fifos.rx_fifos - 1;
	     mask && (i >= priv->fifos.rx_fifo_start);
	     i--) {
		/* if the bit is 0 then continue loop to find a 1 */
		if ((mask & BIT(i)) == 0)
			continue;

		/* so we found a non-0 bit - this is start and end */
		start = i;
		end = i;

		/* find the first bit set */
		for (; mask & BIT(i); i--) {
			mask &= ~BIT(i);
			start = i;
		}

		/* now process that range */
		ret = mcp25xxfd_bulk_read_fifo_range(spi, start, end + 1);
		if (ret)
			return ret;
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_rxif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 mask = priv->status.rxif;
	int ret;

	if (!mask)
		return 0;

	/* read all the fifos - for non-fd case use bulk read optimization */
	if (((priv->can.ctrlmode & CAN_CTRLMODE_FD) == 0) ||
	    use_complete_fdfifo_read)
		ret = mcp25xxfd_bulk_read_fifos(spi);
	else
		ret = mcp25xxfd_read_fifos(spi);

	return 0;
}

static void mcp25xxfd_mark_tx_processed(struct spi_device *spi,
					int fifo)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* set mask */
	priv->fifos.tx_processed_mask |= BIT(fifo);

	/* check if we should reenable the TX-queue */
	if (mcp25xxfd_is_last_txfifo(spi, fifo))
		priv->tx_queue_status = TX_QUEUE_STATUS_NEEDS_START;
}

static int mcp25xxfd_can_ist_handle_tefif_handle_single(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct mcp25xxfd_obj_tef *tef;
	int fifo;
	int ret;

	/* calc address in address space */
	tef = (struct mcp25xxfd_obj_tef *)(priv->fifos.fifo_data +
					   priv->fifos.tef_address);

	/* read all the object data */
	ret = mcp25xxfd_cmd_readn(spi,
				  FIFO_DATA(priv->fifos.tef_address),
				  tef,
				  /* we do not read the last byte of the ts
				   * to avoid MAB issiues
				   */
				  sizeof(*tef) - 1,
				  priv->spi_speed_hz);
	/* increment the counter to read next */
	ret = mcp25xxfd_cmd_write_mask(spi,
				       CAN_TEFCON,
				       CAN_TEFCON_UINC,
				       CAN_TEFCON_UINC,
				       priv->spi_speed_hz);

	/* transform the data to system byte order */
	mcp25xxfd_obj_ts_from_le(&tef->header);

	fifo = (tef->header.flags & CAN_OBJ_FLAGS_SEQ_MASK) >>
		CAN_OBJ_FLAGS_SEQ_SHIFT;

	/* submit to queue */
	tef->header.flags |= CAN_OBJ_FLAGS_CUSTOM_ISTEF;
	mcp25xxfd_addto_queued_fifos(spi, &tef->header);

	/* increment tef_address with rollover */
	priv->fifos.tef_address += sizeof(*tef);
	if (priv->fifos.tef_address > priv->fifos.tef_address_end)
		priv->fifos.tef_address =
			priv->fifos.tef_address_start;

	/* and mark as processed right now */
	mcp25xxfd_mark_tx_processed(spi, fifo);

	return 0;
}

static int mcp25xxfd_can_ist_handle_tefif_conservative(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 val[2];
	int ret;

	while (1) {
		/* get the current TEFSTA and TEFUA */
		ret = mcp25xxfd_cmd_readn(priv->spi,
					  CAN_TEFSTA,
					  val,
					  8,
					  priv->spi_speed_hz);
		if (ret)
			return ret;
		mcp25xxfd_convert_to_cpu(val, 2);

		/* check for interrupt flags */
		if (!(val[0] & CAN_TEFSTA_TEFNEIF))
			return 0;

		if (priv->fifos.tef_address != val[1]) {
			dev_err(&spi->dev,
				"TEF Address mismatch - read: %04x calculated: %04x\n",
				val[1], priv->fifos.tef_address);
			priv->fifos.tef_address = val[1];
		}

		ret = mcp25xxfd_can_ist_handle_tefif_handle_single(spi);
		if (ret)
			return ret;
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_tefif_count(struct spi_device *spi,
						int count)
{
	int i;
	int ret;

	/* now clear TEF for each */
	/* TODO: optimize for BULK reads, as we (hopefully) know COUNT */
	for (i = 0; i < count; i++) {
		/* handle a single TEF */
		ret = mcp25xxfd_can_ist_handle_tefif_handle_single(spi);
		if (ret)
			return ret;
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_tefif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 pending = priv->fifos.tx_pending_mask_in_irq &
		(~priv->fifos.tx_processed_mask);
	int count;

	/* calculate the number of fifos that have been processed */
	count = hweight_long(pending);
	count -= hweight_long(priv->status.txreq & pending);

	/* in case of unexpected results handle "safely" */
	if (count <= 0)
		return mcp25xxfd_can_ist_handle_tefif_conservative(spi);

	return mcp25xxfd_can_ist_handle_tefif_count(spi, count);
}

static int mcp25xxfd_can_ist_handle_txatif_fifo(struct spi_device *spi,
						int fifo)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 val;
	int ret;

	/* read fifo status */
	ret = mcp25xxfd_cmd_read(spi,
				 CAN_FIFOSTA(fifo),
				 &val,
				 priv->spi_speed_hz);
	if (ret)
		return ret;

	/* clear the relevant interrupt flags */
	ret = mcp25xxfd_cmd_write_mask(spi,
				       CAN_FIFOSTA(fifo),
				       0,
				       CAN_FIFOSTA_TXABT |
				       CAN_FIFOSTA_TXLARB |
				       CAN_FIFOSTA_TXERR |
				       CAN_FIFOSTA_TXATIF,
				       priv->spi_speed_hz);

	/* for specific cases we could trigger a retransmit
	 * instead of an abort.
	 */

	/* and we release it from the echo_skb buffer
	 * NOTE: this is one place where packet delivery will not
	 * be ordered, as we do not have any timing information
	 * when this occurred
	 */
	can_get_echo_skb(priv->net, fifo);

	/* but we need to run a bit of cleanup */
	priv->status.txif &= ~BIT(fifo);
	priv->net->stats.tx_aborted_errors++;

	/* mark the fifo as processed */
	 mcp25xxfd_mark_tx_processed(spi, fifo);

	/* handle all the known cases accordingly - ignoring FIFO full */
	val &= CAN_FIFOSTA_TXABT |
		CAN_FIFOSTA_TXLARB |
		CAN_FIFOSTA_TXERR;
	switch (val) {
	case CAN_FIFOSTA_TXERR:
		break;
	default:
		dev_warn_ratelimited(&spi->dev,
				     "Unknown TX-Fifo abort condition: %08x - stopping tx-queue\n",
				     val);
		break;
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_txatif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int i, fifo;
	int ret;

	/* process all the fifos with that flag set */
	for (i = 0, fifo = priv->fifos.tx_fifo_start;
	    i < priv->fifos.tx_fifos; i++, fifo++) {
		if (priv->status.txatif & BIT(fifo)) {
			ret = mcp25xxfd_can_ist_handle_txatif_fifo(spi, fifo);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static void mcp25xxfd_error_skb(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct sk_buff *skb;
	struct can_frame *frame;

	skb = alloc_can_err_skb(net, &frame);
	if (skb) {
		frame->can_id = priv->can_err_id;
		memcpy(frame->data, priv->can_err_data, 8);
		netif_rx_ni(skb);
	} else {
		netdev_err(net, "cannot allocate error skb\n");
	}
}

static int mcp25xxfd_can_ist_handle_rxovif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 mask = priv->status.rxovif;
	int i;
	int ret;

	/* clear all fifos that have an overflow bit set */
	for (i = 0; i < 32; i++) {
		if (mask & BIT(i)) {
			ret = mcp25xxfd_cmd_write_mask(spi,
						       CAN_FIFOSTA(i),
						       0,
						       CAN_FIFOSTA_RXOVIF,
						       priv->spi_speed_hz);
			if (ret)
				return ret;
			/* update statistics */
			priv->net->stats.rx_over_errors++;
			priv->net->stats.rx_errors++;
			priv->stats.rx_overflow++;
			priv->can_err_id |= CAN_ERR_CRTL;
			priv->can_err_data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
		}
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_modif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int omode = priv->active_can_mode;
	int mode;
	int ret;

	/* Note that this irq does not get triggered in all situations
	 * for example SERRIF will move to RESTICTED or LISTENONLY
	 * but MODIF will not be raised!
	 */

	/* get the mode */
	ret = mcp25xxfd_get_opmode(spi, &mode, priv->spi_speed_hz);
	if (ret)
		return ret;

	/* if we are restricted, then return to "normal" mode */
	if (mode == CAN_CON_MODE_RESTRICTED)
		return mcp25xxfd_set_normal_opmode(spi);

	/* the controller itself will transition to sleep, so we ignore it */
	if (mode == CAN_CON_MODE_SLEEP)
		return 0;

	/* switches to the same mode as before are also ignored
	 * - this typically happens if the driver is shortly
	 *   switching to a different mode and then returning to the
	 *   original mode
	 */
	if (mode == omode)
		return 0;

	/* these we need to handle correctly, so warn and give context */
	dev_warn(&spi->dev,
		 "Controller unexpectedly switched from mode %s(%u) to %s(%u)\n",
		 mcp25xxfd_mode_names[omode], omode,
		 mcp25xxfd_mode_names[mode], mode);

	return 0;
}

static int mcp25xxfd_can_ist_handle_cerrif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* in principle we could also delay reading bdiag registers
	 * until we get here - it would add some extra delay in the
	 * error case, but be slightly faster in the "normal" case.
	 * slightly faster would be saving 8 bytes of spi transfer.
	 */

	dev_err_ratelimited(&spi->dev, "CAN Bus error\n");
	priv->can_err_id |= CAN_ERR_BUSERROR;

	if (priv->status.bdiag1 &
	    (CAN_BDIAG1_DBIT0ERR | CAN_BDIAG1_NBIT0ERR)) {
		priv->can_err_id |= CAN_ERR_BUSERROR;
		priv->can_err_data[2] |= CAN_ERR_PROT_BIT0;
		priv->bdiag1_clear_mask |= CAN_BDIAG1_DBIT0ERR |
			CAN_BDIAG1_NBIT0ERR;
	}
	if (priv->status.bdiag1 &
	    (CAN_BDIAG1_DBIT1ERR | CAN_BDIAG1_NBIT1ERR)) {
		priv->can_err_id |= CAN_ERR_BUSERROR;
		priv->can_err_data[2] |= CAN_ERR_PROT_BIT1;
		priv->bdiag1_clear_mask |= CAN_BDIAG1_DBIT1ERR |
			CAN_BDIAG1_NBIT1ERR;
	}
	if (priv->status.bdiag1 &
	    (CAN_BDIAG1_DSTUFERR | CAN_BDIAG1_NSTUFERR)) {
		priv->can_err_id |= CAN_ERR_BUSERROR;
		priv->can_err_data[2] |= CAN_ERR_PROT_STUFF;
		priv->bdiag1_clear_mask |= CAN_BDIAG1_DSTUFERR |
			CAN_BDIAG1_NSTUFERR;
	}
	if (priv->status.bdiag1 &
	    (CAN_BDIAG1_DFORMERR | CAN_BDIAG1_NFORMERR)) {
		priv->can_err_id |= CAN_ERR_BUSERROR;
		priv->can_err_data[2] |= CAN_ERR_PROT_FORM;
		priv->bdiag1_clear_mask |= CAN_BDIAG1_DFORMERR |
			CAN_BDIAG1_NFORMERR;
	}
	if (priv->status.bdiag1 & CAN_BDIAG1_NACKERR) {
		priv->can_err_id |= CAN_ERR_ACK;
		priv->bdiag1_clear_mask |= CAN_BDIAG1_NACKERR;
	}

	return 0;
}

static int mcp25xxfd_can_ist_handle_eccif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int ret;
	u32 val;
	u32 addr;

	priv->can_err_id |= CAN_ERR_CRTL;
	priv->can_err_data[1] |= CAN_ERR_CRTL_UNSPEC;

	/* read ECC status register */
	ret = mcp25xxfd_cmd_read(spi, MCP25XXFD_ECCSTAT, &val,
				 priv->spi_speed_hz);
	if (ret)
		return ret;

	addr = (val & MCP25XXFD_ECCSTAT_ERRADDR_MASK) >>
		MCP25XXFD_ECCSTAT_ERRADDR_SHIFT;

	dev_err_ratelimited(&spi->dev,
			    "ECC %s bit error at %03x\n",
			    (val & MCP25XXFD_ECCSTAT_DEDIF) ?
			    "double" : "single",
			    addr);

	return mcp25xxfd_cmd_write(spi, MCP25XXFD_ECCSTAT, 0,
				 priv->spi_speed_hz);
}

static int mcp25xxfd_can_ist_handle_serrif_txmab(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	priv->net->stats.tx_fifo_errors++;
	priv->net->stats.tx_errors++;
	priv->stats.tx_mab++;

	return 0;
}

static int mcp25xxfd_can_ist_handle_serrif_rxmab(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	priv->net->stats.rx_dropped++;
	priv->net->stats.rx_errors++;
	priv->stats.rx_mab++;

	return 0;
}

static int mcp25xxfd_can_ist_handle_serrif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 clear;
	int ret;

	/* clear some interrupts immediately,
	 * so that we get notified if they happen again
	 */
	clear = CAN_INT_SERRIF | CAN_INT_MODIF | CAN_INT_IVMIF;
	ret = mcp25xxfd_cmd_write_mask(spi, CAN_INT,
				       priv->status.intf & (~clear),
				       clear,
				       priv->spi_speed_hz);
	if (ret)
		return ret;

	/* Errors here are:
	 * * Bus Bandwidth Error: when a RX Message Assembly Buffer
	 *   is still full when the next message has already arrived
	 *   the recived message shall be ignored
	 * * TX MAB Underflow: when a TX Message is invalid
	 *   due to ECC errors or TXMAB underflow
	 *   in this situatioon the system will transition to
	 *   Restricted or Listen Only mode
	 */

	priv->can_err_id |= CAN_ERR_CRTL;
	priv->can_err_data[1] |= CAN_ERR_CRTL_UNSPEC;

	/* a mode change + invalid message would indicate
	 * TX MAB Underflow
	 */
	if ((priv->status.intf & CAN_INT_MODIF) &&
	    (priv->status.intf & CAN_INT_IVMIF)) {
		return mcp25xxfd_can_ist_handle_serrif_txmab(spi);
	}

	/* for RX there is only the RXIF an indicator
	 * - surprizingly RX-MAB does not change mode or anything
	 */
	if (priv->status.intf & CAN_INT_RXIF)
		return mcp25xxfd_can_ist_handle_serrif_rxmab(spi);

	/* the final case */
	dev_warn_ratelimited(&spi->dev,
			     "unidentified system error - intf =  %08x\n",
			     priv->status.intf);

	return 0;
}

static int mcp25xxfd_can_ist_handle_ivmif(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* if we have a systemerror as well, then ignore it */
	if (priv->status.intf & CAN_INT_SERRIF)
		return 0;

	/* otherwise it is an RX issue, so account for it here */
	priv->can_err_id |= CAN_ERR_PROT;
	priv->can_err_data[2] |= CAN_ERR_PROT_FORM;
	priv->net->stats.rx_frame_errors++;
	priv->net->stats.rx_errors++;

	return 0;
}

static int mcp25xxfd_disable_interrupts(struct spi_device *spi,
					u32 speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	priv->status.intf = 0;
	return mcp25xxfd_cmd_write(spi, CAN_INT, 0, speed_hz);
}

static int mcp25xxfd_enable_interrupts(struct spi_device *spi,
				       u32 speed_hz)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	priv->status.intf = CAN_INT_TEFIE |
		CAN_INT_RXIE |
		CAN_INT_MODIE |
		CAN_INT_SERRIE |
		CAN_INT_IVMIE |
		CAN_INT_CERRIE |
		CAN_INT_RXOVIE |
		CAN_INT_ECCIE;
	return mcp25xxfd_cmd_write(spi, CAN_INT,
				   priv->status.intf,
				   speed_hz);
}

static int mcp25xxfd_hw_wake(struct spi_device *spi)
{
	return mcp25xxfd_start_clock(spi, MCP25XXFD_CLK_USER_CAN);
}

static void mcp25xxfd_hw_sleep(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);

	/* disable interrupts */
	mcp25xxfd_disable_interrupts(spi, priv->spi_setup_speed_hz);

	/* stop the clocks */
	mcp25xxfd_stop_clock(spi, MCP25XXFD_CLK_USER_CAN);
}

static int mcp25xxfd_can_ist_handle_status(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	const u32 clear_irq = CAN_INT_TBCIF |
		CAN_INT_MODIF |
		CAN_INT_SERRIF |
		CAN_INT_CERRIF |
		CAN_INT_WAKIF |
		CAN_INT_IVMIF;
	int ret;

	/* clear all the interrupts asap */
	ret = mcp25xxfd_cmd_write_mask(spi, CAN_INT,
				       priv->status.intf & (~clear_irq),
				       clear_irq,
				       priv->spi_speed_hz);
	if (ret)
		return ret;

	/* interrupt clearing info */
	priv->bdiag1_clear_value = 0;
	priv->bdiag1_clear_mask = 0;
	priv->can_err_id = 0;
	memset(priv->can_err_data, 0, 8);

	/* state changes */
	priv->new_state = priv->can.state;

	/* clear queued fifos */
	mcp25xxfd_clear_queued_fifos(spi);

	/* system error interrupt needs to get handled first
	 * to get us out of restricted mode
	 */
	if (priv->status.intf & CAN_INT_SERRIF) {
		priv->stats.int_serr_count++;
		ret = mcp25xxfd_can_ist_handle_serrif(spi);
		if (ret)
			return ret;
	}

	/* mode change interrupt */
	if (priv->status.intf & CAN_INT_MODIF) {
		priv->stats.int_mod_count++;
		ret = mcp25xxfd_can_ist_handle_modif(spi);
		if (ret)
			return ret;
	}

	/* handle the rx */
	if (priv->status.intf & CAN_INT_RXIF) {
		priv->stats.int_rx_count++;
		ret = mcp25xxfd_can_ist_handle_rxif(spi);
		if (ret)
			return ret;
	}

	/* handle aborted TX FIFOs */
	if (priv->status.txatif) {
		priv->stats.int_txat_count++;
		ret = mcp25xxfd_can_ist_handle_txatif(spi);
		if (ret)
			return ret;
	}

	/* handle the tef */
	if (priv->status.intf & CAN_INT_TEFIF) {
		priv->stats.int_tef_count++;
		ret = mcp25xxfd_can_ist_handle_tefif(spi);
		if (ret)
			return ret;
	}

	/* process the queued fifos */
	ret = mcp25xxfd_process_queued_fifos(spi);

	/* handle error interrupt flags */
	if (priv->status.rxovif) {
		priv->stats.int_rxov_count++;
		ret = mcp25xxfd_can_ist_handle_rxovif(spi);
		if (ret)
			return ret;
	}

	/* sram ECC error interrupt */
	if (priv->status.intf & CAN_INT_ECCIF) {
		priv->stats.int_ecc_count++;
		ret = mcp25xxfd_can_ist_handle_eccif(spi);
		if (ret)
			return ret;
	}

	/* message format interrupt */
	if (priv->status.intf & CAN_INT_IVMIF) {
		priv->stats.int_ivm_count++;
		ret = mcp25xxfd_can_ist_handle_ivmif(spi);
		if (ret)
			return ret;
	}

	/* handle bus errors in more detail */
	if (priv->status.intf & CAN_INT_CERRIF) {
		priv->stats.int_cerr_count++;
		ret = mcp25xxfd_can_ist_handle_cerrif(spi);
		if (ret)
			return ret;
	}

	/* Error counter handling */
	if (priv->status.trec & CAN_TREC_TXWARN) {
		priv->new_state = CAN_STATE_ERROR_WARNING;
		priv->can_err_id |= CAN_ERR_CRTL;
		priv->can_err_data[1] |= CAN_ERR_CRTL_TX_WARNING;
	}
	if (priv->status.trec & CAN_TREC_RXWARN) {
		priv->new_state = CAN_STATE_ERROR_WARNING;
		priv->can_err_id |= CAN_ERR_CRTL;
		priv->can_err_data[1] |= CAN_ERR_CRTL_RX_WARNING;
	}
	if (priv->status.trec & CAN_TREC_TXBP) {
		priv->new_state = CAN_STATE_ERROR_PASSIVE;
		priv->can_err_id |= CAN_ERR_CRTL;
		priv->can_err_data[1] |= CAN_ERR_CRTL_TX_PASSIVE;
	}
	if (priv->status.trec & CAN_TREC_RXBP) {
		priv->new_state = CAN_STATE_ERROR_PASSIVE;
		priv->can_err_id |= CAN_ERR_CRTL;
		priv->can_err_data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
	}
	if (priv->status.trec & CAN_TREC_TXBO) {
		priv->new_state = CAN_STATE_BUS_OFF;
		priv->can_err_id |= CAN_ERR_BUSOFF;
	}

	/* based on the last state state check the new state */
	switch (priv->can.state) {
	case CAN_STATE_ERROR_ACTIVE:
		if (priv->new_state >= CAN_STATE_ERROR_WARNING &&
		    priv->new_state <= CAN_STATE_BUS_OFF)
			priv->can.can_stats.error_warning++;
		/* fallthrough */
	case CAN_STATE_ERROR_WARNING:
		if (priv->new_state >= CAN_STATE_ERROR_PASSIVE &&
		    priv->new_state <= CAN_STATE_BUS_OFF)
			priv->can.can_stats.error_passive++;
		break;
	default:
		break;
	}
	priv->can.state = priv->new_state;

	/* and send error packet */
	if (priv->can_err_id)
		mcp25xxfd_error_skb(priv->net);

	/* handle BUS OFF */
	if (priv->can.state == CAN_STATE_BUS_OFF) {
		if (priv->can.restart_ms == 0) {
			mcp25xxfd_stop_queue(priv->net);
			priv->force_quit = 1;
			priv->can.can_stats.bus_off++;
			can_bus_off(priv->net);
			mcp25xxfd_hw_sleep(spi);
		}
	} else {
		/* restart the tx queue if needed */
		if (priv->fifos.tx_processed_mask == priv->fifos.tx_fifo_mask)
			mcp25xxfd_wake_queue(spi);
	}

	/* clear bdiag flags */
	if (priv->bdiag1_clear_mask) {
		ret = mcp25xxfd_cmd_write_mask(spi,
					       CAN_BDIAG1,
					       priv->bdiag1_clear_value,
					       priv->bdiag1_clear_mask,
					       priv->spi_speed_hz);
		if (ret)
			return ret;
	}

	return 0;
}

static irqreturn_t mcp25xxfd_can_ist(int irq, void *dev_id)
{
	struct mcp25xxfd_priv *priv = dev_id;
	struct spi_device *spi = priv->spi;
	int ret;

	priv->stats.irq_calls++;
	priv->stats.irq_state = IRQ_STATE_RUNNING;

	while (!priv->force_quit) {
		/* count irq loops */
		priv->stats.irq_loops++;

		/* copy pending to in_irq - any
		 * updates that happen asyncronously
		 * are not taken into account here
		 */
		priv->fifos.tx_pending_mask_in_irq =
			priv->fifos.tx_pending_mask;

		/* read interrupt status flags */
		ret = mcp25xxfd_cmd_readn(spi, CAN_INT,
					  &priv->status,
					  sizeof(priv->status),
					  priv->spi_speed_hz);
		if (ret)
			return ret;

		/* only act if the mask is applied */
		if ((priv->status.intf &
		     (priv->status.intf >> CAN_INT_IE_SHIFT)) == 0)
			break;

		/* handle the status */
		ret = mcp25xxfd_can_ist_handle_status(spi);
		if (ret)
			return ret;
	}

	return IRQ_HANDLED;
}

static int mcp25xxfd_get_berr_counter(const struct net_device *net,
				      struct can_berr_counter *bec)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);

	bec->txerr = (priv->status.trec & CAN_TREC_TEC_MASK) >>
		CAN_TREC_TEC_SHIFT;
	bec->rxerr = (priv->status.trec & CAN_TREC_REC_MASK) >>
		CAN_TREC_REC_SHIFT;

	return 0;
}

static int mcp25xxfd_power_enable(struct regulator *reg, int enable)
{
	return 0;
}

static int mcp25xxfd_do_set_mode(struct net_device *net, enum can_mode mode)
{
	switch (mode) {
	case CAN_MODE_START:
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int mcp25xxfd_do_set_nominal_bittiming(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct can_bittiming *bt = &priv->can.bittiming;
	struct spi_device *spi = priv->spi;

	int sjw = bt->sjw;
	int pseg2 = bt->phase_seg2;
	int pseg1 = bt->phase_seg1;
	int propseg = bt->prop_seg;
	int brp = bt->brp;

	int tseg1 = propseg + pseg1;
	int tseg2 = pseg2;

	/* calculate nominal bit timing */
	priv->regs.nbtcfg = ((sjw - 1) << CAN_NBTCFG_SJW_SHIFT) |
		((tseg2 - 1) << CAN_NBTCFG_TSEG2_SHIFT) |
		((tseg1 - 1) << CAN_NBTCFG_TSEG1_SHIFT) |
		((brp - 1) << CAN_NBTCFG_BRP_SHIFT);

	return mcp25xxfd_cmd_write(spi, CAN_NBTCFG,
				   priv->regs.nbtcfg,
				   priv->spi_setup_speed_hz);
}

static int mcp25xxfd_do_set_data_bittiming(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct can_bittiming *bt = &priv->can.data_bittiming;
	struct spi_device *spi = priv->spi;

	int sjw = bt->sjw;
	int pseg2 = bt->phase_seg2;
	int pseg1 = bt->phase_seg1;
	int propseg = bt->prop_seg;
	int brp = bt->brp;

	int tseg1 = propseg + pseg1;
	int tseg2 = pseg2;

	int ret;

	/* set up Transmitter delay compensation */
	if (!priv->regs.tdc)
		priv->regs.tdc = CAN_TDC_EDGFLTEN |
			(CAN_TDC_TDCMOD_AUTO << CAN_TDC_TDCMOD_SHIFT);
	ret = mcp25xxfd_cmd_write(spi, CAN_TDC, priv->regs.tdc,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* calculate nominal bit timing */
	priv->regs.dbtcfg = ((sjw - 1) << CAN_DBTCFG_SJW_SHIFT) |
		((tseg2 - 1) << CAN_DBTCFG_TSEG2_SHIFT) |
		((tseg1 - 1) << CAN_DBTCFG_TSEG1_SHIFT) |
		((brp - 1) << CAN_DBTCFG_BRP_SHIFT);

	return mcp25xxfd_cmd_write(spi, CAN_DBTCFG,
				   priv->regs.dbtcfg,
				   priv->spi_setup_speed_hz);
}

static int mcp25xxfd_hw_probe(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int ret;

	/* Wait for oscillator startup timer after power up */
	msleep(MCP25XXFD_OST_DELAY_MS);

	/* send a "blind" reset, hoping we are in Config mode */
	mcp25xxfd_cmd_reset(spi, priv->spi_setup_speed_hz);

	/* Wait for oscillator startup again */
	msleep(MCP25XXFD_OST_DELAY_MS);

	/* check clock register that the clock is ready or disabled */
	ret = mcp25xxfd_cmd_read(spi, MCP25XXFD_OSC,
				 &priv->regs.osc,
				 priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* there can only be one... */
	switch (priv->regs.osc &
		(MCP25XXFD_OSC_OSCRDY | MCP25XXFD_OSC_OSCDIS)) {
	case MCP25XXFD_OSC_OSCRDY: /* either the clock is ready */
		break;
	case MCP25XXFD_OSC_OSCDIS: /* or the clock is disabled */
		/* wakeup sleeping system */
		ret = mcp25xxfd_wake_from_sleep(spi);
		if (ret)
			return ret;
		/* send a reset, hoping we are now in Config mode */
		mcp25xxfd_cmd_reset(spi, priv->spi_setup_speed_hz);

		/* Wait for oscillator startup again */
		msleep(MCP25XXFD_OST_DELAY_MS);
		break;
	default:
		/* otherwise there is no valid device (or in strange state)
		 *
		 * if PLL is enabled but not ready, then there may be
		 * something "fishy"
		 * this happened during driver development
		 * (enabling pll, when when on wrong clock), so best warn
		 * about such a possibility
		 */
		if ((priv->regs.osc &
		     (MCP25XXFD_OSC_PLLEN | MCP25XXFD_OSC_PLLRDY))
		    == MCP25XXFD_OSC_PLLEN)
			dev_err(&spi->dev,
				"mcp25xxfd may be in a strange state - a power disconnect may be required\n");

		return -ENODEV;
	}

	/* check if we are in config mode already*/

	/* read CON register and match */
	ret = mcp25xxfd_cmd_read(spi, CAN_CON,
				 &priv->regs.con,
				 priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* apply mask and check */
	if ((priv->regs.con & CAN_CON_DEFAULT_MASK) == CAN_CON_DEFAULT) {
		priv->active_can_mode = CAN_CON_MODE_CONFIG;
		return 0;
	}

	/* as per datasheet a reset only works in Config Mode
	 * so as we have in principle no knowledge of the current
	 * mode that the controller is in we have no safe way
	 * to detect the device correctly
	 * hence we need to "blindly" put the controller into
	 * config mode.
	 * on the "save" side, the OSC reg has to be valid already,
	 * so there is a chance we got the controller...
	 */

	/* blindly force it into config mode */
	priv->regs.con = CAN_CON_DEFAULT;
	priv->active_can_mode = CAN_CON_MODE_CONFIG;
	ret = mcp25xxfd_cmd_write(spi, CAN_CON, CAN_CON_DEFAULT,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* delay some time */
	msleep(MCP25XXFD_OST_DELAY_MS);

	/* reset can controller */
	mcp25xxfd_cmd_reset(spi, priv->spi_setup_speed_hz);

	/* delay some time */
	msleep(MCP25XXFD_OST_DELAY_MS);

	/* read CON register and match a final time */
	ret = mcp25xxfd_cmd_read(spi, CAN_CON,
				 &priv->regs.con,
				 priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* apply mask and check */
	if ((priv->regs.con & CAN_CON_DEFAULT_MASK) != CAN_CON_DEFAULT)
		return -ENODEV;

	/* just in case: disable interrupts on controller */
	return mcp25xxfd_disable_interrupts(spi,
					    priv->spi_setup_speed_hz);
}

static int mcp25xxfd_setup_osc(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	int val = ((priv->config.clock_pll) ? MCP25XXFD_OSC_PLLEN : 0)
		| ((priv->config.clock_div2) ? MCP25XXFD_OSC_SCLKDIV : 0);
	int waitfor = ((priv->config.clock_pll) ? MCP25XXFD_OSC_PLLRDY : 0)
		| ((priv->config.clock_div2) ? MCP25XXFD_OSC_SCLKRDY : 0)
		| MCP25XXFD_OSC_OSCRDY;
	int ret;
	unsigned long timeout;

	/* manage clock_out divider */
	switch (priv->config.clock_odiv) {
	case 10:
		val |= (MCP25XXFD_OSC_CLKODIV_10)
			<< MCP25XXFD_OSC_CLKODIV_SHIFT;
		break;
	case 4:
		val |= (MCP25XXFD_OSC_CLKODIV_4)
			<< MCP25XXFD_OSC_CLKODIV_SHIFT;
		break;
	case 2:
		val |= (MCP25XXFD_OSC_CLKODIV_2)
			<< MCP25XXFD_OSC_CLKODIV_SHIFT;
		break;
	case 1:
		val |= (MCP25XXFD_OSC_CLKODIV_1)
			<< MCP25XXFD_OSC_CLKODIV_SHIFT;
		break;
	case 0:
		/* this means implicitly SOF output */
		val |= (MCP25XXFD_OSC_CLKODIV_10)
			<< MCP25XXFD_OSC_CLKODIV_SHIFT;
		break;
	default:
		dev_err(&spi->dev,
			"Unsupported output clock divider %i\n",
			priv->config.clock_odiv);
		return -EINVAL;
	}

	/* write clock */
	ret = mcp25xxfd_cmd_write(spi, MCP25XXFD_OSC, val,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* wait for synced pll/osc/sclk */
	timeout = jiffies + MCP25XXFD_OSC_POLLING_JIFFIES;
	while (time_before_eq(jiffies, timeout)) {
		ret = mcp25xxfd_cmd_read(spi, MCP25XXFD_OSC,
					 &priv->regs.osc,
					 priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		if ((priv->regs.osc & waitfor) == waitfor)
			return 0;
	}

	dev_err(&spi->dev,
		"Clock did not lock within the timeout period\n");

	/* we timed out */
	return -ENODEV;
}

static int mcp25xxfd_setup_fifo(struct net_device *net,
				struct mcp25xxfd_priv *priv,
				struct spi_device *spi)
{
	u32 val, available_memory, tx_memory_used;
	int ret;
	int i, fifo;

	/* clear all filter */
	for (i = 0; i < 32; i++) {
		ret = mcp25xxfd_cmd_write(spi, CAN_FLTOBJ(i), 0,
					  priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		ret = mcp25xxfd_cmd_write(spi, CAN_FLTMASK(i), 0,
					  priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		ret = mcp25xxfd_cmd_write_mask(spi, CAN_FLTCON(i), 0,
					       CAN_FILCON_MASK(i),
					       priv->spi_setup_speed_hz);
		if (ret)
			return ret;
	}

	/* decide on TEF, tx and rx FIFOS */
	switch (net->mtu) {
	case CAN_MTU:
		/* note: if we have INT1 connected to a GPIO
		 * then we could handle this differently and more
		 * efficiently
		 */

		/* mtu is 8 */
		priv->fifos.payload_size = 8;
		priv->fifos.payload_mode = CAN_TXQCON_PLSIZE_8;

		/* 7 tx fifos starting at fifo 1 */
		priv->fifos.tx_fifos = 7;

		/* 24 rx fifos with 1 buffers/fifo */
		priv->fifos.rx_fifo_depth = 1;

		break;
	case CANFD_MTU:
		/* wish there was a way to have hw filters
		 * that can separate based on length ...
		 */
		/* MTU is 64 */
		priv->fifos.payload_size = 64;
		priv->fifos.payload_mode = CAN_TXQCON_PLSIZE_64;

		/* 7 tx fifos */
		priv->fifos.tx_fifos = 7;

		/* 19 rx fifos with 1 buffer/fifo */
		priv->fifos.rx_fifo_depth = 1;

		break;
	default:
		return -EINVAL;
	}

	/* if defined as a module modify the number of tx_fifos */
	if (tx_fifos) {
		dev_info(&spi->dev,
			 "Using %i tx-fifos as per module parameter\n",
			 tx_fifos);
		priv->fifos.tx_fifos = tx_fifos;
	}

	/* check range - we need 1 RX-fifo and one tef-fifo, hence 30 */
	if (priv->fifos.tx_fifos > 30) {
		dev_err(&spi->dev,
			"There is an absolute maximum of 30 tx-fifos\n");
		return -EINVAL;
	}

	tx_memory_used = priv->fifos.tx_fifos *
		(sizeof(struct mcp25xxfd_obj_tef) +
		 sizeof(struct mcp25xxfd_obj_tx) +
		 priv->fifos.payload_size);
	/* check that we are not exceeding memory limits with 1 RX buffer */
	if (tx_memory_used + (sizeof(struct mcp25xxfd_obj_rx) +
		   priv->fifos.payload_size) > MCP25XXFD_BUFFER_TXRX_SIZE) {
		dev_err(&spi->dev,
			"Configured %i tx-fifos exceeds available memory already\n",
			priv->fifos.tx_fifos);
		return -EINVAL;
	}

	/* calculate possible amount of RX fifos */
	available_memory = MCP25XXFD_BUFFER_TXRX_SIZE - tx_memory_used;

	priv->fifos.rx_fifos = available_memory /
		(sizeof(struct mcp25xxfd_obj_rx) +
		 priv->fifos.payload_size) /
		priv->fifos.rx_fifo_depth;

	/* we only support 31 FIFOS in total (TEF = FIFO0),
	 * so modify rx accordingly
	 */
	if (priv->fifos.tx_fifos + priv->fifos.rx_fifos > 31)
		priv->fifos.rx_fifos = 31 - priv->fifos.tx_fifos;

	/* calculate effective memory used */
	available_memory -= priv->fifos.rx_fifos *
		(sizeof(struct mcp25xxfd_obj_rx) +
		 priv->fifos.payload_size) *
		priv->fifos.rx_fifo_depth;

	/* calcluate tef size */
	priv->fifos.tef_fifos = priv->fifos.tx_fifos;
	fifo = available_memory / sizeof(struct mcp25xxfd_obj_tef);
	if (fifo > 0) {
		priv->fifos.tef_fifos += fifo;
		if (priv->fifos.tef_fifos > 32)
			priv->fifos.tef_fifos = 32;
	}

	/* calculate rx/tx fifo start */
	priv->fifos.rx_fifo_start = 1;
	priv->fifos.tx_fifo_start =
		priv->fifos.rx_fifo_start + priv->fifos.rx_fifos;

	/* set up TEF SIZE to the number of tx_fifos and IRQ */
	priv->regs.tefcon = CAN_TEFCON_FRESET |
		CAN_TEFCON_TEFNEIE |
		CAN_TEFCON_TEFTSEN |
		((priv->fifos.tef_fifos - 1) << CAN_TEFCON_FSIZE_SHIFT);

	ret = mcp25xxfd_cmd_write(spi, CAN_TEFCON,
				  priv->regs.tefcon,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* set up tx fifos */
	val = CAN_FIFOCON_TXEN |
		CAN_FIFOCON_TXATIE | /* show up txatie flags in txatif reg */
		CAN_FIFOCON_FRESET | /* reset FIFO */
		(priv->fifos.payload_mode << CAN_FIFOCON_PLSIZE_SHIFT) |
		(0 << CAN_FIFOCON_FSIZE_SHIFT); /* 1 FIFO only */

	if (priv->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
		if (three_shot)
			val |= CAN_FIFOCON_TXAT_THREE_SHOT <<
				CAN_FIFOCON_TXAT_SHIFT;
		else
			val |= CAN_FIFOCON_TXAT_ONE_SHOT <<
				CAN_FIFOCON_TXAT_SHIFT;
	else
		val |= CAN_FIFOCON_TXAT_UNLIMITED <<
			CAN_FIFOCON_TXAT_SHIFT;

	for (i = 0; i < priv->fifos.tx_fifos; i++) {
		fifo = priv->fifos.tx_fifo_start + i;
		ret = mcp25xxfd_cmd_write(spi, CAN_FIFOCON(fifo),
					  /* the prioriy needs to be inverted
					   * we need to run from lowest to
					   * highest to avoid MAB errors
					   */
					  val | ((31 - fifo) <<
						 CAN_FIFOCON_TXPRI_SHIFT),
					  priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		priv->fifos.tx_fifo_mask |= BIT(fifo);
	}

	/* now set up RX FIFO */
	for (i = 0,
	     fifo = priv->fifos.rx_fifo_start + priv->fifos.rx_fifos - 1;
	     i < priv->fifos.rx_fifos; i++, fifo--) {
		/* prepare the fifo itself */
		ret = mcp25xxfd_cmd_write(spi, CAN_FIFOCON(fifo),
					  (priv->fifos.payload_mode <<
					   CAN_FIFOCON_PLSIZE_SHIFT) |
					  ((priv->fifos.rx_fifo_depth - 1) <<
					   CAN_FIFOCON_FSIZE_SHIFT) |
					  /* RX timestamps: */
					  CAN_FIFOCON_RXTSEN |
					  /* reset FIFO: */
					  CAN_FIFOCON_FRESET |
					  /* FIFO Full: */
					  CAN_FIFOCON_TFERFFIE |
					  /* FIFO Half Full: */
					  CAN_FIFOCON_TFHRFHIE |
					  /* FIFO not empty: */
					  CAN_FIFOCON_TFNRFNIE |
					  /* on last fifo add overflow flag: */
					  ((i == priv->fifos.rx_fifos - 1) ?
					   CAN_FIFOCON_RXOVIE : 0),
					  priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		/* prepare the rx filter config: filter i directs to fifo
		 * FLTMSK and FLTOBJ are 0 already, so they match everything
		 */
		ret = mcp25xxfd_cmd_write_mask(spi, CAN_FLTCON(i),
					       CAN_FIFOCON_FLTEN(i) |
					       (fifo << CAN_FILCON_SHIFT(i)),
					       CAN_FIFOCON_FLTEN(i) |
					       CAN_FILCON_MASK(i),
					       priv->spi_setup_speed_hz);
		if (ret)
			return ret;

		priv->fifos.rx_fifo_mask |= BIT(fifo);
	}

	/* we need to move out of CONFIG mode shortly to get the addresses */
	ret = mcp25xxfd_set_opmode(spi, CAN_CON_MODE_INTERNAL_LOOPBACK,
				   priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* for the TEF fifo */
	ret = mcp25xxfd_cmd_read(spi, CAN_TEFUA, &val,
				 priv->spi_setup_speed_hz);
	if (ret)
		return ret;
	priv->fifos.tef_address = val;
	priv->fifos.tef_address_start = val;
	priv->fifos.tef_address_end = priv->fifos.tef_address_start +
		priv->fifos.tef_fifos * sizeof(struct mcp25xxfd_obj_tef) -
		1;

	/* get all the relevant addresses for the transmit fifos */
	for (i = 0; i < priv->fifos.tx_fifos; i++) {
		fifo = priv->fifos.tx_fifo_start + i;
		ret = mcp25xxfd_cmd_read(spi, CAN_FIFOUA(fifo),
					 &val, priv->spi_setup_speed_hz);
		if (ret)
			return ret;
		priv->fifos.fifo_address[fifo] = val;
	}

	/* and prepare the spi_messages */
	ret = mcp25xxfd_fill_spi_transmit_fifos(priv);
	if (ret)
		return ret;

	/* get all the relevant addresses for the rx fifos */
	for (i = 0; i < priv->fifos.rx_fifos; i++) {
		fifo = priv->fifos.rx_fifo_start + i;
		ret = mcp25xxfd_cmd_read(spi, CAN_FIFOUA(fifo),
					 &val, priv->spi_setup_speed_hz);
		if (ret)
			return ret;
	       priv->fifos.fifo_address[fifo] = val;
	}

	/* now get back into config mode */
	ret = mcp25xxfd_set_opmode(spi, CAN_CON_MODE_CONFIG,
				   priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	return 0;
}

static int mcp25xxfd_setup(struct net_device *net,
			   struct mcp25xxfd_priv *priv,
			   struct spi_device *spi)
{
	int ret;

	/* set up pll/clock if required */
	ret = mcp25xxfd_setup_osc(spi);
	if (ret)
		return ret;

	/* set up RAM ECC */
	priv->regs.ecccon = MCP25XXFD_ECCCON_ECCEN |
		MCP25XXFD_ECCCON_SECIE |
		MCP25XXFD_ECCCON_DEDIE;
	ret = mcp25xxfd_cmd_write(spi, MCP25XXFD_ECCCON,
				  priv->regs.ecccon,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* clean SRAM now that we have ECC enabled
	 * only this case it is clear that all RAM cels have
	 * valid ECC bits
	 */
	ret = mcp25xxfd_clean_sram(spi, priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* time stamp control register - 1ns resolution, but disabled */
	ret = mcp25xxfd_cmd_write(spi, CAN_TBC, 0,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;
	priv->regs.tscon = CAN_TSCON_TBCEN |
		((priv->can.clock.freq / 1000000)
		 << CAN_TSCON_TBCPRE_SHIFT);
	ret = mcp25xxfd_cmd_write(spi, CAN_TSCON,
				  priv->regs.tscon,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* setup value of con_register */
	priv->regs.con = CAN_CON_STEF /* enable TEF */;

	/* transmission bandwidth sharing bits */
	if (bw_sharing_log2bits > 12)
		bw_sharing_log2bits = 12;
	priv->regs.con |= bw_sharing_log2bits << CAN_CON_TXBWS_SHIFT;
	/* non iso FD mode */
	if (!(priv->can.ctrlmode & CAN_CTRLMODE_FD_NON_ISO))
		priv->regs.con |= CAN_CON_ISOCRCEN;
	/* one shot */
	if (priv->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
		priv->regs.con |= CAN_CON_RTXAT;

	/* and put us into default mode = CONFIG */
	priv->regs.con |= (CAN_CON_MODE_CONFIG << CAN_CON_REQOP_SHIFT) |
		(CAN_CON_MODE_CONFIG << CAN_CON_OPMOD_SHIFT);
	/* apply it now - later we will only switch opsmodes... */
	ret = mcp25xxfd_cmd_write(spi, CAN_CON,
				  priv->regs.con,
				  priv->spi_setup_speed_hz);

	/* setup fifos - this also puts the system into sleep mode */
	return mcp25xxfd_setup_fifo(net, priv, spi);
}

static int mcp25xxfd_open(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;
	int ret;

	//pr_err("mcp25xxfd_open start\n");
	ret = open_candev(net);
	if (ret) {
		dev_err(&spi->dev, "unable to set initial baudrate!\n");
		return ret;
	}

	mcp25xxfd_gpio_direction_output(&priv->gpio, 0, 0);

	mcp25xxfd_power_enable(priv->transceiver, 1);

	priv->force_quit = 0;

	/* clear those statistics */
	memset(&priv->stats, 0, sizeof(priv->stats));

	ret = request_threaded_irq(spi->irq, NULL,
				   mcp25xxfd_can_ist,
				   IRQF_ONESHOT | IRQF_TRIGGER_LOW,
				   DEVICE_NAME, priv);
	if (ret) {
		dev_err(&spi->dev, "failed to acquire irq %d - %i\n",
			spi->irq, ret);
		mcp25xxfd_power_enable(priv->transceiver, 0);
		close_candev(net);
		return ret;
	}

	/* wake from sleep if necessary */
	ret = mcp25xxfd_hw_wake(spi);
	if (ret)
		goto open_clean;

	ret = mcp25xxfd_setup(net, priv, spi);
	if (ret)
		goto open_clean;

	mcp25xxfd_do_set_nominal_bittiming(net);
	mcp25xxfd_do_set_data_bittiming(net);

	ret = mcp25xxfd_set_normal_opmode(spi);
	if (ret)
		goto open_clean;
	/* setting up default state */
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	/* only now enable the interrupt on the controller */
	ret =  mcp25xxfd_enable_interrupts(spi,
					   priv->spi_setup_speed_hz);
	if (ret)
		goto open_clean;

	can_led_event(net, CAN_LED_EVENT_OPEN);

	priv->tx_queue_status = TX_QUEUE_STATUS_RUNNING;
	netif_wake_queue(net);

	return 0;

open_clean:
	mcp25xxfd_disable_interrupts(spi, priv->spi_setup_speed_hz);
	free_irq(spi->irq, priv);
	mcp25xxfd_hw_sleep(spi);
	mcp25xxfd_power_enable(priv->transceiver, 0);
	close_candev(net);

	return ret;
}

static void mcp25xxfd_clean(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	int i;

	for (i = 0; i < priv->fifos.tx_fifos; i++) {
		if (priv->fifos.tx_pending_mask & BIT(i)) {
			can_free_echo_skb(priv->net, 0);
			priv->net->stats.tx_errors++;
		}
	}

	priv->fifos.tx_pending_mask = 0;
}

static int mcp25xxfd_stop(struct net_device *net)
{
	struct mcp25xxfd_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;

	close_candev(net);
	mcp25xxfd_gpio_direction_output(&priv->gpio, 0, 1);
	kfree(priv->spi_transmit_fifos);
	priv->spi_transmit_fifos = NULL;

	priv->force_quit = 1;
	free_irq(spi->irq, priv);

	/* Disable and clear pending interrupts */
	mcp25xxfd_disable_interrupts(spi, priv->spi_setup_speed_hz);

	mcp25xxfd_clean(net);

	mcp25xxfd_hw_sleep(spi);

	mcp25xxfd_power_enable(priv->transceiver, 0);

	priv->can.state = CAN_STATE_STOPPED;

	can_led_event(net, CAN_LED_EVENT_STOP);

	return 0;
}

static const struct net_device_ops mcp25xxfd_netdev_ops = {
	.ndo_open = mcp25xxfd_open,
	.ndo_stop = mcp25xxfd_stop,
	.ndo_start_xmit = mcp25xxfd_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static const struct of_device_id mcp25xxfd_of_match[] = {
	{
		.compatible	= "microchip,mcp2517fd",
		.data		= (void *)CAN_MCP2517FD,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, mcp25xxfd_of_match);

static const struct spi_device_id mcp25xxfd_id_table[] = {
	{
		.name		= "mcp2517fd",
		.driver_data	= (kernel_ulong_t)CAN_MCP2517FD,
	},
	{ }
};
MODULE_DEVICE_TABLE(spi, mcp25xxfd_id_table);

static int mcp25xxfd_dump_regs(struct seq_file *file, void *offset)
{
	struct spi_device *spi = file->private;
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	u32 data[CAN_TXQUA - CAN_CON + 4];
	int i;
	int count;
	int ret;

	count = (CAN_TXQUA - CAN_CON) / 4 + 1;
	ret = mcp25xxfd_cmd_readn(spi, CAN_CON, data, 4 * count,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	mcp25xxfd_convert_to_cpu((u32 *)data, 4 * count);

	for (i = 0; i < count; i++) {
		seq_printf(file, "Reg 0x%03x = 0x%08x\n",
			   CAN_CON + 4 * i,
			   ((u32 *)data)[i]);
	}

	count = (MCP25XXFD_ECCSTAT - MCP25XXFD_OSC) / 4 + 1;
	ret = mcp25xxfd_cmd_readn(spi, MCP25XXFD_OSC, data, 4 * count,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;
	mcp25xxfd_convert_to_cpu((u32 *)data, 4 * count);

	for (i = 0; i < count; i++) {
		seq_printf(file, "Reg 0x%03x = 0x%08x\n",
			   MCP25XXFD_OSC + 4 * i,
			   ((u32 *)data)[i]);
	}

	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static void mcp25xxfd_debugfs_add(struct mcp25xxfd_priv *priv)
{
	struct dentry *root, *fifousage, *fifoaddr, *rx, *tx, *status,
		*regs, *stats, *rxdlc, *txdlc;
	char name[32];
	int i;

	/* create the net device name */
	snprintf(name, sizeof(name), DEVICE_NAME "-%s", priv->net->name);
	priv->debugfs_dir = debugfs_create_dir(name, NULL);
	root = priv->debugfs_dir;

	rx = debugfs_create_dir("rx", root);
	tx = debugfs_create_dir("tx", root);
	fifoaddr = debugfs_create_dir("fifo_address", root);
	status = debugfs_create_dir("status", root);
	regs = debugfs_create_dir("regs", root);
	stats = debugfs_create_dir("stats", root);
	fifousage = debugfs_create_dir("fifo_usage", stats);
	rxdlc = debugfs_create_dir("rx_dlc_usage", stats);
	txdlc = debugfs_create_dir("tx_dlc_usage", stats);

	/* add spi speed info */
	debugfs_create_u32("spi_setup_speed_hz", 0444, root,
			   &priv->spi_setup_speed_hz);
	debugfs_create_u32("spi_speed_hz", 0444, root,
			   &priv->spi_speed_hz);

	/* add irq state info */
	debugfs_create_u32("irq_state", 0444, root, &priv->stats.irq_state);

	/* for the clock user mask */
	debugfs_create_u32("clk_user_mask", 0444, root, &priv->clk_user_mask);

	/* add fd statistics */
	debugfs_create_u64("rx_fd_frames", 0444, stats,
			   &priv->stats.rx_fd_count);
	debugfs_create_u64("tx_fd_frames", 0444, stats,
			   &priv->stats.tx_fd_count);
	debugfs_create_u64("rx_brs_frames", 0444, stats,
			   &priv->stats.rx_brs_count);
	debugfs_create_u64("tx_brs_frames", 0444, stats,
			   &priv->stats.tx_brs_count);

	/* export the status structure */
	debugfs_create_x32("intf", 0444, status, &priv->status.intf);
	debugfs_create_x32("rx_if", 0444, status, &priv->status.rxif);
	debugfs_create_x32("tx_if", 0444, status, &priv->status.txif);
	debugfs_create_x32("rx_ovif", 0444, status, &priv->status.rxovif);
	debugfs_create_x32("tx_atif", 0444, status, &priv->status.txatif);
	debugfs_create_x32("tx_req", 0444, status, &priv->status.txreq);
	debugfs_create_x32("trec", 0444, status, &priv->status.trec);
	debugfs_create_x32("bdiag0", 0444, status, &priv->status.bdiag0);
	debugfs_create_x32("bdiag1", 0444, status, &priv->status.bdiag1);

	/* some configuration registers */
	debugfs_create_x32("con", 0444, regs, &priv->regs.con);
	debugfs_create_x32("ecccon", 0444, regs, &priv->regs.ecccon);
	debugfs_create_x32("osc", 0444, regs, &priv->regs.osc);
	debugfs_create_x32("iocon", 0444, regs, &priv->regs.iocon);
	debugfs_create_x32("tdc", 0774, regs, &priv->regs.tdc);
	debugfs_create_x32("tscon", 0444, regs, &priv->regs.tscon);
	debugfs_create_x32("nbtcfg", 0444, regs, &priv->regs.nbtcfg);
	debugfs_create_x32("dbtcfg", 0444, regs, &priv->regs.dbtcfg);

	/* information on fifos */
	debugfs_create_u32("fifo_start", 0444, rx,
			   &priv->fifos.rx_fifo_start);
	debugfs_create_u32("fifo_count", 0444, rx,
			   &priv->fifos.rx_fifos);
	debugfs_create_x32("fifo_mask", 0444, rx,
			   &priv->fifos.rx_fifo_mask);
	debugfs_create_u64("rx_overflow", 0444, rx,
			   &priv->stats.rx_overflow);
	debugfs_create_u64("rx_mab", 0444, stats,
			   &priv->stats.rx_mab);

	debugfs_create_u32("fifo_start", 0444, tx,
			   &priv->fifos.tx_fifo_start);
	debugfs_create_u32("fifo_count", 0444, tx,
			   &priv->fifos.tx_fifos);
	debugfs_create_x32("fifo_mask", 0444, tx,
			   &priv->fifos.tx_fifo_mask);
	debugfs_create_x32("fifo_pending", 0444, tx,
			   &priv->fifos.tx_pending_mask);
	debugfs_create_x32("fifo_submitted", 0444, tx,
			   &priv->fifos.tx_submitted_mask);
	debugfs_create_x32("fifo_processed", 0444, tx,
			   &priv->fifos.tx_processed_mask);
	debugfs_create_u32("queue_status", 0444, tx,
			   &priv->tx_queue_status);
	debugfs_create_u64("tx_mab", 0444, stats,
			   &priv->stats.tx_mab);

	debugfs_create_u32("tef_count", 0444, tx,
			   &priv->fifos.tef_fifos);

	debugfs_create_u32("fifo_max_payload_size", 0444, root,
			   &priv->fifos.payload_size);

	/* interrupt statistics */
	debugfs_create_u64("int", 0444, stats,
			   &priv->stats.irq_calls);
	debugfs_create_u64("int_loops", 0444, stats,
			   &priv->stats.irq_loops);
	debugfs_create_u64("int_ivm", 0444, stats,
			   &priv->stats.int_ivm_count);
	debugfs_create_u64("int_wake", 0444, stats,
			   &priv->stats.int_wake_count);
	debugfs_create_u64("int_cerr", 0444, stats,
			   &priv->stats.int_cerr_count);
	debugfs_create_u64("int_serr", 0444, stats,
			   &priv->stats.int_serr_count);
	debugfs_create_u64("int_rxov", 0444, stats,
			   &priv->stats.int_rxov_count);
	debugfs_create_u64("int_txat", 0444, stats,
			   &priv->stats.int_txat_count);
	debugfs_create_u64("int_spicrc", 0444, stats,
			   &priv->stats.int_spicrc_count);
	debugfs_create_u64("int_ecc", 0444, stats,
			   &priv->stats.int_ecc_count);
	debugfs_create_u64("int_tef", 0444, stats,
			   &priv->stats.int_tef_count);
	debugfs_create_u64("int_mod", 0444, stats,
			   &priv->stats.int_mod_count);
	debugfs_create_u64("int_tbc", 0444, stats,
			   &priv->stats.int_tbc_count);
	debugfs_create_u64("int_rx", 0444, stats,
			   &priv->stats.int_rx_count);
	debugfs_create_u64("int_tx", 0444, stats,
			   &priv->stats.int_tx_count);

	/* dlc statistics */
	for (i = 0; i < 16; i++) {
		snprintf(name, sizeof(name), "%02i", i);
		debugfs_create_u64(name, 0444, rxdlc,
				   &priv->stats.rx_dlc_usage[i]);
		debugfs_create_u64(name, 0444, txdlc,
				   &priv->stats.tx_dlc_usage[i]);
	}

	/* statistics on fifo buffer usage and address */
	for (i = 1; i < 32; i++) {
		snprintf(name, sizeof(name), "%02i", i);
		debugfs_create_u64(name, 0444, fifousage,
				   &priv->stats.fifo_usage[i]);
		debugfs_create_u32(name, 0444, fifoaddr,
				   &priv->fifos.fifo_address[i]);
	}

	/* dump the controller registers themselves */
	debugfs_create_devm_seqfile(&priv->spi->dev, "reg_dump",
				    root, mcp25xxfd_dump_regs);
}

static void mcp25xxfd_debugfs_remove(struct mcp25xxfd_priv *priv)
{
	debugfs_remove_recursive(priv->debugfs_dir);
}

#else
static void mcp25xxfd_debugfs_add(struct mcp25xxfd_priv *priv)
{
	return 0;
}

static void mcp25xxfd_debugfs_remove(struct mcp25xxfd_priv *priv)
{
}
#endif

#ifdef CONFIG_OF_DYNAMIC
int mcp25xxfd_of_parse(struct mcp25xxfd_priv *priv)
{
	struct spi_device *spi = priv->spi;
	const struct device_node *np = spi->dev.of_node;
	u32 val;
	int ret;

	priv->config.clock_div2 =
		of_property_read_bool(np, "microchip,clock-div2");

	ret = of_property_read_u32_index(np, "microchip,clock-out-div",
					 0, &val);
	if (!ret) {
		switch (val) {
		case 0:
		case 1:
		case 2:
		case 4:
		case 10:
			priv->config.clock_odiv = val;
			break;
		default:
			dev_err(&spi->dev,
				"Invalid value in device tree for microchip,clock_out_div: %u - valid values: 0, 1, 2, 4, 10\n",
				val);
			return -EINVAL;
		}
	}

	priv->config.gpio_opendrain =
		of_property_read_bool(np, "gpio-open-drain");

	return 0;
}
#else
int mcp25xxfd_of_parse(struct mcp25xxfd_priv *priv)
{
	return 0;
}
#endif

static int mcp25xxfd_can_probe(struct spi_device *spi)
{
	const struct of_device_id *of_id =
		of_match_device(mcp25xxfd_of_match, &spi->dev);
	struct net_device *net;
	struct mcp25xxfd_priv *priv;
	struct clk *clk;
	int ret, freq;

	/* as irq_create_fwspec_mapping() can return 0, check for it */
	if (spi->irq <= 0) {
		dev_err(&spi->dev, "no valid irq line defined: irq = %i\n",
			spi->irq);
		return -EINVAL;
	}

	clk = devm_clk_get(&spi->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&spi->dev,
			"Can't get Clock source\n");
		return PTR_ERR(clk);
	}
	freq = clk_get_rate(clk);
	if (freq < MCP25XXFD_MIN_CLOCK_FREQUENCY ||
	    freq > MCP25XXFD_MAX_CLOCK_FREQUENCY) {
		dev_err(&spi->dev,
			"Clock frequency %i is not in range [%i:%i]\n",
			freq,
			MCP25XXFD_MIN_CLOCK_FREQUENCY,
			MCP25XXFD_MAX_CLOCK_FREQUENCY);
		return -ERANGE;
	}

	/* Allocate can/net device */
	net = alloc_candev(sizeof(*priv), TX_ECHO_SKB_MAX);
	if (!net)
		return -ENOMEM;

	net->netdev_ops = &mcp25xxfd_netdev_ops;
	net->flags |= IFF_ECHO;

	priv = netdev_priv(net);
	priv->can.bittiming_const = &mcp25xxfd_nominal_bittiming_const;
	priv->can.do_set_bittiming = &mcp25xxfd_do_set_nominal_bittiming;
	priv->can.data_bittiming_const = &mcp25xxfd_data_bittiming_const;
	priv->can.do_set_data_bittiming = &mcp25xxfd_do_set_data_bittiming;
	priv->can.do_set_mode = mcp25xxfd_do_set_mode;
	priv->can.do_get_berr_counter = mcp25xxfd_get_berr_counter;

	priv->can.ctrlmode_supported =
		CAN_CTRLMODE_FD |
		CAN_CTRLMODE_LOOPBACK |
		CAN_CTRLMODE_LISTENONLY |
		CAN_CTRLMODE_BERR_REPORTING |
		CAN_CTRLMODE_FD_NON_ISO |
		CAN_CTRLMODE_ONE_SHOT;

	if (of_id)
		priv->model = (enum mcp25xxfd_model)of_id->data;
	else
		priv->model = spi_get_device_id(spi)->driver_data;

	spi_set_drvdata(spi, priv);
	priv->spi = spi;
	priv->net = net;
	priv->clk = clk;

	priv->clk_user_mask = MCP25XXFD_CLK_USER_CAN;

	mutex_init(&priv->clk_user_lock);
	mutex_init(&priv->spi_rxtx_lock);

	/* enable the clock and mark as enabled */
	priv->clk_user_mask = MCP25XXFD_CLK_USER_CAN;
	ret = clk_prepare_enable(clk);
	if (ret)
		goto out_free;

	/* Setup GPIO controller */
	ret = mcp25xxfd_gpio_setup(spi);
	if (ret)
		goto out_clk;

	/* all by default as push/pull */
	priv->config.gpio_opendrain = false;

	/* do not use the SCK clock divider of 2 */
	priv->config.clock_div2 = false;

	/* clock output is divided by 10 */
	priv->config.clock_odiv = 10;

	/* as a first guess we assume we are in CAN_CON_MODE_SLEEP
	 * this is how we leave the controller when removing ourselves
	 */
	priv->active_can_mode = CAN_CON_MODE_SLEEP;

	/* if we have a clock that is smaller then 4MHz, then enable the pll */
	priv->config.clock_pll =
		(freq <= MCP25XXFD_AUTO_PLL_MAX_CLOCK_FREQUENCY);

	/* check in device tree for overrrides */
	ret = mcp25xxfd_of_parse(priv);
	if (ret)
		return ret;

	/* decide on real can clock rate */
	priv->can.clock.freq = freq;
	if (priv->config.clock_pll) {
		priv->can.clock.freq *= MCP25XXFD_PLL_MULTIPLIER;
		if (priv->can.clock.freq > MCP25XXFD_MAX_CLOCK_FREQUENCY) {
			dev_err(&spi->dev,
				"PLL clock frequency %i would exceed limit\n",
				priv->can.clock.freq
				);
			return -EINVAL;
		}
	}
	if (priv->config.clock_div2)
		priv->can.clock.freq /= MCP25XXFD_SCLK_DIVIDER;

	/* calclculate the clock frequencies to use */
	priv->spi_setup_speed_hz = freq / 2;
	priv->spi_speed_hz = priv->can.clock.freq / 2;
	if (priv->config.clock_div2) {
		priv->spi_setup_speed_hz /= MCP25XXFD_SCLK_DIVIDER;
		priv->spi_speed_hz /= MCP25XXFD_SCLK_DIVIDER;
	}

	if (spi->max_speed_hz) {
		priv->spi_setup_speed_hz = min_t(int,
						 priv->spi_setup_speed_hz,
						 spi->max_speed_hz);
		priv->spi_speed_hz = min_t(int,
					   priv->spi_speed_hz,
					   spi->max_speed_hz);
	}
	/* Configure the SPI bus */
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		goto out_clk;

	ret = mcp25xxfd_power_enable(priv->power, 1);
	if (ret)
		goto out_clk;

	SET_NETDEV_DEV(net, &spi->dev);

	ret = mcp25xxfd_hw_probe(spi);
	/* on error retry a second time */
	if (ret == -ENODEV) {
		ret = mcp25xxfd_hw_probe(spi);
		if (!ret)
			dev_info(&spi->dev,
				 "found device only during retry\n");
	}
	if (ret) {
		if (ret == -ENODEV)
			dev_err(&spi->dev,
				"Cannot initialize MCP%x. Wrong wiring?\n",
				priv->model);
	}

	/* setting up GPIO+INT as PUSHPULL , TXCAN PUSH/PULL, no Standby */
	priv->regs.iocon = 0;

	/* SOF/CLOCKOUT pin 3 */
	if (priv->config.clock_odiv < 1)
		priv->regs.iocon |= MCP25XXFD_IOCON_SOF;

	/* INT/GPIO (probably also clockout) as open drain */
	if (priv->config.gpio_opendrain)
		priv->regs.iocon |= MCP25XXFD_IOCON_INTOD;

	ret = mcp25xxfd_cmd_write(spi, MCP25XXFD_IOCON, priv->regs.iocon,
				  priv->spi_setup_speed_hz);
	if (ret)
		return ret;

	/* and put controller to sleep */
	mcp25xxfd_hw_sleep(spi);

	ret = register_candev(net);
	if (ret)
		goto error_probe;

	/* register debugfs */
	mcp25xxfd_debugfs_add(priv);

	devm_can_led_init(net);

	netdev_info(net, "MCP%x successfully initialized.\n", priv->model);
	return 0;

error_probe:
	mcp25xxfd_power_enable(priv->power, 0);

out_clk:
	mcp25xxfd_stop_clock(spi, MCP25XXFD_CLK_USER_CAN);

out_free:
	free_candev(net);
	dev_err(&spi->dev, "Probe failed, err=%d\n", -ret);
	return ret;
}

static int mcp25xxfd_can_remove(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct net_device *net = priv->net;

	mcp25xxfd_debugfs_remove(priv);

	unregister_candev(net);

	mcp25xxfd_power_enable(priv->power, 0);

	if (!IS_ERR(priv->clk))
		clk_disable_unprepare(priv->clk);

	free_candev(net);

	return 0;
}

static int __maybe_unused mcp25xxfd_can_suspend(struct device *dev)
{
	struct spi_device *spi = NULL;
	struct mcp25xxfd_priv *priv = NULL;
	struct net_device *net = NULL;

	spi = to_spi_device(dev);
	if (!spi)
		return -EINVAL;

	priv = spi_get_drvdata(spi);
	net = priv->net;

	priv->force_quit = 1;
	disable_irq(spi->irq);

	if (netif_running(net)) {
		netif_device_detach(net);

		mcp25xxfd_hw_sleep(spi);
		mcp25xxfd_power_enable(priv->transceiver, 0);
		priv->after_suspend = AFTER_SUSPEND_UP;
	} else {
		priv->after_suspend = AFTER_SUSPEND_DOWN;
	}

	if (!IS_ERR_OR_NULL(priv->power)) {
		regulator_disable(priv->power);
		priv->after_suspend |= AFTER_SUSPEND_POWER;
	}

	return 0;
}

static int __maybe_unused mcp25xxfd_can_resume(struct device *dev)
{
	struct spi_device *spi = NULL;
	struct mcp25xxfd_priv *priv = NULL;

	spi = to_spi_device(dev);
	if (!spi)
		return -EINVAL;

	priv = spi_get_drvdata(spi);

	if (priv->after_suspend & AFTER_SUSPEND_POWER)
		mcp25xxfd_power_enable(priv->power, 1);

	if (priv->after_suspend & AFTER_SUSPEND_UP)
		mcp25xxfd_power_enable(priv->transceiver, 1);
	else
		priv->after_suspend = 0;

	priv->force_quit = 0;

	enable_irq(spi->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mcp25xxfd_can_pm_ops, mcp25xxfd_can_suspend,
	mcp25xxfd_can_resume);

static struct spi_driver mcp25xxfd_can_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = mcp25xxfd_of_match,
		.pm = &mcp25xxfd_can_pm_ops,
	},
	.probe = mcp25xxfd_can_probe,
	.remove = mcp25xxfd_can_remove,
};

static int __init mcp25xxfd_can_driver_init(void)
{
	int ret;

	ret = spi_register_driver(&mcp25xxfd_can_driver);
	if (ret)
		return ret;

	return 0;
}
module_init(mcp25xxfd_can_driver_init);

static void __exit mcp25xxfd_can_driver_exit(void)
{
	spi_unregister_driver(&mcp25xxfd_can_driver);
}
module_exit(mcp25xxfd_can_driver_exit);

MODULE_DESCRIPTION("Microchip 25XXFD CAN driver");
MODULE_LICENSE("GPL v2");
