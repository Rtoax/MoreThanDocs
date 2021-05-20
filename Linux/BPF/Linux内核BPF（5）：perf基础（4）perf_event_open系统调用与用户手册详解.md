<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>perf（4）perf_event_open系统调用与用户手册详解</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月19日</font></center>
<br/>


* 本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)

在上篇讲到使用strace最终perf stat ls指令的系统调用，perf先从proc文件系统中获取内核支持的perf event，然后使用系统调用perf_event_open和ioctl控制内核中perf_event的使能，并从文件描述符中读取数据。本文将对系统调用perf_event_open进行讲解。


# 1. `perf_event_open`系统调用

## 1.1. 函数原型

```c
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

int perf_event_open(struct perf_event_attr *attr,
                   pid_t pid, int cpu, int group_fd,
                   unsigned long flags);
```
它在lib中是没有wrapper的，所以可以自己定义：

```c
static int perf_event_open(struct perf_event_attr *evt_attr, pid_t pid,
                                int cpu, int group_fd, unsigned long flags)
{
    int ret;
    ret = syscall(__NR_perf_event_open, evt_attr, pid, cpu, group_fd, flags);
    return ret;
}
```

### 1.1.1. `pid`

参数pid允许事件以各种方式附加到进程。 

* 如果pid为0，则在**当前线程**上进行测量；
* 如果pid大于0，则对**pid指示的进程**进行测量；
* 如果pid为-1，则对**所有进程**进行计数。

### 1.1.2. `cpu`

cpu参数允许测量特定于CPU。 

* 如果`cpu>=0`，则将测量限制为指定的CPU；否则，将限制为0。 
* 如果`cpu=-1`，则在所有CPU上测量事件。

> 请注意，`pid == -1`和`cpu == -1`的组合无效。

* `pid> 0`且`cpu == -1`设置会测量每个进程，并将该进程跟随该进程计划调度到的任何CPU。 每个用户都可以创建每个进程的事件。
* 每个CPU的`pid == -1`和`cpu>= 0`设置是针对每个CPU的，并测量指定CPU上的所有进程。 每CPU事件需要`CAP_SYS_ADMIN`功能或小于/ 1的`/proc/sys/kernel/perf_event_paranoid`值。见《`perf_event`相关的配置文件》章节。

### 1.1.3. `group_fd`

group_fd参数允许创建**事件组**。 一个事件组有一个事件，即组长。 首先创建领导者，group_fd = -1。 其余的组成员是通过随后的perf_event_open（）调用创建的，其中group_fd设置为组长的fd。 （使用group_fd = -1单独创建一个事件，并且该事件被认为是只有1个成员的组。）将事件组作为一个单元调度到CPU：仅当所有 可以将组中的事件放到CPU上。 这意味着成员事件的值可以彼此有意义地进行比较，相加，除法（以获得比率）等，因为它们已经为同一组已执行指令计数了事件。

### 1.1.4. `flags`
```c
#define PERF_FLAG_FD_NO_GROUP		(1UL << 0)
#define PERF_FLAG_FD_OUTPUT		(1UL << 1)
#define PERF_FLAG_PID_CGROUP		(1UL << 2) /* pid=cgroup id, per-cpu mode only */
#define PERF_FLAG_FD_CLOEXEC		(1UL << 3) /* O_CLOEXEC */
```

在系统调用man手册中是这样讲解的：

* `PERF_FLAG_FD_NO_GROUP`：此标志允许将事件创建为事件组的一部分，但没有组长。 尚不清楚这为什么有用。
* `PERF_FLAG_FD_OUTPUT`：该标志将输出从事件重新路由到组长。
* `PERF_FLAG_PID_CGROUP`：该标志激活每个容器的系统范围内的监视。 容器是一种抽象，它隔离一组资源以进行更精细的控制（CPU，内存等）。 在这种模式下，仅当在受监视的CPU上运行的线程属于指定的容器（cgroup）时，才测量该事件。 通过传递在cgroupfs文件系统中其目录上打开的文件描述符来标识cgroup。 例如，如果要监视的cgroup称为test，则必须将在/ dev / cgroup / test（假定cgroupfs安装在/ dev / cgroup上）上打开的文件描述程序作为pid参数传递。 cgroup监视仅适用于系统范围的事件，因此可能需要额外的权限。（本文不讨论容器相关内容）
* `PERF_FLAG_FD_CLOEXEC`：O_CLOEXEC：在linux系统中，open一个文件可以带上O_CLOEXEC标志位，这个表示位和用fcntl设置的FD_CLOEXEC有同样的作用，都是在fork的子进程中用exec系列系统调用加载新的可执行程序之前，关闭子进程中fork得到的fd。（在使用`strace perf stat ls`调用`perf_event_open`传入的即为`PERF_FLAG_FD_CLOEXEC`）


### 1.1.5. `struct perf_event_attr`结构

#### 1.1.5.1. 结构原型

```c
struct perf_event_attr { 
   __u32     type;         /* Type of event */
   __u32     size;         /* Size of attribute structure */ 
   __u64     config;       /* Type-specific configuration */
   union { 
       __u64 sample_period;    /* Period of sampling */
       __u64 sample_freq;      /* Frequency of sampling */ 
   };
   __u64     sample_type;  /* Specifies values included in sample */ 
   __u64     read_format;  /* Specifies values returned in read */
   __u64     disabled       : 1,   /* off by default */ 
         inherit        : 1,   /* children inherit it */
         pinned         : 1,   /* must always be on PMU */ 
         exclusive      : 1,   /* only group on PMU */
         exclude_user   : 1,   /* don't count user */ 
         exclude_kernel : 1,   /* don't count kernel */
         exclude_hv     : 1,   /* don't count hypervisor */ 
         exclude_idle   : 1,   /* don't count when idle */
         mmap           : 1,   /* include mmap data */ 
         comm           : 1,   /* include comm data */
         freq           : 1,   /* use freq, not period */ 
         inherit_stat   : 1,   /* per task counts */
         enable_on_exec : 1,   /* next exec enables */ 
         task           : 1,   /* trace fork/exit */
         watermark      : 1,   /* wakeup_watermark */ 
         precise_ip     : 2,   /* skid constraint */
         mmap_data      : 1,   /* non-exec mmap data */ 
         sample_id_all  : 1,   /* sample_type all events */
         exclude_host   : 1,   /* don't count in host */ 
         exclude_guest  : 1,   /* don't count in guest */
         exclude_callchain_kernel : 1, /* exclude kernel callchains */ 
         exclude_callchain_user   : 1, /* exclude user callchains */ 
         __reserved_1   : 41;
   union { 
       __u32 wakeup_events;    /* wakeup every n events */
       __u32 wakeup_watermark; /* bytes before wakeup */ 
   };
   __u32     bp_type;          /* breakpoint type */
   union { 
       __u64 bp_addr;          /* breakpoint address */
       __u64 config1;          /* extension of config */ 
   };
   union { 
       __u64 bp_len;           /* breakpoint length */
       __u64 config2;          /* extension of config1 */ 
   };
   __u64   branch_sample_type; /* enum perf_branch_sample_type */ 
   __u64   sample_regs_user;   /* user regs to dump on samples */
   __u32   sample_stack_user;  /* size of stack to dump on samples */ 
   __u32   __reserved_2;       /* Align to u64 */
};
```

#### 1.1.5.2. type字段

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

* `PERF_TYPE_HARDWARE`: 这表示内核提供的“通用”硬件事件之一。
* `PERF_TYPE_SOFTWARE`: 这表示内核提供的一种软件定义的事件（即使没有可用的硬件支持）。
* `PERF_TYPE_TRACEPOINT`: 这表示内核跟踪点基础结构提供的跟踪点。
* `PERF_TYPE_HW_CACHE`: 这表示硬件高速缓存事件。 这具有特殊的编码，在config字段定义中描述。
* `PERF_TYPE_RAW`: 这表示在config字段中发生的“原始”特定于实现的事件。
* `PERF_TYPE_BREAKPOINT`: 这表示CPU提供的硬件断点。 断点可以是对地址的读/写访问，也可以是指令地址的执行。

> 动态PMU(man)
> 从Linux 2.6.39开始，perf_event_open（）可以支持多个PMU。 为此，可以在类型字段中使用内核导出的值来指示要使用的PMU。 可以在sysfs文件系统中找到要使用的值：每个PMU实例在`/sys/bus/event_source/devices`下都有一个子目录。 在每个子目录中都有一个类型文件，其内容是可以在类型字段中使用的整数。 例如，`/sys/bus/event_source/devices/cpu/type`包含核心CPU PMU的值，通常为4。

#### 1.1.5.3. size字段

用于向前/向后兼容性的perf_event_attr结构的大小。 使用`sizeof(struct perf_event_attr)`进行设置，以允许内核在编译时看到结构大小。

相关的定义`PERF_ATTR_SIZE_VER0`设置为64； 这是第一个发布的struct的大小。 `PERF_ATTR_SIZE_VER1`为72，对应于Linux 2.6.33中添加的断点。 `PERF_ATTR_SIZE_VER2`为80，对应于Linux 3.4中增加的分支采样。 `PERF_ATR_SIZE_VER3`为96，对应于Linux 3.7中的`sample_regs_user`和`sample_stack_user`。

```c
#define PERF_ATTR_SIZE_VER0	64	/* sizeof first published struct */
#define PERF_ATTR_SIZE_VER1	72	/* add: config2 */
#define PERF_ATTR_SIZE_VER2	80	/* add: branch_sample_type */
#define PERF_ATTR_SIZE_VER3	96	/* add: sample_regs_user */
					/* add: sample_stack_user */
#define PERF_ATTR_SIZE_VER4	104	/* add: sample_regs_intr */
#define PERF_ATTR_SIZE_VER5	112	/* add: aux_watermark */
#define PERF_ATTR_SIZE_VER6	120	/* add: aux_sample_size */
```

#### 1.1.5.4. config字段

这与type字段一起指定了您想要的事件。 在64位不足以完全指定事件的情况下，也要考虑config1和config2字段。 这些字段的编码取决于事件。
config的最高有效位（bit 63）表示特定于CPU的（原始）计数器配置数据。 如果未设置最高有效位，则接下来的7位为事件类型，其余位为事件标识符。
有多种方法可以设置配置字段，具体取决于先前描述的类型字段的值。 以下是按类型分开的config的各种可能设置。

