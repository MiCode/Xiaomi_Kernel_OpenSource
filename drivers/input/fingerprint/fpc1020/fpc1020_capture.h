/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_CAPTURE_H
#define LINUX_SPI_FPC1020_CAPTURE_H

extern int fpc1020_init_capture(fpc1020_data_t *fpc1020);

extern int fpc1020_write_capture_setup(fpc1020_data_t *fpc1020);

extern int fpc1020_write_test_setup(fpc1020_data_t *fpc1020, u16 pattern);

extern bool fpc1020_capture_check_ready(fpc1020_data_t *fpc1020);

extern int fpc1020_capture_task(fpc1020_data_t *fpc1020);

extern int fpc1020_capture_wait_finger_down(fpc1020_data_t *fpc1020);

extern int fpc1020_capture_wait_finger_up(fpc1020_data_t *fpc1020);

extern int fpc1020_capture_settings(fpc1020_data_t *fpc1020, int select);

extern int fpc1020_capture_set_sample_mode(fpc1020_data_t *fpc1020,
					unsigned int samples);

extern int fpc1020_capture_set_crop(fpc1020_data_t *fpc1020,
					int first_column,
					int num_columns,
					int first_row,
					int num_rows);

extern int fpc1020_capture_buffer(fpc1020_data_t *fpc1020,
					u8 *data,
					size_t offset,
					size_t image_size_bytes);

extern int fpc1020_capture_deferred_task(fpc1020_data_t *fpc1020);

extern int fpc1020_capture_finger_detect_settings(fpc1020_data_t *fpc1020);

#endif /* LINUX_SPI_FPC1020_CAPTURE_H */

