<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>perf基础（3）用户态指令分析</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月19日</font></center>
<br/>


* 本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)

# 1. strace `perf stat`分析

执行`strace perf stat ls`可以得到如下输出：

```c
execve("/usr/bin/perf", ["perf", "stat", "ls"], [/* 65 vars */]) = 0
[...]

open("/sys/bus/event_source/devices/cpu/format/pc", O_RDONLY) = 5
read(5, "config:19\n", 8192)            = 10
close(5)                                = 0

[按照上面形式获取以下信息]

/sys/bus/event_source/devices/cpu/format/any 	config:21
/sys/bus/event_source/devices/cpu/format/cmask 	config:24-31
/sys/bus/event_source/devices/cpu/format/edge 	config:18
/sys/bus/event_source/devices/cpu/format/event 	config:0-7
/sys/bus/event_source/devices/cpu/format/frontend 	config1:0-23
/sys/bus/event_source/devices/cpu/format/in_tx 	config:32
/sys/bus/event_source/devices/cpu/format/in_tx_cp 	config:33
/sys/bus/event_source/devices/cpu/format/inv 	config:23
/sys/bus/event_source/devices/cpu/format/ldlat 	config1:0-15
/sys/bus/event_source/devices/cpu/format/offcore_rsp 	config1:0-63
/sys/bus/event_source/devices/cpu/format/pc 	config:19
/sys/bus/event_source/devices/cpu/format/umask 	config:8-15

/sys/bus/event_source/devices/cpu/events/branch-instructions 	event=0xc4
/sys/bus/event_source/devices/cpu/events/branch-misses 	event=0xc5
/sys/bus/event_source/devices/cpu/events/bus-cycles 	event=0x3c,umask=0x01
/sys/bus/event_source/devices/cpu/events/cache-misses 	event=0x2e,umask=0x41
/sys/bus/event_source/devices/cpu/events/cache-references 	event=0x2e,umask=0x4f
/sys/bus/event_source/devices/cpu/events/cpu-cycles 	event=0x3c
/sys/bus/event_source/devices/cpu/events/cycles-ct 	event=0x3c,in_tx=1,in_tx_cp=1
/sys/bus/event_source/devices/cpu/events/cycles-t 	event=0x3c,in_tx=1
/sys/bus/event_source/devices/cpu/events/el-abort 	event=0xc8,umask=0x4
/sys/bus/event_source/devices/cpu/events/el-capacity 	event=0x54,umask=0x2
/sys/bus/event_source/devices/cpu/events/el-commit 	event=0xc8,umask=0x2
/sys/bus/event_source/devices/cpu/events/el-conflict 	event=0x54,umask=0x1
/sys/bus/event_source/devices/cpu/events/el-start 	event=0xc8,umask=0x1
/sys/bus/event_source/devices/cpu/events/instructions 	event=0xc0
/sys/bus/event_source/devices/cpu/events/ref-cycles 	event=0x00,umask=0x03
/sys/bus/event_source/devices/cpu/events/tx-abort 	event=0xc9,umask=0x4
/sys/bus/event_source/devices/cpu/events/tx-capacity 	event=0x54,umask=0x2
/sys/bus/event_source/devices/cpu/events/tx-commit 	event=0xc9,umask=0x2
/sys/bus/event_source/devices/cpu/events/tx-conflict 	event=0x54,umask=0x1
/sys/bus/event_source/devices/cpu/events/tx-start 	event=0xc9,umask=0x1

/sys/bus/event_source/devices/cpu/type    4

/sys/bus/event_source/devices/msr/format/event    config:0-63

/sys/bus/event_source/devices/msr/events/smi 	event=0x04
/sys/bus/event_source/devices/msr/events/tsc 	event=0x00
/sys/bus/event_source/devices/msr/events/pperf 	event=0x03

/sys/bus/event_source/devices/msr/type    8


/sys/bus/event_source/devices/software/type 	1
/sys/bus/event_source/devices/software/uevent


/sys/bus/event_source/devices/power/format/event 	config:0-7

/sys/bus/event_source/devices/power/events/energy-cores 	event=0x01
/sys/bus/event_source/devices/power/events/energy-cores.scale 	2.3283064365386962890625e-10
/sys/bus/event_source/devices/power/events/energy-cores.unit 	Joules
/sys/bus/event_source/devices/power/events/energy-pkg 	event=0x02
/sys/bus/event_source/devices/power/events/energy-pkg.scale 	2.3283064365386962890625e-10
/sys/bus/event_source/devices/power/events/energy-pkg.unit 	Joules
/sys/bus/event_source/devices/power/events/energy-ram 	event=0x03
/sys/bus/event_source/devices/power/events/energy-ram.scale 	2.3283064365386962890625e-10
/sys/bus/event_source/devices/power/events/energy-ram.unit 	Joules


/sys/bus/event_source/devices/power/type    9
/sys/bus/event_source/devices/power/cpumask    0-3

/sys/bus/event_source/devices/tracepoint/type    2

/sys/bus/event_source/devices/kprobe/format/retprobe    config:0
/sys/bus/event_source/devices/kprobe/type    6

/sys/bus/event_source/devices/uprobe/format/retprobe    config:0
/sys/bus/event_source/devices/uprobe/type    7

/sys/bus/event_source/devices/breakpoint/type    5


perf_event_open(0x3b33988, 253222, -1, -1, PERF_FLAG_FD_CLOEXEC) = 3
perf_event_open(0x3b33b08, 253222, -1, -1, PERF_FLAG_FD_CLOEXEC) = -1 EACCES (Permission denied)
open("/proc/sys/kernel/perf_event_paranoid", O_RDONLY) = 4
read(4, "2\n", 64)                      = 2
close(4)                                = 0

[perf_event_open 了 3-11文件描述符]

[perf stat command 的 command输出]

read(3, "\276\332\22\0\0\0\0\0\276\332\22\0\0\0\0\0\276\332\22\0\0\0\0\0", 24) = 24
read(4, "\0\0\0\0\0\0\0\0\276\332\22\0\0\0\0\0\276\332\22\0\0\0\0\0", 24) = 24
read(5, "\0\0\0\0\0\0\0\0\276\332\22\0\0\0\0\0\276\332\22\0\0\0\0\0", 24) = 24
read(7, "!\1\0\0\0\0\0\0\276\332\22\0\0\0\0\0\276\332\22\0\0\0\0\0", 24) = 24
read(8, "\313\215\20\0\0\0\0\0\374\30\23\0\0\0\0\0\374\30\23\0\0\0\0\0", 24) = 24
read(9, "k\7\n\0\0\0\0\0\374\30\23\0\0\0\0\0\374\30\23\0\0\0\0\0", 24) = 24
read(10, "Y\22\2\0\0\0\0\0\374\30\23\0\0\0\0\0\374\30\23\0\0\0\0\0", 24) = 24
read(11, "8\34\0\0\0\0\0\0\374\30\23\0\0\0\0\0\374\30\23\0\0\0\0\0", 24) = 24
close(11)                               = 0
close(10)                               = 0
close(9)                                = 0
close(8)                                = 0
close(7)                                = 0
close(5)                                = 0
close(4)                                = 0
close(3)                                = 0

write(2, " Performance counter stats for ", 31 Performance counter stats for ) = 31

[perf 的打印...]

+++ exited with 0 +++
```

