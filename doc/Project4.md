# Project 4: Hello, File System!

* Assigned: 2021-05-13 Thursday 17:00:00 KST
* No Design Review
* **Due: 2021-06-02 Wednesday 20:59:59 KST**

## Introduction

This project is the last OS project. Yay!

In this project, you will develop a new kernel-level mechanism for embedding location information into the ext2 file system. 
You will also control the accessibility of the files inside your custom ext2 file system based on the location metadata.

This is a team project. 
Accept the Github classroom assignment invitation. 
Each team will get access to its own GitHub repository (e.g. swsnu/project4-hello-file-system-team-n) for collaboration and submission. 
Commit your final code and README.md to the master branch for submission.


## 1. Setting the device GPS location (10 pts.)

It would be nice if our device has GPS sensors to acquire location information. 
Unfortunately, Tizen devices do not have such sensors and we don't even use physical devices this semester. 
Thus, we will implement a system call to update the current location of our kernel. Recall `sys_set_orientation` back in project 2.

First, write the following definition of `struct gps_location` on `include/linux/gps.h`.

```c
struct gps_location {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
};
```

Write a new system call that updates the current location of the device. 
The kernel should store the location information in kernel memory space. 
Don't forget to synchronize concurrent access to this piece of information.

The new system call number should be 399, and the prototype should be:

```c
/*
 * Set the current device location. Syscall number 399.
 *
 * Args:
 *   loc: gps_location to set as the current device orientation.
 *
 * Returns:
 *   Zero on success
 *   -EINVAL if loc is NULL or its attributes are invalid.
 *   -EFAULT if loc is outside the accessible address space.
 *
 */
long sys_set_gps_location(struct gps_location *loc);
```

This sets

* latitude = `loc->lat_integer` + `loc->lat_fractional` * (10^-6)
* longitude = `loc->lng_integer` + `loc->lng_fractional` * (10^-6)

The validity of each attribute is determined as follows. 
* 0 <= `loc->lat_fractional`, `loc->lng_fractional` < 1,000,000
* -90 <= latitude <= 90
* -180 <= longitude <= 180
* 0 <= accuracy < 1000

The reason why we define the integer part and fractional part separately in `struct gps_location` is because the Linux kernel does not support floating-point representations. 
Fractional parts of the latitude and longitude represent up to the sixth decimal point. 
`accuracy` refers to how close the device's calculated position is from the truth, expressed as a radius and measured in meters ([source](https://www.gps.gov/systems/gps/performance/accuracy/)).

You may assume that all GPS-related operations are performed after properly setting the device's current location.

Implement this system call in `kernel/gps.c`. 


## 2. Update the GPS location of files on creation or modification (20 pts.)

In this section, you will modify the file system implementation so that it updates location information of **regular files** when they are **created** or **modified**. 

To do this, you have to modify the kernel filesystem machinery to include location attributes with getter and setter operations.

### Modify `ext2_inode` and `ext2_inode_info` struct for ext2 file system. 

First of all, you should add five GPS-related attributes to `struct ext2_inode` and `struct ext2_inode_info`. 
The former struct represents data on-disk and the latter data in-memory. 

Append the following fields _in order_ at the _end_ of the two structs:
```
i_lat_integer (32-bit)
i_lat_fractional (32-bit)
i_lng_integer (32-bit)
i_lng_fractional (32-bit)
i_accuracy (32-bit)
```
You will need to pay close attention to the endianness of the fields you add to the ext2 physical inode structure. 

### Modify struct `inode_operations` 

Secondly, you should modify the inode operations interface. Add the following two members to `struct inode_operations` definition in `include/linux/fs.h`:

```c
int (*set_gps_location)(struct inode *);
int (*get_gps_location)(struct inode *, struct gps_location *);
```

Note that function pointer `set_gps_location` does _not_ accept any GPS location structure as its argument - it should use the latest kernel GPS data set by system call `sys_set_gps_location` you implemented in step 1.

Now you should implement `set_gps_location` function that works **for ext2 file system**. 
You should not consider other file systems such as ext3 or ext4. Look in the `fs/` directory for all the file system code, and in the `fs/ext2/` directory for ext2 specific code. 

### Call `set_gps_location` properly on regular file creation or modification 

The only thing left to do is to call `set_gps_location` whenever a regular file is created or modified. 

You don't have to care about other kinds of special files such as directories and symbolic links (unless you are in a team of four; see below for details), but if you want to track location information for them, feel free to do so.

