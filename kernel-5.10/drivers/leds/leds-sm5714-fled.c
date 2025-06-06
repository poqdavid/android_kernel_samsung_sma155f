/*
 * leds-sm5714-fled.c - SM5714 Flash-LEDs device driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <kd_imgsensor_sysfs_adapter.h>

#define SM5714_FLED_VERSION "XXX.UA1"

#define SM5714_FLASH_LIGHT_MAX 5
unsigned int flashlight_current[SM5714_FLASH_LIGHT_MAX];
int flashlight_using_dt = -1;

static struct sm5714_fled_data *g_sm5714_fled;
#ifdef CONFIG_IMGSENSOR_SYSFS
static struct class *cam_class;
#endif
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
extern void sm5714_request_default_power_src(void);
extern int muic_request_disable_afc_state(void);
extern int sm5714_muic_get_vbus_voltage(void);
extern int muic_check_fled_state(int enable, int mode);   /* mode:1 = FLED_MODE_TORCH, mode:2 = FLED_MODE_FLASH */
extern int sm5714_usbpd_check_fled_state(bool enable, u8 mode);
#endif

static bool fd_use;
bool afc_voltage = true;

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
static void fled_set_enable_push_event(int event_type)
{
	int current_op_status = sm5714_charger_oper_get_current_status();

	if ((current_op_status & 0x20) || (current_op_status & 0x10)) {
		sm5714_charger_oper_push_event(event_type, 1);
		usleep_range(2000, 2001);
	} else {
		pr_info("sm5714-fled: %s : Set otg mode before flash_boost mode Control\n", __func__);
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_USB_OTG, 1);
		usleep_range(2000, 2001);
		sm5714_charger_oper_push_event(event_type, 1);
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_USB_OTG, 0);
	}
}

static void fled_set_disable_push_event(int event_type)
{
	sm5714_charger_oper_push_event(event_type, 0);
}
#endif

static void sm5714_fled_test_read(struct sm5714_fled_data *fled)
{
	u8 data;
	char str[1016] = {0,};
	struct i2c_client *i2c = fled->i2c;

	//open-led and short-led protection interrupt/mask/status registers
	sm5714_read_reg(i2c, SM5714_CHG_REG_INT5, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_INT5, data);

	sm5714_read_reg(i2c, SM5714_CHG_REG_INTMSK5, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_INTMSK5, data);

	sm5714_read_reg(i2c, SM5714_CHG_REG_STATUS5, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_STATUS5, data);

	//boostpok status register
	sm5714_read_reg(i2c, SM5714_CHG_REG_STATUS4, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_STATUS4, data);

	//boost enable/disable register
	sm5714_read_reg(i2c, SM5714_CHG_REG_CNTL2, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_CNTL2, data);

	//fled cntl 1 - timing/mode
	sm5714_read_reg(i2c, SM5714_CHG_REG_FLEDCNTL1, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_FLEDCNTL1, data);

	//fled cntl 2 - current torch/flash
	sm5714_read_reg(i2c, SM5714_CHG_REG_FLEDCNTL2, &data);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", SM5714_CHG_REG_FLEDCNTL2, data);

	pr_info("sm5714-fled: %s: %s\n", __func__, str);

}


static void fled_set_mode(struct sm5714_fled_data *fled, u8 mode)
{
	sm5714_update_reg(fled->i2c, SM5714_CHG_REG_FLEDCNTL1, (mode << 0), (0x3 << 0));
	sm5714_fled_test_read(fled);
}

static void fled_set_fled_current(struct sm5714_fled_data *fled, u8 offset)
{
	sm5714_update_reg(fled->i2c, SM5714_CHG_REG_FLEDCNTL2, (offset << 0), (0xf << 0));
	sm5714_fled_test_read(fled);
	if (offset >= 3)
		pr_info("[%s][D/D] flash current: %d mA\n", __func__, 50 * (offset - 3) + 900);
}

