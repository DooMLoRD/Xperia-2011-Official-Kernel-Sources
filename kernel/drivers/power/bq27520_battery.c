/*
 * linux/drivers/power/bq27520_battery.c
 *
 * TI BQ27520 Fuel Gauge interface
 *
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 *
 * Authors: James Jacobsson <james.jacobsson@sonyericsson.com>
 *          Imre Sunyi <imre.sunyi@sonyericsson.com>
 *          Hiroyuki Namba <Hiroyuki.Namba@sonyericsson.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <asm/atomic.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/i2c/bq27520_battery.h>

#include <asm/mach-types.h>

#define REG_CMD_CONTROL 0x00
#define REG_CMD_TEMPERATURE 0x06
#define REG_CMD_VOLTAGE 0x08
#define REG_CMD_FLAGS 0x0A
#define REG_CMD_SOC 0x2C
#define REG_CMD_AVG_CURRENT 0x14
#define REG_CMD_INS_CURRENT 0x30
#define REG_CMD_APPSTATUS 0x6A

#define SUB_CMD_NULL 0x0000
#define SUB_CMD_OCV_CMD 0x000C
#define SUB_CMD_BAT_INSERT 0x000D
#define SUB_CMD_IT_ENABLE 0x0021
#define SUB_CMD_CHOOSE_A 0x0024
#define SUB_CMD_CHOOSE_B 0x0025
#define SUB_CMD_RESET 0x0041

#define FC_MASK 0x0200
#define SYSDOWN_MASK 0x2
#define LU_PROF_MASK 0x1
#define INIT_COMP_MASK 0x80

#define RETRY_MAX 5
#define FAKE_CAPACITY_BATT_ALIEN 50

#define READ_FC_TIMER 10
#define OCV_CMD_TIMER 2

#define A_TEMP_COEF_DEFINE 2731

#define BITMASK_16 0xffff

#define USB_CHG  0x01
#define WALL_CHG 0x02

/* #define DEBUG_FS */

#ifdef DEBUG_FS
struct override_value {
	u8 active;
	int value;
};
#endif

struct bq27520_data {
	struct power_supply bat_ps;
	struct i2c_client *clientp;
	int curr_mv;
	int curr_capacity;
	int curr_capacity_level;
	int curr_current;
	struct work_struct ext_pwr_change_work;
	struct work_struct soc_int_work;
	struct work_struct init_work;
	struct delayed_work fc_work;
	struct delayed_work ocv_cmd_work;
	struct workqueue_struct *wq;
	int current_avg;
	int impedance;
	int flags;
	int technology;
	int bat_temp;
	int control_status;
	int app_status;
	int chg_connected;
	struct mutex lock;
	int got_technology;
	int lipo_bat_max_volt;
	int lipo_bat_min_volt;
	unsigned char capacity_scaling[2];
	char *battery_dev_name;
	char *set_batt_charged_dev_name;
	int started_worker;
	int polling_lower_capacity;
	int polling_upper_capacity;
	u8 charging_during_boot;
	struct bq27520_block_table *udatap;

#ifdef DEBUG_FS
	struct override_value bat_volt_debug;
	struct override_value bat_curr_debug;
	struct override_value bat_cap_debug;
	struct override_value bat_cap_lvl_debug;
#endif
};

static atomic_t bq27520_init_ok = ATOMIC_INIT(0);

static int get_supplier_data(struct device *dev, void *data);

#ifdef DEBUG_FS
static int read_sysfs_interface(const char *pbuf, s32 *pvalue, u8 base)
{
	long long val;
	int rc;

	rc = strict_strtoll(pbuf, base, &val);
	if (!rc)
		*pvalue = (s32)val;

	return rc;
}

