# Project 2: Hello, Synchronization!

* Assigned: 2021-03-23 Tuesday 17:00:00 KST
* Design Review Due: 2021-04-05 Monday
* **Due: 2021-04-12 Monday 20:59:59 KST**


## Introduction

In this project, you will implement a new synchronization primitive that provides a reader-writer lock that changes its behavior based on the device's current orientation.

This is a **team project**.
Accept the Github classroom assignment invitation.
If you are the first team member to visit the assignment invitation, you will have to *create* your team. Name your team `Team %d`.
Each team will get access to its own GitHub repository (e.g. swsnu/project2-hello-synchronization-team-n) for collaboration and submission.
Commit your final code and `README.md` to the *master* branch for submission.


## Setting the device orientation

A physical Tizen device can detect its 3D orientation through its on-board [orientation sensor](https://developer.tizen.org/development/guides/native-application/location-and-sensors/device-sensors#orientation).
In this semester, however, we're working on QEMU to simulate a Tizen device.
Thus, we will just assume a 1D device orientation, stored as an integer variable (0 to 359) in the kernel.

Without an orientation sensor, we will need a way to set the current orientation of the device.
For this, we will introduce a new system call `set_orientation`.
The specification is as follows:

```c
/*
 * Set the current device orientation. Syscall number 399.
 *
 * Args:
 *   degree: The degree to set as the current device orientation.
 *       Value must be in the range 0 <= degree < 360.
 *
 * Returns:
 *   Zero on success, -EINVAL on invalid argument.
 *
 */
long set_orientation(int degree);
```


## Rotation-based reader/writer locks (45 pts.)

Now, let's dive into the details of our new synchronization primitive: *Rotation locks*.
Rotation locks provide reader and writer locks that synchronize access to a protected object.
Protected objects may be anything that the user processes agree upon (e.g., a file at a specific path).

Although we will elaborate in later sections, in a nutshell, users of the rotation lock specify the **ranges of device orientation** (e.g., from degree 0 to 180) where they wish to obtain access to the protected object.
Then, we wish to give processes access *only* when the current device orientation is inside the degree ranges declared by the processes.

The API for this synchronization mechanism is exposed as the following two system calls:

```c
/*
 * Readers and writers.
 *
 */
#define ROT_READ  0
#define ROT_WRITE 1

/*
 * Claim read or write access in the specified degree range.
 * Syscall number 400.
 *
 * Args:
 *   low: The beginning of the degree range (inclusive).
 *       Value must be in the range 0 <= low < 360.
 *   high: The end of the degree range (inclusive).
 *       Value must be in the range 0 <= high < 360.
 *   type: The type of the access claimed.
 *       Value must be either ROT_READ or ROT_WRITE.
 *
 * Returns:
 *   On success, returns a non-negative lock ID that is unique
 *   for each call to rotation_lock. On invalid argument,
 *   returns -EINVAL.
 *
 */
long rotation_lock(int low, int high, int type);

/*
 * Revoke access claimed by a call to rotation_lock.
 * Syscall number 401.
 *
 * Args:
 *   id: The ID associated with the access to revoke.
 *
 * Returns:
 *   Zero on success, -EINVAL on invalid argument, -EPERM when
 *   the process has no permission.
 *
 */
long rotation_unlock(long id);
```

Several rules govern rotation locks. Your implementation must strictly adhere to all of these rules.

### Range-based readers-writer lock

> For each angle, there may either be multiple readers or a single writer.

The readers-writer lock is a classical synchronization method.
As mentioned earlier, processes may ask for either read (`ROT_READ`) or write (`ROT_WRITE`) access to the protected object.
Since readers do not change the value of the protected object, multiple readers can co-exist in the system.
On the other hand, since a writer may change the value of the projected object, it should gain exclusive access to the object.
That is, no readers are allowed to read to object when there is a writer mutating it.
We call this the readers-writer lock condition.

On top of this, we add the notion of *angles* to define the range-based readers-write lock.
Basically, we require that each angle `i`, where `0 <= i < 360`, meet the readers-writer lock condition.

At any time, every angle `i` (`0 <= i < 360`) has one of the three access states:
- Free: No processes have access.
- Read: One or more processes have read access.
- Write: Exactly one process has write access.

This is best illustrated with examples. R[a, b] and W[a, b] respectively denote a process with read and a write access with range [a, b].

- W[10, 20] and W[60, 80] may co-exist in the system since their ranges do not overlap.
- R[50, 70] and R[60, 80] may co-exist in the system even if their ranges overlap.
- R[0, 30] and W[150, 160] may co-exist in the system since their ranges do not overlap.
- W[50, 70] and W[60, 80] may **not** co-exist in the system since their ranges overlap.
- R[50, 60] and W[60, 70] may **not** co-exist in the system since their ranges overlap.
  - Yes, ranges are inclusive for both sides.

### Claiming access: `rotation_lock`

> Access is only given when the device orientation is within the specified range.

When are processes granted access to the protected object? Let us review the signature of `rotation_lock`:

```c
long rotation_lock(int low, int high, int type);
```

`rotation_lock` specifies its degree range [`low`, `high`], with both sides inclusive. The range can wrap around. For instance, [330, 10] specifies the range [330, 360) U [0, 10]. 

Processes are granted the `type` of access they requested when:

1) granting access still meets the range-based readers-writer lock condition stated above, and
2) the device's current orientation is within [`low`, `high`].

