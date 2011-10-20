/*
 * linux/drivers/power/bq24185_charger.c
 *
 * TI BQ24185 Switch-Mode One-Cell Li-Ion charger
 *
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 *
 * Authors: James Jacobsson <james.jacobsson@sonyericsson.com>
 *          Imre Sunyi <imre.sunyi@sonyericsson.com>
 *
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
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/i2c/bq24185_charger.h>

#include <asm/mach-types.h>

/* #define DEBUG_FS */

#define REG_STATUS	0x00
#define REG_CONTROL	0x01
#define REG_BR_VOLTAGE	0x02
#define REG_VENDOR	0x03
#define REG_TERMINATION	0x04
#define REG_DPM		0x05
#define REG_SAFETY_LIM	0x06
#define REG_NTC		0x07
#define REG_BOOST	0x08

#define WATCHDOG_TIMER		10 /* HW has 12s but have 2s as margin */

#define REG_STATUS_FAULT_MASK	0x07
#define REG_STATUS_BOOST_BIT	3
#define REG_STATUS_STAT_MASK	0x30
#define REG_STATUS_EN_STAT_BIT	6
#define REG_STATUS_TMR_RST_BIT	7

#define REG_CONTROL_HZ_MODE_BIT		1
#define REG_CONTROL_CE_BIT		2
#define REG_CONTROL_TE_BIT		3
#define REG_CONTROL_IIN_LIM_MASK	0xC0

#define REG_BR_VOLTAGE_MASK	0xFC

#define REG_VENDOR_REV_MASK	0x07
#define REG_VENDOR_CODE_MASK	0xE0
#define REG_VENDOR_CODE		0x02

#define REG_TERMINATION_VITERM_MASK	0x07
#define REG_TERMINATION_VICHRG_MASK	0x78
#define REG_TERMINATION_RESET_BIT	7

#define REG_DPM_VINDPM_MASK	0x07
#define REG_DPM_LOW_CHG_BIT	5

#define REG_SAFETY_LIM_VMREG_MASK	0x0F
#define REG_SAFETY_LIM_VMCHRG_MASK	0xF0

#define REG_NTC_TS_EN_BIT	3
#define REG_NTC_TMR_MASK	0x60

#define REG_BOOST_OPA_MODE_BIT	3

/* Values in mV */
#define MIN_CHARGE_VOLTAGE	3500
#define MAX_CHARGE_VOLTAGE	4440
#define MIN_MAX_CHARGE_VOLTAGE	4200
#define MAX_MAX_CHARGE_VOLTAGE	MAX_CHARGE_VOLTAGE
#define CHARGE_VOLTAGE_STEP	20

/* Values in mA */
#define LOW_CHARGE_CURRENT(x)	((350 * BQ24185_RSENS_REF) / (x))
#define MIN_CHARGE_CURRENT(x)	((550 * BQ24185_RSENS_REF) / (x))
#define MAX_CHARGE_CURRENT(x)	((1550 * BQ24185_RSENS_REF) / (x))
#define CHARGE_CURRENT_STEP(x)	((100 * BQ24185_RSENS_REF) / (x))

/* Values in mA */
#define MIN_CHARGE_TERM_CURRENT(x)	((25 * BQ24185_RSENS_REF) / (x))
#define MAX_CHARGE_TERM_CURRENT(x)	((200 * BQ24185_RSENS_REF) / (x))
#define CHARGE_TERM_CURRENT_STEP(x)	((25 * BQ24185_RSENS_REF) / (x))

#define SET_BIT(bit, val, data) ((val << bit) | ((data) & ~(1 << bit)))
#define CHK_BIT(bit, data) (((data) & (1 << bit)) >> bit)
#define SET_MASK(mask, val, data) (((data) & ~(mask)) | (val))
#define CHK_MASK(mask, data) ((data) & (mask))
#define DATA_MASK(mask, data) ((data) << (ffs(mask) - 1))

#ifdef DEBUG
#define MUTEX_LOCK(x) do {						\
	pr_debug("%s: Locking mutex in %s\n", BQ24185_NAME, __func__);	\
	mutex_lock(x);							\
} while (0)
#define MUTEX_UNLOCK(x) do {						\
	pr_debug("%s: Unlocking mutex in %s\n", BQ24185_NAME, __func__);\
	mutex_unlock(x);						\
} while (0)
#else
#define MUTEX_LOCK(x) mutex_lock(x)
#define MUTEX_UNLOCK(x) mutex_unlock(x)
#endif /* DEBUG */

enum bq24185_status {
	STAT_READY,
	STAT_CHARGE_IN_PROGRESS,
	STAT_CHARGE_DONE,
	STAT_FAULT,
};

enum bq24185_fault {
	FAULT_NORMAL,
	FAULT_VBUS_OVP,
	FAULT_SLEEP_MODE,
	FAULT_FAULTY_ADAPTER,
	FAULT_DCOUT_LIMIT,
	FAULT_THERMAL_SHUTDOWN,
	FAULT_TIMER_FAULT,
	FAULT_NO_BATTERY,
};

enum bq24185_iin_lim {
	IIN_LIM_100MA,
	IIN_LIM_500MA,
	IIN_LIM_800MA,
	IIN_LIM_NO,
};

enum bq24185_vindpm {
	VINDPM_4150MV,
	VINDPM_4230MV,
	VINDPM_4310MV,
	VINDPM_4390MV,
	VINDPM_4470MV,
	VINDPM_4550MV,
	VINDPM_4630MV,
	VINDPM_4710MV,
};