static void fled_set_mled_current(struct sm5714_fled_data *fled, u8 offset)
{
	sm5714_update_reg(fled->i2c, SM5714_CHG_REG_FLEDCNTL2, (offset << 4), (0x7 << 4));
	sm5714_fled_test_read(fled);
	pr_info("[%s][D/D] torch current: %d mA\n", __func__, 25 * offset + 50);
}

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
static int sm5714_fled_check_vbus(struct sm5714_fled_data *fled)
{
	fled->vbus_voltage = sm5714_muic_get_vbus_voltage();
	pr_info("sm5714-fled: %s: voltage = %d", __func__, fled->vbus_voltage);
	if (fled->vbus_voltage > 5500) {
		muic_request_disable_afc_state();
		sm5714_request_default_power_src();
	}
	return 0;
}
#endif

void sm5714_fled_set_afc_voltage_mode(bool mode)
{
	afc_voltage = mode; /* true-enable, false-disable */
}
EXPORT_SYMBOL_GPL(sm5714_fled_set_afc_voltage_mode);

static int sm5714_fled_control(u8 fled_mode)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;
	int ret = 0;

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	if (fled_mode == FLED_MODE_FLASH) {
		ret = gpio_request(fled->pdata->led.fen_pin, "sm5714_fled");
		if (ret < 0) {
			dev_err(fled->dev, "%s: failed request fen-gpio(%d)", __func__, ret);

			fled_set_mode(fled, fled_mode);
			fled->pdata->led.used_gpio_ctrl = false;
		} else {
			gpio_direction_output(fled->pdata->led.fen_pin, 0);
			fled_set_mode(fled,  FLED_MODE_EXTERNAL);
			gpio_set_value(fled->pdata->led.fen_pin, 1);
			gpio_set_value(fled->pdata->led.men_pin, 0);
			fled->pdata->led.used_gpio_ctrl = true;
		}
		pr_info("sm5714-fled: %s: Flash mode & used gpio(%d).\n", __func__, fled->pdata->led.used_gpio_ctrl);
		fd_use = true;

	} else if (fled_mode == FLED_MODE_TORCH) {
		ret = gpio_request(fled->pdata->led.men_pin, "sm5714_fled");
		if (ret < 0) {
			dev_err(fled->dev, "%s: failed request men-gpio(%d)", __func__, ret);
			fled_set_mode(fled, fled_mode);
			fled->pdata->led.used_gpio_ctrl = false;
		} else {
			gpio_direction_output(fled->pdata->led.men_pin, 0);
			fled_set_mode(fled, FLED_MODE_EXTERNAL);
			gpio_set_value(fled->pdata->led.men_pin, 1);
			gpio_set_value(fled->pdata->led.fen_pin, 0);
			fled->pdata->led.used_gpio_ctrl = true;
		}
		pr_info("sm5714-fled: %s: Torch mode & used gpio(%d).\n", __func__, fled->pdata->led.used_gpio_ctrl);
		fd_use = true;

	} else if (fled_mode == FLED_MODE_OFF) {
		pr_info("sm5714-fled: %s: off mode & used gpio = %d.\n", __func__, fled->pdata->led.used_gpio_ctrl);

		fled_set_mode(fled, fled_mode);

		if (fled->pdata->led.en_mled == false && fled->pdata->led.en_fled == false) {
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
			muic_check_fled_state(0, FLED_MODE_FLASH);
			sm5714_usbpd_check_fled_state(0, FLED_MODE_FLASH);
#endif
		}

		if (fled->pdata->led.used_gpio_ctrl == true) {
			gpio_set_value(fled->pdata->led.men_pin, 0);
			gpio_set_value(fled->pdata->led.fen_pin, 0);
			gpio_free(fled->pdata->led.men_pin);
			gpio_free(fled->pdata->led.fen_pin);
			fled->pdata->led.used_gpio_ctrl = false;
		}

		if (fled->pdata->led.en_mled == true) {
			fled->torch_on_cnt--;
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
			if (fled->torch_on_cnt == 0) {
				fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
				if (fled->flash_prepare_cnt == 0) {
					muic_check_fled_state(0, FLED_MODE_TORCH);
					sm5714_usbpd_check_fled_state(0, FLED_MODE_TORCH);
				}
			}
#endif
			fled->pdata->led.en_mled = false;
		}

		if (fled->pdata->led.en_fled == true) {
			fled->flash_on_cnt--;
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
			if (fled->flash_on_cnt == 0) {
				fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
				/* flash case, only vbus control, in prepare_flash & close_flash function  */
				if (fled->flash_prepare_cnt == 0) {
					muic_check_fled_state(0, FLED_MODE_FLASH);
					sm5714_usbpd_check_fled_state(0, FLED_MODE_FLASH);
				}
			}
#endif
			fled->pdata->led.en_fled = false;
		}
		fd_use = false;
	} else {
		pr_err("sm5714-fled: %s: fen_pin : %d, men_pin : %d, FLED_MODE = %d\n"
				, __func__,  fled->pdata->led.fen_pin, fled->pdata->led.men_pin, fled_mode);
		return -EINVAL;
	}

	return 0;
}

