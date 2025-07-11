/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include <drm/drm_panel.h>
#include "../../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"

#include <linux/power_supply.h>
#include <uapi/linux/sched/types.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "focaltech_ts"
#define FTS_DRIVER_PEN_NAME                 "fts_ts,pen"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2800000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif
#define DISP_ID_DET (262 + 123)
#define DISP_ID1_DET (262 + 129)

#define DUMPBUFF_HEAD (1+1+24)
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;
static unsigned int fts_dump_frame_count = 0;
/*struct device_node *gf_spi_dp;*/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
static void fts_power_supply_work(struct work_struct *work);

#ifdef FTS_XIAOMI_TOUCHFEATURE
static int fts_read_palm_data(void);
static int fts_read_and_report_foddata(struct fts_ts_data *data);
static int fts_palm_sensor_cmd(int value);
static void fts_palm_mode_recovery(struct fts_ts_data *ts_data);
static void fts_game_mode_recovery(struct fts_ts_data *ts_data);
static void fts_charger_status_recovery(struct fts_ts_data *ts_data);
static void fts_fod_status_recovery(struct fts_ts_data *ts_data);
static void fts_report_rate_recovery(struct fts_ts_data *ts_data);
static void fts_game_idle_high_refresh_recovery(struct fts_ts_data *ts_data);
static int fts_get_charging_status(void);
/*static int fts_change_fps(void *data);*/
/*functions for notifier chain*/
int fts_drm_state_change_callback(struct notifier_block *self,
		unsigned long event, void *data);
/* static void fts_register_panel_notifier_work(struct work_struct *work); */
static void fts_sleep2gesture_work(struct work_struct *work);
static void fts_recover_gesture_from_sleep(struct fts_ts_data *data);
extern void touch_irq_boost(void);
#ifdef CONFIG_TOUCH_BOOST
extern void lpm_disable_for_dev(bool on, char event_dev);
#define LPM_EVENT_INPUT 0x1
#endif
#define ORIENTATION_0_OR_180	0	/* anticlockwise 0 or 180 degrees */
#define NORMAL_ORIENTATION_90	1	/* anticlockwise 90 degrees in normal */
#define NORMAL_ORIENTATION_270	2	/* anticlockwise 270 degrees in normal */
#define GAME_ORIENTATION_90	3	/* anticlockwise 90 degrees in game */
#define GAME_ORIENTATION_270	4	/* anticlockwise 270 degrees in game */
#endif
static struct proc_dir_entry *touch_debug;

static bool touch_boost_en = false;
static bool touch_static = false;
static u64 coord_cnt = 0;

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
        int i = 0;
        struct ft_chip_id_t *cid = &ts_data->ic_info.cid;
        u8 cid_h = 0x0;

        if (cid->type == 0)
                return -ENODATA;

        for (i = 0; i < FTS_MAX_CHIP_IDS; i++) {
                cid_h = ((cid->chip_ids[i] >> 8) & 0x00FF);
                if (cid_h && (id_h == cid_h)) {
                        return 0;
        }
    }

        return -ENODATA;
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
    int ret = 0;
    int cnt = 0;
    u8 idh = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 chip_idh = ts_data->ic_info.ids.chip_idh;

    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
        if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
            FTS_INFO("TP Ready,Device ID:0x%02x", idh);
            return 0;
        } else
            FTS_DEBUG("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	fts_charger_status_recovery(ts_data);
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	/*fts_ex_mode_recovery(ts_data);*/
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	/* recover TP report_rate state 0x92 */
	fts_report_rate_recovery(ts_data);
#ifdef FTS_XIAOMI_TOUCHFEATURE
	/* recover TP game mode state */
	fts_game_mode_recovery(ts_data);
	/* recover TP idle refresh state 0x8E */
	fts_game_idle_high_refresh_recovery(ts_data);
	/* recover TP palm mode state */
	fts_palm_mode_recovery(ts_data);
	/* recover TP fod state 0xCF */
	fts_fod_status_recovery(ts_data);
#endif
        FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	fts_write_reg(0xB6, 0x01);
	msleep(20);
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(1);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	FTS_INFO("tp reset finished");
	if (hdelayms)
		msleep(hdelayms);
	FTS_INFO("tp reset out");
	return 0;
}

int fts_recover_after_reset(void)
{
	int i = 0;
	u8 id = 0;
	for (i = 0; i < 20; i++) {
		fts_read_reg(0xA3, &id);
		if(id == 0x56) {
			break;
		}
		msleep(10);
	}
	if(i >= 20) {
		FTS_ERROR("wait tp fw valid timeout");
	}

#ifdef FTS_TOUCHSCREEN_FOD
	if (fts_data->fod_status != -1 && fts_data->fod_status != 0) {
		FTS_INFO("fod_status = %d\n", fts_data->fod_status);
		fts_fod_recovery();
	}
#endif
	if (fts_data->report_rate_status == 120) {
		FTS_INFO("report_rate = %d\n", fts_data->report_rate_status);
		/*reset Report_Rate to 120HZ*/
		if (fts_write_reg(0x92, 1) < 0) {
			FTS_ERROR("Failed to switch Report_Rate to 120HZ");
		}
	}
	return 0;
}

void fts_irq_disable(void)
{
    unsigned long irqflags;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (!fts_data->irq_disabled) {
        disable_irq_nosync(fts_data->irq);
        fts_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
    unsigned long irqflags = 0;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (fts_data->irq_disabled) {
        enable_irq(fts_data->irq);
        fts_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
    int ret = 0;
    u8 buf[3] = {0xEB, 0xAA, 0x09};

    if (fts_data->bus_type != BUS_TYPE_I2C)
        return;

    ret = fts_write(buf, 3);
    if (ret < 0) {
        FTS_ERROR("hid2std cmd write fail");
    } else {
        msleep(10);
        buf[0] = buf[1] = buf[2] = 0;
        ret = fts_read(NULL, 0, buf, 3);
        if (ret < 0) {
            FTS_ERROR("hid2std cmd read fail");
        } else if ((buf[0] == 0xEB) && (buf[1] == 0xAA) && (buf[2] == 0x08)) {
            FTS_DEBUG("hidi2c change to stdi2c successful");
        } else {
            FTS_DEBUG("hidi2c change to stdi2c not support or fail");
        }
    }
}

static void fts_recover_gesture_from_sleep(struct fts_ts_data *data)
{
	FTS_FUNC_ENTER();
	fts_reset_proc(50);
	fts_tp_state_recovery(data);
	fts_irq_enable();
	FTS_FUNC_EXIT();
}

static int fts_match_cid(struct fts_ts_data *ts_data,
                         u16 type, u8 id_h, u8 id_l, bool force)
{
#ifdef FTS_CHIP_ID_MAPPING
    u32 i = 0;
    u32 j = 0;
    struct ft_chip_id_t chip_id_list[] = FTS_CHIP_ID_MAPPING;
    u32 cid_entries = sizeof(chip_id_list) / sizeof(struct ft_chip_id_t);
    u16 id = (id_h << 8) + id_l;

    memset(&ts_data->ic_info.cid, 0, sizeof(struct ft_chip_id_t));
    for (i = 0; i < cid_entries; i++) {
        if (!force && (type == chip_id_list[i].type)) {
            break;
        } else if (force && (type == chip_id_list[i].type)) {
            FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
            ts_data->ic_info.cid = chip_id_list[i];
            return 0;
        }
    }

    if (i >= cid_entries) {
        return -ENODATA;
    }

    for (j = 0; j < FTS_MAX_CHIP_IDS; j++) {
        if (id == chip_id_list[i].chip_ids[j]) {
            FTS_DEBUG("cid:%x==%x", id, chip_id_list[i].chip_ids[j]);
            FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
            ts_data->ic_info.cid = chip_id_list[i];
            return 0;
        }
    }

    return -ENODATA;
#else
    return -EINVAL;
#endif
}


static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    u32 i = 0;
    struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((id_h == 0x0) || (id_l == 0x0)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_INFO("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (fw_valid == VALID) {
            if (((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                || (!fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 0)))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl))) {
                break;
            }
        }
    }

    if (i >= ctype_entries) {
        return -ENODATA;
    }

    fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 1);
    ts_data->ic_info.ids = ctype[i];
    return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
    int ret = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;

    id_cmd[0] = FTS_CMD_START1;
    id_cmd[1] = FTS_CMD_START2;
    ret = fts_write(id_cmd, 2);
    if (ret < 0) {
        FTS_ERROR("start cmd write fail");
        return ret;
    }

    msleep(FTS_CMD_START_DELAY);
    id_cmd[0] = FTS_CMD_READ_ID;
    id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
    if (ts_data->ic_info.is_incell)
        id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
    else
        id_cmd_len = FTS_CMD_READ_ID_LEN;
    ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
    if ((ret < 0) || (chip_id[0] == 0x0) || (chip_id[1] == 0x0)) {
        FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
        return -EIO;
    }

    id[0] = chip_id[0];
    id[1] = chip_id[1];
    return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;


    do {
        ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
        ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("chip id read invalid, read:0x%02x%02x",
                      chip_id[0], chip_id[1]);
        } else {
            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
            if (!ret)
                break;
            else
                FTS_DEBUG("TP not ready, read:0x%02x%02x",
                          chip_id[0], chip_id[1]);
        }

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
        FTS_INFO("fw is invalid, need read boot id");
        if (ts_data->ic_info.hid_supported) {
            fts_hid2std();
        }

        ret = fts_read_bootid(ts_data, &chip_id[0]);
        if (ret <  0) {
            FTS_ERROR("read boot id fail");
            return ret;
        }

        ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
        if (ret < 0) {
            FTS_ERROR("can't get ic informaton");
            return ret;
        }
    }

    FTS_INFO("get ic information, chip id = 0x%02x%02x(cid type=0x%x)",
             ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl,
             ts_data->ic_info.cid.type);

    return 0;
}

/*****************************************************************************
*  char lockdown[8];//maximum:8
*****************************************************************************/
int fts_get_lockdown_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int i = 0;
	int count = 0;
	u8 temp_lockdown[256] = {0};
	ret = fts_read_lockdown_info_proc(ts_data->lockdown_info);
	if (ret) {
		FTS_ERROR("lockdown_info init fail");
		return -EIO;
	}
	for(i = 0; i < 8; i++) {
		count += sprintf(temp_lockdown + count, " %02x ", ts_data->lockdown_info[i]);
	}
	FTS_INFO("lockdown information before formatting:%s", ts_data->lockdown_info);
	FTS_INFO("lockdown information after formatting:%s", temp_lockdown);
	return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, u32 datalen)
{
    u32 i = 0;
    u32 count = 0;
    char *tmpbuf = NULL;

    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < datalen; i++) {
        count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
        if (count >= 1024)
            break;
    }
    FTS_DEBUG("touch_buf:%s", tmpbuf);


    kfree(tmpbuf);
    tmpbuf = NULL;

}

void fts_release_all_finger(void)
{
    struct fts_ts_data *ts_data = fts_data;
    struct input_dev *input_dev = ts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    u32 max_touches = ts_data->pdata->max_touch_number;
#endif
#ifdef FTS_TOUCHSCREEN_FOD
	fts_data->finger_in_fod = false;
	fts_data->overlap_area = 0;
	FTS_INFO("%s : finger_in_fod = %d", __func__, fts_data->finger_in_fod);
#endif

    mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < max_touches; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
        last_touch_events_collect(finger_count, 0);
    }
#else
    input_mt_sync(input_dev);
#endif
#ifdef FTS_TOUCHSCREEN_FOD
    input_report_key(input_dev, BTN_INFO, 0);
    update_fod_press_status(0);
#endif
    input_report_key(input_dev, BTN_TOUCH, 0);
#ifdef CONFIG_TOUCH_BOOST
    lpm_disable_for_dev(false, LPM_EVENT_INPUT);
#endif
    input_sync(input_dev);

#if FTS_PEN_EN
    input_report_key(ts_data->pen_dev, BTN_TOOL_PEN, 0);
    input_report_key(ts_data->pen_dev, BTN_TOUCH, 0);
#ifdef CONFIG_TOUCH_BOOST
    lpm_disable_for_dev(false, LPM_EVENT_INPUT);
#endif
    input_sync(ts_data->pen_dev);
