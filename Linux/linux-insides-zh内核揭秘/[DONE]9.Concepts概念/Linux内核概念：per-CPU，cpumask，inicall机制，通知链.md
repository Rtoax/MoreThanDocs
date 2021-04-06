<center><font size='5'>Linux内核概念</font></center>
<center><font size='6'>per-CPU，cpumask，inicall机制，通知链</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

* 在[英文原文](https://0xax.gitbook.io/linux-insides)基础上，针对[中文译文](https://xinqiu.gitbooks.io/linux-insides-cn)增加5.10.13内核源码相关内容。



# 1. Per-cpu 变量


Per-cpu 变量是一项内核特性。从它的名字你就可以理解这项特性的意义了。我们可以创建一个变量，然后每个 CPU 上都会有一个此变量的拷贝。本节我们来看下这个特性，并试着去理解它是如何实现以及工作的。

内核提供了一个创建 per-cpu 变量的 API - `DEFINE_PER_CPU` 宏：

```C
#define DEFINE_PER_CPU(type, name) \
        DEFINE_PER_CPU_SECTION(type, name, "")
```

正如其它许多处理 per-cpu 变量的宏一样，这个宏定义在 [include/linux/percpu-defs.h](https://github.com/torvalds/linux/blob/master/include/linux/percpu-defs.h) 中。现在我们来看下这个特性是如何实现的。

看下 `DECLARE_PER_CPU` 的定义，可以看到它使用了 2 个参数：`type` 和 `name`，因此我们可以这样创建 per-cpu 变量：

```C
DEFINE_PER_CPU(int, per_cpu_n)
```

我们传入要创建变量的类型和名字，`DEFINE_PER_CPU` 调用 `DEFINE_PER_CPU_SECTION`，将两个参数和空字符串传递给后者。让我们来看下 `DEFINE_PER_CPU_SECTION` 的定义：

```C
#define DEFINE_PER_CPU_SECTION(type, name, sec)    \
         __PCPU_ATTRS(sec) PER_CPU_DEF_ATTRIBUTES  \
         __typeof__(type) name
```
在5.10.13中为：
```c
#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_ATTRS(sec) __typeof__(type) name
```

```C
#define __PCPU_ATTRS(sec)                                                \
    __percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))     \
    PER_CPU_ATTRIBUTES
//5.10.13中相同
#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))	\
	PER_CPU_ATTRIBUTES
```


其中 `section` 是:

```C
#define PER_CPU_BASE_SECTION ".data..percpu"
```

当所有的宏展开之后，我们得到一个全局的 per-cpu 变量：

```C
__attribute__((section(".data..percpu"))) int per_cpu_n
```

这意味着我们在 `.data..percpu` 段有了一个 `per_cpu_n` 变量，可以在 `vmlinux` 中找到它：

```
.data..percpu 00013a58  0000000000000000  0000000001a5c000  00e00000  2**12
              CONTENTS, ALLOC, LOAD, DATA
```

好，现在我们知道了，当我们使用 `DEFINE_PER_CPU` 宏时，一个在 `.data..percpu` 段中的 per-cpu 变量就被创建了。内核初始化时，调用 `setup_per_cpu_areas` 函数多次加载 `.data..percpu` 段，每个 CPU 一次。

让我们来看下 per-cpu 区域初始化流程。它从 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 中调用 `setup_per_cpu_areas` 函数开始，这个函数定义在 [arch/x86/kernel/setup_percpu.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup_percpu.c) 中。

```C
pr_info("NR_CPUS:%d nr_cpumask_bits:%d nr_cpu_ids:%d nr_node_ids:%d\n",
        NR_CPUS, nr_cpumask_bits, nr_cpu_ids, nr_node_ids);
```

 `setup_per_cpu_areas` 开始输出在内核配置中以 `CONFIG_NR_CPUS` 配置项设置的最大 CPUs 数，实际的 CPU 个数，`nr_cpumask_bits`（对于新的 `cpumask` 操作来说和 `NR_CPUS` 是一样的），还有 `NUMA` 节点个数。

我们可以在 `dmesg` 中看到这些输出：

```
$ dmesg | grep percpu
[    0.000000] setup_percpu: NR_CPUS:8 nr_cpumask_bits:8 nr_cpu_ids:8 nr_node_ids:1
```
我的输出为：
```
# dmesg | grep NR_CPUS
[    0.394194] setup_percpu: NR_CPUS:8192 nr_cpumask_bits:255 nr_cpu_ids:255 nr_node_ids:1
```
然后我们检查 `per-cpu` 第一个块分配器。所有的 per-cpu 区域都是以块进行分配的。第一个块用于静态 per-cpu 变量。Linux 内核提供了决定第一个块分配器类型的命令行：`percpu_alloc` 。我们可以在内核文档中读到它的说明。

```
percpu_alloc=	选择要使用哪个 per-cpu 第一个块分配器。
		当前支持的类型是 "embed" 和 "page"。
        不同架构支持这些类型的子集或不支持。
        更多分配器的细节参考 mm/percpu.c 中的注释。
        这个参数主要是为了调试和性能比较的。
```

[mm/percpu.c](https://github.com/torvalds/linux/blob/master/mm/percpu.c) 包含了这个命令行选项的处理函数：

```C
early_param("percpu_alloc", percpu_alloc_setup);
```

其中 `percpu_alloc_setup` 函数根据 `percpu_alloc` 参数值设置 `pcpu_chosen_fc` 变量。默认第一个块分配器是 `auto`：

```C
enum pcpu_fc pcpu_chosen_fc __initdata = PCPU_FC_AUTO;
```

如果内核命令行中没有设置 `percpu_alloc` 参数，就会使用 `embed` 分配器，将第一个 per-cpu 块嵌入进带 [memblock](http://0xax.gitbooks.io/linux-insides/content/MM/linux-mm-1.html) 的 bootmem。最后一个分配器和第一个块 `page` 分配器一样，只是将第一个块使用 `PAGE_SIZE` 页进行了映射。

如我上面所写，首先我们在 `setup_per_cpu_areas` 中对第一个块分配器检查，检查到第一个块分配器不是 page 分配器：

```C
if (pcpu_chosen_fc != PCPU_FC_PAGE) {
    ...
    ...
    ...
}
```

如果不是 `PCPU_FC_PAGE`，我们就使用 `embed` 分配器并使用 `pcpu_embed_first_chunk` 函数分配第一块空间。

```C
rc = pcpu_embed_first_chunk(PERCPU_FIRST_CHUNK_RESERVE,
					    dyn_size, atom_size,
					    pcpu_cpu_distance,
					    pcpu_fc_alloc, pcpu_fc_free);
```

如前所述，函数 `pcpu_embed_first_chunk` 将第一个 per-cpu 块嵌入 bootmen，因此我们传递一些参数给 `pcpu_embed_first_chunk`。参数如下：

* `PERCPU_FIRST_CHUNK_RESERVE` - 为静态变量 `per-cpu` 保留空间的大小；
* `dyn_size` - 动态分配的最少空闲字节；
* `atom_size` - 所有的分配都是这个的整数倍，并以此对齐；
* `pcpu_cpu_distance` - 决定 cpus 距离的回调函数；
* `pcpu_fc_alloc` - 分配 `percpu` 页的函数；
* `pcpu_fc_free` - 释放 `percpu` 页的函数。

在调用 `pcpu_embed_first_chunk` 前我们计算好所有的参数：

```C
const size_t dyn_size = PERCPU_MODULE_RESERVE + PERCPU_DYNAMIC_RESERVE - PERCPU_FIRST_CHUNK_RESERVE;
size_t atom_size;
#ifdef CONFIG_X86_64
		atom_size = PMD_SIZE;
#else
		atom_size = PAGE_SIZE;
#endif
```

如果第一个块分配器是 `PCPU_FC_PAGE`，我们用 `pcpu_page_first_chunk` 而不是 `pcpu_embed_first_chunk`。

`per-cpu` 区域准备好以后，我们用 `setup_percpu_segment` 函数设置 `per-cpu` 的偏移和段（只针对 `x86` 系统），并将前面的数据从数组移到 `per-cpu` 变量（`x86_cpu_to_apicid`, `irq_stack_ptr` 等等）。当内核完成初始化进程后，我们就有了N个 `.data..percpu` 段，其中 N 是 CPU 个数，bootstrap 进程使用的段将会包含用 `DEFINE_PER_CPU` 宏创建的未初始化的变量。

内核提供了操作 per-cpu 变量的API：

* get_cpu_var(var)
* put_cpu_var(var)

让我们来看看 `get_cpu_var` 的实现：

```C
#define get_cpu_var(var)     \
(*({                         \
         preempt_disable();  \
         this_cpu_ptr(&var); \
}))
```

Linux 内核是抢占式的，获取 per-cpu 变量需要我们知道内核运行在哪个处理器上。因此访问 per-cpu 变量时，当前代码不能被抢占，不能移到其它的 CPU。如我们所见，这就是为什么首先调用 `preempt_disable` 函数然后调用 `this_cpu_ptr` 宏，像这样：

```C
#define this_cpu_ptr(ptr) raw_cpu_ptr(ptr)
```

以及

```C
#define raw_cpu_ptr(ptr)        per_cpu_ptr(ptr, 0)
```

`per_cpu_ptr` 返回一个指向给定 CPU（第 2 个参数） per-cpu 变量的指针。当我们创建了一个 per-cpu 变量并对其进行了修改时，我们必须调用 `put_cpu_var` 宏通过函数 `preempt_enable` 使能抢占。因此典型的 per-cpu 变量的使用如下：

```C
get_cpu_var(var);
...
//用这个 'var' 做些啥
...
put_cpu_var(var);
```

让我们来看下这个 `per_cpu_ptr` 宏：

```C
#define per_cpu_ptr(ptr, cpu)                             \
({                                                        \
        __verify_pcpu_ptr(ptr);                           \
         SHIFT_PERCPU_PTR((ptr), per_cpu_offset((cpu)));  \
})
```

就像我们上面写的，这个宏返回了一个给定 cpu 的 per-cpu 变量。首先它调用了 `__verify_pcpu_ptr`：

```C
#define __verify_pcpu_ptr(ptr)
do {
	const void __percpu *__vpp_verify = (typeof((ptr) + 0))NULL;
	(void)__vpp_verify;
} while (0)
```

该宏声明了 `ptr` 类型的 `const void __percpu *`。

之后，我们可以看到带两个参数的 `SHIFT_PERCPU_PTR` 宏的调用。第一个参数是我们的指针，第二个参数是传给 `per_cpu_offset` 宏的CPU数：

```C
#define per_cpu_offset(x) (__per_cpu_offset[x])
```

该宏将 `x` 扩展为 `__per_cpu_offset` 数组：

```C
extern unsigned long __per_cpu_offset[NR_CPUS];
```

其中 `NR_CPUS` 是 CPU 的数目。`__per_cpu_offset` 数组以 CPU 变量拷贝之间的距离填充。例如，所有 per-cpu 变量是 `X` 字节大小，所以我们通过 `__per_cpu_offset[Y]` 就可以访问 `X*Y`。让我们来看下 `SHIFT_PERCPU_PTR` 的实现：

```C
#define SHIFT_PERCPU_PTR(__p, __offset)                                 \
         RELOC_HIDE((typeof(*(__p)) __kernel __force *)(__p), (__offset))
```

`RELOC_HIDE` 只是取得偏移量 `(typeof(ptr)) (__ptr + (off))`，并返回一个指向该变量的指针。

就这些了！当然这不是全部的 API，只是一个大概。开头是比较艰难，但是理解 per-cpu 变量你只需理解 [include/linux/percpu-defs.h](https://github.com/torvalds/linux/blob/master/include/linux/percpu-defs.h) 的奥秘。

让我们再看下获得 per-cpu 变量指针的算法：

* 内核在初始化流程中创建多个 `.data..percpu` 段（一个 per-cpu 变量一个）；
* 所有 `DEFINE_PER_CPU` 宏创建的变量都将重新分配到首个扇区或者 CPU0；
* `__per_cpu_offset` 数组以 (`BOOT_PERCPU_OFFSET`) 和 `.data..percpu` 扇区之间的距离填充；
* 当 `per_cpu_ptr` 被调用时，例如取一个 per-cpu 变量的第三个 CPU 的指针，将访问 `__per_cpu_offset` 数组，该数组的索引指向了所需 CPU。

就这么多了。


# 2. CPU masks


`Cpumasks` 是Linux内核提供的保存系统CPU信息的特殊方法。包含 `Cpumasks` 操作 API 相关的源码和头文件：

* [include/linux/cpumask.h](https://github.com/torvalds/linux/blob/master/include/linux/cpumask.h)
* [lib/cpumask.c](https://github.com/torvalds/linux/blob/master/lib/cpumask.c)
* [kernel/cpu.c](https://github.com/torvalds/linux/blob/master/kernel/cpu.c)

正如 [include/linux/cpumask.h](https://github.com/torvalds/linux/blob/master/include/linux/cpumask.h) 注释：Cpumasks 提供了代表系统中 CPU 集合的位图，一位放置一个 CPU 序号。我们已经在 [Kernel entry point](http://0xax.gitbooks.io/linux-insides/content/Initialization/linux-initialization-4.html) 部分，函数 `boot_cpu_init` 中看到了一点 cpumask。这个函数将第一个启动的 cpu 上线、激活等等……

```C
set_cpu_online(cpu, true);
set_cpu_active(cpu, true);
set_cpu_present(cpu, true);
set_cpu_possible(cpu, true);
```

`set_cpu_possible` 是一个在系统启动时任意时刻都可插入的 cpu ID 集合。`cpu_present` 代表了当前插入的 CPUs。`cpu_online` 是 `cpu_present` 的子集，表示可调度的 CPUs。这些掩码依赖于 `CONFIG_HOTPLUG_CPU` 配置选项，以及 `possible == present` 和 `active == online` 选项是否被禁用。这些函数的实现很相似，检测第二个参数，如果为 `true`，就调用 `cpumask_set_cpu` ，否则调用 `cpumask_clear_cpu`。

有两种方法创建 `cpumask`。第一种是用 `cpumask_t`。定义如下：

```C
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;
```

它封装了 `cpumask` 结构，其包含了一个位掩码 `bits` 字段。`DECLARE_BITMAP` 宏有两个参数：

* bitmap name;
* number of bits.

并以给定名称创建了一个 `unsigned long` 数组。它的实现非常简单：

```C
#define DECLARE_BITMAP(name,bits) \
        unsigned long name[BITS_TO_LONGS(bits)]
```

其中 `BITS_TO_LONGS`：

```C
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
```

因为我们专注于 `x86_64` 架构，`unsigned long` 是8字节大小，因此我们的数组仅包含一个元素：

```
(((8) + (8) - 1) / (8)) = 1
```

`NR_CPUS` 宏表示的是系统中 CPU 的数目，且依赖于在 [include/linux/threads.h](https://github.com/torvalds/linux/blob/master/include/linux/threads.h) 中定义的 `CONFIG_NR_CPUS` 宏，看起来像这样：

```C
#ifndef CONFIG_NR_CPUS
        #define CONFIG_NR_CPUS  1
#endif

#define NR_CPUS         CONFIG_NR_CPUS
```

第二种定义 cpumask 的方法是直接使用宏 `DECLARE_BITMAP` 和 `to_cpumask` 宏，后者将给定的位图转化为 `struct cpumask *`：

```C
#define to_cpumask(bitmap)                                              \
        ((struct cpumask *)(1 ? (bitmap)                                \
                            : (void *)sizeof(__check_is_bitmap(bitmap))))
```

可以看到这里的三目运算符每次总是 `true`。`__check_is_bitmap` 内联函数定义为：

```C
static inline int __check_is_bitmap(const unsigned long *bitmap)
{
        return 1;
}
```

每次都是返回 `1`。我们需要它只是因为：编译时检测一个给定的 `bitmap` 是一个位图，换句话说，它检测一个 `bitmap` 是否有 `unsigned long *` 类型。因此我们传递 `cpu_possible_bits` 给宏 `to_cpumask` ，将 `unsigned long` 数组转换为 `struct cpumask *`。

## 2.1. cpumask API


因为我们可以用其中一个方法来定义 cpumask，Linux 内核提供了 API 来处理 cpumask。我们来研究下其中一个函数，例如 `set_cpu_online`，这个函数有两个参数：

* CPU 数目;
* CPU 状态;

这个函数的实现如下所示：

```C
void set_cpu_online(unsigned int cpu, bool online)
{
	if (online) {
		cpumask_set_cpu(cpu, to_cpumask(cpu_online_bits));
		cpumask_set_cpu(cpu, to_cpumask(cpu_active_bits));
	} else {
		cpumask_clear_cpu(cpu, to_cpumask(cpu_online_bits));
	}
}
```

该函数首先检测第二个 `state` 参数并调用依赖它的 `cpumask_set_cpu` 或 `cpumask_clear_cpu`。这里我们可以看到在中 `cpumask_set_cpu` 的第二个参数转换为 `struct cpumask *`。在我们的例子中是位图 `cpu_online_bits`，定义如下：

```C
static DECLARE_BITMAP(cpu_online_bits, CONFIG_NR_CPUS) __read_mostly;
```

函数 `cpumask_set_cpu` 仅调用了一次 `set_bit` 函数：

```C
static inline void cpumask_set_cpu(unsigned int cpu, struct cpumask *dstp)
{
        set_bit(cpumask_check(cpu), cpumask_bits(dstp));
}
```

`set_bit` 函数也有两个参数，设置了一个给定位（第一个参数）的内存（第二个参数或 `cpu_online_bits` 位图）。这儿我们可以看到在调用 `set_bit` 之前，它的两个参数会传递给

* cpumask_check;
* cpumask_bits.

让我们细看下这两个宏。第一个 `cpumask_check` 在我们的例子里没做任何事，只是返回了给的参数。第二个 `cpumask_bits` 只是返回了传入 `struct cpumask *` 结构的 `bits` 域。

```C
#define cpumask_bits(maskp) ((maskp)->bits)
```

现在让我们看下 `set_bit` 的实现：

```C
 static __always_inline void
 set_bit(long nr, volatile unsigned long *addr)
 {
         if (IS_IMMEDIATE(nr)) {
                asm volatile(LOCK_PREFIX "orb %1,%0"
                        : CONST_MASK_ADDR(nr, addr)
                        : "iq" ((u8)CONST_MASK(nr))
                        : "memory");
        } else {
                asm volatile(LOCK_PREFIX "bts %1,%0"
                        : BITOP_ADDR(addr) : "Ir" (nr) : "memory");
        }
 }
```

这个函数看着吓人，但它没有看起来那么难。首先传参 `nr` 或者说位数给 `IS_IMMEDIATE` 宏，该宏调用了 GCC 内联函数 `__builtin_constant_p`：

```C
#define IS_IMMEDIATE(nr)    (__builtin_constant_p(nr))
```

`__builtin_constant_p` 检查给定参数是否编译时恒定变量。因为我们的 `cpu` 不是编译时恒定变量，将会执行 `else` 分支：

```C
asm volatile(LOCK_PREFIX "bts %1,%0" : BITOP_ADDR(addr) : "Ir" (nr) : "memory");
```

让我们试着一步一步来理解它如何工作的：

`LOCK_PREFIX` 是个 x86 `lock` 指令。这个指令告诉 CPU 当指令执行时占据系统总线。这允许 CPU 同步内存访问，防止多核（或多设备 - 比如 DMA 控制器）并发访问同一个内存cell。

`BITOP_ADDR` 转换给定参数至 `(*(volatile long *)` 并且加了 `+m` 约束。`+` 意味着这个操作数对于指令是可读写的。`m` 显示这是一个内存操作数。`BITOP_ADDR` 定义如下：

```C
#define BITOP_ADDR(x) "+m" (*(volatile long *) (x))
```

接下来是 `memory`。它告诉编译器汇编代码执行内存读或写到某些项，而不是那些输入或输出操作数（例如，访问指向输出参数的内存）。

`Ir` - 寄存器操作数。

`bts` 指令设置一个位字符串的给定位，存储给定位的值到 `CF` 标志位。所以我们传递 cpu 号，我们的例子中为 0，给 `set_bit` 并且执行后，其设置了在 `cpu_online_bits` cpumask 中的 0 位。这意味着第一个 cpu 此时上线了。

当然，除了 `set_cpu_*` API 外，cpumask 提供了其它 cpumasks 操作的 API。让我们简短看下。

## 2.2. 附加的 cpumask API


cpumaks 提供了一系列宏来得到不同状态 CPUs 序号。例如：

```C
#define num_online_cpus()	cpumask_weight(cpu_online_mask)
```

这个宏返回了 `online` CPUs 数量。它读取 `cpu_online_mask` 位图并调用了 `cpumask_weight` 函数。`cpumask_weight` 函数使用两个参数调用了一次 `bitmap_weight` 函数：

* cpumask bitmap;
* `nr_cpumask_bits` - 在我们的例子中就是 `NR_CPUS`。

```C
static inline unsigned int cpumask_weight(const struct cpumask *srcp)
{
	return bitmap_weight(cpumask_bits(srcp), nr_cpumask_bits);
}
```

并计算给定位图的位数。除了 `num_online_cpus`，cpumask还提供了所有 CPU 状态的宏：

* num_possible_cpus;
* num_active_cpus;
* cpu_online;
* cpu_possible.

等等。

除了 Linux 内核提供的下述操作 `cpumask` 的 API：

* `for_each_cpu` - 遍历一个mask的所有 cpu;
* `for_each_cpu_not` - 遍历所有补集的 cpu;
* `cpumask_clear_cpu` - 清除一个 cpumask 的 cpu;
* `cpumask_test_cpu` - 测试一个 mask 中的 cpu;
* `cpumask_setall` - 设置 mask 的所有 cpu;
* `cpumask_size` - 返回分配 'struct cpumask' 字节数大小;

还有很多。

## 2.3. 链接


* [cpumask documentation](https://www.kernel.org/doc/Documentation/cpu-hotplug.txt)


# 3. initcall 机制

先直接给出代码：

```c
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
                            ->xxx__initcall()
```
其中的一个结构体为：
```c
extern initcall_entry_t __initcall_start[];
extern initcall_entry_t __initcall0_start[];
extern initcall_entry_t __initcall1_start[];
extern initcall_entry_t __initcall2_start[];
extern initcall_entry_t __initcall3_start[];
extern initcall_entry_t __initcall4_start[];
extern initcall_entry_t __initcall5_start[];
extern initcall_entry_t __initcall6_start[];
extern initcall_entry_t __initcall7_start[];
extern initcall_entry_t __initcall_end[];

static initcall_entry_t __initdata*initcall_levels[]  = {
	__initcall0_start,
	__initcall1_start,
	__initcall2_start,
	__initcall3_start,
	__initcall4_start,
	__initcall5_start,
	__initcall6_start,
	__initcall7_start,
	__initcall_end,
};
```
在vmlinux.lds中：
```asm
	__initcall0_start = .; 
	KEEP(*(.initcall0.init)) KEEP(*(.initcall0s.init)) 

	__initcall1_start = .; 
	KEEP(*(.initcall1.init)) KEEP(*(.initcall1s.init)) 

	__initcall2_start = .; 
	KEEP(*(.initcall2.init)) KEEP(*(.initcall2s.init)) 

	__initcall3_start = .; 
	KEEP(*(.initcall3.init)) KEEP(*(.initcall3s.init)) 

	__initcall4_start = .; 
	KEEP(*(.initcall4.init)) KEEP(*(.initcall4s.init)) 

	__initcall5_start = .; 
	KEEP(*(.initcall5.init)) KEEP(*(.initcall5s.init)) 

	__initcallrootfs_start = .; 
	KEEP(*(.initcallrootfs.init)) KEEP(*(.initcallrootfss.init)) 

	__initcall6_start = .; 
	KEEP(*(.initcall6.init)) KEEP(*(.initcall6s.init)) 

	__initcall7_start = .; 
	KEEP(*(.initcall7.init)) KEEP(*(.initcall7s.init)) 
```
再看一眼___define_initcall的定义：

```c
#define ___define_initcall(fn, id, __sec)		/* vmlinux.lds.S  */	\
	__ADDRESSABLE(fn)					\
	asm(".section	\"" #__sec ".init\", \"a\"	\n"	\
	"__initcall_" #fn #id ":			\n"	\
	    ".long	" #fn " - .			\n"	\
	    ".previous					\n");
```

就像你从标题所理解的，这部分将涉及 Linux 内核中有趣且重要的概念，称之为 `initcall`。在 Linux 内核中，我们可以看到类似这样的定义：

```C
early_param("debug", debug_kernel);
```

或者

```C
arch_initcall(init_pit_clocksource);
```

在我们分析这个机制在内核中是如何实现的之前，我们必须了解这个机制是什么，以及在 Linux 内核中是如何使用它的。像这样的定义表示一个 [回调函数](https://en.wikipedia.org/wiki/Callback_%28computer_programming%29) ，它们会在 Linux 内核启动中或启动后调用。实际上 `initcall` 机制的要点是确定内置模块和子系统初始化的正确顺序。举个例子，我们来看看下面的函数：

```C
static int __init nmi_warning_debugfs(void)
{
    debugfs_create_u64("nmi_longest_ns", 0644,
                       arch_debugfs_dir, &nmi_longest_ns);
    return 0;
}
```

这个函数出自源码文件 [arch/x86/kernel/nmi.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/nmi.c)。我们可以看到，这个函数只是在 `arch_debugfs_dir` 目录中创建 `nmi_longest_ns` [debugfs](https://en.wikipedia.org/wiki/Debugfs) 文件。实际上，只有在 `arch_debugfs_dir` 创建后，才会创建这个 `debugfs` 文件。这个目录是在 Linux 内核特定架构的初始化期间创建的。实际上，该目录将在源码文件 [arch/x86/kernel/kdebugfs.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/kdebugfs.c) 的 `arch_kdebugfs_init` 函数中创建。注意 `arch_kdebugfs_init` 函数也被标记为 `initcall`。

```C
arch_initcall(arch_kdebugfs_init);
```

Linux 内核在调用 `fs` 相关的 `initcalls` 之前调用所有特定架构的 `initcalls`。因此，只有在 `arch_kdebugfs_dir` 目录创建以后才会创建我们的 `nmi_longest_ns`。实际上，Linux 内核提供了八个级别的主 `initcalls`：

* `early`;
* `core`;
* `postcore`;
* `arch`;
* `susys`;
* `fs`;
* `device`;
* `late`.

它们的所有名称是由数组 `initcall_level_names` 来描述的，该数组定义在源码文件 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 中：

```C
static char *initcall_level_names[] __initdata = {
	"early",
	"core",
	"postcore",
	"arch",
	"subsys",
	"fs",
	"device",
	"late",
};
```

所有用这些标识符标记为 `initcall` 的函数将会以相同的顺序被调用，或者说，`early initcalls` 会首先被调用，其次是 `core initcalls`，以此类推。现在，我们对 `initcall` 机制了解点了，所以我们可以开始潜入 Linux 内核源码，来看看这个机制是如何实现的。

## 3.1. initcall 机制在 Linux 内核中的实现


Linux 内核提供了一组来自头文件 [include/linux/init.h](https://github.com/torvalds/linux/blob/master/include/linux/init.h) 的宏，来标记给定的函数为 `initcall`。所有这些宏都相当简单：

```C
#define early_initcall(fn)		__define_initcall(fn, early)
#define core_initcall(fn)		__define_initcall(fn, 1)
#define postcore_initcall(fn)		__define_initcall(fn, 2)
#define arch_initcall(fn)		__define_initcall(fn, 3)
#define subsys_initcall(fn)		__define_initcall(fn, 4)
#define fs_initcall(fn)			__define_initcall(fn, 5)
#define device_initcall(fn)		__define_initcall(fn, 6)
#define late_initcall(fn)		__define_initcall(fn, 7)
```

我们可以看到，这些宏只是从同一个头文件的 `__define_initcall` 宏的调用扩展而来。此外，`__define_initcall` 宏有两个参数：

* `fn` - 在调用某个级别 `initcalls` 时调用的回调函数；
* `id` - 识别 `initcall` 的标识符，用来防止两个相同的 `initcalls` 指向同一个处理函数时出现错误。

`__define_initcall` 宏的实现如下所示：

```C
#define __define_initcall(fn, id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" #id ".init"))) = fn; \
	LTO_REFERENCE_INITCALL(__initcall_##fn##id)
```

要了解 `__define_initcall` 宏，首先让我们来看下 `initcall_t` 类型。这个类型定义在同一个 [头文件]() 中，它表示一个返回 [整形](https://en.wikipedia.org/wiki/Integer)指针的函数指针，这将是 `initcall` 的结果：

```C
typedef int (*initcall_t)(void);
```

现在让我们回到 `_-define_initcall` 宏。[##](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html) 提供了连接两个符号的能力。在我们的例子中，`__define_initcall` 宏的第一行产生了 `.initcall id .init` [ELF 部分](http://www.skyfree.org/linux/references/ELF_Format.pdf) 给定函数的定义，并标记以下 [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection) 属性： `__initcall_function_name_id` 和 `__used`。如果我们查看表示内核链接脚本数据的 [include/asm-generic/vmlinux.lds.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/vmlinux.lds.h) 头文件，我们会看到所有的 `initcalls` 部分都将放在 `.data` 段：

```C
#define INIT_CALLS					\
		VMLINUX_SYMBOL(__initcall_start) = .;	\
		*(.initcallearly.init)					\
		INIT_CALLS_LEVEL(0)					    \
		INIT_CALLS_LEVEL(1)					    \
		INIT_CALLS_LEVEL(2)					    \
		INIT_CALLS_LEVEL(3)					    \
		INIT_CALLS_LEVEL(4)					    \
		INIT_CALLS_LEVEL(5)					    \
		INIT_CALLS_LEVEL(rootfs)				\
		INIT_CALLS_LEVEL(6)					    \
		INIT_CALLS_LEVEL(7)					    \
		VMLINUX_SYMBOL(__initcall_end) = .;

#define INIT_DATA_SECTION(initsetup_align)	\
	.init.data : AT(ADDR(.init.data) - LOAD_OFFSET) {	   \
        ...                                                \
        INIT_CALLS						                   \
        ...                                                \
	}

```

第二个属性 - `__used`，定义在 [include/linux/compiler-gcc.h](https://github.com/torvalds/linux/blob/master/include/linux/compiler-gcc.h) 头文件中，它扩展了以下 `gcc` 定义：

```C
#define __used   __attribute__((__used__))
```

它防止 `定义了变量但未使用` 的告警。宏 `__define_initcall` 最后一行是：

```C
LTO_REFERENCE_INITCALL(__initcall_##fn##id)
```

这取决于 `CONFIG_LTO` 内核配置选项，只为编译器提供[链接时间优化](https://gcc.gnu.org/wiki/LinkTimeOptimization)存根：

```
#ifdef CONFIG_LTO
#define LTO_REFERENCE_INITCALL(x) \
        static __used __exit void *reference_##x(void)  \
        {                                               \
                return &x;                              \
        }
#else
#define LTO_REFERENCE_INITCALL(x)
#endif
```

为了防止当模块中的变量没有引用时而产生的任何问题，它被移到了程序末尾。这就是关于 `__define_initcall` 宏的全部了。所以，所有的 `*_initcall` 宏将会在Linux内核编译时扩展，所有的 `initcalls` 会放置在它们的段内，并可以通过 `.data` 段来获取，Linux 内核在初始化过程中就知道在哪儿去找到 `initcall` 并调用它。

既然 Linux 内核可以调用 `initcalls`，我们就来看下 Linux 内核是如何做的。这个过程从 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 头文件的 `do_basic_setup` 函数开始：

```C
static void __init do_basic_setup(void)
{
    ...
    ...
    ...
   	do_initcalls();
    ...
    ...
    ...
}
```

该函数在 Linux 内核初始化过程中调用，调用时机是主要的初始化步骤，比如内存管理器相关的初始化、`CPU` 子系统等完成之后。`do_initcalls` 函数只是遍历 `initcall` 级别数组，并调用每个级别的 `do_initcall_level` 函数：

```C
static void __init do_initcalls(void)
{
	int level;

	for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++)
		do_initcall_level(level);
}
```

`initcall_levels` 数组在同一个源码[文件](https://github.com/torvalds/linux/blob/master/init/main.c)中定义，包含了定义在 `__define_initcall` 宏中的那些段的指针：

```C
static initcall_t *initcall_levels[] __initdata = {
	__initcall0_start,
	__initcall1_start,
	__initcall2_start,
	__initcall3_start,
	__initcall4_start,
	__initcall5_start,
	__initcall6_start,
	__initcall7_start,
	__initcall_end,
};
```

如果你有兴趣，你可以在 Linux 内核编译后生成的链接器脚本 `arch/x86/kernel/vmlinux.lds` 中找到这些段：

```
.init.data : AT(ADDR(.init.data) - 0xffffffff80000000) {
    ...
    ...
    ...
    ...
    __initcall_start = .;
    *(.initcallearly.init)
    __initcall0_start = .;
    *(.initcall0.init)
    *(.initcall0s.init)
    __initcall1_start = .;
    ...
    ...
}
```

如果你对这些不熟，可以在本书的某些[部分](https://0xax.gitbooks.io/linux-insides/content/Misc/linkers.html)了解更多关于[链接器](https://en.wikipedia.org/wiki/Linker_%28computing%29)的信息。

正如我们刚看到的，`do_initcall_level` 函数有一个参数 - `initcall` 的级别，做了以下两件事：首先这个函数拷贝了 `initcall_command_line`，这是通常内核包含了各个模块参数的[命令行](https://www.kernel.org/doc/Documentation/kernel-parameters.txt)的副本，并用 [kernel/params.c](https://github.com/torvalds/linux/blob/master/kernel/params.c)源码文件的 `parse_args` 函数解析它，然后调用各个级别的 `do_on_initcall` 函数：

```C
for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
		do_one_initcall(*fn);
```

`do_on_initcall` 为我们做了主要的工作。我们可以看到，这个函数有一个参数表示 `initcall` 回调函数，并调用给定的回调函数：

```C
int __init_or_module do_one_initcall(initcall_t fn)
{
	int count = preempt_count();
	int ret;
	char msgbuf[64];

	if (initcall_blacklisted(fn))
		return -EPERM;

	if (initcall_debug)
		ret = do_one_initcall_debug(fn);
	else
		ret = fn();

	msgbuf[0] = 0;

	if (preempt_count() != count) {
		sprintf(msgbuf, "preemption imbalance ");
		preempt_count_set(count);
	}
	if (irqs_disabled()) {
		strlcat(msgbuf, "disabled interrupts ", sizeof(msgbuf));
		local_irq_enable();
	}
	WARN(msgbuf[0], "initcall %pF returned with %s\n", fn, msgbuf);

	return ret;
}
```

让我们来试着理解 `do_on_initcall` 函数做了什么。首先我们增加 [preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29) 计数，以便我们稍后进行检查，确保它不是不平衡的。这步以后，我们可以看到 `initcall_backlist` 函数的调用，这个函数遍历包含了 `initcalls` 黑名单的 `blacklisted_initcalls` 链表，如果 `initcall` 在黑名单里就释放它：

```C
list_for_each_entry(entry, &blacklisted_initcalls, next) {
	if (!strcmp(fn_name, entry->buf)) {
		pr_debug("initcall %s blacklisted\n", fn_name);
		kfree(fn_name);
		return true;
	}
}
```

黑名单的 `initcalls` 保存在 `blacklisted_initcalls` 链表中，这个链表是在早期 Linux 内核初始化时由 Linux 内核命令行来填充的。

处理完进入黑名单的 `initcalls`，接下来的代码直接调用 `initcall`：

```C
if (initcall_debug)
	ret = do_one_initcall_debug(fn);
else
	ret = fn();
```

取决于 `initcall_debug` 变量的值，`do_one_initcall_debug` 函数将调用 `initcall`，或直接调用 `fn()`。`initcall_debug` 变量定义在[同一个源码文件](https://github.com/torvalds/linux/blob/master/init/main.c)：

```C
bool initcall_debug;
```

该变量提供了向内核[日志缓冲区](https://en.wikipedia.org/wiki/Dmesg)打印一些信息的能力。可以通过 `initcall_debug` 参数从内核命令行中设置这个变量的值。从Linux内核命令行[文档](https://www.kernel.org/doc/Documentation/kernel-parameters.txt)可以看到：

```
initcall_debug	[KNL] Trace initcalls as they are executed.  Useful
                      for working out where the kernel is dying during
                      startup.
```

确实如此。如果我们看下 `do_one_initcall_debug` 函数的实现，我们会看到它与 `do_one_initcall` 函数做了一样的事，也就是说，`do_one_initcall_debug` 函数调用了给定的 `initcall`，并打印了一些和 `initcall` 相关的信息（比如当前任务的 [pid](https://en.wikipedia.org/wiki/Process_identifier)、`initcall` 的持续时间等）：

```C
static int __init_or_module do_one_initcall_debug(initcall_t fn)
{
	ktime_t calltime, delta, rettime;
	unsigned long long duration;
	int ret;

	printk(KERN_DEBUG "calling  %pF @ %i\n", fn, task_pid_nr(current));
	calltime = ktime_get();
	ret = fn();
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	printk(KERN_DEBUG "initcall %pF returned %d after %lld usecs\n",
		 fn, ret, duration);

	return ret;
}
```

由于 `initcall` 被 `do_one_initcall` 或 `do_one_initcall_debug` 调用，我们可以看到在 `do_one_initcall` 函数末尾做了两次检查。第一个检查在initcall执行内部 `__preempt_count_add` 和 `__preempt_count_sub` 可能的执行次数，如果这个值和之前的可抢占计数不相等，我们就把 `preemption imbalance` 字符串添加到消息缓冲区，并设置正确的可抢占计数：

```C
if (preempt_count() != count) {
	sprintf(msgbuf, "preemption imbalance ");
	preempt_count_set(count);
}
```

稍后这个错误字符串就会被打印出来。最后检查本地 [IRQs](https://en.wikipedia.org/wiki/Interrupt_request_%28PC_architecture%29) 的状态，如果它们被禁用了，我们就将 `disabled interrupts` 字符串添加到我们的消息缓冲区，并为当前处理器使能 `IRQs`，以防出现 `IRQs` 被 `initcall` 禁用了但不再使能的情况出现：

```C
if (irqs_disabled()) {
	strlcat(msgbuf, "disabled interrupts ", sizeof(msgbuf));
	local_irq_enable();
}
```

这就是全部了。通过这种方式，Linux 内核以正确的顺序完成了很多子系统的初始化。现在我们知道 Linux 内核的 `initcall` 机制是怎么回事了。在这部分中，我们介绍了 `initcall` 机制的主要部分，但遗留了一些重要的概念。让我们来简单看下这些概念。

首先，我们错过了一个级别的 `initcalls`，就是 `rootfs initcalls`。和我们在本部分看到的很多宏类似，你可以在 [include/linux/init.h](https://github.com/torvalds/linux/blob/master/include/linux/init.h) 头文件中找到 `rootfs_initcall` 的定义：

```C
#define rootfs_initcall(fn)		__define_initcall(fn, rootfs)
```

从这个宏的名字我们可以理解到，它的主要目的是保存和 [rootfs](https://en.wikipedia.org/wiki/Initramfs) 相关的回调。除此之外，只有在与设备相关的东西没被初始化时，在文件系统级别初始化以后再初始化一些其它东西时才有用。例如，发生在源码文件 [init/initramfs.c](https://github.com/torvalds/linux/blob/master/init/initramfs.c) 中 `populate_rootfs` 函数里的解压  [initramfs](https://en.wikipedia.org/wiki/Initramfs)：

```C
rootfs_initcall(populate_rootfs);
```

在这里，我们可以看到熟悉的输出：

```
[    0.199960] Unpacking initramfs...
```

除了 `rootfs_initcall` 级别，还有其它的 `console_initcall`、 `security_initcall` 和其他辅助的  `initcall` 级别。我们遗漏的最后一件事，是 `*_initcall_sync` 级别的集合。在这部分我们看到的几乎每个 `*_initcall` 宏，都有 `_sync` 前缀的宏伴随：

```C
#define core_initcall_sync(fn)		__define_initcall(fn, 1s)
#define postcore_initcall_sync(fn)	__define_initcall(fn, 2s)
#define arch_initcall_sync(fn)		__define_initcall(fn, 3s)
#define subsys_initcall_sync(fn)	__define_initcall(fn, 4s)
#define fs_initcall_sync(fn)		__define_initcall(fn, 5s)
#define device_initcall_sync(fn)	__define_initcall(fn, 6s)
#define late_initcall_sync(fn)		__define_initcall(fn, 7s)
```

这些附加级别的主要目的是，等待所有某个级别的与模块相关的初始化例程完成。

这就是全部了。

## 3.2. 结论


在这部分中，我们看到了 Linux 内核的一项重要机制，即在初始化期间允许调用依赖于 Linux 内核当前状态的函数。

如果你有问题或建议，可随时在 twitter [0xAX](https://twitter.com/0xAX) 上联系我，给我发 [email](anotherworldofworld@gmail.com)，或者创建 [issue](https://github.com/0xAX/linux-insides/issues/new)。

## 3.3. 链接


* [callback](https://en.wikipedia.org/wiki/Callback_%28computer_programming%29)
* [debugfs](https://en.wikipedia.org/wiki/Debugfs)
* [integer type](https://en.wikipedia.org/wiki/Integer)
* [symbols concatenation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html)
* [GCC](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [Link time optimization](https://gcc.gnu.org/wiki/LinkTimeOptimization)
* [Introduction to linkers](https://0xax.gitbooks.io/linux-insides/content/Misc/linkers.html)
* [Linux kernel command line](https://www.kernel.org/doc/Documentation/kernel-parameters.txt)
* [Process identifier](https://en.wikipedia.org/wiki/Process_identifier)
* [IRQs](https://en.wikipedia.org/wiki/Interrupt_request_%28PC_architecture%29)
* [rootfs](https://en.wikipedia.org/wiki/Initramfs)
* [previous part](https://0xax.gitbooks.io/linux-insides/content/Concepts/cpumask.html)




# 4. 内核中的通知链


The Linux kernel is huge piece of [C](https://en.wikipedia.org/wiki/C_(programming_language)) code which consists from many different subsystems. Each subsystem has its own purpose which is independent of other subsystems. But often one subsystem wants to know something from other subsystem(s). There is special mechanism in the Linux kernel which allows to solve this problem partly. The name of this mechanism is - `notification chains` and its main purpose to provide a way for different subsystems to subscribe on asynchronous events from other subsystems. Note that this mechanism is only for communication inside kernel, but there are other mechanisms for communication between kernel and userspace. 

Before we will consider `notification chains` [API](https://en.wikipedia.org/wiki/Application_programming_interface) and implementation of this API, let's look at `Notification chains` mechanism from theoretical side as we did it in other parts of this book. Everything which is related to `notification chains` mechanism is located in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) header file and [kernel/notifier.c](https://github.com/torvalds/linux/blob/master/kernel/notifier.c) source code file. So let's open them and start to dive.

## 4.1. 通知链相关数据结构


Let's start to consider `notification chains` mechanism from related data structures. As I wrote above, main data structures should be located in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) header file, so the Linux kernel provides generic API which does not depend on certain architecture. In general, the `notification chains` mechanism represents a list (that's why it named `chains`) of [callback](https://en.wikipedia.org/wiki/Callback_(computer_programming)) functions which are will be executed when an event will be occurred.

All of these callback functions are represented as `notifier_fn_t` type in the Linux kernel:

```C
typedef	int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action, void *data);
```

So we may see that it takes three following arguments:

* `nb` - is linked list of function pointers (will see it now);
* `action` - is type of an event. A notification chain may support multiple events, so we need this parameter to distinguish an event from other events;
* `data` - is storage for private information. Actually it allows to provide additional data information about an event.

Additionally we may see that `notifier_fn_t` returns an integer value. This integer value maybe one of:

* `NOTIFY_DONE` - subscriber does not interested in notification;
* `NOTIFY_OK` - notification was processed correctly;
* `NOTIFY_BAD` - something went wrong;
* `NOTIFY_STOP` - notification is done, but no further callbacks should be called for this event.

All of these results defined as macros in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) header file:

```C
#define NOTIFY_DONE		0x0000
#define NOTIFY_OK		0x0001
#define NOTIFY_BAD		(NOTIFY_STOP_MASK|0x0002)
#define NOTIFY_STOP		(NOTIFY_OK|NOTIFY_STOP_MASK)
```

Where `NOTIFY_STOP_MASK` represented by the:

```C
#define NOTIFY_STOP_MASK	0x8000
```

macro and means that callbacks will not be called during next notifications.

Each part of the Linux kernel which wants to be notified on a certain event will should provide own `notifier_fn_t` callback function. Main role of the `notification chains` mechanism is to call certain callbacks when an asynchronous event occurred.

The main building block of the `notification chains` mechanism is the `notifier_block` structure:

```C
struct notifier_block {
	notifier_fn_t notifier_call;
	struct notifier_block __rcu *next;
	int priority;
};
```

which is defined in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) file. This struct contains pointer to callback function - `notifier_call`, link to the next notification callback and `priority` of a callback function as functions with higher priority are executed first.

The Linux kernel provides notification chains of four following types:

* Blocking notifier chains;
* SRCU notifier chains;
* Atomic notifier chains;
* Raw notifier chains.

Let's consider all of these types of notification chains by order:

In the first case for the `blocking notifier chains`, callbacks will be called/executed in process context. This means that the calls in a notification chain may be blocked.

The second `SRCU notifier chains` represent alternative form of `blocking notifier chains`. In the first case, blocking notifier chains uses `rw_semaphore` synchronization primitive to protect chain links. `SRCU` notifier chains run in process context too, but uses special form of [RCU](https://en.wikipedia.org/wiki/Read-copy-update) mechanism which is permissible to block in an read-side critical section.

In the third case for the `atomic notifier chains` runs in interrupt or atomic context and protected by [spinlock](https://0xax.gitbooks.io/linux-insides/content/SyncPrim/sync-1.html) synchronization primitive. The last `raw notifier chains` provides special type of notifier chains without any locking restrictions on callbacks. This means that protection rests on the shoulders of caller side. It is very useful when we want to protect our chain with very specific locking mechanism.

If we will look at the implementation of the `notifier_block` structure, we will see that it contains pointer to the `next` element from a notification chain list, but we have no head. Actually a head of such list is in separate structure depends on type of a notification chain. For example for the `blocking notifier chains`:

```C
struct blocking_notifier_head {
	struct rw_semaphore rwsem;
	struct notifier_block __rcu *head;
};
```

or for `atomic notification chains`:

```C
struct atomic_notifier_head {
	spinlock_t lock;
	struct notifier_block __rcu *head;
};
```

Now as we know a little about `notification chains` mechanism let's consider implementation of its API.

## 4.2. Notification Chains


Usually there are two sides in a publish/subscriber mechanisms. One side who wants to get notifications and other side(s) who generates these notifications. We will consider notification chains mechanism from both sides. We will consider `blocking notification chains` in this part, because of other types of notification chains are similar to it and differs mostly in protection mechanisms.

Before a notification producer is able to produce notification, first of all it should initialize head of a notification chain. For example let's consider notification chains related to kernel [loadable modules](https://en.wikipedia.org/wiki/Loadable_kernel_module). If we will look in the [kernel/module.c](https://github.com/torvalds/linux/blob/master/kernel/module.c) source code file, we will see following definition:

```C
static BLOCKING_NOTIFIER_HEAD(module_notify_list);
```

which defines head for loadable modules blocking notifier chain. The `BLOCKING_NOTIFIER_HEAD` macro is defined in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) header file and expands to the following code:

```C
#define BLOCKING_INIT_NOTIFIER_HEAD(name) do {	\
		init_rwsem(&(name)->rwsem);	                            \
		(name)->head = NULL;		                            \
	} while (0)
```

So we may see that it takes name of a name of a head of a blocking notifier chain and initializes read/write [semaphore](https://0xax.gitbooks.io/linux-insides/content/SyncPrim/sync-3.html) and set head to `NULL`. Besides the `BLOCKING_INIT_NOTIFIER_HEAD` macro, the Linux kernel additionally provides `ATOMIC_INIT_NOTIFIER_HEAD`, `RAW_INIT_NOTIFIER_HEAD` macros and `srcu_init_notifier` function for initialization atomic and other types of notification chains.

After initialization of a head of a notification chain, a subsystem which wants to receive notification from the given notification chain it should register with certain function which is depends on type of notification. If you will look in the [include/linux/notifier.h](https://github.com/torvalds/linux/blob/master/include/linux/notifier.h) header file, you will see following four function for this:

```C
extern int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
		struct notifier_block *nb);

extern int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
		struct notifier_block *nb);

extern int raw_notifier_chain_register(struct raw_notifier_head *nh,
		struct notifier_block *nb);

extern int srcu_notifier_chain_register(struct srcu_notifier_head *nh,
		struct notifier_block *nb);
```

As I already wrote above, we will cover only blocking notification chains in the part, so let's consider implementation of the `blocking_notifier_chain_register` function. Implementation of this function is located in the [kernel/notifier.c](https://github.com/torvalds/linux/blob/master/kernel/notifier.c) source code file and as we may see the `blocking_notifier_chain_register` takes two parameters:

* `nh` - head of a notification chain;
* `nb` - notification descriptor.

Now let's look at the implementation of the `blocking_notifier_chain_register` function:

```C
int raw_notifier_chain_register(struct raw_notifier_head *nh,
		struct notifier_block *n)
{
	return notifier_chain_register(&nh->head, n);
}
```

As we may see it just returns result of the `notifier_chain_register` function from the same source code file and as we may understand this function does all job for us. Definition of the `notifier_chain_register` function looks:

```C
int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	if (unlikely(system_state == SYSTEM_BOOTING))
		return notifier_chain_register(&nh->head, n);

	down_write(&nh->rwsem);
	ret = notifier_chain_register(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
```

As we may see implementation of the `blocking_notifier_chain_register` is pretty simple. First of all there is check which check current system state and if a system in rebooting state we just call the `notifier_chain_register`. In other way we do the same call of the `notifier_chain_register` but as you may see this call is protected with read/write semaphores. Now let's look at the implementation of the `notifier_chain_register` function:

```C
static int notifier_chain_register(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	rcu_assign_pointer(*nl, n);
	return 0;
}
```

This function just inserts new `notifier_block` (given by a subsystem which wants to get notifications) to the notification chain list. Besides subscribing on an event, subscriber may unsubscribe from a certain events with the set of `unsubscribe` functions:

```C
extern int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
		struct notifier_block *nb);

extern int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
		struct notifier_block *nb);

extern int raw_notifier_chain_unregister(struct raw_notifier_head *nh,
		struct notifier_block *nb);

extern int srcu_notifier_chain_unregister(struct srcu_notifier_head *nh,
		struct notifier_block *nb);
```

When a producer of notifications wants to notify subscribers about an event, the `*.notifier_call_chain` function will be called. As you already may guess each type of notification chains provides own function to produce notification:

```C
extern int atomic_notifier_call_chain(struct atomic_notifier_head *nh,
		unsigned long val, void *v);

extern int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
		unsigned long val, void *v);

extern int raw_notifier_call_chain(struct raw_notifier_head *nh,
		unsigned long val, void *v);

extern int srcu_notifier_call_chain(struct srcu_notifier_head *nh,
		unsigned long val, void *v);
```

Let's consider implementation of the `blocking_notifier_call_chain` function. This function is defined in the [kernel/notifier.c](https://github.com/torvalds/linux/blob/master/kernel/notifier.c) source code file:

```C
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
		unsigned long val, void *v)
{
	return __blocking_notifier_call_chain(nh, val, v, -1, NULL);
}
```

and as we may see it just returns result of the `__blocking_notifier_call_chain` function. As we may see, the `blocking_notifer_call_chain` takes three parameters:

* `nh` - head of notification chain list;
* `val` - type of a notification;
* `v` -  input parameter which may be used by handlers.

But the `__blocking_notifier_call_chain` function takes five parameters:

```C
int __blocking_notifier_call_chain(struct blocking_notifier_head *nh,
				   unsigned long val, void *v,
				   int nr_to_call, int *nr_calls)
{
    ...
    ...
    ...
}
```

Where `nr_to_call` and `nr_calls` are number of notifier functions to be called and number of sent notifications. As you may guess the main goal of the `__blocking_notifer_call_chain` function and other functions for other notification types is to call callback function when an event occurred. Implementation of the `__blocking_notifier_call_chain` is pretty simple, it just calls the `notifier_call_chain` function from the same source code file protected with read/write semaphore:

```C
int __blocking_notifier_call_chain(struct blocking_notifier_head *nh,
				   unsigned long val, void *v,
				   int nr_to_call, int *nr_calls)
{
	int ret = NOTIFY_DONE;

	if (rcu_access_pointer(nh->head)) {
		down_read(&nh->rwsem);
		ret = notifier_call_chain(&nh->head, val, v, nr_to_call,
					nr_calls);
		up_read(&nh->rwsem);
	}
	return ret;
}
```

and returns its result. In this case all job is done by the `notifier_call_chain` function. Main purpose of this function informs registered notifiers about an asynchronous event:

```C
static int notifier_call_chain(struct notifier_block **nl,
			       unsigned long val, void *v,
			       int nr_to_call, int *nr_calls)
{
    ...
    ...
    ...
    ret = nb->notifier_call(nb, val, v);
    ...
    ...
    ...
    return ret;
}
```

That's all. In generall all looks pretty simple.

Now let's consider on a simple example related to [loadable modules](https://en.wikipedia.org/wiki/Loadable_kernel_module). If we will look in the [kernel/module.c](https://github.com/torvalds/linux/blob/master/kernel/module.c). As we already saw in this part, there is:

```C
static BLOCKING_NOTIFIER_HEAD(module_notify_list);
```

definition of the `module_notify_list` in the [kernel/module.c](https://github.com/torvalds/linux/blob/master/kernel/module.c) source code file. This definition determines head of list of blocking notifier chains related to kernel modules. There are at least three following events:

* MODULE_STATE_LIVE
* MODULE_STATE_COMING
* MODULE_STATE_GOING

in which maybe interested some subsystems of the Linux kernel. For example tracing of kernel modules states. Instead of direct call of the `atomic_notifier_chain_register`, `blocking_notifier_chain_register` and etc., most notification chains come with a set of wrappers used to register to them. Registatrion on these modules events is going with the help of such wrapper:

```C
int register_module_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&module_notify_list, nb);
}
```

If we will look in the [kernel/tracepoint.c](https://github.com/torvalds/linux/blob/master/kernel/tracepoint.c) source code file, we will see such registration during initialization of [tracepoints](https://www.kernel.org/doc/Documentation/trace/tracepoints.txt):

```C
static __init int init_tracepoints(void)
{
	int ret;

	ret = register_module_notifier(&tracepoint_module_nb);
	if (ret)
		pr_warn("Failed to register tracepoint module enter notifier\n");

	return ret;
}
```

Where `tracepoint_module_nb` provides callback function:

```C
static struct notifier_block tracepoint_module_nb = {
	.notifier_call = tracepoint_module_notify,
	.priority = 0,
};
```

When one of the `MODULE_STATE_LIVE`, `MODULE_STATE_COMING` or `MODULE_STATE_GOING` events occurred. For example the `MODULE_STATE_LIVE` the `MODULE_STATE_COMING` notifications will be sent during execution of the [init_module](http://man7.org/linux/man-pages/man2/init_module.2.html) [system call](https://0xax.gitbooks.io/linux-insides/content/SysCall/syscall-1.html). Or for example `MODULE_STATE_GOING` will be sent during execution of the [delete_module](http://man7.org/linux/man-pages/man2/delete_module.2.html) `system call`:

```C
SYSCALL_DEFINE2(delete_module, const char __user *, name_user,
		unsigned int, flags)
{
    ...
    ...
    ...
    blocking_notifier_call_chain(&module_notify_list,
				     MODULE_STATE_GOING, mod);
    ...
    ...
    ...
}
```

Thus when one of these system call will be called from userspace, the Linux kernel will send certain notification depends on a system call and the `tracepoint_module_notify` callback function will be called.

That's all.

## 4.3. 链接

* [C programming langauge](https://en.wikipedia.org/wiki/C_(programming_language))
* [API](https://en.wikipedia.org/wiki/Application_programming_interface)
* [callback](https://en.wikipedia.org/wiki/Callback_(computer_programming))
* [RCU](https://en.wikipedia.org/wiki/Read-copy-update)
* [spinlock](https://0xax.gitbooks.io/linux-insides/content/SyncPrim/sync-1.html)
* [loadable modules](https://en.wikipedia.org/wiki/Loadable_kernel_module)
* [semaphore](https://0xax.gitbooks.io/linux-insides/content/SyncPrim/sync-3.html)
* [tracepoints](https://www.kernel.org/doc/Documentation/trace/tracepoints.txt)
* [system call](https://0xax.gitbooks.io/linux-insides/content/SysCall/syscall-1.html)
* [init_module system call](http://man7.org/linux/man-pages/man2/init_module.2.html)
* [delete_module](http://man7.org/linux/man-pages/man2/delete_module.2.html)
* [previous part](https://0xax.gitbooks.io/linux-insides/content/Concepts/initcall.html)