static ssize_t store_voltage(struct device *pdev, struct device_attribute *attr,
			     const char *pbuf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(pdev);
	struct bq27520_data *bd =
		container_of(psy, struct bq27520_data, bat_ps);
	int rc = count;
	s32 mv;

	if (!read_sysfs_interface(pbuf, &mv, 10) &&
	    mv >= -1 && mv <= INT_MAX) {
		mutex_lock(&bd->lock);

		bd->bat_volt_debug.active = 0;

		if (mv >= 0) {
			bd->bat_volt_debug.active = 1;
			bd->bat_volt_debug.value = mv;
		}

		mutex_unlock(&bd->lock);

		power_supply_changed(&bd->bat_ps);
	} else {
		pr_err("%s: Wrong input to sysfs set_voltage. "
		       "Expect [-1..%d]. -1 releases the debug value\n",
		       BQ27520_NAME, INT_MAX);
		rc = -EINVAL;
	}

	return rc;
}
static ssize_t store_current(struct device *pdev, struct device_attribute *attr,
			     const char *pbuf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(pdev);
	struct bq27520_data *bd =
		container_of(psy, struct bq27520_data, bat_ps);
	int rc = count;
	s32 curr;

	if (!read_sysfs_interface(pbuf, &curr, 10) &&
	    curr >= -4001 && curr <= INT_MAX) {
		mutex_lock(&bd->lock);

		bd->bat_curr_debug.active = 0;

		if (curr >= -4000) {
			bd->bat_curr_debug.active = 1;
			bd->bat_curr_debug.value = curr;
		}

		mutex_unlock(&bd->lock);

		power_supply_changed(&bd->bat_ps);
	} else {
		pr_err("%s: Wrong input to sysfs set_current. "
		       "Expect [-4001..%d]. -4001 releases the debug value\n",
		       BQ27520_NAME, INT_MAX);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t store_capacity(struct device *pdev,
			      struct device_attribute *attr, const char *pbuf,
			      size_t count)
{
	struct power_supply *psy = dev_get_drvdata(pdev);
	struct bq27520_data *bd =
		container_of(psy, struct bq27520_data, bat_ps);
	int rc = count;
	s32 cap;

	if (!read_sysfs_interface(pbuf, &cap, 10) &&
	    cap >= -1 && cap <= 100) {
		mutex_lock(&bd->lock);

		bd->bat_cap_debug.active = 0;

		if (cap >= 0) {
			bd->bat_cap_debug.active = 1;
			bd->bat_cap_debug.value = cap;
		}

		mutex_unlock(&bd->lock);

		power_supply_changed(&bd->bat_ps);
	} else {
		pr_err("%s: Wrong input to sysfs set_capacity. "
		       "Expect [-1..100]. -1 releases the debug value\n",
		       BQ27520_NAME);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t store_capacity_level(struct device *pdev,
			      struct device_attribute *attr, const char *pbuf,
			      size_t count)
{
	struct power_supply *psy = dev_get_drvdata(pdev);
	struct bq27520_data *bd =
		container_of(psy, struct bq27520_data, bat_ps);
	int rc = count;
	s32 lvl;

	if (!read_sysfs_interface(pbuf, &lvl, 10) &&
	    lvl >= -1 && lvl <= POWER_SUPPLY_CAPACITY_LEVEL_FULL) {
		mutex_lock(&bd->lock);

		bd->bat_cap_lvl_debug.active = 0;

		if (lvl >= 0) {
			bd->bat_cap_lvl_debug.active = 1;
			bd->bat_cap_lvl_debug.value = lvl;
		}

		mutex_unlock(&bd->lock);

		power_supply_changed(&bd->bat_ps);
	} else {
		pr_err("%s: Wrong input to sysfs set_capacity_level. "
		       "Expect [-1..%u]. -1 releases the debug value\n",
		       BQ27520_NAME, POWER_SUPPLY_CAPACITY_LEVEL_FULL);
		rc = -EINVAL;
	}

	return rc;
}

static struct device_attribute debug_attrs[] = {
	__ATTR(set_voltage,  0200, NULL, store_voltage),
	__ATTR(set_current,  0200, NULL, store_current),
	__ATTR(set_capacity, 0200, NULL, store_capacity),
	__ATTR(set_capacity_level, 0200, NULL, store_capacity_level),
};

static int debug_create_attrs(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(debug_attrs); i++)
		if (device_create_file(dev, &debug_attrs[i]))
			goto debug_create_attrs_failed;

	return 0;

debug_create_attrs_failed:
	pr_err("%s: Failed creating debug attrs.\n", BQ27520_NAME);
	while (i--)
		device_remove_file(dev, &debug_attrs[i]);

	return -EIO;
}

static void debug_remove_attrs(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(debug_attrs); i++)
		(void)device_remove_file(dev, &debug_attrs[i]);
}
#endif /* DEBUG_FS */

static short conv_short(int v)
{
	return (short)v;
}

static int bq27520_read_bat_voltage(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_word_data(bd->clientp, REG_CMD_VOLTAGE);
	if (rc < 0)
		return rc;
	bd->curr_mv = rc;
	pr_debug("%s: %s() rc=%d\n", BQ27520_NAME, __func__, bd->curr_mv);
	return 0;
}

static int bq27520_read_bat_capacity(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_byte_data(bd->clientp, REG_CMD_SOC);
	if (rc < 0)
		return rc;
	bd->curr_capacity = rc;
	pr_debug("%s: %s() rc=%d\n", BQ27520_NAME, __func__,
						bd->curr_capacity);
	return 0;
}

static int bq27520_read_bat_current(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_word_data(bd->clientp, REG_CMD_INS_CURRENT);
	if (rc < 0)
		return rc;
	bd->curr_current = (int)conv_short(rc);
	pr_debug("%s: %s() rc=%d\n", BQ27520_NAME, __func__,
						bd->curr_current);
	return 0;
}

static int bq27520_read_bat_current_avg(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_word_data(bd->clientp, REG_CMD_AVG_CURRENT);
	if (rc < 0)
		return rc;
	bd->current_avg = (int)conv_short(rc);
	pr_debug("%s: %s() rc=%d\n", BQ27520_NAME, __func__,
						bd->current_avg);
	return 0;
}

static int bq27520_read_bat_flags(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_word_data(bd->clientp, REG_CMD_FLAGS);
	if (rc < 0)
		return rc;
	bd->flags = rc;
	pr_debug("%s: %s() rc=0x%x\n", BQ27520_NAME, __func__, bd->flags);
	return 0;
}

static int bq27520_read_app_status(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_byte_data(bd->clientp, REG_CMD_APPSTATUS);
	if (rc < 0)
		return rc;
	bd->app_status = rc;
	pr_debug("%s: %s() rc=0x%x\n", BQ27520_NAME, __func__,
						bd->app_status);
	return 0;
}

static int bq27520_read_control_status(struct power_supply *bat_ps)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_read_word_data(bd->clientp, REG_CMD_CONTROL);
	if (rc < 0)
		return rc;
	bd->control_status = rc;
	pr_debug("%s: %s() rc=0x%x\n", BQ27520_NAME, __func__,
						bd->control_status);
	return 0;
}

static int bq27520_write_control(struct power_supply *bat_ps,
								int subcmd)
{
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_write_word_data(bd->clientp, REG_CMD_CONTROL,
							(u16)subcmd);
	pr_debug("%s: %s() subcmd=0x%x rc=%d\n", BQ27520_NAME, __func__,
						subcmd, rc);
	return (int)rc;
}

static int bq27520_write_temperature(struct power_supply *bat_ps,
								int temp)
{
	int k = (temp<<3) + (temp<<1) + A_TEMP_COEF_DEFINE;
	s32 rc;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	rc = i2c_smbus_write_word_data(bd->clientp, REG_CMD_TEMPERATURE,
								(u16)k);
	pr_debug("%s: %s() k=%d rc=%d\n", BQ27520_NAME, __func__, k, rc);
	return (int)rc;
}

static int bq27520_check_initialization_comp(struct bq27520_data *bd)
{
	int i;

	for (i = 0; i < RETRY_MAX; i++) {
		msleep(1000);
		bq27520_write_control(&bd->bat_ps, SUB_CMD_NULL);
		usleep(100);
		if (!bq27520_read_control_status(&bd->bat_ps) &&
			(bd->control_status & INIT_COMP_MASK))
			return 0;
	}
	return -ETIME;
}

static int bq27520_battery_info_setting(struct bq27520_data *bd,
							int type, int temp)
{
	int rc;
	int subcmd = 0;

	rc = bq27520_read_app_status(&bd->bat_ps);
	if (rc)
		return rc;
	pr_debug("%s: %s() type=%d temp=%d status=%d\n",
		BQ27520_NAME, __func__, type, temp, bd->app_status);

	bq27520_write_control(&bd->bat_ps, SUB_CMD_BAT_INSERT);
	rc = bq27520_check_initialization_comp(bd);

	if (rc || !bd->got_technology)
		return -EINVAL;

	if ((bd->app_status & LU_PROF_MASK) &&
		type == POWER_SUPPLY_TECHNOLOGY_LIPO)
		subcmd = SUB_CMD_CHOOSE_A;
	else if (!(bd->app_status & LU_PROF_MASK) &&
		type == POWER_SUPPLY_TECHNOLOGY_LiMn)
		subcmd = SUB_CMD_CHOOSE_B;
	else if (type == POWER_SUPPLY_TECHNOLOGY_UNKNOWN)
		return -EINVAL;

	if (subcmd) {
		bq27520_write_control(&bd->bat_ps, subcmd);
		msleep(1000);

		bq27520_write_control(&bd->bat_ps, SUB_CMD_BAT_INSERT);
		rc = bq27520_check_initialization_comp(bd);
	}

	if (!rc) {
		bq27520_write_control(&bd->bat_ps, SUB_CMD_IT_ENABLE);
		bq27520_write_temperature(&bd->bat_ps, temp);
	}
	return rc;
}

static int bq27520_block_data_update(struct bq27520_data *bd)
{
	int i;
	int rc;

	for (i = 0; i < BQ27520_BTBL_MAX; i++) {
		rc = i2c_smbus_write_byte_data(bd->clientp,
			bd->udatap[i].adr,
			bd->udatap[i].data);
		if (rc < 0) {
			pr_err("%s: %s() rc=0x%x adr=0x%x\n",
				BQ27520_NAME, __func__,
				rc, bd->udatap[i].adr);
			return rc;
		}
		msleep(1);
	}

	msleep(100);
	rc = i2c_smbus_write_word_data(bd->clientp,
		REG_CMD_CONTROL, SUB_CMD_RESET);
	if (rc < 0)
		pr_err("%s: %s() rc=0x%x adr=0x%x\n",
			BQ27520_NAME, __func__,
			rc, REG_CMD_CONTROL);
	msleep(1000);
	return rc;
}

static void bq27520_init_worker(struct work_struct *work)
{
	struct bq27520_data *bd =
		container_of(work, struct bq27520_data, init_work);
	struct power_supply *ps;
	int i;

	if (bd->battery_dev_name) {
		for (i = 0; i < RETRY_MAX; i++) {
			ps = power_supply_get_by_name(bd->battery_dev_name);
			if (ps) {
				if (bd->udatap)
					bq27520_block_data_update(bd);
				get_supplier_data(ps->dev, &bd->bat_ps);
				bq27520_battery_info_setting(bd,
						bd->technology, bd->bat_temp);
				break;
			}
			msleep(1000);
		}
	}
	msleep(1000);
	bq27520_read_bat_capacity(&bd->bat_ps);
	bq27520_read_bat_current_avg(&bd->bat_ps);
	if (bd->current_avg > 0)
		bd->charging_during_boot = 1;
	pr_info("%s: %s() capacity=%d\n", BQ27520_NAME, __func__,
						bd->curr_capacity);
	atomic_set(&bq27520_init_ok, 1);
}

static int bq27520_bat_get_property(struct power_supply *bat_ps,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	int rc = 0;
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	if (!atomic_read(&bq27520_init_ok))
		return -EBUSY;

	mutex_lock(&bd->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = bq27520_read_bat_voltage(bat_ps);
		if (rc)
			break;
		val->intval = bd->curr_mv * 1000;
#ifdef DEBUG_FS
		if (bd->bat_volt_debug.active)
			val->intval = bd->bat_volt_debug.value * 1000;
#endif
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = bd->lipo_bat_max_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = bd->lipo_bat_min_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bd->capacity_scaling[0] == bd->capacity_scaling[1]) {
			val->intval = bd->curr_capacity;
		} else {
			val->intval = min(100,
				  (bd->curr_capacity * bd->capacity_scaling[0] +
				   (bd->capacity_scaling[1] >> 1)) /
					  bd->capacity_scaling[1]);
			pr_debug("%s: Report scaled cap %d (origin %d)\n",
				 BQ27520_NAME, val->intval, bd->curr_capacity);
		}
#ifdef DEBUG_FS
		if (bd->bat_cap_debug.active)
			val->intval = bd->bat_cap_debug.value;
#endif
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = bd->curr_capacity_level;
#ifdef DEBUG_FS
		if (bd->bat_cap_lvl_debug.active)
			val->intval = bd->bat_cap_lvl_debug.value;
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = bq27520_read_bat_current(bat_ps);
		if (rc)
			break;
		val->intval = bd->curr_current * 1000;
#ifdef DEBUG_FS
		if (bd->bat_curr_debug.active)
			val->intval = bd->bat_curr_debug.value * 1000;
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		rc = bq27520_read_bat_current_avg(bat_ps);
		if (rc)
			break;
		val->intval = bd->current_avg * 1000;
#ifdef DEBUG_FS
		if (bd->bat_curr_debug.active)
			val->intval = bd->bat_curr_debug.value * 1000;
#endif
		break;
	default:
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&bd->lock);
	return rc;
}