#endif

    ts_data->touch_points = 0;
    ts_data->key_state = 0;
    mutex_unlock(&ts_data->report_mutex);
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *ts_data, struct ts_event *kevent)
{
    int i = 0;
    int x = kevent->x;
    int y = kevent->y;
    int *x_dim = &ts_data->pdata->key_x_coords[0];
    int *y_dim = &ts_data->pdata->key_y_coords[0];

    if (!ts_data->pdata->have_key)
        return -EINVAL;
    for (i = 0; i < ts_data->pdata->key_number; i++) {
        if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
            (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
            if (EVENT_DOWN(kevent->flag)
                && !(ts_data->key_state & (1 << i))) {
                input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 1);
                ts_data->key_state |= (1 << i);
                FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
            } else if (EVENT_UP(kevent->flag)
                       && (ts_data->key_state & (1 << i))) {
                input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 0);
                ts_data->key_state &= ~(1 << i);
                FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
            }
            return 0;
        }
    }
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *ts_data, struct ts_event *events)
{
	int i = 0;
	int touch_down_point_cur = 0;
	int touch_point_pre = ts_data->touch_points;
	u32 max_touch_num = ts_data->pdata->max_touch_number;
	bool touch_event_coordinate = false;
	struct input_dev *input_dev = ts_data->input_dev;
	static ktime_t down_time;
	ktime_t up_time;
	int touch_boost = 1;

	for (i = 0; i < ts_data->touch_event_num; i++) {
		if (fts_input_report_key(ts_data, &events[i]) == 0)
			continue;

		touch_event_coordinate = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
#if FTS_REPORT_PRESSURE_EN
			input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			/*FTS_DEBUG("fod_finger_skip%d,overlap_area%d,", ts_data->fod_finger_skip, ts_data->overlap_area);*/
			if (!ts_data->fod_finger_skip && ts_data->overlap_area == 100 && !ts_data->suspended) {
				/*be useful when panel has been resumed */
				input_report_key(input_dev, BTN_INFO, 1);
				update_fod_press_status(1);
				/* mi_disp_set_fod_queue_work(1, true); */
		}
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, ts_data->overlap_area);
			input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, events[i].minor);
		/*input_report_abs(input_dev, ABS_MT_WIDTH_MINOR, ts_data->overlap_area);*/
			/*input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);*/
			input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);

			touch_down_point_cur |= (1 << events[i].id);
			touch_point_pre |= (1 << events[i].id);

			if ((ts_data->log_level >= 3) ||
				((ts_data->log_level >= 1) && (events[i].flag == FTS_TOUCH_DOWN))) {
				__test_and_set_bit(events[i].id, &fts_data->touch_id);
				/*FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
							events[i].id, events[i].x, events[i].y,
							events[i].p, events[i].area);*/
				if (events[i].id == 0) { //first finger down
					FTS_INFO("[B]FIRST POINT DOWN!");
					if (touch_static) {
						down_time = ktime_get();
						FTS_INFO("touch coord static down");
						if (touch_boost_en) {
							FTS_INFO("notify hal to boost");
							add_common_data_to_buf(0, SET_CUR_VALUE, Touch_GAMETURBOTOOL_FOLLOWUP, 1, &touch_boost);
							touch_boost_en = false;
						}
					}
				}
			}
			last_touch_events_collect(events[i].id, 1);
		} else {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
			touch_point_pre &= ~(1 << events[i].id);
			if (ts_data->log_level >= 1)
					FTS_DEBUG("[B]P%d UP!", events[i].id);
			__test_and_clear_bit(events[i].id, &fts_data->touch_id);
			last_touch_events_collect(events[i].id, 0);
		}
	}

	if (unlikely(touch_point_pre ^ touch_down_point_cur)) {
		for (i = 0; i < max_touch_num; i++)  {
			if ((1 << i) & (touch_point_pre ^ touch_down_point_cur)) {
				if (ts_data->log_level >= 1)
						FTS_DEBUG("[B]P%d UP!", i);
				__test_and_clear_bit(events[i].id, &fts_data->touch_id);
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
				last_touch_events_collect(i, 0);
			}
		}
	}

	if (touch_down_point_cur)
		input_report_key(input_dev, BTN_TOUCH, 1);
	else if (touch_event_coordinate || ts_data->touch_points) {
		if (ts_data->touch_points && (ts_data->log_level >= 1)) {
			FTS_DEBUG("[B]Points All Up!");
			if (touch_static) {
				up_time = ktime_get();
				FTS_INFO("touch coord static up");
				if (ktime_to_ms(ktime_sub(up_time, down_time)) > 20) {
					coord_cnt++;
					FTS_INFO("coord cnt: %lld", coord_cnt);
				}
			}
		}
		input_report_key(input_dev, BTN_TOUCH, 0);
#ifdef CONFIG_TOUCH_BOOST
		lpm_disable_for_dev(false, LPM_EVENT_INPUT);
#endif
	}

	ts_data->touch_points = touch_down_point_cur;
	input_sync(input_dev);
	return 0;
}
#else
static int fts_input_report_a(struct fts_ts_data *ts_data, struct ts_event *events)
{
    int i = 0;
    int touch_down_point_num_cur = 0;
    bool touch_event_coordinate = false;
    struct input_dev *input_dev = ts_data->input_dev;

    for (i = 0; i < ts_data->touch_event_num; i++) {
        if (fts_input_report_key(ts_data, &events[i]) == 0) {
            continue;
        }

        touch_event_coordinate = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);
            input_mt_sync(input_dev);

            touch_down_point_num_cur++;
            if ((ts_data->log_level >= 2) ||
                ((ts_data->log_level == 1) && (events[i].flag == FTS_TOUCH_DOWN))) {
                FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                          events[i].id, events[i].x, events[i].y,
                          events[i].p, events[i].area);
            }
        }
    }

    if (touch_down_point_num_cur)
        input_report_key(input_dev, BTN_TOUCH, 1);
    else if (touch_event_coordinate || ts_data->touch_points) {
        if (ts_data->touch_points && (ts_data->log_level >= 1))
            FTS_DEBUG("[A]Points All Up!");
        input_report_key(input_dev, BTN_TOUCH, 0);
        input_mt_sync(input_dev);
    }

    ts_data->touch_points = touch_down_point_num_cur;
    input_sync(input_dev);
    return 0;
}
#endif

#if FTS_PEN_EN
static int fts_input_pen_report(struct fts_ts_data *ts_data, u8 *pen_buf)
{
    struct input_dev *pen_dev = ts_data->pen_dev;
    struct pen_event *pevt = &ts_data->pevent;

    /*get information of stylus*/
    pevt->inrange = (pen_buf[2] & 0x20) ? 1 : 0;
    pevt->tip = (pen_buf[2] & 0x01) ? 1 : 0;
    pevt->flag = pen_buf[3] >> 6;
    pevt->id = pen_buf[5] >> 4;
    pevt->x = ((pen_buf[3] & 0x0F) << 8) + pen_buf[4];
    pevt->y = ((pen_buf[5] & 0x0F) << 8) + pen_buf[6];
    pevt->p = ((pen_buf[7] & 0x0F) << 8) + pen_buf[8];
    pevt->tilt_x = (short)((pen_buf[9] << 8) + pen_buf[10]);
    pevt->tilt_y = (short)((pen_buf[11] << 8) + pen_buf[12]);
    pevt->azimuth = ((pen_buf[13] << 8) + pen_buf[14]);
    pevt->tool_type = BTN_TOOL_PEN;

    input_report_key(pen_dev, BTN_STYLUS, !!(pen_buf[2] & 0x02));
    input_report_key(pen_dev, BTN_STYLUS2, !!(pen_buf[2] & 0x08));

    switch (ts_data->pen_etype) {
    case STYLUS_DEFAULT:
        if (pevt->tip && pevt->p) {
            if ((ts_data->log_level >= 2) || (!pevt->down))
                FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d DOWN",
                          pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag,
                          pevt->tilt_x, pevt->tilt_y);
            input_report_abs(pen_dev, ABS_X, pevt->x);
            input_report_abs(pen_dev, ABS_Y, pevt->y);
            input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
            input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
            input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
            input_report_key(pen_dev, BTN_TOUCH, 1);
            input_report_key(pen_dev, BTN_TOOL_PEN, 1);
            pevt->down = 1;
        } else if (!pevt->tip && pevt->down) {
            FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d UP",
                      pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag,
                      pevt->tilt_x, pevt->tilt_y);
            input_report_abs(pen_dev, ABS_X, pevt->x);
            input_report_abs(pen_dev, ABS_Y, pevt->y);
            input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
            input_report_key(pen_dev, BTN_TOUCH, 0);
            input_report_key(pen_dev, BTN_TOOL_PEN, 0);
            pevt->down = 0;
        }
        input_sync(pen_dev);
        break;
    case STYLUS_HOVER:
        if (ts_data->log_level >= 1)
            FTS_DEBUG("[PEN][%02X]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d,%d",
                      pen_buf[2], pevt->x, pevt->y, pevt->p, pevt->tip,
                      pevt->flag, pevt->tilt_x, pevt->tilt_y, pevt->azimuth);
        input_report_abs(pen_dev, ABS_X, pevt->x);
        input_report_abs(pen_dev, ABS_Y, pevt->y);
        input_report_abs(pen_dev, ABS_Z, pevt->azimuth);
        input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
        input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
        input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
        input_report_key(pen_dev, BTN_TOOL_PEN, EVENT_DOWN(pevt->flag));
        input_report_key(pen_dev, BTN_TOUCH, pevt->tip);
        input_sync(pen_dev);
        break;
    default:
        FTS_ERROR("Unknown stylus event");
        break;
    }

    return 0;
}
#endif

static int fts_read_and_report_foddata(struct fts_ts_data *data)
{
	u8 buf[9] = { 0 };
#ifdef CONFIG_FOCAL_HWINFO
	char ch[64] = {0x00,};
#endif
	int ret;
	int x, y, z;
	data->touch_fod_addr = FTS_REG_FOD_OUTPUT_ADDRESS;
	data->touch_fod_size = 9;
	ret = fts_read(&data->touch_fod_addr, 1, buf, data->touch_fod_size);
	if (ret < 0) {
		FTS_ERROR("read fod failed, ret:%d", ret);
		return ret;
	}
	/*
	 * buf[0]: point id
	 * buf[1]:event type， 0x24 is doubletap, 0x25 is single tap, 0x26 is fod pointer event
	 * buf[2]: touch area/fod sensor area
	 * buf[3]: touch area
	 * buf[4-7]: x,y position
	 * buf[8]:pointer up or down, 0 is down, 1 is up
	 */
	switch (buf[1]) {
	case 0x24:
		FTS_INFO("DoubleClick Gesture detected, Wakeup panel\n");
		input_report_key(data->input_dev, KEY_WAKEUP, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_WAKEUP, 0);
		input_sync(data->input_dev);
#ifdef CONFIG_FOCAL_HWINFO
		data->dbclick_count++;
		snprintf(ch, sizeof(ch), "%d", data->dbclick_count);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DBCLICK_COUNT, ch);
#endif
		break;
	case 0x25:
		if (data->nonui_status != 0) {
			FTS_INFO("nonui_status is one/two, don't report key goto\n");
			return 0;
		}
		FTS_INFO("FOD status report KEY_GOTO\n");
		input_report_key(data->input_dev, KEY_GOTO, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_GOTO, 0);
		input_sync(data->input_dev);
		break;
	case 0x26:
		x = (buf[4] << 8) | buf[5];
		y = (buf[6] << 8) | buf[7];
		z = buf[3];
		if (data->log_level >= 2) {
			FTS_INFO("FTS:read fod data: 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x anxis_x: %d anxis_y: %d\n",
					buf[0], buf[1], buf[2], buf[3], buf[8], x, y);
		}
		if (buf[8] == 0) {
			mutex_lock(&data->report_mutex);
			if (!data->fod_finger_skip)
				data->overlap_area = 100;
			if (data->old_point_id != buf[0]) {
				if (data->old_point_id == 0xff)
					data->old_point_id = buf[0];
				else
					data->point_id_changed = true;
			}
			data->finger_in_fod = true;
			if (data->suspended && data->fod_status == 0) {
				if (data->log_level >= 2) {
					FTS_INFO("Panel off and fod status : %d, don't report touch down event\n", data->fod_status);
				}
				mutex_unlock(&data->report_mutex);
				return 0;
			}
			if (!data->suspended) {
				/* report value and 0x152 in @fts_input_report_b */
				if (data->log_level >= 2) {
					FTS_INFO("FTS:touch is not in suspend state, report x,y value by touch nomal report\n");
				}
				mutex_unlock(&data->report_mutex);
				return -EINVAL;
			}
			if (data->nonui_status == 2) {
				if (data->log_level >= 2) {
					FTS_INFO("nonui_status is two, don't report 152\n");
				}
				mutex_unlock(&data->report_mutex);
				return 0;
			}
			if (!data->fod_finger_skip && data->finger_in_fod) {
				input_mt_slot(data->input_dev, buf[0]);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 1);
				input_report_key(data->input_dev, BTN_INFO, 1);
				update_fod_press_status(1);
				/* mi_disp_set_fod_queue_work(1, true); */
				input_report_key(data->input_dev, BTN_TOUCH, 1);
				input_report_key(data->input_dev, BTN_TOOL_FINGER, 1);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, z);
				input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->overlap_area);
				input_report_abs(data->input_dev, ABS_MT_WIDTH_MINOR, data->overlap_area);
				input_report_abs(data->input_dev, ABS_MT_PRESSURE, z);
				input_sync(data->input_dev);
				if (data->log_level >= 2) {
					FTS_INFO("Report_0x152 suspend DOWN report_area %d sucess for miui", data->overlap_area);
				}
			}
			mutex_unlock(&data->report_mutex);
		} else {
			input_report_key(data->input_dev, BTN_INFO, 0);
			update_fod_press_status(0);
			/*mi_disp_set_fod_queue_work(0, true);*/
			input_sync(data->input_dev);
			data->finger_in_fod = false;
			data->fod_finger_skip = false;
			data->old_point_id = 0xff;
			data->point_id_changed = false;
			FTS_INFO("Report_0x152 UP for FingerPrint\n");
			data->overlap_area = 0;
			if (!data->suspended) {
				if (!data->fod_status) {
					/*Turn off FOD enable in the CF register after fingerprint unlocking is successful and FOD finger is up*/
					fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, false);
					FTS_INFO("Turn off FOD enable in the CF register successfully！\n");
                                }
				FTS_INFO("FTS:touch is not in suspend state, report x,y value by touch nomal report\n");
				return -EINVAL;
			}
			mutex_lock(&data->report_mutex);
			input_mt_slot(data->input_dev, buf[0]);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, -1);
			input_sync(data->input_dev);
			mutex_unlock(&data->report_mutex);
		}
		break;
	default:
		data->overlap_area = 0;
		if (data->suspended)
			return 0;
		else
			return -EINVAL;
		break;
	}
	return 0;
}

