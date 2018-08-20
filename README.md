A tool for destroying hard drives through aggressive wear and tear.

Copyright(c) 2018 Virtual Interconnect LLC, Licensed under the Apache 2.0 license. See COPYING file for full details and credits.

NOTE: Before the steps under 'INSTALL', run 'bootstrap.sh'. You will need to have the automake and autoconf packages installed.

# Introduction
## Why?

To improve reliability of drives. Most manufactured devices--including hard drives--exhibit a failure rate pattern called the "bathtub curve." Take some arbitrarily large number of hard drives and subject them to the same conditions. You will see a "high" rate of failure early in their operation, followed by a period of few, random failures, followed by an increase in failures as the group of drives reach the end of their lives.

We subject the drives to intense stress in order to accelerate the initial "infant mortality" period; any drives that survive the initial burn-in period will be less likely to fail until much later in their life time. This can help avoid clustered failures in RAID arrays, for example; once past the infant mortality period, failures are essentially random until you reach the old-age stage.

For further information on this particular phenomena, I would suggest the search terms "bathtub curve" and "burn-in period."

## Why another stress utility?

To my knowledge at the time, there were no other free utilities that performed the types of tests I wanted to perform, and certainly none that I could trivially spin up in a temporary Linux environment.

## What does it do?

`diskstress` attempts to destroy your (spinning rust) hard disk through mechanical wear by forcing aggressive seeking on disk. Using the operating system's direct I/O capabilities, it bypasses the system cache and drives read/write operateions to the underlying devices. It performs reads and writes at widely-separated, continually-changing positions on the disk in order to force as much mechanical wear on the disk in as short a time as possible.

# How to use this tool
## WARNING!

It shouldn't need to be said. This program *WILL DESTROY YOUR DATA*. And, if it serves its purpose, it will also destroy your hardware.

## Usage

Very simple:

    diskstress /dev/disk_to_stress

Replace `disk_to_stress` with the device node you want operated upon.

## Operation

`diskstress` will choose two distant positions on the disk, call them `A` and `B`. It will then perform read/write operations in this order:

1. read(`A`)
2. read(`B`)
3. write(`A`)
4. write(`B`)
5. read(`A`)
6. read(`B`)

In the best case, this will result in six seek operations; first to `A`, then to `B`, then to `A` again, then to `B` again, and then to `A` and then `B` once more. If the disk (or an intermediate controller!) happens to have the data at either `A` or `B` cached, then the drive will not perform that seek. It's for this reason that, after step 6, we begin again at step 1 with a new `A` and a new `B`. Having two different read stages allows us some insight into whether there is any read caching going on, and what the cause may be.

Presently, point A starts at the beginning of the disk, and point B near the end. They move toward each other until they meet, then the test starts over with a different set of parameters.

### Output

Every 10 seconds, `diskstress` will emit a line of output. Here is a live example:

    /dev/sde 61 10.069847920 [0.034776413,0.045707382,0.045891638,0.045758657,0.004500803,0.004644821] 709248417792 2291309961216 1048576
    /dev/sde 64 10.056144268 [0.036206564,0.046458630,0.045139494,0.038166601,0.004513746,0.004517338] 709315526656 2291242852352 1048576
    /dev/sde 64 10.059391059 [0.032707542,0.041617889,0.041646258,0.041660732,0.004668503,0.004539737] 709382635520 2291175743488 1048576
    /dev/sde 65 10.147834596 [0.034035356,0.040208093,0.043062584,0.040529336,0.004450394,0.004871618] 709450792960 2291107586048 1048576

The first column is the device node `diskstress` is operating on.

The second column is the number of test cycles (read-read-write-write-read-read) that were completed within the past ten seconds.

