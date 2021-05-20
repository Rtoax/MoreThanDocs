<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>perf（5）perf_event_open系统调用内核源码分析</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月19日</font></center>
<br/>


* 本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)


# 1. `perf_event_open`系统调用

详解见[Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)。

```c
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

int perf_event_open(struct perf_event_attr *attr,
                   pid_t pid, int cpu, int group_fd,
                   unsigned long flags);
```


## 1.1. `pid`

参数pid允许事件以各种方式附加到进程。 

* 如果pid为0，则在**当前线程**上进行测量；
* 如果pid大于0，则对**pid指示的进程**进行测量；
* 如果pid为-1，则对**所有进程**进行计数。

## 1.2. `cpu`

cpu参数允许测量特定于CPU。 

* 如果`cpu>=0`，则将测量限制为指定的CPU；否则，将限制为0。 
* 如果`cpu=-1`，则在所有CPU上测量事件。

> 请注意，`pid == -1`和`cpu == -1`的组合无效。

* `pid> 0`且`cpu == -1`设置会测量每个进程，并将该进程跟随该进程计划调度到的任何CPU。 每个用户都可以创建每个进程的事件。
* 每个CPU的`pid == -1`和`cpu>= 0`设置是针对每个CPU的，并测量指定CPU上的所有进程。 每CPU事件需要`CAP_SYS_ADMIN`功能或小于/ 1的`/proc/sys/kernel/perf_event_paranoid`值。见《`perf_event`相关的配置文件》章节。

## 1.3. `group_fd`

group_fd参数允许创建**事件组**。 一个事件组有一个事件，即组长。 首先创建领导者，group_fd = -1。 其余的组成员是通过随后的perf_event_open（）调用创建的，其中group_fd设置为组长的fd。 （使用group_fd = -1单独创建一个事件，并且该事件被认为是只有1个成员的组。）将事件组作为一个单元调度到CPU：仅当所有 可以将组中的事件放到CPU上。 这意味着成员事件的值可以彼此有意义地进行比较，相加，除法（以获得比率）等，因为它们已经为同一组已执行指令计数了事件。

## 1.4. `flags`
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


# 2. 测试例