/*
static int fts_read_touchdata(struct fts_ts_data *ts_data, u8 *buf)
{
    	int ret = 0;
	int temp;

    	ts_data->touch_addr = 0x01;
    	ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);

#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch)
		fts_read_palm_data();
#endif

	temp = fts_read_and_report_foddata(ts_data);
	if (((buf[1] == 0xEF) && (buf[2] == 0xEF) && (buf[3] == 0xEF))
		|| ((ret < 0) && (buf[0] == 0xEF))) {
		fts_release_all_finger();
		fts_fw_recovery();
		ts_data->fw_is_running = true;
		return 1;
	} else if (ret < 0) {
		FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
		return ret;
	}

    	return 0;
}
*/

static int fts_read_touchdata(struct fts_ts_data *ts_data, u8 *buf)
{
    	int ret = 0;
	int temp;
    	ts_data->touch_addr = 0x01;
    	ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);

#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch)
		fts_read_palm_data();
#endif
	temp = fts_read_and_report_foddata(ts_data);
        if (ret < 0) {
	    FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
	    return ret;
        }
        return 0;
}

static int fts_save_debug_data(struct fts_ts_data *ts_data)
{
	u8 *buf = ts_data->touch_buf;
	struct tp_info ti = ts_data->tpinfo;

	if(fts_dump_frame_count >= 10000) {
		fts_dump_frame_count = 0;
	}
	buf[0] = fts_dump_frame_count;
	if(fts_dump_frame_count == 0) {
		buf[1] = 1;
	} else {
		buf[1] = 0;
	}
	fts_dump_frame_count++;

	get_current_timestamp(buf+2, 24);
	copy_touch_rawdata(buf, DUMPBUFF_HEAD+ti.total_len); //拷贝数据
	update_touch_rawdata(); //通知apk读取数据

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
    int ret = 0;
    u8 gesture_en = 0xFF;

    memset(touch_buf, 0xFF, FTS_MAX_TOUCH_BUF - DUMPBUFF_HEAD);
    ts_data->ta_size = ts_data->touch_size;

    /*read touch data*/
    ret = fts_read_touchdata(ts_data, touch_buf);
    if (ret < 0) {
        FTS_ERROR("read touch data fails");
        return TOUCH_ERROR;
    }

    if(ts_data->opendump == 1) {
        fts_save_debug_data(ts_data);
    }

    if (ts_data->log_level >= 3)
        fts_show_touch_buffer(touch_buf, ts_data->ta_size);

    if (ret)
        return TOUCH_IGNORE;

    /*gesture*/
    if (ts_data->suspended && ts_data->gesture_support) {
        ret = fts_read_reg(FTS_REG_GESTURE_EN, &gesture_en);
        if ((ret >= 0) && (gesture_en == ENABLE))
            return TOUCH_GESTURE;
        FTS_DEBUG("gesture not enable in fw, don't process gesture");
    }

    if ((touch_buf[1] == 0xFF) && (touch_buf[2] == 0xFF)
        && (touch_buf[3] == 0xFF) && (touch_buf[4] == 0xFF)) {
        FTS_INFO("touch buff is 0xff, need recovery state");
        return TOUCH_FW_INIT;
    }

    return ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F);
}

static int fts_irq_read_report(struct fts_ts_data *ts_data)
{
    int i = 0;
    int max_touch_num = ts_data->pdata->max_touch_number;
    int touch_etype = 0;
    u8 event_num = 0;
    u8 finger_num = 0;
    u8 pointid = 0;
    u8 base = 0;
    u8 *touch_buf = ts_data->touch_buf + DUMPBUFF_HEAD;
    struct ts_event *events = ts_data->events;

    touch_etype = fts_read_parse_touchdata(ts_data, touch_buf);
    /*adjust log_level to output touch_buf when necessary*/
    if (ts_data->log_level >= 2) {
        fts_show_touch_buffer(touch_buf, ts_data->touch_size);
    }
    switch (touch_etype) {
    case TOUCH_DEFAULT:
        finger_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (finger_num > max_touch_num) {
            FTS_ERROR("invalid point_num(%d)", finger_num);
            return -EIO;
        }

        for (i = 0; i < max_touch_num; i++) {
            base = FTS_ONE_TCH_LEN * i + 2;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= FTS_MAX_ID)
                break;
            else if (pointid >= max_touch_num) {
                FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;

	if (ts_data->pdata->super_resolution_factors == 10) {
		events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 11)
			+ ((touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF) << 3)
			+ (((touch_buf[FTS_TOUCH_OFF_PRE + base] & 0xC0) >> 6) << 1)
			+ ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x20) >> 5);
		events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 11)
			+ ((touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF) << 3)
			+ (((touch_buf[FTS_TOUCH_OFF_PRE + base] & 0x30) >> 4) << 1)
			+ ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x10) >> 4);
		events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base] & 0x7F;
		events[i].p =  touch_buf[FTS_TOUCH_OFF_PRE + base] & 0x0F;
	} else {
		events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8)
			+ (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
		events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8)
			+ (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
		events[i].p =  touch_buf[FTS_TOUCH_OFF_PRE + base];
		events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
	}
	FTS_DEBUG("x:%d,y:%d", events[i].x, events[i].y);


            if (events[i].p <= 0)
                    events[i].p = 0x3F;
            if (events[i].area <= 0)
                    events[i].area = 0x09;

            event_num++;
            if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
                FTS_INFO("abnormal touch data from fw");
                return -EIO;
            }
        }

        if (event_num == 0) {
            FTS_INFO("no touch point information(%02x)", touch_buf[2]);
            return -EIO;
        }
        ts_data->touch_event_num = event_num;

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;

#if FTS_PEN_EN
    case TOUCH_PEN:
        mutex_lock(&ts_data->report_mutex);
        fts_input_pen_report(ts_data, touch_buf);
        mutex_unlock(&ts_data->report_mutex);
        break;
#endif

    case TOUCH_PROTOCOL_v2:

	event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
	if (!event_num || (event_num > max_touch_num)) {
		FTS_ERROR("invalid touch event num(%d)", event_num);
		return -EIO;
	}

	ts_data->touch_event_num = event_num;

	for (i = 0; i < event_num; i++) {
		/* base = FTS_ONE_TCH_LEN_V2 * i + 4;
		 pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
		 if (pointid >= FTS_MAX_ID)
			 break;
		 else if (pointid >= max_touch_num) {
			 FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			 return -EINVAL;
		 }*/

		base = FTS_ONE_TCH_LEN_V2 * i + 4;
		pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
		if (pointid >= max_touch_num) {
			FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)",
					  pointid, max_touch_num);
			return -EINVAL;
		}

		events[i].id = pointid;
		events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;

		events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 12) \
					  + ((touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF) << 4) \
					  + ((touch_buf[FTS_TOUCH_OFF_PRE + base] >> 4) & 0x0F);

		events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 12) \
					  + ((touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF) << 4) \
					  + (touch_buf[FTS_TOUCH_OFF_PRE + base] & 0x0F);

		events[i].x = events[i].x *16 / FTS_HI_RES_X_MAX;
		events[i].y = events[i].y *16 / FTS_HI_RES_X_MAX;
		events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
		events[i].minor = touch_buf[FTS_TOUCH_OFF_MINOR + base];
		events[i].p = 0x3F;

		if (events[i].area <= 0) events[i].area = 0x09;
		if (events[i].minor <= 0) events[i].minor = 0x09;

	}

	mutex_lock(&ts_data->report_mutex);

#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;

    case TOUCH_EXTRA_MSG:
        if (!ts_data->touch_analysis_support) {
            FTS_ERROR("touch_analysis is disabled");
            return -EINVAL;
        }

        event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
        if (!event_num || (event_num > max_touch_num)) {
            FTS_ERROR("invalid touch event num(%d)", event_num);
            return -EIO;
        }

        ts_data->touch_event_num = event_num;
        for (i = 0; i < event_num; i++) {
            base = FTS_ONE_TCH_LEN * i + 4;
            pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
            if (pointid >= max_touch_num) {
                FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)",
                          pointid, max_touch_num);
                return -EINVAL;
            }

            events[i].id = pointid;
            events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
            events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) +
                    (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
            events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) +
                    (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
            events[i].p =  touch_buf[FTS_TOUCH_OFF_PRE + base];
            events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
            if (events[i].p <= 0)
                    events[i].p = 0x3F;
            if (events[i].area <= 0)
                    events[i].area = 0x09;
        }

        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data, events);
#else
        fts_input_report_a(ts_data, events);
#endif
        mutex_unlock(&ts_data->report_mutex);
        break;

    case TOUCH_GESTURE:
        if (fts_gesture_readdata(ts_data, touch_buf) != 0) {
            FTS_ERROR("fail to get gesture data in irq handler");
        }
        break;

    case TOUCH_FW_INIT:
        fts_release_all_finger();
        fts_tp_state_recovery(ts_data);
        break;

    case TOUCH_IGNORE:
    case TOUCH_ERROR:
    case TOUCH_FWDBG:
        break;

    default:
        FTS_INFO("unknown touch event(%d)", touch_etype);
        break;
    }

    return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
    struct fts_ts_data *ts_data = fts_data;
    static struct task_struct *touch_task = NULL;
    struct sched_param par = { .sched_priority = MAX_RT_PRIO - 1};

    if (touch_task == NULL) {
        touch_task = current;
	sched_setscheduler_nocheck(touch_task, SCHED_FIFO, &par);
    }
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
#ifdef CONFIG_TOUCH_BOOST
    touch_irq_boost();
#endif
    if ((ts_data->suspended) && (ts_data->pm_suspend)) {
        if (!wait_for_completion_timeout(
                  &ts_data->pm_completion,
                  msecs_to_jiffies(FTS_TIMEOUT_COMERR_PM))) {
            FTS_ERROR("Bus don't resume from pm(deep),timeout,skip irq");
            return IRQ_HANDLED;
        }
    }
#else
#ifdef CONFIG_TOUCH_BOOST
    touch_irq_boost();
#endif
#endif
    cpu_latency_qos_add_request(&ts_data->pm_qos_req_irq, 0);

    ts_data->intr_jiffies = jiffies;
    fts_prc_queue_work(ts_data);
#ifdef CONFIG_TOUCH_BOOST
    lpm_disable_for_dev(true, LPM_EVENT_INPUT);
#endif
    fts_irq_read_report(ts_data);
    if (ts_data->touch_analysis_support && ts_data->ta_flag) {
        ts_data->ta_flag = 0;
        if (ts_data->ta_buf && ts_data->ta_size)
            memcpy(ts_data->ta_buf, ts_data->touch_buf, ts_data->ta_size);
        wake_up_interruptible(&ts_data->ts_waitqueue);
    }
    cpu_latency_qos_remove_request(&ts_data->pm_qos_req_irq);

    return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
    FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
    ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
                               pdata->irq_gpio_flags,
                               FTS_DRIVER_NAME, ts_data);

    return ret;
}

#if FTS_PEN_EN
static int fts_input_pen_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct input_dev *pen_dev;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    FTS_FUNC_ENTER();
    pen_dev = input_allocate_device();
    if (!pen_dev) {
        FTS_ERROR("Failed to allocate memory for input_pen device");
        return -ENOMEM;
    }

    pen_dev->dev.parent = ts_data->dev;
    pen_dev->name = FTS_DRIVER_PEN_NAME;
    pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    __set_bit(ABS_X, pen_dev->absbit);
    __set_bit(ABS_Y, pen_dev->absbit);
    __set_bit(BTN_STYLUS, pen_dev->keybit);
    __set_bit(BTN_STYLUS2, pen_dev->keybit);
    __set_bit(BTN_TOUCH, pen_dev->keybit);
    __set_bit(BTN_TOOL_PEN, pen_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
    input_set_abs_params(pen_dev, ABS_X, pdata->x_min, pdata->x_max, 0, 0);
    input_set_abs_params(pen_dev, ABS_Y, pdata->y_min, pdata->y_max, 0, 0);
    input_set_abs_params(pen_dev, ABS_PRESSURE, 0, 4096, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_X, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_TILT_Y, -9000, 9000, 0, 0);
    input_set_abs_params(pen_dev, ABS_Z, 0, 36000, 0, 0);

    ret = input_register_device(pen_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_free_device(pen_dev);
        pen_dev = NULL;
        return ret;
    }

    ts_data->pen_dev = pen_dev;
    ts_data->pen_etype = STYLUS_DEFAULT;
    FTS_FUNC_EXIT();
    return 0;
}
#endif

