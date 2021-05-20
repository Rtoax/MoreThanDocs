<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>perf（2）：性能管理单元PMU的注册</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月18日</font></center>
<br/>


* 本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)



# 1. perf类型

在`include\uapi\linux\perf_event.h`中有：

```c
/*
 * attr.type
 */
enum perf_type_id { /* perf 类型 */
	PERF_TYPE_HARDWARE			= 0,    /* 硬件 */
	PERF_TYPE_SOFTWARE			= 1,    /* 软件 */
	PERF_TYPE_TRACEPOINT		= 2,    /* 跟踪点 */
	PERF_TYPE_HW_CACHE			= 3,    /* 硬件cache */
	PERF_TYPE_RAW				= 4,    /* RAW */
	PERF_TYPE_BREAKPOINT		= 5,    /* 断点 */

	PERF_TYPE_MAX,				/* non-ABI */
};
```

他们是传入**性能管理单元PMU**注册函数`perf_pmu_register`的字段`type`。列出注册的PMU：

```c
[rongtao@localhost src]$ grep -r "perf_pmu_register" | grep "\""
arch/x86/events/intel/bts.c:	return perf_pmu_register(&bts_pmu, "intel_bts", -1);
arch/x86/events/intel/pt.c:	ret = perf_pmu_register(&pt_pmu.pmu, "intel_pt", -1);
arch/x86/events/amd/power.c:	ret = perf_pmu_register(&pmu_class, "power", -1);
arch/x86/events/core.c:	err = perf_pmu_register(&pmu, "cpu", PERF_TYPE_RAW);
arch/x86/events/msr.c:	perf_pmu_register(&pmu_msr, "msr", -1);
arch/x86/events/rapl.c:	ret = perf_pmu_register(&rapl_pmus->pmu, "power", -1);
kernel/events/hw_breakpoint.c:	perf_pmu_register(&perf_breakpoint, "breakpoint", PERF_TYPE_BREAKPOINT);
kernel/events/core.c:	perf_pmu_register(&perf_tracepoint, "tracepoint", PERF_TYPE_TRACEPOINT);
kernel/events/core.c:	perf_pmu_register(&perf_kprobe, "kprobe", -1);
kernel/events/core.c:	perf_pmu_register(&perf_uprobe, "uprobe", -1);
kernel/events/core.c:	perf_pmu_register(&perf_swevent, "software", PERF_TYPE_SOFTWARE);

kernel/events/core.c:	perf_pmu_register(&perf_cpu_clock, NULL, -1);
kernel/events/core.c:	perf_pmu_register(&perf_task_clock, NULL, -1);
```




# 2. `perf_pmu_register`

```c
int perf_pmu_register(struct pmu *pmu, const char *name, int type)
```
这里需要注意，函数`perf_pmu_register`是非常重要的注册函数，注册的pmu将加入全局链表`pmus`中：
```c
static LIST_HEAD(pmus);
```

函数`perf_pmu_register`首先申请per-cpu变量：
```c
pmu->pmu_disable_count = alloc_percpu(int);
```

接着，如果类型不是`PERF_TYPE_SOFTWARE`，将分配一个ID（前提是name没有设定，如`perf_cpu_clock`）
```c
if (type != PERF_TYPE_SOFTWARE) {
	if (type >= 0)
		max = type;

    /* 分配一个ID */
	ret = idr_alloc(&pmu_idr, pmu, max, 0, GFP_KERNEL);
	if (ret < 0)
		goto free_pdc;

	WARN_ON(type >= 0 && ret != type);

	type = ret;
}
```
然后，申请一个设备：

```c
	if (pmu_bus_running/* perf_event_sysfs_init() 中被设置 为 1 */) {
		ret = pmu_dev_alloc(pmu);   /* 分配一个设备 device- /sys/devices/ */
		if (ret)
			goto free_idr;
	}
```

接下来这段代码表明，每个hw只能注册一次：