static int sm5714_fled_torch_on(u8 brightness)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;

	pr_info("sm5714-fled: %s: start.\n", __func__);

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	mutex_lock(&fled->fled_mutex);

	fled_set_mled_current(fled, brightness);

	if (fled->pdata->led.en_mled == false) {
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
		if (fled->torch_on_cnt == 0)
			fled_set_enable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
#endif
		fled->pdata->led.en_mled = true;
		fled->torch_on_cnt++;
	}

	sm5714_fled_control(FLED_MODE_TORCH);

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5714-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5714_fled_flash_on(u8 brightness)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;

	pr_info("sm5714-fled: %s: start.\n", __func__);

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	mutex_lock(&fled->fled_mutex);

	fled_set_fled_current(fled, brightness);

	if (fled->pdata->led.en_fled == false) {
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
		if (fled->flash_on_cnt == 0)
			fled_set_enable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
#endif
		fled->pdata->led.en_fled = true;
		fled->flash_on_cnt++;
	}
	sm5714_fled_control(FLED_MODE_FLASH);

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5714-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5714_fled_prepare_flash(void)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;

	pr_info("sm5714-fled: %s: start.\n", __func__);

	if (fled == NULL) {
		pr_err("sm5714-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}


	if (fled->pdata->led.pre_fled == true) {
		pr_info("sm5714-fled: %s: already prepared\n", __func__);
		return 0;
	}

	mutex_lock(&fled->fled_mutex);

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
	if (fled->flash_prepare_cnt == 0) {
		sm5714_fled_check_vbus(fled);
		muic_check_fled_state(1, FLED_MODE_FLASH);
		sm5714_usbpd_check_fled_state(1, FLED_MODE_FLASH);
	}
#endif
	/* removed code because this is unnecessary code */
	//fled_set_fled_current(fled, fled->pdata->led.flash_brightness);
	//fled_set_mled_current(fled, fled->pdata->led.torch_brightness);

	fled->flash_prepare_cnt++;
	fled->pdata->led.pre_fled = true;

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5714-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5714_fled_close_flash(void)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;

	pr_info("sm5714-fled: %s: start.\n", __func__);

	if (fled == NULL) {
		pr_err("sm5714-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}
	mutex_lock(&fled->fled_mutex);

	if (fled->pdata->led.pre_fled == false) {
		pr_info("sm5714-fled: %s: already closed\n", __func__);
		mutex_unlock(&fled->fled_mutex);
		return 0;
	}

	fled_set_mode(fled, FLED_MODE_OFF);
	fled->flash_prepare_cnt--;

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
	if (fled->flash_prepare_cnt == 0) {
		fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
		fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
		if (afc_voltage) {
			muic_check_fled_state(0, FLED_MODE_FLASH);
			sm5714_usbpd_check_fled_state(0, FLED_MODE_FLASH);
		}
	}
#endif
	fled->pdata->led.pre_fled = false;

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5714-fled: %s: done.\n", __func__);

	return 0;
}

/**
 * For export Camera flash control support
 *
 */

int32_t sm5714_fled_mode_ctrl(int state, uint32_t brightness)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;
	int ret = 0;
	u8 iq_cur = 0;

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714_fled: %s: g_sm5714_fled is not initialized.\n", __func__);
		return -EFAULT;
	}

	if (brightness > 0 && (state == SM5714_FLED_MODE_TORCH_FLASH || state == SM5714_FLED_MODE_PRE_FLASH)) {
		if (brightness < 50)
			iq_cur = 0x0;
		else if (brightness > 225)
			iq_cur = 0x7;
		else
			iq_cur = (brightness - 50) / 25;
	} else if (brightness > 0 && state == SM5714_FLED_MODE_MAIN_FLASH) {
		if (brightness < 700)
			iq_cur = 0x0;
		else if (brightness < 800)
			iq_cur = 0x1;
		else if (brightness < 900)
			iq_cur = 0x2;
		else
			iq_cur = 3 + (brightness - 900) / 50;
	}

	pr_info("sm5714-fled: %s: iq_cur=0x%x brightness=%u\n", __func__, iq_cur, brightness);

	switch (state) {

	case SM5714_FLED_MODE_OFF:
		/* FlashLight Mode OFF */
		ret = sm5714_fled_control(FLED_MODE_OFF);
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_OFF(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_OFF(%d) done\n", __func__, state);
		sm5714_fled_close_flash();
		break;

	case SM5714_FLED_MODE_MAIN_FLASH:
		/* FlashLight Mode Flash */
		if (brightness > 0)
			ret = sm5714_fled_flash_on(iq_cur);
		else
			ret = sm5714_fled_flash_on(fled->pdata->led.flash_brightness);
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_MAIN_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_MAIN_FLASH(%d) done\n", __func__, state);
		break;

	case SM5714_FLED_MODE_TORCH_FLASH: /* TORCH FLASH */
		/* FlashLight Mode TORCH */
		if (brightness > 0)
			ret = sm5714_fled_torch_on(iq_cur);
		else
			ret = sm5714_fled_torch_on(fled->pdata->led.torch_brightness);
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_TORCH_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_TORCH_FLASH(%d) done\n", __func__, state);
		break;

	case SM5714_FLED_MODE_PRE_FLASH: /* TORCH FLASH */
		sm5714_fled_prepare_flash();
		/* FlashLight Mode TORCH */
		if (brightness > 0)
			ret = sm5714_fled_torch_on(iq_cur);
		else
			ret = sm5714_fled_torch_on(fled->pdata->led.preflash_brightness);
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_PRE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_PRE_FLASH(%d) done\n", __func__, state);
		break;

	case SM5714_FLED_MODE_PREPARE_FLASH:
		/* 9V -> 5V VBUS change */
		ret = sm5714_fled_prepare_flash();
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_PREPARE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_PREPARE_FLASH(%d) done\n", __func__, state);
		break;

	case SM5714_FLED_MODE_CLOSE_FLASH:
		/* 5V -> 9V VBUS change */
		ret = sm5714_fled_close_flash();
		if (ret < 0)
			pr_err("sm5714-fled: %s: SM5714_FLED_MODE_CLOSE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: SM5714_FLED_MODE_CLOSE_FLASH(%d) done\n", __func__, state);
		break;

	default:
		/* FlashLight Mode OFF */
		ret = sm5714_fled_control(FLED_MODE_OFF);
		if (ret < 0)
			pr_err("sm5714-fled: %s: FLED_MODE_OFF(%d) failed\n", __func__, state);
		else
			pr_info("sm5714-fled: %s: FLED_MODE_OFF(%d) done\n", __func__, state);
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(sm5714_fled_mode_ctrl);

/**
 *  For cam_class device file control (Torch-LED)
 */
#ifdef CONFIG_IMGSENSOR_SYSFS
static ssize_t sm5714_rear_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u32 store_value;
	int ret;
	struct sm5714_fled_data *fled = g_sm5714_fled;

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714-fled: %s: g_sm5714_fled NULL\n", __func__);
		return -ENODEV;
	}

	if ((buf == NULL) || kstrtouint(buf, 10, &store_value))
		return -ENXIO;

	fled->pdata->led.sysfs_input_data = store_value;

	dev_info(fled->dev, "%s: value=%d\n", __func__, store_value);

	mutex_lock(&fled->fled_mutex);

	if (store_value == 0) { /* 0: Torch or Flash OFF */
		if (fled->pdata->led.en_mled == false && fled->pdata->led.en_fled == false)
			goto out_skip;
		sm5714_fled_control(FLED_MODE_OFF);
		dev_info(fled->dev, "%s: en_fled=%d flash_on_cnt=%d\n", __func__, fled->pdata->led.en_fled, fled->flash_on_cnt);
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
		if (fled->pdata->led.en_fled == true && fled->flash_on_cnt == 0) {
			fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
			fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
			muic_check_fled_state(0, FLED_MODE_FLASH);
			sm5714_usbpd_check_fled_state(0, FLED_MODE_FLASH);
		}
#endif
	} else if (store_value == 200) { /* 200 : Flash ON */
		fled_set_fled_current(fled, fled->pdata->led.factory_current); /* Set fled = 300mA */

		if (fled->pdata->led.en_fled == true)
			goto out_skip;
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
		if (fled->flash_on_cnt == 0) {
			sm5714_fled_check_vbus(fled);
			muic_check_fled_state(1, FLED_MODE_FLASH);
			sm5714_usbpd_check_fled_state(1, FLED_MODE_FLASH);
			fled_set_enable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
		}
#endif
		sm5714_fled_control(FLED_MODE_FLASH);

		fled->pdata->led.en_fled = true;
		fled->flash_on_cnt++;

	} else { /* 1, 100, 1001~1010, : Torch ON */
		/* Main Torch on */
		if (store_value == 1) {
			fled_set_mled_current(fled, 0x1); /* Set mled = 75mA(0x1) */
		/* Factory Torch on */
		} else if (store_value == 100) {
			fled_set_mled_current(fled, 0x7);    /* Set mled=225mA */
		} else if (store_value >= 1001 && store_value <= 1010) {
			if (flashlight_using_dt == 1) {
				/* (value) 1001, 1002, 1004, 1006, 1009 */
				/* 50mA(0x0) ~ 225mA(0x7) at 25mA step */
				pr_info("flashlight_current is in dt file.\n");
				if (store_value <= 1001)
					fled_set_mled_current(fled, flashlight_current[0]);
				else if (store_value <= 1002)
					fled_set_mled_current(fled, flashlight_current[1]);
				else if (store_value <= 1004)
					fled_set_mled_current(fled, flashlight_current[2]);
				else if (store_value <= 1006)
					fled_set_mled_current(fled, flashlight_current[3]);
				else if (store_value <= 1009)
					fled_set_mled_current(fled, flashlight_current[4]);
				else
					fled_set_mled_current(fled, fled->pdata->led.torch_brightness);
			} else {
				/* Torch on (Normal) */
				if (store_value-1001 > 7)
					fled_set_mled_current(fled, 0x07); /* Max 225mA(0x7)  */
				else {
					fled_set_mled_current(fled, (store_value-1001));
					/* 50mA(0x0) ~ 225mA(0x7) at 25mA step */
				}
			}
		} else {
			dev_err(fled->dev, "%s: failed store cmd\n", __func__);
			ret = -EINVAL;
			goto out_p;
		}
		dev_info(fled->dev, "%s: en_mled=%d, torch_on_cnt = %d\n"
				, __func__, fled->pdata->led.en_mled, fled->torch_on_cnt);

		if (fled->pdata->led.en_mled == true)
			goto out_skip;

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
		if (fled->torch_on_cnt == 0) {
			sm5714_fled_check_vbus(fled);
			muic_check_fled_state(1, FLED_MODE_TORCH);
			sm5714_usbpd_check_fled_state(1, FLED_MODE_TORCH);
			fled_set_enable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
		}
#endif
		sm5714_fled_control(FLED_MODE_TORCH);

		fled->pdata->led.en_mled = true;
		fled->torch_on_cnt++;
	}

out_skip:
	ret = count;

out_p:
	mutex_unlock(&fled->fled_mutex);

	return count;
}

static ssize_t sm5714_rear_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5714_fled_data *fled = g_sm5714_fled;

	if (g_sm5714_fled == NULL) {
		pr_err("sm5714-fled: %s: g_sm5714_fled is NULL\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%d\n", fled->pdata->led.sysfs_input_data);
}

static DEVICE_ATTR(rear_flash, 0664, sm5714_rear_flash_show, sm5714_rear_flash_store);
#endif

bool sm5714_is_fd_in_use(void)
{
	return fd_use;
}
EXPORT_SYMBOL_GPL(sm5714_is_fd_in_use);


static int sm5714_fled_parse_dt(struct device *dev, struct sm5714_fled_platform_data *pdata)
{
	struct device_node *np;
	u32 temp;
	int ret;

	dev_info(dev, "%s: parse from dtsi file\n", __func__);

	np = of_find_node_by_name(NULL, "sm5714-fled");
	if (!np) {
		dev_err(dev, "%s: can't find sm5714-fled np\n", __func__);
		return -EINVAL;
	}

	of_property_read_u32(np, "flash-brightness", &temp);
	pdata->led.flash_brightness = (temp & 0xff);
	of_property_read_u32(np, "preflash-brightness", &temp);
	pdata->led.preflash_brightness = (temp & 0xff);
	of_property_read_u32(np, "torch-brightness", &temp);
	pdata->led.torch_brightness = (temp & 0xff);
	of_property_read_u32(np, "timeout", &temp);
	pdata->led.timeout = (temp & 0xff);
	of_property_read_u8(np, "factory_current", &pdata->led.factory_current);
	ret = of_property_read_u32_array(np, "flashlight_current",
			flashlight_current, SM5714_FLASH_LIGHT_MAX);
	if (ret < 0)
		pr_info("%s : could not find flashlight_current\n", __func__);
	else
		flashlight_using_dt = 1;

	ret = pdata->led.fen_pin = of_get_named_gpio(np, "flash-en-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get flash-en-gpio\n", __func__);
		return ret;
	}

	ret = pdata->led.men_pin = of_get_named_gpio(np, "torch-en-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get torch-en-gpio\n", __func__);
		return ret;
	}

	dev_info(dev, "%s: f_cur=0x%x, pre_cur=0x%x, t_cur=0x%x, tout=%d, gpio=%d:%d\n",
		__func__, pdata->led.flash_brightness, pdata->led.preflash_brightness,
		pdata->led.torch_brightness, pdata->led.timeout, pdata->led.fen_pin, pdata->led.men_pin);
	dev_info(dev, "%s: parse dt done.\n", __func__);

	return 0;
}