static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int key_num = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;

    FTS_FUNC_ENTER();
    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    /* Init and register Input device */
    input_dev->name = FTS_DRIVER_NAME;
    if (ts_data->bus_type == BUS_TYPE_I2C)
        input_dev->id.bustype = BUS_I2C;
    else
        input_dev->id.bustype = BUS_SPI;
    input_dev->dev.parent = ts_data->dev;

    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    if (pdata->have_key) {
        FTS_INFO("set key capabilities");
        for (key_num = 0; key_num < pdata->key_number; key_num++)
            input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
    }

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif
    input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
    input_set_capability(input_dev, EV_KEY, KEY_GOTO);
#ifdef FTS_TOUCHSCREEN_FOD
    input_set_capability(input_dev, EV_KEY, BTN_INFO);
    input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, pdata->x_min, pdata->x_max - 1, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR, pdata->x_min, pdata->x_max - 1, 0, 0);
#endif

    ret = input_register_device(input_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }

#if FTS_PEN_EN
    ret = fts_input_pen_init(ts_data);
    if (ret) {
        FTS_ERROR("Input-pen device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }
#endif

    ts_data->input_dev = input_dev;
    FTS_FUNC_EXIT();
    return 0;
}

static int fts_buffer_init(struct fts_ts_data *ts_data)
{
    ts_data->touch_buf = kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
    if (!ts_data->touch_buf) {
        FTS_ERROR("failed to alloc memory for touch buf");
        return -ENOMEM;
    }

    ts_data->touch_size = FTS_TOUCH_DATA_LEN_V2;


    ts_data->touch_analysis_support = 0;
    ts_data->ta_flag = 0;
    ts_data->ta_size = 0;

    return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
    int ret = 0;

    ts->pinctrl = devm_pinctrl_get(ts->dev);
    if (IS_ERR_OR_NULL(ts->pinctrl)) {
        FTS_ERROR("Failed to get pinctrl, please check dts");
        ret = PTR_ERR(ts->pinctrl);
        goto err_pinctrl_get;
    }

    ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
    if (IS_ERR_OR_NULL(ts->pins_active)) {
        FTS_ERROR("Pin state[active] not found");
        ret = PTR_ERR(ts->pins_active);
        goto err_pinctrl_lookup;
    }

    ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(ts->pins_suspend)) {
        FTS_ERROR("Pin state[suspend] not found");
        ret = PTR_ERR(ts->pins_suspend);
        goto err_pinctrl_lookup;
    }

	ts->pinctrl_state_spimode = pinctrl_lookup_state(ts->pinctrl, "pmx_gt_spi_mode");
	if (IS_ERR_OR_NULL(ts->pinctrl_state_spimode)) {
		ret = PTR_ERR(ts->pinctrl_state_spimode);
		FTS_ERROR("Can not lookup pinctrl_spi_mode pinstate %d\n", ret);
		goto err_pinctrl_lookup;
	}

    return 0;
err_pinctrl_lookup:
    if (ts->pinctrl) {
        devm_pinctrl_put(ts->pinctrl);
    }
err_pinctrl_get:
    ts->pinctrl = NULL;
    ts->pins_suspend = NULL;
    ts->pins_active = NULL;
    ts->pinctrl_state_spimode = NULL;
    return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_active) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
        if (ret < 0) {
            FTS_ERROR("Set normal pin state error:%d", ret);
        }
    }

    return ret;
}

/*
static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
    int ret = 0;

    if (ts->pinctrl && ts->pins_suspend) {
        ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
        if (ret < 0) {
            FTS_ERROR("Set suspend pin state error:%d", ret);
        }
    }

    return ret;
}
*/

static int fts_pinctrl_select_spimode(struct fts_ts_data *ts)
{
	int ret = 0;
	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pinctrl_state_spimode)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl, ts->pinctrl_state_spimode);
			if (ret < 0)
				FTS_ERROR("Set gesture pin state error:%d", ret);
		}
	}
	return ret;
}

#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;
	FTS_FUNC_ENTER();
	fts_write_reg(0xB6, 0x01);
	msleep(20);
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			usleep_range(1000, 1100);
			if (!IS_ERR_OR_NULL(ts_data->avdd)) {
				ret = regulator_enable(ts_data->avdd);
				if (ret) {
					FTS_ERROR("enable avdd regulator failed,ret=%d", ret);
				}
			}
			if (ts_data->pdata->avdd_gpio > 0) {
				gpio_direction_output(ts_data->pdata->avdd_gpio, 1);
				//FTS_INFO("enable avdd gpio");
			}
			if (!IS_ERR_OR_NULL(ts_data->iovdd)) {
				ret = regulator_enable(ts_data->iovdd);
				if (ret) {
					FTS_ERROR("enable iovdd regulator failed,ret=%d", ret);
				} else {
					FTS_INFO("successs to enable iovdd\n");
				}
			} else
				FTS_ERROR("failed to get iovdd regulator\n");

			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			usleep_range(1000, 1100);
			if (!IS_ERR_OR_NULL(ts_data->iovdd)) {
				ret = regulator_disable(ts_data->iovdd);
				if (ret) {
					FTS_ERROR("disable iovdd regulator failed,ret=%d", ret);
				}/* else {
					FTS_INFO("%s: successs to disable iovdd\n", __func__);
				}*/
			}
			if (!IS_ERR_OR_NULL(ts_data->avdd)) {
				ret = regulator_disable(ts_data->avdd);
				if (ret) {
					FTS_ERROR("disable avdd regulator failed,ret=%d", ret);
				} else {
					FTS_INFO("successs to disable avdd\n");
				}
			}
			if (ts_data->pdata->avdd_gpio > 0) {
				gpio_direction_output(ts_data->pdata->avdd_gpio, 0);
				FTS_INFO("disable avdd gpio");
			}
			ts_data->power_disabled = true;
		}
	}
	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    if (strlen(ts_data->pdata->avdd_name)) {
        ts_data->avdd = regulator_get(ts_data->dev, ts_data->pdata->avdd_name);
        if (IS_ERR_OR_NULL(ts_data->avdd)) {
            ret = PTR_ERR(ts_data->avdd);
            FTS_ERROR("get avdd regulator failed,ret=%d", ret);
            return ret;
        }
        FTS_INFO("get avdd regulator success");
        if (regulator_count_voltages(ts_data->avdd) > 0) {
            ret = regulator_set_voltage(ts_data->avdd, FTS_VTG_MIN_UV,
                                        FTS_VTG_MAX_UV);
            if (ret) {
                FTS_ERROR("avdd regulator set_vtg failed ret=%d", ret);
                regulator_put(ts_data->avdd);
                return ret;
            }
        }
    } else {
        ts_data->avdd = NULL;
        FTS_INFO("avdd name is NULL");
    }

    if (strlen(ts_data->pdata->iovdd_name)) {
        ts_data->iovdd = regulator_get(ts_data->dev, ts_data->pdata->iovdd_name);
        if (IS_ERR_OR_NULL(ts_data->iovdd)) {
            ret = PTR_ERR(ts_data->iovdd);
            FTS_ERROR("get iovdd regulator failed,ret=%d", ret);
            return ret;
        } else {
            FTS_INFO("get iovdd regulator success");
            if (regulator_count_voltages(ts_data->iovdd) > 0) {
                ret = regulator_set_voltage(ts_data->iovdd,
                                            FTS_I2C_VTG_MIN_UV,
                                            FTS_I2C_VTG_MAX_UV);
                if (ret) {
                    FTS_ERROR("iovdd regulator set_vtg failed,ret=%d", ret);
                    regulator_put(ts_data->iovdd);
                    return ret;
                }
            }
        }
    } else {
        ts_data->iovdd = NULL;
        FTS_INFO("iovdd name is NULL");
    }

#if FTS_PINCTRL_EN
    fts_pinctrl_init(ts_data);
    fts_pinctrl_select_normal(ts_data);
    fts_pinctrl_select_spimode(ts_data);
#endif

    ts_data->power_disabled = true;
    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret) {
        FTS_ERROR("fail to enable power(regulator)");
    }

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    fts_power_source_ctrl(ts_data, DISABLE);

    if (!IS_ERR_OR_NULL(ts_data->avdd)) {
        if (regulator_count_voltages(ts_data->avdd) > 0)
            regulator_set_voltage(ts_data->avdd, 0, FTS_VTG_MAX_UV);
        regulator_put(ts_data->avdd);
    }

    if (!IS_ERR_OR_NULL(ts_data->iovdd)) {
        if (regulator_count_voltages(ts_data->iovdd) > 0)
            regulator_set_voltage(ts_data->iovdd, 0, FTS_I2C_VTG_MAX_UV);
        regulator_put(ts_data->iovdd);
    }
    FTS_FUNC_EXIT();
    return 0;
}

/*
static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_suspend(ts_data);
#endif

    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }

    return ret;
}
*/

/*
static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
    int ret = 0;

#if FTS_PINCTRL_EN
    fts_pinctrl_select_normal(ts_data);
#endif

    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret < 0) {
        FTS_ERROR("power on fail, ret=%d", ret);
    }

    return ret;
}
*/

#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    /* request irq gpio */
    if (gpio_is_valid(data->pdata->irq_gpio)) {
        ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]irq gpio request failed");
            goto err_irq_gpio_req;
        }

        ret = gpio_direction_input(data->pdata->irq_gpio);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for irq gpio failed");
            goto err_irq_gpio_dir;
        }
    }

    /* request reset gpio */
    if (gpio_is_valid(data->pdata->reset_gpio)) {
        ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]reset gpio request failed");
            goto err_irq_gpio_dir;
        }

    }

    FTS_FUNC_EXIT();
    return 0;

err_irq_gpio_dir:
    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
    FTS_FUNC_EXIT();
    return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
                             struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
    struct property *prop;
    struct device_node *np = dev->of_node;
    int coords_size;

    prop = of_find_property(np, name, NULL);
    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENODATA;

    coords_size = prop->length / sizeof(u32);
    if (coords_size != FTS_COORDS_ARR_SIZE) {
        FTS_ERROR("invalid:%s, size:%d", name, coords_size);
        return -EINVAL;
    }

    ret = of_property_read_u32_array(np, name, coords, coords_size);
    if (ret < 0) {
        FTS_ERROR("Unable to read %s, please check dts", name);
        pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
        pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
        pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
        pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
        return -ENODATA;
    } else {
        pdata->x_min = coords[0];
        pdata->y_min = coords[1];
        pdata->x_max = coords[2];
        pdata->y_max = coords[3];
    }

    FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
             pdata->y_min, pdata->y_max);
    return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
    int ret = 0;
    struct device_node *np = dev->of_node;
    u32 temp_val = 0;
    const char *name_tmp;

    FTS_FUNC_ENTER();

    ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
    if (ret < 0)
        FTS_ERROR("Unable to get display-coords");

    /* key */
    pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
    if (pdata->have_key) {
        ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key number undefined!");

        ret = of_property_read_u32_array(np, "focaltech,keys",
                                         pdata->keys, pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Keys undefined!");
        else if (pdata->key_number > FTS_MAX_KEYS)
            pdata->key_number = FTS_MAX_KEYS;

        ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
                                         pdata->key_x_coords,
                                         pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key Y Coords undefined!");

        ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
                                         pdata->key_y_coords,
                                         pdata->key_number);
        if (ret < 0)
            FTS_ERROR("Key X Coords undefined!");

        FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
                 "coords:(%d,%d),(%d,%d),(%d,%d)",
                 pdata->key_number,
                 pdata->keys[0], pdata->keys[1], pdata->keys[2],
                 pdata->key_x_coords[0], pdata->key_y_coords[0],
                 pdata->key_x_coords[1], pdata->key_y_coords[1],
                 pdata->key_x_coords[2], pdata->key_y_coords[2]);
    }

    /* reset, irq gpio info */
    pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
                        0, &pdata->reset_gpio_flags);
    if (pdata->reset_gpio < 0)
        FTS_ERROR("Unable to get reset_gpio");

    pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
                      0, &pdata->irq_gpio_flags);
    if (pdata->irq_gpio < 0)
        FTS_ERROR("Unable to get irq_gpio");

	ret = of_get_named_gpio(np, "goodix,avdd-gpio", 0);
	if (ret < 0) {
		FTS_ERROR("can't find avdd-gpio, use other power supply");
		pdata->avdd_gpio = 0;
	} else {
		FTS_INFO("get avdd-gpio[%d] from dt", ret);
		pdata->avdd_gpio = ret;
	}

	memset(pdata->avdd_name, 0, sizeof(pdata->avdd_name));
	ret = of_property_read_string(np, "focaltech,avdd-name", &name_tmp);
	if (!ret) {
		FTS_INFO("avdd name from dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(pdata->avdd_name))
			strncpy(pdata->avdd_name, name_tmp, sizeof(pdata->avdd_name));
		else
			FTS_INFO("invalied avdd name length: %ld > %ld", strlen(name_tmp),
				sizeof(pdata->avdd_name));
	}

	memset(pdata->iovdd_name, 0, sizeof(pdata->iovdd_name));
	ret = of_property_read_string(np, "focaltech,iovdd-name", &name_tmp);
	if (!ret) {
		FTS_INFO("iovdd name from dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(pdata->iovdd_name))
			strncpy(pdata->iovdd_name, name_tmp, sizeof(pdata->iovdd_name));
		else
			FTS_INFO("invalied iovdd name length: %ld > %ld", strlen(name_tmp),
				sizeof(pdata->iovdd_name));
	}

    ret = of_property_read_u32(np, "focaltech,super-resolution-factors", &temp_val);
    if (ret < 0) {
	    FTS_ERROR("Unable to get super-resolution-factors, please use default");
	    pdata->super_resolution_factors = 1;
    }  else
	    pdata->super_resolution_factors = temp_val;

    ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
    if (ret < 0) {
        FTS_ERROR("Unable to get max-touch-number, please check dts");
        pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
    } else {
        if (temp_val < 2)
            pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
        else if (temp_val > FTS_MAX_POINTS_SUPPORT)
            pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
        else
            pdata->max_touch_number = temp_val;
    }

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
             pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);


#ifdef FTS_XIAOMI_TOUCHFEATURE
	ret = of_property_read_u32_array(np, "focaltech,touch-def-array",
						pdata->touch_def_array, 4);
	if (ret < 0) {
		FTS_ERROR("Unable to get touch default array, please check dts");
		return ret;
	}
	ret = of_property_read_u32_array(np, "focaltech,touch-range-array",
						pdata->touch_range_array, 5);
	if (ret < 0) {
		FTS_ERROR("Unable to get touch range array, please check dts");
		return ret;
	}
	ret = of_property_read_u32_array(np, "focaltech,touch-expert-array",
						pdata->touch_expert_array, 4 * EXPERT_ARRAY_SIZE);
	if (ret < 0) {
		FTS_ERROR("Unable to get touch expert array, please check dts");
		return ret;
	}
#endif

    FTS_FUNC_EXIT();
    return 0;
}

