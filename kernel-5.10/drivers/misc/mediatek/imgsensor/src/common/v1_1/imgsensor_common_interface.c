// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#include "imgsensor_common.h"
#include "imgsensor_i2c.h"
#include "imgsensor_common_interface.h"

static int check_condition(struct imgsensor_i2c_info *info)
{
	if (info == NULL) {
		PK_PR_ERR("info is NULL");
		return -1;
	}

	if (!info->i2c_device_id) {
		PK_PR_ERR("invalid i2c device id");
		return -1;
	}

	if (!info->i2c_speed) {
		info->i2c_speed = IMGSENSOR_I2C_SPEED;
		PK_INFO("set default speed: %d KHz", info->i2c_speed);
	}
	return 0;
}

unsigned short imgsensor_i2c_read_addr16_data16(struct imgsensor_i2c_info *info, unsigned short addr)
{
	unsigned short get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (check_condition(info) < 0)
		return 0;

	iReadRegI2CTiming(pusendcmd, 2, (unsigned char *) &get_byte, 2, info->i2c_device_id, info->i2c_speed);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}

unsigned char imgsensor_i2c_read_addr16_data8(struct imgsensor_i2c_info *info, unsigned short addr)
{
	unsigned char get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (check_condition(info) < 0)
		return 0;

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1, info->i2c_device_id, info->i2c_speed);
	return get_byte;
}

unsigned char imgsensor_i2c_read_addr8_data8(struct imgsensor_i2c_info *info, unsigned char addr)
{
	unsigned char get_byte = 0;

	if (check_condition(info) < 0)
		return 0;

	iReadRegI2CTiming(&addr, 2, (unsigned char *) &get_byte, 1, info->i2c_device_id, info->i2c_speed);
	return get_byte;
}

int imgsensor_i2c_write_addr16_data16(struct imgsensor_i2c_info *info, unsigned short addr, unsigned short data)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(data >> 8), (char)(data & 0xFF)};

	if (check_condition(info) < 0)
		return -1;

	return iWriteRegI2CTiming(pusendcmd, 4, info->i2c_device_id, info->i2c_speed);
}

int imgsensor_i2c_write_addr16_data8(struct imgsensor_i2c_info *info, unsigned short addr, unsigned char data)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(data & 0xFF)};

	if (check_condition(info) < 0)
		return -1;

	return iWriteRegI2CTiming(pusendcmd, 3, info->i2c_device_id, info->i2c_speed);
}

int imgsensor_i2c_write_addr8_data8(struct imgsensor_i2c_info *info, unsigned char addr, unsigned char data)
{
	char pusendcmd[2] = {(char)(addr), (char)(data)};

	if (check_condition(info) < 0)
		return -1;

	return iWriteRegI2CTiming(pusendcmd, 2, info->i2c_device_id, info->i2c_speed);
}
