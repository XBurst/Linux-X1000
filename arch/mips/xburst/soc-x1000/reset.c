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
#include <soc/rtc.h>
#include <soc/tcu.h>
#include <jz_notifier.h>

#include <asm/reboot.h>

#define INGENIC_RTC   (32768)
#define INGENIC_EXTAL   (24*1000*1000)
#define RECOVERY_SIGNATURE	(0x001a1a)
#define REBOOT_SIGNATURE	(0x003535)
#define UNMSAK_SIGNATURE	(0x7c0000)//do not use these bits

#if 0
static void dump_wdt_register(void)
{
    printk("WDT_TDR  = 0x%08x\n", inl(WDT_IOBASE + WDT_TDR));
    printk("WDT_TCER = 0x%08x\n", inl(WDT_IOBASE + WDT_TCER));
    printk("WDT_TCNT = 0x%08x\n", inl(WDT_IOBASE + WDT_TCNT));
    printk("WDT_TCSR = 0x%08x\n", inl(WDT_IOBASE + WDT_TCSR));
    printk("OPCR  = %08x\n", cpm_inl(CPM_OPCR));
}
#endif

static void wdt_start_count(int msecs)
{
	unsigned int time;

        if (cpm_inl(CPM_OPCR) & OPCR_ERCS) {
            /* 2ms < timer < 128s */
            time = INGENIC_RTC / 64 * msecs / 1000;
        } else {
            /* if EXTAL == 24M,  1.36ms < timer < 90s */
            time = (INGENIC_EXTAL / 512) / 64 * msecs / 1000;
        }

	if(time > 65535)
		time = 65535;

	outl(1 << 16,TCU_IOBASE + TCU_TSCR);

	outl(0,WDT_IOBASE + WDT_TCNT);		//counter
	outl(time,WDT_IOBASE + WDT_TDR);	//data
	outl((3<<3 | 1<<1),WDT_IOBASE + WDT_TCSR);
	outl(0,WDT_IOBASE + WDT_TCER);
	outl(1,WDT_IOBASE + WDT_TCER);
}

static void wdt_stop_count(void)
{
	outl(1 << 16,TCU_IOBASE + TCU_TSCR);
	outl(0,WDT_IOBASE + WDT_TCNT);		//counter
	outl(65535,WDT_IOBASE + WDT_TDR);	//data
	outl(1 << 16,TCU_IOBASE + TCU_TSSR);
}
static inline int wait_write_ready(void)
{
	int timeout = 0x2000;
	while (!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY) && timeout--);
	if (timeout <= 0) {
		printk("RTC : %s timeout!\n",__func__);
		return -1;
	}
	return 0;
}
static int inline rtc_write_reg(int reg,int value)
{
	int timeout = 0x2000;
	if(wait_write_ready()) {
		return -1;
	}
	outl(0xa55a,(RTC_IOBASE + RTC_WENR));
	if(wait_write_ready()) {
		return -1;
	}
	while(!(inl(RTC_IOBASE + RTC_WENR) & WENR_WEN) && timeout--);
	if (timeout <= 0) {
		printk("RTC : %s timeout!\n",__func__);
		outl(inl(RTC_IOBASE + RTC_RTCCR) | (1 << 1), (RTC_IOBASE + RTC_RTCCR));
		return -1;
	}
	if(wait_write_ready()) {
		return -1;
	}
	outl(value,(RTC_IOBASE + reg));
	if(wait_write_ready()) {
		return -1;
	}

	return 0;
}

/*
 * Function: Keep power for CPU core when reset.
 * So that EPC, tcsm and so on can maintain it's status after reset-key pressed.
 */
int reset_keep_power(void)
{
	return rtc_write_reg(RTC_WKUPPINCR, inl(RTC_IOBASE + RTC_WKUPPINCR) & ~(1 << 2));
}

#define HWFCR_WAIT_TIME_D(x) ((x > 0x7fff ? 0x7fff: (0x7ff*(x)) / 2000) << 5)
#define HRCR_WAIT_TIME_D(x) ((((x) > 1875 ? 1875: (x)) / 125) << 11)

void jz_hibernate(void)
{
	uint32_t rtc_rtccr;

	local_irq_disable();
	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 1000ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME_D(1000));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, HRCR_WAIT_TIME_D(125));

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	rtc_write_reg(RTC_HWCR, 0x8);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, 0x1);

	/*poweroff the pmu*/
	jz_notifier_call(NOTEFY_PROI_HIGH, JZ_POST_HIBERNATION, NULL);

	rtc_rtccr = inl(RTC_IOBASE + RTC_RTCCR);
	rtc_rtccr |= 0x1 << 0;
	rtc_write_reg(RTC_RTCCR,rtc_rtccr);

	mdelay(200);

	while(1)
		printk("%s:We should NOT come here.%08x\n",__func__, inl(RTC_IOBASE + RTC_HCR));
}

