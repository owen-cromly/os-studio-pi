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
static struct task_struct *rpkthread0 = 0;
static struct task_struct *rpkthread1 = 0;
static struct task_struct *rpkthread2 = 0;
static struct task_struct *rpkthread3 = 0;

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
	rpkthread0 = kthread_create(*(foo), NULL, "my foo thread on core 0");
	if (rpkthread0==0) {
		printk(KERN_ALERT "failed to create thread.\n");	
		return FAIL_TO_START_THREAD;
	}
	kthread_bind(rpkthread0, 0);
	rpkthread1 = kthread_create(*(foo), NULL, "my foo thread on core 1");
	if (rpkthread1==0) {
		printk(KERN_ALERT "failed to create thread.\n");	
		return FAIL_TO_START_THREAD;
	}
	kthread_bind(rpkthread1, 1);
	rpkthread2 = kthread_create(*(foo), NULL, "my foo thread on core 2");
	if (rpkthread2==0) {
		printk(KERN_ALERT "failed to create thread.\n");	
		return FAIL_TO_START_THREAD;
	}
	kthread_bind(rpkthread2, 2);
	rpkthread3 = kthread_create(*(foo), NULL, "my foo thread on core 3");
	if (rpkthread3==0) {
		printk(KERN_ALERT "failed to create thread.\n");	
		return FAIL_TO_START_THREAD;
	}
	kthread_bind(rpkthread3, 3);
	wake_up_process(rpkthread0);
	wake_up_process(rpkthread1);
	wake_up_process(rpkthread2);
	wake_up_process(rpkthread3);

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
	printk("testing 0");
	if (rpkthread0)
	{
		printk("thread 0 found to be valid, now stopping it");
		kthread_stop(rpkthread0);
		printk("successfully stopped thread 0");
	}
	printk("testing 1");
	if (rpkthread1)
	{
		printk("thread 1 found to be valid, now stopping it");
		kthread_stop(rpkthread1);
		printk("successfully stopped thread 1");
	}
	printk("testing 2");
	if (rpkthread2)
	{
		printk("thread 2 found to be valid, now stopping it");
		kthread_stop(rpkthread2);
		printk("successfully stopped thread 2");
	}
	printk("testing 3");
	if (rpkthread3)
	{
		printk("thread 3 found to be valid, now stopping it");
		kthread_stop(rpkthread3);
		printk("successfully stopped thread 3");
	}
	printk(KERN_ALERT "rptimer module unloaded.\n");
}
/*
* hrtimer_restart - the restart function for my timer.
*/
static enum hrtimer_restart restart(struct hrtimer* timer) {
	// wake the thread before starting the timer
	if (rpkthread0)
		wake_up_process(rpkthread0);
	if (rpkthread1)
		wake_up_process(rpkthread1);
	if (rpkthread2)
		wake_up_process(rpkthread2);
	if (rpkthread3)
		wake_up_process(rpkthread3);
	hrtimer_forward_now(timer, ival);
	//printk("INFO: rptimer restarting timer");
	return HRTIMER_RESTART;
}
/*
* run_function
*/
static int foo(void *data) {
	int count = 0;
	int cpu_core = smp_processor_id();
	printk("INFO: function foo was executed on core %d\n",
		cpu_core);	 
	while (!kthread_should_stop()) {
		printk("INFO: foo count %d on core %d: nvcsw=%lu nivcsw=%lu\n",
			count,
			cpu_core,
			current->nvcsw,
			current->nivcsw);
		if (kthread_should_stop()) break; // be vigilant, young thread
		count++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	printk("INFO: foo thread on core %d terminating.\n",
		cpu_core);
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
MODULE_DESCRIPTION("A multi-threaded kernel module for Lab 1, CSE 4202");