static void sm5714_fled_init(struct sm5714_fled_data *fled)
{
	fled_set_mode(fled, FLED_MODE_OFF);
	fled->pdata->led.en_mled = 0;
	fled->pdata->led.en_fled = 0;
	fled->pdata->led.pre_fled = 0;
	fled->pdata->led.used_gpio_ctrl = 0;

	fled->torch_on_cnt = 0;
	fled->flash_on_cnt = 0;
	fled->flash_prepare_cnt = 0;

	mutex_init(&fled->fled_mutex);
	fd_use = false;
	mutex_lock(&fled->fled_mutex);

	fled_set_mode(fled, FLED_MODE_OFF);

#if IS_ENABLED(CONFIG_CHARGER_SM5714)
	if (fled->flash_prepare_cnt == 0) {
		fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_TORCH);
		fled_set_disable_push_event(SM5714_CHARGER_OP_EVENT_FLASH);
		muic_check_fled_state(0, FLED_MODE_FLASH);
		sm5714_usbpd_check_fled_state(0, FLED_MODE_FLASH);
	}
#endif
	fled->pdata->led.pre_fled = false;

	mutex_unlock(&fled->fled_mutex);

	sm5714_fled_test_read(fled);
	dev_info(fled->dev, "%s: flash init done\n", __func__);
}

