/*
 * Copyrights (C) 2018 Samsung Electronics, Inc.
 * Copyrights (C) 2018 Silicon Mitus, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#else
#include <linux/battery/sec_pd.h>
#endif
#include <linux/usb/typec/sm/sm5714/sm5714_pd.h>
#include <linux/usb/typec/sm/sm5714/sm5714_typec.h>
#include <linux/of_gpio.h>
#include <linux/mfd/sm/sm5714/sm5714_log.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../../../battery/common/sec_charging_common.h"
#endif

struct sm5714_usbpd_data *sm5714_g_pd_data;

void sm5714_usbpd_inform_pdo_list(void);
static int sm5714_usbpd_command_to_policy(struct device *dev,
		sm5714_usbpd_manager_command_type command);

void sm5714_usbpd_change_source_cap(int enable, int max_cur, int init)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;

	msg_header_type *msg_header = &pd_data->source_msg_header;
	data_obj_type *data_obj = &pd_data->source_data_obj[0]; /* Fixed PDO */
	int max_current = 0;

	if (pd_data->thermal_state == enable) {
		sm5714_info("%s, ignored prev(%d), enable(%d)\n", __func__,
				pd_data->thermal_state, enable);
		return;
	}

	sm5714_info("%s, en(%d), max_cur(%d) init(%d)\n", __func__, enable, max_cur, init);
	if (enable) {
		pd_data->thermal_state = 1;
		msg_header->num_data_objs = 1;
		if (max_cur > 500)
			max_current = 500 / 10;
		else
			max_current = (max_cur / 10) & 0x3ff;
		data_obj->power_data_obj.max_current = max_current;
	} else {
		pd_data->thermal_state = 0;
		msg_header->num_data_objs = 2;
	}
	if (!init)
		sm5714_usbpd_command_to_policy(pd_data->dev, MANAGER_REQ_SRCCAP_CHANGE);
}

void sm5714_usbpd_forced_change_srccap(int max_cur)
{
	sm5714_usbpd_change_source_cap(2, max_cur, 0);
}

void sm5714_select_pdo(int num)
{
	struct sm5714_usbpd_data *psubpd = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = psubpd->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &psubpd->manager;
	bool vbus_short = false;

	if (!pdic_data->is_attached || pdic_data->shut_down) {
		sm5714_info(" %s : PDO(%d) is ignored because of plug detached\n",
				__func__, num);
		return;
	}

	if (psubpd->policy.state != PE_SNK_Ready) {
		sm5714_info(" %s : PDO(%d) is ignored because of not SNK ready..\n",
				__func__, num);
		return;
	}
	psubpd->phy_ops.get_short_state(psubpd, &vbus_short);

	if (vbus_short) {
		sm5714_info(" %s : PDO(%d) is ignored because of vbus short\n",
				__func__, num);
		return;
	}

	if (num > 1 && (manager->fled_torch_enable || manager->fled_flash_enable)) {
		sm5714_info(" %s : PDO(%d) is ignored because of [torch(%d) or flash(%d)]\n",
				__func__, num, manager->fled_torch_enable, manager->fled_flash_enable);
		return;
	}

	if (psubpd->pd_noti.sink_status.selected_pdo_num == num) {
		if (psubpd->policy.state == PE_SNK_Ready)
			sm5714_usbpd_inform_pdo_list();
		return;
	} else if (num > psubpd->pd_noti.sink_status.available_pdo_num)
		psubpd->pd_noti.sink_status.selected_pdo_num =
			psubpd->pd_noti.sink_status.available_pdo_num;
	else if (num < 1)
		psubpd->pd_noti.sink_status.selected_pdo_num = 1;
	else
		psubpd->pd_noti.sink_status.selected_pdo_num = num;

	sm5714_info(" %s : PDO(%d) is selected to change\n",
		__func__, psubpd->pd_noti.sink_status.selected_pdo_num);
#if !IS_ENABLED(CONFIG_SM5714_DISABLE_PD)
	sm5714_usbpd_inform_event(psubpd, MANAGER_NEW_POWER_SRC);
#endif
}

int sm5714_select_pps(int num, int ppsVol, int ppsCur)
{
	struct sm5714_usbpd_data *psubpd = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = psubpd->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &psubpd->manager;
	bool vbus_short = false;
	int timeout = 0;

	if (!pdic_data->is_attached || pdic_data->shut_down) {
		sm5714_info(" %s : PDO(%d) is ignored because of plug detached\n",
				__func__, num);
		return -EPERM;
	}

	psubpd->phy_ops.get_short_state(psubpd, &vbus_short);

	if (vbus_short) {
		sm5714_info(" %s : PDO(%d) is ignored because of vbus short\n",
				__func__, num);
		return -EPERM;
	}

	if (num > 1 && (manager->fled_torch_enable || manager->fled_flash_enable)) {
		sm5714_info(" %s : PDO(%d) is ignored because of [torch(%d) or flash(%d)]\n",
				__func__, num, manager->fled_torch_enable, manager->fled_flash_enable);
		return -EPERM;
	}

	if (num > psubpd->pd_noti.sink_status.available_pdo_num) {
		sm5714_info("%s: request pdo num(%d) is higher that available pdo.\n", __func__, num);
		return -EINVAL;
	}

	psubpd->pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > psubpd->pd_noti.sink_status.power_list[num].max_voltage) {
		sm5714_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, psubpd->pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = psubpd->pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < psubpd->pd_noti.sink_status.power_list[num].min_voltage) {
		sm5714_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, psubpd->pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = psubpd->pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > psubpd->pd_noti.sink_status.power_list[num].max_current) {
		sm5714_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, psubpd->pd_noti.sink_status.power_list[num].max_current);
		ppsCur = psubpd->pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		sm5714_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	psubpd->pd_noti.sink_status.pps_voltage = ppsVol;
	psubpd->pd_noti.sink_status.pps_current = ppsCur;

	sm5714_info(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n", __func__,
		psubpd->pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	if ((pdic_data->rp_currentlvl == RP_CURRENT_LEVEL3) &&
			(psubpd->specification_revision == USBPD_REV_30)) {
		pdic_data->is_wait_sinktxok = false;
#if !IS_ENABLED(CONFIG_SM5714_DISABLE_PD)
		sm5714_usbpd_inform_event(psubpd, MANAGER_NEW_POWER_SRC);
#endif
	} else {
		pdic_data->is_wait_sinktxok = true;
		sm5714_info(" %s : PD 3.0, but SinkTxNG state.\n", __func__);
	}

	reinit_completion(&psubpd->pd_completion);
	timeout =
	    wait_for_completion_timeout(&psubpd->pd_completion,
					msecs_to_jiffies(1000));

	if (!timeout) {
		return -EBUSY;
	} else {
		if (pdic_data->is_wait_sinktxok && pdic_data->is_attached) {
#if !IS_ENABLED(CONFIG_SM5714_DISABLE_PD)
			sm5714_usbpd_inform_event(psubpd, MANAGER_NEW_POWER_SRC);
#endif
			pdic_data->is_wait_sinktxok = false;
			reinit_completion(&psubpd->pd_completion);
			if (!wait_for_completion_timeout(&psubpd->pd_completion,
					msecs_to_jiffies(1000)))
				return -EBUSY;
		}
	}

	return 0;
}
#ifdef CONFIG_BATTERY_NOTIFIER
int sm5714_get_apdo_max_power(unsigned int *pdo_pos,
		unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	int i;
	int ret = 0;
	int max_current = 0, max_voltage = 0, max_power = 0;

	if (!pd_data->pd_noti.sink_status.has_apdo) {
		sm5714_info("%s: pd don't have apdo\n",	__func__);
		return -1;
	}

	/* First, get TA maximum power from the fixed PDO */
	for (i = 1; i <= pd_data->pd_noti.sink_status.available_pdo_num; i++) {
		if (!(pd_data->pd_noti.sink_status.power_list[i].apdo)) {
			max_voltage = pd_data->pd_noti.sink_status.power_list[i].max_voltage;
			max_current = pd_data->pd_noti.sink_status.power_list[i].max_current;
			max_power = (max_voltage * max_current > max_power) ? (max_voltage * max_current) : max_power;
			*taMaxPwr = max_power;	/* mW */
		}
	}

	if (*pdo_pos == 0) {
		/* Get the proper PDO */
		for (i = 1; i <= pd_data->pd_noti.sink_status.available_pdo_num; i++) {
			if (pd_data->pd_noti.sink_status.power_list[i].apdo) {
				if (pd_data->pd_noti.sink_status.power_list[i].max_voltage >= *taMaxVol) {
					*pdo_pos = i;
					*taMaxVol = pd_data->pd_noti.sink_status.power_list[i].max_voltage;
					*taMaxCur = pd_data->pd_noti.sink_status.power_list[i].max_current;
					break;
				}
			}
			if (*pdo_pos)
				break;
		}

		if (*pdo_pos == 0) {
			sm5714_info("mv (%d) and ma (%d) out of range of APDO\n",
				*taMaxVol, *taMaxCur);
			ret = -EINVAL;
		}
	} else {
		/* If we already have pdo object position, we don't need to search max current */
		ret = -ENOTSUPP;
	}

	sm5714_info("%s : *pdo_pos(%d), *taMaxVol(%d), *maxCur(%d), *maxPwr(%d)\n",
		__func__, *pdo_pos, *taMaxVol, *taMaxCur, *taMaxPwr);

	return ret;
}
#endif
void sm5714_vpdo_auth(int auth, int d2d_type)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	int power_role = 0;

	pd_data->phy_ops.get_power_role(pd_data, &power_role);
	sm5714_info("%s auth(%d) d2d_type(%d) power_role(%d) auth_type(%d)\n",
		__func__, auth, d2d_type, power_role, pd_data->auth_type);

	if (d2d_type == 0)
		return;

	if (power_role == USBPD_SOURCE) {
		if (((pd_data->auth_type != AUTH_LOW_PWR) && (auth == AUTH_LOW_PWR)) ||
				((pd_data->auth_type != AUTH_HIGH_PWR) && (auth == AUTH_HIGH_PWR))) {
			if (auth == AUTH_LOW_PWR)
				sm5714_usbpd_change_source_cap(1, 500, 0);
			else
				sm5714_usbpd_change_source_cap(0, 1000, 0);
			sm5714_info("%s: change src %s -> %s\n", __func__,
				(auth == AUTH_LOW_PWR) ? "HIGH PWR" : "LOW PWR",
				(auth == AUTH_LOW_PWR) ? "LOW PWR" : "HIGH PWR");
		}
	} else if ((power_role == USBPD_SINK) &&
		(auth == AUTH_HIGH_PWR)) {
		sm5714_info("%s: preset vpdo auth for prswap snk to src\n", __func__);
	}
	/* set default src cap for detach or hard reset case */
	if (power_role != USBPD_SINK) {
		if ((pd_data->auth_type == AUTH_HIGH_PWR) && (auth == AUTH_NONE)) {
			sm5714_usbpd_change_source_cap(1, 500, 0);
			sm5714_info("%s: set to default src cap\n", __func__);
		}
	}
	pd_data->auth_type = auth;
	pd_data->d2d_type = d2d_type;
}

void sm5714_pd_manual_jig_ctrl(bool mode)
{
	struct sm5714_usbpd_data *psubpd = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = psubpd->phy_driver_data;

	sm5714_info("%s: mode(%d)\n", __func__, mode);
	sm5714_JIGON(pdic_data, mode);
}

void sm5714_pd_manual_ccopen_req(int cc_open)
{
	struct sm5714_usbpd_data *psubpd = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = psubpd->phy_driver_data;
	int cc_off = 0;

	cc_off = cc_open;

	sm5714_info("%s: is_on %d. If is_on is true, It means cc disconnect\n", __func__, cc_open);

	if ((cc_off && pdic_data->pd_support) || !cc_off)
		sm5714_cc_control_command(pdic_data, cc_off);
	else
		sm5714_info("%s skip. pd_support %d\n", __func__, pdic_data->pd_support);
}