static void start_read_fc(struct bq27520_data *bd)
{
	pr_debug("%s: %s()\n", BQ27520_NAME, __func__);
	queue_delayed_work(bd->wq, &bd->fc_work, 0);
}

static void stop_read_fc(struct bq27520_data *bd)
{
	pr_debug("%s: %s()\n", BQ27520_NAME, __func__);
	if (delayed_work_pending(&bd->fc_work))
		cancel_delayed_work_sync(&bd->fc_work);
}

static void bq27520_read_fc_worker(struct work_struct *work)
{
	int rc;
	struct delayed_work *dwork =
		container_of(work, struct delayed_work, work);
	struct bq27520_data *bd =
		container_of(dwork, struct bq27520_data, fc_work);

	mutex_lock(&bd->lock);
	rc = bq27520_read_bat_flags(&bd->bat_ps);
	mutex_unlock(&bd->lock);

	pr_debug("%s: %s() capacity=%d flags=0x%x\n", BQ27520_NAME, __func__,
		 bd->curr_capacity, bd->flags);

	if (!rc) {
		u8 changed = 0;

		mutex_lock(&bd->lock);
		if (bd->flags & FC_MASK &&
		    bd->chg_connected &&
		    bd->curr_capacity_level !=
		    POWER_SUPPLY_CAPACITY_LEVEL_FULL) {
			bd->curr_capacity_level =
				POWER_SUPPLY_CAPACITY_LEVEL_FULL;
			changed = 1;
		} else if (!(bd->flags & FC_MASK) &&
			   bd->curr_capacity_level !=
			   POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN) {
			bd->curr_capacity_level =
				POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
			changed = 1;
		}
		mutex_unlock(&bd->lock);

		if (changed)
			power_supply_changed(&bd->bat_ps);
	}

	queue_delayed_work(bd->wq, &bd->fc_work, HZ * READ_FC_TIMER);
}