[https://github.com/Rtoax/test/tree/master/c/glibc/linux/perf_event](https://github.com/Rtoax/test/tree/master/c/glibc/linux/perf_event)

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

    do_something(-1);
    
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

程序运行结果：

```c
[rongtao@localhost perf_event]$ ./a.out 
ioctl 2640, 2641, 2642, 2643, 2644
cpu cycles:     12996145
cache misses:   1135
page faults:    518
 cpu clock:     5417726
task clock:     5393251
```

如程序所示，建立五个fd，并将后四个添加到第一个fd中组成一个组，分别监控以上信息，在下章源码接收中，我们将介绍以上信息是如何从内核中获取的。需要注意的是，代码中的内存锁定对缺页中断没有影响：

```c
    mlock(ptr, len);
    
    for (i = 0; i < len; i++) {
        ptr[i] = (char) (i & 0xff); // pagefault
    }
```


# 3. `perf_event_open`源码分析

## 3.1. 如何调用`perf_event_open`

代码如下：

```c
    struct perf_event_attr pea;
    uint64_t id1;
    
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_HARDWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_HW_CPU_CYCLES;
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    int fd = syscall(__NR_perf_event_open, &pea, getpid(), -1, -1, 0);
    ioctl(fd, PERF_EVENT_IOC_ID, &id1);
```

此处的PID为当前进程PID（也可以填写0），CPU为-1，group_fd为-1，flags填0，标识位flags填写为0，flags在perf命令中填写的PERF_FLAG_FD_CLOEXEC，在源码中为：

```c
	if (flags & PERF_FLAG_FD_CLOEXEC)
		f_flags |= O_CLOEXEC;
```

所以以上的入参表明测量当前进程并将该进程跟随该进程计划调度到的任何CPU（`pid> 0`且`cpu == -1`）。在syscall过程，将陷入内核态。

## 3.2. 如何处理入参

### 3.2.1. `struct perf_event_attr`
首先是从用户空间拷贝到内核空间：
```c
err = perf_copy_attr(attr_uptr, &attr);
```
其内部考虑到了不同版本的`struct perf_event_attr`结构的长度：

```c
	if (!size)
		size = PERF_ATTR_SIZE_VER0;
	if (size < PERF_ATTR_SIZE_VER0 || size > PAGE_SIZE)
		goto err_size;
```

然后使用`copy_struct_from_user`拷贝。接着进行参数检查：

```c
	if (attr->__reserved_1 || attr->__reserved_2 || attr->__reserved_3)
		return -EINVAL;

	if (attr->sample_type & ~(PERF_SAMPLE_MAX-1))
		return -EINVAL;

	if (attr->read_format & ~(PERF_FORMAT_MAX-1))
		return -EINVAL;
```

接下来是对sample_type字段的检查（见《[Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)》），分为两部分：

* `PERF_SAMPLE_BRANCH_STACK`：（从Linux 3.4开始），它提供了最近分支的记录，该记录由CPU分支采样硬件（例如Intel Last Branch Record）提供。并非所有硬件都支持此功能。有关如何过滤报告的分支的信息，请参见branch_sample_type字段。
* `PERF_SAMPLE_REGS_USER`：（从Linux 3.7开始）记录当前用户级别的CPU寄存器状态（调用内核之前的进程中的值）。
* `PERF_SAMPLE_STACK_USER`：（从Linux 3.7开始）记录用户级堆栈，从而允许展开堆栈。
* `PERF_SAMPLE_REGS_INTR`
* `PERF_SAMPLE_CGROUP`

### 3.2.2. `pid`和`cpu`

如果标志位`PERF_FLAG_PID_CGROUP`（该标志激活每个容器的系统范围内的监视），那么`pid`和`cpu`都不可以为-1。
```c
	/*
	 * In cgroup mode, the pid argument is used to pass the fd
	 * opened to the cgroup directory in cgroupfs. The cpu argument
	 * designates the cpu on which to monitor threads from that
	 * cgroup.
	 */
	if ((flags & PERF_FLAG_PID_CGROUP) && (pid == -1 || cpu == -1))
		return -EINVAL;
```
如果标志位`PERF_FLAG_PID_CGROUP`未设置并且`pid != -1`，查找task_struct结构。

```c
static struct task_struct *
find_lively_task_by_vpid(pid_t vpid)    /* 获取task_struct */
{
	struct task_struct *task;

	rcu_read_lock();
	if (!vpid)
		task = current; /* 当前进程 */
	else
		task = find_task_by_vpid(vpid); /* 查找 */
	if (task)
		get_task_struct(task);  /* 引用计数 */
	rcu_read_unlock();

	if (!task)
		return ERR_PTR(-ESRCH);

	return task;
}
```

### 3.2.3. `group_fd`

如果设置了`group_fd != -1`，将获取`struct fd`结构
```c
static inline int perf_fget_light(int fd, struct fd *p)
{
	struct fd f = fdget(fd);
	if (!f.file)
		return -EBADF;

	if (f.file->f_op != &perf_fops) {
		fdput(f);
		return -EBADF;
	}
	*p = f;
	return 0;
}
```
这里需要注意文件操作符为`perf_fops`（后续会很有用）：

```c
static const struct file_operations perf_fops = {
	.llseek			= no_llseek,
	.release		= perf_release,
	.read			= perf_read,
	.poll			= perf_poll,
	.unlocked_ioctl		= perf_ioctl,
	.compat_ioctl		= perf_compat_ioctl,
	.mmap			= perf_mmap,
	.fasync			= perf_fasync,
};
```

### 3.2.4. `flags`

一系列的鉴权工作，此处不做详细讨论，可参见《[Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)》

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


## 3.3. `perf_event_alloc`

```c
	event = perf_event_alloc(&attr, cpu, task, group_leader, NULL,
				 NULL, NULL, cgroup_fd);
```

使用`kzalloc`申请`struct perf_event`结构，然后是一系列的初始化工作。需要注意的几点如下：

* `overflow_handler`为NULL，决定了环形队列的写入方向。
```c
	if (overflow_handler) {
		event->overflow_handler	= overflow_handler;
		event->overflow_handler_context = context;
	} else if (is_write_backward(event)){/* Write ring buffer from end to beginning */
		event->overflow_handler = perf_event_output_backward;
		event->overflow_handler_context = NULL;
	} else {
		event->overflow_handler = perf_event_output_forward;
		event->overflow_handler_context = NULL;
	}
```

* 状态`event->state`

```c
/*
 * Initialize event state based on the perf_event_attr::disabled.
 */
static inline void perf_event__state_init(struct perf_event *event)
{
	event->state = event->attr.disabled ? PERF_EVENT_STATE_OFF :
					      PERF_EVENT_STATE_INACTIVE;
}
```

* more

### 3.3.1. `perf_init_event`

#### 3.3.1.1. `perf_try_init_event`

该函数将调用PMU的相关操作符`pmu->event_init(event);`(参见《[Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)》)，如：

```c
//kernel/events/core.c
static struct pmu/* 性能监控单元 */ perf_swevent = {
	.task_ctx_nr	= perf_sw_context,

	.capabilities	= PERF_PMU_CAP_NO_NMI,

	.event_init	= perf_swevent_init,
	.add		= perf_swevent_add,
	.del		= perf_swevent_del,
	.start		= perf_swevent_start,
	.stop		= perf_swevent_stop,
	.read		= perf_swevent_read,
};

perf_pmu_register(&perf_swevent, "software", PERF_TYPE_SOFTWARE);
```
```c
//kernel/events/core.c
static struct pmu perf_cpu_clock = {
	.task_ctx_nr	= perf_sw_context,

	.capabilities	= PERF_PMU_CAP_NO_NMI,

	.event_init	= cpu_clock_event_init,
	.add		= cpu_clock_event_add,
	.del		= cpu_clock_event_del,
	.start		= cpu_clock_event_start,
	.stop		= cpu_clock_event_stop,
	.read		= cpu_clock_event_read,
};

perf_pmu_register(&perf_cpu_clock, NULL, -1);
```

##### 3.3.1.1.1. perf_swevent->`perf_swevent_init`

见《[Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)》.

## 3.4. `find_get_context`

从task或per-CPU变量中获取target上下文`struct perf_event_context`结构。如果传入的task为空：

```c
	if (!task) {
		/* Must be root to operate on a CPU event: */
		err = perf_allow_cpu(&event->attr);
		if (err)
			return ERR_PTR(err);

		cpuctx = per_cpu_ptr(pmu->pmu_cpu_context, cpu);
		ctx = &cpuctx->ctx;
		get_ctx(ctx);
		++ctx->pin_count;

		return ctx;
	}
```

### 3.4.1. `alloc_perf_context`

```c
static struct perf_event_context *
alloc_perf_context(struct pmu *pmu, struct task_struct *task)
{
	struct perf_event_context *ctx;

	ctx = kzalloc(sizeof(struct perf_event_context), GFP_KERNEL);
	if (!ctx)
		return NULL;

	__perf_event_init_context(ctx);
	if (task)
		ctx->task = get_task_struct(task);
	ctx->pmu = pmu;

	return ctx;
}
```

#### 3.4.1.1. `__perf_event_init_context`
```c
/*
 * Initialize the perf_event context in a task_struct:
 */
static void __perf_event_init_context(struct perf_event_context *ctx)   /* 初始化CPU ctx */
{
	raw_spin_lock_init(&ctx->lock);
	mutex_init(&ctx->mutex);
	INIT_LIST_HEAD(&ctx->active_ctx_list);
	perf_event_groups_init(&ctx->pinned_groups);
	perf_event_groups_init(&ctx->flexible_groups);
	INIT_LIST_HEAD(&ctx->event_list);
	INIT_LIST_HEAD(&ctx->pinned_active);
	INIT_LIST_HEAD(&ctx->flexible_active);
	refcount_set(&ctx->refcount, 1);
}
```

在这里我们的group_fd填写的-1，所以所有的`if (group_leader)`不成立，这将在下一次的系统调用传入一个fd作为leader。




## 3.5. `perf_event_set_output`

由于我们这里的`output_event`为空，此处先给出，该接口主要是分配了环形缓冲区`struct perf_buffer`。
```c
	if (output_event) {
		err = perf_event_set_output(event, output_event);
		if (err)
			goto err_context;
	}
```

## 3.6. `anon_inode_getfile`

创建一个新的`struct file`实例，挂入 匿名 inode 中并且 一个 detry 描述 class，此处也是使用的文件操作符`perf_fops`，对此接口不做详细讲解。这个file将于fd绑定，fd将返回用户空间。


## 3.7. `perf_event_validate_size`

```c
static bool perf_event_validate_size(struct perf_event *event)
{
	/*
	 * The values computed here will be over-written when we actually
	 * attach the event.
	 */
	__perf_event_read_size(event, event->group_leader->nr_siblings + 1);
	__perf_event_header_size(event, event->attr.sample_type & ~PERF_SAMPLE_READ);
	perf_event__id_header_size(event);

	/*
	 * Sum the lot; should not exceed the 64k limit we have on records.
	 * Conservative limit to allow for callchains and other variable fields.
	 */
	if (event->read_size + event->header_size +
	    event->id_header_size + sizeof(struct perf_event_header) >= 16*1024)
		return false;

	return true;
}
```

### 3.7.1. `__perf_event_read_size`

参见《[Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)》
```c
static void __perf_event_read_size(struct perf_event *event, int nr_siblings)
{
	int entry = sizeof(u64); /* value */
	int size = 0;
	int nr = 1;

	if (event->attr.read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
		size += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
		size += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_ID)
		entry += sizeof(u64);

	if (event->attr.read_format & PERF_FORMAT_GROUP) {
		nr += nr_siblings;
		size += sizeof(u64);
	}

	size += entry * nr;
	event->read_size = size;
}
```

### 3.7.2. `__perf_event_header_size`
```c
static void __perf_event_header_size(struct perf_event *event, u64 sample_type)
{
	struct perf_sample_data *data;
	u16 size = 0;

	if (sample_type & PERF_SAMPLE_IP)
		size += sizeof(data->ip);

	if (sample_type & PERF_SAMPLE_ADDR)
		size += sizeof(data->addr);

	if (sample_type & PERF_SAMPLE_PERIOD)
		size += sizeof(data->period);

	if (sample_type & PERF_SAMPLE_WEIGHT)
		size += sizeof(data->weight);

	if (sample_type & PERF_SAMPLE_READ)
		size += event->read_size;

	if (sample_type & PERF_SAMPLE_DATA_SRC)
		size += sizeof(data->data_src.val);

	if (sample_type & PERF_SAMPLE_TRANSACTION)
		size += sizeof(data->txn);

	if (sample_type & PERF_SAMPLE_PHYS_ADDR)
		size += sizeof(data->phys_addr);

	if (sample_type & PERF_SAMPLE_CGROUP)
		size += sizeof(data->cgroup);

	event->header_size = size;
}
```

### 3.7.3. `perf_event__id_header_size`
```c
static void perf_event__id_header_size(struct perf_event *event)
{
	struct perf_sample_data *data;
	u64 sample_type = event->attr.sample_type;
	u16 size = 0;

	if (sample_type & PERF_SAMPLE_TID)
		size += sizeof(data->tid_entry);

	if (sample_type & PERF_SAMPLE_TIME)
		size += sizeof(data->time);

	if (sample_type & PERF_SAMPLE_IDENTIFIER)
		size += sizeof(data->id);

	if (sample_type & PERF_SAMPLE_ID)
		size += sizeof(data->id);

	if (sample_type & PERF_SAMPLE_STREAM_ID)
		size += sizeof(data->stream_id);

	if (sample_type & PERF_SAMPLE_CPU)
		size += sizeof(data->cpu_entry);

	event->id_header_size = size;
}
```

### 3.7.4. 大小限制

不能超出`16*1024`大小
```c
	if (event->read_size + event->header_size +
	    event->id_header_size + sizeof(struct perf_event_header) >= 16*1024)
		return false;
```

## 3.8. `perf_install_in_context`

```c
	/*
	 * Precalculate sample_data sizes; do while holding ctx::mutex such
	 * that we're serialized against further additions and before
	 * perf_install_in_context() which is the point the event is active and
	 * can use these values.
	 */
	perf_event__header_size(event);
	perf_event__id_header_size(event);

	event->owner = current;

	perf_install_in_context(ctx, event, event->cpu);
	perf_unpin_context(ctx);

	if (move_group)
		perf_event_ctx_unlock(group_leader, gctx);
	mutex_unlock(&ctx->mutex);

	if (task) {
		up_read(&task->signal->exec_update_lock);
		put_task_struct(task);
	}
```

### 3.8.1. `__perf_install_in_context`

TODO

## 3.9. 添加至进程链表

```c
	mutex_lock(&current->perf_event_mutex);
	list_add_tail(&event->owner_entry, &current->perf_event_list);
	mutex_unlock(&current->perf_event_mutex);
```

## 3.10. 返回用户空间

至此，系统调用返回用户空间，这个fd在我们的测试例中是`group_fd`，后续的fd将于这个group_fd组成一个perf组。


## 3.11. 再次调用`perf_event_open`

```c
    struct perf_event_attr pea;
    uint64_t id2;
    
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_SOFTWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_HW_CACHE_MISSES;
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    int fd = syscall(__NR_perf_event_open, &pea, getpid(), -1, grp_fd, 0);
    ioctl(fd, PERF_EVENT_IOC_ID, &id2);
```

这次，在系统调用中，`group_leader`不为空：

```c
	if (group_fd != -1) {
		err = perf_fget_light(group_fd, &group);
		if (err)
			goto err_fd;
		group_leader = group.file->private_data;
```

造成的不同之处在于：

* 在`perf_event_alloc`中赋值`event->group_leader	= group_leader;`；
* 关于`group_leader`的分支：
    * `pmu = group_leader->ctx->pmu;`软和硬的区别；
    * 将时间添加到leader；
    * 一些兄弟的操作；
    * `perf_event_set_output`;

# 4. 文件操作符`perf_fops`

```c
static const struct file_operations perf_fops = {
	.llseek			= no_llseek,
	.release		= perf_release,
	.read			= perf_read,
	.poll			= perf_poll,
	.unlocked_ioctl	= perf_ioctl,
	.compat_ioctl	= perf_compat_ioctl,
	.mmap			= perf_mmap,
	.fasync			= perf_fasync,
};
```

# 5. 文件操作`ioctl`

ioctl支持的操作为：

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

arg目前只有一项：
```c
enum perf_event_ioc_flags {
	PERF_IOC_FLAG_GROUP		= 1U << 0,
};
```

在我们的测试例中，ioctl是这样被使用的：

```c
    ioctl(group_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(group_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    do_something(-1);
    ioctl(group_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
```
它在内核中的路径为：
```c
ioctl
    perf_fops->compat_ioctl
        perf_compat_ioctl
            perf_ioctl
                _perf_ioctl
```

针对我们的例子，简化`_perf_ioctl`代码为：
```c
static long _perf_ioctl(struct perf_event *event, unsigned int cmd, unsigned long arg)
{
	void (*func)(struct perf_event *);
	u32 flags = arg;

	switch (cmd) {
	case PERF_EVENT_IOC_ENABLE:
		func = _perf_event_enable;
		break;
	case PERF_EVENT_IOC_DISABLE:
		func = _perf_event_disable;
		break;
	case PERF_EVENT_IOC_RESET:
		func = _perf_event_reset;
		break;
	default:
		return -ENOTTY;
	}
	if (flags & PERF_IOC_FLAG_GROUP)
		perf_event_for_each(event, func);
	else
		perf_event_for_each_child(event, func);

	return 0;
}
```

我们传入的flags为`PERF_IOC_FLAG_GROUP`，也就是会执行`perf_event_for_each`，该函数比`perf_event_for_each_child`多了一层兄弟的遍历：


```c
static void perf_event_for_each_child(struct perf_event *event,
					void (*func)(struct perf_event *))
{
	struct perf_event *child;

	WARN_ON_ONCE(event->ctx->parent_ctx);

	mutex_lock(&event->child_mutex);
	func(event);
	list_for_each_entry(child, &event->child_list, child_list) {
		func(child);}
	mutex_unlock(&event->child_mutex);
}

static void perf_event_for_each(struct perf_event *event,
				  void (*func)(struct perf_event *))
{
	struct perf_event_context *ctx = event->ctx;
	struct perf_event *sibling;

	lockdep_assert_held(&ctx->mutex);

	event = event->group_leader;

	perf_event_for_each_child(event, func);
	for_each_sibling_event(sibling, event) {
		perf_event_for_each_child(sibling, func);}
}
```

最终都会直接调用`func`，分别对应：

* `PERF_EVENT_IOC_RESET`: `_perf_event_reset`
* `PERF_EVENT_IOC_ENABLE`: `_perf_event_enable`
* `PERF_EVENT_IOC_DISABLE`: `_perf_event_disable`

在《[Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)》介绍过他么底层的内核路径是如何调用性能管理实体PMU的。

## 5.1. `_perf_event_reset`

### 5.1.1. `perf_event_update_userpage`

关于结构`struct perf_event_mmap_page`的介绍，可参考《[Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)》。
总体来讲，就是对`struct perf_event_mmap_page`的填充。需要注意的是，`arch_perf_update_userpage`调用`cyc2ns_read_begin`从CPU获取开始时钟。


## 5.2. `_perf_event_enable`

核心的，他讲调用`event_function_call(event, __perf_event_enable, NULL);`，可在《[Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)》中查看，实际上，最终对调用对应event PMU的add函数：

```
perf_event_enable
    _perf_event_enable
        __perf_event_enable
            ctx_sched_in
                ctx_flexible_sched_in|ctx_pinned_sched_in
                    merge_sched_in
                        group_sched_in
                            event_sched_in
                                event->pmu->add(event, PERF_EF_START)
```

以`perf_swevent`为例，即调用`perf_swevent_add`，也就是说，`ioctl(group_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);`将对调用`event->pmu->add`，当为`PERF_TYPE_SOFTWARE`时，即调用`perf_swevent_add`。

```c
//kernel/events/core.c
static struct pmu/* 性能监控单元 */ perf_swevent = {
	.task_ctx_nr	= perf_sw_context,

	.capabilities	= PERF_PMU_CAP_NO_NMI,

	.event_init	= perf_swevent_init,
	.add		= perf_swevent_add,
	.del		= perf_swevent_del,
	.start		= perf_swevent_start,
	.stop		= perf_swevent_stop,
	.read		= perf_swevent_read,
};

perf_pmu_register(&perf_swevent, "software", PERF_TYPE_SOFTWARE);
```

> 我们在上面章节中提到，我们的group_fd为`PERF_TYPE_HARDWARE`类型。




## 5.3. `_perf_event_disable`

在《[Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)》中查看
```
perf_event_disable
    _perf_event_disable
        __perf_event_disable
            group_sched_out
                event_sched_out
                    event->pmu->del(event, 0);
```





# 6. 文件操作`mmap`

`struct perf_event_attr`结构`mmap`位允许记录执行mmap事件。不得不提出结构：


## 6.1. `perf_mmap_vmops`

```c
static const struct vm_operations_struct perf_mmap_vmops = {
	.open		= perf_mmap_open,
	.close		= perf_mmap_close, /* non mergeable */
	.fault		= perf_mmap_fault,
	.page_mkwrite	= perf_mmap_fault,
};
```

## 6.2. 分配内存

```c
perf_mmap
    rb_alloc
        kzalloc
        vmalloc_user
        ring_buffer_init
    ring_buffer_attach
        list_add_rcu
    perf_event_init_userpage
    perf_event_update_userpage
        calc_timer_values
            __perf_update_times
        arch_perf_update_userpage
```


# 7. 文件操作`read`

`perf_read`
```c
perf_read
    __perf_read
        perf_read_group
            __perf_read_group_add
                perf_event_read
                    __perf_event_read_cpu
                        topology_physical_package_id
                            (cpu_data(cpu).phys_proc_id)
                                per_cpu(cpu_info, cpu)
                    __perf_event_read
                        __get_cpu_context
                        perf_event_update_time
                            __perf_update_times
                        perf_event_update_sibling_time
                        pmu->start_txn(pmu, PERF_PMU_TXN_READ);    ->x86_pmu_start_txn
                        pmu->read(event);    -> x86_pmu_read
                        pmu->commit_txn(pmu); -> x86_pmu_commit_txn
            copy_to_user
        perf_read_one
            TODO
```


# 8. 缺页统计`PERF_COUNT_SW_PAGE_FAULTS`

在5.10.13内核的缺页中断中，perf_event相关代码路径如下：

在较早版本内核中处理缺页的函数为`do_page_fault`，在5.10.13中更名为`exc_page_fault`，并使用`DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)`定义，下面给出perf相关的代码路径：

```c
exc_page_fault
    handle_page_fault
        do_kern_addr_fault    - 内核不统计（我没找到）
        do_user_addr_fault
            perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, address); ######### perf入口
                if (static_key_false(&perf_swevent_enabled[event_id]))
                    __perf_sw_event(event_id, nr, regs, addr);
                        ___perf_sw_event
                            perf_sample_data_init
                                data->addr = addr;
                                data->raw  = NULL;
                                data->br_stack = NULL;
                                data->period = period;
                                data->weight = 0;
                                data->data_src.val = PERF_MEM_NA;
                                data->txn = 0;
                            do_perf_sw_event
                                perf_swevent_event
                                    local64_add(nr, &event->count);
                                    perf_swevent_overflow
                                        perf_swevent_set_period
                                        __perf_event_overflow
                                            __perf_event_account_interrupt #通用事件溢出处理，采样
                                                perf_adjust_period
                                                    perf_calculate_period
                                                        pmu->stop(event, PERF_EF_UPDATE);
                                                        pmu->start(event, PERF_EF_RELOAD);
                                            irq_work_queue
```

* 函数`perf_swevent_set_period`：我们直接增加`event->count`，并在`event->hw.period_left`中保留第二个值以计算间隔。 此周期事件保持在`[-sample_period，0]`范围内，以便我们可以将符号用作触发器。
* 函数`__perf_event_account_interrupt`：通用事件溢出处理，采样。




# 9. 总结

本文的分析就到这里，因为perf性能部分的内容相对较多较庞杂（相对tracepoint,kprobe等），后续具体的函数分析将做单独的篇幅研究。


# 10. 相关链接

* [注释源码：https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核 eBPF基础：perf（1）：perf_event在内核中的初始化](https://rtoax.blog.csdn.net/article/details/116982544)
* [Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)
* [Linux内核 eBPF基础：perf（3）用户态指令分析](https://rtoax.blog.csdn.net/article/details/117027406)
* [Linux内核 eBPF基础：perf（4）perf_event_open系统调用与用户手册详解](https://rtoax.blog.csdn.net/article/details/117040529)
* [Linux kernel perf architecture](http://terenceli.github.io/技术/2020/08/29/perf-arch)
* [Linux perf 1.1、perf_event内核框架](https://blog.csdn.net/pwl999/article/details/81200439)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)
* [https://www.kernel.org/doc/man-pages/](https://www.kernel.org/doc/man-pages/)

