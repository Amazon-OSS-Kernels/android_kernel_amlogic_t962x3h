/*
 * led pattern driver for Jane
 *
 * Copyright (C) 2017 Technologies inc.
 * Jian Hu <jian.hu@amlogic.com>
 * Copyright (C) 2017 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/leds.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>

unsigned int pattern_val;
unsigned int led_scpi_switch;

#define LED_ON_BRIGHTNESS 255
//static DEFINE_SPINLOCK(pattern_lock);

enum breathe_state {
	BREATHE_STATE_OFF = 0,
	BREATHE_STATE_UP,
	BREATHE_STATE_PAUSE,
	BREATHE_STATE_DOWN,
	BREATHE_STATE_ON,
};

struct trigger_breathe_data {
	enum breathe_state state;
	struct timer_list timer;
	bool			activated;
};

void led_breathe_function(unsigned long data)
{
	struct led_classdev *ldev = (struct led_classdev *) data;
	struct trigger_breathe_data *breathe_data = ldev->trigger_data;

	int brightness = LED_OFF;
	unsigned long delay = 0;

	/* @todo breathe led setting */
	switch (breathe_data->state) {
	case BREATHE_STATE_OFF:
		delay = msecs_to_jiffies(30);

		brightness = LED_OFF;
		breathe_data->state = BREATHE_STATE_UP;
		break;

	case BREATHE_STATE_UP:
		delay = msecs_to_jiffies(45);

		brightness = ldev->brightness + 5;
		if (brightness > ldev->max_brightness) {
			brightness =  ldev->max_brightness;
			breathe_data->state = BREATHE_STATE_PAUSE;
		}
		break;

	case BREATHE_STATE_PAUSE:
		delay = msecs_to_jiffies(20);

		brightness = ldev->max_brightness;
		breathe_data->state = BREATHE_STATE_DOWN;
		break;

	case BREATHE_STATE_DOWN:
		delay = msecs_to_jiffies(31);

		brightness = ldev->brightness - 5;
		if (brightness < 0) {
			brightness =  LED_OFF;
			breathe_data->state = BREATHE_STATE_OFF;
		}
		break;

	default:
		delay = 0;
		brightness = LED_OFF;
		break;

	}

	led_set_brightness(ldev, brightness);
	mod_timer(&breathe_data->timer, jiffies + delay);
}

void led_breathe(struct led_classdev *led_cdev)
{
	struct trigger_breathe_data *breathe_data;

	breathe_data = led_cdev->trigger_data;
	setup_timer(&breathe_data->timer,
		led_breathe_function, (unsigned long)led_cdev);

	/* @todo init parameter */
	breathe_data->state = BREATHE_STATE_OFF;

	led_breathe_function(breathe_data->timer.data);
	breathe_data->activated = true;
}

char get_led_flag_from_file(const char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	char flag = -1;

	set_fs(KERNEL_DS);

	filp = filp_open(path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		pr_err("failed to open led_flag file: %s\n", path);
		goto PROCESS_END;
	}

	vfs_read(filp, &flag, 1, &pos);
	pr_warn("get led_flag flag=0x%x\n", flag);

	filp_close(filp, NULL);

PROCESS_END:
	set_fs(old_fs);
	return flag;
}

/*pattern = 1, led on*/
int red_solid(struct led_classdev *led_cdev)
{
	pr_warn("led: enter red_solid\n");
	led_set_brightness(led_cdev, LED_ON_BRIGHTNESS);
	return 0;
}

/*pattern = 2, led off*/
int red_off(struct led_classdev *led_cdev)
{
	led_cdev->blink_delay_on = 0;
	led_cdev->blink_delay_off = 0;
	pr_warn("led: enter red_off\n");
	led_set_brightness(led_cdev, LED_OFF);

	return 0;
}

/*pattern = 3, led breathe*/
int red_breathe(struct led_classdev *led_cdev)
{
	led_breathe(led_cdev);
	pr_warn("led: enter red_breathe\n");

	return 0;
}

/*pattern = 4, led blink*/
int red_blink(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;
	pr_warn("led: enter red_blink\n");

	led_cdev->brightness = LED_ON_BRIGHTNESS;
	led_blink_set(led_cdev, &delay_on, &delay_off);

	return 0;
}

/*pattern = 5, led blink once*/
int red_single_blink_on(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;

	pr_warn("led: enter red_single_blink_on\n");
	led_cdev->brightness = 0;
	led_blink_set_oneshot(led_cdev, &delay_on, &delay_off, 0);

	return 0;
}