static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
								  suspend_work);
	fts_ts_suspend(ts_data->dev);
}

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
								resume_work);
	fts_ts_resume(ts_data->dev);
}

static void fts_sleep2gesture_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
								sleep2gesture_work);
	fts_recover_gesture_from_sleep(ts_data);
}

static int fts_get_charging_status(void)
{
	struct power_supply *usb_psy;
	struct power_supply *dc_psy;
	union power_supply_propval val;
	int ret = 0;
	int is_charging = 0;
	is_charging = !!power_supply_is_system_supplied();
	if (!is_charging)
		return 0;
	dc_psy = power_supply_get_by_name("wireless");
	if (dc_psy) {
		ret = power_supply_get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret < 0)
			FTS_ERROR("Couldn't get DC online status, rc=%d\n", ret);
		else if (val.intval == 1)
			return 1;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		ret = power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret < 0)
			FTS_ERROR("Couldn't get usb online status, rc=%d\n", ret);
		else if (val.intval == 1)
			return 1;
	}
	return 0;
}

/**
 * @brief Write 1/0 to Touch IC 0x8B register depending on whether it is in charge state
 */
static void fts_power_supply_work(struct work_struct *work)
{
	int ret = 0;
	struct fts_ts_data *ts_data =
			container_of(work, struct fts_ts_data, power_supply_work);
	int charger_status = -1;
	if (ts_data == NULL)
		return;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	if (ts_data->pm_suspend) {
		FTS_ERROR("TP is in suspend mode, don't set usb status!");
		return;
	}
#endif
	pm_stay_awake(ts_data->dev);
	charger_status = !!fts_get_charging_status();
	if (charger_status != ts_data->charger_status || ts_data->charger_status <0) {
		ts_data->charger_status = charger_status;
		if(charger_status) {
			FTS_INFO("charger usb in");
			ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, true);
			if (ret < 0) {
				FTS_ERROR("failed to set power supply status:%d", ts_data->charger_status);
			} else {
				FTS_INFO("success to set power supply status:%d", ts_data->charger_status);
			}
		}else {
			FTS_INFO("charger usb out");
			ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, false);
			if (ret < 0) {
				FTS_ERROR("failed to set power supply status:%d", ts_data->charger_status);
			} else {
				FTS_INFO("success to set power supply status:%d", ts_data->charger_status);
			}
		}
	}
	pm_relax(ts_data->dev);
}
/**
 * @brief Charge mode callback function
 *
 * @param nb - Notification chain structure pointer
 * @param event - useless on here,Parameters passed in the notification side
 * @param ptr - useless on here,The pointer passed in on the notification side
 * @return int
 */
static int fts_power_supply_callback(struct notifier_block *nb,
						unsigned long event, void *ptr)
{
/*
 * Find the first address of the variable of type struct fts_ts_data
 * through the power_supply notifier member of the structure nb
 */
	struct fts_ts_data *ts_data =
			container_of(nb, struct fts_ts_data, power_supply_notifier);
	/* If the ts_data structure is found, insert the work into the work queue */
	if (ts_data)
		queue_work(ts_data->ts_workqueue, &ts_data->power_supply_work);
	return 0;
}

#if defined(CONFIG_DRM)

/*fts notifier callback*/
int fts_drm_state_change_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fts_ts_data *core_data =
		container_of(self, struct fts_ts_data, fb_notif);
	struct mi_disp_notifier *evdata = data;
	int blank;

	if (evdata && evdata->data && core_data) {
		blank = *(int *)(evdata->data);
		FTS_INFO("notifier tp event:%lu, code:%d.", event, blank);
		flush_workqueue(core_data->ts_workqueue);
		if (event == MI_DISP_DPMS_EARLY_EVENT && (blank == MI_DISP_DPMS_POWERDOWN || blank == MI_DISP_DPMS_LP1 || blank == MI_DISP_DPMS_LP2)) {
			FTS_INFO("touchpanel suspend by %s", blank ==  MI_DISP_DPMS_POWERDOWN ? "blank" : "doze");
			queue_work(core_data->ts_workqueue, &core_data->suspend_work);
		} else if (event == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {
			FTS_INFO("touchpanel resume");
			queue_work(core_data->ts_workqueue, &core_data->resume_work);
		}
	}

	return 0;
}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
    struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
                                  early_suspend);

    cancel_work_sync(&fts_data->resume_work);
    fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
    struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
                                  early_suspend);

    queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
}
#endif

#ifdef FTS_TOUCHSCREEN_FOD
static ssize_t fts_fod_test_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int value = 0;
	struct fts_ts_data *info = dev_get_drvdata(dev);
	FTS_INFO("%s,buf:%s,count:%zu\n", __func__, buf, count);
	if (kstrtoint(buf, 10, &value))
		return -ENODEV;
	if (value) {
		input_report_key(info->input_dev, BTN_INFO, 1);
		update_fod_press_status(1);
		/* mi_disp_set_fod_queue_work(1, true); */
		input_sync(info->input_dev);
		input_mt_slot(info->input_dev, 0);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, 0);
		input_report_abs(info->input_dev, ABS_MT_WIDTH_MINOR, 1);
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, 5400);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, 21490);
		input_sync(info->input_dev);
	} else {
		input_mt_slot(info->input_dev, 0);
		input_report_abs(info->input_dev, ABS_MT_WIDTH_MINOR, 0);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
		input_report_key(info->input_dev, BTN_INFO, 0);
		update_fod_press_status(0);
		/* mi_disp_set_fod_queue_work(0, true); */
		input_sync(info->input_dev);
	}
	return count;
}
static DEVICE_ATTR(fod_test, (0644), NULL, fts_fod_test_store);
#endif
#ifdef FTS_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static void fts_update_gesture_state(struct fts_ts_data *ts_data, int bit, bool enable)
{
	u8 cmd_shift = 0;
	if (bit == GESTURE_DOUBLETAP)
		cmd_shift = FTS_GESTURE_DOUBLETAP;
	else if (bit == GESTURE_AOD)
		cmd_shift = FTS_GESTURE_AOD;
	mutex_lock(&ts_data->input_dev->mutex);
	if (enable) {
		ts_data->gesture_status |= 1 << bit;
		ts_data->gesture_cmd |= 1 << cmd_shift;
	} else {
		ts_data->gesture_status &= ~(1 << bit);
		ts_data->gesture_cmd &= ~(1 << cmd_shift);
	}
	if (ts_data->suspended) {
		FTS_ERROR("TP is suspended, do not update gesture state");
		ts_data->gesture_cmd_delay = true;
		FTS_INFO("delay gesture state:0x%02X, delay write cmd:0x%02X",
			ts_data->gesture_status, ts_data->gesture_cmd);
		mutex_unlock(&ts_data->input_dev->mutex);
		return;
	}
	FTS_INFO("AOD: %d DoubleClick: %d ", ts_data->gesture_status>>1 & 0x01, ts_data->gesture_status & 0x01);
	FTS_INFO("gesture state:0x%02X, write cmd:0x%02X", ts_data->gesture_status, ts_data->gesture_cmd);
	ts_data->gesture_support = ts_data->gesture_status != 0 ? ENABLE : DISABLE;
	mutex_unlock(&ts_data->input_dev->mutex);
}
static void fts_restore_mode_value(int mode, int value_type)
{
	xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[mode][value_type];
}
static void fts_restore_normal_mode(void)
{
	int i;
	for (i = 0; i <= Touch_Panel_Orientation; i++) {
		if (i != Touch_Panel_Orientation)
			fts_restore_mode_value(i, GET_DEF_VALUE);
	}
}
/*
 *static void fts_write_touchfeature_reg(int mode)
 *{
 *	int ret = 0;
 *	u8 temp_value = (u8)xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
 *
 *	switch (mode) {
 *	case Touch_Game_Mode:
 *		if (temp_value && xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] == 1) {
 *			temp_value = 3;
 *			ret = fts_write_reg(FTS_REG_EDGE_FILTER_EN, temp_value);
 *		} else if (temp_value && xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] == 2) {
 *			temp_value = 4;
 *			ret = fts_write_reg(FTS_REG_EDGE_FILTER_EN, temp_value);
 *		} else if (!temp_value)
 *			fts_restore_normal_mode();
 *		break;
 *	case Touch_UP_THRESHOLD:
 *		ret = fts_write_reg(FTS_REG_SENSIVITY, temp_value);
 *		break;
 *	case Touch_Tolerance:
 *		ret = fts_write_reg(FTS_REG_THDIFF, 3 - temp_value);
 *		break;
 *	case Touch_Aim_Sensitivity:
 *		ret = fts_write_reg();
 *		break;
 *	case Touch_Tap_Stability:
 *		ret = fts_write_reg();
 *		break;
 *	case Touch_Edge_Filter:
 *		ret = fts_write_reg(FTS_REG_EDGE_FILTER_LEVEL, temp_value);
 *		break;
 *	case Touch_Panel_Orientation:
 *		if (temp_value == PANEL_ORIENTATION_DEGREE_90 && xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] == 0)
 *			temp_value = 1;
 *		else if (temp_value == PANEL_ORIENTATION_DEGREE_270 && xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] == 0)
 *			temp_value = 2;
 *		else if (temp_value == PANEL_ORIENTATION_DEGREE_90 && xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] == 1)
 *			temp_value = 3;
 *		else if (temp_value == PANEL_ORIENTATION_DEGREE_270 && xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] == 1)
 *			temp_value = 4;
 *		else
 *			temp_value = 0;
 *		ret = fts_write_reg(FTS_REG_EDGE_FILTER_EN, temp_value);
 *		break;
 *	case Touch_Active_MODE:
 *		break;
 *	default:
 *		ret = -1;
 *		break;
 *	}
 *	if (ret < 0) {
 *		FTS_ERROR("write mode:%d reg failed", mode);
 *	} else {
 *		FTS_INFO("write mode:%d value:%d success", mode, temp_value);
 *		xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] = temp_value;
 *	}
 *}
 */