void sm5714_usbpd_inform_pdo_list(void)
{
	PD_NOTI_ATTACH_TYPEDEF pd_notifier;
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;

	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
	pd_notifier.src = PDIC_NOTIFY_DEV_PDIC;
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATTERY;
#else
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATT;
#endif
	pd_notifier.id = PDIC_NOTIFY_ID_POWER_STATUS;
	pd_notifier.attach = 1;
	pd_notifier.pd = &pd_data->pd_noti;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pd_notifier,
			NULL, 1/* pdic_attach */);
#endif
}

void sm5714_usbpd_start_discover_msg_handler(struct work_struct *work)
{
	struct sm5714_usbpd_manager_data *manager =
		container_of(work, struct sm5714_usbpd_manager_data,
				start_discover_msg_handler.work);
#if !IS_ENABLED(CONFIG_SM5714_DISABLE_PD)
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
#endif

	sm5714_info("%s: call start discover handler\n", __func__);

	if (manager->alt_sended == 0 && manager->vdm_en == 1) {
#if !IS_ENABLED(CONFIG_SM5714_DISABLE_PD)
		sm5714_usbpd_inform_event(pd_data->pd_noti.pusbpd,
						MANAGER_SEND_DISCOVER_IDENTITY);
#endif
		manager->alt_sended = 1;
	}
}

void sm5714_usbpd_start_discover_msg_cancel(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->start_discover_msg_handler);
}

void sm5714_usbpd_start_dex_discover_msg_cancel(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->start_dex_discover_msg_handler);
}

void sm5714_usbpd_change_available_pdo(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	PD_NOTI_ATTACH_TYPEDEF pd_notifier;

	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	pd_notifier.src = PDIC_NOTIFY_DEV_PDIC;
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATTERY;
#else
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATT;
#endif
	pd_notifier.id = PDIC_NOTIFY_ID_POWER_STATUS;
	pd_notifier.attach = 1;
	pd_notifier.pd = &pd_data->pd_noti;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pd_notifier,
			NULL, 1/* pdic_attach */);
#endif
}

void sm5714_request_default_power_src(void)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	int pdo_num = pd_data->pd_noti.sink_status.selected_pdo_num;

	sm5714_info(" %s : policy->state = (0x%x), pdo_num = %d, max vol = %d\n", __func__,
		pd_data->policy.state, pdo_num,
		pd_data->pd_noti.sink_status.power_list[pdo_num].max_voltage);

	if ((pdo_num > 1) &&
		(pd_data->pd_noti.sink_status.power_list[pdo_num].max_voltage > 5000)) {
		pd_data->pd_noti.sink_status.available_pdo_num = 1;
		pd_data->pd_noti.sink_status.has_apdo = false;
		sm5714_usbpd_change_available_pdo(pd_data->dev);
	}
}
EXPORT_SYMBOL(sm5714_request_default_power_src);

