TJA110X Linux Phy Driver Release v0.1 20170119 - User Notes
------------------------------------------------------------
Table of Contents
1) TJA110X loading
2) TJA110X configuration management
3) TJA110X autonomous and managed mode
4) Limitations and known issues
-------------------------------------------------------------

1) TJA110X loading
Load the kernel module: eg. "insmod nxp.ko"
For managedMode configuration see 3)

2) TJA110X configuration management
sysFS nodes are created in: /sys/bus/mdio_bus/devices/{devName}/configuration

	- cable_test	R: execute a cable test and return the result
			W: /

	- led_cfg	R: return the current led mode, one of {DISABLED, LINKUP, FRAMEREC, SYMERR, CRSSIG}
			W: set the led mode, with 0=DISABLED, 1=LINKUP, 2=FRAMEREC, 3=SYMERR, 4=CRSSIG

	- link_status	R: return the current link status, one of {up, down}
			W: /

	- loopback_cfg	R: return the current loopback mode, one of {LOOPBACK_DISABLED, INTERNAL_LOOPBACK, EXTERNAL_LOOPBACK, REMOTE_LOOPBACK}
			W: set the loopback mode, with 0=LOOPBACK_DISABLED, 1=INTERNAL_LOOPBACK, 2=EXTERNAL_LOOPBACK, 3=REMOTE_LOOPBACK

	- master_cfg	R: return the current master/slave configuration, one of {master, slave}
			W: set the master/slave configuration, with 0=slave, 1=master

	- power_cfg	R: return the current power mode, one of {POWER_MODE_NORMAL, POWER_MODE_SLEEPREQUEST, POWER_MODE_SLEEP, POWER_MODE_SILENT, POWER_MODE_STANDBY, POWER_MODE_NOCHANGE}
			W: initiate a power mode transition, with 0=SUSPEND, 1=RESUME, 2=SLEEPREQUEST, 3=WAKEUP

	- test_mode	R: return the current testmode mode, one of {NO_TMODE, TMODE1, TMODE2, TMODE3, TMODE4, TMODE5, TMODE6}
			W: set the testmode, with 0=NO_TMODE, 1=TMODE1, 2=TMODE2, 3=TMODE3, 4=TMODE4, 5=TMODE5, 6=TMODE6

	- wakeup_cfg	R: get the current wakeup configuration, one of: fwdphyloc[on/off], remwuphy[on/off], locwuphy[on/off], fwdphyrem[on/off]
			W: set the wakeup configuration: pass a hexadecimal number, the first four bits are interpreted as switches for the four configurable options
			    --> TJA1102: all four options are configurable separately
			    --> TJA1100: FWDPHYLOC MUST be off
					 REMWUPHY MUST be on
					 only LOCWUPHY and FWDPHYREM are configurable
					 Possible configurations:
					 - BOTH enabled (then led MUST be off)
					 - BOTH disabled (then led CAN be on)
					 all other configurations are invalid.

					 Therefore valid values to write to sysfs are:
					 - 2 (LOCWUPHY and FWDPHYREM off)
					 - E (LOCWUPHY and FWDPHYREM on)

3) TJA110X autonomous and managed mode
To choose between managed and autonomous mode, the kernel module parameter "managedMode" shall be used.
	- if managedMode is given a nonzero value(eg. "insmod nxp.ko managedMode=1"), the Phy operates in managed mode
	- if managedMode is set to zero (default value), the phy operates in autonomous mode. In autonomous mode, the following features are not available:
		- cable test
		- loopback modes
		- test modes
		- power modes (ie. the sleep, wakeup, suspend and resume commands)

4) Limitations and known issues
- The TJA1100 disables SMI when entering sleep. Once entered it can only be woken up by bus activity or via the WAKE pin