enum bq247185_tmr {
	TMR_27MIN,
	TMR_3H,
	TMR_6H,
	TMR_OFF,
};

struct bq24185_status_data {
	enum bq24185_status stat;
	enum bq24185_fault fault;
};

static atomic_t bq24185_init_ok = ATOMIC_INIT(0);

struct bq24185_data {
	struct power_supply bat_ps;
	struct i2c_client *clientp;
	struct delayed_work work;
	struct workqueue_struct *wq;
	struct bq24185_status_data cached_status;
	struct mutex lock;
	struct wake_lock wake_lock;
	struct bq24185_platform_data *control;

	int ext_status;
	int chg_status;
	int bat_present;
	u8 rsens;
	u8 watchdog_enable_vote;
	u8 hz_disable_vote;
	u8 irq_wake_enabled;
	u8 chg_disable_vote;
	u8 usb_compliant_mode;
	u8 boot_initiated_charging;
	u16 charging_safety_timer;
	u8 chg_disabled_by_voltage;
	u8 chg_disabled_by_current;
	u8 chg_disabled_by_input_current;

	enum bq24185_opa_mode mode;
};

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

static ssize_t store_power_charger(struct device *pdev,
				   struct device_attribute *attr,
				   const char *pbuf,
				   size_t count)
{
	int rc = count;
	s32 onoff;

	if (!read_sysfs_interface(pbuf, &onoff, 10) &&
	    onoff >= 0 && onoff <= 2) {
		int ret;

		if (!onoff)
			ret = bq24185_turn_off_charger();
		else
			ret = bq24185_turn_on_charger(onoff - 1);

		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..2] where:\n0: Turn off charger\n"
		       "1: Turn on charger in non USB compatible mode\n"
		       "2: Turn on charger in USB compatible mode\n",
		       BQ24185_NAME);
		rc = -EINVAL;
	}

	return rc;
}
static ssize_t store_opa(struct device *pdev,
			 struct device_attribute *attr,
			 const char *pbuf,
			 size_t count)
{
	int rc = count;
	s32 opa;

	if (!read_sysfs_interface(pbuf, &opa, 10) &&
	    opa >= 0 && opa <= CHARGER_BOOST_MODE) {
		int ret = bq24185_set_opa_mode(opa);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..1] where:\n0: Charger mode\n"
		       "1: Boost mode\n", BQ24185_NAME);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t store_chg_volt(struct device *pdev,
			      struct device_attribute *attr,
			      const char *pbuf,
			      size_t count)
{
	int rc = count;
	s32 mv;

	if (!read_sysfs_interface(pbuf, &mv, 10) &&
	    mv >= 0 && mv <= USHORT_MAX) {
		int ret = bq24185_set_charger_voltage(mv);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..%u] mV\n", BQ24185_NAME, USHORT_MAX);
		rc = -EINVAL;
	}

	return rc;
}
static ssize_t store_chg_curr(struct device *pdev,
			      struct device_attribute *attr,
			      const char *pbuf,
			      size_t count)
{
	int rc = count;
	s32 ma;

	if (!read_sysfs_interface(pbuf, &ma, 10) &&
	    ma >= 0 && ma <= USHORT_MAX) {
		int ret = bq24185_set_charger_current(ma);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..%u] mA\n", BQ24185_NAME, USHORT_MAX);
		rc = -EINVAL;
	}

	return rc;
}
static ssize_t store_chg_curr_term(struct device *pdev,
				   struct device_attribute *attr,
				   const char *pbuf,
				   size_t count)
{
	int rc = count;
	s32 ma;

	if (!read_sysfs_interface(pbuf, &ma, 10) &&
	    ma >= 0 && ma <= USHORT_MAX) {
		int ret = bq24185_set_charger_termination_current(ma);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..%u] mA\n", BQ24185_NAME, USHORT_MAX);
		rc = -EINVAL;
	}

	return rc;
}
static ssize_t store_input_curr_lim(struct device *pdev,
				    struct device_attribute *attr,
				    const char *pbuf,
				    size_t count)
{
	int rc = count;
	s32 ma;

	if (!read_sysfs_interface(pbuf, &ma, 10) &&
	    ma >= 0 && ma <= USHORT_MAX) {
		int ret = bq24185_set_input_current_limit(ma);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..%u] mA\n", BQ24185_NAME, USHORT_MAX);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t store_safety_timer(struct device *pdev,
				  struct device_attribute *attr,
				  const char *pbuf,
				  size_t count)
{
	int rc = count;
	s32 time;

	if (!read_sysfs_interface(pbuf, &time, 10) &&
	    time >= 0 && time <= USHORT_MAX) {
		int ret = bq24185_set_charger_safety_timer(time);
		if (ret < 0)
			rc = ret;
	} else {
		pr_err("%s: Wrong input to sysfs. "
		       "Expect [0..%u] minutes\n", BQ24185_NAME, USHORT_MAX);
		rc = -EINVAL;
	}

	return rc;
}

static struct device_attribute debug_attrs[] = {
	__ATTR(set_power_charger,	0200, NULL, store_power_charger),
	__ATTR(set_opa,			0200, NULL, store_opa),
	__ATTR(set_chg_volt,		0200, NULL, store_chg_volt),
	__ATTR(set_chg_curr,		0200, NULL, store_chg_curr),
	__ATTR(set_chg_curr_term,	0200, NULL, store_chg_curr_term),
	__ATTR(set_input_curr_lim,	0200, NULL, store_input_curr_lim),
	__ATTR(set_safety_timer,	0200, NULL, store_safety_timer),
};