The caller of `rotation_lock` is **blocked** until these conditions are met.
Also, the access granted is **not** taken away even when the device's current orientation moves out of the specified range.
Additionally, the call to `rotation_lock` returns a unique ID associated with the call.
This ID should be a non-negative `long` integer that is unique for each call to `rotation_lock`.
Processes will use this ID to revoke the access right given to them.

### Revoking access: `rotation_unlock`

> Access can be revoked at any time, regardless of the device's current orientation.

Now we have a way to claim access. Then, what about revoking (or giving up) access? The signature of `rotation_unlock` is simple:

```c
long rotation_unlock(long id);
```

A process can revoke its own access rights by calling `rotation_unlock` with the ID returned by the call to `rotation_lock`.
Unlike `rotation_lock`, processes may revoke their access rights any time regardless of the device's current orientation, and thus the call to `rotation_unlock` will never block.
Also, a process may only revoke access rights that it owns.
If a process calls `rotation_unlock` with an ID that it does not own, the system call should abort and return `-EPERM`.


### Fairness policy

> Among **in-range** waiting locks, readers whose degree range overlap with that of a waiting writer must not be granted access before the waiting writer.

Just like other synchronization primitives, multiple processes will contend to gain read or write access through our rotation lock.
How can we be *fair*?

There can be many different but plausible definitions to fairness, but let us primarily consider *starvation* in this assignment.
What is starvation?
We say that a process may starve if a request by the process may never be completed, i.e. it's waiting time is unbounded.
In such an unfortunate case, the process will wait forever, starving.

Let me give you an example.
Students in this course post their questions on the issue board, and the TAs answer their questions.
Say that the TAs adopt the following policy: Select the most "urgent" question and answer it first.
This policy is not fair in our sense; it may lead to starvation.
Say that a student asks something about Neovim (which is obviously of *zero* urgency and only Jae-Won is interested in answering it).
If a non-stop flood of super urgent questions (e.g., "The assignment spec is wrong!") arrives, the Neovim question will not be answered until the end of the semester.
In other words, the poor Neovim question starves.

Then for us, what does it mean for a process, either requesting read or write access, to starve?
Recall that a process may be blocked by a call to `rotation_lock` if either the readers-writer condition is not met, or the device orientation is not in the specified range.
For the second condition, we actually *want* processes to sleep forever!
That is, unless the device orientation moves into the degree range specified by a process, we want the process to be blocked indefinitely.
However, we do not want processes to starve due to contentions with other reader or writer processes.

The question of fairness arises when the device's orientation changes (`set_orientation`), when a process requests access (`rotation_lock`), and when access is revoked (`rotation_unlock`).
In all cases, the kernel will have to choose one or more processes to wake up and grant them the requested access rights.
Which processes should be awoken?

I'll give you an example of a naïve and unfair policy: "There are no rules."
Assume that we receive the following series of requests: R[10, 50], W[10, 30], R[0, 30].
The device's current orientation is 20 and there are initially no processes.

1) The first request (R[10, 50]) can be fulfilled, so we do so.
2) The second request (W[10, 30]) cannot be fulfilled since its range overlaps with the first one, so the process is put to sleep.
3) The third request (R[0, 30]) can be fulfilled, so we do so.

Now, imagine that a non-stop flood of R[0, 30] requests arrives and there are always one or more readers inside range [10, 30].
Then, the second request (W[10, 30]) will starve.

This situation is called "writer starvation".
Thus, in this assignment, you will implement the following fairness policy in order to mitigate writer starvation.

> Among **in-range** waiting locks, readers whose degree ranges overlap with that of a waiting writer must not be granted access before the waiting writer.

Again, an example will help.
Say that we have the same situation as the example above: requests R[10, 50], W[10, 30], R[0, 30] with current orientation 20.

1) [Same] The first request (R[10, 50]) can be fulfilled, so we do so.
2) [Same] The second request (W[10, 30]) cannot be fulfilled since its range overlaps with the first one, so the process is put to sleep.
3) [Different] The third request (R[0, 30]) will wait. Among in-range waiting locks (W[10, 30] and R[0, 30]), readers (R[0, 30]) whose degree range overlap with that of a waiting writer (W[10, 30]) must not be granted access before the waiting writer (W[10, 30]).

Eventually R[10, 50] will call `rotation_unlock`.
Then, W[10, 30] will gain access without starving.
Only after W[10, 30] calls `rotation_unlock` can R[0, 30] gain read access.


