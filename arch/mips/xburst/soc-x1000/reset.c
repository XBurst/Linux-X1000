/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <soc/base.h>
#include <soc/cpm.h>

#include <asm/reboot.h>

#define INGENIC_RTC   (32768)
#define INGENIC_EXTAL   (24*1000*1000)
#define RECOVERY_SIGNATURE	(0x001a1a)
#define REBOOT_SIGNATURE	(0x003535)
#define UNMSAK_SIGNATURE	(0x7c0000)//do not use these bits

int __init reset_init(void)
{
	return 0;
}
arch_initcall(reset_init);

///////////////////////////////////////////////////////////////////////////////////////////////////
struct wdt_reset {
	unsigned stop;
	unsigned msecs;
	struct task_struct *task;
	unsigned count;
};

static int reset_task(void *data) {
	struct wdt_reset *wdt = data;
	const struct sched_param param = {
		.sched_priority = MAX_RT_PRIO-1,
	};
	sched_setscheduler(current,SCHED_RR,&param);

	while (1) {
		if(kthread_should_stop()) {
			break;
		}
		msleep(wdt->msecs / 3);
	}

	return 0;
}

static void jz_burner_boot(void)
{
	unsigned int val = 'b' << 24 | 'u' << 16 | 'r' << 8 | 'n';

	cpm_outl(val, CPM_SLPC);

	while(1);
}

/* ============================wdt control sysfs start============================== */
static ssize_t wdt_time_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct wdt_reset *wdt = dev_get_drvdata(dev);
	return sprintf(buf, "%d msecs\n", wdt->msecs);
}
static ssize_t wdt_time_store(struct device *dev, struct device_attribute *attr,
								 const char *buf, size_t count)
{
	struct wdt_reset *wdt = dev_get_drvdata(dev);
	unsigned int msecs;

	if(!wdt->stop)
		return -EBUSY;

	sscanf(buf,"%d\n",&msecs);
	if(msecs < 1000) msecs = 1000;
	if(msecs > 30000) msecs = 30000;

	wdt->msecs = msecs;
	return count;
}
static DEVICE_ATTR_RW(wdt_time);
/* ============================wdt time sysfs end================================== */
/* ============================wdt control sysfs start=============================== */
static ssize_t wdt_control_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct wdt_reset *wdt = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", wdt->stop?">off<on\n":"off>on<\n");
}
static ssize_t wdt_control_store(struct device *dev, struct device_attribute *attr,
								 const char *buf, size_t count)
{
	struct wdt_reset *wdt = dev_get_drvdata(dev);

	if(!strncmp(buf,"on",2) && (wdt->stop == 1)) {
		wdt->task = kthread_run(reset_task, wdt, "reset_task%d",wdt->count++);
		wdt->stop = 0;
	} else if(!strncmp(buf,"off",3) && (wdt->stop == 0)) {
		kthread_stop(wdt->task);
		wdt->stop = 1;
	}

	return count;
}
static DEVICE_ATTR_RW(wdt_control);
/* ============================wdt control sysfs end =============================== */
/* ============================reset sysfs start=================================== */
static char *reset_command[] = {"wdt","hibernate","recovery", "burnerboot"};
static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int len = 0, i;

	for(i = 0; i < ARRAY_SIZE(reset_command); i++)
		len += sprintf(buf + len, "%s\t", reset_command[i]);
	len += sprintf(buf + len, "\n");

	return len;
}

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
						   const char *buf, size_t count)
{
	int command_size = 0;
	int i;

	if(count == 0)
		return -EINVAL;

	command_size = ARRAY_SIZE(reset_command);
	for(i = 0;i < command_size; i++) {
		if(!strncmp(buf, reset_command[i], strlen(reset_command[i])))
			break;
	}
	if(i == command_size)
		return -EINVAL;

	local_irq_disable();
	switch(i) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		jz_burner_boot();
		break;
	default:
		printk("not support command %d\n", i);
	}

	return count;
}
static DEVICE_ATTR_RW(reset);
/* ============================reset sysfs end=============================== */

static struct attribute *reset_attrs[] = {
	&dev_attr_wdt_time.attr,
	&dev_attr_wdt_control.attr,
	&dev_attr_reset.attr,
	NULL
};

static const struct attribute_group attr_group = {
	.attrs	= reset_attrs,
};

static int __init init_reset(void)
{
	return 0;
}
module_init(init_reset);