int sm5714_usbpd_check_fled_state(bool enable, u8 mode)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	int pdo_num = pd_data->pd_noti.sink_status.selected_pdo_num;

	sm5714_info("[%s] enable(%d), mode(%d)\n", __func__, enable, mode);

	if (mode == FLED_MODE_TORCH) { /* torch */
		cancel_delayed_work(&manager->new_power_handler);
		sm5714_info("[%s] new_power_handler cancel\n", __func__);

		manager->fled_torch_enable = enable;
	} else if (mode == FLED_MODE_FLASH) { /* flash */
		manager->fled_flash_enable = enable;
	}

	sm5714_info("[%s] fled_torch_enable(%d), fled_flash_enable(%d)\n", __func__,
		manager->fled_torch_enable, manager->fled_flash_enable);

	if (manager->fled_torch_enable || manager->fled_flash_enable) {
		if (pdic_data->pd_support && pd_data->pd_noti.sink_status.has_apdo) {
			pd_data->pd_noti.sink_status.available_pdo_num = 1;
			pd_data->pd_noti.sink_status.has_apdo = false;
			sm5714_usbpd_change_available_pdo(pd_data->dev);
		}
	}

	if ((manager->fled_torch_enable == false) &&
			(manager->fled_flash_enable == false)) {
		if ((mode == FLED_MODE_TORCH) && (enable == false)) {
			cancel_delayed_work(&manager->new_power_handler);
			schedule_delayed_work(&manager->new_power_handler,
				msecs_to_jiffies(5000));
			sm5714_info("[%s] new_power_handler start(5sec)\n", __func__);
		} else {
			if (pdic_data->is_attached && (pdo_num > 0)) {
				pd_data->pd_noti.sink_status.available_pdo_num = manager->origin_available_pdo_num;
				if (pd_data->pd_noti.sink_status.power_list[pd_data->pd_noti.sink_status.available_pdo_num].apdo)
					pd_data->pd_noti.sink_status.has_apdo = true;
				sm5714_usbpd_change_available_pdo(pd_data->dev);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(sm5714_usbpd_check_fled_state);

int sm5714_usbpd_uvdm_ready(void)
{
	int uvdm_ready = 0;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct sm5714_phydrv_data *phy_data;
	struct sm5714_usbpd_data *pd_data;
	struct sm5714_usbpd_manager_data *manager;

	if (!pdic_device) {
		sm5714_err("%s: pdic_device is null.\n", __func__);
		return -ENODEV;
	}
	pdic_data = dev_get_drvdata(pdic_device);
	if (!pdic_data) {
		sm5714_err("pdic_data is null\n");
		return -ENXIO;
	}
	phy_data = pdic_data->drv_data;
	if (!phy_data) {
		sm5714_err("phy_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
		sm5714_err("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		sm5714_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}

	if (manager->is_samsung_accessory_enter_mode && manager->pn_flag)
		uvdm_ready = 1;

	sm5714_info("%s: uvdm ready is %s, entermode : %d, pn_flag : %d\n", __func__,
		uvdm_ready ? "true" : "false",
		manager->is_samsung_accessory_enter_mode,
		manager->pn_flag);
	return uvdm_ready;
}

void sm5714_usbpd_uvdm_close(void)
{
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct sm5714_phydrv_data *phy_data;
	struct sm5714_usbpd_data *pd_data;
	struct sm5714_usbpd_manager_data *manager;

	if (!pdic_device) {
		sm5714_err("%s: pdic_device is null.\n", __func__);
		return;
	}
	pdic_data = dev_get_drvdata(pdic_device);
	if (!pdic_data) {
		sm5714_err("pdic_data is null\n");
		return;
	}
	phy_data = pdic_data->drv_data;
	if (!phy_data) {
		sm5714_err("phy_data is null\n");
		return;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
		sm5714_err("pd_data is null\n");
		return;
	}
	manager = &pd_data->manager;
	if (!manager) {
		sm5714_err("%s: manager is null\n", __func__);
		return;
	}
	manager->uvdm_out_ok = 1;
	manager->uvdm_in_ok = 1;
	wake_up(&manager->uvdm_out_wq);
	wake_up(&manager->uvdm_in_wq);
	sm5714_info("%s\n", __func__);
}

int sm5714_usbpd_uvdm_out_request_message(void *data, int size)
{
	uint8_t *SEC_DATA;
	uint8_t rcv_data[MAX_INPUT_DATA] = {0,};
	int need_set_cnt = 0;
	int cur_set_data = 0;
	int cur_set_num = 0;
	int remained_data_size = 0;
	int accumulated_data_size = 0;
	int received_data_index = 0;
	int time_left = 0;
	int i;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct sm5714_phydrv_data *phy_data;
	struct sm5714_usbpd_data *pd_data;
	struct sm5714_usbpd_manager_data *manager;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	if (!pdic_device) {
		sm5714_err("%s: pdic_device is null.\n", __func__);
		return -ENODEV;
	}
	pdic_data = dev_get_drvdata(pdic_device);
	if (!pdic_data) {
		sm5714_err("pdic_data is null\n");
		return -ENXIO;
	}
	phy_data = pdic_data->drv_data;
	if (!phy_data) {
		sm5714_err("phy_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
		sm5714_err("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		sm5714_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}

	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 7);
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);
	set_endian(data, rcv_data, size);

	if (size <= 1) {
		sm5714_info("%s - process short data\n", __func__);
		/* VDM Header + 6 VDOs = MAX 7 */
		manager->uvdm_msg_header.num_data_objs = 2;
		manager->uvdm_data_obj[1].sec_uvdm_header.total_set_num = 1;
		manager->uvdm_data_obj[1].sec_uvdm_header.data = rcv_data[0];
		manager->uvdm_out_ok = 0;
		sm5714_usbpd_inform_event(
			pd_data, MANAGER_UVDM_SEND_MESSAGE);
		time_left =
			wait_event_interruptible_timeout(
				manager->uvdm_out_wq, manager->uvdm_out_ok,
				msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (manager->uvdm_out_ok == 2)	{
			sm5714_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (manager->uvdm_out_ok == 3) {
			sm5714_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			sm5714_err("%s timeout\n", __func__);
#if defined(CONFIG_USB_NOTIFY_PROC_LOG) && defined(CONFIG_USB_HW_PARAM)
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;
	} else {
		sm5714_info("%s - process long data\n", __func__);
		need_set_cnt = set_uvdmset_count(size);
		manager->uvdm_first_req = true;
		manager->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (manager->uvdm_first_req)
			set_sec_uvdm_header(&manager->uvdm_data_obj[0],
					manager->Product_ID,
					SEC_UVDM_LONG_DATA,
					SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(
					&manager->uvdm_data_obj[0], manager->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data =
				get_data_size(manager->uvdm_first_req, remained_data_size);

			sm5714_info("%s - cur_set_data:%d, size:%d, cur_set_num:%d\n",
				__func__, cur_set_data, size, cur_set_num);

			if (manager->uvdm_first_req) {
				SEC_DATA =
					(uint8_t *)&manager->uvdm_data_obj[3];
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			} else {
				SEC_DATA =
					(uint8_t *)&manager->uvdm_data_obj[2];
				for (i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&manager->uvdm_data_obj[0]);
			manager->uvdm_out_ok = 0;

			sm5714_usbpd_inform_event(
				pd_data, MANAGER_UVDM_SEND_MESSAGE);

			time_left =
				wait_event_interruptible_timeout(
					manager->uvdm_out_wq, manager->uvdm_out_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (manager->uvdm_out_ok == 2 ||
				manager->uvdm_out_ok == 4)	{
				sm5714_err("%s NAK\n", __func__);
				return -ENODATA;
			} else if (manager->uvdm_out_ok == 3 ||
					   manager->uvdm_out_ok == 5) {
				sm5714_err("%s BUSY\n", __func__);
				return -EBUSY;
			} else if (!time_left) {
				sm5714_err("%s timeout\n", __func__);
#if defined(CONFIG_USB_NOTIFY_PROC_LOG) && defined(CONFIG_USB_HW_PARAM)
				event = NOTIFY_EXTRA_UVDM_TIMEOUT;
				store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
				return -ETIME;
			} else if (time_left == -ERESTARTSYS)
				return -ERESTARTSYS;

			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (manager->uvdm_first_req)
				manager->uvdm_first_req = false;
			cur_set_num++;
		}
	}
	return size;
}

int sm5714_usbpd_uvdm_in_request_message(void *data)
{
	uint8_t in_data[MAX_INPUT_DATA] = {0,};

	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	msg_header_type	uvdm_msg_header;

	int cur_set_data = 0;
	int cur_set_num = 0;
	int total_set_num = 0;
	int rcv_data_size = 0;
	int total_rcv_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int cal_checksum = 0;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct sm5714_phydrv_data *phy_data;
	struct sm5714_usbpd_data *pd_data;
	struct sm5714_usbpd_manager_data *manager;
	struct sm5714_policy_data *policy;
#if defined(CONFIG_USB_NOTIFY_PROC_LOG) && defined(CONFIG_USB_HW_PARAM)
	int event;
#endif

	if (!pdic_device) {
		sm5714_err("%s: pdic_device is null.\n", __func__);
		return -ENODEV;
	}
	pdic_data = dev_get_drvdata(pdic_device);
	if (!pdic_data) {
		sm5714_err("pdic_data is null\n");
		return -ENXIO;
	}
	phy_data = pdic_data->drv_data;
	if (!phy_data) {
		sm5714_err("phy_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
		sm5714_err("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		sm5714_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}
	policy = &pd_data->policy;
	if (!policy) {
		sm5714_err("%s: policy is null\n", __func__);
		return -ENXIO;
	}
	sm5714_info("%s\n", __func__);

	manager->uvdm_dir = DIR_IN;
	manager->uvdm_first_req = true;
	uvdm_msg_header.word = policy->rx_msg_header.word;

	/* 2. Common : Fill the MSGHeader */
	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if (manager->uvdm_first_req)
		set_sec_uvdm_header(&manager->uvdm_data_obj[0],
				manager->Product_ID,
				SEC_UVDM_LONG_DATA,
				SEC_UVDM_ININIATOR, DIR_IN, 0, 0);
	/* 5. Send data to PDIC */
	manager->uvdm_in_ok = 0;
	sm5714_usbpd_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_event_interruptible_timeout(
					manager->uvdm_in_wq, manager->uvdm_in_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (manager->uvdm_in_ok == 2)	{
			sm5714_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (manager->uvdm_in_ok == 3) {
			sm5714_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			sm5714_err("%s timeout\n", __func__);
#if defined(CONFIG_USB_NOTIFY_PROC_LOG) && defined(CONFIG_USB_HW_PARAM)
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;

		/* read data */
		uvdm_msg_header.word = policy->rx_msg_header.word;
		for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
			uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

		if (manager->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[1].object;
			SEC_TX_HEADER.object = uvdm_data_obj[2].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				sm5714_info("%s - process short data\n", __func__);
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			}
			/* 1. check the data size received */
			size = SEC_TX_HEADER.total_size;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			total_set_num = SEC_RES_HEADER.total_set_num;

			manager->uvdm_first_req = false;
			/* 2. copy data to buffer */
			for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[3+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
			manager->uvdm_first_req = false;
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
		}
		/* 3. Check Checksum */
		SEC_TX_TAILER.object = uvdm_data_obj[6].object;
		cal_checksum =
			get_checksum((char *)&uvdm_data_obj[0], 4, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		/* 5. Common : Fill the MSGHeader */
		set_msg_header(&manager->uvdm_msg_header,
				USBPD_Vendor_Defined, 2);
		/* 5.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&manager->uvdm_data_obj[0],
				SAMSUNG_VENDOR_ID, 0);
		/* 5.2. Common : Fill the First SEC_VDMHeader*/
		set_sec_uvdm_rx_header(&manager->uvdm_data_obj[0],
				cur_set_num, cur_set_data, ack);
		manager->uvdm_in_ok = 0;
		sm5714_usbpd_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
	} while (cur_set_num < total_set_num);

	set_endian(in_data, data, size);
	return size;
}

static void sm5714_usbpd_receive_samsung_uvdm_message(
					struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	int i = 0;
	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	s_uvdm_header SEC_UVDM_RES_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;

	uvdm_msg_header.word = policy->rx_msg_header.word;

	for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
		uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

	uvdm_msg_header.word = policy->rx_msg_header.word;
	sm5714_info("%s dir %s\n", __func__, (manager->uvdm_dir == DIR_OUT)
		? "OUT":"IN");

	if (manager->uvdm_dir == DIR_OUT) {
		if (manager->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				SEC_UVDM_RX_HEADER.object = uvdm_data_obj[2].object;
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
						manager->uvdm_out_ok = 1;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
						sm5714_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
						manager->uvdm_out_ok = 4;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
						sm5714_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
						manager->uvdm_out_ok = 5;
					}
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					sm5714_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					manager->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					sm5714_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					manager->uvdm_out_ok = 3;
				}
			} else if (SEC_UVDM_RES_HEADER.data_type == TYPE_SHORT) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					manager->uvdm_out_ok = 1;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					sm5714_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					manager->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					sm5714_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					manager->uvdm_out_ok = 3;
				}
			}
		} else {
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
				manager->uvdm_out_ok = 1;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
				sm5714_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
				manager->uvdm_out_ok = 4;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
				sm5714_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
				manager->uvdm_out_ok = 5;
			}
		}
		wake_up(&manager->uvdm_out_wq);
	} else { /* DIR_IN */
		if (manager->uvdm_first_req) { /* Long = Short */
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
				manager->uvdm_in_ok = 1;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
				sm5714_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
				manager->uvdm_in_ok = 2;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
				sm5714_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
				manager->uvdm_in_ok = 3;
			}
		} else {
			/* IN response case, SEC_TX_HEADER has no ACK message. So uvdm_is_ok is always 1. */
			manager->uvdm_in_ok = 1;
		}
		wake_up(&manager->uvdm_in_wq);
	}
}

void sm5714_usbpd_dp_detach(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	sm5714_info("%s: dp_is_connect %d\n", __func__, manager->dp_is_connect);

	sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_USB_DP,
		PDIC_NOTIFY_ID_USB_DP, 0, manager->dp_hs_connect, 0);
	sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_DP,
		PDIC_NOTIFY_ID_DP_CONNECT, 0, 0, 0);
	manager->dp_selected_pin = 0;
	manager->dp_is_connect = 0;
	manager->dp_hs_connect = 0;
	manager->pin_assignment = 0;
}

void sm5714_usbpd_acc_detach(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_info("%s: acc_type %d\n",
		__func__, manager->acc_type);
	manager->alt_sended = 0;
	manager->vdm_en = 0;
	manager->ext_sended = 0;
	if (manager->acc_type != PDIC_DOCK_DETACHED) {
		if (manager->acc_type == PDIC_DOCK_HMT)
			schedule_delayed_work(&manager->acc_detach_handler,
				msecs_to_jiffies(GEAR_VR_DETACH_WAIT_MS));
		else
			schedule_delayed_work(&manager->acc_detach_handler,
				msecs_to_jiffies(0));
		sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_ALL,
			PDIC_NOTIFY_ID_CLEAR_INFO, PDIC_NOTIFY_ID_DEVICE_INFO, 0, 0);
	}
}

static void sm5714_usbpd_manager_new_power_handler(struct work_struct *wk)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	int pdo_num = pd_data->pd_noti.sink_status.selected_pdo_num;

	sm5714_info("[%s] pdic_data->is_attached = %d\n", __func__, pdic_data->is_attached);
	if (pdic_data->is_attached && (pdo_num > 0)) {
		pd_data->pd_noti.sink_status.available_pdo_num = manager->origin_available_pdo_num;
		if (pd_data->pd_noti.sink_status.power_list[pd_data->pd_noti.sink_status.available_pdo_num].apdo)
			pd_data->pd_noti.sink_status.has_apdo = true;
		sm5714_usbpd_change_available_pdo(pd_data->dev);
	}
#endif
}

static void sm5714_usbpd_acc_detach_handler(struct work_struct *wk)
{
	struct sm5714_usbpd_manager_data *manager =
		container_of(wk, struct sm5714_usbpd_manager_data,
				acc_detach_handler.work);

	sm5714_info("%s: acc_type %d\n",
		__func__, manager->acc_type);
	if (manager->acc_type != PDIC_DOCK_DETACHED) {
		if (manager->acc_type != PDIC_DOCK_NEW)
			pdic_send_dock_intent(PDIC_DOCK_DETACHED);
		pdic_send_dock_uevent(manager->Vendor_ID,
				manager->Product_ID,
				PDIC_DOCK_DETACHED);
		manager->acc_type = PDIC_DOCK_DETACHED;
		manager->Vendor_ID = 0;
		manager->Product_ID = 0;
		manager->Device_Version = 0;
		manager->SVID_0 = 0;
		manager->SVID_1 = 0;
		manager->SVID_DP = 0;
		manager->Standard_Vendor_ID = 0;
		manager->is_samsung_accessory_enter_mode = 0;
		manager->uvdm_out_ok = 1;
		manager->uvdm_in_ok = 1;
		wake_up(&manager->uvdm_out_wq);
		wake_up(&manager->uvdm_in_wq);
	}
}

int sm5714_usbpd_check_accessory(
		struct sm5714_usbpd_manager_data *manager)
{
	uint16_t vid = manager->Vendor_ID;
	uint16_t pid = manager->Product_ID;
	uint16_t acc_type = PDIC_DOCK_DETACHED;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	/* detect Gear VR */
	if (manager->acc_type == PDIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID ... GEARVR_PRODUCT_ID_5:
				acc_type = PDIC_DOCK_HMT;
				sm5714_info("%s : Samsung Gear VR connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_VR_USE_COUNT);
#endif
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = PDIC_DOCK_DEX;
				sm5714_info("%s : Samsung DEX connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case DEXPAD_PRODUCT_ID:
				acc_type = PDIC_DOCK_DEXPAD;
				sm5714_info("%s : Samsung DEX PAD connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case HDMI_PRODUCT_ID:
				acc_type = PDIC_DOCK_HDMI;
				sm5714_info("%s : Samsung HDMI adapter(EE-HG950) connected\n",
					__func__);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				if (pid == FRIENDS_PRODUCT_ID)
					sm5714_info("%s : Kakao Friends Stand connected\n",
						__func__);
				else
					sm5714_info("%s : default device connected\n",
						__func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch (pid) {
			case MPA_PRODUCT_ID:
				acc_type = PDIC_DOCK_MPA;
				sm5714_info("%s : Samsung MPA connected.\n",
					__func__);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				sm5714_info("%s : default device connected\n",
					__func__);
				break;
			}
		} else {
			acc_type = PDIC_DOCK_NEW;
			sm5714_info("%s : unknown device connected\n",
				__func__);
		}
		manager->acc_type = acc_type;
	} else
		acc_type = manager->acc_type;

	if (acc_type != PDIC_DOCK_NEW)
		pdic_send_dock_intent(acc_type);

	pdic_send_dock_uevent(vid, pid, acc_type);
	return (acc_type != PDIC_DOCK_NEW || vid == SAMSUNG_VENDOR_ID) ? 1 : 0;
}

void sm5714_usbpd_power_ready(struct device *dev,
	PDIC_OTP_MODE power_role)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct sm5714_policy_data *policy = &pd_data->policy;
	PD_NOTI_ATTACH_TYPEDEF pd_notifier;
#endif
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	bool short_cable = false;
#if defined(CONFIG_TYPEC)
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif
	if (!pdic_data->pd_support) {
		pd_data->phy_ops.get_short_state(pd_data, &short_cable);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (short_cable) {
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT) && defined(CONFIG_SM5714_SUPPORT_SBU)
			if (pdic_data->is_lpcharge &&
					pdic_data->is_1st_short && pdic_data->is_2nd_short)
				sm5714_usbpd_delayed_sbu_short_notify(pd_data);
#endif
			pd_data->pd_noti.sink_status.available_pdo_num = 1;
			pd_data->pd_noti.sink_status.power_list[1].max_current =
				pd_data->pd_noti.sink_status.power_list[1].max_current > manager->short_cable_current ?
				manager->short_cable_current : pd_data->pd_noti.sink_status.power_list[1].max_current;
			pd_data->pd_noti.sink_status.has_apdo = false;
		}
#endif
		pdic_data->pd_support = 1;
		sm5714_info("%s : pd_support : %d, short_cable : %d\n",
				__func__, pdic_data->pd_support, short_cable);
#if defined(CONFIG_TYPEC)
		mode = sm5714_get_pd_support(pdic_data);
		typec_set_pwr_opmode(pdic_data->port, mode);
#endif
		send_otg_notify(get_otg_notify(), NOTIFY_EVENT_PD_CONTRACT, 1);
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (power_role == TYPE_C_ATTACH_SNK &&
		(policy->last_state == PE_SNK_Transition_Sink ||
		policy->last_state == PE_SNK_Get_Source_Cap_Ext)) {
		if (policy->send_sink_cap) {
			pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
			policy->send_sink_cap = 0;
		} else
			pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
		pd_notifier.src = PDIC_NOTIFY_DEV_PDIC;
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATTERY;
#else
	pd_notifier.dest = PDIC_NOTIFY_DEV_BATT;
#endif
		pd_notifier.id = PDIC_NOTIFY_ID_POWER_STATUS;
		pd_notifier.attach = 1;
		pd_notifier.pd = &pd_data->pd_noti;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pd_notifier,
				NULL, 1/* pdic_attach */);
#endif
	}
#endif
}

static int sm5714_usbpd_command_to_policy(struct device *dev,
		sm5714_usbpd_manager_command_type command)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	manager->cmd = command;

	sm5714_usbpd_kick_policy_work(dev);

	return 0;
}

void sm5714_usbpd_start_dex_discover_msg_handler(struct work_struct *work)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;
	sm5714_info("%s: call start dex discover handler\n", __func__);

	sm5714_usbpd_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_DISCOVER_MODE);
}

void sm5714_usbpd_inform_event(struct sm5714_usbpd_data *pd_data,
		sm5714_usbpd_manager_event_type event)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	manager->event = event;

	switch (event) {
	case MANAGER_SEND_DISCOVER_IDENTITY:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_IDENTITY);
		break;
	case MANAGER_DISCOVER_IDENTITY_ACKED:
		sm5714_usbpd_get_identity(pd_data);
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_SVID);
		break;
	case MANAGER_DISCOVER_SVID_ACKED:
		sm5714_usbpd_get_svids(pd_data);
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_MODE);
		break;
	case MANAGER_DISCOVER_MODE_ACKED:
		sm5714_usbpd_get_modes(pd_data);
		sm5714_usbpd_set_usb_safe_mode(pdic_data);
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_ENTER_MODE);
		break;
	case MANAGER_ENTER_MODE_ACKED:
		sm5714_usbpd_enter_mode(pd_data);
		if (manager->SVID_0 == PD_SID_1)
			sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_STATUS_UPDATE);
		sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_ALL,
			PDIC_NOTIFY_ID_DEVICE_INFO, manager->Vendor_ID,
			manager->Product_ID, manager->Device_Version);
		break;
	case MANAGER_STATUS_UPDATE_ACKED:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DisplayPort_Configure);
		break;
	case MANAGER_DisplayPort_Configure_ACKED:
		break;
	case MANAGER_NEW_POWER_SRC:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_NEW_POWER_SRC);
		break;
	case MANAGER_UVDM_SEND_MESSAGE:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_UVDM_SEND_MESSAGE);
		break;
	case MANAGER_UVDM_RECEIVE_MESSAGE:
		sm5714_usbpd_receive_samsung_uvdm_message(pd_data);
		break;
	case MANAGER_PR_SWAP_REQUEST:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_PR_SWAP);
		break;
	case MANAGER_DR_SWAP_REQUEST:
		sm5714_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_DR_SWAP);
		break;
	case MANAGER_DISCOVER_IDENTITY_NAKED:
	case MANAGER_DISCOVER_SVID_NAKED:
	case MANAGER_DISCOVER_MODE_NAKED:
	case MANAGER_ENTER_MODE_NAKED:
		sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_ALL,
			PDIC_NOTIFY_ID_DEVICE_INFO, manager->Vendor_ID,
			manager->Product_ID, manager->Device_Version);
		break;
	default:
		sm5714_info("%s: not matched event(%d)\n", __func__, event);
	}
}