static void bq27520_send_ocv_cmd_worker(struct work_struct *work)
{
	int rc;
	struct delayed_work *dwork =
		container_of(work, struct delayed_work, work);
	struct bq27520_data *bd =
		container_of(dwork, struct bq27520_data, ocv_cmd_work);

	rc = bq27520_write_control(&bd->bat_ps, SUB_CMD_OCV_CMD);
	if (rc < 0)
		pr_err("%s: %s() rc=0x%x adr=0x%x\n",
			BQ27520_NAME, __func__,
			rc, REG_CMD_CONTROL);
}

static void bq27520_handle_soc_worker(struct work_struct *work)
{
	int valid_cap = 0;
	struct bq27520_data *bd =
		container_of(work, struct bq27520_data, soc_int_work);

	mutex_lock(&bd->lock);

	if (bd->got_technology &&
		bd->technology == POWER_SUPPLY_TECHNOLOGY_UNKNOWN) {
		if (!bq27520_read_bat_voltage(&bd->bat_ps)) {
			bd->curr_capacity =
			((clamp(bd->curr_mv,
			bd->lipo_bat_min_volt, bd->lipo_bat_max_volt) -
			bd->lipo_bat_min_volt) * 100) /
			(bd->lipo_bat_max_volt - bd->lipo_bat_min_volt);
			valid_cap = 1;
		}
	} else if (!bq27520_read_bat_capacity(&bd->bat_ps))
			valid_cap = 1;

	if (!bq27520_read_bat_flags(&bd->bat_ps) &&
			(bd->flags & SYSDOWN_MASK)) {
		bd->curr_capacity = 0;
		valid_cap = 1;
	} else if (valid_cap && bd->curr_capacity == 0)
		bd->curr_capacity = 1;

	mutex_unlock(&bd->lock);

	if (valid_cap) {
		power_supply_changed(&bd->bat_ps);

		mutex_lock(&bd->lock);
		if (bd->chg_connected &&
		    bd->curr_capacity >= bd->polling_lower_capacity &&
			bd->curr_capacity <= bd->polling_upper_capacity) {
			if (!bd->started_worker) {
				start_read_fc(bd);
				bd->started_worker = 1;
			}
		} else {
			if (bd->started_worker) {
				stop_read_fc(bd);
				bd->started_worker = 0;
			}
		}
		mutex_unlock(&bd->lock);
	}
	pr_info("%s: %s() capacity=%d flags=0x%x valid=%d\n",
		BQ27520_NAME, __func__,
		bd->curr_capacity, bd->flags, valid_cap);
}