extern int idme_get_model_name(char *model_name);
static void led_set_twenty_percent(struct work_struct *work)
{
	struct led_pwm_data *ldata = container_of(to_delayed_work(work),
			struct led_pwm_data, led_work);
	struct led_classdev *led_cdev = &ldata->cdev;
	char model_name[128] = {0};

	idme_get_model_name(model_name);
	if (strstr(model_name, "modelc") != NULL) {
		pr_warn("led: enter led_set_fifty_percent\n");
		led_set_brightness(led_cdev, 128);/* 50 percent brightness */
	} else {
		pr_warn("led: enter led_set_twenty_percent\n");
		led_set_brightness(led_cdev, 51);/* 20 percent brightness */
	}
}

/*pattern = 6, led blink once and set 20 percent brightness*/
int red_blink_once_20_percent(struct led_classdev *led_cdev)
{
	unsigned long delay_on = 500;
	unsigned long delay_off = 500;
	char flag = 0;

	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	pr_warn("led: enter red_blink_once_20_percent\n");
	led_cdev->brightness = 0;
	led_blink_set_oneshot(led_cdev, &delay_on, &delay_off, 0);

	flag = get_led_flag_from_file("/data/led/led_standby_flag");
    /// LED will turn on if ASCII "1"
    /// is got from led_standby_flag.
    /// ASCII "1" equals Hex 0x31

    /// LED will turn off if ASCII "0"
    /// is got from led_standby_flag.
    /// ASCII "0" equals Hex 0x30
	if (flag == 0x30) {
		pr_warn("led is off flag=%d\n", flag);
	} else {
		pr_warn("led is on flag=%d\n", flag);
		schedule_delayed_work(&ldata->led_work, msecs_to_jiffies(1100));
	}

	return 0;
}

static void red_breathe_off(struct led_classdev *led_cdev)
{
	struct trigger_breathe_data *breathe_data = led_cdev->trigger_data;

	if (breathe_data->activated) {
		del_timer_sync(&breathe_data->timer);
		breathe_data->activated = false;
	}
}

static ssize_t led_pattern_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pattern_val);
}

static ssize_t led_pattern_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int val, res;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//unsigned long flags;
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	 res = sscanf(buf, "%d", &val);
	if (res != 1) {
		dev_err(dev, "Can't parse parameter\n");
		return -EINVAL;
	}

	//spin_lock_irqsave(&pattern_lock, flags);

	red_breathe_off(led_cdev);
	cancel_delayed_work_sync(&ldata->led_work);
	switch (val) {
	case 1:
		red_solid(led_cdev);
		pattern_val = 1;
		break;
	case 2:
		red_off(led_cdev);
		pattern_val = 2;
		break;
	case 3:
		red_breathe(led_cdev);
		pattern_val = 3;
		break;
	case 4:
		red_blink(led_cdev);
		pattern_val = 4;
		break;
	case 5:
		red_single_blink_on(led_cdev);
		pattern_val = 5;
		break;
	case 6:
		red_blink_once_20_percent(led_cdev);
		pattern_val = 6;
		break;
	default:
		dev_err(dev, "unknown led pattern,only support 1-5 patterns\n");
		break;
	}
	//spin_unlock_irqrestore(&pattern_lock, flags);

	return size;
}

static DEVICE_ATTR(led_pattern, 0644,
				 led_pattern_show,
				 led_pattern_store);

static ssize_t led_scpi_switch_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", led_scpi_switch);
}

static ssize_t led_scpi_switch_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	 int val, res;

	 res = sscanf(buf, "%d", &val);
	if (res != 1) {
		dev_err(dev, "Can't parse parameter\n");
		return -EINVAL;
	}

	scpi_set_led_pattern(val);

	return size;
}
static DEVICE_ATTR(led_scpi, 0644,
				 led_scpi_switch_show,
				 led_scpi_switch_store);

void led_pattern_init(struct led_classdev *led_cdev)
{
	int rc;
	struct trigger_breathe_data *breathe_data;
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);
	breathe_data = kzalloc(sizeof(*breathe_data), GFP_KERNEL);
	if (!breathe_data)
		return;
	led_cdev->trigger_data = breathe_data;

	rc = device_create_file(led_cdev->dev, &dev_attr_led_pattern);
	rc = device_create_file(led_cdev->dev, &dev_attr_led_scpi);
	if (rc)
		goto err_out_led_pattern;

	INIT_DELAYED_WORK(&ldata->led_work, led_set_twenty_percent);
	pattern_val = 0;

	return;
err_out_led_pattern:
	device_remove_file(led_cdev->dev, &dev_attr_led_pattern);
}

void led_pattern_exit(struct led_classdev *led_cdev)
{
	struct led_pwm_data *ldata = container_of(led_cdev,
			struct led_pwm_data, cdev);

	cancel_delayed_work_sync(&ldata->led_work);
	device_remove_file(led_cdev->dev, &dev_attr_led_pattern);
	/* Stop blinking */
	led_set_brightness(led_cdev, LED_OFF);

	kfree(led_cdev->trigger_data);
	return;
}