static int debug_create_attrs(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(debug_attrs); i++)
		if (device_create_file(dev, &debug_attrs[i]))
			goto debug_create_attrs_failed;

	return 0;

debug_create_attrs_failed:
	pr_err("%s: Failed creating semc battery attrs.\n", BQ24185_NAME);
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

#ifdef DEBUG
static void bq24185_dump_registers(struct bq24185_data *bd)
{
	u8 i;
	s32 data[9];

	for (i = 0; i < 9; i++) {
		data[i] = i2c_smbus_read_byte_data(bd->clientp, i);
		if (data[i] < 0)
			pr_err("%s: Failed dumping reg %u\n", BQ24185_NAME, i);
	}

	pr_debug("%s: Regdump\n\t0: 0x%.2x, 1: 0x%.2x, 2: 0x%.2x, 3: 0x%.2x, "
		 "4: 0x%.2x, 5: 0x%.2x, 6: 0x%.2x, 7: 0x%.2x, 8: 0x%.2x\n",
		 BQ24185_NAME, data[0], data[1], data[2], data[3], data[4],
		 data[5], data[6], data[7], data[8]);
}
#endif

static int bq24185_enable_charger(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (bd->chg_disable_vote && --bd->chg_disable_vote) {
		MUTEX_UNLOCK(&bd->lock);
		return 0;
	}
	MUTEX_UNLOCK(&bd->lock);

	pr_info("%s: Enabling charger\n", BQ24185_NAME);

	return i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL,
		 SET_BIT(REG_CONTROL_CE_BIT, 0,
			 i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL)));
}

static int bq24185_disable_charger(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (bd->chg_disable_vote++) {
		MUTEX_UNLOCK(&bd->lock);
		return 0;
	}
	MUTEX_UNLOCK(&bd->lock);

	pr_info("%s: Disabling charger\n", BQ24185_NAME);

	return i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL,
		 SET_BIT(REG_CONTROL_CE_BIT, 1,
			 i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL)));
}

static int bq24185_reset_charger(struct bq24185_data *bd)
{
	s32 data = i2c_smbus_read_byte_data(bd->clientp, REG_TERMINATION);

	if (data < 0) {
		pr_err("%s: Failed resetting charger (getting reg)\n",
		       BQ24185_NAME);
		return (int)data;
	}

	data = SET_BIT(REG_TERMINATION_RESET_BIT, 1, data);
	data = i2c_smbus_write_byte_data(bd->clientp, REG_TERMINATION, data);
	if (data < 0)
		pr_err("%s: Failed resetting charger (setting reg)\n",
		       BQ24185_NAME);

	return (int)data;
}

static int bq24185_check_status(struct bq24185_data *bd)
{
	s32 status;
	const char *pzstat[] = {
		"Ready", "Charge in progress", "Charge done", "Fault" };
	const char *pzfault[] = {
		"Normal", "VBUS OVP", "Sleep mode",
		"Fault adapter or VBUS<Vuvlo", "DCOUT Current limit tripped",
		"Thermal shutdown or TS Fault",	"Timer fault", "No battery"
	};

	status = i2c_smbus_read_byte_data(bd->clientp, REG_STATUS);
	if (status < 0) {
		pr_err("%s: Failed checking status. Errno = %d\n",
		       BQ24185_NAME, status);
		return (int)status;
	}

	MUTEX_LOCK(&bd->lock);
	bd->cached_status.stat = (status & REG_STATUS_STAT_MASK) >>
		(ffs(REG_STATUS_STAT_MASK) - 1);
	bd->cached_status.fault = (status & REG_STATUS_FAULT_MASK) >>
		(ffs(REG_STATUS_FAULT_MASK) - 1);
	if ((CHARGER_BOOST_MODE == bd->mode) &&
		(CHARGER_BOOST_MODE !=
		CHK_BIT(REG_STATUS_BOOST_BIT, status))) {
		pr_debug("%s: Detect vbus_drop\n", BQ24185_NAME);
		if (bd->control && bd->control->notify_vbus_drop)
			bd->control->notify_vbus_drop();
	}
	bd->mode = CHK_BIT(REG_STATUS_BOOST_BIT, status);
	MUTEX_UNLOCK(&bd->lock);

	pr_debug("--[ Status of %s ]--\n", BQ24185_NAME);
	pr_debug(" Status/Control:\n   PSEL=%s\n   EN_STAT=%s\n"
		 " STAT=%s\n   BOOST=%s\n   FAULT=%s\n",
		 status & (1 << 7) ? "high" : "low",
		 status & (1 << 6) ? "Enabled" : "Disabled",
		 pzstat[bd->cached_status.stat],
		 status & (1 << 3) ? "Boost" : "Charger",
		 pzfault[bd->cached_status.fault]);

	return 0;
}

static void bq24185_update_power_supply(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (bd->ext_status < 0) {
		switch (bd->cached_status.stat) {
		case STAT_READY:
			bd->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case STAT_CHARGE_IN_PROGRESS:
			bd->chg_status = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case STAT_CHARGE_DONE:
			bd->chg_status = POWER_SUPPLY_STATUS_FULL;
			break;
		case STAT_FAULT:
		default:
			bd->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		}
	} else {
		bd->chg_status = bd->ext_status;
	}

	switch (bd->cached_status.fault) {
	case FAULT_NO_BATTERY:
		bd->bat_present = 0;
		break;
	default:
		bd->bat_present = 1;
		break;
	}
	MUTEX_UNLOCK(&bd->lock);

	power_supply_changed(&bd->bat_ps);
}