#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
void sm5714_set_enable_alternate_mode(int mode)
{
	struct sm5714_usbpd_data *pd_data = sm5714_g_pd_data;	
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	static int check_is_driver_loaded;
	static int prev_alternate_mode;
	int data_role = 0;

	if ((mode & ALTERNATE_MODE_NOT_READY) &&
	    (mode & ALTERNATE_MODE_READY)) {
		sm5714_info("%s: mode is invalid!", __func__);
		return;
	}
	if ((mode & ALTERNATE_MODE_START) && (mode & ALTERNATE_MODE_STOP)) {
		sm5714_info("%s: mode is invalid!", __func__);
		return;
	}
	if (mode & ALTERNATE_MODE_RESET) {
		sm5714_info("%s: mode is reset! check_is_driver_loaded=%d, prev_alternate_mode=%d",
			__func__, check_is_driver_loaded, prev_alternate_mode);
		if (check_is_driver_loaded &&
		    (prev_alternate_mode == ALTERNATE_MODE_START)) {

			sm5714_info("%s: [No process] alternate mode is reset as start!", __func__);
			prev_alternate_mode = ALTERNATE_MODE_START;
		} else if (check_is_driver_loaded &&
			   (prev_alternate_mode == ALTERNATE_MODE_STOP)) {
			sm5714_info("%s: [No process] alternate mode is reset as stop!", __func__);
			prev_alternate_mode = ALTERNATE_MODE_STOP;
		} else {
			;
		}
	} else {
		if (mode & ALTERNATE_MODE_NOT_READY) {
			check_is_driver_loaded = 0;
			sm5714_info("%s: alternate mode is not ready!", __func__);
		} else if (mode & ALTERNATE_MODE_READY) {
			check_is_driver_loaded = 1;
			sm5714_info("%s: alternate mode is ready!", __func__);
		} else {
			;
		}

		if (mode & ALTERNATE_MODE_START) {
			pd_data->altmode_enable = 1;
			prev_alternate_mode = ALTERNATE_MODE_START;
			sm5714_info("%s: alternate mode is started!\n", __func__);
			pd_data->phy_ops.get_data_role(pd_data, &data_role);
			if (data_role == USBPD_DFP) {
				manager->alt_sended = 0;
				manager->vdm_en = 0;
				sm5714_info("%s : request vdm for DFP\n", __func__);
				sm5714_usbpd_vdm_request_enabled(pd_data);
			}
		} else if (mode & ALTERNATE_MODE_STOP) {
			pd_data->altmode_enable = 0;
			sm5714_info("%s: alternate mode is stopped!\n", __func__);
		}
	}
}
#endif

bool sm5714_usbpd_vdm_request_enabled(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
	sm5714_info("%s: alt_sended : %d, vdm_en : %d\n", __func__, manager->alt_sended, manager->vdm_en);
#endif
	if (manager->alt_sended == 1 && manager->vdm_en == 1)
		return true;

	manager->vdm_en = 1;

	schedule_delayed_work(&manager->start_discover_msg_handler, msecs_to_jiffies(tDiscoverIdentity));
	return true;
}

bool sm5714_usbpd_dex_vdm_request(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	schedule_delayed_work(&manager->start_dex_discover_msg_handler, msecs_to_jiffies(tDiscoverIdentity));
	return true;
}

bool sm5714_usbpd_ext_request_enabled(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	bool ret = false;

	if ((pd_data->pd_noti.sink_status.rp_currentlvl == RP_CURRENT_LEVEL3) &&
			(pd_data->specification_revision == USBPD_REV_30)) {
		if (manager->ext_sended)
			ret = false;
		else {
			manager->ext_sended = 1;
			ret = true;
		}
	}
	sm5714_info("%s: rp_currentlvl(%d), %s\n", __func__, pd_data->pd_noti.sink_status.rp_currentlvl,
			pd_data->pd_noti.sink_status.rp_currentlvl == RP_CURRENT_LEVEL3 ? "SINK TX OK" : "SINK TX NG");
	return ret;
}

bool sm5714_usbpd_power_role_swap(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	return manager->power_role_swap;
}

bool sm5714_usbpd_vconn_source_swap(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	return manager->vconn_source_swap;
}

static void sm5714_disable_bc12(struct sm5714_phydrv_data *pdic_data)
{
#if defined(CONFIG_IF_CB_MANAGER)
	muic_set_bc12(pdic_data->man, 0);
#endif
}

void sm5714_usbpd_turn_on_source(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_info("%s\n", __func__);
	sm5714_disable_bc12(pdic_data);
	sm5714_vbus_turn_on_ctrl(pdic_data, 1);
}

void sm5714_usbpd_turn_on_reverse_booster(struct sm5714_usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;

	sm5714_info("%s\n", __func__);
	val.intval = 1;
	 /* disable dc reverse boost before otg on */
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, val);
#endif
}

void sm5714_usbpd_turn_off_reverse_booster(struct sm5714_usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval val;

	sm5714_info("%s\n", __func__);
	val.intval = 0;
	 /* disable dc reverse boost before otg on */
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, val);
#endif
}

void sm5714_usbpd_turn_off_power_supply(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_info("%s\n", __func__);

	sm5714_vbus_turn_on_ctrl(pdic_data, 0);
	sm5714_usbpd_set_vbus_dischg_gpio(pdic_data, 1);
	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SRCTOSNK;

	sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
		PDIC_NOTIFY_DEV_BATTERY,
#else
		PDIC_NOTIFY_DEV_BATT,
#endif
		PDIC_NOTIFY_ID_POWER_STATUS,
		0/*attach*/, 0, 0);
}

void sm5714_usbpd_turn_off_power_sink(struct sm5714_usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_info("%s\n", __func__);

	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.sink_status.available_pdo_num = 0;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;

	sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
		PDIC_NOTIFY_DEV_BATTERY,
#else
		PDIC_NOTIFY_DEV_BATT,
#endif
		PDIC_NOTIFY_ID_POWER_STATUS,
		0/*attach*/, 0, 0);
#endif
}

bool sm5714_usbpd_data_role_swap(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_info("%s - %s, %s, data_role_swap : %d\n", __func__,
		pdic_data->typec_power_role == TYPEC_DEVICE ? "ufp":"dfp",
		pdic_data->typec_data_role == TYPEC_SINK ? "snk":"src",
		manager->data_role_swap);

	return manager->data_role_swap;
}

int sm5714_usbpd_get_identity(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	manager->Vendor_ID = policy->rx_data_obj[1].id_header.usb_vendor_id;
	manager->Product_ID = policy->rx_data_obj[3].product_vdo.product_id;
	manager->Device_Version =
		policy->rx_data_obj[3].product_vdo.device_version;

	sm5714_info("%s, Vendor_ID : 0x%x, Product_ID : 0x%x, Device Version : 0x%x\n",
		__func__, manager->Vendor_ID, manager->Product_ID,
		manager->Device_Version);

	if (sm5714_usbpd_check_accessory(manager))
		sm5714_info("%s, Samsung Accessory Connected.\n", __func__);

	return 0;
}

int sm5714_usbpd_get_svids(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT)
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	int timeleft = 0;
#endif
	int i = 0, vdo_num = 0;

	vdo_num = policy->rx_msg_header.num_data_objs - 1;
	manager->SVID_DP = 0;
	manager->SVID_0 = policy->rx_data_obj[1].vdm_svid.svid_0;
	manager->SVID_1 = policy->rx_data_obj[1].vdm_svid.svid_1;

	for (i = 0; i < vdo_num; i++) {
		if (policy->rx_data_obj[i+1].vdm_svid.svid_0 == PD_SID_1) {
			manager->SVID_0 = policy->rx_data_obj[i+1].vdm_svid.svid_0;
			sm5714_info("%s, SVID_%d : 0x%x\n", __func__, (i * 2) + 1, manager->SVID_0);
			break;
		} else if (policy->rx_data_obj[i+1].vdm_svid.svid_1 == PD_SID_1) {
			manager->SVID_0 = policy->rx_data_obj[i+1].vdm_svid.svid_1;
			sm5714_info("%s, SVID_%d : 0x%x\n", __func__, (i * 2) + 2, manager->SVID_0);
			break;
		}
	}
	if (((manager->SVID_0 != PD_SID_1) && (manager->SVID_0 != SAMSUNG_VENDOR_ID)) &&
			((manager->SVID_1 != PD_SID_1) && (manager->SVID_1 != SAMSUNG_VENDOR_ID))) {
		sm5714_info("%s, No have availible SVIDs.\n", __func__);
		manager->SVID_0 = PD_SID_1;
	}

	if (manager->SVID_0 == PD_SID_1)
		manager->SVID_DP = PD_SID_1;
	sm5714_info("%s, SVID_0 : 0x%x, SVID_1 : 0x%x, SVID_DP : 0x%x\n", __func__,
		manager->SVID_0, manager->SVID_1, manager->SVID_DP);