* 如果类型为`PERF_TYPE_HARDWARE`，则我们正在测量通用硬件CPU事件之一。 并非所有这些功能在所有平台上都可用。 将config设置为以下之一：
    * `PERF_COUNT_HW_CPU_CYCLES`总周期。警惕CPU频率缩放期间会发生什么
    * `PERF_COUNT_HW_INSTRUCTIONS`退休说明。请注意，这些问题可能会受到各种问题的影响，最明显的是硬件中断计数
    * `PERF_COUNT_HW_CACHE_REFERENCES`缓存访问。通常，这表示“最后一级缓存”访问，但这可能因您的CPU而异。这可能包括预取和一致性消息；同样，这取决于您的CPU的设计。
    * `PERF_COUNT_HW_CACHE_MISSES`高速缓存未命中。通常，这表明“最后一级高速缓存未命中”。它应与`PERF_COUNT_HW_CACHE_REFERENCES`事件一起使用，以计算高速缓存未命中率。
    * `PERF_COUNT_HW_BRANCH_INSTRUCTIONS`退休的分支指令。在Linux 2.6.34之前的版本中，这在AMD处理器上使用了错误的事件。
    * `PERF_COUNT_HW_BRANCH_MISSES`错误的分支指令。
    * `PERF_COUNT_HW_BUS_CYCLES`总线周期，可以与总周期不同。
    * `PERF_COUNT_HW_STALLED_CYCLES_FRONTEND`（从Linux 3.0开始）在发行过程中出现了停顿。
    * `PERF_COUNT_HW_STALLED_CYCLES_BACKEND`（从Linux 3.0开始）退休期间的停顿周期。
    * `PERF_COUNT_HW_REF_CPU_CYCLES`（从Linux 3.3开始）总周期；不受CPU频率缩放的影响。


```c
/*
 * Generalized performance event event_id types, used by the
 * attr.event_id parameter of the sys_perf_event_open()
 * syscall:
 */
enum perf_hw_id {
	/*
	 * Common hardware events, generalized by the kernel:
	 */
	PERF_COUNT_HW_CPU_CYCLES		= 0,
	PERF_COUNT_HW_INSTRUCTIONS		= 1,
	PERF_COUNT_HW_CACHE_REFERENCES		= 2,
	PERF_COUNT_HW_CACHE_MISSES		= 3,
	PERF_COUNT_HW_BRANCH_INSTRUCTIONS	= 4,
	PERF_COUNT_HW_BRANCH_MISSES		= 5,
	PERF_COUNT_HW_BUS_CYCLES		= 6,
	PERF_COUNT_HW_STALLED_CYCLES_FRONTEND	= 7,
	PERF_COUNT_HW_STALLED_CYCLES_BACKEND	= 8,
	PERF_COUNT_HW_REF_CPU_CYCLES		= 9,

	PERF_COUNT_HW_MAX,			/* non-ABI */
};
```

* 如果type为`PERF_TYPE_SOFTWARE`，则我们正在测量内核提供的软件事件。将配置设置为以下之一：
    * `PERF_COUNT_SW_CPU_CLOCK`这将报告CPU时钟，这是每个CPU的高分辨率计时器。
    * `PERF_COUNT_SW_TASK_CLOCK`这报告特定于正在运行的任务的时钟计数。
    * `PERF_COUNT_SW_PAGE_FAULTS`此报告页面错误的数量。
    * `PERF_COUNT_SW_CONTEXT_SWITCHES`这计算上下文切换。在Linux 2.6.34之前，所有这些都被报告为用户空间事件，之后它们被报告为在内核中发生。
    * `PERF_COUNT_SW_CPU_MIGRATIONS`这报告了进程已迁移到新CPU的次数。
    * `PERF_COUNT_SW_PAGE_FAULTS_MIN`此计数小页面错误的数量。这些不需要处理磁盘I / O。
    * `PERF_COUNT_SW_PAGE_FAULTS_MAJ`这将计算主要页面错误的数量。这些需要处理的磁盘I / O。
    * `PERF_COUNT_SW_ALIGNMENT_FAULTS`（从Linux 2.6.33开始）这计算对齐错误的数量。这些发生在未对齐的内存访问发生时。内核可以处理这些，但会降低性能。这仅在某些架构上才会发生（在x86上则永远不会发生）。
    * `PERF_COUNT_SW_EMULATION_FAULTS`（从Linux 2.6.33开始）这计算了仿真错误的数量。内核有时会捕获未执行的指令，并为用户空间模拟它们。这会对性能产生负面影响。

```c
/*
 * Special "software" events provided by the kernel, even if the hardware
 * does not support performance events. These events measure various
 * physical and sw events of the kernel (and allow the profiling of them as
 * well):
 */
enum perf_sw_ids {
	PERF_COUNT_SW_CPU_CLOCK			= 0,
	PERF_COUNT_SW_TASK_CLOCK		= 1,
	PERF_COUNT_SW_PAGE_FAULTS		= 2,
	PERF_COUNT_SW_CONTEXT_SWITCHES		= 3,
	PERF_COUNT_SW_CPU_MIGRATIONS		= 4,
	PERF_COUNT_SW_PAGE_FAULTS_MIN		= 5,
	PERF_COUNT_SW_PAGE_FAULTS_MAJ		= 6,
	PERF_COUNT_SW_ALIGNMENT_FAULTS		= 7,
	PERF_COUNT_SW_EMULATION_FAULTS		= 8,
	PERF_COUNT_SW_DUMMY			= 9,
	PERF_COUNT_SW_BPF_OUTPUT		= 10,

	PERF_COUNT_SW_MAX,			/* non-ABI */
};
```
如果type为`PERF_TYPE_TRACEPOINT`，则我们正在测量内核跟踪点。 如果在内核中启用了ftrace，则可从debugfs `/sys/kernel/debug/tracing/events/*/*/id`下获取在config中使用的值，如：
```
[root@localhost sched_switch]# pwd
/sys/kernel/debug/tracing/events/sched/sched_switch
[root@localhost sched_switch]# more id 
326
```

* 如果类型为`PERF_TYPE_HW_CACHE`，则我们正在测量硬件CPU缓存事件。要计算适当的配置值，请使用以下公式：

```
(perf_hw_cache_id)|(perf_hw_cache_op_id << 8)|(perf_hw_cache_op_result_id << 16)
```

其中`perf_hw_cache_id`是以下之一：

* `PERF_COUNT_HW_CACHE_L1D`用于测量1级数据缓存
* `PERF_COUNT_HW_CACHE_L1I`用于测量1级指令缓存
* `PERF_COUNT_HW_CACHE_LL`用于测量最后一级缓存
* `PERF_COUNT_HW_CACHE_DTLB`用于测量数据TLB
* `PERF_COUNT_HW_CACHE_ITLB`用于测量指令TLB
* `PERF_COUNT_HW_CACHE_BPU`用于测量分支预测单元
* `PERF_COUNT_HW_CACHE_NODE`（从Linux 3.0开始），用于测量本地内存访问

而`perf_hw_cache_op_id`是以下项之一

* `PERF_COUNT_HW_CACHE_OP_READ`用于读取访问
* `PERF_COUNT_HW_CACHE_OP_WRITE`用于写访问
* `PERF_COUNT_HW_CACHE_OP_PREFETCH`用于预取访问

而`perf_hw_cache_op_result_id`是以下项之一

* `PERF_COUNT_HW_CACHE_RESULT_ACCESS`来衡量访问
* `PERF_COUNT_HW_CACHE_RESULT_MISS`来衡量未命中率

```c
/*
 * Generalized hardware cache events:
 *
 *       { L1-D, L1-I, LLC, ITLB, DTLB, BPU, NODE } x
 *       { read, write, prefetch } x
 *       { accesses, misses }
 */
enum perf_hw_cache_id {
	PERF_COUNT_HW_CACHE_L1D			= 0,
	PERF_COUNT_HW_CACHE_L1I			= 1,
	PERF_COUNT_HW_CACHE_LL			= 2,
	PERF_COUNT_HW_CACHE_DTLB		= 3,
	PERF_COUNT_HW_CACHE_ITLB		= 4,
	PERF_COUNT_HW_CACHE_BPU			= 5,
	PERF_COUNT_HW_CACHE_NODE		= 6,

	PERF_COUNT_HW_CACHE_MAX,		/* non-ABI */
};

enum perf_hw_cache_op_id {
	PERF_COUNT_HW_CACHE_OP_READ		= 0,
	PERF_COUNT_HW_CACHE_OP_WRITE		= 1,
	PERF_COUNT_HW_CACHE_OP_PREFETCH		= 2,

	PERF_COUNT_HW_CACHE_OP_MAX,		/* non-ABI */
};

enum perf_hw_cache_op_result_id {
	PERF_COUNT_HW_CACHE_RESULT_ACCESS	= 0,
	PERF_COUNT_HW_CACHE_RESULT_MISS		= 1,

	PERF_COUNT_HW_CACHE_RESULT_MAX,		/* non-ABI */
};
```


* 如果类型为`PERF_TYPE_RAW`，则需要一个自定义的“原始”配置值。 大多数CPU支持“通用”事件未涵盖的事件。 这些是实现定义的； 请参阅您的CPU手册（例如，英特尔第3B卷文档或《 AMD BIOS和内核开发人员指南》）。 libpfm4库可用于从体系结构手册中的名称转换为该字段中期望的原始十六进制值perf_event_open（）。

* 如果类型为`PERF_TYPE_BREAKPOINT`，则将配置设置为零。 它的参数在其他地方设置。

#### 1.1.5.5. sample_period, sample_freq字段
“采样”计数器是每N个事件生成一个中断的计数器，其中N由`sample_period`给出。 采样计数器的sample_period>0。发生溢出中断时，请求的数据将记录在mmap缓冲区中。 sample_type字段控制在每个中断上记录哪些数据。

如果您希望使用频率而非周期，则可以使用`sample_freq`。 在这种情况下，您可以设置freq标志。 内核将调整采样周期以尝试达到所需的速率。 调整率是一个计时器刻度。

#### 1.1.5.6. sample_type字段
该字段中的各个位指定要包括在样本中的值。它们将被记录在环形缓冲区中，使用`mmap（2）`可在用户空间中使用。值在样本中的保存顺序在下面的MMAP布局小节中介绍；它不是枚举perf_event_sample_format顺序。

* `PERF_SAMPLE_IP`记录指令指针。
* `PERF_SAMPLE_TID`记录进程和线程ID。
* `PERF_SAMPLE_TIME`记录时间戳。
* `PERF_SAMPLE_ADDR`记录地址（如果适用）。
* `PERF_SAMPLE_READ`记录组中所有事件的计数器值，而不仅仅是组长。
* `PERF_SAMPLE_CALLCHAIN`记录调用链（堆栈回溯）。
* `PERF_SAMPLE_ID`为打开的事件的组长记录唯一的ID。
* `PERF_SAMPLE_CPU`记录CPU编号。
* `PERF_SAMPLE_PERIOD`记录当前采样周期。
* `PERF_SAMPLE_STREAM_ID`记录打开的事件的唯一ID。与PERF_SAMPLE_ID不同，返回的是实际ID，而不是组长。此ID与`PERF_FORMAT_ID`返回的ID相同。
* `PERF_SAMPLE_RAW`记录其他数据（如果适用）。通常由跟踪点事件返回。
* `PERF_SAMPLE_BRANCH_STACK`（从Linux 3.4开始），它提供了最近分支的记录，该记录由CPU分支采样硬件（例如Intel Last Branch Record）提供。并非所有硬件都支持此功能。有关如何过滤报告的分支的信息，请参见branch_sample_type字段。
* `PERF_SAMPLE_REGS_USER`（从Linux 3.7开始）记录当前用户级别的CPU寄存器状态（调用内核之前的进程中的值）。
* `PERF_SAMPLE_STACK_USER`（从Linux 3.7开始）记录用户级堆栈，从而允许展开堆栈。
* `PERF_SAMPLE_WEIGHT`（从Linux 3.10开始）记录硬件提供的权重值，该权重值表示采样事件的成本。这允许硬件在配置文件中突出显示昂贵的事件。
* `PERF_SAMPLE_DATA_SRC`（从Linux 3.10开始）记录数据源：与采样指令关联的数据在内存层次结构中的位置。仅当基础硬件支持此功能时，此选项才可用。