static void fts_config_game_mode_cmd(struct fts_ts_data *ts_data, u8 *cmd, bool is_expert_mode)
{
	int temp_value;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
	cmd[1] = (u8)(temp_value);
	temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE];
	cmd[2] = (u8)(temp_value ? 30 : 3);
	if (is_expert_mode) {
		temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE];
		cmd[3] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4));
		cmd[4] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 1));
		cmd[5] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 2));
		cmd[6] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 3));
	} else {
		temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
		cmd[3] = (u8)(*(pdata->touch_range_array + temp_value - 1));
		temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
		cmd[4] = (u8)(*(pdata->touch_range_array + temp_value - 1));
		temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE];
		cmd[5] = (u8)(*(pdata->touch_range_array + temp_value - 1));
		temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE];
		cmd[6] = (u8)(*(pdata->touch_range_array + temp_value - 1));
	}
}
static void fts_update_touchmode_data(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int mode = 0;
	u8 mode_set_value = 0;
	u8 mode_addr = 0;
	bool game_mode_state_change = false;
	u8 cmd[7] = {0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	if (ts_data && ts_data->pm_suspend) {
		FTS_ERROR("SYSTEM is in suspend mode, don't set touch mode data");
		return;
	}
#endif
	pm_stay_awake(ts_data->dev);
	mutex_lock(&ts_data->cmd_update_mutex);
	fts_config_game_mode_cmd(ts_data, cmd, ts_data->is_expert_mode);
	ret = fts_write(cmd, sizeof(cmd));
	if (ret < 0) {
		FTS_ERROR("write game mode parameter failed\n");
	} else {
		FTS_INFO("update game mode cmd: %02X,%02X,%02X,%02X,%02X,%02X,%02X",
				cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6]);
		for (mode = Touch_Game_Mode; mode <= Touch_Expert_Mode; mode++) {
			if (mode == Touch_Game_Mode &&
				(xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] !=
					xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE])) {
				game_mode_state_change = true;
				fts_data->gamemode_enabled = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
			}
			xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
		}
	}
	mode = Touch_Panel_Orientation;
	mode_set_value = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
	if (mode_set_value != xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] ||
			game_mode_state_change) {
		mode_addr = FTS_REG_ORIENTATION;
		game_mode_state_change = false;
		if (mode_set_value == PANEL_ORIENTATION_DEGREE_0 ||
				mode_set_value == PANEL_ORIENTATION_DEGREE_180) {
			mode_set_value = ORIENTATION_0_OR_180;
		} else if (mode_set_value == PANEL_ORIENTATION_DEGREE_90) {
			mode_set_value = fts_data->gamemode_enabled ?
				GAME_ORIENTATION_90 : NORMAL_ORIENTATION_90;
		} else if (mode_set_value == PANEL_ORIENTATION_DEGREE_270) {
			mode_set_value = fts_data->gamemode_enabled ?
				GAME_ORIENTATION_270 : NORMAL_ORIENTATION_270;
		}
		ret = fts_write_reg(mode_addr, mode_set_value);
		if (ret < 0) {
			FTS_ERROR("write touch mode:%d reg failed", mode);
		} else {
			FTS_INFO("write touch mode:%d, value: %d, addr:0x%02X",
				mode, mode_set_value, mode_addr);
			xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
		}
	}
	mode = Touch_Edge_Filter;
	mode_set_value = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
	if (mode_set_value != xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE]) {
		mode_addr = FTS_REG_EDGE_FILTER_LEVEL;
		ret = fts_write_reg(mode_addr, mode_set_value);
		if (ret < 0) {
			FTS_ERROR("write touch mode:%d reg failed", mode);
		} else {
			FTS_INFO("write touch mode:%d, value: %d, addr:0x%02X",
				mode, mode_set_value, mode_addr);
			xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
		}
	}
	mutex_unlock(&ts_data->cmd_update_mutex);
	pm_relax(ts_data->dev);
}

/* support 480HZ report rate by interpolation */
#define FTS_HIGH_RATE_CMD		0xC3
int fts_switch_report_rate(struct fts_ts_data *cd, bool on)
{
	int ret = 0;

	ret = fts_write_reg(FTS_HIGH_RATE_CMD, (on == true) ? 1 : 0);
	if (ret < 0) {
		FTS_ERROR("failed send report rate cmd, on = %d", on);
		return -EINVAL;
	} else {
		FTS_INFO("reprot rate switch: %s", (on == true) ? "480HZ" : "240HZ");
	}

	return 0;
}

static void fts_power_status_handle(struct fts_ts_data *ts_data)
{
	flush_workqueue(ts_data->ts_workqueue);

	if (ts_data->power_status) {
		FTS_INFO("SuperWallpaper out");
		// ts_data->super_wallpaper = 0;
		queue_work(ts_data->ts_workqueue, &ts_data->resume_work);
	} else if (!ts_data->power_status) {
		queue_work(ts_data->ts_workqueue, &ts_data->suspend_work);
		FTS_INFO("SuperWallpaper in");
		// ts_data->super_wallpaper = 1;
	}

	return;
}

#define FTS_CMD_IDLE 0x8E
int fts_htc_enter_idle(bool en)
{
	u8 cmd[3] = {FTS_CMD_IDLE, 0x00, 0x00};
	int ret = -1;

	cmd[1] = (en == true) ? 0x1 : 0x0;    //选择IDLE频率 120/2160Hz
	cmd[2] = 0x9;           //高刷时停留时间9s
	ret = fts_write(cmd, sizeof(cmd));
	FTS_INFO("%s send high speed idle cmd %d, time %d",
		ret ? "failed" : "success", cmd[1], cmd[2]);

	return ret;
}

#define FTS_CMD_IDLE_THRESHOLD 0x93
int fts_htc_set_idle_threshold(int threshold)
{
	int ret = -1;

	ret = fts_write_reg(FTS_CMD_IDLE_THRESHOLD, (u8)(threshold & 0xFF));
	FTS_INFO("%s send idle threshold cmd %d, ret %d",
		ret ? "failed" : "success", threshold, ret);

	return ret;
}

#define FTS_CMD_EMPTY_INT 0x94
int fts_htc_enable_empty_int(bool en)
{
	int ret = -1;

	ret = fts_write_reg(FTS_CMD_EMPTY_INT, (en == true) ? 0x1 : 0x0);
	FTS_INFO("%s send idle threshold cmd %d, ret %d",
		ret ? "failed" : "success", en, ret);

	return ret;
}

static int fts_set_cur_value(int mode, int value)
{
	int ret = 0;
	u8 state;
	if (!fts_data || mode < 0) {
		FTS_ERROR("Error, fts_data is NULL or the parameter is incorrect");
		return -1;
	}
	FTS_INFO("touch mode:%d, value:%d", mode, value);

	//use thp mode
	if (mode == Touch_THP_Dump){
		FTS_INFO("Mode:Touch_THP_Dump  Dump_status = %d",value);
		fts_dump_frame_count = 0;
		/*open or close dump*/
		fts_dump_switch(fts_data, value);
		return 0;
	}
	if (mode == Touch_Boost_EN) {
		FTS_INFO("touch coord static enable: %d", value);
		touch_static = value;
		return 0;
	}
	if (mode == Touch_Empty_Int) {
		fts_htc_enable_empty_int(!!value);
		return 0;
	}
	if (mode == Touch_Super_Report) {
		fts_switch_report_rate(fts_data, !!value);
		return 0;
	}
	if (mode == Touch_Idle_Scan_Rate) {
		fts_htc_enter_idle(!!value);
		return 0;
	}
	if (mode == Touch_Idle_THD) {
		fts_htc_set_idle_threshold(!!value);
		return 0;
	}
	if (mode == Touch_Move_Jitter) {
		FTS_INFO("Touch_Move_Jitter not support");
		// TODO:impl
		return 0;
	}
	if (mode == Touch_GAMETURBOTOOL_FOLLOWUP) {
		if (value == 2) {
			touch_boost_en = true;
			FTS_INFO("touch boost enable!");
			mutex_lock(&fts_data->report_mutex);
			if (fts_data->touch_id == 0)
				coord_cnt = 0;
			mutex_unlock(&fts_data->report_mutex);
			return 0;
		}
		return 0;
	}

	if (mode >= Touch_Mode_NUM) {
		FTS_ERROR("mode is error:%d", mode);
		return -EINVAL;
	}
	if (mode == Touch_Power_Status) {
		fts_data->power_status = value;
		fts_power_status_handle(fts_data);
		return 0;
	}
	if (mode == Touch_Doubletap_Mode && value >= 0) {
		FTS_INFO("Mode:DoubleClick  double_status = %d", value);
		fts_data->doubletap_status = value;
		fts_update_gesture_state(fts_data, GESTURE_DOUBLETAP, value != 0 ? true : false);
		return 0;
	}
	if (mode == Touch_Aod_Enable && value >= 0) {
		FTS_INFO("Mode:AOD  aod_status = %d", value);
		fts_data->aod_status = value;
		fts_update_gesture_state(fts_data, GESTURE_AOD, value != 0 ? true : false);
		return 0;
	}
	if (mode == Touch_Report_Rate) {
		FTS_INFO("Mode:Touch_Report_Rate  Report_Rate_status = %d", value);
		if (value == 0) {
			fts_data->report_rate_status = 120;
			/*update Report_Rate to 120HZ*/
			ret = fts_write_reg(0x92, 1);
			if (ret < 0) {
				FTS_ERROR("Failed to switch Report_Rate to 120HZ, ret=%d", ret);
				return -EINVAL;
			}
		}
		if (value == 1) {
			fts_data->report_rate_status = 240;
			/*update Report_Rate to 240HZ*/
			ret = fts_write_reg(0x92, 0);
			if (ret < 0) {
				FTS_ERROR("Failed to switch Report_Rate to 240HZ, ret=%d", ret);
				return -EINVAL;
			}
		}
		return 0;
	}
	if (mode == Touch_FodIcon_Enable && value >= 0) {
		FTS_INFO("Mode:FodIcon  FodIcon_status = %d", value);
		fts_data->fod_icon_status = value;
		return 0;
	}
	if (mode == Touch_Fod_Enable && value >=0){
		FTS_INFO("Mode:FOD  fod_status = %d",value);
		fts_data->fod_status = value;
		fts_data->gesture_support = 1;
		if (!fts_data->finger_in_fod && fts_data->fod_status == 0) {
			fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, false);
		}
		if (fts_data->fod_status == 2) {
			fts_fod_recovery();
		}
		if (fts_data->fod_status == 1) {
			flush_workqueue(fts_data->ts_workqueue);
			fts_read_reg(FTS_REG_POWER_MODE, &state);
			if (state != 0x03) {
				fts_fod_recovery();
			} else {
				flush_workqueue(fts_data->ts_workqueue);
				fts_read_reg(FTS_REG_POWER_MODE, &state);
				if (state == 0x03)
					queue_work(fts_data->ts_workqueue, &fts_data->sleep2gesture_work);
				else
					fts_fod_recovery();
			}
		}
		return 0;
	}
	if (mode == Touch_Nonui_Mode && value >=0){
		FTS_INFO("Mode:Nonui_Mode  nonui_status = %d",value);
		fts_data->nonui_status = value;
		return 0;
	}
	if (mode == Touch_Expert_Mode) {
		FTS_INFO("Enter Mode:Expert_Mode");
		fts_data->is_expert_mode = true;
	} else if (mode >= Touch_UP_THRESHOLD && mode <= Touch_Tap_Stability)
		fts_data->is_expert_mode = false;
	/* orientation for IC:
	 * 0: vertival
	 * 1: left horizontal at normal mode
	 * 2: right horizontal at normal mode
	 * 3: left horizontal at game mode
	 * 4: right horizontal at game mode
	 */
	if (mode > Touch_Panel_Orientation) {
		FTS_ERROR("game mode:%d don't support", mode);
		return -EINVAL;
	}
	if (mode == Touch_Game_Mode && value == 1){
		FTS_INFO("Mode:Game_Mode  Game_Mode_status = %d",value);
		/*Enable instantaneous sampling*/
		fts_write_reg(0x8E, true);
	}

	if (mode < 0 || mode >= Touch_Mode_NUM) {
		FTS_ERROR("fts mode is error:%d", mode);
		return -EINVAL;
	}

	xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] = value;
	if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
	}
	fts_update_touchmode_data(fts_data);
	return 0;
}

static int fts_touch_doze_analysis(int input)
{
	int error =  0;
	int irq_status = 0;

	FTS_INFO("input: %d", input);
	if (!fts_data) {
		FTS_ERROR("fts_data is null");
		return -EIO;
	}

	switch(input) {
		case POWER_RESET:
			flush_workqueue(fts_data->ts_workqueue);
			fts_data->doze_test = 1;
			queue_work(fts_data->ts_workqueue, &fts_data->suspend_work);
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
			flush_workqueue(fts_data->ts_workqueue);
			fts_data->doze_test = 0;
		break;
		case RELOAD_FW:
			flush_workqueue(fts_data->ts_workqueue);
			queue_work(fts_data->ts_workqueue, &fts_data->fwupg_work);
		break;
		case ENABLE_IRQ:
			enable_irq(fts_data->irq);
		break;
		case DISABLE_IRQ:
			disable_irq(fts_data->irq);
		break;
		case REGISTER_IRQ:
			error = request_threaded_irq(fts_data->irq, NULL, fts_irq_handler,
							fts_data->pdata->irq_gpio_flags,
							FTS_DRIVER_NAME, fts_data);
			FTS_INFO("irq:%d, flag:%x", fts_data->irq, fts_data->pdata->irq_gpio_flags);
			if (error < 0)
				FTS_ERROR("Failed to requeset threaded irq:%d", error);
			else
				enable_irq(fts_data->irq);
		break;
		case IRQ_PIN_LEVEL:
			irq_status = gpio_get_value(fts_data->pdata->irq_gpio) == 0 ? 0 : 1;
		break;
		case ENTER_SUSPEND:
			flush_workqueue(fts_data->ts_workqueue);
			queue_work(fts_data->ts_workqueue, &fts_data->suspend_work);
		break;
		case ENTER_RESUME:
			flush_workqueue(fts_data->ts_workqueue);
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		break;
		default:
			FTS_ERROR("%s don't support touch doze analysis\n", __func__);
			break;
	}
	return irq_status;
}

