#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>

static unsigned long log_sec, log_nsec;
static ktime_t ival;
static struct hrtimer timer;
static enum hrtimer_restart restart(struct hrtimer* timer);

/*
* rptimer_init – the init function, called when the module is loaded.
* Returns zero if successfully loaded, nonzero otherwise.
*/
static int rptimer_init(void)
{
	printk(KERN_ALERT "rptimer module loaded.\n");
	ival = ktime_set(log_sec, log_nsec);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = restart;
	return 0;
}
/*
* rptimer_exit – the exit function, called when the module is removed.
*/
static void rptimer_exit(void)
{
	printk(KERN_ALERT "rptimer module unloaded.\n");
}
/*
* hrtimer_restart - the restart function for my timer.
*/
static enum hrtimer_restart restart(struct hrtimer* timer) {
	hrtimer_forward_now(timer, ival);
	return HRTIMER_RESTART;
}
// two unexposed parameters
module_param(log_sec, ulong, 0);
module_param(log_nsec, ulong, 0);
//
module_init(rptimer_init);
module_exit(rptimer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Owen Cromly");
MODULE_DESCRIPTION("A kernel module for Lab 1, CSE 4202");
