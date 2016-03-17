/*
* Copyright (C) 2015 InvenSense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include <linux/kernel.h>
#include <linux/string.h>

#include "inv_sh_command.h"
#include "inv_sh_data.h"

#define INV_SH_DATA_HEADER_SIZE				6

#define INV_SH_DATA_ID_INDEX				0
#define INV_SH_DATA_ID_GET(a)				((a) & 0xFF)

#define INV_SH_DATA_STATUS_INDEX			1
#define INV_SH_DATA_STATUS_ACCURACY_GET(a)		((a) & 0x03)
#define INV_SH_DATA_STATUS_STATUS_GET(a)		(((a) & 0x0C) >> 2)
#define INV_SH_DATA_STATUS_DATASIZE_GET(a)		(((a) & 0x10) >> 4)
#define INV_SH_DATA_STATUS_ANSWER_GET(a)		(((a) & 0x20) >> 5)
#define INV_SH_DATA_STATUS_PAYLOAD_GET(a)		(((a) & 0x40) >> 6)
#define INV_SH_DATA_STATUS_EXTENDED_GET(a)		(((a) & 0x80) >> 7)

#define INV_SH_DATA_TIMESTAMP_CMD_INDEX			2

static int32_t inv_le_to_int32(const uint8_t *frame)
{
	return (frame[3] << 24) | (frame[2] << 16) | (frame[1] << 8) | frame[0];
}

static int16_t inv_le_to_int16(const uint8_t *frame)
{
	return (frame[1] << 8) | frame[0];
}

static int parse_common(const uint8_t *frame, size_t size,
			struct inv_sh_data *data)
{
	uint8_t d;
	uint32_t timestamp_cmd;

	if (size < INV_SH_DATA_HEADER_SIZE)
		return -EINVAL;

	/* Parse header */
	d = frame[INV_SH_DATA_ID_INDEX];
	data->id = INV_SH_DATA_ID_GET(d);

	d = frame[INV_SH_DATA_STATUS_INDEX];
	data->accuracy = INV_SH_DATA_STATUS_ACCURACY_GET(d);
	data->status = INV_SH_DATA_STATUS_STATUS_GET(d);
	data->data_size = INV_SH_DATA_STATUS_DATASIZE_GET(d);
	data->is_answer = INV_SH_DATA_STATUS_ANSWER_GET(d);
	data->payload = INV_SH_DATA_STATUS_PAYLOAD_GET(d);
	data->extended = INV_SH_DATA_STATUS_EXTENDED_GET(d);

	/* Extended format not supported yet */
	if (data->extended != 0)
		return -EINVAL;

	/* Parse timestamp/cmd */
	timestamp_cmd = inv_le_to_int32(&frame[INV_SH_DATA_TIMESTAMP_CMD_INDEX]);
	if (data->is_answer)
		data->command = timestamp_cmd;
	else
		data->timestamp = timestamp_cmd;

	return INV_SH_DATA_HEADER_SIZE;
}

static int parse_data_accelerometer(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t)))
		return -EINVAL;

	data->accelerometer.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->accelerometer.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->accelerometer.z = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_magnetic_field(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t)))
		return -EINVAL;

	data->magnetic_field.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field.z = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_orientation(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t)))
		return -EINVAL;

	data->orientation.azimuth = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->orientation.pitch = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->orientation.roll = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_gyroscope(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t))) {
		return -EINVAL;
	}

	data->gyroscope.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope.z = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_light(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint32_t))
		return -EINVAL;

	data->light = inv_le_to_int32(frame);

	return sizeof(uint32_t);
}

static int parse_data_pressure(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint32_t))
		return -EINVAL;

	data->pressure = inv_le_to_int32(frame);

	return sizeof(uint32_t);
}

static int parse_data_proximity(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint16_t))
		return -EINVAL;

	data->proximity = inv_le_to_int16(frame);

	return sizeof(uint16_t);
}

static int parse_data_gravity(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t)))
		return -EINVAL;

	data->gravity.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gravity.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gravity.z = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_linear_acceleration(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (3 * sizeof(int16_t)))
		return -EINVAL;

	data->linear_acceleration.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->linear_acceleration.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->linear_acceleration.z = inv_le_to_int16(frame);

	return 3 * sizeof(int16_t);
}

static int parse_data_rotation_vector(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (4 * sizeof(int32_t) + sizeof(uint16_t)))
		return -EINVAL;

	data->rotation_vector.x = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->rotation_vector.y = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->rotation_vector.z = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->rotation_vector.w = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->rotation_vector.accuracy = inv_le_to_int16(frame);

	return 4 * sizeof(int32_t) + sizeof(uint16_t);
}

