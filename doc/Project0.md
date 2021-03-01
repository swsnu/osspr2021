# Project 0: Hello, System Call!


* Assigned: 2021-03-02 Tuesday 17:00:00 KST
* **Due: 2021-03-15 Monday 20:59:59 KST**

## Introduction

Project 0 will get you started with Linux kernel development. The project consists of two parts: 1) understanding how to build the kernel from its source code and boot the QEMU emulator, and 2) implementing a very simple system call. After this project, you will get ready to dive into the kernel world!

This is an individual project. Read this document carefully and follow the instructions. Setting up the environment may not be a trivial task, so we suggest that you start early. For project 0 only, you will submit a screenshot of your terminal via email ([osspr2021@gmail.com](mailto:osspr2021@gmail.com)).

## Development Environment

To build the kernel, you should setup the development environment. We strongly recommend you to work on native Ubuntu 18.04 or 20.04 instead of other operating systems such as Windows or macOS. If you have troubles with on non-Ubuntu systems, the TAs CANNOT actively help you resolve it. You can install Ubuntu side-by-side with the existing OS on your machine (See the official [installation guide](https://help.ubuntu.com/lts/installation-guide/amd64/index.html) and the [Windows dual-booting guide](https://help.ubuntu.com/community/WindowsDualBoot)).

### Setup on Ubuntu 20.04 (or 18.04)

Install necessary tools:
```bash
sudo apt-get update
sudo apt-get install git curl -y
```

Install kernel build dependencies:
```bash
sudo apt-get install build-essential ccache gcc-aarch64-linux-gnu obs-build pv flex bison libssl-dev bv dosfstools kmod -y
```

## Compile the Kernel & Boot it with QEMU

Now it's time to actually have our operating system running!

### Get the Kernel Source

Follow the Github classroom assignment invitation link we gave you for project 0. You will be asked to select your student id from the list, and tie your Github account to the id. This identification process is only needed on project 0. Once you identify yourself, you will be prompted to accept the assignment invitation. When you accept the invitation, your own private assignment repository will be created from the kernel source repository we prepared.

Clone your assignment repository with `git clone`.

### Compile the Kernel

Walk into the kernel source.
```bash
cd project-0-hello-linux-<username>
```

and just type
```bash
./build-rpi3.sh
```
to compile the kernel. The first compilation will take around five to twenty minutes, differing based on your machine's performance. Re-compiling after modifying the source will not take this long thanks to caching.

### Make Tizen Images

First, we will create kernel-related images (the boot image and kernel modules) to feed into our QEMU emulator. The following script helps you create the images from the compilation artifacts. 
```bash
sudo ./scripts/mkbootimg_rpi3.sh
```
Ensure that two new image files (modules.img, boot.img) are created in the repository root.

We also need other images (e.g. initrd ramdisk for booting, root filesystem, system tools). We'll use those provided by Tizen 6.0. Download the zipped file by typing
```bash
curl -LO http://download.tizen.org/releases/milestone/tizen/unified/latest/images/standard/iot-headless-2parts-armv7l-btrfs-rootfs-rpi/tizen-unified_20201020.1_iot-headless-2parts-armv7l-btrfs-rootfs-rpi.tar.gz
```
This file contains images that populate folders such as /usr, /media, etc. 

Then, create the `tizen-image` directory to hold these images. Note that the directory name _must_ be `tizen-image` for `qemu.sh` to recognize it.

```bash
mkdir tizen-image
tar xzvf tizen-unified_20201020.1_iot-headless-2parts-armv7l-btrfs-rootfs-rpi.tar.gz -C ./tizen-image/
cp modules.img boot.img ./tizen-image/
```

For your convenience, we provide you with a shell script named `setup-images.sh`. This scripts automates all steps of creating images and storing them in `tizen-image/`. 
```bash
./setup-images.sh
```

### Boot the kernel with QEMU

The next step is to boot the kernel image using QEMU. QEMU is a full-system emulator and we can run a variety of guest operating systems on it. 
We found out that old versions of QEMU sometimes fails on booting, so we highly recommend you to get the latest version (QEMU 5.2.0) by following below instructions. 

```bash
sudo apt-get install libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build -y
curl -LO download.qemu.org/qemu-5.2.0.tar.xz
tar xJf qemu-5.2.0.tar.xz
cd qemu-5.2.0
./configure --target-list=aarch64-softmmu
make -j$(nproc)
export PATH="$(pwd)/build:$PATH"
```

Then run the following script.
```bash
./qemu.sh
```
If you face an error message related to ownership, make sure that you (the output of `whoami`) is the owner of all image files in `tizen-image`. If not, change the owner to you with `chown`.

Log in to your emulator as `root`, with password `tizen`. Now you are inside the OS that runs a kernel that you compiled!

`Ctrl-C` will be passed to the shell running inside QEMU, so it won't kill QEMU. Type `Ctrl-A` then `X` to exit from QEMU.


## Register a Simple System Call 

Now that we have our kernel up and running, we will implement a simple system call. As you know, system call is a protocol between the user application and the operating system.

The system call we will make for this project - let's call it `sys_hello` - has no input argument. It just records the number of times the system call has been invoked since boot and prints the following hello message with the call counter.

```bash
"Hello, this is syscall by %s! (%d)\n", your_name, number_of_the syscall_called.
```

As a side note, make sure you end your message with a newline (`\n`). The kernel buffer might not get flushed if you don't.

Since there are already 399 syscalls (from number 0 to 398) implemented in Linux 4.19, `sys_hello` should be assigned the syscall number **399**. 

Be aware that when writing kernel code, you cannot use C library functions as you usually do. For example, you cannot import `stdio.h` and use `printf` inside the kernel. There exists `printk` as an alternative.

You can refer to the bountiful system call examples from the Linux kernel source. Also refer to the [official guide](https://www.kernel.org/doc/html/v4.19/process/adding-syscalls.html).

Remember that **we only target the ARM64 (aarch64) architecture** throughout this class. You do not need to implement code for other architectures. 

## Test your new system call

In order to check that your system call is successfully implemented, you should write a simple C code which runs on userland. The test code should invoke the your new system call once and exit.

You can invoke the `sys_hello` system call using the `syscall` function. (See [here](https://linux.die.net/man/2/syscall) for details.)

You should compile the userland code with `arm-linux-gnueabi-gcc` (install with `sudo apt-get install gcc-arm-linux-gnueabi`). For example, if your test code is named `test_hello.c`, you can compile it with the following command.

```bash
arm-linux-gnueabi-gcc test_hello.c -o test_hello
```
To execute the test binary inside QEMU, you should copy the executable into the root file system image.

Run
```bash
mkdir mntdir
sudo mount tizen-image/rootfs.img ./mntdir
```

This command mounts the Tizen root file system image to the directory named `mntdir/`. If you list the contents in `mntdir/`, you can see that it is a subset of what you can find in the root directory inside QEMU.

Copy your test binary into the `root` directory under `mntdir/`, and then unmount `mntdir/`. 
```bash
sudo cp test_hello ./mntdir/root
sudo umount ./mntdir
rmdir mntdir
```
You will see nothing inside `mntdir/` after unmounting. Now, `rootfs.img` contains our test executable.
Boot up QEMU again, and you will find your executable inside `/root`!

## Submission

Take a screenshot of your test binary working and send it to [osspr2021@gmail.com](mailto:osspr2021@gmail.com). The title should follow the format "[Project 0] <student_id> <name>", and the screenshot should look like this:
![screenshot_syshello](/doc/assets/syshello_example.png)

Please keep in mind that **2021-03-15 Monday 20:59:59 KST** is the strict deadline - we never accept the submissions after the deadline.


## We're Here to Help You

Any troubles? Discussions on [issue board](https://github.com/swsnu/osspr2021/issues) are more than welcome. Remember, we are here to help you!

Start early, ask for help if needed, and most importantly, have fun!