static int fts_reset_mode(int mode)
{
	if (mode == Touch_Game_Mode) {
		fts_restore_normal_mode();
		fts_data->gamemode_enabled = false;
		fts_data->is_expert_mode = false;
		/*Disable instantaneous sampling*/
		fts_write_reg(0x8E, false);
	} else if (mode < Touch_Report_Rate)
		fts_restore_mode_value(mode, GET_DEF_VALUE);
	else
		FTS_ERROR("mode:%d don't support", mode);
	FTS_INFO("mode:%d reset", mode);
	fts_update_touchmode_data(fts_data);
	return 0;
}
static int fts_get_mode_value(int mode, int value_type)
{
	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
		FTS_INFO("mode:%d, value_type:%d, value:%d", mode, value_type, value);
	} else if (mode == Touch_GAMETURBOTOOL_FOLLOWUP) {
		mutex_lock(&fts_data->report_mutex);
		if (fts_data->touch_id) {
			value = 1;
		} else {
			value = coord_cnt > 0 ? 1: 0;
			coord_cnt = 0;
		}
		mutex_unlock(&fts_data->report_mutex);
		FTS_INFO("get follow up value");
	} else if (mode == Touch_THP_Dump) {
		if (fts_data != NULL) {
			value = fts_data->opendump;
		}
	} else if (mode == Touch_THP_Dump_Size) {
		if(fts_data!=NULL) {
			value = (fts_data->tpinfo).total_len;
			FTS_INFO("TOUCH_DUMP_LEN:%d", value);
		} else {
			FTS_INFO("fts_data is null");
		}
	} else {
		FTS_ERROR("mode:%d don't support", mode);
	}
	return value;
}
static int fts_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		FTS_ERROR("mode:%d don't support", mode);
	}
	FTS_INFO("mode:%d, value:%d:%d:%d:%d", mode,
				value[0], value[1], value[2], value[3]);
	return 0;
}
static void fts_charger_status_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->charger_status) {
		FTS_DEBUG("%s, recover charger mode to usb_in\n", __func__);
		fts_write_reg(FTS_REG_CHARGER_MODE_EN, true);
	} else {
		FTS_DEBUG("%s, recover charger mode to usb_out\n", __func__);
		fts_write_reg(FTS_REG_CHARGER_MODE_EN, false);
	}
}
static void fts_game_idle_high_refresh_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->gamemode_enabled) {
		FTS_DEBUG("%s, gamemode enabled, recover game_idle_high_refresh\n", __func__);
		fts_write_reg(0x8E, true);
	}
}
static void fts_report_rate_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->report_rate_status == 120) {
		FTS_DEBUG("%s, recover report_rate to %d\n", __func__, ts_data->report_rate_status);
		/*recover Report_Rate to 120HZ*/
		if (fts_write_reg(0x92, 1) < 0) {
			FTS_ERROR("%s, Failed to switch Report_Rate to 120HZ\n", __func__);
		}
	}
}
static void fts_fod_status_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->fod_status != -1 && ts_data->fod_status != 0) {
		FTS_DEBUG("%s, fod_status = %d, enable CF register\n",  __func__, ts_data->fod_status);
		if (ts_data->suspended) {
			FTS_DEBUG("%s, tp is in suspend mode, write 0xD0 to 1", __func__);
			fts_gesture_reg_write(0x01, true);
		}
		fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, true);
	}
}
static void fts_game_mode_recovery(struct fts_ts_data *ts_data)
{
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE];
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE];
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE];
	fts_update_touchmode_data(ts_data);
}
static void fts_palm_mode_recovery(struct fts_ts_data *ts_data)
{
	int ret = 0;
	ret = fts_palm_sensor_cmd(ts_data->palm_sensor_switch);
	if (ret < 0)
		FTS_ERROR("set palm sensor cmd failed: %d\n", ts_data->palm_sensor_switch);
}
static void fts_init_touchmode_data(struct fts_ts_data *ts_data)
{
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;
	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;
	/* UP_THRESHOLD */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = pdata->touch_def_array[0];
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = pdata->touch_def_array[0];
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = pdata->touch_def_array[0];
	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = pdata->touch_def_array[1];
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = pdata->touch_def_array[1];
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = pdata->touch_def_array[1];
	/*  Aim_Sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = pdata->touch_def_array[2];
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = pdata->touch_def_array[2];
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = pdata->touch_def_array[2];
	/*  Tap_Stability */
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = pdata->touch_def_array[3];
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = pdata->touch_def_array[3];
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = pdata->touch_def_array[3];
	/* panel orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
	/* Expert_Mode*/
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_CUR_VALUE] = 1;
	/* edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;
	FTS_INFO("touchfeature value init done");
}
static int fts_read_palm_data(void)
{
	int ret = 0;
	u8 reg_value;
	if (fts_data == NULL)
		return -EINVAL;
	ret = fts_read_reg(FTS_PALM_DATA, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read palm data error\n");
		return -EINVAL;
	}
	if (reg_value == 0x01)
		update_palm_sensor_value(1);
	else if (reg_value == 0x00)
		update_palm_sensor_value(0);
	if (reg_value == 0x01)
		FTS_INFO("update palm data:0x%02X", reg_value);
	return 0;
}
static int fts_palm_sensor_cmd(int value)
{
	int ret = 0;
	ret = fts_write_reg(FTS_PALM_EN, value ? FTS_PALM_ON : FTS_PALM_OFF);
	if (ret < 0)
		FTS_ERROR("Set palm sensor switch failed!\n");
	else
		FTS_INFO("Set palm sensor switch: %d\n", value);
	return ret;
}
static int fts_palm_sensor_write(int value)
{
	int ret = 0;
	if (fts_data == NULL)
		return -EINVAL;
	fts_data->palm_sensor_switch = value;
	if (fts_data->suspended)
		return 0;
	ret = fts_palm_sensor_cmd(value);
	if (ret < 0)
		FTS_ERROR("set palm sensor cmd failed: %d\n", value);
	if (!fts_data->palm_sensor_switch) {
		FTS_INFO("disable palm reg and update palm_status_value for palm_sensor node :%d", 0);
		update_palm_sensor_value(0);
	}
	return ret;
}

static int fts_get_touch_super_resolution_factor(void)
{
	int temp_value = 1;

	if (fts_data) {
		temp_value = fts_data->pdata->super_resolution_factors;
	}

	return temp_value;
}

static u8 fts_panel_vendor_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[0];
	else
		return 0;
}
static u8 fts_panel_color_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[2];
	else
		return 0;
}
static u8 fts_panel_display_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[1];
	else
		return 0;
}
static char fts_touch_vendor_read(void)
{
	return '3';
}
static void tpdbg_shutdown(struct fts_ts_data *ts_data, bool enable)
{
	if (enable) {
		fts_data->poweroff_on_sleep = true;
		cancel_work_sync(&fts_data->resume_work);
		fts_ts_suspend(&fts_data->client->dev);
	} else {
		fts_ts_resume(&fts_data->client->dev);
	}
}
static void tpdbg_suspend(struct fts_ts_data *ts_data, bool enable)
{
	if (enable) {
		cancel_work_sync(&fts_data->resume_work);
		fts_ts_suspend(&fts_data->client->dev);
	} else {
		fts_ts_resume(&fts_data->client->dev);
	}
}
static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	const char *str = "cmd support as below:\n"
		"\n echo \"irq-disable\" or \"irq-enable\" to ctrl irq\n"
		"\n echo \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n"
		"\n echo \"tp-sd-en\" or \"tp-sd-off\" to ctrl panel in or off sleep status\n";
	loff_t pos = *ppos;
	int len = strlen(str);
	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;
	if (copy_to_user(buf, str, len))
		return -EFAULT;
	*ppos = pos + len;
	return len;
}
static ssize_t tpdbg_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct fts_ts_data *ts_data = file->private_data;
	char *cmd = kzalloc(size, GFP_KERNEL);
	int ret = size;
	if (!cmd)
		return -ENOMEM;
	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}
	if (!strncmp(cmd, "irq-disable", 11))
		fts_irq_disable();
	else if (!strncmp(cmd, "irq-enable", 10))
		fts_irq_enable();
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(ts_data, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(ts_data, false);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_shutdown(ts_data, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		tpdbg_shutdown(ts_data, false);
out:
	kfree(cmd);
	return ret;
}
static int tpdbg_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

#define TPDEBUG_IN_D 1
#ifdef TPDEBUG_IN_D
static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};
#else
static const struct proc_ops tpdbg_operations = {
	/*.owner = THIS_MODULE,*/
	.proc_open = tpdbg_open,
	.proc_read = tpdbg_read,
	.proc_write = tpdbg_write,
	.proc_release = tpdbg_release,
};
int fts_proc_init(void)
{
	struct proc_dir_entry *entry;
	touch_debug = proc_mkdir_data("tp_debug", 0777, NULL, NULL);
	if (IS_ERR_OR_NULL(touch_debug))
		return -ENOMEM;
	entry = proc_create("switch_state", 0644, touch_debug, &tpdbg_operations);
	if (IS_ERR_OR_NULL(entry)) {
		FTS_ERROR("create node fail");
		remove_proc_entry("tp_debug", NULL);
		return -ENOMEM;
	}
	return 0;
}
#endif
void fts_proc_remove(void)
{
	remove_proc_entry("switch_state", touch_debug);
	remove_proc_entry("tp_debug", NULL);
}
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static void fts_init_xiaomi_touchfeature(struct fts_ts_data *ts_data)
{
	mutex_init(&ts_data->cmd_update_mutex);
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
	xiaomi_touch_interfaces.resetMode = fts_reset_mode;
	xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
	xiaomi_touch_interfaces.panel_vendor_read = fts_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = fts_panel_color_read;
	xiaomi_touch_interfaces.panel_display_read = fts_panel_display_read;
	xiaomi_touch_interfaces.touch_vendor_read = fts_touch_vendor_read;
	xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;
	xiaomi_touch_interfaces.touch_doze_analysis = fts_touch_doze_analysis;
	xiaomi_touch_interfaces.get_touch_super_resolution_factor = fts_get_touch_super_resolution_factor;
	/*xiaomi_touch_interfaces.enable_touch_delta = fts_enable_touch_delta;*/
	fts_init_touchmode_data(ts_data);
	xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
}
#endif

int fts_get_tp_info(struct fts_ts_data *ts_data)
{
	u8 touch_info[32] = {0};
	u8 cmd = 0;
	int ret = 0;
	struct tp_info *temp = &(ts_data->tpinfo);

	temp->success_get = 0;
	cmd = 0x9D;
	ret = fts_read(&cmd, 1, touch_info, 32);
	if (ret < 0) {
		FTS_ERROR("read touh info data fail");
		return ret;
	} else {
		temp->success_get = 1;
		temp->total_len = (touch_info[2] << 8) + touch_info[3];
	}

	return 0;
}

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int pdata_size = sizeof(struct fts_ts_platform_data);

    FTS_FUNC_ENTER();
    FTS_INFO("%s", FTS_DRIVER_VERSION);
    ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!ts_data->pdata) {
        FTS_ERROR("allocate memory for platform_data fail");
        return -ENOMEM;
    }

    if (ts_data->dev->of_node) {
        ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
        if (ret) {
            FTS_ERROR("device-tree parse fail");
        }
    } else {
        if (ts_data->dev->platform_data) {
            memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
        } else {
            FTS_ERROR("platform_data is null");
            return -ENODEV;
        }
    }

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (!ts_data->ts_workqueue) {
        FTS_ERROR("create fts workqueue fail");
    }

    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_lock);
    init_waitqueue_head(&ts_data->ts_waitqueue);

#ifdef FTS_TOUCHSCREEN_FOD
	mutex_init(&ts_data->fod_mutex);
	ts_data->fod_status = -1;
#endif
    ts_data->doubletap_status = 0;
    ts_data->aod_status = 0;
    ts_data->report_rate_status = 240;
    /* Init communication interface */
    ret = fts_bus_init(ts_data);
    if (ret) {
        FTS_ERROR("bus initialize fail");
        goto err_bus_init;
    }

    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("input initialize fail");
        goto err_input_init;
    }

    ret = fts_buffer_init(ts_data);
    if (ret) {
        FTS_ERROR("buffer init fail");
        goto err_buffer_init;
    }

    ret = fts_gpio_configure(ts_data);
    if (ret) {
        FTS_ERROR("configure the gpios fail");
        goto err_gpio_config;
    }

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_init(ts_data);
    if (ret) {
        FTS_ERROR("fail to get power(regulator)");
        goto err_power_init;
    }
#endif

#if (!FTS_CHIP_IDC)
    msleep(1);
    ret = gpio_direction_output(fts_data->pdata->reset_gpio, 1);
    if (ret) {
        FTS_ERROR("set gpio reset to high failed");
    } else {
        FTS_INFO("set gpio reset to high success");
    }
    msleep(100);
#ifdef FTS_TOUCHSCREEN_FOD
    if (fts_data->fod_status != -1) {
        FTS_INFO("fod_status = %d\n", fts_data->fod_status);
        fts_fod_recovery();
    }