```c
/*
 * Bits that can be set in attr.sample_type to request information
 * in the overflow packets.
 */
enum perf_event_sample_format {
	PERF_SAMPLE_IP				= 1U << 0,
	PERF_SAMPLE_TID				= 1U << 1,
	PERF_SAMPLE_TIME			= 1U << 2,
	PERF_SAMPLE_ADDR			= 1U << 3,
	PERF_SAMPLE_READ			= 1U << 4,
	PERF_SAMPLE_CALLCHAIN			= 1U << 5,
	PERF_SAMPLE_ID				= 1U << 6,
	PERF_SAMPLE_CPU				= 1U << 7,
	PERF_SAMPLE_PERIOD			= 1U << 8,
	PERF_SAMPLE_STREAM_ID			= 1U << 9,
	PERF_SAMPLE_RAW				= 1U << 10,
	PERF_SAMPLE_BRANCH_STACK		= 1U << 11,
	PERF_SAMPLE_REGS_USER			= 1U << 12,
	PERF_SAMPLE_STACK_USER			= 1U << 13,
	PERF_SAMPLE_WEIGHT			= 1U << 14,
	PERF_SAMPLE_DATA_SRC			= 1U << 15,
	PERF_SAMPLE_IDENTIFIER			= 1U << 16,
	PERF_SAMPLE_TRANSACTION			= 1U << 17,
	PERF_SAMPLE_REGS_INTR			= 1U << 18,
	PERF_SAMPLE_PHYS_ADDR			= 1U << 19,
	PERF_SAMPLE_AUX				= 1U << 20,
	PERF_SAMPLE_CGROUP			= 1U << 21,

	PERF_SAMPLE_MAX = 1U << 22,		/* non-ABI */

	__PERF_SAMPLE_CALLCHAIN_EARLY		= 1ULL << 63, /* non-ABI; internal use */
};
```

#### 1.1.5.7. read_format
该字段指定perf_event_open（）文件描述符上read（2）返回的数据格式。

* `PERF_FORMAT_TOTAL_TIME_ENABLED`添加64位启用时间的字段。 如果PMU过量使用并且正在发生多路复用，则可将其用于计算估计总数。
* `PERF_FORMAT_TOTAL_TIME_RUNNING`添加64位time_running字段。 如果PMU过量使用并且正在发生多路复用，则可将其用于计算估计总数。
* `PERF_FORMAT_ID`添加一个与事件组相对应的64位唯一值。
* `PERF_FORMAT_GROUP`允许通过一次读取来读取事件组中的所有计数器值。

```c

/*
 * The format of the data returned by read() on a perf event fd,
 * as specified by attr.read_format:
 *
 * struct read_format {
 *	{ u64		value;
 *	  { u64		time_enabled; } && PERF_FORMAT_TOTAL_TIME_ENABLED
 *	  { u64		time_running; } && PERF_FORMAT_TOTAL_TIME_RUNNING
 *	  { u64		id;           } && PERF_FORMAT_ID
 *	} && !PERF_FORMAT_GROUP
 *
 *	{ u64		nr;
 *	  { u64		time_enabled; } && PERF_FORMAT_TOTAL_TIME_ENABLED
 *	  { u64		time_running; } && PERF_FORMAT_TOTAL_TIME_RUNNING
 *	  { u64		value;
 *	    { u64	id;           } && PERF_FORMAT_ID
 *	  }		cntr[nr];
 *	} && PERF_FORMAT_GROUP
 * };
 */
enum perf_event_read_format {
	PERF_FORMAT_TOTAL_TIME_ENABLED		= 1U << 0,
	PERF_FORMAT_TOTAL_TIME_RUNNING		= 1U << 1,
	PERF_FORMAT_ID				= 1U << 2,
	PERF_FORMAT_GROUP			= 1U << 3,

	PERF_FORMAT_MAX = 1U << 4,		/* non-ABI */
};
```

#### 1.1.5.8. disable
禁用位指定计数器开始时是禁用还是启用。 如果禁用该事件，则以后可以通过ioctl（2），prctl（2）或enable_on_exec启用该事件。

#### 1.1.5.9. inherit
继承位指定此计数器应对子任务以及指定任务的事件进行计数。 这仅适用于新子代，不适用于创建计数器时的任何现有子代（也不适用于现有子代的任何新子代）。
继承不适用于某些read_formats组合，例如PERF_FORMAT_GROUP。

#### 1.1.5.10. pinned
固定位指定计数器应尽可能始终位于CPU上。 它仅适用于硬件计数器，并且仅适用于组长。 如果无法将固定计数器放到CPU上（例如，由于没有足够的硬件计数器或由于与其他事件发生冲突），则计数器进入“错误”状态，读取返回文件末尾 （即read（2）返回0），直到随后启用或禁用计数器为止。

#### 1.1.5.11. exclusive
独占位指定当此计数器的组在CPU上时，它应该是使用CPU计数器的唯一组。 将来，这可能允许监视程序支持需要单独运行的PMU功能，以使它们不会破坏其他硬件计数器。

#### 1.1.5.12. exclude_user
如果该位置1，则计数不包括用户空间中发生的事件。

#### 1.1.5.13. exclude_kernel
如果该位置1，则计数不包括发生在内核空间中的事件。

#### 1.1.5.14. exclude_hv
如果设置了此位，则计数将排除虚拟机管理程序中发生的事件。 这主要是针对具有内置支持的PMU（例如POWER）。 在大多数机器上处理虚拟机监控程序测量需要额外的支持。

#### 1.1.5.15. exclude_idle
如果设置，则不计入CPU空闲时间。

#### 1.1.5.16. mmap
mmap位允许记录执行mmap事件。
#### 1.1.5.17. comm
comm位可跟踪由exec（2）和prctl（PR_SET_NAME）系统调用修改的进程命令名称。 不幸的是，对于工具而言，无法区分一个系统调用与另一个系统调用。

#### 1.1.5.18. freq
如果设置了此位，则在设置采样间隔时将使用sample_frequency而不是sample_period。
#### 1.1.5.19. Inherit_stat
通过该位，可以在上下文切换中保存继承任务的事件计数。 仅当设置了继承字段时，这才有意义。
#### 1.1.5.20. enable_on_exec
如果设置了此位，则在调用exec（2）之后将自动启用计数器。
#### 1.1.5.21. task
如果该位置1，则frok/exit通知将包含在环形缓冲区中。
#### 1.1.5.22. watermark
如果设置，当我们越过wakeup_watermark边界时，会发生采样中断。 否则，在wakeup_events样本之后发生中断。
#### 1.1.5.23. precision_ip
（从Linux 2.6.35开始）控制滑移量。 Skid是在感兴趣的事件发生和内核能够停止并记录该事件之间执行的指令数量。 滑移越小越好，并且可以更准确地报告哪些事件与哪些指令相对应，但是硬件通常受到限制，因为它可能很小。
其值如下：

* 0-SAMPLE_IP可以任意打滑
* 1-SAMPLE_IP必须持续滑动
* 2-SAMPLE_IP请求打滑0
* 3-SAMPLE_IP必须具有0打滑。 另请参见PERF_RECORD_MISC_EXACT_IP。
#### 1.1.5.24. mmap_data
（自Linux 2.6.36起）mmap字段的对应部分，但允许在环形缓冲区中包含数据mmap事件。
#### 1.1.5.25. sample_id_all
（从Linux 2.6.38开始）（如果已设置），那么如果选择了相应的sample_type，则TID，TIME，ID，CPU和STREAM_ID可以另外包含在非PERF_RECORD_SAMPLE中。
#### 1.1.5.26. exclude_host
（从Linux 3.2开始）不衡量在VM主机上花费的时间
#### 1.1.5.27. exclude_guest
（从Linux 3.2开始）不衡量在VM guest虚拟机上花费的时间
#### 1.1.5.28. exclude_callchain_kernel
（从Linux 3.7开始）不包括内核调用链。
#### 1.1.5.29. exclude_callchain_user
（从Linux 3.7开始）不包括用户调用链。
#### 1.1.5.30. wakeup_events，wakeup_watermark
该联合会设置在发生溢出信号之前发生的采样数（wakeup_events）或字节数（wakeup_watermark）。 水印位标志选择使用哪一个。
wakeup_events仅计数PERF_RECORD_SAMPLE记录类型。 要接收每种传入的PERF_RECORD类型的信号，请将wakeup_watermark设置为1。
#### 1.1.5.31. bp_type
（从Linux 2.6.33开始）选择断点类型。 它是以下之一：

* `HW_BREAKPOINT_EMPTY`无断点
* 当我们读取内存位置时，`HW_BREAKPOINT_R`计数
* 写入内存位置时的`HW_BREAKPOINT_W`计数
* 当我们读取或写入内存位置时，`HW_BREAKPOINT_RW`计数
* 当我们在内存位置执行代码时，`HW_BREAKPOINT_X`计数
* 这些值可以按位或进行组合，但不允许将`HW_BREAKPOINT_R`或`HW_BREAKPOINT_W`与`HW_BREAKPOINT_X`组合。

```c
enum {
	HW_BREAKPOINT_EMPTY	= 0,
	HW_BREAKPOINT_R		= 1,
	HW_BREAKPOINT_W		= 2,
	HW_BREAKPOINT_RW	= HW_BREAKPOINT_R | HW_BREAKPOINT_W,
	HW_BREAKPOINT_X		= 4,
	HW_BREAKPOINT_INVALID   = HW_BREAKPOINT_RW | HW_BREAKPOINT_X,
};
```

#### 1.1.5.32. bp_addr
（自Linux 2.6.33开始）断点的bp_addr地址。 对于执行断点，这是目标指令的内存地址； 对于读写断点，它是目标内存位置的内存地址。
#### 1.1.5.33. config1
（自Linux 2.6.39起）config1用于设置需要额外寄存器或不适合常规config字段的事件。 Nehalem / Westmere / SandyBridge上的Raw OFFCORE_EVENTS在3.3及更高版本的内核上使用此字段。
#### 1.1.5.34. bp_len
（从Linux 2.6.33开始）如果类型为PERF_TYPE_BREAKPOINT，则bp_len是要测量的断点的长度。 选项为`HW_BREAKPOINT_LEN_1`，`HW_BREAKPOINT_LEN_2`，`HW_BREAKPOINT_LEN_4`，`HW_BREAKPOINT_LEN_8`。 对于执行断点，请将其设置为sizeof（long）。