static irqreturn_t bq27520_soc_thread_irq(int irq, void *data)
{
	struct bq27520_data *bd = (struct bq27520_data *)data;

	if (atomic_read(&bq27520_init_ok))
		bq27520_handle_soc_worker(&bd->soc_int_work);
	return IRQ_HANDLED;
}

static int get_supplier_data(struct device *dev, void *data)
{
	struct power_supply *psy = (struct power_supply *)data;
	struct power_supply *pst = dev_get_drvdata(dev);
	unsigned int i;
	union power_supply_propval ret;
	struct bq27520_data *bd =
		container_of(psy, struct bq27520_data, bat_ps);

	mutex_lock(&bd->lock);

	for (i = 0; i < pst->num_supplicants; i++) {
		if (strcmp(pst->supplied_to[i], psy->name))
			continue;

		if (!pst->get_property(pst, POWER_SUPPLY_PROP_TEMP, &ret)) {
			bq27520_write_temperature(psy, ret.intval);
			bd->bat_temp = ret.intval;
			pr_debug("%s: got temperature %d C\n", BQ27520_NAME,
				ret.intval);
		}

		if (!pst->get_property(pst, POWER_SUPPLY_PROP_TECHNOLOGY,
				       &ret)) {
			bd->technology = ret.intval;
			bd->got_technology = 1;
			pr_debug("%s: got technology %d\n", BQ27520_NAME,
				ret.intval);
		}
	}

	mutex_unlock(&bd->lock);

	return 0;
}