```c
	if (pmu->task_ctx_nr == perf_hw_context) {
		static int hw_context_taken = 0;

		/*
		 * Other than systems with heterogeneous CPUs, it never makes
		 * sense for two PMUs to share perf_hw_context. PMUs which are
		 * uncore must use perf_invalid_context.
		 */
		if (WARN_ON_ONCE(hw_context_taken &&
		    !(pmu->capabilities & PERF_PMU_CAP_HETEROGENEOUS_CPUS)))
			pmu->task_ctx_nr = perf_invalid_context;

		hw_context_taken = 1;
	}
```
否则，其将被设置为`perf_invalid_context`。然后为每个CPU分配上下文：

```c
    pmu->pmu_cpu_context = alloc_percpu(struct perf_cpu_context);
```
紧接着，进行初始化：
```c
	for_each_possible_cpu(cpu) {    /* 遍历 CPU */
		struct perf_cpu_context *cpuctx;

		cpuctx = per_cpu_ptr(pmu->pmu_cpu_context, cpu);    /* 1.获取 CPU 的ctx */
		__perf_event_init_context(&cpuctx->ctx);            /* 2.初始化这个ctx */
		lockdep_set_class(&cpuctx->ctx.mutex, &cpuctx_mutex);/*3.初始化lockdep  */
		lockdep_set_class(&cpuctx->ctx.lock, &cpuctx_lock);
		cpuctx->ctx.pmu = pmu;                              /* 4.指向这个PMU */
		cpuctx->online = cpumask_test_cpu(cpu, perf_online_mask);/* 5.是否在线标记 */

		__perf_mux_hrtimer_init(cpuctx, cpu);               /* 6.高精度定时器，function=perf_mux_hrtimer_handler */

		cpuctx->heap_size = ARRAY_SIZE(cpuctx->heap_default);/*  */
		cpuctx->heap = cpuctx->heap_default;    /* 默认使用2个 */
	}
```
其中`__perf_event_init_context`初始化`struct perf_event_context`结构：

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

`__perf_mux_hrtimer_init`初始化一个高精度定时器，

```c
static void __perf_mux_hrtimer_init(struct perf_cpu_context *cpuctx, int cpu)   /* 高精度定时器 */
{
	struct hrtimer *timer = &cpuctx->hrtimer;
	struct pmu *pmu = cpuctx->ctx.pmu;
	u64 interval;

	/* no multiplexing needed for SW PMU */
	if (pmu->task_ctx_nr == perf_sw_context)
		return;

	/*
	 * check default is sane, if not set then force to
	 * default interval (1/tick)
	 */
	interval = pmu->hrtimer_interval_ms;
	if (interval < 1)
		interval = pmu->hrtimer_interval_ms = PERF_CPU_HRTIMER; /* 小于1ms，就让他是 1ms */

	cpuctx->hrtimer_interval = ns_to_ktime(NSEC_PER_MSEC * interval);

	raw_spin_lock_init(&cpuctx->hrtimer_lock);
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_PINNED_HARD);
	timer->function = perf_mux_hrtimer_handler; /* 处理函数 */
}
```
需要注意一下几点：

* 如果是软件上下文`perf_sw_context`，不创建定时器；
* 如果ioctl设置的到期时间小于`1ms`，将其设置为`1ms`；
* 会调函数为`perf_mux_hrtimer_handler`；

在获取到CPU上下文后，给没有初始化的PMU函数指针赋值：

```c
    /*  */
	if (!pmu->start_txn) {
		if (pmu->pmu_enable) {
			/*
			 * If we have pmu_enable/pmu_disable calls, install
			 * transaction stubs that use that to try and batch
			 * hardware accesses.
			 */
			pmu->start_txn  = perf_pmu_start_txn;
			pmu->commit_txn = perf_pmu_commit_txn;
			pmu->cancel_txn = perf_pmu_cancel_txn;
		} else {
			pmu->start_txn  = perf_pmu_nop_txn;
			pmu->commit_txn = perf_pmu_nop_int;
			pmu->cancel_txn = perf_pmu_nop_void;
		}
	}

    /* 使能 */
	if (!pmu->pmu_enable) {
		pmu->pmu_enable  = perf_pmu_nop_void;
		pmu->pmu_disable = perf_pmu_nop_void;
	}

    /* 检测周期 ioctl(PERF_EVENT_IOC_PERIOD) */
	if (!pmu->check_period)
		pmu->check_period = perf_event_nop_int;

    /*  */
	if (!pmu->event_idx)
		pmu->event_idx = perf_event_idx_default;
```