```c
enum {
	HW_BREAKPOINT_LEN_1 = 1,
	HW_BREAKPOINT_LEN_2 = 2,
	HW_BREAKPOINT_LEN_3 = 3,
	HW_BREAKPOINT_LEN_4 = 4,
	HW_BREAKPOINT_LEN_5 = 5,
	HW_BREAKPOINT_LEN_6 = 6,
	HW_BREAKPOINT_LEN_7 = 7,
	HW_BREAKPOINT_LEN_8 = 8,
};
```

#### 1.1.5.35. config2 (Since Linux 2.6.39)
config2是config1字段的进一步扩展。
#### 1.1.5.36. branch_sample_type
（自Linux 3.4开始）如果启用了PERF_SAMPLE_BRANCH_STACK，则此选项指定要在分支记录中包括哪些分支。 如果用户未明确设置特权级别，则内核将使用事件的特权级别。 事件和分支特权级别不必匹配。 尽管PERF_SAMPLE_BRANCH_ANY涵盖了所有分支类型，但是通过将零个或多个以下值进行或运算来形成该值。

* `PERF_SAMPLE_BRANCH_USER`分支目标在用户空间中
* `PERF_SAMPLE_BRANCH_KERNEL`分支目标在内核空间中
* `PERF_SAMPLE_BRANCH_HV`分支目标在管理程序中
* `PERF_SAMPLE_BRANCH_ANY`任何分支类型。
* `PERF_SAMPLE_BRANCH_ANY_CALL`任何呼叫分支
* `PERF_SAMPLE_BRANCH_ANY_RETURN`任何返回分支
* `PERF_SAMPLE_BRANCH_IND_CALL`间接调用
* `PERF_SAMPLE_BRANCH_PLM_ALL`用户，内核和HV

```c

/*
 * values to program into branch_sample_type when PERF_SAMPLE_BRANCH is set
 *
 * If the user does not pass priv level information via branch_sample_type,
 * the kernel uses the event's priv level. Branch and event priv levels do
 * not have to match. Branch priv level is checked for permissions.
 *
 * The branch types can be combined, however BRANCH_ANY covers all types
 * of branches and therefore it supersedes all the other types.
 */
enum perf_branch_sample_type_shift {
	PERF_SAMPLE_BRANCH_USER_SHIFT		= 0, /* user branches */
	PERF_SAMPLE_BRANCH_KERNEL_SHIFT		= 1, /* kernel branches */
	PERF_SAMPLE_BRANCH_HV_SHIFT		= 2, /* hypervisor branches */

	PERF_SAMPLE_BRANCH_ANY_SHIFT		= 3, /* any branch types */
	PERF_SAMPLE_BRANCH_ANY_CALL_SHIFT	= 4, /* any call branch */
	PERF_SAMPLE_BRANCH_ANY_RETURN_SHIFT	= 5, /* any return branch */
	PERF_SAMPLE_BRANCH_IND_CALL_SHIFT	= 6, /* indirect calls */
	PERF_SAMPLE_BRANCH_ABORT_TX_SHIFT	= 7, /* transaction aborts */
	PERF_SAMPLE_BRANCH_IN_TX_SHIFT		= 8, /* in transaction */
	PERF_SAMPLE_BRANCH_NO_TX_SHIFT		= 9, /* not in transaction */
	PERF_SAMPLE_BRANCH_COND_SHIFT		= 10, /* conditional branches */

	PERF_SAMPLE_BRANCH_CALL_STACK_SHIFT	= 11, /* call/ret stack */
	PERF_SAMPLE_BRANCH_IND_JUMP_SHIFT	= 12, /* indirect jumps */
	PERF_SAMPLE_BRANCH_CALL_SHIFT		= 13, /* direct call */

	PERF_SAMPLE_BRANCH_NO_FLAGS_SHIFT	= 14, /* no flags */
	PERF_SAMPLE_BRANCH_NO_CYCLES_SHIFT	= 15, /* no cycles */

	PERF_SAMPLE_BRANCH_TYPE_SAVE_SHIFT	= 16, /* save branch type */

	PERF_SAMPLE_BRANCH_HW_INDEX_SHIFT	= 17, /* save low level index of raw branch records */

	PERF_SAMPLE_BRANCH_MAX_SHIFT		/* non-ABI */
};

enum perf_branch_sample_type {
	PERF_SAMPLE_BRANCH_USER		= 1U << PERF_SAMPLE_BRANCH_USER_SHIFT,
	PERF_SAMPLE_BRANCH_KERNEL	= 1U << PERF_SAMPLE_BRANCH_KERNEL_SHIFT,
	PERF_SAMPLE_BRANCH_HV		= 1U << PERF_SAMPLE_BRANCH_HV_SHIFT,

	PERF_SAMPLE_BRANCH_ANY		= 1U << PERF_SAMPLE_BRANCH_ANY_SHIFT,
	PERF_SAMPLE_BRANCH_ANY_CALL	= 1U << PERF_SAMPLE_BRANCH_ANY_CALL_SHIFT,
	PERF_SAMPLE_BRANCH_ANY_RETURN	= 1U << PERF_SAMPLE_BRANCH_ANY_RETURN_SHIFT,
	PERF_SAMPLE_BRANCH_IND_CALL	= 1U << PERF_SAMPLE_BRANCH_IND_CALL_SHIFT,
	PERF_SAMPLE_BRANCH_ABORT_TX	= 1U << PERF_SAMPLE_BRANCH_ABORT_TX_SHIFT,
	PERF_SAMPLE_BRANCH_IN_TX	= 1U << PERF_SAMPLE_BRANCH_IN_TX_SHIFT,
	PERF_SAMPLE_BRANCH_NO_TX	= 1U << PERF_SAMPLE_BRANCH_NO_TX_SHIFT,
	PERF_SAMPLE_BRANCH_COND		= 1U << PERF_SAMPLE_BRANCH_COND_SHIFT,

	PERF_SAMPLE_BRANCH_CALL_STACK	= 1U << PERF_SAMPLE_BRANCH_CALL_STACK_SHIFT,
	PERF_SAMPLE_BRANCH_IND_JUMP	= 1U << PERF_SAMPLE_BRANCH_IND_JUMP_SHIFT,
	PERF_SAMPLE_BRANCH_CALL		= 1U << PERF_SAMPLE_BRANCH_CALL_SHIFT,

	PERF_SAMPLE_BRANCH_NO_FLAGS	= 1U << PERF_SAMPLE_BRANCH_NO_FLAGS_SHIFT,
	PERF_SAMPLE_BRANCH_NO_CYCLES	= 1U << PERF_SAMPLE_BRANCH_NO_CYCLES_SHIFT,

	PERF_SAMPLE_BRANCH_TYPE_SAVE	=
		1U << PERF_SAMPLE_BRANCH_TYPE_SAVE_SHIFT,

	PERF_SAMPLE_BRANCH_HW_INDEX	= 1U << PERF_SAMPLE_BRANCH_HW_INDEX_SHIFT,

	PERF_SAMPLE_BRANCH_MAX		= 1U << PERF_SAMPLE_BRANCH_MAX_SHIFT,
};

```

#### 1.1.5.37. sample_regs_user
（自Linux 3.7起）此位掩码定义了要在样本上转储的用户CPU寄存器的集合。 寄存器掩码的布局是特定于体系结构的，并在内核头文件`arch/ARCH/include/uapi/asm/perf_regs.h`中进行了描述。

```c
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_X86_PERF_REGS_H
#define _ASM_X86_PERF_REGS_H

enum perf_event_x86_regs {
	PERF_REG_X86_AX,
	PERF_REG_X86_BX,
	PERF_REG_X86_CX,
	PERF_REG_X86_DX,
	PERF_REG_X86_SI,
	PERF_REG_X86_DI,
	PERF_REG_X86_BP,
	PERF_REG_X86_SP,
	PERF_REG_X86_IP,
	PERF_REG_X86_FLAGS,
	PERF_REG_X86_CS,
	PERF_REG_X86_SS,
	PERF_REG_X86_DS,
	PERF_REG_X86_ES,
	PERF_REG_X86_FS,
	PERF_REG_X86_GS,
	PERF_REG_X86_R8,
	PERF_REG_X86_R9,
	PERF_REG_X86_R10,
	PERF_REG_X86_R11,
	PERF_REG_X86_R12,
	PERF_REG_X86_R13,
	PERF_REG_X86_R14,
	PERF_REG_X86_R15,
	/* These are the limits for the GPRs. */
	PERF_REG_X86_32_MAX = PERF_REG_X86_GS + 1,
	PERF_REG_X86_64_MAX = PERF_REG_X86_R15 + 1,

	/* These all need two bits set because they are 128bit */
	PERF_REG_X86_XMM0  = 32,
	PERF_REG_X86_XMM1  = 34,
	PERF_REG_X86_XMM2  = 36,
	PERF_REG_X86_XMM3  = 38,
	PERF_REG_X86_XMM4  = 40,
	PERF_REG_X86_XMM5  = 42,
	PERF_REG_X86_XMM6  = 44,
	PERF_REG_X86_XMM7  = 46,
	PERF_REG_X86_XMM8  = 48,
	PERF_REG_X86_XMM9  = 50,
	PERF_REG_X86_XMM10 = 52,
	PERF_REG_X86_XMM11 = 54,
	PERF_REG_X86_XMM12 = 56,
	PERF_REG_X86_XMM13 = 58,
	PERF_REG_X86_XMM14 = 60,
	PERF_REG_X86_XMM15 = 62,

	/* These include both GPRs and XMMX registers */
	PERF_REG_X86_XMM_MAX = PERF_REG_X86_XMM15 + 2,
};

#define PERF_REG_EXTENDED_MASK	(~((1ULL << PERF_REG_X86_XMM0) - 1))

#endif /* _ASM_X86_PERF_REGS_H */
```

#### 1.1.5.38. sample_stack_user
（从Linux 3.7开始）这定义了如果指定了PERF_SAMPLE_STACK_USER，则要转储的用户堆栈的大小。


## 1.2. 5.10.13中增加了字段

在5.10.13中增加了不少字段：

