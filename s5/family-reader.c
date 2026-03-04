// SPDX-License-Identifier: GPL-2.0
/*
 * Sample kobject implementation
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 */
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
// new includes
#include <linux/pid.h>
#include <linux/sched.h>

#define PID_ACCESS_ERROR -1

/*
 * This module shows how to create a simple subdirectory in sysfs called
 * /sys/kernel/kobject-example  In that directory, a file called "pid" 
 * is created.  If an integer is written to these files, it can be
 * later read out of it.
 */

static int pid;

/*
 * The "pid" file where a static variable is read from and written to.
 */
static ssize_t pid_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", pid);
}

static ssize_t pid_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	// obtain *tsk_ptr
	int pid_given;
	int write_status;
	struct pid *kpid_ptr;
	struct task_struct *tsk_ptr;
	struct task_struct *curr;
	write_status = kstrtoint(buf, 10, &pid_given);
	if (write_status != 0) {
		printk("Error: could not convert parameter '%s' to integer (to PID)",buf);
		return write_status;
	}
	kpid_ptr = find_vpid(pid_given);	
	if (kpid_ptr == NULL) {
		printk("Error: attempt to read family history of %d failed. No kernel PID could be found from %d",pid_given,pid_given);
		return PID_ACCESS_ERROR;
	}
	tsk_ptr = get_pid_task(kpid_ptr, PIDTYPE_PID);
	if (tsk_ptr == NULL) {
		printk("Error: attempt to read family history of %d failed. A kernel PID (count=%d) was found, but no task struct pointer appears to be associated with it",pid_given,kpid_ptr->count.refs.counter);
		return PID_ACCESS_ERROR;
	}
	// now is the real meat of the function
	curr = tsk_ptr;

	printk(KERN_INFO "Walking up process ancestry:\n");

	while (curr) {
		printk(KERN_INFO "PID: %d | Comm: %s\n",
		       curr->pid,
		       curr->comm);

		if (curr->pid == 1)
		    break;

		curr = curr->parent;
	}	


	return count;
	// ^ old code

	
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute pid_attribute =
	__ATTR(pid, 0664, pid_show, pid_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&pid_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *example_kobj;

static int __init example_init(void)
{
	int retval;

	/*
	 * Create a simple kobject with the name of "family_reader",
	 * located under /sys/kernel/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	example_kobj = kobject_create_and_add("family_reader", kernel_kobj);
	if (!example_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(example_kobj, &attr_group);
	if (retval)
		kobject_put(example_kobj);

	return retval;
}

static void __exit example_exit(void)
{
	kobject_put(example_kobj);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Greg Kroah-Hartman <greg@kroah.com>");
MODULE_VERSION("ocrom04-modified+");
