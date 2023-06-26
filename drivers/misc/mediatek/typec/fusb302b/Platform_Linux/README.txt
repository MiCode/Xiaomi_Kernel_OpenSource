README.TXT

Fairchild Semiconductor FUSB302 Linux Platform Driver Integration Notes
_______________________________________________________________________________
Device Tree
    Currently, the driver requires the use of Device Tree in order to
    function with the I2C bus and the required GPIO pins. Modify the
    following Device Tree snippet to specify resources specific to your
    system and include them in your kernel's Device Tree. The FUSB302
    requires a minimum of 2 GPIO pins, a valid I2C bus, and I2C slave
    address 0x22.

    /*********************************************************************/
    i2c@######## {                                              // replace ######## with the address of your I2C bus
        fusb30x@22 {                                            // I2C Slave Address - Always @22
                compatible = "fairchild,fusb302";               // String must match driver's .compatible string exactly
                reg = <0x22>;                                   // I2C Slave address
                status = "okay";                                // The device is enabled, comment out or delete to disable
                //status = "disabled";                          // Uncomment to disable the device from being loaded at boot
                fairchild,vbus5v    = <&msm_gpio 39 0>;         // VBus 5V GPIO pin - <&gpio_bus pin# 0>. Do not change "fairchild,vbus5v"
                fairchild,vbusOther    = <&msm_gpio 40 0>;      // VBus Other GPIO pin - optional, but if used, name "fairchild,vbusOther" must not change.
                fairchild,int_n        = <&pm8994_gpios 4 0>;   // INT_N GPIO pin - <&gpio_bus pin# 0>. Do not change "fairchild,int_n"
            };
    };
    /*********************************************************************/

_______________________________________________________________________________
Compilation/Makefile
    You must define the preprocessor macro "FSC_PLATFORM_LINUX" in order to
    pull in the correct typedefs.

    The following example snippet is from a Makefile expecting the
    following directory structure:

    path/to/MakefileDir/
        |---- Makefile
        |---- Platform_Linux/
        |---- core/
            |---- vdm/
                |---- DisplayPort/

    Makefile
    /*********************************************************************/
    # Required flag to configure the core to operate with the Linux kernel
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_PLATFORM_LINUX
    # Optional flag to enable debug/hostcomm interface and functionality
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_DEBUG

    # The following flags are used to configure which features are compiled in,
    # and the allowed combinations are:
    #    FSC_HAVE_SRC - Source only
    #    FSC_HAVE_SNK - Sink only
    #    FSC_HAVE_SRC, FSC_HAVE_SNK - Source or sink configurable
    #    FSC_HAVE_SRC, FSC_HAVE_SNK, FSC_HAVE_DRP - DRP capable source or sink
    #    FSC_HAVE_ACCMODE - Accessory mode. Requires FSC_HAVE_SRC.
    #    FSC_HAVE_VDM - Vendor Defined Message (VDM) support.
    #    FSC_HAVE_DP - Display Port (DP) support. Requires FSC_HAVE_VDM.
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_SRC
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_SNK
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_DRP
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_ACCMODE
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_VDM
    ccflags-$(CONFIG_FUSB_30X) += -DFSC_HAVE_DP

    obj-$(CONFIG_FUSB_30X)     += fusb30x_whole.o
    fusb30x_whole-objs := Platform_Linux/fusb30x_driver.o \
                          Platform_Linux/fusb30x_global.o \
                          etc.

    # Please use the Makefile in this directory as a reference.

    /*********************************************************************/

_______________________________________________________________________________
SysFs HostComm/Debug interface
    When FSC_HAVE_DEBUG is defined, this driver will attempt to create sysfs and debugfs files to provide
    user-space access to driver functionality. If it is not defined, then these files will not exist.
    You can find the sysfs files in:
        /sys/class/i2c-dev/i2c-<i2c bus number>/device/<i2c bus number>-0022/
        |---- fairchild,dbg_sm (access to debug GPIO)
        |---- fairchild,int_n (access to INT_N GPIO)
        |---- fairchild,vbus5v (access to VBus5V GPIO)
        |---- control/
            |---- fusb30x_hostcomm  (Provides access to HostComm interface. Examine core/modules/hostcomm.(h|c) for details.)
            |---- reinitialize      (read this file to reinitialize the FUSB302)
            |---- cc_term           (Attached cc termination)
            |---- vconn_term        (Attached vconn termination)
            |---- cc_pin            (CC pin index)
            |---- pe_enabled        (R/W PE state machines enabled)
            |---- src_pref          (R/W Try-Source enabled)
            |---- snk_pref          (R/W Try-Sink enabled)
            |---- etc...            (See Platform_Linux/platform_helpers.c implemenation for others)

    You can find the sysfs files in:
                /sys/kernel/debug/
                |---- tc_log (Type-C state log)
                |---- pe_log (Policy Engine state log)

    Usage examples:
        PE State Log:
        $ cat pe_log
            [sec.ms]    peSourceStartup
            [sec.ms]    peSourceSendCaps
            ...

        Type-C State Log:
        $ cat tc_log
            [sec.ms]    Unattached
            [sec.ms]    AttachWaitSink
            [sec.ms]    AttachedSink
            ...

        Reinitialize the Device:
        $ cat reinitialize
            FUSB302 Reinitialized!

        $ cat cc_pin
            "CC1"

        $ cat cc_term
            "R3p0"

        $ echo "1" > src_pref
            (No output - enables Try-Source and disables Try-Sink)