```c

/*
 * Hardware event_id to monitor via a performance monitoring event:
 *
 * @sample_max_stack: Max number of frame pointers in a callchain,
 *		      should be < /proc/sys/kernel/perf_event_max_stack
 */
struct perf_event_attr {

	/*
	 * Major type: hardware/software/tracepoint/etc.
	 */
	__u32			type;

	/*
	 * Size of the attr structure, for fwd/bwd compat.
	 */
	__u32			size;

	/*
	 * Type specific configuration information.
	 */
	__u64			config;

	union {
		__u64		sample_period;
		__u64		sample_freq;
	};

	__u64			sample_type;
	__u64			read_format;

	__u64			disabled       :  1, /* off by default        */
				inherit	       :  1, /* children inherit it   */
				pinned	       :  1, /* must always be on PMU */
				exclusive      :  1, /* only group on PMU     */
				exclude_user   :  1, /* don't count user      */
				exclude_kernel :  1, /* ditto(同上) kernel          */
				exclude_hv     :  1, /* ditto hypervisor      */
				exclude_idle   :  1, /* don't count when idle */
				mmap           :  1, /* include mmap data     */
				comm	       :  1, /* include comm data     */
				freq           :  1, /* use freq, not period  */
				inherit_stat   :  1, /* per task counts       */
				enable_on_exec :  1, /* next exec enables     */
				task           :  1, /* trace fork/exit       */
				watermark      :  1, /* wakeup_watermark      */
				/*
				 * precise_ip:
				 *
				 *  0 - SAMPLE_IP can have arbitrary skid
				 *  1 - SAMPLE_IP must have constant skid
				 *  2 - SAMPLE_IP requested to have 0 skid
				 *  3 - SAMPLE_IP must have 0 skid
				 *
				 *  See also PERF_RECORD_MISC_EXACT_IP
				 */
				precise_ip     :  2, /* skid constraint       */
				mmap_data      :  1, /* non-exec mmap data    */
				sample_id_all  :  1, /* sample_type all events */

				exclude_host   :  1, /* don't count in host   */
				exclude_guest  :  1, /* don't count in guest  */

				exclude_callchain_kernel : 1, /* exclude kernel callchains */
				exclude_callchain_user   : 1, /* exclude user callchains */
				mmap2          :  1, /* include mmap with inode data     */
				comm_exec      :  1, /* flag comm events that are due to an exec */
				use_clockid    :  1, /* use @clockid for time fields */
				context_switch :  1, /* context switch data */
				write_backward :  1, /* Write ring buffer from end to beginning */
				namespaces     :  1, /* include namespaces data */
				ksymbol        :  1, /* include ksymbol events */
				bpf_event      :  1, /* include bpf events */
				aux_output     :  1, /* generate AUX records instead of events */
				cgroup         :  1, /* include cgroup events */
				text_poke      :  1, /* include text poke events */
				__reserved_1   : 30;

	union {
		__u32		wakeup_events;	  /* wakeup every n events */
		__u32		wakeup_watermark; /* bytes before wakeup   */
	};

	__u32			bp_type;
	union {
		__u64		bp_addr;
		__u64		kprobe_func; /* for perf_kprobe */
		__u64		uprobe_path; /* for perf_uprobe */
		__u64		config1; /* extension of config */
	};
	union {
		__u64		bp_len;
		__u64		kprobe_addr; /* when kprobe_func == NULL */
		__u64		probe_offset; /* for perf_[k,u]probe */
		__u64		config2; /* extension of config1 */
	};
	__u64	branch_sample_type; /* enum perf_branch_sample_type */

	/*
	 * Defines set of user regs to dump on samples.
	 * See asm/perf_regs.h for details.
	 */
	__u64	sample_regs_user;

	/*
	 * Defines size of the user stack to dump on samples.
	 */
	__u32	sample_stack_user;

	__s32	clockid;
	/*
	 * Defines set of regs to dump for each sample
	 * state captured on:
	 *  - precise = 0: PMU interrupt
	 *  - precise > 0: sampled instruction
	 *
	 * See asm/perf_regs.h for details.
	 */
	__u64	sample_regs_intr;

	/*
	 * Wakeup watermark for AUX area
	 */
	__u32	aux_watermark;
	__u16	sample_max_stack;
	__u16	__reserved_2;
	__u32	aux_sample_size;
	__u32	__reserved_3;
};
```

## 1.3. 读取结果

一旦打开了perf_event_open（）文件描述符，就可以从文件描述符中读取事件的值。 在打开时，其中的值由attr结构中的read_format字段指定。
如果您尝试读入一个不足以容纳数据的缓冲区，则返回ENOSPC
这是读取返回的数据的布局：

如果指定`PERF_FORMAT_GROUP`允许一次读取组中的所有事件：
```c
struct read_format {
    u64 nr;            /* The number of events */
    u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */ 
    u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
    struct {
        u64 value;     /* The value of the event */
        u64 id;        /* if PERF_FORMAT_ID */ 
    } values[nr]; 
};
```
如果未指定`PERF_FORMAT_GROUP`：

```c
struct read_format { 
    u64 value;         /* The value of the event */
    u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
    u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
    u64 id;            /* if PERF_FORMAT_ID */ 
};
```

读取的值如下：

* `nr`此文件描述符中的事件数。 仅在指定了PERF_FORMAT_GROUP的情况下可用。
* `time_enabled`，`time_running`启用并运行事件的总时间。 通常，这些是相同的。 如果启动的事件多于PMU上可用计数器插槽的数量，则将发生多路复用，并且事件仅在部分时间运行。 在这种情况下，可以使用time_enabled和运行时间值来缩放计数的估计值。
* `value`包含计数器结果的无符号64位值。
* `id`此特定事件的全局唯一值，仅当在read_format中指定了PERF_FORMAT_ID时才存在。

## 1.4. MMAP 布局

在**采样模式**下使用perf_event_open（）时，**异步事件**（如计数器溢出或PROT_EXEC mmap跟踪）将记录到**环形缓冲区**中。 该环形缓冲区是通过mmap（2）创建和访问的。
mmap大小应为`1+2^n`页，其中**第一页是元数据页**（结构perf_event_mmap_page），其中包含各种信息位，例如环形缓冲区头的位置。
在内核2.6.39之前，存在一个错误，这意味着即使您不打算访问它，也必须在采样时分配mmap环形缓冲区。

### 1.4.1. `struct perf_event_mmap_page`

第一个元数据mmap页面的结构如下：

```c
struct perf_event_mmap_page { 
    __u32 version;          /* version number of this structure */
    __u32 compat_version;   /* lowest version this is compat with */
    __u32 lock;             /* seqlock for synchronization */
    __u32 index;            /* hardware counter identifier */ 
    __s64 offset;           /* add to hardware counter value */
    __u64 time_enabled;     /* time event active */
    __u64 time_running;     /* time event on CPU */
    union { 
        __u64   capabilities;
        __u64   cap_usr_time  : 1, 
                cap_usr_rdpmc : 1, 
    };
    __u16   pmc_width;
    __u16   time_shift;
    __u32   time_mult;
    __u64   time_offset;
    __u64   __reserved[120];   /* Pad to 1k */
    __u64   data_head;         /* head in the data section */ 
    __u64   data_tail;         /* user-space written tail */ 
}
```

以下内容更详细地介绍了`perf_event_mmap_page`结构中的字段：

* `version` 此结构的版本号。
* `compat_version` 与此兼容的最低版本。
* `lock` 用于同步的seqlock。
* `index` 唯一的硬件计数器标识符。
* `offset` 将其添加到硬件计数器值中？？
* `time_enabled` 事件处于活动状态的时间。
* `time_running` 事件运行的时间。
* `cap_usr_time` 用户时间功能
* `cap_usr_rdpmc` 如果硬件在不进行系统调用的情况下支持用户空间读取性能计数器（这是x86上的“ `rdpmc`”指令），则可以使用以下代码进行读取：

```c
  u32 seq, time_mult, time_shift, idx, width;
  u64 count, enabled, running;
  u64 cyc, time_offset;
  s64 pmc = 0;
  do { 
      seq = pc->lock;
      barrier();
      enabled = pc->time_enabled;
      running = pc->time_running;
      if (pc->cap_usr_time && enabled != running) { 
          cyc = rdtsc();
          time_offset = pc->time_offset;
          time_mult   = pc->time_mult;
          time_shift  = pc->time_shift; 
      }
      idx = pc->index;
      count = pc->offset;
      if (pc->cap_usr_rdpmc && idx) { 
          width = pc->pmc_width;
          pmc = rdpmc(idx - 1); 
      }
      barrier(); 
  } while (pc->lock != seq);
```

* `pmc_width` 如果是cap_usr_rdpmc，则此字段提供使用rdpmc或等效指令读取的值的位宽。 这可以用来对结果进行扩展，例如：
```c
pmc <<= 64 - pmc_width;
pmc >>= 64 - pmc_width; // signed shift right
count += pmc;
```

* `time_shift, time_mult, time_offset` 如果使用`cap_usr_time`，则这些字段可用于计算使用rdtsc或类似功能启用`time_enabled`以来的时间增量（以纳秒为单位）。

```c
u64 quot, rem;
u64 delta;
quot = (cyc >> time_shift);
rem = cyc & ((1 << time_shift) - 1);
delta = time_offset + quot * time_mult + ((rem * time_mult) >> time_shift);
```
在上述seqcount循环中读取time_offset，time_mult，time_shift和cyc。 然后可以将此增量添加到已启用且可能正在运行的状态（如果是idx），从而改善缩放比例：

```c
enabled += delta; 
if (idx) 
    running += delta; 
quot = count / running;
rem  = count % running;
count = quot * enabled + (rem * enabled) / running;
```

* `data_head` 这指向数据部分的开头。 该值持续增加，不会自动换行。 在访问样品之前，需要根据mmap缓冲区的大小手动包装该值。在支持SMP的平台上，读取data_head值后，用户空间应发出rmb（）。
* `data_tail`：当映射为`PROT_WRITE`时，应在用户空间中写入data_tail值以反映最近读取的数据。 在这种情况下，内核将不会覆盖未读取的数据。

以下2 ^ n个环形缓冲区页面具有以下描述的布局。

如果设置了`perf_event_attr.sample_id_all`，则所有事件类型都将具有与以下`PERF_RECORD_SAMPLE`中描述的事件发生的时间/时间（身份）相关的所选sample_type字段（TID，TIME，ID，CPU，STREAM_ID），它将被隐藏 在perf_event_header和现有字段已经存在的字段之后，即在有效负载的末尾。 这样，较旧的perf工具将支持较新的`perf.data`文件，而这些新的可选字段将被忽略。


在5.10.13源码中的定义为：