#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT)
	if (manager->SVID_DP == PD_SID_1) {
		manager->dp_is_connect = 1;
		/* If you want to support USB SuperSpeed when you connect
		 * Display port dongle, You should change dp_hs_connect depend
		 * on Pin assignment.If DP use 4lane(Pin Assignment C,E,A),
		 * dp_hs_connect is 1. USB can support HS.If DP use
		 * 2lane(Pin Assignment B,D,F), dp_hs_connect is 0. USB
		 * can support SS
		 */
		manager->dp_hs_connect = 1;

		timeleft = wait_event_interruptible_timeout(pdic_data->host_turn_on_wait_q,
					pdic_data->host_turn_on_event && !pdic_data->detach_done_wait
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
					&& !pdic_data->wait_entermode
#endif
					, (pdic_data->host_turn_on_wait_time)*HZ);
		sm5714_info("%s host turn on wait = %d\n", __func__, timeleft);
		/* notify to dp event */
		sm5714_pdic_event_work(pdic_data,
				PDIC_NOTIFY_DEV_DP,
				PDIC_NOTIFY_ID_DP_CONNECT,
				PDIC_NOTIFY_ATTACH,
				manager->Vendor_ID,
				manager->Product_ID);
		/* recheck this notifier */
		sm5714_pdic_event_work(pdic_data,
			PDIC_NOTIFY_DEV_USB_DP,
			PDIC_NOTIFY_ID_USB_DP,
			manager->dp_is_connect /*attach*/,
			manager->dp_hs_connect, 0);
	}
#endif
	return 0;
}

int sm5714_usbpd_get_modes(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	manager->Standard_Vendor_ID =
			policy->rx_data_obj[0].structured_vdm.svid;

	sm5714_info("%s, Standard_Vendor_ID = 0x%x\n", __func__,
		manager->Standard_Vendor_ID);

	return 0;
}

int sm5714_usbpd_enter_mode(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;

	manager->Standard_Vendor_ID =
		policy->rx_data_obj[0].structured_vdm.svid;
	if (manager->Standard_Vendor_ID == SAMSUNG_VENDOR_ID)
		manager->is_samsung_accessory_enter_mode = 1;

	sm5714_info("%s, entermode = %s\n", __func__,
		manager->is_samsung_accessory_enter_mode ? "true" : "false");

	return 0;
}

int sm5714_usbpd_exit_mode(struct sm5714_usbpd_data *pd_data, unsigned int mode)
{
	return 0;
}

data_obj_type sm5714_usbpd_select_capability(struct sm5714_usbpd_data *pd_data)
{
	data_obj_type obj;

	int pdo_num = pd_data->pd_noti.sink_status.selected_pdo_num;
	int output_vol = 0;
	int op_curr = 0;

	if (pd_data->pd_noti.sink_status.power_list[pdo_num].apdo) {
		output_vol = (pd_data->pd_noti.sink_status.pps_voltage / USBPD_OUT_VOLT_UNIT);
		op_curr = (pd_data->pd_noti.sink_status.pps_current / USBPD_PPS_CURRENT_UNIT);
		obj.request_data_object_programmable.op_current = op_curr;
		obj.request_data_object_programmable.reserved1 = 0;
		obj.request_data_object_programmable.output_voltage = output_vol;
		obj.request_data_object_programmable.reserved2 = 0;
		obj.request_data_object_programmable.unchunked_ext_msg_support = 0;
		obj.request_data_object_programmable.no_usb_suspend = 1;
		obj.request_data_object_programmable.usb_comm_capable = 1;
		obj.request_data_object_programmable.capability_mismatch = 0;
		obj.request_data_object_programmable.reserved3 = 0;
		obj.request_data_object_programmable.object_position = pd_data->pd_noti.sink_status.selected_pdo_num;
		obj.request_data_object_programmable.reserved4 = 0;
	} else {
		obj.request_data_object.no_usb_suspend = 1;
		obj.request_data_object.usb_comm_capable = 1;
		obj.request_data_object.capability_mismatch = 0;
		obj.request_data_object.give_back = 0;
		obj.request_data_object.min_current =
			pd_data->pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
		obj.request_data_object.op_current =
			pd_data->pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
		obj.request_data_object.object_position =
			pd_data->pd_noti.sink_status.selected_pdo_num;
	}

	return obj;
}

int sm5714_usbpd_evaluate_capability(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	int i = 0;
	int power_type = 0;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	int available_pdo_num = 0;
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
	PDIC_SINK_STATUS * pdic_sink_status = &pd_data->pd_noti.sink_status;
#else
	SEC_PD_SINK_STATUS * pdic_sink_status = &pd_data->pd_noti.sink_status;
#endif
	data_obj_type *pd_obj;
	int min_volt = 0, max_volt = 0, max_current = 0, max_power = 0;
	int usb_comm_capable = 0;

#if IS_ENABLED(CONFIG_PDIC_PD30)
	pd_data->specification_revision =
			pd_data->policy.rx_msg_header.spec_revision >= USBPD_REV_30 ?
			USBPD_REV_30 : pd_data->policy.rx_msg_header.spec_revision;
#else
	pd_data->specification_revision = USBPD_REV_20;
#endif
	pdic_sink_status->has_apdo = false;

	for (i = 0; i < policy->rx_msg_header.num_data_objs; i++) {
		pd_obj = &policy->rx_data_obj[i];
		power_type = pd_obj->power_data_obj_supply_type.supply_type;
		switch (power_type) {
		case POWER_TYPE_FIXED:
			max_volt = pd_obj->power_data_obj.voltage * USBPD_VOLT_UNIT;
			max_current = pd_obj->power_data_obj.max_current * USBPD_CURRENT_UNIT;
			dev_info(pd_data->dev, "[%d] FIXED volt(%d)mV, max_current(%d)\n",
				i+1, max_volt, max_current);
			if (pdic_data->pd_support) {
				if ((pdic_sink_status->power_list[i + 1].max_voltage != max_volt) ||
					(max_current != pdic_sink_status->power_list[i + 1].max_current))
					policy->send_sink_cap = 1;
			}

			available_pdo_num = i + 1;
			if (max_volt > AVAILABLE_VOLTAGE)
				pdic_sink_status->power_list[i + 1].accept = false;
			else
				pdic_sink_status->power_list[i + 1].accept = true;
			pdic_sink_status->power_list[i + 1].max_voltage = max_volt;
			pdic_sink_status->power_list[i + 1].max_current = max_current;
			pdic_sink_status->power_list[i + 1].min_voltage = 0;
			pdic_sink_status->power_list[i + 1].apdo = false;
			pdic_sink_status->power_list[i + 1].pdo_type = FPDO_TYPE;
			pdic_sink_status->power_list[i + 1].comm_capable =
						pd_obj->power_data_obj.usb_comm_capable;
			pdic_sink_status->power_list[i + 1].suspend =
						pd_obj->power_data_obj.usb_suspend_support;
			if (!usb_comm_capable)
				usb_comm_capable = !!pd_obj->power_data_obj.usb_comm_capable;
			break;
		case POWER_TYPE_BATTERY:
			min_volt = pd_obj->power_data_obj_battery.min_voltage * USBPD_VOLT_UNIT;
			max_volt = pd_obj->power_data_obj_battery.max_voltage * USBPD_VOLT_UNIT;
			max_power = pd_obj->power_data_obj_battery.max_power * USBPD_POWER_UNIT;
			dev_info(pd_data->dev, "[%d] BATTERY min_volt(%d)mV, max_volt(%d)mV, max_power(%d)mW\n",
						i+1, min_volt, max_volt, max_power);
			pdic_sink_status->power_list[i + 1].accept = false;
			pdic_sink_status->power_list[i + 1].apdo = false;
			break;
		case POWER_TYPE_VARIABLE:
			min_volt = pd_obj->power_data_obj_variable.min_voltage * USBPD_VOLT_UNIT;
			max_volt = pd_obj->power_data_obj_variable.max_voltage * USBPD_VOLT_UNIT;
			max_current = pd_obj->power_data_obj_variable.max_current * USBPD_CURRENT_UNIT;
			dev_info(pd_data->dev, "[%d] VARIABLE min_volt(%d)mV, max_volt(%d)mV, max_current(%d)mA\n",
						i+1, min_volt, max_volt, max_current);

			if (!manager->support_vpdo) {
				pdic_sink_status->power_list[i + 1].accept = false;
				pdic_sink_status->power_list[i + 1].apdo = false;
				continue;
			}

			available_pdo_num = i + 1;
			if (max_volt > AVAILABLE_VOLTAGE)
				pdic_sink_status->power_list[i + 1].accept = false;
			else
				pdic_sink_status->power_list[i + 1].accept = true;
			pdic_sink_status->power_list[i + 1].min_voltage = min_volt;
			pdic_sink_status->power_list[i + 1].max_voltage = max_volt;
			pdic_sink_status->power_list[i + 1].max_current = max_current;
			pdic_sink_status->power_list[i + 1].apdo = false;
			pdic_sink_status->power_list[i + 1].pdo_type = VPDO_TYPE;
			break;
		case POWER_TYPE_APDO:
			min_volt = pd_obj->power_data_obj_programmable.min_voltage * USBPD_PPS_VOLT_UNIT;
			max_volt = pd_obj->power_data_obj_programmable.max_voltage * USBPD_PPS_VOLT_UNIT;
			max_current = pd_obj->power_data_obj_programmable.max_current * USBPD_PPS_CURRENT_UNIT;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
			pd_data->specification_revision = USBPD_REV_30;
#endif
			dev_info(pd_data->dev, "[%d] Augmented min_volt(%d)mV, max_volt(%d)mV, max_current(%d)mA\n",
					i+1, min_volt, max_volt, max_current);

			available_pdo_num = i + 1;
			pdic_sink_status->has_apdo = true;
			pdic_sink_status->power_list[i + 1].accept = true;
			pdic_sink_status->power_list[i + 1].max_voltage = max_volt;
			pdic_sink_status->power_list[i + 1].min_voltage = min_volt;
			pdic_sink_status->power_list[i + 1].max_current = max_current;
			pdic_sink_status->power_list[i + 1].apdo = true;
			pdic_sink_status->power_list[i + 1].pdo_type = APDO_TYPE;
			break;
		default:
			dev_err(pd_data->dev, "[%d] Power Type Error\n", i+1);
			break;
		}
	}

#if IS_ENABLED(CONFIG_USE_USB_COMMUNICATIONS_CAPABLE)
	if (usb_comm_capable)
		send_otg_notify(get_otg_notify(), NOTIFY_EVENT_PD_USB_COMM_CAPABLE, USB_NOTIFY_COMM_CAPABLE);
	else
		send_otg_notify(get_otg_notify(), NOTIFY_EVENT_PD_USB_COMM_CAPABLE, USB_NOTIFY_NO_COMM_CAPABLE);
#endif

	if (pdic_sink_status->rp_currentlvl == RP_CURRENT_ABNORMAL) {
		available_pdo_num = 1;
		pdic_sink_status->power_list[1].max_current =
				pdic_sink_status->power_list[1].max_current > 1800 ?
				1800 : pdic_sink_status->power_list[1].max_current;
		dev_info(pd_data->dev, "Fixed max_current to 1.8A because of vbus short\n");
	}

	if ((pdic_sink_status->available_pdo_num > 0) &&
			(pdic_sink_status->available_pdo_num != available_pdo_num)) {
		policy->send_sink_cap = 1;
		if ((available_pdo_num == 3) && (pdic_sink_status->power_list[3].pdo_type == VPDO_TYPE))
			pdic_sink_status->selected_pdo_num = 3;
		else if ((available_pdo_num == 2) && (pdic_sink_status->power_list[2].pdo_type == VPDO_TYPE))
			pdic_sink_status->selected_pdo_num = 2;
		else
			pdic_sink_status->selected_pdo_num = 1;
	}

	if ((available_pdo_num == 1) &&
			((pdic_sink_status->power_list[1].max_current == 3000) ||
			(pdic_sink_status->power_list[1].max_current == 100)))
		policy->skip_ufp_svid_ack = 1;

	pdic_sink_status->available_pdo_num = available_pdo_num;
	if (manager->fled_torch_enable || manager->fled_flash_enable) {
		sm5714_info(" %s : PDO(%d) is ignored because of [torch(%d) or flash(%d)]\n",
				__func__, available_pdo_num, manager->fled_torch_enable, manager->fled_flash_enable);
		pdic_sink_status->available_pdo_num = 1;
		pdic_sink_status->has_apdo = false;
		policy->send_sink_cap = 1;
	}
	manager->origin_available_pdo_num = available_pdo_num;
	return available_pdo_num;
}