static void bq27520_ext_pwr_change_worker(struct work_struct *work)
{
	struct bq27520_data *bd =
		container_of(work, struct bq27520_data, ext_pwr_change_work);
	int chg_connected = power_supply_am_i_supplied(&bd->bat_ps);

	if (chg_connected != bd->chg_connected) {
		mutex_lock(&bd->lock);
		bd->chg_connected = chg_connected;
		mutex_unlock(&bd->lock);
		pr_debug("%s: Charger %sonnected\n", BQ27520_NAME,
			 bd->chg_connected ? "c" : "disc");
		if (!chg_connected) {
			mutex_lock(&bd->lock);
			if (bd->started_worker) {
				stop_read_fc(bd);
				bd->started_worker = 0;
			}
			bd->curr_capacity_level =
				POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
			mutex_unlock(&bd->lock);
			if (bd->charging_during_boot) {
				bd->charging_during_boot = 0;
				queue_delayed_work(bd->wq, &bd->ocv_cmd_work,
						   HZ * OCV_CMD_TIMER);
			}
		}
	}
	class_for_each_device(power_supply_class, NULL, &bd->bat_ps,
			      get_supplier_data);

}

static void bq27520_bat_external_power_changed(struct power_supply *bat_ps)
{
	struct bq27520_data *bd =
		container_of(bat_ps, struct bq27520_data, bat_ps);

	queue_work(bd->wq, &bd->ext_pwr_change_work);
}