```c
/*
 * Structure of the page that can be mapped via mmap
 */
struct perf_event_mmap_page {   /*  */
	__u32	version;		/* version number of this structure */
	__u32	compat_version;		/* lowest version this is compat with */

	/*
	 * Bits needed to read the hw events in user-space.
	 *
	 *   u32 seq, time_mult, time_shift, index, width;
	 *   u64 count, enabled, running;
	 *   u64 cyc, time_offset;
	 *   s64 pmc = 0;
	 *
	 *   do {
	 *     seq = pc->lock;
	 *     barrier()
	 *
	 *     enabled = pc->time_enabled;
	 *     running = pc->time_running;
	 *
	 *     if (pc->cap_usr_time && enabled != running) {
	 *       cyc = rdtsc();
	 *       time_offset = pc->time_offset;
	 *       time_mult   = pc->time_mult;
	 *       time_shift  = pc->time_shift;
	 *     }
	 *
	 *     index = pc->index;
	 *     count = pc->offset;
	 *     if (pc->cap_user_rdpmc && index) {
	 *       width = pc->pmc_width;
	 *       pmc = rdpmc(index - 1);
	 *     }
	 *
	 *     barrier();
	 *   } while (pc->lock != seq);
	 *
	 * NOTE: for obvious reason this only works on self-monitoring
	 *       processes.
	 */
	__u32	lock;			/* seqlock for synchronization */
	__u32	index;			/* hardware event identifier */
	__s64	offset;			/* add to hardware event value */
	__u64	time_enabled;		/* time event active */
	__u64	time_running;		/* time event on cpu */
	union {
		__u64	capabilities;
		struct {
			__u64	cap_bit0		: 1, /* Always 0, deprecated, see commit 860f085b74e9 */
				cap_bit0_is_deprecated	: 1, /* Always 1, signals that bit 0 is zero */

				cap_user_rdpmc		: 1, /* The RDPMC instruction can be used to read counts */
				cap_user_time		: 1, /* The time_{shift,mult,offset} fields are used */
				cap_user_time_zero	: 1, /* The time_zero field is used */
				cap_user_time_short	: 1, /* the time_{cycle,mask} fields are used */
				cap_____res		: 58;
		};
	};

	/*
	 * If cap_user_rdpmc this field provides the bit-width of the value
	 * read using the rdpmc() or equivalent instruction. This can be used
	 * to sign extend the result like:
	 *
	 *   pmc <<= 64 - width;
	 *   pmc >>= 64 - width; // signed shift right
	 *   count += pmc;
	 */
	__u16	pmc_width;

	/*
	 * If cap_usr_time the below fields can be used to compute the time
	 * delta since time_enabled (in ns) using rdtsc or similar.
	 *
	 *   u64 quot, rem;
	 *   u64 delta;
	 *
	 *   quot = (cyc >> time_shift);
	 *   rem = cyc & (((u64)1 << time_shift) - 1);
	 *   delta = time_offset + quot * time_mult +
	 *              ((rem * time_mult) >> time_shift);
	 *
	 * Where time_offset,time_mult,time_shift and cyc are read in the
	 * seqcount loop described above. This delta can then be added to
	 * enabled and possible running (if index), improving the scaling:
	 *
	 *   enabled += delta;
	 *   if (index)
	 *     running += delta;
	 *
	 *   quot = count / running;
	 *   rem  = count % running;
	 *   count = quot * enabled + (rem * enabled) / running;
	 */
	__u16	time_shift;
	__u32	time_mult;
	__u64	time_offset;
	/*
	 * If cap_usr_time_zero, the hardware clock (e.g. TSC) can be calculated
	 * from sample timestamps.
	 *
	 *   time = timestamp - time_zero;
	 *   quot = time / time_mult;
	 *   rem  = time % time_mult;
	 *   cyc = (quot << time_shift) + (rem << time_shift) / time_mult;
	 *
	 * And vice versa:
	 *
	 *   quot = cyc >> time_shift;
	 *   rem  = cyc & (((u64)1 << time_shift) - 1);
	 *   timestamp = time_zero + quot * time_mult +
	 *               ((rem * time_mult) >> time_shift);
	 */
	__u64	time_zero;

	__u32	size;			/* Header size up to __reserved[] fields. */
	__u32	__reserved_1;

	/*
	 * If cap_usr_time_short, the hardware clock is less than 64bit wide
	 * and we must compute the 'cyc' value, as used by cap_usr_time, as:
	 *
	 *   cyc = time_cycles + ((cyc - time_cycles) & time_mask)
	 *
	 * NOTE: this form is explicitly chosen such that cap_usr_time_short
	 *       is a correction on top of cap_usr_time, and code that doesn't
	 *       know about cap_usr_time_short still works under the assumption
	 *       the counter doesn't wrap.
	 */
	__u64	time_cycles;
	__u64	time_mask;

		/*
		 * Hole for extension of the self monitor capabilities
		 */

	__u8	__reserved[116*8];	/* align to 1k. */

	/*
	 * Control data for the mmap() data buffer.
	 *
	 * User-space reading the @data_head value should issue an smp_rmb(),
	 * after reading this value.
	 *
	 * When the mapping is PROT_WRITE the @data_tail value should be
	 * written by userspace to reflect the last read data, after issueing
	 * an smp_mb() to separate the data read from the ->data_tail store.
	 * In this case the kernel will not over-write unread data.
	 *
	 * See perf_output_put_handle() for the data ordering.
	 *
	 * data_{offset,size} indicate the location and size of the perf record
	 * buffer within the mmapped area.
	 */
	__u64   data_head;		/* head in the data section */
	__u64	data_tail;		/* user-space written tail */
	__u64	data_offset;		/* where the buffer starts */
	__u64	data_size;		/* data buffer size */

	/*
	 * AUX area is defined by aux_{offset,size} fields that should be set
	 * by the userspace, so that
	 *
	 *   aux_offset >= data_offset + data_size
	 *
	 * prior to mmap()ing it. Size of the mmap()ed area should be aux_size.
	 *
	 * Ring buffer pointers aux_{head,tail} have the same semantics as
	 * data_{head,tail} and same ordering rules apply.
	 */
	__u64	aux_head;
	__u64	aux_tail;
	__u64	aux_offset;
	__u64	aux_size;
};
```

### 1.4.2. `struct perf_event_header`

mmap以下面结构开始：

```c
struct perf_event_header { 
    __u32   type;
    __u16   misc;
    __u16   size;
};
```
下面，我们更详细地描述perf_event_header字段。

#### 1.4.2.1. type

类型值是以下值之一。 相应记录（在标题之后）中的值取决于所选择的类型，如图所示。

```c
enum perf_event_type {
	PERF_RECORD_MMAP			= 1,
	PERF_RECORD_LOST			= 2,
	PERF_RECORD_COMM			= 3,
	PERF_RECORD_EXIT			= 4,
	PERF_RECORD_THROTTLE		= 5,
	PERF_RECORD_UNTHROTTLE		= 6,
	PERF_RECORD_FORK			= 7,
	PERF_RECORD_READ			= 8,
	PERF_RECORD_SAMPLE			= 9,
	PERF_RECORD_MMAP2			= 10,
	PERF_RECORD_AUX				= 11,
	PERF_RECORD_ITRACE_START	= 12,
	PERF_RECORD_LOST_SAMPLES	= 13,
	PERF_RECORD_SWITCH			= 14,
	PERF_RECORD_SWITCH_CPU_WIDE	= 15,

	PERF_RECORD_MAX,			/* non-ABI */
};
```

##### 1.4.2.1.1. `PERF_RECORD_MMAP`
MMAP事件记录了PROT_EXEC映射，因此我们可以将用户空间IP与代码相关联。 它们具有以下结构：
```c
struct { 
    struct perf_event_header header;
    u32    pid, tid;
    u64    addr;
    u64    len;
    u64    pgoff;
    char   filename[]; 
};
```

##### 1.4.2.1.2. `PERF_RECORD_LOST`
该记录指示事件丢失的时间。
```c
struct { 
    struct perf_event_header header;
    u64 id;
    u64 lost; 
};
```
* id是丢失的样本的唯一事件ID。
* lost是丢失事件的数量。

##### 1.4.2.1.3. `PERF_RECORD_COMM`
该记录表明进程名称已更改。
```c
struct { 
    struct perf_event_header header;
    u32 pid, tid;
    char comm[];
};
```

##### 1.4.2.1.4. `PERF_RECORD_EXIT`
该记录指示进程退出事件。
```c
struct {
    struct perf_event_header header;
    u32 pid, ppid;
    u32 tid, ptid;
    u64 time; 
};
```

##### 1.4.2.1.5. `PERF_RECORD_THROTTLE`,`PERF_RECORD_UNTHROTTLE`
该记录指示门事件。
```c
struct {
    struct perf_event_header header;
    u64 time;
    u64 id;
    u64 stream_id; 
};
```

##### 1.4.2.1.6. `PERF_RECORD_FORK`
该记录指示fork事件。
```c
struct { 
    struct perf_event_header header;
    u32 pid, ppid;
    u32 tid, ptid;
    u64 time; 
};
```

##### 1.4.2.1.7. `PERF_RECORD_READ`
该记录指示读取事件。
```c
struct {
    struct perf_event_header header;
    u32 pid, tid;
    struct read_format values; 
};
```

##### 1.4.2.1.8. `PERF_RECORD_SAMPLE`
该记录表示一个样本。
```c
struct {
    struct perf_event_header header;
    u64   ip;         /* if PERF_SAMPLE_IP */
    u32   pid, tid;   /* if PERF_SAMPLE_TID */
    u64   time;       /* if PERF_SAMPLE_TIME */ 
    u64   addr;       /* if PERF_SAMPLE_ADDR */
    u64   id;         /* if PERF_SAMPLE_ID */
    u64   stream_id;  /* if PERF_SAMPLE_STREAM_ID */ 
    u32   cpu, res;   /* if PERF_SAMPLE_CPU */
    u64   period;     /* if PERF_SAMPLE_PERIOD */ 
    struct read_format v; /* if PERF_SAMPLE_READ */
    u64   nr;         /* if PERF_SAMPLE_CALLCHAIN */
    u64   ips[nr];    /* if PERF_SAMPLE_CALLCHAIN */
    u32   size;       /* if PERF_SAMPLE_RAW */
    char  data[size]; /* if PERF_SAMPLE_RAW */
    u64   bnr;        /* if PERF_SAMPLE_BRANCH_STACK */
    struct perf_branch_entry lbr[bnr]; /* if PERF_SAMPLE_BRANCH_STACK */ 
    u64   abi;        /* if PERF_SAMPLE_REGS_USER */
    u64   regs[weight(mask)]; /* if PERF_SAMPLE_REGS_USER */
    u64   size;       /* if PERF_SAMPLE_STACK_USER */
    char  data[size]; /* if PERF_SAMPLE_STACK_USER */ 
    u64   dyn_size;   /* if PERF_SAMPLE_STACK_USER */
    u64   weight;     /* if PERF_SAMPLE_WEIGHT */ 
    u64   data_src;   /* if PERF_SAMPLE_DATA_SRC */ 
};
```


##### 1.4.2.1.9. `PERF_RECORD_MMAP2`
```c
	/*
	 * The MMAP2 records are an augmented version of MMAP, they add
	 * maj, min, ino numbers to be used to uniquely identify each mapping
	 *
	 * struct {
	 *	struct perf_event_header	header;
	 *
	 *	u32				pid, tid;
	 *	u64				addr;
	 *	u64				len;
	 *	u64				pgoff;
	 *	u32				maj;
	 *	u32				min;
	 *	u64				ino;
	 *	u64				ino_generation;
	 *	u32				prot, flags;
	 *	char				filename[];
	 * 	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_MMAP2			= 10,
```