/* return: 0: cab be met, -1: cannot be met, -2: could be met later */
int sm5714_usbpd_match_request(struct sm5714_usbpd_data *pd_data)
{
	unsigned int mismatch, max_min, op, src_max_cur;
	unsigned int pos =
		pd_data->source_request_obj.request_data_object.object_position;
	unsigned int supply_type =
		pd_data->source_request_obj.power_data_obj_supply_type.supply_type;

#if IS_ENABLED(CONFIG_PDIC_PD30)
	if (pd_data->policy.rx_msg_header.spec_revision == USBPD_REV_30)
		pd_data->specification_revision = USBPD_REV_30;
	else
		pd_data->specification_revision = USBPD_REV_20;
#endif

	switch (pos) {
	case 1:
		supply_type = POWER_TYPE_FIXED;
		break;
	case 2:
		supply_type = POWER_TYPE_VARIABLE;
		break;
	default:
		break;
	}

	if (supply_type == POWER_TYPE_FIXED) {
		src_max_cur = pd_data->source_data_obj[0].power_data_obj.max_current;
		sm5714_info("REQUEST: FIXED\n");
		goto log_fixed_variable;
	} else if (supply_type == POWER_TYPE_VARIABLE) {
		src_max_cur = pd_data->source_data_obj[1].power_data_obj_variable.max_current;
		sm5714_info("REQUEST: VARIABLE\n");
		goto log_fixed_variable;
	} else if (supply_type == POWER_TYPE_BATTERY) {
		sm5714_info("REQUEST: BATTERY\n");
		goto log_battery;
	} else {
		sm5714_info("REQUEST: UNKNOWN Supply type.\n");
		return -1;
	}

log_fixed_variable:
	mismatch =
		pd_data->source_request_obj.request_data_object.capability_mismatch;
	max_min = pd_data->source_request_obj.request_data_object.min_current;
	op = pd_data->source_request_obj.request_data_object.op_current;
	pos = pd_data->source_request_obj.request_data_object.object_position;
	sm5714_info("Obj position: %d\n", pos);
	sm5714_info("Mismatch: %d\n", mismatch);
	sm5714_info("Operating Current: %d mA\n", op*10);
	if (pd_data->source_request_obj.request_data_object.give_back)
		sm5714_info("Min current: %d mA\n", max_min*10);
	else
		sm5714_info("Max current: %d mA\n", max_min*10);

	if ((pos > pd_data->source_msg_header.num_data_objs) ||
			(op > src_max_cur)) {
		sm5714_info("Invalid Request Message.\n");
		return -1;
	}
	return 0;

log_battery:
	mismatch =
		pd_data->source_request_obj.request_data_object_battery.capability_mismatch;
	return 0;
}

static void sm5714_usbpd_read_ext_msg(struct sm5714_usbpd_data *pd_data)
{
	int i = 0, j = 0, k = 0, l = 1, obj_num = 0;
	unsigned short vid = 0, pid = 0, xid = 0;

	pd_data->policy.rx_msg_header.word
		= pd_data->protocol_rx.msg_header.word;
	pd_data->policy.rx_msg_ext_header.word
		= pd_data->protocol_rx.data_obj[0].word[0];
	obj_num = pd_data->policy.rx_msg_header.num_data_objs;

	for (i = 0; i < (obj_num * 4); i++) {
		if ((i != 0) && (i % 4 == 0))
			j++;
		if (i == 0)
			pd_data->policy.rx_data_obj[0].byte[0]
				= pd_data->protocol_rx.data_obj[j].byte[2];
		else if (i == 1)
			pd_data->policy.rx_data_obj[0].byte[1]
				= pd_data->protocol_rx.data_obj[j].byte[3];
		else {
			if ((k != 0) && (k % 4 == 0))
				l++;
			pd_data->policy.rx_data_obj[j].byte[i % 4]
				= pd_data->protocol_rx.data_obj[l].byte[k % 4];
			k++;
		}
	}

	vid = pd_data->policy.rx_data_obj[0].source_capabilities_extended_data1.VID;
	pid = pd_data->policy.rx_data_obj[0].source_capabilities_extended_data1.PID;
	xid = pd_data->policy.rx_data_obj[1].source_capabilities_extended_data2.XID;
	sm5714_info("%s : VID = 0x%x   PID = 0x%x  XID = 0x%x\n", __func__, vid, pid, xid);
#ifdef CONFIG_BATTERY_NOTIFIER
	if (fp_count_cisd_pd_data)
		fp_count_cisd_pd_data(vid, pid);
#endif
}

static void sm5714_usbpd_read_msg(struct sm5714_usbpd_data *pd_data)
{
	int i;

	pd_data->policy.rx_msg_header.word
		= pd_data->protocol_rx.msg_header.word;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++) {
		pd_data->policy.rx_data_obj[i].object
			= pd_data->protocol_rx.data_obj[i].object;
	}
}

static void sm5714_usbpd_protocol_tx(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_protocol_data *tx = &pd_data->protocol_tx;

	tx->status = DEFAULT_PROTOCOL_NONE;

	if (pd_data->phy_ops.tx_msg(pd_data, &tx->msg_header, tx->data_obj))
		dev_err(pd_data->dev, "%s error\n", __func__);
	dev_info(pd_data->dev, "%s: [Tx] [0x%x] [0x%x]\n", __func__,
			tx->msg_header.word, tx->data_obj[0].object);
	tx->msg_header.word = 0;
}

/* return 1: sent with goodcrc, 0: fail */
bool sm5714_usbpd_send_msg(struct sm5714_usbpd_data *pd_data, msg_header_type *header,
		data_obj_type *obj)
{
	int i;

	if (obj)
		for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
			pd_data->protocol_tx.data_obj[i].object = obj[i].object;
	else
		header->num_data_objs = 0;

	header->spec_revision = pd_data->specification_revision;
	pd_data->protocol_tx.msg_header.word = header->word;
	sm5714_usbpd_protocol_tx(pd_data);

	return true;
}

inline bool sm5714_usbpd_send_ctrl_msg(struct sm5714_usbpd_data *d, msg_header_type *h,
		unsigned int msg, unsigned int dr, unsigned int pr)
{
	h->msg_type = msg;
	h->port_data_role = dr;
	h->port_power_role = pr;
	h->num_data_objs = 0;
	return sm5714_usbpd_send_msg(d, h, 0);
}

/* return: 0 if timed out, positive is status */
inline u64 sm5714_usbpd_wait_msg(struct sm5714_usbpd_data *pd_data,
				u64 msg_status, unsigned int ms)
{
	u64 ret = 0;

	ret = pd_data->phy_ops.get_pdmsg_status(pd_data, msg_status);
	if (ret) {
		pd_data->policy.abnormal_state = false;
		return ret;
	}
	dev_info(pd_data->dev,
		"%s: msg_status = %llx, time = %d\n", __func__, msg_status, ms);
	/* wait */
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = msg_status;
	ret = wait_for_completion_timeout(&pd_data->msg_arrived,
						msecs_to_jiffies(ms));

	if (!pd_data->policy.state) {
		dev_err(pd_data->dev,
			"%s : return for policy state error\n", __func__);
		pd_data->policy.abnormal_state = true;
		return 0;
	}

	pd_data->policy.abnormal_state = false;

	return pd_data->phy_ops.get_pdmsg_status(pd_data, msg_status);
}

static void sm5714_usbpd_check_vdm(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
#if IS_ENABLED(CONFIG_PDIC_PD30)
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
#endif
	unsigned int cmd, cmd_type, vdm_type;
	unsigned int obj_pos, version, svid, product_type;

	cmd = pd_data->policy.rx_data_obj[0].structured_vdm.command;
	cmd_type = pd_data->policy.rx_data_obj[0].structured_vdm.command_type;
	vdm_type = pd_data->policy.rx_data_obj[0].structured_vdm.vdm_type;
	obj_pos = pd_data->policy.rx_data_obj[0].structured_vdm.obj_pos;
	version = pd_data->policy.rx_data_obj[0].structured_vdm.version;
	svid = pd_data->policy.rx_data_obj[0].structured_vdm.svid;
	product_type = pd_data->policy.rx_data_obj[1].id_header.product_type;

	dev_info(pd_data->dev, "%s: cmd = %x, cmd_type = %x, vdm_type = %x\n",
			__func__, cmd, cmd_type, vdm_type);

	/* TD.PD.VDMU.E17 Interruption by VDM Command */
	if (sm5714_get_rx_buf_st(pd_data) && ((cmd == Discover_Identity) ||
			(cmd == Discover_SVIDs) || (cmd == Discover_Modes) ||
			(cmd == Enter_Mode) || (cmd == Exit_Mode)))
		return;

	if (vdm_type == Unstructured_VDM) {
		/* TEST.PD.VDM.SNK.7 Unrecognized VID in Unstructured VDM */
#if IS_ENABLED(CONFIG_PDIC_PD30)
		if ((pd_data->policy.rx_msg_header.spec_revision == USBPD_REV_30) &&
				!manager->is_samsung_accessory_enter_mode)
			pdic_data->status_reg |= BITMSG(MSG_NOT_SUPPORTED);
		else
#endif
			pdic_data->status_reg |= BITMSG(UVDM_MSG);
		return;
	}

	/* TD.PD.VDMD.E3 Incorrect Identity Fields */
	if ((cmd == Discover_Identity) && (cmd_type == Responder_ACK)) {
		if ((version > USBPD_REV_20) || (obj_pos == 7) || (svid != PD_SID) ||
			(product_type == 6) || (product_type == 7)) {
			dev_err(pd_data->dev,
				"%s : version=%d, obj_pos=%d, svid=0x%x, product_type=%d\n",
				__func__, version, obj_pos, svid, product_type);
			return;
		}
	}

	if (cmd_type == Initiator) {
		switch (cmd) {
		case Discover_Identity:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_IDENTITY);
			break;
		case Discover_SVIDs:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_SVID);
			break;
		case Discover_Modes:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_MODE);
			break;
		case Enter_Mode:
			pdic_data->status_reg |= BITMSG(VDM_ENTER_MODE);
			break;
		case Exit_Mode:
			pdic_data->status_reg |= BITMSG(VDM_EXIT_MODE);
			break;
		case Attention:
			pdic_data->status_reg |= BITMSG(VDM_ATTENTION);
			break;
		case DisplayPort_Status_Update:
			pdic_data->status_reg |= BITMSG(MSG_PASS);
			break;
		case DisplayPort_Configure:
			pdic_data->status_reg |= BITMSG(MSG_PASS);
			break;
		}
	} else {
		switch (cmd) {
		case Discover_Identity:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_IDENTITY);
			break;
		case Discover_SVIDs:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_SVID);
			break;
		case Discover_Modes:
			pdic_data->status_reg |= BITMSG(VDM_DISCOVER_MODE);
			break;
		case Enter_Mode:
			pdic_data->status_reg |= BITMSG(VDM_ENTER_MODE);
			break;
		case Exit_Mode:
			pdic_data->status_reg |= BITMSG(VDM_EXIT_MODE);
			break;
		case DisplayPort_Status_Update:
			pdic_data->status_reg |= BITMSG(VDM_DP_STATUS_UPDATE);
			break;
		case DisplayPort_Configure:
			pdic_data->status_reg |= BITMSG(VDM_DP_CONFIGURE);
			break;
		}
	}
}