下面是将这个PMU添加到pmus链表中：

```c
	/*
	 * Ensure the TYPE_SOFTWARE PMUs are at the head of the list,
	 * since these cannot be in the IDR. This way the linear search
	 * is fast, provided a valid software event is provided.
	 */
	if (type == PERF_TYPE_SOFTWARE || !name)
		list_add_rcu(&pmu->entry, &pmus);   /* 软件 或者 name=NULL */
	else
		list_add_tail_rcu(&pmu->entry, &pmus);/*  */
```

需要注意的是，软件类型的PMU将放到链表开头，以提高线性查询速度。


# 3. 例: `software`

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

## 3.1. `perf_swevent_init`



# 4. 例: `perf_cpu_clock`

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


# 5. 例: `perf_task_clock`

```c
//kernel/events/core.c
static struct pmu perf_task_clock = {
	.task_ctx_nr	= perf_sw_context,

	.capabilities	= PERF_PMU_CAP_NO_NMI,

	.event_init	= task_clock_event_init,
	.add		= task_clock_event_add,
	.del		= task_clock_event_del,
	.start		= task_clock_event_start,
	.stop		= task_clock_event_stop,
	.read		= task_clock_event_read,
};

perf_pmu_register(&perf_task_clock, NULL, -1);
```

# 6. 例: `kprobe`

```c
//kernel/events/core.c
static struct pmu perf_kprobe = {
	.task_ctx_nr	= perf_sw_context,
	.event_init	= perf_kprobe_event_init,
	.add		= perf_trace_add,
	.del		= perf_trace_del,
	.start		= perf_swevent_start,
	.stop		= perf_swevent_stop,
	.read		= perf_swevent_read,
	.attr_groups	= kprobe_attr_groups,
};

perf_pmu_register(&perf_kprobe, "kprobe", -1);
```

# 7. 例: `tracepoint`


```c
//kernel/events/core.c
static struct pmu perf_tracepoint = {
	.task_ctx_nr	= perf_sw_context,

	.event_init	= perf_tp_event_init,
	.add		= perf_trace_add,
	.del		= perf_trace_del,
	.start		= perf_swevent_start,
	.stop		= perf_swevent_stop,
	.read		= perf_swevent_read,
};

perf_pmu_register(&perf_tracepoint, "tracepoint", PERF_TYPE_TRACEPOINT);
```




# 8. pmu->`event_init`

```c
perf_event_alloc
    perf_init_event
        perf_try_init_event
            pmu->event_init(event);
```

而调用了`perf_event_alloc`的有：

* `perf_event_open`
* `perf_event_create_kernel_counter`
* `fork|clone`
```c
kernel_clone
    copy_process
        perf_event_init_task
            perf_event_init_context
                inherit_task_group
                    inherit_group
                        inherit_event
                            perf_event_alloc
```

# 9. pmu->`add`

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


# 10. pmu->`del`


```
perf_event_disable
    _perf_event_disable
        __perf_event_disable
            group_sched_out
                event_sched_out
                    event->pmu->del(event, 0);
```

# 11. pmu->`start`


# 12. pmu->`stop`


```c
perf_fops->perf_mmap
    ring_buffer_attach
        perf_event_stop
            __perf_event_stop
                event->pmu->stop(event, PERF_EF_UPDATE);
```

# 13. pmu->`read`





# 14. 相关链接

* [注释源码：https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核 eBPF基础：perf（1）：perf_event在内核中的初始化](https://rtoax.blog.csdn.net/article/details/116982544)
* [Linux内核 eBPF基础：perf（2）：perf性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)
* [Linux kernel perf architecture](http://terenceli.github.io/技术/2020/08/29/perf-arch)
* [Linux perf 1.1、perf_event内核框架](https://blog.csdn.net/pwl999/article/details/81200439)
* [Linux内核性能架构：perf_event](https://rtoax.blog.csdn.net/article/details/116991729)