##### 1.4.2.1.10. `PERF_RECORD_AUX`
```c
	/*
	 * Records that new data landed in the AUX buffer part.
	 *
	 * struct {
	 * 	struct perf_event_header	header;
	 *
	 * 	u64				aux_offset;
	 * 	u64				aux_size;
	 *	u64				flags;
	 * 	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_AUX				= 11,
```
##### 1.4.2.1.11. `PERF_RECORD_ITRACE_START`
```c
	/*
	 * Indicates that instruction trace has started
	 *
	 * struct {
	 *	struct perf_event_header	header;
	 *	u32				pid;
	 *	u32				tid;
	 *	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_ITRACE_START		= 12,
```
##### 1.4.2.1.12. `PERF_RECORD_LOST_SAMPLES`
```c
	/*
	 * Records the dropped/lost sample number.
	 *
	 * struct {
	 *	struct perf_event_header	header;
	 *
	 *	u64				lost;
	 *	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_LOST_SAMPLES		= 13,
```
##### 1.4.2.1.13. `PERF_RECORD_SWITCH`
```c
	/*
	 * Records a context switch in or out (flagged by
	 * PERF_RECORD_MISC_SWITCH_OUT). See also
	 * PERF_RECORD_SWITCH_CPU_WIDE.
	 *
	 * struct {
	 *	struct perf_event_header	header;
	 *	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_SWITCH			= 14,
```
##### 1.4.2.1.14. `PERF_RECORD_SWITCH_CPU_WIDE`
```c
	/*
	 * CPU-wide version of PERF_RECORD_SWITCH with next_prev_pid and
	 * next_prev_tid that are the next (switching out) or previous
	 * (switching in) pid/tid.
	 *
	 * struct {
	 *	struct perf_event_header	header;
	 *	u32				next_prev_pid;
	 *	u32				next_prev_tid;
	 *	struct sample_id		sample_id;
	 * };
	 */
	PERF_RECORD_SWITCH_CPU_WIDE		= 15,
```

#### 1.4.2.2. misc

misc字段包含有关样本的其他信息。
通过使用`PERF_RECORD_MISC_CPUMODE_MASK`进行屏蔽并寻找以下选项之一，可以从该值确定CPU模式（请注意，这些不是位掩码，一次只能设置一个）。

```c

#define PERF_RECORD_MISC_CPUMODE_MASK		(7 << 0)
#define PERF_RECORD_MISC_CPUMODE_UNKNOWN	(0 << 0)
#define PERF_RECORD_MISC_KERNEL			(1 << 0)
#define PERF_RECORD_MISC_USER			(2 << 0)
#define PERF_RECORD_MISC_HYPERVISOR		(3 << 0)
#define PERF_RECORD_MISC_GUEST_KERNEL		(4 << 0)
#define PERF_RECORD_MISC_GUEST_USER		(5 << 0)

/*
 * Indicates that /proc/PID/maps parsing are truncated by time out.
 */
#define PERF_RECORD_MISC_PROC_MAP_PARSE_TIMEOUT	(1 << 12)
/*
 * Following PERF_RECORD_MISC_* are used on different
 * events, so can reuse the same bit position:
 *
 *   PERF_RECORD_MISC_MMAP_DATA  - PERF_RECORD_MMAP* events
 *   PERF_RECORD_MISC_COMM_EXEC  - PERF_RECORD_COMM event
 *   PERF_RECORD_MISC_FORK_EXEC  - PERF_RECORD_FORK event (perf internal)
 *   PERF_RECORD_MISC_SWITCH_OUT - PERF_RECORD_SWITCH* events
 */
#define PERF_RECORD_MISC_MMAP_DATA		(1 << 13)
#define PERF_RECORD_MISC_COMM_EXEC		(1 << 13)
#define PERF_RECORD_MISC_FORK_EXEC		(1 << 13)
#define PERF_RECORD_MISC_SWITCH_OUT		(1 << 13)
/*
 * These PERF_RECORD_MISC_* flags below are safely reused
 * for the following events:
 *
 *   PERF_RECORD_MISC_EXACT_IP           - PERF_RECORD_SAMPLE of precise events
 *   PERF_RECORD_MISC_SWITCH_OUT_PREEMPT - PERF_RECORD_SWITCH* events
 *
 *
 * PERF_RECORD_MISC_EXACT_IP:
 *   Indicates that the content of PERF_SAMPLE_IP points to
 *   the actual instruction that triggered the event. See also
 *   perf_event_attr::precise_ip.
 *
 * PERF_RECORD_MISC_SWITCH_OUT_PREEMPT:
 *   Indicates that thread was preempted in TASK_RUNNING state.
 */
#define PERF_RECORD_MISC_EXACT_IP		(1 << 14)
#define PERF_RECORD_MISC_SWITCH_OUT_PREEMPT	(1 << 14)
/*
 * Reserve the last bit to indicate some extended misc field
 */
#define PERF_RECORD_MISC_EXT_RESERVED		(1 << 15)
```

* `PERF_RECORD_MISC_CPUMODE_UNKNOWN`未知的CPU模式。
* `PERF_RECORD_MISC_KERNEL`样本发生在内核中。
* `PERF_RECORD_MISC_USER`示例在用户代码中发生。
* `PERF_RECORD_MISC_HYPERVISOR`样本在管理程序中发生。
* `PERF_RECORD_MISC_GUEST_KERNEL`样本发生在客户机内核中。
* `PERF_RECORD_MISC_GUEST_USER`来宾用户代码中发生了示例。

此外，可以设置以下位之一：

* `PERF_RECORD_MISC_MMAP_DATA`当映射不可执行时设置。 否则，映射是可执行的。
* `PERF_RECORD_MISC_EXACT_IP`这表明PERF_SAMPLE_IP的内容指向触发事件的实际指令。 另请参见perf_event_attr.precise_ip。
* `PERF_RECORD_MISC_EXT_RESERVED`这表示存在扩展数据可用（当前未使用）。

#### 1.4.2.3. size
这表示记录的大小。

## 1.5. 信号溢出

可以设置事件以在超过阈值时传递信号。 信号处理程序是使用`poll(2)`, `select(2)`, `epoll(2)` and `fcntl(2)`系统调用来设置的。
要生成信号，必须启用采样（`sample_period`必须具有非零值）。

有两种产生信号的方法。

* 首先是设置一个`wakeup_events`或`wakeup_watermark`值，如果已将一定数量的样本或字节写入mmap环形缓冲区，则将生成信号。 在这种情况下，将发送`POLL_IN`类型的信号。
* 另一种方法是使用`PERF_EVENT_IOC_REFRESH` ioctl。 该ioctl添加到一个计数器，该计数器在每次事件溢出时都会减少。 非零时，将在溢出时发送`POLL_IN`信号，但是一旦该值达到0，将发送`POLL_HUP`类型的信号，并禁用基础事件。

> 注意：在较新的内核（在3.2中明确指出）上，即使未设置wakeup_events，也会为每次溢出提供信号。

## 1.6. rdpmc指令

从x86上的Linux 3.4开始，您**可以使用rdpmc指令获得低延迟读取，而不必进入内核**。 请注意，使用rdpmc不一定比其他读取事件值的方法快。
可以通过mmap页面中的`cap_usr_rdpmc`字段检测到对此的支持。 在该部分中可以找到有关如何计算事件值的文档。

## 1.7. `perf_event`的`ioctl`调用

各种ioctl作用于perf_event_open（）文件描述符

```c
/*
 * Ioctls that can be done on a perf event fd:
 */
#define PERF_EVENT_IOC_ENABLE			_IO ('$', 0)
#define PERF_EVENT_IOC_DISABLE			_IO ('$', 1)
#define PERF_EVENT_IOC_REFRESH			_IO ('$', 2)
#define PERF_EVENT_IOC_RESET			_IO ('$', 3)
#define PERF_EVENT_IOC_PERIOD			_IOW('$', 4, __u64)
#define PERF_EVENT_IOC_SET_OUTPUT		_IO ('$', 5)
#define PERF_EVENT_IOC_SET_FILTER		_IOW('$', 6, char *)
#define PERF_EVENT_IOC_ID			_IOR('$', 7, __u64 *)
#define PERF_EVENT_IOC_SET_BPF			_IOW('$', 8, __u32)
#define PERF_EVENT_IOC_PAUSE_OUTPUT		_IOW('$', 9, __u32)
#define PERF_EVENT_IOC_QUERY_BPF		_IOWR('$', 10, struct perf_event_query_bpf *)
#define PERF_EVENT_IOC_MODIFY_ATTRIBUTES	_IOW('$', 11, struct perf_event_attr *)
```

* `PERF_EVENT_IOC_ENABLE`启用文件描述符参数指定的单个事件或事件组。
    * 如果ioctl参数中的PERF_IOC_FLAG_GROUP位置1，则即使指定的事件不是组长（但请参阅BUGS），也会启用组中的所有事件。
* `PERF_EVENT_IOC_DISABLE`禁用文件描述符参数指定的单个计数器或事件组。
    * 启用或禁用组长可以启用或禁用整个组；也就是说，在禁用组长时，组中的任何计数器都不会计数。启用或禁用组长（而不是组长）的成员只会影响该计数器；禁用非领导者可以停止对该计数器进行计数，但不会影响其他任何计数器。
    * 如果ioctl参数中的PERF_IOC_FLAG_GROUP位置1，则即使指定的事件不是组长（但请参阅BUGS），也将禁用组中的所有事件。
* `PERF_EVENT_IOC_REFRESH`非继承的溢出计数器可以使用它来启用针对参数指定的大量溢出的计数器，然后将其禁用。此ioctl的后续调用会将参数值添加到当前计数中。每次溢出前都会产生一个设置了POLL_IN的信号，直到计数达到0为止；否则，每次溢出都会产生一个信号。当发生这种情况时，将发送设置了POLL_HUP的信号，并禁用该事件。使用参数0认为是未定义的行为。
* `PERF_EVENT_IOC_RESET`将文件描述符参数指定的事件计数重置为零。这只会重置计数；无法重置多路复用的time_enabled或time_running值。
    * 如果ioctl参数中的PERF_IOC_FLAG_GROUP位置1，则即使指定的事件不是组长（但请参阅BUGS），也会重置组中的所有事件。
* `PERF_EVENT_IOC_PERIOD IOC_PERIOD`是更新时间段的命令；它不会更新当前时间段，而是推迟到下一个时间段。
    * 该参数是指向包含所需新周期的64位值的指针。
* `PERF_EVENT_IOC_SET_OUTPUT`这告诉内核将事件通知报告给指定的文件描述符，而不是默认的文件描述符。文件描述符必须全部在同一CPU上。
    * 该参数指定所需的文件描述符，如果应忽略输出，则为-1。
* `PERF_EVENT_IOC_SET_FILTER`（从Linux 2.6.33开始）这将向该事件添加ftrace过滤器。
    * 该参数是指向所需ftrace过滤器的指针。

## 1.8. 使用`prctl`

进程可以使用`prctl(2)` `PR_TASK_PERF_EVENTS_ENABLE`和`PR_TASK_PERF_EVENTS_DISABLE`操作来启用或禁用附加到该事件组的所有事件组。 这适用于当前进程上的所有计数器，无论是由该进程还是其他进程创建的计数器，都不会影响此进程在其他进程上创建的任何计数器。 它仅启用或禁用组长，而不启用或禁用组中的任何其他成员。

