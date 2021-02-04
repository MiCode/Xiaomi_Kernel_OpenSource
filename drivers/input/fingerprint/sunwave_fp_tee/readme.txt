
# Sunwave 2-1: Please edit kernel-3.18/drivers/input/fingerprint/Kconfig to add SUNWAVE_FP option.
config SUNWAVE_FP
	tristate "Sunwave Fingerprint"
	default y
	help
	  If you say Y to this option, support will be included for
	  the Sunwave's fingerprint sensor. This driver supports
	  both REE and TEE. If in REE, CONFIG_SPI_SPIDEV must be set
	  to use the standard 'spidev' driver.

	  This driver can also be built as a module. If so, the module
	  will be called 'sunwave_fp'.


# Sunwave 2-2: Please edit kernel-3.18/drivers/input/fingerprint/Makefile to add SUNWAVE_FP option.
obj-$(CONFIG_SUNWAVE_FP) += sunwave_fp/

