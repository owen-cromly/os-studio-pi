Lab 1
=====

This was done by Owen Cromly, alone. A mirror with source and .ko is
available <http://github.com/owen-cromly/os-studio-pi.git/tree/main/l1>

A PDF version is also submitted, as desired.

Module Design
-------------

I started with the hello world module in LKD 338-339. I added the
following global variables (with initial values to serve as defaults),

`static unsigned long log_sec = 1`
`static unsigned long log_nsec = 0;`

I then added them as module params, using a `0` for `perm` to ensure
they are not exposed in the `sysfs` interface.

`module_param(log_sec, ulong, 0);`
`module_param(log_nsec, ulong, 0);`

Timer Design and Evaluation
---------------------------

At first, I implemented this in a fairly naive way. I created a
function, restart():

`/*`
`* hrtimer_restart - the restart function for my timer.`
`*/`
`static enum hrtimer_restart restart(struct hrtimer* timer) {`
        `printk("INFO: rptimer (re)starting timer");`
        `hrtimer_forward_now(timer, ival);`
        `return HRTIMER_RESTART;`
`}`

which is not ideal but is broadly functional. I made the necessary calls
in `rptimer_init()` and `rptimer_exit()` which are specified in the
instructions. The results of the first test are shown below.

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko`
`ocrom04@rpi4b:~/repos/l1 $ dmesg -wH`
`[Mar 8 10:12] rptimer module loaded.`
`[  +1.003634] INFO: rptimer (re)starting timer`
`[  +1.000031] INFO: rptimer (re)starting timer`
`[  +1.000027] INFO: rptimer (re)starting timer`
`[  +1.000029] INFO: rptimer (re)starting timer`
`[  +1.000029] INFO: rptimer (re)starting timer`
`[  +1.000027] INFO: rptimer (re)starting timer`
`[  +1.000029] INFO: rptimer (re)starting timer`

I noticed that the first repeat has extra time required. This was not
correct---I simply had a missing print statement.... The following
change was made to properly track the first timer...

`/*`
`* rptimer_init – the init function, called when the module is loaded.`
`* Returns zero if successfully loaded, nonzero otherwise.`
`*/`
`static int rptimer_init(void)`
`{`
        `printk(KERN_ALERT "rptimer module loaded.\n");`
        `ival = ktime_set(log_sec, log_nsec);`
        `hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);`
        `timer.function = restart;`
        `hrtimer_start(&timer, ival, HRTIMER_MODE_REL);`
`+`      `printk("INFO: rptimer starting timer");`
        `return 0;`
`}`
`/*`

Now, the problem is fixed. Here is the result, for several
configurations

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=1 log_nsec=0`
`ocrom04@rpi4b:~/repos/l1 $ dmesg -wH`
`[Mar14 16:40] rptimer module loaded.`
`[  +0.003495] INFO: rptimer starting timer`
`[  +1.000008] INFO: rptimer restarting timer`
`[  +0.999997] INFO: rptimer restarting timer`
`[  +0.999997] INFO: rptimer restarting timer`
`[  +0.999999] INFO: rptimer restarting timer`

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=0 log_nsec=900000000`
`ocrom04@rpi4b:~/repos/l1 $ dmesg -wH`
`[Mar14 16:45] rptimer module loaded.`
`[  +0.003495] INFO: rptimer starting timer`
`[  +0.899998] INFO: rptimer restarting timer`
`[  +0.899991] INFO: rptimer restarting timer`
`[  +0.899994] INFO: rptimer restarting timer`
`[  +0.899990] INFO: rptimer restarting timer`

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=1 log_nsec=400000000`
`ocrom04@rpi4b:~/repos/l1 $ dmesg -wH`
`[Mar14 16:53] rptimer module loaded.`
`[  +0.003952] INFO: rptimer starting timer`
`[  +1.400035] INFO: rptimer restarting timer`
`[  +1.400028] INFO: rptimer restarting timer`
`[  +1.400029] INFO: rptimer restarting timer`
`[  +1.400027] INFO: rptimer restarting timer`

Thread Design and Evaluation
----------------------------

In my loop function `foo`, I placed a `while` loop that checks
`kthread_should_stop()`. Inside the loop, the process prints its loop
count and context-switch counts, sets itself to TASK\_INTERRUPTIBLE, and
calls `schedule()`. This ensures that `foo`, each time it is called,
prints information and then waits to be called again. Additionally, it
will stop altogether at the next loop boundary after
`kthread_stop(rpkthread)` is called.

In my timer-triggered restart function `restart`, I check if `rpkthread`
is nonzero (i.e. properly initialized) and, if so, I call
`wake_up_process(rpkthread)`. Regardless of whether the process is
initialized, I will still restart the timer. This way, if the timer ever
runs out before the thread is properly initialized, behavior is still
defined and unproblematic.

In my init function `rptimer_init`, I first create the thread, saving
the `task_struct*` in `rpkthread`, which is initially set to 0. I check
that `rpkthread` is not zero (and do not continue if it is), and then I
initialize the timer.

The ultimate design in general has two components

-   a looping function `foo` that is run by a kernel thread, which runs,
    blocks, and waits for a trigger to run again.
-   a timer-triggered function `restart` that wakes up `foo` and resets
    the timer.

If `foo` does not finish execution by the time the timer expires and
`restart` is run, we do miss the signal but do not run into recursion
problems and overflow of the kernel stack. This is a great benefit of
this design.

Lastly, I tried to fix an issue that was preventing the module from
unloading with short timer periods. When the `exit` function was called,
if the timer was short enough, it would get interrupted before it was
even able to disable the clock....

I added a static variable `static bool stopping = 0`, and made it so
that the first line of `rptimer_exit()` is `stopping = 1`. I modified
`restart` so that it returns early `if (stopping)`, without resetting
the timer.

It should be noted that this is not the proper solution. The problem was
not that the `hrtimer_cancel()` call was getting interrupted (it would
just continue on after getting interrupted). Yet at the time, I thought
this was correct. The *actual* problem was discovered and solved later,
during testing of the multi-threaded version, and applied to both
modules. It was a problem with how `foo` was checking (or not checking)
`kthread_should_stop()`.

Here are the results of a few runs:
`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=1 log_nsec=0`
`ocrom04@rpi4b:~/repos/l1 $ sudo dmesg -wH`
`[Mar15 11:11] rptimer module loaded.`
`[  +0.003769] INFO: rptimer starting timer`
`[  +0.000021] INFO: function foo was executed`
`[  +0.008316] INFO: foo count 0: nvcsw=1 nivcsw=0`
`[  +0.991727] INFO: foo count 1: nvcsw=2 nivcsw=1`
`[  +1.000016] INFO: foo count 2: nvcsw=3 nivcsw=1`
`[  +1.000014] INFO: foo count 3: nvcsw=4 nivcsw=1`
`[  +1.000023] INFO: foo count 4: nvcsw=5 nivcsw=1`
`[  +1.000024] INFO: foo count 5: nvcsw=6 nivcsw=1`

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=0 log_nsec=50000000`
`ocrom04@rpi4b:~/repos/l1 $ sudo dmesg -wH`
`[Mar15 11:12] rptimer module loaded.`
`[  +0.003751] INFO: rptimer starting timer`
`[  +0.000019] INFO: function foo was executed`
`[  +0.010236] INFO: foo count 0: nvcsw=1 nivcsw=1`
`[  +0.039786] INFO: foo count 1: nvcsw=2 nivcsw=2`
`[  +0.050004] INFO: foo count 2: nvcsw=3 nivcsw=2`
`[  +0.049992] INFO: foo count 3: nvcsw=4 nivcsw=3`
`[  +0.050012] INFO: foo count 4: nvcsw=5 nivcsw=3`
`[  +0.049994] INFO: foo count 5: nvcsw=6 nivcsw=3`

`ocrom04@rpi4b:~/repos/l1 $ sudo insmod rptimer.ko log_sec=0 log_nsec=2500000`
`ocrom04@rpi4b:~/repos/l1 $ sudo dmesg -wH`
`[Mar15 11:20] rptimer module loaded.`
`[  +0.008947] INFO: rptimer starting timer`
`[  +0.000017] INFO: function foo was executed`
`[  +0.009022] INFO: foo count 0: nvcsw=1 nivcsw=1`
`[  +0.006005] INFO: foo count 1: nvcsw=2 nivcsw=2`
`[  +0.004996] INFO: foo count 2: nvcsw=3 nivcsw=3`
`[  +0.007494] INFO: foo count 3: nvcsw=4 nivcsw=4`
`[  +0.004994] INFO: foo count 4: nvcsw=5 nivcsw=4`

The variation is not high for 1 second or 50 millisecond, but for 2.5
millisecond it is high. This makes sense, as the high variation is
probably caused by involuntary context switches. For a longer log, it
can be seen that the moments of high delay are not always correlated to
the involuntary context switches. It can also be seen that high delays
are not always followed by low or lower delays, and that the average is
not 2.5 milliseconds. This all is because the delays are caused as much
by involuntary context switches of `rpkthread` as involuntary context
switches of `restart` and the time it takes to awaken `rpkthread`.

It can be seen that as the timer duration approaches a millisecond, the
amount of contention greatly increases. This is because the 'duty cycle'
increases as the timer period gets shorter (and the execution of `foo`
takes the same amount of time). There are two compounding considerations
that increase the number of involuntary context switches.

-   First, the module spends a larger percentage of its time
    running---so, for some given stream of involuntary scheduling
    events, a scheduling event is more likely to occur during execution
    of `foo` and less likely to occur while `foo` is waiting---for a
    short timer compared to a long timer.
-   Second, the greater usage of the CPU by this module increases the
    overall contention and the number of involuntary scheduling events
    in the first place. The first factor only explains why the module
    experiences more involuntary context switches in a span of time. The
    second one explains why the module also experiences more context
    switches per voluntary context switch.

Multi-threading Design and Evaluation
-------------------------------------

In order to easily collect the correct number of `dmesg` logs and run a
test in one go, I use the following script:

`fscript:`
`#!/bin/bash`

`dmesg --clear`
`numsec=$(bc -l <<< "5.5*( $2+$(bc -l <<<$3/1000000000) )")`
`#echo "insmod ${1}.ko log_sec=$2 log_nsec=$3"`
`insmod "${1}.ko" log_sec=$2 log_nsec=$3`
`sleep ${numsec}s`
`dmesg`
`rmmod ${1}`

In order to implement multi-threading, the changes themselves were
relatively trivial. I essentially replaced each instance of `rpkthread`
with `rpkthreadN`. If a section of code tested `rpkthread`, I created a
separate test and conditional code segment for each thread. Finally, I
used `smp_processor_id()` to set `foo`-local `int cpu_core` and mention
the core in kernel logs.

The only structural change was that instead of using `kthread_run()` I
used, as instructed, `kthread_create()`, `kthread_bind()` and
`wake_up_process()` to allow each thread to be bound to a separate
processor core.

Lastly---in the course of testing this new module, I discovered a bug.
The result of the bug is that, for short timer intervals, the `exit()`
function will fail. The mechanism is that, when the `exit()` function
calls
`kthread_stop(rpkthreadN) during a period of time in which the`foo`function in the thread is actively running (e.g. as it is waiting on`printk()`), the wake signal is sent before the thread sleeps. The thread therefore never returns to the while loop condition, and the thread stalls. Likewise, the`rmmod\`
process also stalls, blocking on the termination of a thread that is
sleeping for good.

The fix is super simple:

`static int foo(void *data) {`
        `int count = 0;`
        `int cpu_core = smp_processor_id();`
        `printk("INFO: function foo was executed on core %d\n",`
                `cpu_core);`
        `while (!kthread_should_stop()) {`
                `printk("INFO: foo count %d on core %d: nvcsw=%lu nivcsw=%lu\n",`
                        `count,`
                        `cpu_core,`
                        `current->nvcsw,`
                        `current->nivcsw);`
`+`               `if (kthread_should_stop()) break; // be vigilant, young thread`
                `count++;`
                `set_current_state(TASK_INTERRUPTIBLE);`
                `schedule();`
        `}`
        `printk("INFO: foo thread on core %d terminating.\n",`
                `cpu_core);`
        `return 0;`
`}`

Now, we make several runs for different time values. We'll include the
log contents until each core has reached 5 `foo` loop runs.

First, for 1 second ------

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 1 0`
`[ 6962.708992] rptimer module loaded.`
`[ 6962.713578] INFO: function foo was executed on core 0`
`[ 6962.718743] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[ 6962.727748] INFO: rptimer starting timer`
`[ 6962.727762] INFO: function foo was executed on core 1`
`[ 6962.727770] INFO: function foo was executed on core 3`
`[ 6962.727788] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[ 6962.734897] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=1`
`[ 6962.737844] INFO: function foo was executed on core 2`
`[ 6962.758864] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=1`
`[ 6963.727829] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=1`
`[ 6963.727838] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=2`
`[ 6963.727846] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=1`
`[ 6963.727855] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=2`
`[ 6964.727819] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=1`
`[ 6964.727828] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=2`
`[ 6964.727837] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=2`
`[ 6964.727844] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=2`
`[ 6965.727835] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=2`
`[ 6965.727843] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=1`
`[ 6965.727852] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=2`
`[ 6965.727860] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=2`
`[ 6966.727847] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=2`
`[ 6966.727856] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=2`
`[ 6966.727864] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=1`
`[ 6966.727872] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=2`
`[ 6967.727840] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=1`
`[ 6967.727849] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=3`
`[ 6967.727857] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=2`
`[ 6967.727866] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=2`

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 1 0`
`[  341.553566] mt_rptimer: loading out-of-tree module taints kernel.`
`[  341.560468] rptimer module loaded.`
`[  341.564764] INFO: rptimer starting timer`
`[  341.564780] INFO: function foo was executed on core 1`
`[  341.564785] INFO: function foo was executed on core 2`
`[  341.564791] INFO: function foo was executed on core 3`
`[  341.564802] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=0`
`[  341.566011] INFO: function foo was executed on core 0`
`[  341.566027] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[  341.570840] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=1`
`[  341.598025] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=1`
`[  342.564837] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=0`
`[  342.564845] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=0`
`[  342.564852] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=2`
`[  342.564860] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[  343.564842] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=1`
`[  343.564850] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=2`
`[  343.564857] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=0`
`[  343.564865] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[  344.564842] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=2`
`[  344.564850] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[  344.564857] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=0`
`[  344.564865] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=2`
`[  345.564854] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=3`
`[  345.564862] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[  345.564869] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=0`
`[  345.564876] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=2`
`[  346.564856] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=4`
`[  346.564865] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[  346.564873] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=0`
`[  346.564880] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=2`

Then, for 100 ms ------

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 100000000`
`[ 7345.838698] rptimer module loaded.`
`[ 7345.842989] INFO: function foo was executed on core 2`
`[ 7345.842999] INFO: function foo was executed on core 0`
`[ 7345.843005] INFO: function foo was executed on core 1`
`[ 7345.843018] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[ 7345.848316] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=1`
`[ 7345.848352] INFO: rptimer starting timer`
`[ 7345.857596] INFO: function foo was executed on core 3`
`[ 7345.864648] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=1`
`[ 7345.870304] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=2`
`[ 7345.948403] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=1`
`[ 7345.948411] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[ 7345.948420] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=0`
`[ 7345.948428] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=3`
`[ 7346.048409] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[ 7346.048417] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=0`
`[ 7346.048426] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=2`
`[ 7346.048434] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=3`
`[ 7346.148404] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=0`
`[ 7346.148413] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=2`
`[ 7346.148421] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[ 7346.148429] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=3`
`[ 7346.248408] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[ 7346.248418] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=0`
`[ 7346.248426] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=2`
`[ 7346.248434] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=3`
`[ 7346.348417] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=0`
`[ 7346.348425] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[ 7346.348433] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=2`
`[ 7346.348441] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=3`

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 100000000`
`[  455.221261] rptimer module loaded.`
`[  455.225512] INFO: rptimer starting timer`
`[  455.225527] INFO: function foo was executed on core 2`
`[  455.225533] INFO: function foo was executed on core 1`
`[  455.225551] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=0`
`[  455.225662] INFO: function foo was executed on core 3`
`[  455.225677] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[  455.228711] INFO: function foo was executed on core 0`
`[  455.228732] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[  455.233132] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=1`
`[  455.325565] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=0`
`[  455.325573] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[  455.325581] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=0`
`[  455.325588] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=2`
`[  455.425561] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=1`
`[  455.425569] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[  455.425576] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=0`
`[  455.425583] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=2`
`[  455.525568] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=0`
`[  455.525576] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=2`
`[  455.525584] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[  455.533501] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=2`
`[  455.625568] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[  455.625576] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=1`
`[  455.625584] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=2`
`[  455.633181] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=2`
`[  455.725572] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=2`
`[  455.725580] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=2`
`[  455.725588] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=1`
`[  455.728664] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=3`

Then, for 1 ms ------

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 1000000`
`[ 7445.037665] rptimer module loaded.`
`[ 7445.041991] INFO: rptimer starting timer`
`[ 7445.042001] INFO: function foo was executed on core 1`
`[ 7445.042013] INFO: function foo was executed on core 0`
`[ 7445.042019] INFO: function foo was executed on core 2`
`[ 7445.042032] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[ 7445.042038] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=0`
`[ 7445.046048] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=0`
`[ 7445.046056] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=0`
`[ 7445.047028] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=0`
`[ 7445.047385] INFO: function foo was executed on core 3`
`[ 7445.047408] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[ 7445.048022] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=0`
`[ 7445.048029] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=0`
`[ 7445.049018] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=0`
`[ 7445.049904] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=0`
`[ 7445.050021] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=0`
`[ 7445.050027] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=0`
`[ 7445.051016] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=0`
`[ 7445.051023] INFO: foo count 6 on core 0: nvcsw=7 nivcsw=0`
`[ 7445.052140] INFO: foo count 7 on core 0: nvcsw=8 nivcsw=0`
`[ 7445.052148] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=0`
`[ 7445.054737] INFO: foo count 6 on core 3: nvcsw=7 nivcsw=0`
`[ 7445.055018] INFO: foo count 7 on core 3: nvcsw=8 nivcsw=0`
`[ 7445.055510] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=1`
`[ 7445.056018] INFO: foo count 8 on core 3: nvcsw=9 nivcsw=0`
`[ 7445.056025] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[ 7445.057019] INFO: foo count 9 on core 3: nvcsw=10 nivcsw=0`
`[ 7445.057039] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[ 7445.067265] INFO: foo count 8 on core 0: nvcsw=9 nivcsw=0`
`[ 7445.067274] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=1`
`[ 7445.068878] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[ 7445.072771] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=1`
`[ 7445.072779] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[ 7445.073018] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[ 7445.073025] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=1`
`[ 7445.074025] INFO: foo count 10 on core 3: nvcsw=11 nivcsw=1`
`[ 7445.074714] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=1`

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 1000000`
`[  579.508150] rptimer module loaded.`
`[  579.513279] INFO: rptimer starting timer`
`[  579.513285] INFO: function foo was executed on core 0`
`[  579.513291] INFO: function foo was executed on core 1`
`[  579.513304] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[  579.513310] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=0`
`[  579.517342] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=0`
`[  579.518660] INFO: function foo was executed on core 2`
`[  579.518676] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=0`
`[  579.519308] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=0`
`[  579.520321] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=0`
`[  579.520748] INFO: function foo was executed on core 3`
`[  579.520764] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[  579.521322] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=0`
`[  579.522314] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=0`
`[  579.523312] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=1`
`[  579.523319] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=0`
`[  579.524383] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=0`
`[  579.525309] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=0`
`[  579.526305] INFO: foo count 6 on core 3: nvcsw=7 nivcsw=0`
`[  579.527310] INFO: foo count 7 on core 3: nvcsw=8 nivcsw=0`
`[  579.528307] INFO: foo count 8 on core 3: nvcsw=9 nivcsw=0`
`[  579.530647] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=0`
`[  579.531462] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=0`
`[  579.532361] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=0`
`[  579.533393] INFO: foo count 6 on core 2: nvcsw=7 nivcsw=0`
`[  579.534320] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=2`
`[  579.535462] INFO: foo count 7 on core 2: nvcsw=8 nivcsw=0`
`[  579.536306] INFO: foo count 8 on core 2: nvcsw=9 nivcsw=0`
`[  579.537304] INFO: foo count 9 on core 2: nvcsw=10 nivcsw=0`
`[  579.538311] INFO: foo count 10 on core 2: nvcsw=11 nivcsw=0`
`[  579.539305] INFO: foo count 11 on core 2: nvcsw=12 nivcsw=0`
`[  579.539313] INFO: foo count 9 on core 3: nvcsw=10 nivcsw=0`
`[  579.544519] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[  579.547610] INFO: foo count 10 on core 3: nvcsw=11 nivcsw=0`
`[  579.549726] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=3`
`[  579.549733] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[  579.550305] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[  579.551310] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[  579.552305] INFO: foo count 6 on core 1: nvcsw=7 nivcsw=1`
`[  579.553304] INFO: foo count 7 on core 1: nvcsw=8 nivcsw=1`
`[  579.554303] INFO: foo count 8 on core 1: nvcsw=9 nivcsw=1`
`[  579.555406] INFO: foo count 9 on core 1: nvcsw=10 nivcsw=1`
`[  579.556314] INFO: foo count 11 on core 3: nvcsw=12 nivcsw=1`
`[  579.556906] INFO: foo count 12 on core 2: nvcsw=13 nivcsw=1`
`[  579.557308] INFO: foo count 13 on core 2: nvcsw=14 nivcsw=1`
`[  579.557314] INFO: foo count 12 on core 3: nvcsw=13 nivcsw=1`
`[  579.558305] INFO: foo count 14 on core 2: nvcsw=15 nivcsw=1`
`[  579.558311] INFO: foo count 13 on core 3: nvcsw=14 nivcsw=1`
`[  579.559304] INFO: foo count 15 on core 2: nvcsw=16 nivcsw=1`
`[  579.559310] INFO: foo count 14 on core 3: nvcsw=15 nivcsw=1`
`[  579.560304] INFO: foo count 16 on core 2: nvcsw=17 nivcsw=1`
`[  579.560309] INFO: foo count 15 on core 3: nvcsw=16 nivcsw=1`
`[  579.561310] INFO: foo count 17 on core 2: nvcsw=18 nivcsw=1`
`[  579.561316] INFO: foo count 16 on core 3: nvcsw=17 nivcsw=1`
`[  579.561832] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=4`
`[  579.566344] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=4`

Then, for 100 us ------

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 100000`
`[ 7539.472848] rptimer module loaded.`
`[ 7539.477183] INFO: rptimer starting timer`
`[ 7539.477191] INFO: function foo was executed on core 0`
`[ 7539.477205] INFO: function foo was executed on core 2`
`[ 7539.477211] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[ 7539.481384] INFO: function foo was executed on core 1`
`[ 7539.481774] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=0`
`[ 7539.483458] INFO: function foo was executed on core 3`
`[ 7539.483477] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[ 7539.486599] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=0`
`[ 7539.487690] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=0`
`[ 7539.490650] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=1`
`[ 7539.491768] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=0`
`[ 7539.491776] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=1`
`[ 7539.491909] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=1`
`[ 7539.492007] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=1`
`[ 7539.492106] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=1`
`[ 7539.492207] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=1`
`[ 7539.492308] INFO: foo count 6 on core 2: nvcsw=7 nivcsw=1`
`[ 7539.492420] INFO: foo count 7 on core 2: nvcsw=8 nivcsw=1`
`[ 7539.492510] INFO: foo count 8 on core 2: nvcsw=9 nivcsw=1`
`[ 7539.492619] INFO: foo count 9 on core 2: nvcsw=10 nivcsw=1`
`[ 7539.492720] INFO: foo count 10 on core 2: nvcsw=11 nivcsw=1`
`[ 7539.492817] INFO: foo count 11 on core 2: nvcsw=12 nivcsw=1`
`[ 7539.492913] INFO: foo count 12 on core 2: nvcsw=13 nivcsw=1`
`[ 7539.493011] INFO: foo count 13 on core 2: nvcsw=14 nivcsw=1`
`[ 7539.493111] INFO: foo count 14 on core 2: nvcsw=15 nivcsw=1`
`[ 7539.493217] INFO: foo count 15 on core 2: nvcsw=16 nivcsw=1`
`[ 7539.493312] INFO: foo count 16 on core 2: nvcsw=17 nivcsw=1`
`[ 7539.493419] INFO: foo count 17 on core 2: nvcsw=18 nivcsw=1`
`[ 7539.493516] INFO: foo count 18 on core 2: nvcsw=19 nivcsw=1`
`[ 7539.493616] INFO: foo count 19 on core 2: nvcsw=20 nivcsw=1`
`[ 7539.493721] INFO: foo count 20 on core 2: nvcsw=21 nivcsw=1`
`[ 7539.493818] INFO: foo count 21 on core 2: nvcsw=22 nivcsw=1`
`[ 7539.493911] INFO: foo count 22 on core 2: nvcsw=23 nivcsw=1`
`[ 7539.494006] INFO: foo count 23 on core 2: nvcsw=24 nivcsw=1`
`[ 7539.494112] INFO: foo count 24 on core 2: nvcsw=25 nivcsw=1`
`[ 7539.494147] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=1`
`[ 7539.494214] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[ 7539.494308] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[ 7539.494406] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[ 7539.494504] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[ 7539.494604] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[ 7539.494704] INFO: foo count 6 on core 1: nvcsw=7 nivcsw=1`
`[ 7539.494804] INFO: foo count 7 on core 1: nvcsw=8 nivcsw=1`
`[ 7539.494904] INFO: foo count 8 on core 1: nvcsw=9 nivcsw=1`
`[ 7539.495005] INFO: foo count 9 on core 1: nvcsw=10 nivcsw=1`
`[ 7539.495106] INFO: foo count 10 on core 1: nvcsw=11 nivcsw=1`
`[ 7539.495211] INFO: foo count 11 on core 1: nvcsw=12 nivcsw=1`
`[ 7539.495317] INFO: foo count 12 on core 1: nvcsw=13 nivcsw=1`
`[ 7539.495418] INFO: foo count 13 on core 1: nvcsw=14 nivcsw=1`
`[ 7539.495513] INFO: foo count 14 on core 1: nvcsw=15 nivcsw=1`
`[ 7539.495609] INFO: foo count 15 on core 1: nvcsw=16 nivcsw=1`
`[ 7539.495705] INFO: foo count 16 on core 1: nvcsw=17 nivcsw=1`
`[ 7539.495805] INFO: foo count 17 on core 1: nvcsw=18 nivcsw=1`
`[ 7539.495905] INFO: foo count 18 on core 1: nvcsw=19 nivcsw=1`
`[ 7539.496009] INFO: foo count 19 on core 1: nvcsw=20 nivcsw=1`
`[ 7539.496112] INFO: foo count 20 on core 1: nvcsw=21 nivcsw=1`
`[ 7539.496207] INFO: foo count 21 on core 1: nvcsw=22 nivcsw=1`
`[ 7539.496305] INFO: foo count 22 on core 1: nvcsw=23 nivcsw=1`
`[ 7539.496410] INFO: foo count 23 on core 1: nvcsw=24 nivcsw=1`
`[ 7539.496509] INFO: foo count 24 on core 1: nvcsw=25 nivcsw=1`
`[ 7539.496614] INFO: foo count 25 on core 1: nvcsw=26 nivcsw=1`
`[ 7539.496705] INFO: foo count 26 on core 1: nvcsw=27 nivcsw=1`
`[ 7539.496810] INFO: foo count 27 on core 1: nvcsw=28 nivcsw=1`
`[ 7539.496907] INFO: foo count 28 on core 1: nvcsw=29 nivcsw=1`
`[ 7539.497004] INFO: foo count 29 on core 1: nvcsw=30 nivcsw=1`
`[ 7539.497107] INFO: foo count 30 on core 1: nvcsw=31 nivcsw=1`
`[ 7539.497219] INFO: foo count 31 on core 1: nvcsw=32 nivcsw=1`
`[ 7539.497331] INFO: foo count 32 on core 1: nvcsw=33 nivcsw=1`
`[ 7539.502468] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=1`
`[ 7539.502511] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=1`
`[ 7539.503059] INFO: foo count 25 on core 2: nvcsw=26 nivcsw=2`
`[ 7539.508118] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=1`
`[ 7539.513462] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=1`
`[ 7539.514517] INFO: foo count 26 on core 2: nvcsw=27 nivcsw=3`
`[ 7539.514609] INFO: foo count 27 on core 2: nvcsw=28 nivcsw=3`
`[ 7539.514706] INFO: foo count 28 on core 2: nvcsw=29 nivcsw=3`
`[ 7539.514805] INFO: foo count 29 on core 2: nvcsw=30 nivcsw=3`
`[ 7539.514906] INFO: foo count 30 on core 2: nvcsw=31 nivcsw=3`
`[ 7539.515005] INFO: foo count 31 on core 2: nvcsw=32 nivcsw=3`
`[ 7539.515108] INFO: foo count 32 on core 2: nvcsw=33 nivcsw=3`
`[ 7539.515205] INFO: foo count 33 on core 2: nvcsw=34 nivcsw=3`
`[ 7539.515318] INFO: foo count 34 on core 2: nvcsw=35 nivcsw=3`
`[ 7539.515422] INFO: foo count 35 on core 2: nvcsw=36 nivcsw=3`
`[ 7539.515522] INFO: foo count 36 on core 2: nvcsw=37 nivcsw=3`
`[ 7539.515623] INFO: foo count 37 on core 2: nvcsw=38 nivcsw=3`
`[ 7539.515716] INFO: foo count 38 on core 2: nvcsw=39 nivcsw=3`
`[ 7539.515808] INFO: foo count 39 on core 2: nvcsw=40 nivcsw=3`
`[ 7539.515907] INFO: foo count 40 on core 2: nvcsw=41 nivcsw=3`
`[ 7539.516018] INFO: foo count 41 on core 2: nvcsw=42 nivcsw=3`
`[ 7539.516108] INFO: foo count 42 on core 2: nvcsw=43 nivcsw=3`
`[ 7539.516206] INFO: foo count 43 on core 2: nvcsw=44 nivcsw=3`
`[ 7539.516307] INFO: foo count 44 on core 2: nvcsw=45 nivcsw=3`
`[ 7539.516417] INFO: foo count 45 on core 2: nvcsw=46 nivcsw=3`
`[ 7539.516522] INFO: foo count 46 on core 2: nvcsw=47 nivcsw=3`
`[ 7539.516615] INFO: foo count 47 on core 2: nvcsw=48 nivcsw=3`
`[ 7539.516711] INFO: foo count 48 on core 2: nvcsw=49 nivcsw=3`
`[ 7539.516819] INFO: foo count 49 on core 2: nvcsw=50 nivcsw=3`
`[ 7539.516908] INFO: foo count 50 on core 2: nvcsw=51 nivcsw=3`
`[ 7539.517006] INFO: foo count 51 on core 2: nvcsw=52 nivcsw=3`
`[ 7539.517107] INFO: foo count 52 on core 2: nvcsw=53 nivcsw=3`
`[ 7539.517205] INFO: foo count 53 on core 2: nvcsw=54 nivcsw=3`
`[ 7539.517308] INFO: foo count 54 on core 2: nvcsw=55 nivcsw=3`
`[ 7539.517406] INFO: foo count 55 on core 2: nvcsw=56 nivcsw=3`
`[ 7539.517856] INFO: foo count 56 on core 2: nvcsw=57 nivcsw=3`
`[ 7539.518259] INFO: foo count 57 on core 2: nvcsw=58 nivcsw=3`
`[ 7539.518308] INFO: foo count 58 on core 2: nvcsw=59 nivcsw=3`
`[ 7539.518414] INFO: foo count 59 on core 2: nvcsw=60 nivcsw=3`
`[ 7539.518506] INFO: foo count 60 on core 2: nvcsw=61 nivcsw=3`
`[ 7539.518956] INFO: foo count 61 on core 2: nvcsw=62 nivcsw=3`
`[ 7539.524908] INFO: foo count 33 on core 1: nvcsw=34 nivcsw=2`
`[ 7539.530305] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=2`
`[ 7539.530314] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=2`

`ocrom04@rpi4b:~/repos/l1 $ sudo ./fscript mt_rptimer 0 100000`
`[  623.650043] rptimer module loaded.`
`[  623.656160] INFO: function foo was executed on core 0`
`[  623.661423] INFO: foo count 0 on core 0: nvcsw=1 nivcsw=0`
`[  623.661434] INFO: function foo was executed on core 1`
`[  623.667228] INFO: rptimer starting timer`
`[  623.667251] INFO: function foo was executed on core 2`
`[  623.667268] INFO: foo count 0 on core 2: nvcsw=1 nivcsw=0`
`[  623.672137] INFO: foo count 1 on core 0: nvcsw=2 nivcsw=1`
`[  623.672144] INFO: foo count 1 on core 2: nvcsw=2 nivcsw=0`
`[  623.672217] INFO: foo count 0 on core 1: nvcsw=1 nivcsw=0`
`[  623.676207] INFO: foo count 2 on core 2: nvcsw=3 nivcsw=0`
`[  623.676215] INFO: foo count 2 on core 0: nvcsw=3 nivcsw=1`
`[  623.676645] INFO: function foo was executed on core 3`
`[  623.676661] INFO: foo count 0 on core 3: nvcsw=1 nivcsw=0`
`[  623.676854] INFO: foo count 1 on core 3: nvcsw=2 nivcsw=0`
`[  623.678262] INFO: foo count 2 on core 3: nvcsw=3 nivcsw=0`
`[  623.678372] INFO: foo count 3 on core 3: nvcsw=4 nivcsw=0`
`[  623.678465] INFO: foo count 4 on core 3: nvcsw=5 nivcsw=0`
`[  623.682091] INFO: foo count 5 on core 3: nvcsw=6 nivcsw=0`
`[  623.688003] INFO: foo count 3 on core 0: nvcsw=4 nivcsw=1`
`[  623.692418] INFO: foo count 3 on core 2: nvcsw=4 nivcsw=1`
`[  623.692555] INFO: foo count 4 on core 2: nvcsw=5 nivcsw=1`
`[  623.694881] INFO: foo count 1 on core 1: nvcsw=2 nivcsw=1`
`[  623.694950] INFO: foo count 2 on core 1: nvcsw=3 nivcsw=1`
`[  623.695054] INFO: foo count 3 on core 1: nvcsw=4 nivcsw=1`
`[  623.695161] INFO: foo count 4 on core 1: nvcsw=5 nivcsw=1`
`[  623.695255] INFO: foo count 5 on core 1: nvcsw=6 nivcsw=1`
`[  623.695352] INFO: foo count 6 on core 1: nvcsw=7 nivcsw=1`
`[  623.695450] INFO: foo count 7 on core 1: nvcsw=8 nivcsw=1`
`[  623.696168] INFO: foo count 8 on core 1: nvcsw=9 nivcsw=1`
`[  623.698946] INFO: foo count 9 on core 1: nvcsw=10 nivcsw=1`
`[  623.704711] INFO: foo count 6 on core 3: nvcsw=7 nivcsw=1`
`[  623.709952] INFO: foo count 4 on core 0: nvcsw=5 nivcsw=2`
`[  623.714762] INFO: foo count 10 on core 1: nvcsw=11 nivcsw=2`
`[  623.714885] INFO: foo count 11 on core 1: nvcsw=12 nivcsw=2`
`[  623.724271] INFO: foo count 7 on core 3: nvcsw=8 nivcsw=2`
`[  623.731182] INFO: foo count 5 on core 0: nvcsw=6 nivcsw=3`
`[  623.731191] INFO: foo count 12 on core 1: nvcsw=13 nivcsw=4`
`[  623.731373] INFO: foo count 5 on core 2: nvcsw=6 nivcsw=2`

There are a few things that are interesting to notice, or to take note
of, here.

-   As the timer period decreases, the misalignment between the cores
    increases. This is because each core may experience a different
    amount of contention.
-   The cores do not experience a large change in `ivcsw` events per
    timer expiration over different periods. They do experience a large
    per second.
-   Involuntary sleeps are far longer in duration with a *longer* period
    than with a *shorter* period, yet proportionally they affect clock
    alignment less.
-   Long periods of silence from any core correspond always to an
    increment of `nicsw`: in other words, to an involuntary context
    switch.
-   There are sometimes instances where, even without an involuntary
    context switch, the time difference between two repeats of the same
    core is too long.

1.  The timing does not vary much between the intervals outside of an
    involuntary context switch. At an involuntary context switch, there
    is no difference for the largest timer periods and an astronomical
    difference for the smallest. There are some occasions where there is
    no involuntary context switch and still a jump that is several
    periods long. This is probably due to other interrupts from
    unrelated processes preempting the softirq.

2.  For periods of a similar scale, there were similar amounts of
    involuntary context switches per voluntary context switch. There was
    not a large difference per unit time either, though the
    multi-threaded case may have had slightly more. This is unexpected,
    as more work was being done total, and the multi-threaded module did
    not 'spare' any cores.

3.  In the runs I did, the amount of involuntary context switches per
    voluntary context switch did not change much (this is different from
    the results of the single-threaded module), but far, far more
    involuntary context switches per unit time occurred. The biggest
    jump is from 100ms to 1ms. In every case, the number of voluntary
    context switches per unit time was directly correlated to frequency
    (increasing proportionally as period decreased).

System Behavior
---------------

In the `trace-cmd` output and in KernelShark, the thread name
`my foo thread on core N` is truncated to `my foo thread o`.

I ran `$ sudo insmod mt_rptimer log_sec=0 log_nsec=1000000`. I saved
this in `trace.dat`. The behavior is verified by the distance in time
between two consecutive voluntary context switches from
`my foo thread o` in the same core, when there are no involuntary
context switches between.

System Performance
------------------

1.  My threads do not run to completion every time. As seen in the
    kernel log, there are involuntary interrupts. We can also see these
    take place in KernelShark wherever
    `Info="my foo thread o:PID [120] R ==> otherproc:PID [120]"`
    (keyword `R`).

2.  This is a preemptible kernel, so anything that needs to be run of a
    similar priority can be scheduled in place of our module's kernel
    thread. In my `trace.dat`, the main processes preempting my kernel
    thread are `tailscaled`, `kworker/u8:3`, `rcu_preempt`, and
    `systemd_journal`.

While it was fun to verify behavior in a tighter timer period, measuring
the performance in the way desired requires a larger period. Otherwise,
it is too difficult to find a timer expiration that all 4 kernel threads
are prepared for (for example, if one is inactive because it is waiting
on a `printk`, it is neither executing nor wakeable and it will miss
that expiration signal---this is seen in our system log excerpts in the
occasions where a count takes more than one period without an
involuntary context switch and is expected). I ran
`$ sudo insmod mt_rptimer log_sec=1 log_nsec=0` and saved this in
`trace2.dat`. To measure the performance, I take the lengths in time of
three wakeups, from wakeup to final-core voluntary block.

Wakeup 1 : `A to B = 0.023_665_685 s`
Wakeup 2 : `A to B = 0.023_078_926 s`
Wakeup 3\*: `A to B = 0.023_248_444 s`

These intervals denote the time it takes for `foo` to run through a
single loop and voluntarily block. The interval is from the first
preemption of another process by `my foo thread o` to the voluntary
block by `foo` on the final of the four cores. Wakeup 3 is pictured.

Next, I'll measure jitter using the same `trace2.dat` as before. Here
are three measurements of the variation in wakeup between the initial
and final core to do so.

Jitter 1 : `A to B = 0.000_006_500 s`
Jitter 2 : `A to B = 0.000_014_537 s`
Jitter 3\*: `A to B = 0.000_007_074 s`

These intervals denote the time that passes between the wakeup of the
`my foo thread o` instance on the first core to respond and the wakeup
of the `my foo thread o` instane on the final core to respond.

Now, in order to find the max, min, and avg time for a particular thread
to loop on one particular core, I run
`$ sudo trace-cmd report | grep "my foo thread o" | grep '\[000\]' > report_core0.txt`.
Here is what I find:

Maximum : `0.023324 s`
Minimum : `0.000068 s`
Average : `0.008084 s`