static int sm5714_fled_probe(struct platform_device *pdev)
{
	struct sm5714_dev *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_fled_data *fled;
	int ret = 0;

	dev_info(&pdev->dev, "sm5714 fled probe start (rev=%d) pdev=%p\n", sm5714->pmic_rev, pdev);

	fled = devm_kzalloc(&pdev->dev, sizeof(struct sm5714_fled_data), GFP_KERNEL);
	if (unlikely(!fled)) {
		dev_err(&pdev->dev, "%s: fail to alloc_devm\n", __func__);
		return -ENOMEM;
	}
	fled->dev = &pdev->dev;
	fled->i2c = sm5714->charger;

	fled->pdata = devm_kzalloc(&pdev->dev, sizeof(struct sm5714_fled_platform_data), GFP_KERNEL);
	if (unlikely(!fled->pdata)) {
		dev_err(fled->dev, "%s: fail to alloc_pdata\n", __func__);
		ret = -ENOMEM;
		goto free_dev;
	}
	ret = sm5714_fled_parse_dt(fled->dev, fled->pdata);
	if (ret < 0)
		goto free_pdata;

	sm5714_fled_init(fled);
	g_sm5714_fled = fled;

#ifdef CONFIG_IMGSENSOR_SYSFS
	IMGSENSOR_SYSFS_GET_CAMERA_CLASS(&cam_class);
	pr_info("[%s] cam_class:0x%p", __func__, cam_class);

	if (IS_ERR_OR_NULL(cam_class)) {
		dev_err(fled->dev, "%s: can't find cam_class sysfs object", __func__);
		dev_err(fled->dev, ", didn't used rear_flash attribute\n");
		goto free_pdata;
	}

	fled->rear_fled_dev = device_create(cam_class, NULL, 0, NULL, "flash");
	if (IS_ERR(fled->rear_fled_dev)) {
		dev_err(fled->dev, "%s failed create device for rear_flash\n", __func__);
		goto free_pdata;
	}

	fled->rear_fled_dev->parent = fled->dev;

	ret = device_create_file(fled->rear_fled_dev, &dev_attr_rear_flash);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		dev_err(fled->dev, "%s failed create device file for rear_flash\n", __func__);
		device_destroy(cam_class, fled->rear_fled_dev->devt);
	}

	dev_info(&pdev->dev, "sm5714 fled probe done.[%s]\n", SM5714_FLED_VERSION);
