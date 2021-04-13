# Project 3: Hello, Scheduler!

* Assigned: 2021-04-13 Tuesday 17:00:00 KST
* Design Review Deadline: 2021-04-30 Friday KST
* **Submission Deadline: 2021-05-14 Friday 20:59:59 KST**


## Introduction

In this project, you will implement a weighted round-robin (WRR) scheduler in the Tizen Linux kernel.

This is a **team project**.
Accept the Github classroom assignment invitation.
Each team will get access to its own GitHub repository (e.g. swsnu/project3-hello-scheduler-team-n) for collaboration and submission.
Commit your final code and `README.md` to the *master* branch for submission.


## 1. Implement a weighted round-robin (WRR) scheduler (60 pts.)

Add a WRR scheduler in the Tizen Linux kernel, which works on a symmetric multiprocessing (SMP) system.
The new scheduler will completely replace the CFS scheduler, and orchestrate every *normal* task (i.e. user processes and kernel threads with either `SCHED_NORMAL` or `SCHED_BATCH` policy) including `swapper`, `systemd`, and `kthreadd`.


### WRR scheduling policy
The WRR policy is defined as follows:

- The scheduler basically follows the round-robin (RR) policy. Each task is given a time slice. When a task is scheduled, the task runs on a CPU until it uses up its time slice, and then is preempted. The preempted task is scheduled again after all the other runnable tasks are scheduled once.
- Each task also has a *weight*. A task's time slice is the product of the task's weight and the system's base time slice (**10 ms**).
- The weight of a task is between **1 and 20 (both sides inclusive)**. The default weight is **10**, i.e. the weight of the first task in the system (`swapper`) is 10 and its time slice is 100 ms unless altered after booting. Tasks newly assigned to the WRR scheduler through the `sched_setscheduler` system call are also given the default weight. 
- When a task is created, it inherits the WRR weight, but not the time slice, from its parent. A task's weight can be queried and updated by the system calls `sched_getweight` and `sched_setweight`. These are custom system calls that you should implement. Details are given below.
-  When a task is assigned a CPU to run on, a CPU with the smallest *total weight* is selected. The total weight of a CPU is the sum of the weights of the all WRR tasks assigned to the CPU.
-  The time slice of a task is initialized at the creation time, and refilled/recalculated only after the task uses up its remaining time slice. For example, changing the weight of a running task does not affect its preemption time.


### Load balancing

As with the CFS scheduler, the WRR scheduler periodically performs load balancing between CPUs.
The scheduler takes the following steps every **2000 ms**:

1) The scheduler selects one CPU with the largest total weight and another CPU with the smallest total weight. Let's call the CPUs `max_cpu` and `min_cpu` respectively.
2) Among the WRR tasks assigned to `max_cpu`, the scheduler searches for *transferable* tasks. Here, a WRR task is transferable if 1) it is not running on the CPU, 2) its CPU affinity allows moving the task to `min_cpu`, and 3) moving it from `max_cpu` to `min_cpu` does not make the total weight of `min_cpu` *equal to or greater than* that of `max_cpu`.
3) If a transferable task exists, the scheduler selects a transferable task with the largest weight and moves it from `max_cpu` to `min_cpu`. If not, the scheduler skips load balancing and retries next time.

Whenever a WRR task is migrated by load balancing, you should log the information in the following format:

```c
#include <linux/jiffies.h>

printk(KERN_DEBUG "[WRR LOAD BALANCING] jiffies: %Ld\n"
                  "[WRR LOAD BALANCING] max_cpu: %d, total weight: %u\n"
                  "[WRR LOAD BALANCING] min_cpu: %d, total weight: %u\n"
                  "[WRR LOAD BALANCING] migrated task name: %s, task weight: %u\n",
		  (long long)(jiffies),
		  ...);
```

The total weights of `max_cpu` and `min_cpu` here are the weights *before* migration.
You can check the logs using the command `dmesg`.


### Priority between schedulers

The Linux kernel employs multiple schedulers, each of which governs different types of tasks in different algorithms.
For example, the RT scheduler handles real-time tasks, and the CFS scheduler handles normal tasks.
Each scheduler has a priority.
The scheduler with the highest priority at the time takes the chance to schedule one of its tasks to a CPU.
That is, a scheduler with lower priority only gets a chance when all the schedulers with higher priority have no tasks to schedule.
For example, the RT scheduler is always prioritized over the CFS scheduler, and therefore normal tasks in Linux can be processed only when no real-time task is ready for running.

*The WRR scheduler is placed between the RT scheduler and the CFS scheduler.*
In other words, it is prioritized over the CFS scheduler, but not over the RT scheduler.
Although seemingly simple, this raises a tricky problem.
Unlike the RT scheduler, the WRR scheduler is likely to have one or more runnable tasks at every moment.
Accordingly, prioritizing the WRR scheduler over the CFS scheduler can lead to the starvation of the tasks under the CFS scheduler.
Consider the case where some important kernel threads are assigned to the CFS scheduler and never executed.
The system will easily crash!

To settle this issue, we'll have the WRR scheduler *completely replace* the CFS scheduler.
Specifically,

- Every normal task is assigned to the WRR scheduler. The CFS scheduler will never have any kind of task at any point of time.
- Changing a task's scheduler to the CFS scheduler through `sched_setscheduler` is redirected: the task is instead sent to the WRR scheduler.

