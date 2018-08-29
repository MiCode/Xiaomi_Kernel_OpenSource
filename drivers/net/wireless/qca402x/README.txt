This directory contains support to communicate beteween an APQ8053 Host
and Qualcomm's QCA402x wireless SoC.

QCA4020 SoC supports
    802.11 (i.e. WiFi/WLAN)
    802.15.4 (i.e. Zigbee, Thread)
    BT LE

Contents of this directory may eventually include:
	cfg80211 support
	SoftMAC wireless driver
	Perhaps a mac80211 driver
	Zigbee APIs
	Thread APIs
	BT APIs

For now, all that is present are the bottommost layers of a communication stack:

	HTCA - Host/Target Communications protocol
		htca_mbox
		    Quartz SDIO/SPI address space
		    Quartz mailboxes and associated SDIO/SPI registers
		    Quartz mbox credit-based flow control
		htca_uart (TBD)

	HIF - a shim layer which abstracts the underlying Master/Host-side
		interconnect controller (e.g. SDIO controller) to provide
		an interconnect-independent API for use by HTCA.
		hif_sdio
			Host Interface layer for SDIO Master controllers
		hif_spi (TBD)
			Host Interface layer for SPI Master controllers
		hif_uart (TBD)
			Host Interface layer for UART-based controllers

	qrtzdev-a simple driver used for HTCA TESTING.

Note: The initial implementation supports HTCA Protocol Version 1 over SDIO.
It is based on previous HTCA implementations for Atheros SoCs, but uses a
revised design which appropriately leverages kernel threads.

This implementation is likely to evolve with increasing focus on performance,
especially for use cases of current interest such as streaming video from
Host over SDIO to WLAN; however this evolution may differ from the existing
implementation of HTCA Protocol Version 2 used by earlier Atheros SoC's.

However there are several issues with this code:
  it is based on HTCA v2 protocol which adds complexity
  it is based on a non-threaded design, originally for a non-threaded RTOS
TBD: Ideally, these two implementations ought to be merged so that the resulting
implementation is based on a proper threaded design and supports both HTCA
protocol v1 and v2.