#ifdef CONFIG_SUSPEND
static int bq27520_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27520_data *bd = i2c_get_clientdata(client);

	set_irq_wake(client->irq, 1);

	flush_workqueue(bd->wq);
	return 0;
}

static int bq27520_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	set_irq_wake(client->irq, 0);

	return 0;
}
#else
#define bq27520_pm_suspend	NULL
#define bq27520_pm_resume	NULL
#endif

static int __exit bq27520_remove(struct i2c_client *client)
{
	struct bq27520_data *bd = i2c_get_clientdata(client);

	free_irq(client->irq, 0);

	if (work_pending(&bd->soc_int_work))
		cancel_work_sync(&bd->soc_int_work);

	if (work_pending(&bd->ext_pwr_change_work))
		cancel_work_sync(&bd->ext_pwr_change_work);

	if (work_pending(&bd->init_work))
		cancel_work_sync(&bd->init_work);

	if (delayed_work_pending(&bd->fc_work))
		cancel_delayed_work_sync(&bd->fc_work);

	if (delayed_work_pending(&bd->ocv_cmd_work))
		cancel_delayed_work_sync(&bd->ocv_cmd_work);

	destroy_workqueue(bd->wq);

#ifdef DEBUG_FS
	debug_remove_attrs(bd->bat_ps.dev);
#endif
	power_supply_unregister(&bd->bat_ps);

	i2c_set_clientdata(client, NULL);

	kfree(bd);
	return 0;
}

static enum power_supply_property bq27520_bat_main_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_PRESENT
};

