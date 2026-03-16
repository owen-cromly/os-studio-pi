#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#define FAIL_TO_START_THREAD -1

// global variables
static unsigned long log_sec = 1;
static unsigned long log_nsec = 0;
static ktime_t ival;
static struct hrtimer timer;
static struct task_struct *rpkthread = 0;

// function declarations
static enum hrtimer_restart restart(struct hrtimer* timer);
static int foo(void *data);
/*
* rptimer_init – the init function, called when the module is loaded.
* Returns zero if successfully loaded, nonzero otherwise.
*/
static int rptimer_init(void)
{	
	printk(KERN_ALERT "rptimer module loaded.\n");
	rpkthread = kthread_run(*(foo), NULL, "my foo thread");
	if (rpkthread==0) {
		printk(KERN_ALERT "failed to create thread.\n");	
		return FAIL_TO_START_THREAD;
	}
	ival = ktime_set(log_sec, log_nsec);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = restart;
	// after the thread exists
	hrtimer_start(&timer, ival, HRTIMER_MODE_REL);
	printk("INFO: rptimer starting timer");
	return 0;
}
/*
* rptimer_exit – the exit function, called when the module is removed.
*/
static void rptimer_exit(void)
{
	hrtimer_cancel(&timer); // stop timer first so it won't try to wake the thread
	printk(KERN_INFO "rptimer hrtimer successfully canceled");
	if (rpkthread)	
		kthread_stop(rpkthread);
	printk(KERN_ALERT "rptimer module unloaded.\n");
}
/*
* hrtimer_restart - the restart function for my timer.
*/
static enum hrtimer_restart restart(struct hrtimer* timer) {
	// wake the thread before starting the timer
	if (rpkthread)
		wake_up_process(rpkthread);
	hrtimer_forward_now(timer, ival);
	//printk("INFO: rptimer restarting timer");
	return HRTIMER_RESTART;
}
/*
* run_function
*/
static int foo(void *data) {
	int count = 0;
	printk("INFO: function foo was executed\n");
	while (!kthread_should_stop()) {
		printk("INFO: foo count %d: nvcsw=%lu nivcsw=%lu\n",
			count,
			current->nvcsw,
			current->nivcsw);
		if (kthread_should_stop()) break; // be vigilant, young thread
		count++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	printk("INFO: foo thread terminating\n");
	return 0;
}
// two unexposed parameters
module_param(log_sec, ulong, 0);
module_param(log_nsec, ulong, 0);
//
module_init(rptimer_init);
module_exit(rptimer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Owen Cromly");
MODULE_DESCRIPTION("A single-threaded kernel module for Lab 1, CSE 4202");