A file being "modified" means that its content has changed. ([Learn more](https://unix.stackexchange.com/questions/2464/timestamp-modification-time-and-created-time-of-a-file)) Note that merely accessing the file with commands such as `cat` does not modify the file.


## 3. User-space testing for location information (10 pts.)

### Write a sytem call to get gps location of a given file. 

In order to retrieve the location information of a file, write a new system call numbered 400 with the following prototype:

```c
/*
 * Set the current device location. Syscall number 400.
 *
 * Args:
 *   pathname: absolute or relative path that describes
 *       the location of a specific file
 *   loc: user-space buffer that the gps location
 *       of a file would be copied to. 
 *
 * Returns:
 *    Zero on success
 *   -EINVAL if one of the argument is NULL or its attributes are invalid.
 *   -EFAULT if loc is outside the accessible address space.
 *   -ENOENT if there is not file or directory named with pathname
 *   -EACCES if the file not is readable by the current user.
 *   -ENODEV if no GPS coordinates are embedded in the file.
 *
 */
long sys_get_gps_location(const char *pathname, struct gps_location *loc);
```

On success, the system call should return 0 and `*loc` should be populated with the location information of the file specified by `pathname`. 
Implement this system call in `kernel/gps.c`. 
We will not test for other possible errors that not mentioned above.

Note that you are to implement a pair of functions that are quite similar in functionality. Namely, a location getter for an inode and a system call that retrieves the location information from the path of a file.


### Use e2fsprogs to create an ext2 filesystem image in user-space
As you have modified the physical representation of the ext2 file system on the disk, you also need a special tool that creates such a file system in user-space. In this project, we will make use of the ext2 file system utilities that [e2fsprogs](http://e2fsprogs.sourceforge.net/) provides. 

Navigate to the link provided above to download `e2fsprogs`.

Then, **modify the appropriate file(s)** in `e2fsprogs/lib/ext2fs/` to match the new physical layout.

After making the aforementioned modifications, compile the utilities.
```bash
os@ubuntu:e2fsprogs$ ./configure
os@ubuntu:e2fsprogs$ make
```

The binary you will be the most interested in is `e2fsprogs/misc/mke2fs`. This tool should now create an ext2 file system with your modifications. Before this, you should find the empty loop device using `losetup -f`. 

```bash
os@ubuntu:~$ sudo losetup -f
/dev/loopXX
```
XX is a positive integer that changes based on your system environment.

Create a modified ext2 file system using the modified `mke2fs` tool on your host PC.

```bash
os@ubuntu:proj4$ dd if=/dev/zero of=proj4.fs bs=1M count=1
os@ubuntu:proj4$ sudo losetup /dev/loopXX proj4.fs
os@ubuntu:proj4$ sudo ../e2fsprogs/misc/mke2fs -I 256 -L os.proj4 /dev/loopXX
os@ubuntu:proj4$ sudo losetup -d /dev/loopXX
```
The file `proj4.fs` now contains the modified ext2 file system! Copy it into QEMU, just like your test binaries.

In QEMU, create a directory called `/root/proj4` and mount the `proj4.fs` on it. 

```bash 
root:~> mkdir /root/proj4
root:~> mount -o loop -t ext2 proj4.fs /root/proj4
```

Files and directory operations that you execute on `/root/proj4` will now be ext2 operations. Check whether the newly created regular files in `/root/proj4` are properly geo-tagged! 


## 4. Location-based file access control (15 pts.)

Access permission to a geo-tagged file should be granted only when the system's current GPS location is _geometrically close_ to the GPS location of the file.

Let us define the geometrical distance between two locations (both described by their longitudes and latitudes) as the shortest path between the two *along the surface of the Earth*.
The two locations are geometrically close if their geometrical distance is smaller than the sum of their `accuracy` values.

In calculating the geometrical distance, the system will have to do floating-point arithmetic.
However, because floating-point operations are not supported in the Linux kernel, you should find a workaround for this; mimic floating-point operations using integer.
Since there must be some errors whichever method you take, we will not strictly examine the result of the calculations.
Yet, make sure you take integer overflows into account.

In order to simplify the calcuation, you can make some geometrical assumptions.
For example, you may assume that the Earth is a perfect sphere with a radius of 6400 km.
Write in `README.md` any assumptions and approximations you made.

In addition to the location-based access control, the new ext2 file system should respect the existing access control mechanism.
That is, proximity to the GPS location of a file should work as an **extra condition** for the system granting the access permission to the file.
If permission cannot be granted under the existing file access control mechanism, access to a file should be disallowed even if its GPS location matches the system's.

Again, you do NOT have to care about directories and symbolic links unless you are in a team of four.
However, if you want to implement extra policies for them, you are free to do so.
It is advisable to document extra policies on `README.md`.

**DO NOT CONFUSE.** The only case you need to check GPS accuracy-based permission is when user requests access to file content. 
In other words, you do not have to consider GPS proximity when implementing `sys_get_gps_location`, since this syscall requests only file metadata.


## 5. Test Code (10 pts.)
In this project, you should write two test programs `test/gpsupdate.c` and `test/file_loc.c`. 

* `gpsupdate.c` takes five command line arguments: `lat_integer`, `lat_fractional`, `lng_integer`, `lng_fractional`, and `accuracy`. It updates the system's GPS location via `sys_set_gps_location` based on the given arguments. 

* `file_loc.c` takes one command line argument, which is a path to a file. Then it prints out the GPS coordinates and `accuracy` of the file, plus the Google Maps link of the corresponding location. Its output format should be as following:

```
Latitude = 10.000000, Longitude = 20.123456
Accuracy = 10 [m]
Google Link = https://maps.google.com/?q=10.000000,20.123456
```
  
Finally, you should place your `proj4.fs` in the root directory. 
Your `proj4.fs` should contain at least 1 directory and 2 files, each of which has different GPS coordinates.

## Extra Problem for Teams of Four
## 6. Geo-tagged Special Files (10 pts.)

This project only handled regular files. 
However, there are other types of files such as directories, symbolic links, block special files, and named pipes. 
How will you implement all the location-based file system machinery for these special files?
You do not have to implement code, but you should explain in **sufficient** detail and length.

**Note that this question is only assigned to teams with four members.
To these teams, the total score will be (the sum of the scores obtained from the six problems) * 65/75.**


## Submission
- Code
  - Commit to the master branch of your team repo.
	- The `README.md` should describe your implementation, how to build your kernel (if you changed anything), and lessons learned.
	- `test/gpsupdate.c`, `test/file_loc.c`, `proj4.fs`
- Slides and Demo (`n` is your team number)
	- Email: osspr2021@gmail.com
	- Title: [Project 4] Team `n`
	- Attachments: team-`n`-slides.{ppt.pdf}, team-`n`-demo.{mp4,avi,…}
	- One slide file, one demo video!


## We're here to help you

Any trouble? Questions on the [issue board](https://github.com/swsnu/osspr2021/issues) are more than welcome.
We also strongly encourage discussions between students.
Start early, be collaborative, and most importantly, have fun!