void jz_wdt_restart(char *command)
{
	printk("Restarting after 4 ms\n");
	if ((command != NULL) && !strcmp(command, "recovery")) {
		while(cpm_inl(CPM_CPPSR) != RECOVERY_SIGNATURE) {
			printk("set RECOVERY_SIGNATURE\n");
			cpm_outl(0x5a5a,CPM_CPSPPR);
			cpm_outl(RECOVERY_SIGNATURE,CPM_CPPSR);
			cpm_outl(0x0,CPM_CPSPPR);
			udelay(100);
		}
	} else {
		cpm_outl(0x5a5a,CPM_CPSPPR);
		cpm_outl(REBOOT_SIGNATURE,CPM_CPPSR);
		cpm_outl(0x0,CPM_CPSPPR);
	}

	wdt_start_count(4);
	mdelay(200);
	while(1)
		printk("check wdt.\n");
}

static void hibernate_restart(void)
{
	uint32_t rtc_rtcsr,rtc_rtccr;

	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));

	rtc_rtcsr = inl(RTC_IOBASE + RTC_RTCSR);
	rtc_rtccr = inl(RTC_IOBASE + RTC_RTCCR);

	rtc_write_reg(RTC_RTCSAR,rtc_rtcsr + 5);
	rtc_rtccr &= ~(1 << 4 | 1 << 1);
	rtc_rtccr |= 0x3 << 2;
	rtc_write_reg(RTC_RTCCR,rtc_rtccr);

	/* Clear reset status */
	cpm_outl(0,CPM_RSR);

	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 1000ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME_D(1000));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, HRCR_WAIT_TIME_D(125));

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	rtc_write_reg(RTC_HWCR, 0x9);
	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, 0x1);

	rtc_rtccr = inl(RTC_IOBASE + RTC_RTCCR);
	rtc_rtccr |= 0x1 << 0;
	rtc_write_reg(RTC_RTCCR,rtc_rtccr);

	mdelay(200);
	while(1)
		printk("%s:We should NOT come here.%08x\n",__func__, inl(RTC_IOBASE + RTC_HCR));
}
#ifdef CONFIG_HIBERNATE_RESET
void jz_hibernate_restart(char *command)
{
	local_irq_disable();

	if ((command != NULL) && !strcmp(command, "recovery")) {
		jz_wdt_restart(command);
	}

	hibernate_restart();
}
#endif

int __init reset_init(void)
{
	pm_power_off = jz_hibernate;
#ifdef CONFIG_HIBERNATE_RESET
	_machine_restart = jz_hibernate_restart;
#else
	_machine_restart = jz_wdt_restart;
#endif
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

	wdt_start_count(wdt->msecs);
	while (1) {
		if(kthread_should_stop()) {
			wdt_stop_count();
			break;
		}
		outl(0,WDT_IOBASE + WDT_TCNT);
		msleep(wdt->msecs / 3);
	}

	return 0;
}

static void jz_burner_boot(void)
{
	unsigned int val = 'b' << 24 | 'u' << 16 | 'r' << 8 | 'n';

	cpm_outl(val, CPM_SLPC);

	wdt_start_count(2);
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
		jz_wdt_restart(NULL);
		break;
	case 1:
		hibernate_restart();
		break;
	case 2:
		jz_wdt_restart("recovery");
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

static int wdt_probe(struct platform_device *pdev)
{
	struct wdt_reset *wdt;
	int ret;

	wdt = kmalloc(sizeof(struct wdt_reset),GFP_KERNEL);
	if(!wdt) {
		return -ENOMEM;
	}

	wdt->count = 0;
	wdt->msecs = 3000;

	dev_set_drvdata(&pdev->dev, wdt);
#ifdef CONFIG_SUSPEND_WDT
	wdt->stop = 0;
	wdt->task = kthread_run(reset_task, wdt, "reset_task%d",wdt->count++);
#else
	wdt->stop = 1;
#endif

	ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (ret) {
		printk("wdt creat sysfs failed\n");
		return ret;

	}

	return 0;
}

int wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct wdt_reset *wdt = dev_get_drvdata(&pdev->dev);
	if(wdt->stop)
		return 0;
	kthread_stop(wdt->task);
	return 0;
}

void wdt_shutdown(struct platform_device *pdev)
{
	wdt_stop_count();
}

int wdt_resume(struct platform_device *pdev)
{
	struct wdt_reset *wdt = dev_get_drvdata(&pdev->dev);
	if(wdt->stop)
		return 0;
	wdt->task = kthread_run(reset_task, wdt, "reset_task%d",wdt->count++);
	return 0;
}

static struct platform_device wdt_pdev = {
	.name		= "wdt_reset",
};

static struct platform_driver wdt_pdrv = {
	.probe		= wdt_probe,
	.shutdown	= wdt_shutdown,
	.suspend	= wdt_suspend,
	.resume		= wdt_resume,
	.driver		= {
		.name	= "wdt_reset",
		.owner	= THIS_MODULE,
	},
};

static int __init init_reset(void)
{
	platform_driver_register(&wdt_pdrv);
	platform_device_register(&wdt_pdev);
	return 0;
}
module_init(init_reset);