static int parse_data_relative_humidity(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint16_t))
		return -EINVAL;

	data->relative_humidity = inv_le_to_int16(frame);

	return sizeof(uint16_t);
}

static int parse_data_ambient_temperature(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(int16_t))
		return -EINVAL;

	data->ambient_temperature = inv_le_to_int16(frame);

	return sizeof(int16_t);
}

static int parse_data_magnetic_field_uncalibrated(const uint8_t *frame,
		size_t size, union inv_sh_data_sensor *data)
{
	if (size < (6 * sizeof(int16_t)))
		return -EINVAL;

	data->magnetic_field_uncalibrated.uncalib.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field_uncalibrated.uncalib.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field_uncalibrated.uncalib.z = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field_uncalibrated.bias.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field_uncalibrated.bias.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->magnetic_field_uncalibrated.bias.z = inv_le_to_int16(frame);

	return 6 * sizeof(int16_t);
}

static int parse_data_game_rotation_vector(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (4 * sizeof(int32_t) + sizeof(uint16_t)))
		return -EINVAL;

	data->game_rotation_vector.x = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->game_rotation_vector.y = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->game_rotation_vector.z = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->game_rotation_vector.w = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->game_rotation_vector.accuracy = inv_le_to_int16(frame);

	return 4 * sizeof(int32_t) + sizeof(uint16_t);
}

static int parse_data_gyroscope_uncalibrated(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < (6 * sizeof(int16_t)))
		return -EINVAL;

	data->gyroscope_uncalibrated.uncalib.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope_uncalibrated.uncalib.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope_uncalibrated.uncalib.z = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope_uncalibrated.bias.x = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope_uncalibrated.bias.y = inv_le_to_int16(frame);
	frame += sizeof(int16_t);
	data->gyroscope_uncalibrated.bias.z = inv_le_to_int16(frame);

	return 6 * sizeof(int16_t);
}

static int parse_data_step_counter(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint32_t))
		return -EINVAL;

	data->step_counter = inv_le_to_int32(frame);

	return sizeof(uint32_t);
}

static int parse_data_geomagnetic_rotation_vector(const uint8_t *frame,
		size_t size, union inv_sh_data_sensor *data)
{
	if (size < (4 * sizeof(int32_t) + sizeof(uint16_t)))
		return -EINVAL;

	data->geomagnetic_rotation_vector.x = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->geomagnetic_rotation_vector.y = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->geomagnetic_rotation_vector.z = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->geomagnetic_rotation_vector.w = inv_le_to_int32(frame);
	frame += sizeof(int32_t);
	data->geomagnetic_rotation_vector.accuracy = inv_le_to_int16(frame);

	return 4 * sizeof(int32_t) + sizeof(uint16_t);
}

static int parse_data_heart_rate(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint16_t))
		return -EINVAL;

	data->heart_rate = inv_le_to_int16(frame);

	return sizeof(uint16_t);
}

static int parse_data_activity_recognition(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < sizeof(uint8_t))
		return -EINVAL;

	data->activity_recognition = frame[0];

	return sizeof(uint8_t);
}

static int parse_data_vendor(const uint8_t *frame, size_t size,
		union inv_sh_data_sensor *data)
{
	if (size < data->vendor.size)
		return -EINVAL;

	if (data->vendor.size > 0)
		data->vendor.data = frame;
	else
		data->vendor.data = NULL;

	return data->vendor.size;
}

static int process_sensor_data(const uint8_t *frame, size_t size,
		struct inv_sh_data *sensor_data)
{
	int sensor_id;
	int ret = -EINVAL;

	/* No data if not DATA_UPDATED or POLL */
	if (sensor_data->status != INV_SH_DATA_STATUS_DATA_UPDATED &&
			sensor_data->status != INV_SH_DATA_STATUS_POLL)
		return 0;