# 2. `perf_event_open`系统调用

```c
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

int perf_event_open(struct perf_event_attr *attr,
                   pid_t pid, int cpu, int group_fd,
                   unsigned long flags);
```

# 3. `/sys/bus/event_source/devices/`文件系统

```c
# pwd
/sys/bus/event_source/devices
# tree
.
├── breakpoint -> ../../../devices/breakpoint
├── cpu -> ../../../devices/cpu
├── kprobe -> ../../../devices/kprobe
├── msr -> ../../../devices/msr
├── power -> ../../../devices/power
├── software -> ../../../devices/software
├── tracepoint -> ../../../devices/tracepoint
└── uprobe -> ../../../devices/uprobe
```

他们的类型为：

```c
/*
 * attr.type
 */
enum perf_type_id { /* perf 类型 */
	PERF_TYPE_HARDWARE			= 0,    /* 硬件 */
	PERF_TYPE_SOFTWARE			= 1,    /* 软件 */
	PERF_TYPE_TRACEPOINT		= 2,    /* 跟踪点 /sys/bus/event_source/devices/tracepoint/type */
	PERF_TYPE_HW_CACHE			= 3,    /* 硬件cache */
	PERF_TYPE_RAW				= 4,    /* RAW/CPU /sys/bus/event_source/devices/cpu/type */
	PERF_TYPE_BREAKPOINT		= 5,    /* 断点 /sys/bus/event_source/devices/breakpoint/type */

	PERF_TYPE_MAX,				/* non-ABI */
};
```

有些为注册时填入，有些则为初始化阶段生成。

# 4. 相关链接

* [注释源码：https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核 eBPF基础：perf（1）：perf_event在内核中的初始化](https://rtoax.blog.csdn.net/article/details/116982544)
* [Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)
* [Linux kernel perf architecture](http://terenceli.github.io/技术/2020/08/29/perf-arch)
* [Linux perf 1.1、perf_event内核框架](https://blog.csdn.net/pwl999/article/details/81200439)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)
