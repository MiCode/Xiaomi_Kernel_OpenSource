/*
 * STICK ST1Wire driver
 * Copyright (C) 2020 ST Microelectronics S.A.
 */

struct stick_device;

int stick_kernel_open(struct stick_device **psd, bool debug);
void stick_kernel_reset(struct stick_device *sd);
ssize_t stick_send_frame(struct stick_device *sd, uint8_t *buf, size_t frame_length);
ssize_t stick_read_frame(struct stick_device *sd, uint8_t *buf);
int stick_kernel_release(struct stick_device *sd);

