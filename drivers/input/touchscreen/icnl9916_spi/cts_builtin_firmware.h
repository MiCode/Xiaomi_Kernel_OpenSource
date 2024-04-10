static u8 icnl9916_driver_builtin_firmware[] = {
};

static u8 icnl9916c_driver_builtin_firmware[] = {
};

const static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9916,
        .fwid = CTS_DEV_FWID_ICNL9916,
        .data = icnl9916_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9916_driver_builtin_firmware),
    },
    {
        .name = "OEM-Project",    /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9916C,
        .fwid = CTS_DEV_FWID_ICNL9916C,
        .data = icnl9916c_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9916c_driver_builtin_firmware),
    },

};
