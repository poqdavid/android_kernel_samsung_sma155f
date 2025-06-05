// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_aax_v16.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,   S5KJN1_SENSOR_ID,    &rear_s5kjn1_cal_addr},
	{SENSOR_POSITION_FRONT,  GC13A0_SENSOR_ID,    &front_gc13a0_cal_addr},
	{SENSOR_POSITION_REAR2,  SC501CS_SENSOR_ID,   &rear2_sc501cs_cal_addr},
	{SENSOR_POSITION_REAR3,  GC02M1_SENSOR_ID,    &rear3_gc02m1_cal_addr},
};