void sm5714_usbpd_protocol_rx(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_protocol_data *rx = &pd_data->protocol_rx;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	u8 ext_msg;

	if (pd_data->phy_ops.rx_msg(pd_data, &rx->msg_header, rx->data_obj)) {
		dev_err(pd_data->dev, "%s IO Error\n", __func__);
		return;
	}
	dev_info(pd_data->dev, "%s: stored_message_id = %x, msg_id = %d\n",
			__func__, rx->stored_message_id, rx->msg_header.msg_id);
	if (rx->msg_header.word == 0) {
		dev_err(pd_data->dev, "[Rx] No Message.\n");
		return; /* no message */
	} else if (rx->msg_header.msg_type == USBPD_Soft_Reset) {
		sm5714_info("%s : Got SOFT_RESET.\n", __func__);
		pdic_data->status_reg |= BITMSG(MSG_SOFTRESET);
		return;
	}
	dev_err(pd_data->dev, "[Rx] [0x%x] [0x%x]\n",
		rx->msg_header.word, rx->data_obj[0].object);

	if (rx->stored_message_id != rx->msg_header.msg_id) {
		if (pd_data->policy.origin_message == 0x0)
			rx->stored_message_id = rx->msg_header.msg_id;

		ext_msg = pd_data->protocol_rx.msg_header.byte[1] & 0x80;

		if (ext_msg)
			sm5714_usbpd_read_ext_msg(pd_data);
		else
			sm5714_usbpd_read_msg(pd_data);

		dev_info(pd_data->dev, "%s: ext_msg = %x, obj_num = %d, msg_type = %d\n",
			__func__, ext_msg, pd_data->policy.rx_msg_header.num_data_objs,
					pd_data->policy.rx_msg_header.msg_type);

		if (ext_msg && pd_data->policy.rx_msg_header.spec_revision == 0) {
			return;
		} else if (ext_msg && pd_data->policy.rx_msg_header.spec_revision == USBPD_REV_30) {
			switch (pd_data->policy.rx_msg_header.msg_type) {
			case USBPD_Source_Cap_Ext:
				sm5714_info("%s : Chunked = %d, Chunk Number = %d, Request Chunk = %d, Data Size = %d\n",
						__func__, pd_data->policy.rx_msg_ext_header.chunked, pd_data->policy.rx_msg_ext_header.chunk_number,
						pd_data->policy.rx_msg_ext_header.request_chunk, pd_data->policy.rx_msg_ext_header.data_size);
				sm5714_info("%s : VID = 0x%x\n", __func__, pd_data->policy.rx_data_obj[0].source_capabilities_extended_data1.VID);
				sm5714_info("%s : PID = 0x%x\n", __func__, pd_data->policy.rx_data_obj[0].source_capabilities_extended_data1.PID);
				sm5714_info("%s : XID = 0x%x\n", __func__, pd_data->policy.rx_data_obj[1].source_capabilities_extended_data2.XID);
				break;
			case USBPD_Status:
				pdic_data->status_reg |= BITMSG(MSG_NONE);
				break;
			case USBPD_Get_Battery_Cap:
				pdic_data->status_reg |= BITMSG(MSG_GET_BAT_CAP);
				break;
			case USBPD_Get_Batt_Status:
				pdic_data->status_reg |= BITMSG(MSG_GET_BAT_STATUS);
				break;
			case USBPD_Battery_Cap:
				pdic_data->status_reg |= BITMSG(MSG_GET_BAT_CAP);
				break;
			case USBPD_Get_Manuf_Info:
				pdic_data->status_reg |= BITMSG(MSG_GET_MANUF_INFO);
				break;
			case USBPD_Manuf_Info:
				pdic_data->status_reg |= BITMSG(MSG_MANUF_INFO);
				break;
			case USBPD_Security_Request:
				pdic_data->status_reg |= BITMSG(MSG_SECURITY_REQ);
				break;
			case USBPD_Security_Response:
				pdic_data->status_reg |= BITMSG(MSG_SECURITY_RES);
				break;
			case USBPD_FW_Update_Req:
				pdic_data->status_reg |= BITMSG(MSG_FW_UPDATE_REQ);
				break;
			case USBPD_FW_Update_Res:
				pdic_data->status_reg |= BITMSG(MSG_FW_UPDATE_RES);
				break;
			case USBPD_PPS_Status:
				pdic_data->status_reg |= BITMSG(MSG_PPS_STATUS);
				break;
			case USBPD_Country_Info:
				pdic_data->status_reg |= BITMSG(MSG_COUNTRY_INFO);
				break;
			case USBPD_Country_Codes:
				pdic_data->status_reg |= BITMSG(MSG_COUNTRY_CODES);
				break;
			case USBPD_Sink_Cap_Ext:
				pdic_data->status_reg |= BITMSG(MSG_SNK_CAP_EXT);
				break;
			default:
				pdic_data->status_reg |= BITMSG(MSG_RESERVED);
				break;
			}
		} else if (pd_data->policy.rx_msg_header.num_data_objs > 0) {
			switch (pd_data->policy.rx_msg_header.msg_type) {
			case USBPD_Source_Capabilities:
				pdic_data->status_reg |= BITMSG(MSG_SRC_CAP);
				break;
			case USBPD_Request:
				pdic_data->status_reg |= BITMSG(MSG_REQUEST);
				break;
			case USBPD_BIST:
				if (pd_data->policy.state == PE_SNK_Ready ||
						pd_data->policy.state == PE_SRC_Ready) {
					if (pd_data->policy.rx_data_obj[0].bist_data_object.bist_mode ==
							BIST_Carrier_Mode2) {
						pdic_data->status_reg |= BITMSG(MSG_BIST_M2);
					} else if (pd_data->policy.rx_data_obj[0].bist_data_object.bist_mode ==
							BIST_Test_Mode) {
						pdic_data->status_reg |= BITMSG(MSG_NONE);
						pd_data->policy.is_bist_test_mode = 1;
					} else {
						/* Not Support */
					}
				}
				break;
			case USBPD_Sink_Capabilities:
				pdic_data->status_reg |= BITMSG(MSG_SNK_CAP);
				break;
			case USBPD_Battery_Status:
				pdic_data->status_reg |= BITMSG(MSG_BAT_STATUS);
				break;
			case USBPD_Alert:
				if (pd_data->policy.rx_data_obj->alert_data_obejct.type_of_alert == Battery_Status_Change)
					pdic_data->status_reg |= BITMSG(MSG_GET_BAT_STATUS);
				else
					pdic_data->status_reg |= BITMSG(MSG_GET_STATUS);
				break;
			case USBPD_Get_Country_Info:
				pdic_data->status_reg |= BITMSG(MSG_GET_COUNTRY_INFO);
				break;
			case USBPD_Vendor_Defined:
				sm5714_usbpd_check_vdm(pd_data);
				break;
			case 8 ... 14:
				pdic_data->status_reg |= BITMSG(MSG_RESERVED);
				break;
			default:
				break;
			}
		} else {
			switch (pd_data->policy.rx_msg_header.msg_type) {
			case USBPD_GoodCRC:
				/* Do nothing */
				break;
			case USBPD_Ping:
				/* Do nothing */
				break;
			case USBPD_GotoMin:
				if (pd_data->policy.state == PE_SNK_Ready)
					pd_data->policy.state =
						PE_SNK_Transition_Sink;
				break;
			case USBPD_Accept:
				pdic_data->status_reg |= BITMSG(MSG_ACCEPT);
				break;
			case USBPD_Reject:
				pdic_data->status_reg |= BITMSG(MSG_REJECT);
				break;
			case USBPD_PS_RDY:
				pdic_data->status_reg |= BITMSG(MSG_PSRDY);
				break;
			case USBPD_Get_Source_Cap:
				pdic_data->status_reg |= BITMSG(MSG_GET_SRC_CAP);
				break;
			case USBPD_Get_Sink_Cap:
				pdic_data->status_reg |= BITMSG(MSG_GET_SNK_CAP);
				break;
			case USBPD_DR_Swap:
				pdic_data->status_reg |= BITMSG(MSG_DR_SWAP);
				break;
			case USBPD_PR_Swap:
				pdic_data->status_reg |= BITMSG(MSG_PR_SWAP);
				break;
			case USBPD_VCONN_Swap:
				pdic_data->status_reg |= BITMSG(MSG_VCONN_SWAP);
				break;
			case USBPD_Wait:
				pdic_data->status_reg |= BITMSG(MSG_WAIT);
				break;
			case USBPD_Soft_Reset:
				pdic_data->status_reg |= BITMSG(MSG_SOFTRESET);
				break;
			case USBPD_Data_Reset: /* (Reserved, in PD2 mode) */
				if (pd_data->policy.rx_msg_header.spec_revision == USBPD_REV_20)
					pdic_data->status_reg |= BITMSG(MSG_REJECT);
				break;
			case USBPD_Not_Supported:
				pdic_data->status_reg |= BITMSG(MSG_NOT_SUPPORTED);
				break;
			case USBPD_Get_Src_Cap_Ext:
				pdic_data->status_reg |= BITMSG(MSG_GET_SRC_CAP_EXT);
				break;
			case USBPD_Get_Status:
				pdic_data->status_reg |= BITMSG(MSG_NOT_SUPPORTED);
				break;
			case USBPD_FR_Swap:
				pdic_data->status_reg |= BITMSG(MSG_FR_SWAP);
				break;
			case USBPD_Get_PPS_Status:
				pdic_data->status_reg |= BITMSG(MSG_GET_PPS_STATUS);
				break;
			case USBPD_Get_Country_Codes:
				pdic_data->status_reg |= BITMSG(MSG_GET_COUNTRY_CODES);
				break;
			case USBPD_Get_Sink_Cap_Ext:
				pdic_data->status_reg |= BITMSG(MSG_GET_SNK_CAP_EXT);
				break;
			case USBPD_Get_Source_Info:
				pdic_data->status_reg |= BITMSG(MSG_GET_SRC_INFO);
				break;
			case USBPD_Get_Revision:
				pdic_data->status_reg |= BITMSG(MSG_NOT_SUPPORTED);
				break;
			case 25 ... 31:
				pdic_data->status_reg |= BITMSG(MSG_RESERVED);
				break;
			default:
				break;
			}
		}
	}
}

void sm5714_usbpd_rx_hard_reset(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);

	sm5714_usbpd_reinit(dev);
#if IS_ENABLED(CONFIG_PDIC_PD30)
	pd_data->specification_revision = USBPD_REV_30;
#else
	pd_data->specification_revision = USBPD_REV_20;
#endif
	sm5714_usbpd_policy_reset(pd_data, HARDRESET_RECEIVED);
}

void sm5714_usbpd_rx_soft_reset(struct sm5714_usbpd_data *pd_data)
{
	sm5714_usbpd_reinit(pd_data->dev);
	sm5714_usbpd_policy_reset(pd_data, SOFTRESET_RECEIVED);
}

void sm5714_usbpd_set_ops(struct device *dev, usbpd_phy_ops_type *ops)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) dev_get_drvdata(dev);

	pd_data->phy_ops.tx_msg = ops->tx_msg;
	pd_data->phy_ops.rx_msg = ops->rx_msg;
	pd_data->phy_ops.hard_reset = ops->hard_reset;
	pd_data->phy_ops.set_power_role = ops->set_power_role;
	pd_data->phy_ops.get_power_role = ops->get_power_role;
	pd_data->phy_ops.set_data_role = ops->set_data_role;
	pd_data->phy_ops.get_data_role = ops->get_data_role;
	pd_data->phy_ops.get_vconn_source = ops->get_vconn_source;
	pd_data->phy_ops.set_vconn_source = ops->set_vconn_source;
	pd_data->phy_ops.set_check_msg_pass = ops->set_check_msg_pass;
	pd_data->phy_ops.get_status = ops->get_status;
	pd_data->phy_ops.get_pdmsg_status = ops->get_pdmsg_status;
	pd_data->phy_ops.poll_status = ops->poll_status;
	pd_data->phy_ops.driver_reset = ops->driver_reset;
	pd_data->phy_ops.get_short_state = ops->get_short_state;
}