```c
#define PR_TASK_PERF_EVENTS_DISABLE		31
#define PR_TASK_PERF_EVENTS_ENABLE		32

#include <sys/prctl.h>

int prctl(int option, unsigned long arg2, unsigned long arg3,
         unsigned long arg4, unsigned long arg5);
```

## 1.9. `perf_event`相关的配置文件

### 1.9.1. `/proc/sys/kernel/`

#### 1.9.1.1. `/proc/sys/kernel/perf_event_paranoid`

可以设置`perf_event_paranoid`文件以限制对性能计数器的访问。

* **2**-仅允许用户空间测量
* **1**-（默认）允许内核和用户测量
* **0**-允许访问特定于CPU的数据，但不访问原始跟踪点样本
* **-1**-无限制

`perf_event_paranoid`文件的存在是确定内核是否支持`perf_event_open()`的正式方法。

#### 1.9.1.2. `/proc/sys/kernel/perf_event_max_sample_rate`

设置**最大采样率**。 将此值设置得太高会导致用户以影响整体机器性能的速率进行采样，并有可能锁定机器。 默认值为100000（每秒采样数）。

#### 1.9.1.3. `/proc/sys/kernel/perf_event_mlock_kb`

非特权用户可以锁定（`mlock(2)`）的最大页面数。 默认值为516（kB）。

### 1.9.2. `/sys/bus/event_source/devices/`

从Linux 2.6.34开始，内核支持具有多个可用于监视的PMU。 有关如何对这些PMU进行编程的信息，可以在`/sys/bus/event_source/devices/`下找到。 每个子目录对应于一个不同的PMU。

#### 1.9.2.1. `/sys/bus/event_source/devices/*/type`
(Since Linux 2.6.38)
它包含一个可以在`perf_event_attr`的`type`字段中使用的整数，以指示您希望使用此PMU。

#### 1.9.2.2. `/sys/bus/event_source/devices/*/rdpmc`
(Since Linux 3.4)
如果该文件为1，则**可以通过rdpmc指令直接在用户空间中访问性能计数器寄存器**。 可以通过在文件中回显0来禁用此功能。

#### 1.9.2.3. `/sys/bus/event_source/devices/*/format/`
(Since Linux 3.4)
该子目录包含有关特定于体系结构的子字段的信息，这些子字段可用于对`perf_event_attr`结构中的各种配置字段进行编程。
每个文件的内容是config字段的名称，后跟一个冒号，然后是一系列用逗号分隔的整数位范围。 例如，文件事件可能包含值`config1:1,6-10,44`，该值指示事件是占据`perf_event_attr::config1`的位`1,6-10`和`44`的属性。

在[Linux内核 eBPF基础：perf（3）用户态指令分析](https://blog.csdn.net/Rong_Toa/article/details/117027406)中有过示例：
```c
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
```

#### 1.9.2.4. `/sys/bus/event_source/devices/*/events/`
(Since Linux 3.4)
**该子目录包含具有预定义事件的文件**。 内容是描述事件设置的字符串，这些事件设置是根据前面提到的`/sys/bus/event_source/devices/*/format/`目录中的字段表示的。 这些不一定是PMU支持的所有事件的完整列表，而是通常被认为有用或有趣的事件子集。
每个文件的内容是由逗号分隔的属性名称的列表。 每个条目都有一个可选值（十六进制或十进制）。 如果未指定任何值，则假定它是一个值为1的单个位字段。示例条目可能如下所示：`event=0x2,inv,ldlat=3`

在[Linux内核 eBPF基础：perf（3）用户态指令分析](https://blog.csdn.net/Rong_Toa/article/details/117027406)中有过示例：

```c
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
```

#### 1.9.2.5. `/sys/bus/event_source/devices/*/uevent`
该文件是用于注入热插拔事件的标准内核设备接口。

#### 1.9.2.6. `/sys/bus/event_source/devices/*/cpumask`
(Since Linux 3.7)
cpumask文件包含一个逗号分隔的整数列表，这些整数表示主板上每个插槽（包装）的代表性cpu编号。 **设置非核心或北桥事件时，这是必需的**，因为这些PMU会显示套接字范围的事件。

在[Linux内核 eBPF基础：perf（3）用户态指令分析](https://blog.csdn.net/Rong_Toa/article/details/117027406)中有过示例：
```c
/sys/bus/event_source/devices/power/cpumask    0-3
```

## 1.10. 返回值

perf_event_open（）返回**新的文件描述符**，如果发生错误则返回**-1**（在这种情况下，将正确设置errno）。

## 1.11. 错误码

* `EINVAL`如果指定的事件不可用，则返回。
* `ENOSPC`在Linux 3.3之前，如果没有足够的空间容纳事件，则返回ENOSPC。 Linus不喜欢这样，因此将其更改为EINVAL。 如果您尝试将结果读取到太小的缓冲区中，则仍会返回ENOSPC。

## 1.12. manual中的实例代码

```c
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid, 
                int cpu, int group_fd, unsigned long flags) { 
   int ret;
   ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags); 
   return ret; 
}
int
main(int argc, char **argv)
{
   struct perf_event_attr pe;
   long long count;
   int fd;
   memset(&pe, 0, sizeof(struct perf_event_attr));
   pe.type = PERF_TYPE_HARDWARE;
   pe.size = sizeof(struct perf_event_attr);
   pe.config = PERF_COUNT_HW_INSTRUCTIONS;
   pe.disabled = 1;
   pe.exclude_kernel = 1;
   pe.exclude_hv = 1;
   fd = perf_event_open(&pe, 0, -1, -1, 0);
   if (fd == -1) { 
       fprintf(stderr, "Error opening leader %llx\n", pe.config);
       exit(EXIT_FAILURE); 
   }
   ioctl(fd, PERF_EVENT_IOC_RESET, 0);
   ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
   printf("Measuring instruction count for this printf\n");
   ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
   read(fd, &count, sizeof(long long));
   printf("Used %lld instructions\n", count);
   close(fd); }
```

# 2. 示例代码

网址：[https://github.com/Rtoax/test/tree/master/c/glibc/linux/perf_event](https://github.com/Rtoax/test/tree/master/c/glibc/linux/perf_event)

```c
/*
https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events

perf stat -e cycles,faults ls

*/
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

struct read_format {
    uint64_t nr;
    struct {
        uint64_t value;
        uint64_t id;
    } values[];
};

void do_malloc() {

    int i;
    char* ptr;
    int len = 2*1024*1024;
    ptr = malloc(len);
    
    mlock(ptr, len);
    
    for (i = 0; i < len; i++) {
        ptr[i] = (char) (i & 0xff); // pagefault
    }
    
    free(ptr);
}

void do_ls() {
    system("/bin/ls");
}

void do_something(int something) {
    
    switch(something) {
    case 1:
        do_ls();
        break;
    case 0:
    default:
        do_malloc();
        break;
    }
}

int create_hardware_perf(int grp_fd, enum perf_hw_id hw_ids, uint64_t *ioc_id)
{
    if(PERF_COUNT_HW_MAX <= hw_ids || hw_ids < 0) {
        printf("Unsupport enum perf_hw_id.\n");
        return -1;
    }
    
    struct perf_event_attr pea;
    
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_HARDWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = hw_ids;
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    int fd = syscall(__NR_perf_event_open, &pea, 0, -1, grp_fd>2?grp_fd:-1, 0);
    ioctl(fd, PERF_EVENT_IOC_ID, ioc_id);

    return fd;
}

int create_software_perf(int grp_fd, enum perf_sw_ids sw_ids, uint64_t *ioc_id)
{
    if(PERF_COUNT_SW_MAX <= sw_ids || sw_ids < 0) {
        printf("Unsupport enum perf_sw_ids.\n");
        return -1;
    }

    struct perf_event_attr pea;
    
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_SOFTWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = sw_ids;
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    int fd = syscall(__NR_perf_event_open, &pea, 0, -1, grp_fd>2?grp_fd:-1 /*!!!*/, 0);
    ioctl(fd, PERF_EVENT_IOC_ID, ioc_id);

    return fd;
}


int main(int argc, char* argv[]) 
{
    struct perf_event_attr pea;
    
    int group_fd, fd2, fd3, fd4, fd5;
    uint64_t id1, id2, id3, id4, id5;
    uint64_t val1, val2, val3, val4, val5;
    char buf[4096];
    struct read_format* rf = (struct read_format*) buf;
    int i;

    group_fd = create_hardware_perf(-1, PERF_COUNT_HW_CPU_CYCLES, &id1);
    
    fd2 = create_hardware_perf(group_fd, PERF_COUNT_HW_CACHE_MISSES, &id2);
    fd3 = create_software_perf(group_fd, PERF_COUNT_SW_PAGE_FAULTS, &id3);
    fd4 = create_software_perf(group_fd, PERF_COUNT_SW_CPU_CLOCK, &id4);
    fd5 = create_software_perf(group_fd, PERF_COUNT_SW_CPU_CLOCK, &id5);

    printf("ioctl %ld, %ld, %ld, %ld, %ld\n", id1, id2, id3, id4, id5);

    ioctl(group_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(group_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

    do_something(1);
    
    ioctl(group_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);


    read(group_fd, buf, sizeof(buf));
    for (i = 0; i < rf->nr; i++) {
        if (rf->values[i].id == id1) {
            val1 = rf->values[i].value;
        } else if (rf->values[i].id == id2) {
            val2 = rf->values[i].value;
        } else if (rf->values[i].id == id3) {
            val3 = rf->values[i].value;
        } else if (rf->values[i].id == id4) {
            val4 = rf->values[i].value;
        } else if (rf->values[i].id == id5) {
            val5 = rf->values[i].value;
        }
    }

    printf("cpu cycles:     %"PRIu64"\n", val1);
    printf("cache misses:   %"PRIu64"\n", val2);
    printf("page faults:    %"PRIu64"\n", val3);
    printf(" cpu clock:     %"PRIu64"\n", val4);
    printf("task clock:     %"PRIu64"\n", val5);

    close(group_fd);
    close(fd2);
    close(fd3);
    close(fd4);
    close(fd5);

    return 0;
}
```




# 3. 相关链接

* [注释源码：https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核 eBPF基础：perf（1）：perf_event在内核中的初始化](https://rtoax.blog.csdn.net/article/details/116982544)
* [Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)
* [Linux内核 eBPF基础：perf（3）用户态指令分析](https://rtoax.blog.csdn.net/article/details/117027406)
* [Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)
* [https://rtoax.blog.csdn.net/article/details/117040529](https://rtoax.blog.csdn.net/article/details/117040529)
* [Linux kernel perf architecture](http://terenceli.github.io/技术/2020/08/29/perf-arch)
* [Linux perf 1.1、perf_event内核框架](https://blog.csdn.net/pwl999/article/details/81200439)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)
* [https://www.kernel.org/doc/man-pages/](https://www.kernel.org/doc/man-pages/)
