<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>perf（1）：perf_event在内核中的初始化</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月12日</font></center>
<br/>


本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)



# 1. 初始化函数调用关系

## 1.1. start_kernel

可参见[《Linux开机启动过程（10）：start_kernel 初始化（至setup_arch初期）》](https://rtoax.blog.csdn.net/article/details/114976658)系列文章。

在内核启动阶段，会调用`perf_event_init`对perf_event进行初始化。

```c
start_kernel() ...
    perf_event_init()      <<<<<<<<<<perf相关
      perf_tp_register()   <<<<<<<<<<perf相关
      init_hw_breakpoint() <<<<<<<<<<perf相关
    arch_call_rest_init()
      rest_init()
        kernel_thread()
          kernel_init() => PID=1
          kernel_init()
			->kernel_init_freeable()
   			  ->do_basic_setup()
   				->do_initcalls()
   				  ->do_initcall_level()
   					->do_one_initcall()
   					  ->xxx__initcall() <<<<<<<<<<perf相关
```

## 1.2. initcall

被这些宏标注的函数将在`do_initcall_level`中被调用

```c
#define early_initcall(fn) /* fn */ __define_initcall(fn, early)/* (fn,early) */
/* 数字越小，优先级越高 */
#define pure_initcall(fn)		__define_initcall(fn, 0)
#define core_initcall(fn)		__define_initcall(fn, 1)
#define core_initcall_sync(fn)		__define_initcall(fn, 1s)
#define postcore_initcall(fn)		__define_initcall(fn, 2)
#define postcore_initcall_sync(fn)	__define_initcall(fn, 2s)
#define arch_initcall(fn)		__define_initcall(fn, 3)
#define arch_initcall_sync(fn)		__define_initcall(fn, 3s)
#define subsys_initcall(fn)		__define_initcall(fn, 4)
#define subsys_initcall_sync(fn)	__define_initcall(fn, 4s)
#define fs_initcall(fn)			__define_initcall(fn, 5)
#define fs_initcall_sync(fn)		__define_initcall(fn, 5s)
#define rootfs_initcall(fn)		__define_initcall(fn, rootfs)
#define device_initcall(fn)		__define_initcall(fn, 6)
#define device_initcall_sync(fn)	__define_initcall(fn, 6s)
#define late_initcall(fn)		__define_initcall(fn, 7)
#define late_initcall_sync(fn)		__define_initcall(fn, 7s)
```

关于perf的initcall函数包括：
```c
early_initcall(init_hw_perf_events);
arch_initcall(bts_init);
arch_initcall(pt_init);
device_initcall(perf_event_sysfs_init);
device_initcall(amd_ibs_init);
device_initcall(amd_iommu_pc_init);
device_initcall(msr_init);
device_initcall(amd_uncore_init);
```

## 1.3. 模块

此外，有些perf相关的是

```
static int __init cstate_pmu_init(void)
{
    return cstate_init();
}
module_init(cstate_pmu_init);

static int __init amd_power_pmu_init(void)
{
}
module_init(amd_power_pmu_init);

static int __init rapl_pmu_init(void)
{
}
module_init(rapl_pmu_init);

static int __init intel_uncore_init(void)
{
    uncore_pci_init
        uncore_pci_sub_driver_init
            uncore_pci_pmu_register
                uncore_pmu_register
    uncore_cpu_init
        uncore_msr_pmus_register
            type_pmu_register
                uncore_pmu_register
    uncore_mmio_init
        type_pmu_register
            uncore_pmu_register
}
module_init(intel_uncore_init);
```

这些在我的系统中统统没有找到，我认为它们不是必须的，所以本系列文章我不打算讨论。

```bash
lsmod | grep -e core -e power -e stat
```



# 2. `perf_event_init`


`perf_event_init`在`start_kernel`中被调用
```
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)
{
    perf_event_init();
    ...
    arch_call_rest_init();
}
```
调用关系：

```c
start_kernel() ...
    perf_event_init()      <<<<<<<<<<perf相关
      perf_tp_register()   <<<<<<<<<<perf相关
      init_hw_breakpoint() <<<<<<<<<<perf相关
    arch_call_rest_init()
      rest_init()
        kernel_thread()
          kernel_init() => PID=1
          kernel_init()
			->kernel_init_freeable()
   			  ->do_basic_setup()
   				->do_initcalls()
   				  ->do_initcall_level()
   					->do_one_initcall()
   					  ->xxx__initcall() <<<<<<<<<<perf相关
```

这里有必要先说下perf类型（`include\uapi\linux\perf_event.h`）：

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
他们是传入**性能管理单元PMU**注册函数`perf_pmu_register`的字段`type`
```c
int perf_pmu_register(struct pmu *pmu, const char *name, int type)
```
这里需要注意，函数`perf_pmu_register`是非常重要的注册函数，注册的pmu将加入全局链表`pmus`中，这将在另一篇文章中单独介绍。
```c
static LIST_HEAD(pmus);
```

`perf_event_init`函数分别调用`perf_pmu_register`注册了如下内容：

```c
	perf_pmu_register(&perf_swevent, "software", PERF_TYPE_SOFTWARE);
	perf_pmu_register(&perf_cpu_clock, NULL, -1);
	perf_pmu_register(&perf_task_clock, NULL, -1);
	perf_tp_register();
	init_hw_breakpoint();
```
其中`perf_tp_register`为：

```c
static inline void perf_tp_register(void)
{
	perf_pmu_register(&perf_tracepoint, "tracepoint", PERF_TYPE_TRACEPOINT);
#ifdef CONFIG_KPROBE_EVENTS
	perf_pmu_register(&perf_kprobe, "kprobe", -1);
#endif
#ifdef CONFIG_UPROBE_EVENTS
	perf_pmu_register(&perf_uprobe, "uprobe", -1);
#endif
}
```

其中`init_hw_breakpoint`为：

```c
int __init init_hw_breakpoint(void)
{
    ...
    perf_pmu_register(&perf_breakpoint, "breakpoint", PERF_TYPE_BREAKPOINT);
    ...
}
```



# 3. `initcall`

使用initcall方式初始化的pmu注册包括：
```
early_initcall(init_hw_perf_events);
arch_initcall(bts_init);
arch_initcall(pt_init);
device_initcall(perf_event_sysfs_init);
device_initcall(amd_ibs_init);
device_initcall(amd_iommu_pc_init);
device_initcall(msr_init);
device_initcall(amd_uncore_init);
```



下面对他们分别简述

## 3.1. `init_hw_perf_events`

这是和架构/厂商相关的硬件perf初始化，我们只看英特尔相关（值得说明 的是这里有国产兆芯，这里不讲家国情怀，只讲技术，所以不看兆芯相关代码）。

```
static int __init init_hw_perf_events(void)
{
    intel_pmu_init();
    pmu_check_apic();
    perf_events_lapic_init();   /* 注册 PMU 为 NMI 中断 */
	register_nmi_handler(NMI_LOCAL, perf_event_nmi_handler, 0, "PMI");  /* 注册函数 */

	pr_info("... version:                %d\n",     x86_pmu.version);
	pr_info("... bit width:              %d\n",     x86_pmu.cntval_bits);
	pr_info("... generic registers:      %d\n",     x86_pmu.num_counters);
	pr_info("... value mask:             %016Lx\n", x86_pmu.cntval_mask);
	pr_info("... max period:             %016Lx\n", x86_pmu.max_period);
	pr_info("... fixed-purpose events:   %d\n",     x86_pmu.num_counters_fixed);
	pr_info("... event mask:             %016Lx\n", x86_pmu.intel_ctrl);

    perf_pmu_register(&pmu, "cpu", PERF_TYPE_RAW);
}
early_initcall(init_hw_perf_events);
```

此函数中有相关x86 pmu 信息的打印：

```c
[rongtao@localhost perf]$ dmesg | grep fixed- -A 1 -B 5
[    0.320139] ... version:                2
[    0.320140] ... bit width:              48
[    0.320141] ... generic registers:      4
[    0.320142] ... value mask:             0000ffffffffffff
[    0.320143] ... max period:             000000007fffffff
[    0.320144] ... fixed-purpose events:   3
[    0.320145] ... event mask:             000000070000000f
```

> `perf_event_nmi_handler`会单独讲解。

## 3.2. `bts_init`

关于BTS的介绍可参见[什么是Intel LBR，BTS和AET？](https://rtoax.blog.csdn.net/article/details/116979913)，即BTS使用RAM缓存（CAR）或系统DRAM来存储更多的指令和事件（Branch Tracking Store (BTS **分支跟踪存储**)），同时可参考[《Processor Tracing | 处理器追踪》](https://rtoax.blog.csdn.net/article/details/116980183)。

> 注意不要和[BTS — Bit Test and Set](https://www.felixcloutier.com/x86/bts)或[x86和amd64指令参考](https://rtoax.blog.csdn.net/article/details/116978549)混淆。


## 3.3. `pt_init`

上节中讲到[《Processor Tracing | 处理器追踪》](https://rtoax.blog.csdn.net/article/details/116980183)。
这将在后续单独介绍。


## 3.4. `perf_event_sysfs_init`

`perf_event``sysfs`初始化，在`kernel\events\core.c`中有`perf_event_sysfs_init`：

```
[root@localhost event_source]# pwd
/sys/bus/event_source
[root@localhost event_source]# tree
.
├── devices
│   ├── breakpoint -> ../../../devices/breakpoint
│   ├── cpu -> ../../../devices/cpu
│   ├── kprobe -> ../../../devices/kprobe
│   ├── msr -> ../../../devices/msr
│   ├── power -> ../../../devices/power
│   ├── software -> ../../../devices/software
│   ├── tracepoint -> ../../../devices/tracepoint
│   └── uprobe -> ../../../devices/uprobe
├── drivers
├── drivers_autoprobe
├── drivers_probe
└── uevent
```

本节内容也将在单独的文章中介绍。

## 3.5. `msr_init`

以下内容摘自[x86 CPU的MSR寄存器](https://zhuanlan.zhihu.com/p/50142793)：

> MSR（Model Specific Register）是x86架构中的概念，指的是在x86架构处理器中，一系列用于控制CPU运行、功能开关、调试、跟踪程序执行、监测CPU性能等方面的寄存器。
> MSR寄存器的雏形开始于Intel 80386和80486处理器，到Intel Pentium处理器的时候，Intel就正式引入RDMSR和WRMSR两个指令用于读和写MSR寄存器，这个时候MSR就算被正式引入。在引入RDMSR和WRMSR指令的同时，也引入了CPUID指令，该指令用于指明具体的CPU芯片中，哪些功能是可用的，或者这些功能对应的MSR寄存器是否存在，软件可以通过CPUID指令查询某些功能是否在当前CPU上是否支持。
> 每个MSR寄存器都会有一个相应的ID，即MSR Index，或者也叫作MSR寄存器地址，当执行RDMSR或者WRMSR指令的时候，只要提供MSR Index就能让CPU知道目标MSR寄存器。这些MSR寄存器的编号（MSR Index）、名字及其各个数据区域的定义可以在Intel x86架构手册”Intel 64 and IA-32 Architectures Software Developer’s Manual"的Volume 4中找到。

关于MSR的详细讨论也会在单独的一篇文章中。

## 3.6. `amd_ibs_init`

IBS用于Apic初始化，用于perf和oprofile，关于`amd_ibs_init`的详细讨论也会在单独的一篇文章中，并主要涉及`perf_event_ibs_init`的介绍。

## 3.7. `amd_iommu_pc_init`

负责DMA remapping操作的硬件称为IOMMU。做个类比：大家都知道MMU是支持内存地址虚拟化的硬件，MMU是为CPU服务的；而IOMMU是为I/O设备服务的，是将DMA地址进行虚拟化的硬件。

[《ARM SMMU原理与IOMMU技术（“VT-d” DMA、I/O虚拟化、内存虚拟化）》](https://rtoax.blog.csdn.net/article/details/108997226)
[《DMAR（DMA remapping）与 IOMMU》](https://rtoax.blog.csdn.net/article/details/109607590)
[《内核引导参数IOMMU与INTEL_IOMMU有何不同？》](https://rtoax.blog.csdn.net/article/details/109607565)
[《提升KVM异构虚拟机启动效率：透传(pass-through)、DMA映射（VFIO、PCI、IOMMU）、virtio-balloon、异步DMA映射、预处理》](https://rtoax.blog.csdn.net/article/details/109500236)

关于`iommu perf`的详细讨论也会在单独的一篇文章中，而详细介绍的是`init_one_iommu`中`perf_pmu_register`注册过程。


## 3.8. `amd_uncore_init`

以下内容摘自[《Intel微处理器Uncore架构简介》](https://blog.csdn.net/asmartkiller/article/details/109709686)：

uncore一词，是英特尔用来描述微处理器中，功能上为非处理器核心（Core）所负担，但是对处理器性能的发挥和维持有必不可少的作用的组成部分。处理器核心（Core）包含的处理器组件都涉及处理器命令的运行，包括算术逻辑单元（ALU）、浮点运算单元（FPU）、一级缓存（L1 Cache）、二级缓存（L2 Cache）。Uncore的功能包括QPI控制器、三级缓存（L3 Cache）、内存一致性监测（snoop agent pipeline）、内存控制器，以及Thunderbolt控制器。至于其余的总线控制器，像是PCI-E、SPI等，则是属于芯片组的一部分。

英特尔Uncore设计根源，来自于北桥芯片。Uncroe的设计是，将对于处理器核心有关键作用的功能重新组合编排，从物理上使它们更靠近核心（集成至处理器芯片上，而它们有部分原来是位于北桥上），以降低它们的访问延时。而北桥上余下的和处理器核心并无太大关键作用的功能，像是PCI-E控制器或者是电源控制单元（PCU），并没有集成至Uncore部分，而是继续作为芯片组的一部分。

具体而言，微架构中的uncore是被细分为数个模块单元的。uncore连接至处理器核心是通过一个叫Cache Box（CBox）的接口实现的，CBox也是末级缓存（Last Level Cache，LLC）的连接接口，同时负责管理缓存一致性。复合的内部与外部QPI链接由物理层单元（Physical Layer units）管理，称为PBox。PBox、CBox以及一个或更多的内置存储器控制器（iMC，作MBox）的连接由系统配置控制器（System Config Controller，作UBox）和路由器（Router，作RBox）负责管理。

从uncore部分移出列表总线控制器，可以更好地促进性能的提升，通过允许uncore的时钟频率（UCLK）运作于基准的2.66GHz，提升至超过超频限制值的3.44GHz，实现性能提升。这种时脉提升使得核心访问关键功能部件（像是存储器控制器）时的延时值更低（典型情况下处理器核心访问DRAM的时间可降低10纳秒或更多）。

关于`uncore`的详细讨论也会在单独的一篇文章中。

# 4. `perf_pmu_register`

注册一个PMU的接口，这将在本系列其他文章中详述。

# 5. 相关链接

* [注释源码：https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [Linux内核 eBPF基础：perf（1）：perf_event在内核中的初始化](https://rtoax.blog.csdn.net/article/details/116982544)
* [Linux内核 eBPF基础：perf（2）：性能管理单元PMU的注册](https://rtoax.blog.csdn.net/article/details/116982875)
