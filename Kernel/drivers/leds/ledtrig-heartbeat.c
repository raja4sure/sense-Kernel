/*
 * LED Heartbeat Trigger
 *
 * Copyright (C) 2006 Atsushi Nemoto <anemo@mba.ocn.ne.jp>
 *
 * Based on Richard Purdie's ledtrig-timer.c and some arch's
 * CONFIG_HEARTBEAT code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#include "leds.h"

struct heartbeat_trig_data {
	unsigned int phase;
	unsigned int period;
	struct timer_list timer;
};

static int ledtrig_sleep_pm_callback(struct notifier_block *nfb,
					unsigned long action,
					void *ignored)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		//led_trigger_event(ledtrig_sleep, LED_FULL);
		return NOTIFY_OK;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		//led_trigger_event(ledtrig_sleep, LED_OFF);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ledtrig_sleep_pm_notifier = {
	.notifier_call = ledtrig_sleep_pm_callback,
	.priority = 0,
};

static int ledtrig_sleep_pm_callback(struct notifier_block *nfb,
					unsigned long action,
					void *ignored);

static void ledtrig_sleep_early_suspend(struct early_suspend *h)
{
	//led_trigger_event(ledtrig_sleep, LED_OFF);
}

static void ledtrig_sleep_early_resume(struct early_suspend *h)
{
	//led_trigger_event(ledtrig_sleep, LED_FULL);
}

static struct early_suspend ledtrig_sleep_early_suspend_handler = {
	.suspend = ledtrig_sleep_early_suspend,
	.resume = ledtrig_sleep_early_resume,
};

static void led_heartbeat_function(unsigned long data)
{
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct heartbeat_trig_data *heartbeat_data = led_cdev->trigger_data;
	unsigned long brightness = LED_OFF;
	unsigned long delay = 0;

	/* acts like an actual heart beat -- ie thump-thump-pause... */
	switch (heartbeat_data->phase) {
	case 0:
		heartbeat_data->period = msecs_to_jiffies(10000);
		delay = msecs_to_jiffies(70);
		heartbeat_data->phase++;
		brightness = led_cdev->max_brightness;
		break;
	case 1:
		delay = heartbeat_data->period / 4 - msecs_to_jiffies(70);
		heartbeat_data->phase++;
		break;
	case 2:
		delay = msecs_to_jiffies(70);
		heartbeat_data->phase++;
		brightness = led_cdev->max_brightness;
		break;
	default:
		delay = heartbeat_data->period - heartbeat_data->period / 4 -
			msecs_to_jiffies(70);
		heartbeat_data->phase = 0;
		break;
	}

	led_set_brightness(led_cdev, brightness);
	mod_timer(&heartbeat_data->timer, jiffies + delay);
}

static void heartbeat_trig_activate(struct led_classdev *led_cdev)
{
	struct heartbeat_trig_data *heartbeat_data;

	heartbeat_data = kzalloc(sizeof(*heartbeat_data), GFP_KERNEL);
	if (!heartbeat_data)
		return;

	led_cdev->trigger_data = heartbeat_data;
	setup_timer(&heartbeat_data->timer,
		    led_heartbeat_function, (unsigned long) led_cdev);
	heartbeat_data->phase = 0;
	led_heartbeat_function(heartbeat_data->timer.data);
}

static void heartbeat_trig_deactivate(struct led_classdev *led_cdev)
{
	struct heartbeat_trig_data *heartbeat_data = led_cdev->trigger_data;

	if (heartbeat_data) {
		del_timer_sync(&heartbeat_data->timer);
		kfree(heartbeat_data);
	}
}

static struct led_trigger heartbeat_led_trigger = {
	.name     = "heartbeat",
	.activate = heartbeat_trig_activate,
	.deactivate = heartbeat_trig_deactivate,
};

static int __init heartbeat_trig_init(void)
{
	register_pm_notifier(&ledtrig_sleep_pm_notifier);
	register_early_suspend(&ledtrig_sleep_early_suspend_handler);
	return led_trigger_register(&heartbeat_led_trigger);
}

static void __exit heartbeat_trig_exit(void)
{
	led_trigger_unregister(&heartbeat_led_trigger);
	unregister_early_suspend(&ledtrig_sleep_early_suspend_handler);
	unregister_pm_notifier(&ledtrig_sleep_pm_notifier);
}

module_init(heartbeat_trig_init);
module_exit(heartbeat_trig_exit);

MODULE_AUTHOR("Atsushi Nemoto <anemo@mba.ocn.ne.jp>");
MODULE_DESCRIPTION("Heartbeat LED trigger");
MODULE_LICENSE("GPL");
