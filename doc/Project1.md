# Project 1

* Assigned: 2021-03-09 Tuesday 17:00:00 KST
* **Due: 2021-03-22 Monday 20:59:59 KST**

## Introduction

In this project, you will implement a new system call `ptree`.

Linux has a unique hierarchy between processes. All processes are descendants of the `init` process, which is started by the kernel in the course of booting. Every process except `init` has exactly one parent and zero or more childern. In this sense, the hierarchy of the processes on Linux can be viewed as a tree. The new system call `ptree` is to traverse the process familiy tree in [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR) from the root (`init`) and return the information of the processes running on the system.

This is an **individual** project. As we did in the project 0, we will provide an invitation link for project 1. When you accept the invitation, your own private project repository will be created from our kernel source repository. We will use this repository for grading. In addition to the final code, a `README.md` document that illustrates your implementation should be committed to the **master** branch of your repository. Again, remember that the deadline is hard. All project repositories will be cloned at once exactly at the time of the deadline and no further commits will be pulled thereafter.

## 1. Implement the `ptree` system call (45 pts.)

`ptree` takes two arguments and returns an array of `pinfo`s along with the number of valid `pinfo`s in the array.

The function prototype of `sys_ptree` is:
```c
ssize_t sys_ptree(struct pinfo *buf, size_t len);
```

The definition of `struct pinfo` is:
```c
struct pinfo {
  int64_t       state;       /* current state of the process */
  pid_t         pid;         /* process id */
  int64_t       uid;         /* user id of the process owner */
  char          comm[64];    /* name of the program executed */
  unsigned int  depth;       /* depth of the process in the process tree */
};
```
You should define `struct pinfo` in `include/linux/pinfo.h` as a part of your solution.

The first argument `buf` points to a user-space buffer for `pinfo`s, and the second argument `len` is the length of the buffer, i.e., the number of the allocated `pinfo` entries in the buffer.
A pointer to the `task_struct` of `init` is globally defined inside the kernel: `init_task`. Thus, starting from the root process (`init_task`), `ptree` traverses the process tree in pre-order and fills in the buffer _in the order of traversal_ with the information of the processes existing on the system.
If the number of the processes exceeds `len`, the information of the first `len` processes is copied to the buffer.
If not, the information of all the processes is copied.
The return value of `ptree` indicates the number of `pinfo`s actually copied to the buffer.
Thus, when reading the buffer after the system call, you can check this value to bound the indices that you access.

Note that the `depth` of the root process is 0. Obviously, the `depth` of any other process will be a positive value.

Every system call must be assigned a syscall number. In this project, the syscall number of `ptree` is **399**.

### Return value

We mentioned that the return value of `ptree` is the number of `pinfo` entries copied to the buffer.
This is when the system call was successful.
In case an error occured during the system call, `ptree` should return an appropriate `errno` error code.

For example, `ptree` should be able to detect and report the following error conditions:
* `-EINVAL`: if `buf` is NULL or if `len` is 0.
* `-EFAULT`: if `buf` is outside the accessible address space.

The referenced error codes are defined in `include/uapi/asm-generic/errno-base.h`

### tasklist_lock

Linux maintains all processes in a doubly linked list of `task_struct`s. In order to ensure consistency, it is necessary to prevent the list from being changed during the traversal of the process tree. For this purpose the kernel introduces a special lock, named `tasklist_lock`. You should acquire this lock before starting the traversal, and release the lock only when the traversal is completed. Make sure that while holding the lock the kernel thread never goes to sleep. Allocating memory or copying data into and out of the kernel may put the current thread to sleep, and may result in deadlocks on the lock you are holding.

Use the following code to acquire and release the lock (make sure your code includes the required header file):
```c
#include <linux/sched/task.h>

read_lock(&tasklist_lock);
/* do the job... */
read_unlock(&tasklist_lock);
```

## 2. Test your new system call (10 pts.)

Write a simple C program `test_ptree.c` which calls the `ptree` system call and prints the result. The program takes an integer command-line argument (through `argv[1]`), and invokes `ptree` with a buffer of length `argv[1]`. After the system call, it prints the obtained process tree, using tabs to indent the children from their parents.

Use the following format for the program output:

```
printf("%s, %d, %lld, %lld\n", p.comm, p.pid, p.state, p.uid);
```

If an error occurs during the system call, `test_ptree.c` should instead print an appropriate message (to stdout) according to the error type. The message should include one of the kernel-defined error code name (in this project, either `EINVAL` or `EFAULT`).

As you did in the previous project, you can invoke the `ptree` system call using `syscall` (See [here](https://linux.die.net/man/2/syscall) for details).

### Example program output

```
swapper/0, 0, 0, 0
	systemd, 1, 0, 0
		dbus-daemon, 160, 0, 81
		systemd-journal, 163, 1, 0
		systemd-udevd, 176, 1, 0
			systemd-udevd, 443, 1, 0
			systemd-udevd, 445, 1, 0
			systemd-udevd, 447, 1, 0
		actd, 239, 1, 0
		buxton2d, 241, 1, 375
		key-manager, 242, 1, 444
		dlog_logger, 244, 1, 1901
		amd, 245, 1, 301
		alarm-server, 246, 1, 301
		bt-service, 248, 1, 551
		cynara, 249, 1, 401
		deviced, 253, 1, 0
		esd, 258, 1, 301
		license-manager, 259, 1, 402
		resourced-headl, 267, 1, 0
```

**Save your C program as: `test/test_ptree.c` in your kernel repository.**

## Submission

Submit the following to the **master** branch of your repository:  
* Kernel code
* Test code (`test/test_ptree.c`)
* README doc (You may replace the existing README)

Note that any commits made after the deadline will not be considered for grading.

## We're here to help you

Any trouble? Questions on the [issue board](https://github.com/swsnu/osspr2021/issues) are more than welcome.

Start early, and more importantly, have fun!