The third column is how many seconds since our last line of output. (Roughly. We're not going for `ntpd`-grade accuracy with timing adjustments.)

The fourth through ninth columns are an array of operation timings; the test consists of two reads, followed by two writes, followed by two reads, so each number in the sequence tells you how long that operating took. Here, you can see the second pair of reads was faster than the first pair, suggesting that caching is in play somewhere, but only immediately following a write to that location. Which is fine. I'm not confident that bit of caching can be eliminated, which suggests that it can be used as a "this is how fast we go when we don't touch disk" baseline.

The tenth column is the location of "Point A", and the eleventh "Point B."

The twelfth coloumn is how far points A and B jump between each cycle. 

If your disk experiences an error, you will see output like this:

    /dev/sdd 75 10.093418697 [0.026662891,0.034494836,0.032197224,0.034127212,0.004376702,0.004307148] 730144440320 2269341245440 1073741824
    /dev/sdd 75 10.086650507 [0.029098818,0.031784389,0.026519784,0.031710075,0.004365484,0.004511186] 810675077120 2188810608640 1073741824
    Error: system:5 - Input/output error

Either the read() or write() call returned an error. Sometimes, you'll be able to see the precise nature of the failure in the system logs.

# Considerations

## Caching

*Any* caching blunts the effectiveness of the mechanical wear. Half of the design of the program is aimed around evading caching. If you have any caching enabled, you should disable it.

### Read cache

If a read cache already has the data being requested, it will return that data without consulting the underlying disk. For general performance purposes, this is great, but for our purposes, this is a lost seek, and undesirable. Read caches may be populated by previous requests for the data, by previous writes of the data to disk (with a copy kept in memory for as long as convenient), or even by readahead proactively pulling data from disk that may be asked for.

### Write cache

A write cache (or, more specifically, a write-back cache) accumulates write operations, permitting the writing process to go on with its day. At some point, typically in the background, the write cache will reorder the writes for efficiency and flush the data to disk. "Write barriers" can prevent this reordering, but some layers of the I/O subsystem--most notably storage controllers with flash-backed or battery-backed write caches--will violate those barriers from the perspective of the raw underlying disk, so long as it can safely ensure that the application can never perceive that the data was written to the underlying disk out of order. While this is stellar behavior for database loads and the like, it interferes with our purpose of forcing the underlying hard drives to operate inefficiently and degradingly.

### System page cache

The system page cache is not a concern; `diskstress` opens the target file with the `O_DIRECT` mode to bypass this subsystem.

### Drive cache

All drives have a built-in read cache. This will prevent the drive from seeking to any position where the drive's cache already contains the data at the address requested. This cache cannot be turned off, to my knowledge. For this reason, `diskstress` seeks to never read from the same place twice in a given cycle.

All drives also have a write cache, but this is disabled for good reason; unless you *really* know what you're doing, enabling the drive write cache carries a strong risk of data loss in the event of power loss or accidental disconnect.

### Controller cache

If you have a fancy hard disk controller with a battery-backed or flash-backed write cache, you probably have a very nice write cache enabled. That write cache will totally blunt the impact of `diskstress`'s writes. Disable it.

Controllers like that will also have (typically very large) read caches. `diskstress`'s normal behavior should *mostly* work around this, but you should disable this cache, too, if you can.

## RAID

If you're testing disks for inclusion in a RAID array, you should run `diskstress` on the disks individually, not on the final array. In addition to caching effects, the synchronous nature of a RAID array will slow the stressing activity down. Worse, it may blunt the effectiveness of `diskstress`'s seeking patterns. The best way to run `diskstress` is to have multiple instances running at the same time, each against a specific disk.

It's fairly straightforward: Don't do it while running `diskstress`.

### Striped RAID (0, 0+1, 1+0, 10, 50, 60...)

Our problems with striped RAID arrays aren't related to speed as they are with mirrored and parity RAIDs. With striped arrays, we don't know if two addresses are on the same disk or not, which makes forcibly moving a given disk's head across its platter difficult. To be sure, RAID arrays are generally predictable enough that, informed of a few basic attributes of the array's layout, we could handle this just fine...but I don't see much benefit to it, given you can simply present the disks individually. And if we manage to break a disk, you don't want the stressing of other disks to halt while you replace the broken disk.

### Mirrored RAID (1, 0+1, 1+0, 10...)

Let's say that you have an array of two disks in RAID 1. If you run `diskstress` on the array device, each read operation `diskstress` issues will only affect one disk; RAID supervisors don't typically compare reads from multiple disks as a validity check. I don't know if the disk chosen is random, hashed or hardcoded; you might be halving the effectiveness of your reads, or you might be eliminating them entirely for some of the disks in your array!

Now, for writes, the story is a little bit better; the RAID supervisor will issue your write instruction to both disks, but will wait for both disks to complete before permitting `diskstress` to even think about issuing its second write instruction. While the two disks will hopefully be very similar in performance, if one is even 1ms slower than the other, you're starving that other disk for activity.

All of this gets worse the more disks you have in the array.

### Parity RAID (3, 4, 5, 6, 50, 60...)

Lets say you have at least two data disks and one parity disk in a parity raid setup. Reads suffer the same problem as in mirrored RAID; the typical supervisor isn't going to consult any more disks than required, and with small reads, that number of disks will be exactly one; parity is typically only read when serving partial writes or when reconstructing lost data, not when handling arbitrary read requests on a healthy array.

For writes in this setup, the story starts off a little bit worse than in mirrored RAID; `diskstress` will issue small writes, which will force the array supervisor to read the existing data off of a data disk and the parity disk, and then write out a whole stripe to both. That means that one 'write' became a "read near where I want to write on a data section and a parity section, then write to where my heads are already near." This takes longer than a simple write for little gain. (Remember, *our* purpose is to force head travel; *we* don't actually care about the array's concept of data integrity.)

Further, all commonly-used parity RAID setups are also striped, so we also have all of the problems you get with striped RAID.

With more disks, all of these problems are amplified.

## Volume Managers

Volume managers such as `LVM` (and `btrfs`, and `ZFS`, which also have the functions of volume managers) can perform a huge variety of modifications to I/O operations on the fly, including *all* of the operations described for various RAID levels, some of the operations mentioned relating to caching, and other fancy things. You might even wind up with a byte at the end of your logical disk being physically earlier than a byte at the beginning of your logical disk, due to fragmentation and thin provisioning. I am in no way sufficiently knowledgable about volume manager capabilities to give a full treatment here just to explain to you *why* it's a bad idea. Just don't do it.

## Networks

While it's certainly possible to run `diskstress` against SANs and NASes, I would recommend against it. It's not a bandwidth issue; `diskstress` tends to be very low in that regard. Rather, network filesystems themselves perform caching to reduce latency, and the remote end of SAN protocols like iSCSI may well have RAID levels and volume managers between the SAN endpoint and the underlying raw disk. And in all cases, you may well have system page cache active on the SAN or NAS in question.

And, of course, adding the network will increase the latency, reducing how much effective stress you put on your disks. Networks aren't subject to mechanical wear from seeks, so who cares?

## Journalling

Journalling can have the same behavior as a write cache, depending on where it's implemented and how it's configured. A journalling filesystem (or volume manager, or raid supervisor) may queue up writes in a persistent place before flushing that data to its final destination in the background. As with a write cache, this will give `diskstress` the wrong idea about how fast seek operations are, and will likely reorder write operations for efficiency--defeating our purpose.

## Readahead

Most layers of the I/O system implement readahead; the idea that if you asked for byte A and A+1, then you're probably going to want byte A+2, so why don't I just go grab that for you in advance? Read ahead is implemented on individual disks, often on storage controllers, on arbitrary block devices, and usually in filesystems. And it's often something that can be tuned at every stage. `diskstress` deals with readahead the same way it deals with read caches; it tries to read in places that have no reason to be in cache yet.

## Filesystems

You can create a file (sparse or not) that's anywhere from a few gigabytes to a few terabytes, and point `diskstress` at that. That should *mostly* work, although there's no good way to guarantee that the head seeks will go very far, or how far they'll go; file fragmentation is your big risk here. Further, filesystems may implement their own caching, their own journalling, their own readahead. Bear all that in mind.

#Flaws, Plans and Improvements

Nothing is perfect.

## Needs improvement:
### Configurable read, write and jump increments
Right now, the sizes of reads and writes are all hardcoded lists; `diskstress` will try a couple different read sizes, a couple different write sizes, and a handful of different jump increments, and runs every combination thereof. These should be configurable via the command line, or even via file.
### Better output
Right now, the output is a hodge podge. Truthfully, each column should be labeled with the nature of its output. The read and write sizes should also be included.

### Persistent output
It would be pretty darn helpful to save the telemetry data out to disk rather than just emit it to the console; we could save the record of every cycle that way, and not just the average over every ten seconds. (If you output every cycle, even to a fast console, you' can watch your disk spend its time idle. Not ideal.)

# Nice-to-haves:
## Multi-disk
Rather than running a separate instance per disk, we should have a single instance operate on multiple disks in parallel. This would give us the ability to isolate sources of timing discrepency (and thus interference with our disk operations). By streaming data into disk A to fill up any intermediate cache with disk-A-data, we could calculate how much data disk B caches, for example.
## Diagnostics
In the sample output above, it's possible to observe that there is a read cache in place, but that the positions being written to were not cached prior to being written to. If the write operation timings were about the same as the second set of read operations, it could be inferred that a write cache was in place.

If we were to compare subsequent timings of subsequent regions of the disk, we could observe anomalies stemming from, e.g. remapped sectors.

If we can keep track of where we've written data, (and where our data is in which caches; see self-tuning) we can avoid clobbering our own previously-written data, and thus perform data integrity checks on a subsequent pass, ensuring the data on the disk media is precisely what we left there.
## Self-tuning
By beginning with larger jumps and working our way down, it should be possible to detect the boundaries of any active readahead, and use that as an absolute minimum distance between reads.

If we can isolate and calculate the disk's cache size vs the controller's (or any other intermediate's) cache size and readahead lengths, we can perform read and write accounting to keep track of where we've read or written data that's likely to be in a cache somewhere, and so avoid issuing a read request that will get short-circuited by cache.
## SMART data
I would love to pull SMART data periodically to look for spikes and anomalies in various variables. Did the ECC counters for various operations increase at a significantly higher or lower rate at particular places on the disk? Did the increase or decrease with different seek distances?
## More aggressive tests
### Armature harmonics
Right now, we have a seek pattern that starts with a broad seek, and gradually shrinks the seek distance down to nothing. In mechanical terms, that means we're driving the armature at a low frequency, gradually increase that frequency until it's as high as we can get it. I would love to try to find a harmonic of the armature's resonant frequency, for example, in the hopes of revealing defects in the drive's behavior. This might tie in to reading SMART data and watching for high-flying writes or ECC correction spikes.
### Drive "hotspots"
Again tying into the SMART data, I'd love to watch for changes in SMART variables as different areas of the drive are focused on, and map out what areas of the drive get higher ECC error rates, what seek sizes get higher ECC error rates, whether certain areas of the drive are more or less vulnerable to different seek sizes, etc. Where we wee spikes in error rates, perhaps we want to perform more testing centered on that area of the drive; high error rates might happen somewhere near a sector more likely to fail earlier, and the sooner we can fail a disk, the better.

### Scan patterns
Right now, the test pattern touches opposite ends of the disks, and then moves the touch points gradually toward the logical center of the disk. Thus, the seek length goes from high to low, but it's only low around one area of the disk. Call this the A-B scan.

A couple alternative seek patterns I'd like to try:

#### A-B-C-n scan
As with A-B scan, but when the seek distance shrinks to half the logical disk length, add a third touch point, and redistribute the touch points evenly. As the seek distance shrinks further, to one-third the logical disk length, add a fourth touch point, and again redistribute the touch points evenly. Repeat each time there is sufficient room for an additional touch point with the given seek distance.

This should give us much better coverage of the shorter strokes. (And if we're saving our telemetry, much better mapping of the drive to different characteristics.)