static irqreturn_t bq24185_thread_irq(int irq, void *data)
{
	struct bq24185_data *bd = data;
	struct bq24185_status_data old_status = bd->cached_status;

	pr_debug("%s: Receiving threaded interrupt\n", BQ24185_NAME);
	/* Delay the interrupt handling since STATx in register '0' is not
	 * always updated when receiving this.
	 * 300 ms according to TI.
	 */
	msleep(300);

	if (!bq24185_check_status(bd) &&
	    memcmp(&bd->cached_status, &old_status, sizeof bd->cached_status)) {
		bq24185_update_power_supply(bd);
	}

	return IRQ_HANDLED;
}

static void bq24185_hz_enable(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (bd->hz_disable_vote)
		bd->hz_disable_vote--;
	MUTEX_UNLOCK(&bd->lock);

	if (!bd->hz_disable_vote) {
		s32 data = i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL);
		data = SET_BIT(REG_CONTROL_HZ_MODE_BIT, 1, data);
		(void)i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL, data);
	}

	return;
}

static void bq24185_hz_disable(struct bq24185_data *bd)
{
	s32 data;

	MUTEX_LOCK(&bd->lock);
	if (bd->hz_disable_vote++) {
		MUTEX_UNLOCK(&bd->lock);
		return;
	}
	MUTEX_UNLOCK(&bd->lock);

	data = i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL);
	data = SET_BIT(REG_CONTROL_HZ_MODE_BIT, 0, data);
	(void)i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL, data);

	return;
}

static void bq24185_start_watchdog_reset(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (bd->watchdog_enable_vote++) {
		MUTEX_UNLOCK(&bd->lock);
		return;
	}
	MUTEX_UNLOCK(&bd->lock);

	wake_lock(&bd->wake_lock);

	(void)queue_delayed_work(bd->wq, &bd->work, 0);
}

static void bq24185_stop_watchdog_reset(struct bq24185_data *bd)
{
	MUTEX_LOCK(&bd->lock);
	if (!bd->watchdog_enable_vote || --bd->watchdog_enable_vote) {
		MUTEX_UNLOCK(&bd->lock);
		return;
	}

	/* Clear the 'disabled_by_xxx' flags.
	 * If asic HW watchdog has been timed out, as it can from this
	 * function, then asic will be resetted to default mode.
	 * In default mode it will start charging as soon as VBUS goes high
	 * again. Clearing these flags will allow this driver to set initial
	 * settings that will put asic in non-charging mode when external
	 * turns on the charger again.
	 */
	bd->chg_disabled_by_voltage = 0;
	bd->chg_disabled_by_current = 0;
	bd->chg_disabled_by_input_current = 0;
	bd->chg_disable_vote = 0;
	MUTEX_UNLOCK(&bd->lock);

	cancel_delayed_work(&bd->work);

	wake_unlock(&bd->wake_lock);
}

static void bq24185_reset_watchdog_worker(struct work_struct *work)
{
	struct delayed_work *dwork =
		container_of(work, struct delayed_work, work);
	struct bq24185_data *bd =
		container_of(dwork, struct bq24185_data, work);
	s32 data = i2c_smbus_read_byte_data(bd->clientp, REG_STATUS);

	if (data >= 0) {
		data = SET_BIT(REG_STATUS_TMR_RST_BIT, 1, data);
		if (i2c_smbus_write_byte_data(bd->clientp, REG_STATUS, data))
			pr_err("%s: Watchdog reset failed\n", BQ24185_NAME);
	}

	/* bq24185_check_status(bd); */
#ifdef DEBUG
	bq24185_dump_registers(bd);
#endif

	(void)queue_delayed_work(bd->wq, &bd->work, HZ * WATCHDOG_TIMER);
}

static int bq24185_bat_get_property(struct power_supply *bat_ps,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct bq24185_data *bd =
		container_of(bat_ps, struct bq24185_data, bat_ps);

	MUTEX_LOCK(&bd->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bd->chg_status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bd->bat_present;
		break;
	default:
		MUTEX_UNLOCK(&bd->lock);
		return -EINVAL;
	}
	MUTEX_UNLOCK(&bd->lock);
	return 0;
}

static void bq24185_set_init_values(struct bq24185_data *bd)
{
	s32 data;

	pr_info("%s: Set init values\n", BQ24185_NAME);

	/* Enable status interrupts */
	data = i2c_smbus_read_byte_data(bd->clientp, REG_STATUS);
	data = SET_BIT(REG_STATUS_EN_STAT_BIT, 1, data);
	(void)i2c_smbus_write_byte_data(bd->clientp, REG_STATUS, data);

	if (bd->boot_initiated_charging)
		return;

	/* Sets any charging relates registers to 'off' */
	(void)bq24185_set_charger_voltage(0);
	(void)bq24185_set_charger_current(0);
	(void)bq24185_set_charger_termination_current(0);
	(void)bq24185_set_charger_safety_timer(0);

	data = i2c_smbus_read_byte_data(bd->clientp, REG_DPM);
	if (bd->usb_compliant_mode)
		data = SET_MASK(REG_DPM_VINDPM_MASK,
				DATA_MASK(REG_DPM_VINDPM_MASK, VINDPM_4550MV),
				data);
	else
		data = SET_MASK(REG_DPM_VINDPM_MASK,
				DATA_MASK(REG_DPM_VINDPM_MASK, VINDPM_4470MV),
				data);

	(void)i2c_smbus_write_byte_data(bd->clientp, REG_DPM, data);
}