#else
	dev_err(fled->dev, "%s: Failed to build sysfs: CONFIG_IMGSENSOR_SYSFS is not defined", __func__);
#endif
	return 0;

free_pdata:
	devm_kfree(&pdev->dev, fled->pdata);
free_dev:
	devm_kfree(&pdev->dev, fled);

	return ret;
}

static int sm5714_fled_remove(struct platform_device *pdev)
{
	struct sm5714_fled_data *fled = platform_get_drvdata(pdev);
#ifdef CONFIG_IMGSENSOR_SYSFS
	device_remove_file(fled->rear_fled_dev, &dev_attr_rear_flash);


	device_destroy(cam_class, fled->rear_fled_dev->devt);
#endif


	fled_set_mode(fled, FLED_MODE_OFF);

	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, fled->pdata);
	devm_kfree(&pdev->dev, fled);

	return 0;
}

static const struct of_device_id sm5714_fled_match_table[] = {
	{ .compatible = "siliconmitus,sm5714-fled",},
	{},
};

static const struct platform_device_id sm5714_fled_id[] = {
	{"sm5714-fled", 0},
	{},
};

static struct platform_driver sm5714_led_driver = {
	.driver = {
		.name  = "sm5714-fled",
		.owner = THIS_MODULE,
		.of_match_table = sm5714_fled_match_table,
		},
	.probe  = sm5714_fled_probe,
	.remove = sm5714_fled_remove,
	.id_table = sm5714_fled_id,
};

static int __init sm5714_led_driver_init(void)
{
	return platform_driver_register(&sm5714_led_driver);
}
late_initcall(sm5714_led_driver_init);

static void __exit sm5714_led_driver_exit(void)
{
	platform_driver_unregister(&sm5714_led_driver);
}
module_exit(sm5714_led_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Flash-LED device driver for SM5714");
MODULE_VERSION(SM5714_FLED_VERSION);