static const struct i2c_device_id bq27520_id[] = {
	{BQ27520_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bq27520_id);

static int bq27520_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int rc = 0;
	struct bq27520_platform_data *pdata;
	struct bq27520_data *bd;

	bd = kzalloc(sizeof(struct bq27520_data), GFP_KERNEL);
	if (!bd) {
		rc = -ENOMEM;
		goto probe_exit;
	}

	bd->bat_ps.name = BQ27520_NAME;
	bd->bat_ps.type = POWER_SUPPLY_TYPE_BATTERY;
	bd->bat_ps.properties = bq27520_bat_main_props;
	bd->bat_ps.num_properties = ARRAY_SIZE(bq27520_bat_main_props);
	bd->bat_ps.get_property = bq27520_bat_get_property;
	bd->bat_ps.external_power_changed =
		bq27520_bat_external_power_changed;
	bd->bat_ps.use_for_apm = 1;
	bd->clientp = client;

	bd->polling_lower_capacity = 95;
	bd->polling_upper_capacity = 100;
	pdata = client->dev.platform_data;
	if (pdata) {
		bd->battery_dev_name = pdata->battery_dev_name;
		bd->lipo_bat_max_volt = pdata->lipo_bat_max_volt;
		bd->lipo_bat_min_volt = pdata->lipo_bat_min_volt;
		if (pdata->capacity_scaling)
			memcpy(bd->capacity_scaling, pdata->capacity_scaling,
			       sizeof(bd->capacity_scaling));
		bd->polling_lower_capacity = pdata->polling_lower_capacity;
		bd->polling_upper_capacity = pdata->polling_upper_capacity;
		bd->udatap = pdata->udatap;

		if (pdata->supplied_to) {
			bd->bat_ps.supplied_to = pdata->supplied_to;
			bd->bat_ps.num_supplicants = pdata->num_supplicants;
		}
	}

	mutex_init(&bd->lock);

	bd->wq = create_singlethread_workqueue("batteryworker");
	if (!bd->wq) {
		pr_err("%s: Failed creating workqueue\n", BQ27520_NAME);
		rc = -EIO;
		goto probe_exit_mem_free;
	}

	INIT_WORK(&bd->init_work, bq27520_init_worker);
	INIT_WORK(&bd->ext_pwr_change_work, bq27520_ext_pwr_change_worker);
	INIT_WORK(&bd->soc_int_work, bq27520_handle_soc_worker);
	INIT_DELAYED_WORK(&bd->fc_work, bq27520_read_fc_worker);
	INIT_DELAYED_WORK(&bd->ocv_cmd_work, bq27520_send_ocv_cmd_worker);

	rc = power_supply_register(&client->dev, &bd->bat_ps);
	if (rc) {
		pr_err("%s: Failed to regist power supply\n", BQ27520_NAME);
		goto probe_exit_destroy_wq;
	}

	i2c_set_clientdata(client, bd);

	bd->got_technology = 0;
	bd->started_worker = 0;
	rc = request_threaded_irq(client->irq,
				NULL, bq27520_soc_thread_irq,
				IRQF_TRIGGER_FALLING | IRQF_DISABLED,
				BQ27520_NAME,
				bd);
	if (rc) {
		pr_err("%s: Failed requesting IRQ\n", BQ27520_NAME);
		goto probe_exit_unregister;
	}

#ifdef DEBUG_FS
	if (debug_create_attrs(bd->bat_ps.dev))
		pr_info("%s: Debug support failed\n", BQ27520_NAME);
#endif

	queue_work(bd->wq, &bd->init_work);
	return 0;

probe_exit_unregister:
	power_supply_unregister(&bd->bat_ps);
probe_exit_destroy_wq:
	destroy_workqueue(bd->wq);
probe_exit_mem_free:
	kfree(bd);
probe_exit:
	return rc;
}

static const struct dev_pm_ops bq27520_pm = {
	.suspend = bq27520_pm_suspend,
	.resume = bq27520_pm_resume,
};

static struct i2c_driver bq27520_driver = {
	.driver = {
		.name = BQ27520_NAME,
		.owner = THIS_MODULE,
		.pm = &bq27520_pm,
	},
	.probe = bq27520_probe,
	.remove = __exit_p(bq27520_remove),
	.id_table = bq27520_id,
};

static int __init bq27520_init(void)
{
	int rc;

	rc = i2c_add_driver(&bq27520_driver);
	if (rc) {
		pr_err("%s: FAILED: i2c_add_driver rc=%d\n", __func__, rc);
		goto init_exit;
	}
	return 0;

init_exit:
	return rc;
}

static void __exit bq27520_exit(void)
{
	i2c_del_driver(&bq27520_driver);
}

module_init(bq27520_init);
module_exit(bq27520_exit);

MODULE_AUTHOR("James Jacobsson, Imre Sunyi, Hiroyuki Namba");
MODULE_LICENSE("GPL");