	/* Process normal data */
	sensor_id = sensor_data->id & ~INV_SH_DATA_SENSOR_ID_FLAG_MASK;
	switch (sensor_id) {
	case INV_SH_DATA_SENSOR_ID_ACCELEROMETER:
		ret = parse_data_accelerometer(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_MAGNETIC_FIELD:
		ret = parse_data_magnetic_field(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_ORIENTATION:
		ret = parse_data_orientation(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_GYROSCOPE:
		ret = parse_data_gyroscope(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_LIGHT:
		ret = parse_data_light(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_PRESSURE:
		ret = parse_data_pressure(frame, size, &sensor_data->data);
		break;
	/* DEPRECATED: SENSOR_ID_TEMPERATURE */
	case INV_SH_DATA_SENSOR_ID_PROXIMITY:
		ret = parse_data_proximity(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_GRAVITY:
		ret = parse_data_gravity(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_LINEAR_ACCELERATION:
		ret = parse_data_linear_acceleration(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_ROTATION_VECTOR:
		ret = parse_data_rotation_vector(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_RELATIVE_HUMIDITY:
		ret = parse_data_relative_humidity(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_AMBIENT_TEMPERATURE:
		ret = parse_data_ambient_temperature(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_MAGNETIC_FIELD_UNCALIBRATED:
		ret = parse_data_magnetic_field_uncalibrated(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_GAME_ROTATION_VECTOR:
		ret = parse_data_game_rotation_vector(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_GYROSCOPE_UNCALIBRATED:
		ret = parse_data_gyroscope_uncalibrated(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_STEP_COUNTER:
		ret = parse_data_step_counter(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_GEOMAGNETIC_ROTATION_VECTOR:
		ret = parse_data_geomagnetic_rotation_vector(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_HEART_RATE:
		ret = parse_data_heart_rate(frame, size, &sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_ACTIVITY_RECOGNITION:
		ret = parse_data_activity_recognition(frame, size,
				&sensor_data->data);
		break;
	case INV_SH_DATA_SENSOR_ID_METADATA:
	case INV_SH_DATA_SENSOR_ID_SIGNIFICANT_MOTION:
	case INV_SH_DATA_SENSOR_ID_STEP_DETECTOR:
	case INV_SH_DATA_SENSOR_ID_TILT_DETECTOR:
	case INV_SH_DATA_SENSOR_ID_WAKE_GESTURE:
	case INV_SH_DATA_SENSOR_ID_GLANCE_GESTURE:
	case INV_SH_DATA_SENSOR_ID_PICKUP_GESTURE:
		/* No data */
		ret = 0;
		break;
	default:
		/* Process vendor specific data */
		if (sensor_id >= INV_SH_DATA_SENSOR_ID_VENDOR_BASE)
			ret = parse_data_vendor(frame, size,
					&sensor_data->data);
		break;
	}

	/* Update packet size */
	if (ret > 0) {
		sensor_data->size += ret;
		ret = 0;
	}

	return ret;
}

static int parse_answer_get_calib_gains(const uint8_t *frame, size_t size,
		union inv_sh_data_answer *answer)
{
	if (size < sizeof(answer->gain))
		return -EINVAL;

	memcpy(answer->gain, frame, sizeof(answer->gain));

	return sizeof(answer->gain);
}

static int parse_answer_get_calib_offsets(const uint8_t *frame, size_t size,
		union inv_sh_data_answer *answer)
{
	if (size < sizeof(answer->offset))
		return -EINVAL;

	memcpy(answer->offset, frame, sizeof(answer->offset));

	return sizeof(answer->offset);
}

static int process_sensor_answer(const uint8_t *frame, size_t size,
		struct inv_sh_data *data)
{
	int ret;

	switch (data->command) {
	case INV_SH_COMMAND_GET_CALIB_GAINS:
		ret = parse_answer_get_calib_gains(frame, size, &data->answer);
		break;
	case INV_SH_COMMAND_GET_CALIB_OFFSETS:
		ret = parse_answer_get_calib_offsets(frame, size,
				&data->answer);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	/* Update packet size */
	if (ret > 0) {
		data->size += ret;
		ret = 0;
	}

	return ret;
}


int inv_sh_data_parse(const void *frame, size_t size,
			struct inv_sh_data *sensor_data)
{
	const uint8_t *data = frame;
	int ret;

	/* Parse common header */
	ret = parse_common(data, size, sensor_data);
	if (ret < 0)
		return ret;
	sensor_data->raw = data;
	sensor_data->size = ret;
	data += sensor_data->size;
	size -= sensor_data->size;

	/* Process payload bit */
	if (sensor_data->payload) {
		if (size < 1)
			return -EINVAL;
		sensor_data->data.vendor.size = data[0];
		sensor_data->size++;
		data++;
		size--;
	}

	/* Process answer or data */
	if (sensor_data->is_answer)
		ret = process_sensor_answer(data, size, sensor_data);
	else
		ret = process_sensor_data(data, size, sensor_data);

	return ret;
}
