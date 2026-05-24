// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#ifndef __IMGSENSOR_COMMON_INTERFACE_H__
#define __IMGSENSOR_COMMON_INTERFACE_H__

struct imgsensor_i2c_info {
	unsigned int i2c_device_id;
	unsigned int i2c_speed;
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId, u16 timing);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);

/* I2C read interface */
unsigned short imgsensor_i2c_read_addr16_data16(struct imgsensor_i2c_info *info, unsigned short addr);
unsigned char imgsensor_i2c_read_addr16_data8(struct imgsensor_i2c_info *info, unsigned short addr);
unsigned char imgsensor_i2c_read_addr8_data8(struct imgsensor_i2c_info *info, unsigned char addr);

/* I2C write interface */
int imgsensor_i2c_write_addr16_data16(struct imgsensor_i2c_info *info, unsigned short addr, unsigned short data);
int imgsensor_i2c_write_addr16_data8(struct imgsensor_i2c_info *info, unsigned short addr, unsigned char data);
int imgsensor_i2c_write_addr8_data8(struct imgsensor_i2c_info *info, unsigned char addr, unsigned char data);

#endif