int bq24185_turn_on_charger(u8 usb_compliant)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	pr_info("%s: Turning on charger. USB-%s mode\n", BQ24185_NAME,
		usb_compliant ? "Host" : "Dedicated");

	/* Need to start watchdog reset otherwise HW will reset itself.
	 * If boot has triggered charging the watchdog resetter is already
	 * started.
	 */
	if (!bd->boot_initiated_charging)
		bq24185_start_watchdog_reset(bd);
	else
		bd->boot_initiated_charging = 0;

	bd->usb_compliant_mode = usb_compliant;

	bq24185_set_init_values(bd);
	bq24185_hz_disable(bd);

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_turn_on_charger);

int bq24185_turn_off_charger(void)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	pr_info("%s: Turning off charger\n", BQ24185_NAME);

	bq24185_stop_watchdog_reset(bd);
	bq24185_hz_enable(bd);

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_turn_off_charger);

int bq24185_set_opa_mode(enum bq24185_opa_mode mode)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	s32 data;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);
	data = i2c_smbus_read_byte_data(bd->clientp, REG_BOOST);
	if (data < 0)
		return (int)data;

	MUTEX_LOCK(&bd->lock);
	bd->mode = mode;
	MUTEX_UNLOCK(&bd->lock);

	switch (mode) {
	case CHARGER_CHARGER_MODE:
		if (!CHK_BIT(REG_BOOST_OPA_MODE_BIT, data))
			return 0;

		(void)bq24185_set_charger_safety_timer(
			bd->charging_safety_timer);
		bq24185_stop_watchdog_reset(bd);
		bq24185_hz_enable(bd);
		data = SET_BIT(REG_BOOST_OPA_MODE_BIT, 0, data);
		pr_info("%s: Disabling boost mode\n", BQ24185_NAME);
		return i2c_smbus_write_byte_data(bd->clientp, REG_BOOST, data);
	case CHARGER_BOOST_MODE:
		if (CHK_BIT(REG_BOOST_OPA_MODE_BIT, data))
			return 0;

		(void)bq24185_set_charger_safety_timer(0);
		/* HZ_MODE overrides OPA_MODE. Set no HZ_MODE */
		bq24185_hz_disable(bd);
		bq24185_start_watchdog_reset(bd);
		data = SET_BIT(REG_BOOST_OPA_MODE_BIT, 1, data);
		pr_info("%s: Enabling boost mode\n", BQ24185_NAME);
		return i2c_smbus_write_byte_data(bd->clientp, REG_BOOST, data);
	default:
		pr_err("%s: Invalid charger mode\n", BQ24185_NAME);
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(bq24185_set_opa_mode);

int bq24185_get_opa_mode(enum bq24185_opa_mode *mode)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;

	if (!mode)
		return -EPERM;

	if (!psy)
		return -EAGAIN;

	bd = container_of(psy, struct bq24185_data, bat_ps);

	MUTEX_LOCK(&bd->lock);
	*mode = bd->mode;
	MUTEX_UNLOCK(&bd->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_get_opa_mode);

int bq24185_set_charger_voltage(u16 mv)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	u8 voreg;
	s32 data;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	if (mv < MIN_CHARGE_VOLTAGE) {
		MUTEX_LOCK(&bd->lock);
		if (!bd->chg_disabled_by_voltage) {
			bd->chg_disabled_by_voltage = 1;
			MUTEX_UNLOCK(&bd->lock);
			bq24185_disable_charger(bd);
		} else {
			MUTEX_UNLOCK(&bd->lock);
		}
		return 0;
	}

	voreg = (min_t(u16, mv, MAX_CHARGE_VOLTAGE) - MIN_CHARGE_VOLTAGE) /
		CHARGE_VOLTAGE_STEP;
	pr_info("%s: Setting charger voltage to %u mV\n", BQ24185_NAME,
		MIN_CHARGE_VOLTAGE + voreg * CHARGE_VOLTAGE_STEP);
	data = i2c_smbus_read_byte_data(bd->clientp, REG_BR_VOLTAGE);
	if (CHK_MASK(REG_BR_VOLTAGE_MASK, data) !=
	    DATA_MASK(REG_BR_VOLTAGE_MASK, voreg)) {
		data = SET_MASK(REG_BR_VOLTAGE_MASK,
				DATA_MASK(REG_BR_VOLTAGE_MASK, voreg),
				data);
		(void)i2c_smbus_write_byte_data(bd->clientp, REG_BR_VOLTAGE,
						data);
	}

	MUTEX_LOCK(&bd->lock);
	if (bd->chg_disabled_by_voltage) {
		bd->chg_disabled_by_voltage = 0;
		MUTEX_UNLOCK(&bd->lock);
		bq24185_enable_charger(bd);
	} else {
		MUTEX_UNLOCK(&bd->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_set_charger_voltage);

int bq24185_set_charger_current(u16 ma)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	s32 data;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	if (ma < LOW_CHARGE_CURRENT(bd->rsens)) {
		MUTEX_LOCK(&bd->lock);
		if (!bd->chg_disabled_by_current) {
			bd->chg_disabled_by_current = 1;
			MUTEX_UNLOCK(&bd->lock);
			bq24185_disable_charger(bd);
		} else {
			MUTEX_UNLOCK(&bd->lock);
		}
		return 0;
	}

	data = i2c_smbus_read_byte_data(bd->clientp, REG_DPM);

	if (ma < MIN_CHARGE_CURRENT(bd->rsens)) {
		pr_info("%s: Setting charger current to %u mA\n",
			BQ24185_NAME, LOW_CHARGE_CURRENT(bd->rsens));
		if (!CHK_BIT(REG_DPM_LOW_CHG_BIT, data)) {
			data = SET_BIT(REG_DPM_LOW_CHG_BIT, 1, data);
			(void)i2c_smbus_write_byte_data(bd->clientp, REG_DPM,
							data);
		}
	} else {
		u8 vichrg = (min_t(u16, ma, MAX_CHARGE_CURRENT(bd->rsens)) -
			     MIN_CHARGE_CURRENT(bd->rsens)) /
			CHARGE_CURRENT_STEP(bd->rsens);
		pr_info("%s: Setting charger current to %u mA\n",
			BQ24185_NAME, MIN_CHARGE_CURRENT(bd->rsens) +
			vichrg * CHARGE_CURRENT_STEP(bd->rsens));

		if (CHK_BIT(REG_DPM_LOW_CHG_BIT, data)) {
			data = SET_BIT(REG_DPM_LOW_CHG_BIT, 0, data);
			(void)i2c_smbus_write_byte_data(bd->clientp, REG_DPM,
							data);
		}

		data = i2c_smbus_read_byte_data(bd->clientp, REG_TERMINATION);
		if (CHK_MASK(REG_TERMINATION_VICHRG_MASK, data) !=
		    DATA_MASK(REG_TERMINATION_VICHRG_MASK, vichrg)) {
			data = SET_MASK(REG_TERMINATION_VICHRG_MASK,
					DATA_MASK(REG_TERMINATION_VICHRG_MASK,
						  vichrg), data);
			(void)i2c_smbus_write_byte_data(bd->clientp,
							REG_TERMINATION, data);
		}
	}

	MUTEX_LOCK(&bd->lock);
	if (bd->chg_disabled_by_current) {
		bd->chg_disabled_by_current = 0;
		MUTEX_UNLOCK(&bd->lock);
		bq24185_enable_charger(bd);
	} else {
		MUTEX_UNLOCK(&bd->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_set_charger_current);

int bq24185_set_charger_termination_current(u16 ma)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	s32 data;
	u8 viterm;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	data = i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL);

	if (ma < MIN_CHARGE_TERM_CURRENT(bd->rsens)) {
		if (CHK_BIT(REG_CONTROL_TE_BIT, data)) {
			data = SET_BIT(REG_CONTROL_TE_BIT, 0, data);
			pr_info("%s: Disable charge current termination\n",
				BQ24185_NAME);
			(void)i2c_smbus_write_byte_data(bd->clientp,
							REG_CONTROL, data);
		}
		return 0;
	}

	if (!CHK_BIT(REG_CONTROL_TE_BIT, data)) {
		data = SET_BIT(REG_CONTROL_TE_BIT, 1, data);
		pr_info("%s: Enable charge current termination\n",
			BQ24185_NAME);
		(void)i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL, data);
	}

	viterm = (clamp_val(ma, MIN_CHARGE_TERM_CURRENT(bd->rsens),
			    MAX_CHARGE_TERM_CURRENT(bd->rsens)) -
		  MIN_CHARGE_TERM_CURRENT(bd->rsens))
		/ CHARGE_TERM_CURRENT_STEP(bd->rsens);

	data = i2c_smbus_read_byte_data(bd->clientp, REG_TERMINATION);
	if (CHK_MASK(REG_TERMINATION_VITERM_MASK, data) ==
	    DATA_MASK(REG_TERMINATION_VITERM_MASK, viterm))
		return 0;

	data = SET_MASK(REG_TERMINATION_VITERM_MASK,
			DATA_MASK(REG_TERMINATION_VITERM_MASK, viterm), data);
	pr_info("%s: Charge current termination set to %u mA\n", BQ24185_NAME,
		25 + viterm * 25);
	return i2c_smbus_write_byte_data(bd->clientp, REG_TERMINATION, data);
}
EXPORT_SYMBOL_GPL(bq24185_set_charger_termination_current);

