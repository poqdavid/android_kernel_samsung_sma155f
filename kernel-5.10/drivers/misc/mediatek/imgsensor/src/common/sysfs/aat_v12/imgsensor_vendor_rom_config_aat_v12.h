#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V12_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V12_H

#include "imgsensor_eeprom_rear_gm2.h"
#include "imgsensor_eeprom_rear_2p6.h"
#include "imgsensor_otp_front_sr846d.h"
#include "imgsensor_otp_front_4ha.h"
#include "imgsensor_otp_rear2_gc5035b.h"
#include "imgsensor_otp_rear3_gc02m1b.h"
#include "imgsensor_eeprom_rear4_gc02m1.h"
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3
#define REAR_CAMERA4

#define REAR_SUB_CAMERA
#define USE_SHARED_ROM_REAR3

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V12_H*/