## Notes on implementation

All machinery related to rotation locks should be placed in `kernel/rotation.c`, and in `include/linux/rotation.h`.

Think carefully about the data structures that you will use to solve this problem.
The data structure you use should be able to hold multiple processes blocking on different degree ranges.
For example, it might be a good idea to keep a list of access requests and implement utility methods for the list, such as `find` and `filter`.

There are many different ways to block processes.
You can use mutexes (`linux/mutex.h`) or semaphores (`linux/semaphore.h`).
If you are brave enough, you can choose to work at the level of wait queues using either the associated low-level routines such as `add_wait_queue()`, `remove_wait_queue()`, or the higher-level routines such as `prepare_to_wait()`, `finish_wait()`.
You can find code examples both in the book (pages 58 - 61 of Linux Kernel Development) and in the kernel.

Finally, but most importantly, make sure to correctly synchronize accesses to your internal data structures.
Remember, we're working on a multicore machine, and races will always happen.


## Test your rotation lock implementation (10 pts.)

You will implement the following test scenario:

- Imagine that the system is deployed on your phone and the phone rotates +30 degrees every two seconds.
- Access to a file named `quiz` will be protected with the rotation lock with range [0, 180], i.e. when your phone is face-up.
- A `professor` process writes a positive integer to `quiz`. It repeats this indefinitely, incrementing the number every iteration.
- A `student` process reads the integer from `quiz` and prints the factorization of the number. It repeats this indefinitely.

For your convenience, we will provide a *rotation daemon* that rotates the device +30 degrees every two seconds, starting from degree zero.
Find the source code [here](https://github.com/swsnu/osspr2021/blob/main/handout/rotd.c).
Save it as `test/rotd.c`, cross-compile it, and copy it into QEMU.
Make sure you have this daemon always running.

Your task is to implement `test/professor.c` and `test/student.c`.

The detailed specification of the programs are as follows:

- **professor:**
`professor` accepts a starting integer as the only argument.
The first thing `professor` does on each iteration is to claim write access with the degree range [0, 180].
Then, it writes the integer from the argument to a file named `quiz` in the current working directory.
After writing, `professor` closes the file, revokes its write access, and increments the integer by one.
The program runs an infinite loop until it receives an interrupt from the user (`Ctrl-C`).
Before releasing the write lock, the program should output the integer to standard output.
Your screen should look like the following:
  ```bash
  $ ./professor 7492
  professor: 7492
  professor: 7493
  professor: 7494
  ...
  ```

- **student:**
`student` accepts two integer arguments (`low` and `high`) that represent the student's read access degree range.
[`low`, `high`] should be within [0, 180], and since `rotd` changes the device orientation +30 degrees at a time, you may want to select a degree range that is larger than 30 degrees.
The first thing `student` does on each iteration is to claim read access with its degree range.
After gaining read access, `student` will open the file `quiz` in the current working directory, calculate the prime number factorization of the integer (you can just use *Trial Division*), and write the result to standard output.
When done, it will close the file and revoke access.
The program runs an infinite loop until it receives an interrupt from the user (`Ctrl-C`).
Your screen should look like the following:
  ```bash
  $ ./student 10 50
  student-10-50: 7492 = 2 * 2 * 1873
  student-10-50: 7493 = 59 * 127
  student-10-50: 7494 = 2 * 3 * 1249
  ...
  ```

Here, you should understand that the `professor` and `student`s agree to synchronize their access to `quiz`, the protected object, with the rotation range [0, 180].
This should be perceived as an **agreement** between `professor` and `student`s, just as if how threads agree to first `acquire` a shared mutex before executing the critical section and `release` it when they're done.

We recommend that you use `tmux` to run these test binaries.
Refer to [this issue](https://github.com/swsnu/osspr2021/issues/29) for instructions on how to install `tmux`.
First of all, run the rotation daemon.
Then, run one `professor` and two or more `student`s with different degree ranges (e.g., [10, 80] and [50, 130]).
Observe that no process is granted access to `quiz` when the device orientation is outside of [0, 180].
Also, verify that the `professor` has exclusive access to `quiz` while multiple `student`s have simultaneous read access even when their degree ranges overlap.


## Submission
- Code
  - Commit to the master branch of your team repo.
	- The `README.md` should describe your implementation, how to build your kernel (if you changed anything), and lessons learned.
- Slides and Demo (`n` is your team number)
	- Email: osspr2021@gmail.com
	- Title: [Project 2] Team `n`
	- Attachments: team-`n`-slides.{ppt.pdf}, team-`n`-demo.{mp4,avi,…}
	- One slide file, one demo video!


## We're here to help you

Any trouble? Questions on the [issue board](https://github.com/swsnu/osspr2021/issues) are more than welcome. Discussions between students are also encouraged.

Start early, be collaborative, and most importantly, have fun!