int bq24185_set_charger_safety_timer(u16 minutes)
{
	const char *hwtime[] = {
		"27 minutes", "3 hours", "6 hours", "disable"
	};
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	enum bq247185_tmr safety_timer;
	s32 data;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	MUTEX_LOCK(&bd->lock);

	if (CHARGER_BOOST_MODE == bd->mode && minutes) {
		/* In boost mode safety timer must be disabled.
		 * Enable safety timer when leaving that mode.
		 */
		bd->charging_safety_timer = minutes;
		minutes = 0;
	} else if (CHARGER_BOOST_MODE != bd->mode) {
		bd->charging_safety_timer = minutes;
	}

	MUTEX_UNLOCK(&bd->lock);

	if (!minutes)
		safety_timer = TMR_OFF;
	else if (minutes <= 27)
		safety_timer = TMR_27MIN;
	else if (minutes <= 180)
		safety_timer = TMR_3H;
	else
		safety_timer = TMR_6H;

	data = i2c_smbus_read_byte_data(bd->clientp, REG_NTC);
	if (CHK_MASK(REG_NTC_TMR_MASK, data) ==
	    DATA_MASK(REG_NTC_TMR_MASK, safety_timer))
		return 0;

	pr_info("%s: Set safety timer to %s\n", BQ24185_NAME,
		hwtime[safety_timer]);

	data = SET_MASK(REG_NTC_TMR_MASK,
			DATA_MASK(REG_NTC_TMR_MASK, safety_timer),
			data);
	return i2c_smbus_write_byte_data(bd->clientp, REG_NTC, data);
}
EXPORT_SYMBOL_GPL(bq24185_set_charger_safety_timer);