**We uploaded [a patch](https://github.com/swsnu/osspr2021/blob/main/patch/proj3-1.patch) that implements the second bullet. Apply the patch to the kernel before you start.**
Refer to [this issue](https://github.com/swsnu/osspr2021/issues/7) on how to apply patches.

You are free to take your own non-hacky approach instead of using the patch we provided.
If this is the case, please describe your approach in `README.md`.


### System calls to set/get WRR weights

Add two system calls, `sched_setweight` and `sched_getweight`, for updating and querying the WRR weight of a task.

```c
/*
 * Update the WRR weight of a task.
 * Syscall number 399.
 * 
 * Args:
 * 	pid: target task's PID. PID 0 indicates the calling task.
 *  	weight: the new weight.
 * 
 * Returns:
 * 	On success, return 0.
 * 	Otherwise, return an error code.
 */
long sched_setweight(pid_t pid, unsigned int weight);

/*
 * Query the WRR weight of a task.
 * Syscall number 400.
 * 
 * Args:
 * 	pid: target task's PID. PID 0 indicates the calling task.
 * 
 * Returns:
 * 	On success, return the queried weight value.
 * 	Otherwise, return an error code.
 */
long sched_getweight(pid_t pid);
```

Only the administrator (the `root`) and the owner of a task are allowed to change the task's weight using `sched_setweight`.
Furthermore, increasing the weight of a task should only be done by the administrator.
In contrast, it is allowed for any user to query the WRR weight of a task using `sched_getweight`.

If an error occurs, the system calls should return an appropriate error code.
Possible error conditions and the corresponding error codes are as follows:

- `ESRCH`: If the task with the given PID does not exist.
- `EINVAL`: If the task with the given PID is not under the `SCHED_WRR` policy. If the given weight is out of the valid range [1, 20].
- `EPERM`: If the calling task does not have permission to give the task the new weight.

**As before, we assume in this project that each task has a unique PID. Each process will have a single thread of execution.**


### Implementation guide

Linux realizes the schedulers (e.g., RT, CFS) as *scheduling classes*.
In addition, Linux has a set of *scheduling policies*, which hints how to schedule and preempt the tasks in the scheduling classes.
For this project, you will create a new scheduling class `wrr_sched_class` and a new scheduling policy `SCHED_WRR`.
Define your own data structure to handle the tasks assigned to the WRR scheduler, and incorporate the scheduler into the core scheduling logic.
Since the Linux scheduler is designed in a modular manner, you should first understand the structure of `sched_class` and how its methods (e.g., `enqueue_task`) are used in the core scheduling logic (i.e., `kernel/sched/core.c`).
We also recommend you to refer to the implementations of `fair_sched_class` (in `kernel/sched/fair.c`) and `rt_sched_class` (in `kernel/sched/rt.c`).

Note that it is not necessary nor possible to read every single line of code in the Linux scheduler for this project.
One major goal of this project is to learn how to get a high-level view of the code and differentiate important parts from unimportant ones.
Begin by reading textbooks and documents on the Linux scheduler.
After getting a high-level picture, you will realize that the actual amount of code you should focus on is not that large.
You will also understand that you do not have to implement all of the methods in `wrr_sched_class`: more than half of the methods can be left empty or `NULL`.

As the scheduler will run on a SMP system, you should care about concurrency (a good news is that we will not test your code on a single-core machine).
Carefully review how the existing schedulers protect their own data structures (e.g., `struct rt_sched_entity`) and the objects shared with other schedulers (e.g., `struct rq`). 
You may need to study [RCU](https://en.wikipedia.org/wiki/Read-copy-update) and other synchronization methods used by the schedulers.


## 2. Write a test program and plot the result (10 pts.)

Write a test program that calculates the prime factorization of a number with the naïve trial division method.
Execute this program with different WRR weights (from 1 to 20) and **plot** how the turnaround time of the program changes.
Then, breifly explain your results in `README.md` (*you should pick a sufficiently large number to see the trend*).

As usual, your test program(s) and Makefile should be located in the `test` directory.
The complete set of result data and the graph should be also found there.


## 3. Ensure code quality (5 pts.)

In this project, 5 points are allocated to assess your code quality.
Write clean and readable code with informative comments.
Specifically,

- Attach brief comments to the functions you implemented
- Explain why you do (or do not) acquire/release certain locks
- Delete redundant parts of your code (e.g. unused utility functions, bulk-commented code, `printk` invocations for debugging)

Note that we do not aim to deduct your points here.
We will deduct points if we unconsciously say "Oh gosh..." while reading your code.
We expect that most of the teams will get full credit.
Do not spend too much time in code refactoring for obtaining the points.


## Tips - use debugfs to debug your scheduler

In this project, [debugfs](https://www.kernel.org/doc/Documentation/filesystems/debugfs.txt) will be a great tool for your debugging.
We have already mounted debugfs and enabled the `CONFIG_SCHED_DEBUG` and `CONFIG_SCHEDSTATS` options.
Modify `kernel/sched/debug.c` and `kernel/sched/stats.c`, and then you can query the current state and statistics of the WRR scheduler using `cat /proc/sched_debug` or `cat /proc/schedstat`.


## Submission

- Code
  - Commit to the master branch of your team repo.
	- The `README.md` should describe your implementation, how to build your kernel (if you changed anything), and lessons learned.
- Slides and Demo (`n` is your team number)
	- Email: osspr2021@gmail.com
	- Title: [Project 3] Team `n`
	- Attachments: team-`n`-slides.{ppt.pdf}, team-`n`-demo.{mp4,avi,…}
	- One slide file, one demo video!


## We're here to help you

Any trouble? Questions on the [issue board](https://github.com/swsnu/osspr2021/issues) are more than welcome.
We also highly encourage discussions between students.
Start early, be collaborative, and most importantly, have fun!