#ifdef CONFIG_OF
static int of_sm5714_usbpd_dt(struct sm5714_usbpd_manager_data *_data)
{
	int ret = 0;
	struct device_node *np =
		of_find_node_by_name(NULL, "pdic-manager");

	if (np == NULL) {
		sm5714_err("%s np NULL\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "pdic,max_power",
			&_data->max_power);
	if (ret < 0)
		sm5714_err("%s error reading max_power %d\n",
				__func__, _data->max_power);

	ret = of_property_read_u32(np, "pdic,op_power",
			&_data->op_power);
	if (ret < 0)
		sm5714_err("%s error reading op_power %d\n",
				__func__, _data->max_power);

	ret = of_property_read_u32(np, "pdic,max_current",
			&_data->max_current);
	if (ret < 0)
		sm5714_err("%s error reading max_current %d\n",
				__func__, _data->max_current);

	ret = of_property_read_u32(np, "pdic,min_current",
			&_data->min_current);
	if (ret < 0)
		sm5714_err("%s error reading min_current %d\n",
				__func__, _data->min_current);

	_data->giveback = of_property_read_bool(np,
						"pdic,giveback");
	_data->usb_com_capable = of_property_read_bool(np,
						"pdic,usb_com_capable");
	_data->no_usb_suspend = of_property_read_bool(np,
						"pdic,no_usb_suspend");

	/* source capability */
	ret = of_property_read_u32(np, "source,max_voltage",
			&_data->source_max_volt);
	ret = of_property_read_u32(np, "source,min_voltage",
			&_data->source_min_volt);
	ret = of_property_read_u32(np, "source,max_power",
			&_data->source_max_power);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		sm5714_err("%s: np(battery) NULL\n", __func__);
	} else {
		_data->support_vpdo = of_property_read_bool(np,
						"battery,support_vpdo");
		_data->support_15w_vpdo = of_property_read_bool(np,
						"battery,support_15w_vpdo");
		ret = of_property_read_u32(np, "battery,short_cable_current", &_data->short_cable_current);
		if (ret) {
			pr_info("%s : short_cable_current is Empty, set as 1800 mA\n", __func__);
			_data->short_cable_current  = 1800;
		}
	}
#else
	_data->support_vpdo = false;
	_data->support_15w_vpdo = false;
#endif

	return ret;
}
#endif

static void sm5714_usbpd_init_source_cap_data(struct sm5714_usbpd_manager_data *_data)
{
	msg_header_type *msg_header = &_data->pd_data->source_msg_header;
	data_obj_type *data_obj = _data->pd_data->source_data_obj;

	msg_header->msg_type = USBPD_Source_Capabilities;
	msg_header->port_data_role = USBPD_DFP;
	msg_header->spec_revision = _data->pd_data->specification_revision;
	msg_header->port_power_role = USBPD_SOURCE;
	msg_header->num_data_objs = 2;

	data_obj->power_data_obj.max_current = 500 / 10;
	data_obj->power_data_obj.voltage = 5000 / 50;
	data_obj->power_data_obj.supply = POWER_TYPE_FIXED;
	data_obj->power_data_obj.data_role_swap = 1;
	data_obj->power_data_obj.dual_role_power = 1;
	data_obj->power_data_obj.usb_suspend_support = 1;
	data_obj->power_data_obj.usb_comm_capable = 1;
	data_obj->power_data_obj.reserved = 0;

	if (_data->support_15w_vpdo) {
		(data_obj + 1)->power_data_obj_variable.supply_type = POWER_TYPE_VARIABLE;
		(data_obj + 1)->power_data_obj_variable.max_voltage = 9000/50;
		(data_obj + 1)->power_data_obj_variable.min_voltage = 7000/50;
		(data_obj + 1)->power_data_obj_variable.max_current = 1650/10;
	} else {
		(data_obj + 1)->power_data_obj_variable.supply_type = POWER_TYPE_VARIABLE;
		(data_obj + 1)->power_data_obj_variable.max_voltage = 9000/50;
		(data_obj + 1)->power_data_obj_variable.min_voltage = 7000/50;
		(data_obj + 1)->power_data_obj_variable.max_current = 1000/10;
	}
}

static int sm5714_usbpd_manager_init(struct sm5714_usbpd_data *pd_data)
{
	int ret = 0;
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	if (manager == NULL) {
		sm5714_err("%s, usbpd manager data is error!!\n", __func__);
		return -ENOMEM;
	}
	sm5714_info("%s\n", __func__);
	ret = of_sm5714_usbpd_dt(manager);
#ifdef CONFIG_BATTERY_NOTIFIER
	fp_select_pdo = sm5714_select_pdo;
	fp_sec_pd_select_pps = sm5714_select_pps;
	fp_sec_pd_get_apdo_max_power = sm5714_get_apdo_max_power;
#else
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pdo = sm5714_select_pdo;
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pps = sm5714_select_pps;
	pd_data->pd_noti.sink_status.fp_sec_pd_vpdo_auth = sm5714_vpdo_auth;
	pd_data->pd_noti.sink_status.fp_sec_pd_detach_with_cc = sm5714_detach_with_cc;
#if IS_ENABLED(CONFIG_HICCUP_CC_DISABLE)
	pd_data->pd_noti.sink_status.fp_sec_pd_manual_ccopen_req = sm5714_pd_manual_ccopen_req;
#endif
	pd_data->pd_noti.sink_status.fp_sec_pd_manual_jig_ctrl = sm5714_pd_manual_jig_ctrl;
	pd_data->pd_noti.sink_status.fp_sec_pd_change_src = sm5714_usbpd_forced_change_srccap;
#endif
	manager->pd_data = pd_data;
	manager->power_role_swap = true;
	manager->data_role_swap = true;
	manager->vconn_source_swap = pdic_data->vconn_en ? true : false;
	manager->acc_type = PDIC_DOCK_DETACHED;
	manager->Vendor_ID = 0;
	manager->Product_ID = 0;
	manager->Device_Version = 0;
	manager->alt_sended = 0;
	manager->vdm_en = 0;
	manager->ext_sended = 0;
	manager->SVID_0 = 0;
	manager->SVID_1 = 0;
	manager->SVID_DP = 0;
	manager->Standard_Vendor_ID = 0;
	manager->is_samsung_accessory_enter_mode = 0;
	manager->fled_flash_enable = 0;
	manager->fled_torch_enable = 0;
	manager->origin_available_pdo_num = 0;
	manager->uvdm_out_ok = 1;
	manager->uvdm_in_ok = 1;
	manager->dr_swap_cnt = 0;

	init_waitqueue_head(&manager->uvdm_out_wq);
	init_waitqueue_head(&manager->uvdm_in_wq);

	sm5714_usbpd_init_source_cap_data(manager);
	INIT_DELAYED_WORK(&manager->acc_detach_handler,
			sm5714_usbpd_acc_detach_handler);
	INIT_DELAYED_WORK(&manager->new_power_handler,
			sm5714_usbpd_manager_new_power_handler);
	INIT_DELAYED_WORK(&manager->start_discover_msg_handler,
			sm5714_usbpd_start_discover_msg_handler);
	INIT_DELAYED_WORK(&manager->start_dex_discover_msg_handler,
			sm5714_usbpd_start_dex_discover_msg_handler);
	return ret;
}

static void sm5714_usbpd_rx_layer_init(struct sm5714_protocol_data *rx)
{
	int i;

	rx->stored_message_id = USBPD_nMessageIDCount+1;
	rx->msg_header.word = 0;
	rx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		rx->data_obj[i].object = 0;
}

static void sm5714_usbpd_tx_layer_init(struct sm5714_protocol_data *tx)
{
	int i;

	tx->stored_message_id = USBPD_nMessageIDCount+1;
	tx->msg_header.word = 0;
	tx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;
}

void sm5714_usbpd_tx_request_discard(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_protocol_data *tx = &pd_data->protocol_tx;
	int i;

	tx->msg_header.word = 0;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;

	dev_err(pd_data->dev, "%s\n", __func__);
}

void sm5714_usbpd_init_protocol(struct sm5714_usbpd_data *pd_data)
{
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	if (pdic_data->is_jig_case_on) {
		sm5714_info("%s: Do not protocol reset.\n", __func__);
		return;
	}

	if (policy->state == PE_SRC_Startup ||
			policy->state == PE_SNK_Startup ||
			policy->state == PE_SRC_Send_Soft_Reset ||
			policy->state == PE_SNK_Send_Soft_Reset ||
			policy->state == PE_SRC_Soft_Reset ||
			policy->state == PE_SNK_Soft_Reset) {
		if ((pdic_data->reset_done == 0) && !pdic_data->is_mpsm_exit)
			sm5714_protocol_layer_reset(pd_data);
	}

	sm5714_usbpd_rx_layer_init(&pd_data->protocol_rx);
	sm5714_usbpd_tx_layer_init(&pd_data->protocol_tx);
}

static void sm5714_usbpd_init_counters(struct sm5714_usbpd_data *pd_data)
{
	sm5714_info("%s: init counter\n", __func__);
	pd_data->counter.retry_counter = 0;
	pd_data->counter.message_id_counter = 0;
	pd_data->counter.caps_counter = 0;
	pd_data->counter.hard_reset_counter = 0;
	pd_data->counter.swap_hard_reset_counter = 0;
	pd_data->counter.discover_identity_counter = 0;
}

void sm5714_usbpd_reinit(struct device *dev)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;

	sm5714_usbpd_init_counters(pd_data);
	sm5714_usbpd_init_protocol(pd_data);
	sm5714_usbpd_init_policy(pd_data);
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = 0;
	if (!pdic_data->is_attached) {
		pd_data->auth_type = AUTH_NONE;
		sm5714_usbpd_change_source_cap(1, 500, 1);
	}
	if ((pdic_data->power_role == PDIC_SOURCE) && !pdic_data->is_otg_vboost)
		sm5714_usbpd_turn_off_reverse_booster(pd_data);
	sm5714_usbpd_start_discover_msg_cancel(pd_data->dev);
	sm5714_usbpd_start_dex_discover_msg_cancel(pd_data->dev);
	complete(&pd_data->msg_arrived);
	reinit_completion(&pd_data->pd_completion);
	complete(&pd_data->pd_completion);
}

int sm5714_usbpd_init(struct device *dev, void *phy_driver_data)
{
	struct sm5714_usbpd_data *pd_data;

	if (!dev)
		return -EINVAL;

	pd_data = kzalloc(sizeof(struct sm5714_usbpd_data), GFP_KERNEL);

	if (!pd_data)
		return -ENOMEM;

	pd_data->dev = dev;
	pd_data->phy_driver_data = phy_driver_data;
	dev_set_drvdata(dev, pd_data);

	sm5714_g_pd_data = pd_data;
	pd_data->pd_noti.pusbpd = pd_data;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.sink_status.pps_voltage = 0;
	pd_data->pd_noti.sink_status.pps_current = 0;
	pd_data->pd_noti.sink_status.has_apdo = false;
	pd_data->thermal_state = 0;
	pd_data->auth_type = AUTH_NONE;
#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
	pd_data->altmode_enable = 0;
#endif

#if IS_ENABLED(CONFIG_PDIC_PD30)
	pd_data->specification_revision = USBPD_REV_30;
#else
	pd_data->specification_revision = USBPD_REV_20;
#endif
	sm5714_usbpd_init_counters(pd_data);
	sm5714_usbpd_init_protocol(pd_data);
	sm5714_usbpd_init_policy(pd_data);
	sm5714_usbpd_manager_init(pd_data);
	sm5714_usbpd_change_source_cap(1, 500, 1);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 188)
	wakeup_source_init(pd_data->policy_engine_wake, "policy_engine_wake");	 // 4.19 R
	if (!(pd_data->policy_engine_wake)) {
		pd_data->policy_engine_wake = wakeup_source_create("policy_engine_wake"); // 4.19 Q
		if (pd_data->policy_engine_wake)
			wakeup_source_add(pd_data->policy_engine_wake);
	}
#else
	pd_data->policy_engine_wake = wakeup_source_register(NULL, "policy_engine_wake"); // 5.4 R
#endif
	INIT_WORK(&pd_data->worker, sm5714_usbpd_policy_work);
	init_completion(&pd_data->msg_arrived);
	init_completion(&pd_data->pd_completion);

	return 0;
}