#endif
#endif

    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_irq_req;
    }

    ret = fts_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }

    ret = fts_create_proc(ts_data);
    if (ret)
	FTS_ERROR("create proc node fail");

    ret = fts_create_sysfs(ts_data);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }

    ts_data->tpdbg_dentry = debugfs_create_dir("tp_debug", NULL);
    if (IS_ERR_OR_NULL(ts_data->tpdbg_dentry))
	FTS_ERROR("create tp_debug dir fail");

#ifdef TPDEBUG_IN_D
    if (IS_ERR_OR_NULL(debugfs_create_file("switch_state", 0660,
			ts_data->tpdbg_dentry, ts_data, &tpdbg_operations)))
	FTS_ERROR("create switch_state fail");
#else
    ret = fts_proc_init();
    if (ret)
	FTS_ERROR("create debug proc failed");
#endif

    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }

    ret = fts_ex_mode_init(ts_data);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }

#if FTS_TEST_EN
    ret = fts_test_init(ts_data);
    if (ret) {
        FTS_ERROR("init host test fail");
    }
#endif

    ret = fts_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }

    fts_get_tp_info(ts_data);

    ret = fts_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }

    ret = fts_fwupg_init(ts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
    }

#ifdef CONFIG_FACTORY_BUILD
    ts_data->fod_status = 1;
    fts_fod_recovery();
#else
    ts_data->fod_status = -1;
#endif

	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
		INIT_WORK(&ts_data->suspend_work, fts_suspend_work);
		INIT_WORK(&ts_data->power_supply_work, fts_power_supply_work);
		INIT_WORK(&ts_data->sleep2gesture_work, fts_sleep2gesture_work);
	}

if (ts_data->fts_tp_class == NULL) {
#ifdef FTS_XIAOMI_TOUCHFEATURE
	ts_data->fts_tp_class = get_xiaomi_touch_class();
#else
	ts_data->fts_tp_class = class_create(THIS_MODULE, "touch");
#endif
	if (ts_data->fts_tp_class) {
		ts_data->fts_touch_dev = device_create(ts_data->fts_tp_class, NULL, 0x38, ts_data, "tp_dev");
		if (IS_ERR(ts_data->fts_touch_dev)) {
			FTS_ERROR("Failed to create device !\n");
			goto err_class_create;
		}
		dev_set_drvdata(ts_data->fts_touch_dev, ts_data);
		if (sysfs_create_file(&ts_data->fts_touch_dev->kobj,  &dev_attr_fod_test.attr)) {
			FTS_ERROR("Failed to create fod_test sysfs group!\n");
			goto err_class_create;
		}
	}
}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	init_completion(&ts_data->pm_completion);
	ts_data->pm_suspend = false;
#endif

	/* Put the callback function in the notification chain */
#if defined(CONFIG_DRM)
    ts_data->fb_notif.notifier_call = fts_drm_state_change_callback;
    if (mi_disp_register_client(&ts_data->fb_notif) < 0) {
        FTS_ERROR("ERROR: register notifier failed!\n");
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
    ts_data->early_suspend.suspend = fts_ts_early_suspend;
    ts_data->early_suspend.resume = fts_ts_late_resume;
    register_early_suspend(&ts_data->early_suspend);
#endif

	ts_data->charger_status = -1;
	/* register charger status change notifier */
	ts_data->power_supply_notifier.notifier_call = fts_power_supply_callback;
	power_supply_reg_notifier(&ts_data->power_supply_notifier);

	fts_init_xiaomi_touchfeature(ts_data);

    FTS_FUNC_EXIT();
    return 0;

err_class_create:
	class_destroy(ts_data->fts_tp_class);
	ts_data->fts_tp_class = NULL;

err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
    fts_power_source_exit(ts_data);
#endif
    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);
    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);
    if (gpio_is_valid(ts_data->pdata->avdd_gpio))
        gpio_free(ts_data->pdata->avdd_gpio);
err_gpio_config:
    kfree_safe(ts_data->touch_buf);
err_buffer_init:
    input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
    input_unregister_device(ts_data->pen_dev);
#endif
err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
    kfree_safe(ts_data->pdata);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

    cancel_work_sync(&fts_data->resume_work);
    cancel_work_sync(&fts_data->suspend_work);
    fts_point_report_check_exit(ts_data);
    fts_release_apk_debug_channel(ts_data);
    fts_proc_remove();
    fts_remove_sysfs(ts_data);
    fts_ex_mode_exit(ts_data);

    fts_fwupg_exit(ts_data);

#if FTS_TEST_EN
    fts_test_exit(ts_data);
#endif

    fts_esdcheck_exit(ts_data);

    fts_gesture_exit(ts_data);

    free_irq(ts_data->irq, ts_data);

    power_supply_unreg_notifier(&ts_data->power_supply_notifier);

    input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
    input_unregister_device(ts_data->pen_dev);
#endif

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_DRM)
    //if (active_panel){
		mi_disp_unregister_client(&ts_data->fb_notif);
    //}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    unregister_early_suspend(&ts_data->early_suspend);
#endif

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_source_exit(ts_data);
#endif

    fts_bus_exit(ts_data);

    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);

    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);

    if (gpio_is_valid(ts_data->pdata->avdd_gpio))
        gpio_free(ts_data->pdata->avdd_gpio);

    kfree_safe(ts_data->touch_buf);
    kfree_safe(ts_data->pdata);
    kfree_safe(ts_data);

    FTS_FUNC_EXIT();

    return 0;
}

/*
static int fts_ts_suspend(struct device *dev)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;

    FTS_FUNC_ENTER();
    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return 0;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return 0;
    }

    fts_esdcheck_suspend(ts_data);

    if (ts_data->gesture_support) {
        fts_gesture_suspend(ts_data);
    } else {
        fts_irq_disable();

        FTS_INFO("make TP enter into sleep mode");
        ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
        if (ret < 0)
            FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

        if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
            ret = fts_power_source_suspend(ts_data);
            if (ret < 0) {
                FTS_ERROR("power enter suspend fail");
            }
#endif
        }
    }

    fts_release_all_finger();
    ts_data->suspended = true;
    FTS_FUNC_EXIT();
    return 0;
}
*/

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}
	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}
	/* close dump */
	if(ts_data->opendump) {
		fts_dump_switch(ts_data, 0);
		ts_data->opendump = true;
	}
#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch) {
		FTS_INFO("palm sensor ON, switch to OFF");
		update_palm_sensor_value(0);
		fts_palm_sensor_cmd(0);
	}
	if (ts_data->gesture_cmd_delay) {
		ts_data->gesture_support = ts_data->gesture_status != 0 ? ENABLE : DISABLE;
		FTS_INFO("suspended gesture state:0x%02X, write cmd:0x%02X",
			ts_data->gesture_status, ts_data->gesture_cmd);
		ts_data->gesture_cmd_delay = false;
	}
	if ((ts_data->fod_status != 0 && ts_data->fod_status != -1) || ts_data->doubletap_status || ts_data->aod_status ) {
		ts_data->gesture_support = 1;
	}
	/*Fod_status is 0 when the fingerprint is closed after locking the screen*/
	if (!ts_data->doubletap_status && !ts_data->aod_status && !ts_data->fod_status) {
		ts_data->gesture_support = 0;
	}
#endif
#ifdef FTS_TOUCHSCREEN_FOD
	if ((ts_data->fod_status == -1 || ts_data->fod_status == 100)) {
		FTS_INFO("clear CF reg");
		 ret = fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, false);
		if (ret < 0)
			FTS_ERROR("%s fts_fod_reg_write failed\n", __func__);
	}
	if ((ts_data->fod_status != -1 && ts_data->fod_status != 100)) {
		ret = fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, true);
		if (ret < 0)
			FTS_ERROR("%s fts_fod_reg_write failed\n", __func__);
		fts_gesture_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, true);
		if (ret < 0)
			FTS_ERROR("%s fts_fod_reg_write failed\n", __func__);
	}
#endif
	fts_esdcheck_suspend(ts_data);
#ifdef CONFIG_FACTORY_BUILD
	ts_data->poweroff_on_sleep = true;
#endif
	FTS_INFO("%s : gesture_support:%d  poweroff_on_sleep:%d ", __func__, ts_data->gesture_support, ts_data->poweroff_on_sleep);
	if (ts_data->gesture_support && !ts_data->poweroff_on_sleep)
		fts_gesture_suspend(ts_data);
	else {
		fts_irq_disable();
		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
	}
	fts_release_all_finger();

	if(ts_data->doze_test == 1) {
		ret = fts_power_source_ctrl(ts_data, DISABLE);
		if (ret < 0)
			FTS_ERROR("%s: ERROR Failed to enable regulators\n", __func__);
	}

	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;
	int ret = 0;
	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}
	ts_data->suspended = false;

	if(ts_data->doze_test == 1) {
		ret = fts_power_source_ctrl(ts_data, ENABLE);
		if (ret < 0)
			FTS_ERROR("%s: ERROR Failed to enable regulators\n", __func__);
	}

#ifndef CONFIG_FACTORY_BUILD
#ifdef FTS_TOUCHSCREEN_FOD
	FTS_INFO("%s finger_in_fod:%d fod_finger_skip:%d\n", __func__, ts_data->finger_in_fod, ts_data->fod_finger_skip);
	if (!ts_data->finger_in_fod && !ts_data->fod_finger_skip) {
		FTS_INFO("resume reset");
		fts_reset_proc(40);
		fts_recover_after_reset();
		fts_release_all_finger();
	}
#endif
#endif
	if (!ts_data->ic_info.is_incell) {
#ifdef CONFIG_FACTORY_BUILD
		fts_reset_proc(40);
		fts_recover_after_reset();
		fts_release_all_finger();
#endif
	}

	fts_ex_mode_recovery(ts_data);
	fts_esdcheck_resume(ts_data);
#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch) {
		fts_palm_sensor_cmd(1);
		FTS_INFO("palm sensor OFF, switch to ON");
	}
#endif
	/* enable charger mode */
	if (ts_data->charger_status)
		fts_write_reg(FTS_REG_CHARGER_MODE_EN, true);
	if (ts_data->gesture_support && !ts_data->poweroff_on_sleep)
		fts_gesture_resume(ts_data);
	fts_irq_enable();
	ts_data->poweroff_on_sleep = false;
#ifdef FTS_TOUCHSCREEN_FOD
	fts_gesture_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, false);
#endif

	/* open dump when open debug node*/
	if(ts_data->opendump) {
		fts_dump_switch(ts_data, 1);
	}

	FTS_FUNC_EXIT();
	return 0;
}


#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system enters into pm_suspend");
    ts_data->pm_suspend = true;
    reinit_completion(&ts_data->pm_completion);
    return 0;
}

static int fts_pm_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_INFO("system resumes from pm_suspend");
    ts_data->pm_suspend = false;
    complete(&ts_data->pm_completion);
    return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
    .suspend = fts_pm_suspend,
    .resume = fts_pm_resume,
};
#endif

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_probe(struct spi_device *spi)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;

    FTS_INFO("Touch Screen(SPI BUS) driver probe...");

#if (FTS_CHIP_TYPE == _FT8719) || (FTS_CHIP_TYPE == _FT8615) || (FTS_CHIP_TYPE == _FT8006P) || (FTS_CHIP_TYPE == _FT7120)
    spi->mode = SPI_MODE_1;
#else
    spi->mode = SPI_MODE_0;
#endif
    /*spi->mode = SPI_MODE_0;*/
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) {
        FTS_ERROR("spi setup fail");
        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->log_level = 1;

    ts_data->bus_type = BUS_TYPE_SPI_V2;
    ts_data->poweroff_on_sleep = false;
    spi_set_drvdata(spi, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
        kfree_safe(ts_data);
        return ret;
    }

    FTS_INFO("Touch Screen(SPI BUS) driver probe successfully");
    return 0;
}

static void fts_ts_remove(struct spi_device *spi)
{
    fts_ts_remove_entry(spi_get_drvdata(spi));
}

static void fts_ts_shutdown(struct spi_device *spi)
{
    FTS_INFO("fts_ts_shutdown enter");
    if (fts_data) {
        fts_power_source_ctrl(fts_data, DISABLE);
    }
}

static const struct spi_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech,n11a-3683g-spi", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .shutdown = fts_ts_shutdown,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
        .pm = &fts_dev_pm_ops,
#endif
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;
	int gpio_123;

	gpio_direction_input(DISP_ID_DET);
	gpio_123 = gpio_get_value(DISP_ID_DET);
	pr_info("gpio_123 = %d\n", gpio_123);
	if (!gpio_123) {
		pr_info("TP is fts");
	} else {
		pr_info("TP is goodix");
		return 0;
	}

	FTS_FUNC_ENTER();
	ret = spi_register_driver(&fts_ts_driver);
	if (ret != 0)
		FTS_ERROR("Focaltech touch screen driver init failed!");
	FTS_FUNC_EXIT();
	return ret;
}

static void __exit fts_ts_exit(void)
{
    spi_unregister_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
/*late_initcall(fts_ts_init);*/
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