int bq24185_set_input_current_limit(u16 ma)
{
	const char *hwlim[] = {
		"100 mA", "500 mA", "800 mA", "no limit"
	};
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;
	enum bq24185_iin_lim iin_lim;
	s32 data;

	if (!psy)
		return -EAGAIN;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	if (ma < 100) {
		MUTEX_LOCK(&bd->lock);
		if (!bd->chg_disabled_by_input_current) {
			bd->chg_disabled_by_input_current = 1;
			MUTEX_UNLOCK(&bd->lock);
			bq24185_disable_charger(bd);
		} else {
			MUTEX_UNLOCK(&bd->lock);
		}
		return 0;
	}

	if (ma < 500)
		iin_lim = IIN_LIM_100MA;
	else if (ma < 800)
		iin_lim = IIN_LIM_500MA;
	else if (ma > 800)
		iin_lim = IIN_LIM_NO;
	else
		iin_lim = IIN_LIM_800MA;

	pr_info("%s: Setting input charger current to %s\n",
		BQ24185_NAME, hwlim[iin_lim]);

	data = i2c_smbus_read_byte_data(bd->clientp, REG_CONTROL);
	if (CHK_MASK(REG_CONTROL_IIN_LIM_MASK, data) !=
	    DATA_MASK(REG_CONTROL_IIN_LIM_MASK, iin_lim)) {
		data = SET_MASK(REG_CONTROL_IIN_LIM_MASK,
				DATA_MASK(REG_CONTROL_IIN_LIM_MASK, iin_lim),
				data);
		(void)i2c_smbus_write_byte_data(bd->clientp, REG_CONTROL, data);
	}

	MUTEX_LOCK(&bd->lock);
	if (bd->chg_disabled_by_input_current) {
		bd->chg_disabled_by_input_current = 0;
		MUTEX_UNLOCK(&bd->lock);
		bq24185_enable_charger(bd);
	} else {
		MUTEX_UNLOCK(&bd->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_set_input_current_limit);

int bq24185_set_ext_charging_status(int status)
{
	struct power_supply *psy = power_supply_get_by_name(BQ24185_NAME);
	struct bq24185_data *bd;

	if (!psy)
		return -EINVAL;
	bd = container_of(psy, struct bq24185_data, bat_ps);

	MUTEX_LOCK(&bd->lock);
	bd->ext_status = status;
	MUTEX_UNLOCK(&bd->lock);

	bq24185_update_power_supply(bd);

	return 0;
}
EXPORT_SYMBOL_GPL(bq24185_set_ext_charging_status);

int bq24185_charger_initialized(void)
{
	return (int)atomic_read(&bq24185_init_ok);
}
EXPORT_SYMBOL_GPL(bq24185_charger_initialized);

static int __exit bq24185_remove(struct i2c_client *client)
{
	struct bq24185_data *bd = i2c_get_clientdata(client);

	if (bd->irq_wake_enabled)
		(void)disable_irq_wake(client->irq);

	free_irq(client->irq, bd);

	if (delayed_work_pending(&bd->work))
		cancel_delayed_work_sync(&bd->work);

	destroy_workqueue(bd->wq);

	wake_lock_destroy(&bd->wake_lock);

#ifdef DEBUG_FS
	debug_remove_attrs(bd->bat_ps.dev);
#endif
	power_supply_unregister(&bd->bat_ps);

	i2c_set_clientdata(client, NULL);

	kfree(bd);
	return 0;
}

static const struct i2c_device_id bq24185_id[] = {
	{BQ24185_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, bq24185_id);

static enum power_supply_property bq24185_bat_main_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
};

static int bq24185_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq24185_platform_data *pdata = client->dev.platform_data;
	struct bq24185_data *bd;
	s32 buf;
	int rc = 0;

	/* Make sure we have at least i2c functionality on the bus */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		pr_err("%s: No i2c functionality available\n", BQ24185_NAME);
		return -EIO;
	}

	buf = i2c_smbus_read_byte_data(client, REG_VENDOR);
	if (buf <= 0) {
		pr_err("%s: Failed read vendor info\n", BQ24185_NAME);
		return -EIO;
	}

	if (((buf & REG_VENDOR_CODE_MASK) >> 5) == REG_VENDOR_CODE) {
		pr_info("%s: Found BQ24185, rev 0x%.2x\n", BQ24185_NAME,
			buf & 7);
	} else {
		pr_err("%s: Invalid vendor code\n", BQ24185_NAME);
		return -ENODEV;
	}

	pdata = client->dev.platform_data;
	if (pdata) {
		/* Set the safety limit register. Must be done before any other
		 * registers are written to.
		 */
		buf = i2c_smbus_write_byte_data(client, REG_SAFETY_LIM,
						pdata->mbrv | pdata->mccsv);
		if (buf < 0) {
			pr_err("%s: Failed setting safety limit register\n",
			       BQ24185_NAME);
			return (int)buf;
		}

		buf = i2c_smbus_read_byte_data(client, REG_SAFETY_LIM);
		if (buf < 0) {
			pr_err("%s: Failed reading safety limit register\n",
			       BQ24185_NAME);
			return (int)buf;
		}

		if (buf != (pdata->mbrv | pdata->mccsv)) {
			if (pdata->support_boot_charging) {
				pr_warning("%s: *** Safety limit could not be "
					   "set!\n Please check boot accessing "
					   "charger (want 0x%x, got 0x%x) ***",
					   BQ24185_NAME,
					   pdata->mbrv | pdata->mccsv, buf);
			} else {
				pr_err("%s, Safety limit could not be set!\n",
				       BQ24185_NAME);
				return -EPERM;
			}
		}
	}

	bd = kzalloc(sizeof(struct bq24185_data), GFP_KERNEL);
	if (!bd)
		return -ENOMEM;

	bd->bat_ps.name = BQ24185_NAME;
	bd->bat_ps.type = POWER_SUPPLY_TYPE_BATTERY;
	bd->bat_ps.properties = bq24185_bat_main_props;
	bd->bat_ps.num_properties = ARRAY_SIZE(bq24185_bat_main_props);
	bd->bat_ps.get_property = bq24185_bat_get_property;
	bd->bat_ps.use_for_apm = 1;
	bd->clientp = client;
	bd->rsens = BQ24185_RSENS_REF;
	bd->ext_status = -1;

	pdata = client->dev.platform_data;
	if (pdata) {
		bd->bat_ps.name = pdata->name;
		bd->rsens = pdata->rsens;
		bd->control = pdata;

		if (pdata->supplied_to) {
			bd->bat_ps.supplied_to = pdata->supplied_to;
			bd->bat_ps.num_supplicants = pdata->num_supplicants;
		}
	}

	if (!bd->rsens) {
		kfree(bd);
		pr_err("%s: Invalid sense resistance!\n", BQ24185_NAME);
		return -EINVAL;
	}

	mutex_init(&bd->lock);
	wake_lock_init(&bd->wake_lock, WAKE_LOCK_SUSPEND,
		       "bq24185_watchdog_lock");

	bd->wq = create_singlethread_workqueue("bq24185worker");
	if (!bd->wq) {
		pr_err("%s: Failed creating workqueue\n", BQ24185_NAME);
		rc = -ENOMEM;
		goto probe_exit_free;
	}

	INIT_DELAYED_WORK(&bd->work, bq24185_reset_watchdog_worker);

	rc = power_supply_register(&client->dev, &bd->bat_ps);
	if (rc) {
		pr_err("%s: Failed registering to power_supply class\n",
		       BQ24185_NAME);
		goto probe_exit_work_queue;
	}

	i2c_set_clientdata(client, bd);

	bq24185_check_status(bd);
	bq24185_update_power_supply(bd);

	if (pdata && pdata->support_boot_charging &&
	    STAT_CHARGE_IN_PROGRESS == bd->cached_status.stat) {
		pr_info("%s: Charging started by boot\n", BQ24185_NAME);
		bd->boot_initiated_charging = 1;
		/* Kick the watchdog to not lose the boot setting */
		bq24185_start_watchdog_reset(bd);
	} else {
		pr_info("%s: Not initialized by boot\n", BQ24185_NAME);
		rc = bq24185_reset_charger(bd);
		if (rc)
			goto probe_exit_unregister;

	}

	rc = request_threaded_irq(client->irq, NULL, bq24185_thread_irq,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				  IRQF_DISABLED | IRQF_ONESHOT,
				  "bq24185interrupt", bd);
	if (rc) {
		pr_err("%s: Failed requesting IRQ\n", BQ24185_NAME);
		goto probe_exit_unregister;
	}

	device_init_wakeup(&client->dev, 1);

	rc = enable_irq_wake(client->irq);
	if (rc)
		pr_err("%s: Failed to enable wakeup on IRQ request\n",
		BQ24185_NAME);
	else
		bd->irq_wake_enabled = 1;

#ifdef DEBUG_FS
	if (debug_create_attrs(bd->bat_ps.dev))
		pr_info("%s: Debug support failed\n", BQ24185_NAME);
#endif

	atomic_set(&bq24185_init_ok, 1);
	return 0;

probe_exit_unregister:
	power_supply_unregister(&bd->bat_ps);
probe_exit_work_queue:
	destroy_workqueue(bd->wq);
probe_exit_free:
	wake_lock_destroy(&bd->wake_lock);
	kfree(bd);

	return rc;
}

static struct i2c_driver bq24185_driver = {
	.driver = {
		   .name = BQ24185_NAME,
		   .owner = THIS_MODULE,
	},
	.probe = bq24185_probe,
	.remove = __exit_p(bq24185_remove),
	.id_table = bq24185_id,
};

static int __init bq24185_init(void)
{
	int rc;

	rc = i2c_add_driver(&bq24185_driver);
	if (rc) {
		pr_err("%s FAILED: i2c_add_driver rc=%d\n", __func__, rc);
		goto init_exit;
	}
	return 0;

init_exit:
	return rc;
}

static void __exit bq24185_exit(void)
{
	i2c_del_driver(&bq24185_driver);
}

module_init(bq24185_init);
module_exit(bq24185_exit);

MODULE_AUTHOR("James Jacobsson, Imre Sunyi");
MODULE_LICENSE("GPLv2");
