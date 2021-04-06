<center><font size='6'>Linux内核深入理解中断和异常（1）</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>


# 1. 中断介绍

内核中第一个子系统是[中断（interrupts）](http://en.wikipedia.org/wiki/Interrupt)。

## 1.1. 什么是中断？


我们已经在这本书的很多地方听到过 `中断（interrupts）` 这个词，也看到过很多关于中断的例子。在这一章中我们将会从下面的主题开始：

* 什么是 `中断（interrupts）` ？
* 什么是 `中断处理（interrupt handlers）` ？

我们将会继续深入探讨 `中断` 的细节和 Linux 内核如何处理这些中断。

所以，首先什么是中断？中断就是当软件或者硬件需要使用 CPU 时引发的 `事件（event）`。比如，当我们在键盘上按下一个键的时候，我们下一步期望做什么？操作系统和电脑应该怎么做？做一个简单的假设，每一个物理硬件都有一根连接 CPU 的中断线，设备可以通过它对 CPU 发起中断信号。但是中断信号并不是直接发送给 CPU。在老机器上中断信号发送给 [PIC](http://en.wikipedia.org/wiki/Programmable_Interrupt_Controller) ，它是一个顺序处理各种设备的各种中断请求的芯片。在新机器上，则是[高级程序中断控制器（Advanced Programmable Interrupt Controller）](https://en.wikipedia.org/wiki/Advanced_Programmable_Interrupt_Controller)做这件事情，即我们熟知的 `APIC`。一个 APIC 包括两个独立的设备：

* `Local APIC`
* `I/O APIC`

第一个设备 -  `Local APIC ` 存在于每个CPU核心中，Local APIC 负责处理特定于 CPU 的中断配置。Local APIC 常被用于管理来自 APIC 时钟（APIC-timer）、热敏元件和其他与 I/O 设备连接的设备的中断。

第二个设备 -  `I/O APIC` 提供了多核处理器的中断管理。它被用来在所有的 CPU 核心中分发外部中断。更多关于 local 和 I/O APIC 的内容将会在这一节的下面讲到。就如你所知道的，中断可以在任何时间发生。当一个中断发生时，操作系统必须立刻处理它。但是 `处理一个中断` 是什么意思呢？当一个中断发生时，操作系统必须确保下面的步骤顺序：

* 内核必须暂停执行当前进程(取代当前的任务)；
* 内核必须搜索中断处理程序并且转交控制权(执行中断处理程序)；
* 中断处理程序结束之后，被中断的进程能够恢复执行。

当然，在这个中断处理程序中会涉及到很多错综复杂的过程。但是上面 3 条是这个程序的基本骨架。

每个中断处理程序的地址都保存在一个特殊的位置，这个位置被称为 `中断描述符表（Interrupt Descriptor Table）` 或者 `IDT`。处理器使用一个唯一的数字来识别中断和异常的类型，这个数字被称为 `中断标识码（vector number）`。一个中断标识码就是一个 `IDT` 的标识。中断标识码范围是有限的，从 `0` 到 `255`。你可以在 Linux 内核源码中找到下面的中断标识码范围检查代码：

```C
BUG_ON((unsigned)n > 0xFF);
```

你可以在 Linux 内核源码中关于中断设置的地方找到这个检查(例如：`set_intr_gate`, `void set_system_intr_gate` 在 [arch/x86/include/asm/desc.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/desc.h)中)。

```c
/**
 * @n       中断号
 * @addr    中断/异常处理函数的基地址
 */
static __init void set_intr_gate(unsigned int n, const void *addr)
{
	struct idt_data data;

    /*  */
	init_idt_data(&data, n/* 中断号 */, addr/* 处理地址 */);

    //将中断门插入至 `IDT` 表中
	idt_setup_from_table(idt_table, &data, 1, false);
}
```

从 `0` 到 `31` 的 32 个中断标识码被处理器保留，用作处理架构定义的异常和中断。

你可以在 Linux 内核初始化程序的第二部分 - [早期中断和异常处理](https://rtoax.blog.csdn.net/article/details/114926850)中找到这个表和关于这些中断标识码的描述。

从 `32` 到 `255` 的中断标识码设计为用户定义中断并且不被系统保留。这些中断通常分配给外部 I/O 设备，使这些设备可以发送中断给处理器。

现在，我们来讨论中断的类型。笼统地来讲，我们可以把中断分为两个主要类型：

* 外部或者硬件引起的中断；
* 软件引起的中断（或者叫做“内部中断”）。

第一种类型 - **外部中断**，由 `Local APIC` 或者与 `Local APIC` 连接的处理器针脚接收。第二种类型 - **软件引起的中断**（或者叫做“内部中断”），由处理器自身的特殊情况引起(有时使用特殊架构的指令)。一个常见的关于特殊情况的例子就是 `除零`。另一个例子就是使用 `系统调用（syscall）` 退出程序。

就如之前提到过的，中断可以在任何时间因为超出代码和 CPU 控制的原因而发生。另一方面，异常和程序执行 `同步（synchronous）` ，并且可以被分为 3 类：

* `故障（Faults）`
* `陷入（Traps）`
* `终止（Aborts）`

`故障` 是在执行一个“不完善的”指令（可以在之后被修正）之前被报告的异常。如果发生了，它允许被中断的程序继续执行。

接下来的 `陷入` 是一个在执行了 `陷入` 指令后立刻被报告的异常。陷入同样允许被中断的程序继续执行，就像 `故障` 一样。

最后的 `终止` 是一个从不报告引起异常的精确指令的异常，并且不允许被中断的程序继续执行。

我们已经从前面的[部分](http://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-3.html)知道，中断可以分为 `可屏蔽的（maskable）` 和 `不可屏蔽的（non-maskable）`。可屏蔽的中断可以被阻塞，使用 `x86_64` 的指令 - `sti` 和 `cli`。我们可以在 Linux 内核代码中找到他们：

```C
static inline void native_irq_disable(void)
{
        asm volatile("cli": : :"memory");
}
```

和

```C
static inline void native_irq_enable(void)
{
        asm volatile("sti": : :"memory");
}
```

这两个指令修改了在中断寄存器中的 `IF` 标识位。 `sti` 指令设置 `IF` 标识，`cli` 指令清除这个标识。不可屏蔽的中断总是被报告。通常，**任何硬件上的失败都映射为不可屏蔽中断**。

如果多个异常或者中断同时发生，处理器以事先设定好的中断优先级处理他们。我们可以定义下面表中的从最低到最高的优先级：

```
+----------------------------------------------------------------+
|              |                                                 |
|   Priority   | Description                                     |
|              |                                                 |
+--------------+-------------------------------------------------+
|              | Hardware Reset and Machine Checks               |
|     1        | - RESET                                         |
|              | - Machine Check                                 |
+--------------+-------------------------------------------------+
|              | Trap on Task Switch                             |
|     2        | - T flag in TSS is set                          |
|              |                                                 |
+--------------+-------------------------------------------------+
|              | External Hardware Interventions                 |
|              | - FLUSH                                         |
|     3        | - STOPCLK                                       |
|              | - SMI                                           |
|              | - INIT                                          |
+--------------+-------------------------------------------------+
|              | Traps on the Previous Instruction               |
|     4        | - Breakpoints                                   |
|              | - Debug Trap Exceptions                         |
+--------------+-------------------------------------------------+
|     5        | Nonmaskable Interrupts                          |
+--------------+-------------------------------------------------+
|     6        | Maskable Hardware Interrupts                    |
+--------------+-------------------------------------------------+
|     7        | Code Breakpoint Fault                           |
+--------------+-------------------------------------------------+
|     8        | Faults from Fetching Next Instruction           |
|              | Code-Segment Limit Violation                    |
|              | Code Page Fault                                 |
+--------------+-------------------------------------------------+
|              | Faults from Decoding the Next Instruction       |
|              | Instruction length > 15 bytes                   |
|     9        | Invalid Opcode                                  |
|              | Coprocessor Not Available                       |
|              |                                                 |
+--------------+-------------------------------------------------+
|     10       | Faults on Executing an Instruction              |
|              | Overflow                                        |
|              | Bound error                                     |
|              | Invalid TSS                                     |
|              | Segment Not Present                             |
|              | Stack fault                                     |
|              | General Protection                              |
|              | Data Page Fault                                 |
|              | Alignment Check                                 |
|              | x87 FPU Floating-point exception                |
|              | SIMD floating-point exception                   |
|              | Virtualization exception                        |
+--------------+-------------------------------------------------+
```

现在我们了解了一些关于各种类型的中断和异常的内容，是时候转到更实用的部分了。我们从 `中断描述符表（IDT）` 开始。就如之前所提到的，`IDT` 保存了中断和异常处理程序的入口指针。

`IDT` 是一个类似于 `全局描述符表（Global Descriptor Table）`的结构，我们在[内核启动程序](http://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-2.html)的第二部分已经介绍过。但是他们确实有一些不同，`IDT` 的表项被称为 `门（gates）`，而不是 `描述符（descriptors）`。它可以包含下面的一种：

* 中断门（Interrupt gates）
* 任务门（Task gates）
* 陷阱门（Trap gates）

在 `x86` 架构中，只有 [长模式](http://en.wikipedia.org/wiki/Long_mode) 中断门和陷阱门可以在 `x86_64` 中引用。就像 `全局描述符表`，`中断描述符表` 在 `x86` 上是一个 8 字节数组门，而在 `x86_64` 上是一个 16 字节数组门。让我们回忆在[内核启动程序](http://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-2.html)的第二部分，`全局描述符表` 必须包含 `NULL` 描述符作为它的第一个元素。与 `全局描述符表` 不一样的是，`中断描述符表` 的第一个元素可以是一个门。它并不是强制要求的。比如，你可能还记得我们只是在早期的章节中过渡到[保护模式](http://en.wikipedia.org/wiki/Protected_mode)时用 `NULL` 门加载过中断描述符表：

```C
/*
 * Set up the IDT
 */
static void setup_idt(void)
{
	static const struct gdt_ptr null_idt = {0, 0};
	asm volatile("lidtl %0" : : "m" (null_idt));
}
```

在 [arch/x86/boot/pm.c](https://github.com/torvalds/linux/blob/master/arch/x86/boot/pm.c)中。`中断描述符表` 可以在线性地址空间和基址的任何地方被加载，只要在 `x86` 上以 8 字节对齐，在 `x86_64` 上以 16 字节对齐。`IDT` 的基址存储在一个特殊的寄存器 - IDTR。在 `x86` 上有两个指令 - 协调工作来修改 `IDTR` 寄存器： 

* `LIDT`
* `SIDT`

第一个指令 `LIDT` 用来加载 `IDT` 的基址，即在 `IDTR` 的指定操作数。第二个指令 `SIDT` 用来在指定操作数中读取和存储 `IDTR` 的内容。在 `x86` 上 `IDTR` 寄存器是 48 位，包含了下面的信息：

```
+-----------------------------------+----------------------+
|                                   |                      |
|     Base address of the IDT       |   Limit of the IDT   |
|                                   |                      |
+-----------------------------------+----------------------+
47                                16 15                    0
```

让我们看看 `setup_idt` 的实现，我们准备了一个 `null_idt`，并且使用 `lidt` 指令把它加载到 `IDTR` 寄存器。注意，`null_idt` 是 `gdt_ptr` 类型，后者定义如下：

```C
struct gdt_ptr {
        u16 len;
        u32 ptr;
} __attribute__((packed));
```

这里我们可以看看 `IDTR` 结构的定义，就像我们在示意图中看到的一样，由 2 字节和 4 字节（共 48 位）的两个域组成。现在，让我们看看 `IDT` 入口结构体，它是一个在 `x86` 中被称为门的 16 字节数组。它拥有下面的结构：

```
127                                                                             96
+-------------------------------------------------------------------------------+
|                                                                               |
|                                Reserved                                       |
|                                                                               |
+--------------------------------------------------------------------------------
95                                                                              64
+-------------------------------------------------------------------------------+
|                                                                               |
|                               Offset 63..32                                   |
|                                                                               |
+-------------------------------------------------------------------------------+
63                               48 47      46  44   42    39             34    32
+-------------------------------------------------------------------------------+
|                                  |       |  D  |   |     |      |   |   |     |
|       Offset 31..16              |   P   |  P  | 0 |Type |0 0 0 | 0 | 0 | IST |
|                                  |       |  L  |   |     |      |   |   |     |
 -------------------------------------------------------------------------------+
31                                   16 15                                      0
+-------------------------------------------------------------------------------+
|                                      |                                        |
|          Segment Selector            |                 Offset 15..0           |
|                                      |                                        |
+-------------------------------------------------------------------------------+
```

为了把索引格式化成 IDT 的格式，处理器把异常和中断向量分为 16 个级别。处理器处理异常和中断的发生就像它看到 `call` 指令时处理一个程序调用一样。处理器使用中断或异常的唯一的数字或 `中断标识码` 作为索引来寻找对应的 `中断描述符表` 的条目。现在让我们更近距离地看看 `IDT` 条目。

就像我们所看到的一样，在表中的 `IDT` 条目由下面的域组成：

* `0-15` bits  - 段选择器偏移，处理器用它作为中断处理程序的入口指针基址；
* `16-31` bits - 段选择器基址，包含中断处理程序入口指针；
* `IST` - 在 `x86_64` 上的一个新的机制，下面我们会介绍它；
* `DPL` - 描述符特权级；
* `P` - 段存在标志；
* `48-63` bits - 中断处理程序基址的第二部分；
* `64-95` bits - 中断处理程序基址的第三部分；
* `96-127` bits - CPU 保留位.

`Type` 域描述了 `IDT` 条目的类型。有三种不同的中断处理程序：

* 中断门（Interrupt gate）
* 陷入门（Trap gate）
* 任务门（Task gate）

上述结构体定义为：

```c
struct idt_bits {/* 中断 索引 */
	u16		ist	: 3,    //* `IST` - 中断堆栈表；`Interrupt Stack Table` 是 `x86_64` 中的新机制
	                    //代替传统的栈切换机制
			zero	: 5,
			type	: 5,    // `Type` 域描述了这一项的类型
                            //* 任务描述符
                            //* 中断描述符
                            //* 陷阱描述符
			dpl	: 2,    //* `DPL` - 描述符权限级别；
			p	: 1;    //* `P` - 当前段标志；
} __attribute__((packed));

struct gate_struct {/* 门 */
	u16		offset_low; //* `Offset` - 处理程序入口点的偏移量；
	u16		segment;    //* `Selector` - 目标代码段的段选择子；
	struct idt_bits	bits;
	u16		offset_middle;
#ifdef CONFIG_X86_64
	u32		offset_high;
	u32		reserved;
#endif
} __attribute__((packed));
```

`IST` 或者说是 `Interrupt Stack Table` 是 `x86_64` 中的新机制，它用来代替传统的栈切换机制。之前的 `x86` 架构提供的机制可以在响应中断时自动切换栈帧。`IST` 是 `x86` 栈切换模式的一个修改版，在它使能之后可以无条件地切换栈，并且可以被任何与确定中断（我们将在下面介绍它）关联的 `IDT` 条目中的中断使能。从这里可以看出，`IST` 并不是所有的中断必须的，一些中断可以继续使用传统的栈切换模式。`IST` 机制在[任务状态段（Task State Segment）](http://en.wikipedia.org/wiki/Task_state_segment)或者 `TSS` 中提供了 7 个 `IST` 指针。`TSS` 是一个包含进程信息的特殊结构，用来在执行中断或者处理 Linux 内核异常的时候做栈切换。每一个指针都被 `IDT` 中的中断门引用。

`中断描述符表` 使用 `gate_desc` 的数组描述：

```C
extern gate_desc idt_table[];
```

`gate_desc` 定义如下：

```C
#ifdef CONFIG_X86_64
...
...
...
typedef struct gate_struct gate_desc;
...
...
...
#endif
```

`gate_struct` 定义如下：

```C
struct idt_bits {/* 中断 索引 */
	u16		ist	: 3,    //* `IST` - 中断堆栈表；`Interrupt Stack Table` 是 `x86_64` 中的新机制
	                    //代替传统的栈切换机制
			zero	: 5,
			type	: 5,    // `Type` 域描述了这一项的类型
                            //* 任务描述符
                            //* 中断描述符
                            //* 陷阱描述符
			dpl	: 2,    //* `DPL` - 描述符权限级别；
			p	: 1;    //* `P` - 当前段标志；
} __attribute__((packed));

struct gate_struct {/* 门 */
	u16		offset_low; //* `Offset` - 处理程序入口点的偏移量；
	u16		segment;    //* `Selector` - 目标代码段的段选择子；
	struct idt_bits	bits;
	u16		offset_middle;
#ifdef CONFIG_X86_64
	u32		offset_high;
	u32		reserved;
#endif
} __attribute__((packed));
```

在 `x86_64` 架构中，每一个活动的线程在 Linux 内核中都有一个很大的栈。这个栈的大小由 `THREAD_SIZE` 定义，而且与下面的定义相等：

```C
#define PAGE_SHIFT      12
#define PAGE_SIZE       (_AC(1,UL) << PAGE_SHIFT)
...
...
...
#define THREAD_SIZE_ORDER       (2 + KASAN_STACK_ORDER)
#define THREAD_SIZE  (PAGE_SIZE << THREAD_SIZE_ORDER)
```

`PAGE_SIZE` 是 `4096` 字节，`THREAD_SIZE_ORDER` 的值依赖于 `KASAN_STACK_ORDER`。就像我们看到的，`KASAN_STACK` 依赖于 `CONFIG_KASAN` 内核配置参数，它定义如下：

```C
#ifdef CONFIG_KASAN
    #define KASAN_STACK_ORDER 1
#else
    #define KASAN_STACK_ORDER 0
#endif
```

`KASan` 是一个运行时内存[调试器](http://lwn.net/Articles/618180/)。所以，如果 `CONFIG_KASAN` 被禁用，`THREAD_SIZE` 是 `16384` ；如果内核配置选项打开，`THREAD_SIZE` 的值是 `32768`。这块栈空间保存着有用的数据，只要线程是活动状态或者僵尸状态。但是当线程在用户空间的时候，这个内核栈是空的，除非 `thread_info` 结构（关于这个结构的详细信息在 Linux 内核初始程序的第四[部分](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-4.html)）在这个栈空间的底部。活动的或者僵尸线程并不是在他们栈中的唯一的线程，与每一个 CPU 关联的特殊栈也存在于这个空间。当内核在这个 CPU 上执行代码的时候，这些栈处于活动状态；当在这个 CPU 上执行用户空间代码时，这些栈不包含任何有用的信息。每一个 CPU 也有一个特殊的 per-cpu 栈。首先是给外部中断使用的 `中断栈（interrupt stack）`。它的大小定义如下： 

```C
#define IRQ_STACK_ORDER (2 + KASAN_STACK_ORDER)
#define IRQ_STACK_SIZE (PAGE_SIZE << IRQ_STACK_ORDER)
```

或者是 `16384` 字节。Per-cpu 的中断栈在 `x86_64` 架构中使用 `irq_stack_union` 联合描述:

```C
union irq_stack_union {
	char irq_stack[IRQ_STACK_SIZE];

    struct {
		char gs_base[40];
		unsigned long stack_canary;
	};
};
```
在5.10.13中是单独这样定义的：
```c
/* Per CPU interrupt stacks  `中断栈`*/
struct irq_stack {
	char		stack[IRQ_STACK_SIZE];  /* 32KB */
} __aligned(IRQ_STACK_SIZE);

struct fixed_percpu_data {
	/*
	 * GCC hardcodes the stack canary as %gs:40.  Since the
	 * irq_stack is the object at %gs:0, we reserve the bottom
	 * 48 bytes of the irq stack for the canary.
	 */
	char		gs_base[40];
	unsigned long	stack_canary;   /* 金丝雀 */
};
```
第一个 `irq_stack` 域是一个 16KB 的数组。然后你可以看到 `irq_stack_union` 联合包含了一个结构体，这个结构体有两个域：

* `gs_base` - 总是指向 `irqstack` 联合底部的 `gs` 寄存器。在 `x86_64` 中， per-cpu（更多关于 `per-cpu` 变量的信息可以阅读特定的[章节](http://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html)） 和 stack canary 共享 `gs` 寄存器。所有的 per-cpu 标志初始值为零，并且 `gs` 指向 per-cpu 区域的开始。你已经知道[段内存模式](http://en.wikipedia.org/wiki/Memory_segmentation)已经废除很长时间了，但是我们可以使用[特殊模块寄存器（Model specific registers）](http://en.wikipedia.org/wiki/Model-specific_register)给这两个段寄存器 - `fs` 和 `gs` 设置基址，并且这些寄存器仍然可以被用作地址寄存器。如果你记得 Linux 内核初始程序的第一[部分](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-1.html)，你会记起我们设置了 `gs` 寄存器：

```assembly
	movl	$MSR_GS_BASE,%ecx
	movl	initial_gs(%rip),%eax
	movl	initial_gs+4(%rip),%edx
	wrmsr
```

`initial_gs` 指向 `irq_stack_union`:

```assembly
GLOBAL(initial_gs)
.quad	INIT_PER_CPU_VAR(irq_stack_union)
```

* `stack_canary` - [Stack canary](http://en.wikipedia.org/wiki/Stack_buffer_overflow#Stack_canaries) 对于中断栈来说是一个用来验证栈是否已经被修改的 `栈保护者（stack protector）`。`gs_base` 是一个 40 字节的数组，`GCC` 要求 stack canary 在被修正过的偏移量上，并且 `gs` 的值在 `x86_64` 架构上必须是 `40`，在 `x86` 架构上必须是 `20`。 

`irq_stack_union` 是 `percpu` 的第一个数据, 我们可以在 `System.map`中看到它：

```
0000000000000000 D __per_cpu_start
0000000000000000 D irq_stack_union
0000000000004000 d exception_stacks
0000000000009000 D gdt_page
...
...
...
```
在5.10.13中是：
```
[rongtao@localhost src]$ grep -n __per_cpu_start System.map -A 3
1:0000000000000000 D __per_cpu_start
2-0000000000000000 D fixed_percpu_data
3-00000000000001d9 A kexec_control_code_size
4-0000000000001000 D cpu_debug_store
```


我们可以看到它在代码中的定义:

```C
DECLARE_PER_CPU_FIRST(union irq_stack_union, irq_stack_union) __visible;
```

现在，是时候来看 `irq_stack_union` 的初始化过程了。除了 `irq_stack_union` 的定义，我们可以在[arch/x86/include/asm/processor.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/processor.h)中查看下面的 per-cpu 变量

```C
DECLARE_PER_CPU(char *, irq_stack_ptr);
DECLARE_PER_CPU(unsigned int, irq_count);
```

第一个就是 `irq_stack_ptr`。从这个变量的名字中可以知道，它显然是一个指向这个栈顶的指针。第二个 `irq_count` 用来检查 CPU 是否已经在中断栈。`irq_stack_ptr` 的初始化在[arch/x86/kernel/setup_percpu.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup_percpu.c)的 `setup_per_cpu_areas` 函数中：

```C
void __init setup_per_cpu_areas(void)
{
...
...
#ifdef CONFIG_X86_64
for_each_possible_cpu(cpu) {
    ...
    ...
    ...
    per_cpu(irq_stack_ptr, cpu) =
            per_cpu(irq_stack_union.irq_stack, cpu) +
            IRQ_STACK_SIZE - 64;
    ...
    ...
    ...
#endif
...
...
}
```

现在，我们一个一个查看所有 CPU，并且设置 `irq_stack_ptr`。事实证明它等于中断栈的顶减去 `64`。为什么是 `64`？TODO [[arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/cpu/common.c)] 代码如下：

```C
void load_percpu_segment(int cpu)
{
        ...
        ...
        ...
        loadsegment(gs, 0);
        wrmsrl(MSR_GS_BASE, (unsigned long)per_cpu(irq_stack_union.gs_base, cpu));
}
```

就像我们所知道的一样，`gs` 寄存器指向中断栈的栈底：

```assembly
	movl	$MSR_GS_BASE,%ecx
	movl	initial_gs(%rip),%eax
	movl	initial_gs+4(%rip),%edx
	wrmsr

	GLOBAL(initial_gs)
	.quad	INIT_PER_CPU_VAR(irq_stack_union)
```

现在我们可以看到 `wrmsr` 指令，这个指令从 `edx:eax` 加载数据到 被 `ecx` 指向的[MSR寄存器]((http://en.wikipedia.org/wiki/Model-specific_register))。在这里MSR寄存器是 `MSR_GS_BASE`，它保存了被 `gs` 寄存器指向的内存段的基址。`edx:eax` 指向 `initial_gs` 的地址，它就是 `irq_stack_union`的基址。

我们还知道，`x86_64` 有一个叫 `中断栈表（Interrupt Stack Table）` 或者 `IST` 的组件，当发生不可屏蔽中断、双重错误等等的时候，这个组件提供了切换到新栈的功能。这可以到达7个 `IST` per-cpu 入口。其中一些如下;

* `DOUBLEFAULT_STACK`
* `NMI_STACK`
* `DEBUG_STACK`
* `MCE_STACK`

或者

```C
#define DOUBLEFAULT_STACK 1
#define NMI_STACK 2
#define DEBUG_STACK 3
#define MCE_STACK 4
```

所有被 `IST` 切换到新栈的中断门描述符都由 `set_intr_gate_ist` 函数初始化。例如:

```C
set_intr_gate_ist(X86_TRAP_NMI, &nmi, NMI_STACK);
...
...
...
set_intr_gate_ist(X86_TRAP_DF, &double_fault, DOUBLEFAULT_STACK);
```

5.10.13中只有：
```c
/**
 * @n       中断号
 * @addr    中断/异常处理函数的基地址
 */
static __init void set_intr_gate(unsigned int n, const void *addr)
{
	struct idt_data data;

    /*  */
	init_idt_data(&data, n/* 中断号 */, addr/* 处理地址 */);

    //将中断门插入至 `IDT` 表中
	idt_setup_from_table(idt_table, &data, 1, false);
}
```

其中 `&nmi` 和 `&double_fault` 是中断函数的入口地址：

```C
asmlinkage void nmi(void);
asmlinkage void double_fault(void);
```

定义在 [arch/x86/kernel/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/entry_64.S)中

```assembly
idtentry double_fault do_double_fault has_error_code=1 paranoid=2
...
...
...
ENTRY(nmi)
...
...
...
END(nmi)
```
我找到了：

```asm
/*
 * Runs on exception stack.  Xen PV does not go through this path at all,
 * so we can use real assembly here.
 *
 * Registers:
 *	%r14: Used to save/restore the CR3 of the interrupted context
 *	      when PAGE_TABLE_ISOLATION is in use.  Do not clobber.
 */
SYM_CODE_START(asm_exc_nmi)
    ...
	iretq
SYM_CODE_END(asm_exc_nmi)
```


当一个中断或者异常发生时，新的 `ss` 选择器被强制置为 `NULL`，并且 `ss` 选择器的 `rpl` 域被设置为新的 `cpl`。旧的 `ss`、`rsp`、寄存器标志、`cs`、`rip` 被压入新栈。在 64 位模型下，中断栈帧大小固定为 8 字节，所以我们可以得到下面的栈:

```
+---------------+
|               |
|      SS       | 40
|      RSP      | 32
|     RFLAGS    | 24
|      CS       | 16
|      RIP      | 8
|   Error code  | 0
|               |
+---------------+
```
如代码所示：

```asm
	pushq	5*8(%rdx)	/* pt_regs->ss */
	pushq	4*8(%rdx)	/* pt_regs->rsp */
	pushq	3*8(%rdx)	/* pt_regs->flags */
	pushq	2*8(%rdx)	/* pt_regs->cs */
	pushq	1*8(%rdx)	/* pt_regs->rip */
```

如果在中断门中 `IST` 域不是 `0`，我们把 `IST` 读到 `rsp` 中。如果它关联了一个中断向量错误码，我们再把这个错误码压入栈。如果中断向量没有错误码，就继续并且把虚拟错误码压入栈。我们必须做以上的步骤以确保栈一致性。接下来我们从门描述符中加载段选择器域到 CS 寄存器中，并且通过验证第 `21` 位的值来验证目标代码是一个 64 位代码段，例如 `L` 位在 `全局描述符表（Global Descriptor Table）`。最后我们从门描述符中加载偏移域到 `rip` 中，`rip` 是中断处理函数的入口指针。然后中断函数开始执行，在中断函数执行结束后，它必须通过 `iret` 指令把控制权交还给被中断进程。`iret` 指令无条件地弹出栈指针（`ss:rsp`）来恢复被中断的进程，并且不会依赖于 `cpl` 改变。

这就是中断的所有过程。

显然需要再很仔细的研读内核源码的nmi部分。

## 1.2. 总结


关于 Linux 内核的中断和中断处理的第一部分至此结束。我们初步了解了一些理论和与中断和异常相关的初始化条件。在下一部分，我会接着深入了解中断和中断处理 - 更深入了解她真实的样子。

如果你有任何问题或建议，请给我发评论或者给我发 [Twitter](https://twitter.com/0xAX)。

**请注意英语并不是我的母语，我为任何表达不清楚的地方感到抱歉。如果你发现任何错误请发 PR 到 [linux-insides](https://github.com/MintCN/linux-insides-zh)。(译者注：翻译问题请发 PR 到 [linux-insides-cn](https://www.gitbook.com/book/xinqiu/linux-insides-cn))**


## 1.3. 链接


* [PIC](http://en.wikipedia.org/wiki/Programmable_Interrupt_Controller)
* [Advanced Programmable Interrupt Controller](https://en.wikipedia.org/wiki/Advanced_Programmable_Interrupt_Controller)
* [protected mode](http://en.wikipedia.org/wiki/Protected_mode)
* [long mode](http://en.wikipedia.org/wiki/Long_mode)
* [kernel stacks](https://www.kernel.org/doc/Documentation/x86/x86_64/kernel-stacks)
* [Task State Segement](http://en.wikipedia.org/wiki/Task_state_segment)
* [segmented memory model](http://en.wikipedia.org/wiki/Memory_segmentation)
* [Model specific registers](http://en.wikipedia.org/wiki/Model-specific_register)
* [Stack canary](http://en.wikipedia.org/wiki/Stack_buffer_overflow#Stack_canaries)
* [Previous chapter](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/index.html)



# 2. 深入Linux内核中的中断和异常处理

在 [上一章节](http://0xax.gitbooks.io/linux-insides/content/interrupts/interrupts-1.html)中我们学习了中断和异常处理的一些理论知识，在本章节中，我们将深入了解Linux内核源代码中关于中断与异常处理的部分。之前的章节中主要从理论方面描述了Linux中断和异常处理的相关内容，而在本章节中，我们将直接深入Linux源代码来了解相关内容。像其他章节一样，我们将从启动早期的代码开始阅读。本章将不会像 [Linux内核启动过程](http://0xax.gitbooks.io/linux-insides/content/Booting/index.html)中那样从Linux内核启动的 [最开始](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/header.S#L292)几行代码读起，而是从与中断与异常处理相关的最早期代码开始阅读，了解Linux内核源代码中所有与中断和异常处理相关的代码。

如果你读过本书的前面部分，你可能记得Linux内核中关于 `x86_64`架构的代码中与中断相关的最早期代码出现在 [arch/x86/boot/pm.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/pm.c)文件中，该文件首次配置了 [中断描述符表](http://en.wikipedia.org/wiki/Interrupt_descriptor_table)(IDT)。对IDT的配置在`go_to_protected_mode`函数中完成，该函数首先调用了 `setup_idt`函数配置了IDT，然后将处理器的工作模式切换为 [保护模式](http://en.wikipedia.org/wiki/Protected_mode):

```C
void go_to_protected_mode(void)
{
    ...
    setup_idt();    //设置中断描述符表（IDT）
    setup_gdt();    //设置全局描述符表
    ...
}
```

`setup_idt`函数在同一文件中定义，它仅仅是用 `NULL`填充了中断描述符表:

```C
static void setup_idt(void)
{
    static const struct gdt_ptr null_idt = {0, 0};
    asm volatile("lidtl %0" : : "m" (null_idt));
}
```

其中，`gdt_ptr`表示了一个48-bit的特殊功能寄存器 `GDTR`，其包含了全局描述符表 `Global Descriptor`的基地址:

```C
/*
 * Set up the GDT
 */
//+-----------------------------------+----------------------+
//|                                   |                      |
//|     Base address of the IDT       |   Limit of the IDT   |
//|                                   |                      |
//+-----------------------------------+----------------------+
//47                                16 15                    0
struct gdt_ptr {    /* 描述符表 -> GDTR 寄存器是 48 bit 长度的*/
	u16 len;
	u32 ptr;
} __attribute__((packed));
```

显然，在此处的 `gdt_prt`不是代表 `GDTR`寄存器而是代表 `IDTR`寄存器，因为我们将其设置到了中断描述符表中。之所以在Linux内核代码中没有`idt_ptr`结构体，是因为其与`gdt_prt`具有相同的结构而仅仅是名字不同，因此没必要定义两个重复的数据结构。可以看到，内核在此处并没有填充`Interrupt Descriptor Table`，这是因为此刻处理任何中断或异常还为时尚早，因此我们仅仅以`NULL`来填充`IDT`。

GDT初始化：
```c
static void setup_gdt(void)
{
	/* There are machines which are known to not boot with the GDT
	   being 8-byte unaligned.  Intel recommends 16 byte alignment. */
	   //用于代码，数据和TSS（任务状态段）
	static const u64 boot_gdt[] __attribute__((aligned(16)))/* 16 字节对齐,共占用了 48 字节 */ = {
		/* CS: code, read/execute, 4 GB, base 0 */
		[GDT_ENTRY_BOOT_CS] = GDT_ENTRY(0xc09b, 0, 0xfffff),
		/* DS: data, read/write, 4 GB, base 0 */
		[GDT_ENTRY_BOOT_DS] = GDT_ENTRY(0xc093, 0, 0xfffff),
		/* TSS: 32-bit tss, 104 bytes, base 4096 */
		/* We only have a TSS here to keep Intel VT happy;
		   we don't actually use it for anything. */
		[GDT_ENTRY_BOOT_TSS] = GDT_ENTRY(0x0089, 4096, 103),
	};
	/* Xen HVM incorrectly stores a pointer to the gdt_ptr, instead
	   of the gdt_ptr contents.  Thus, make it static so it will
	   stay in memory, at least long enough that we switch to the
	   proper kernel GDT. */
	static struct gdt_ptr gdt;

    //GDT的长度为
	gdt.len = sizeof(boot_gdt)-1;

    //获得一个指向GDT的指针
    //获取boot_gdt的地址，并将其添加到左移4位的数据段的地址中
    //（请记住，我们现在处于实模式）
    //因为我们还在实模式，所以就是 （ ds << 4 + 数组起始地址）
	gdt.ptr = (u32)&boot_gdt + (ds() << 4);

    //执行lgdtl指令以将GDT加载到GDTR寄存器中
	asm volatile("lgdtl %0" : : "m" (gdt));
}
```
此处填充了结构`boot_gdt`。

在设置完 [Interrupt descriptor table](http://en.wikipedia.org/wiki/Interrupt_descriptor_table), [Global Descriptor Table](http://en.wikipedia.org/wiki/GDT)和其他一些东西以后，内核开始进入保护模式，这部分代码在 [arch/x86/boot/pmjump.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/pmjump.S)中实现，你可以在描述如何进入保护模式的 [章节](http://0xax.gitbooks.io/linux-insides/content/Booting/linux-bootstrap-3.html)中了解到更多细节。

在最早的章节中我们已经了解到进入保护模式的代码位于 `boot_params.hdr.code32_start`，你可以在 [arch/x86/boot/pm.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/pm.c)的末尾看到内核将入口函数指针和启动参数 `boot_params`传递给了 `protected_mode_jump`函数:

```C
protected_mode_jump(boot_params.hdr.code32_start,
                            (u32)&boot_params + (ds() << 4));
```

定义在文件 [arch/x86/boot/pmjump.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/pmjump.S)中的函数`protected_mode_jump`通过一种[8086](http://en.wikipedia.org/wiki/Intel_8086)的调用 [约定](http://en.wikipedia.org/wiki/X86_calling_conventions#List_of_x86_calling_conventions)，通过 `ax`和 `dx`两个寄存器来获取参数:

```assembly
GLOBAL(protected_mode_jump)
        ...
        ...
        ...
        .byte   0x66, 0xea              # ljmpl opcode
2:      .long   in_pm32                 # offset
        .word   __BOOT_CS               # segment
...
...
...
ENDPROC(protected_mode_jump)
```

其中 `in_pm32`包含了对32-bit入口的跳转语句:

```assembly
GLOBAL(in_pm32)
        ...
        ...
        jmpl    *%eax // %eax contains address of the `startup_32`
        ...
        ...
ENDPROC(in_pm32)
```

你可能还记得32-bit的入口地址位于汇编文件 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S)中，尽管它的名字包含 `_64`后缀。我们可以在 `arch/x86/boot/compressed`目录下看到两个相似的文件:

* `arch/x86/boot/compressed/head_32.S`.
* `arch/x86/boot/compressed/head_64.S`;

然而32-bit模式的入口位于第二个文件中，而第一个文件在 `x86_64`配置下不会参与编译。如 [arch/x86/boot/compressed/Makefile](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/Makefile):

```
vmlinux-objs-y := $(obj)/vmlinux.lds $(obj)/head_$(BITS).o $(obj)/misc.o \
...
...
```

代码中的 `head_*`取决于 `$(BITS)` 变量的值，而该值由"架构"决定。我们可以在 [arch/x86/Makefile](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/Makefile)找到相关信息:

```
ifeq ($(CONFIG_X86_32),y)
...
        BITS := 32
else
        BITS := 64
        ...
endif
```

现在我们从 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S)跳入了 `startup_32`函数，在这个函数中没有与中断处理相关的内容。`startup_32`函数包含了进入 [long mode](http://en.wikipedia.org/wiki/Long_mode)之前必须的准备工作，并直接进入了 `long mode`。 

`long mode`的入口位于 `startup_64`函数中，在这个函数中完成了 [内核解压](http://0xax.gitbooks.io/linux-insides/content/Booting/linux-bootstrap-5.html)的准备工作。内核解压的代码位于 [arch/x86/boot/compressed/misc.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/misc.c)中的 `decompress_kernel`函数中。在5.10.13中已经更名为：
```c
/*
 * The compressed kernel image (ZO), has been moved so that its position
 * is against the end of the buffer used to hold the uncompressed kernel
 * image (VO) and the execution environment (.bss, .brk), which makes sure
 * there is room to do the in-place decompression. (See header.S for the
 * calculations.)
 *
 *                             |-----compressed kernel image------|
 *                             V                                  V
 * 0                       extract_offset                      +INIT_SIZE
 * |-----------|---------------|-------------------------|--------|
 *             |               |                         |        |
 *           VO__text      startup_32 of ZO          VO__end    ZO__end
 *             ^                                         ^
 *             |-------uncompressed kernel image---------|
 *
 */
asmlinkage __visible void *extract_kernel(void *rmode, memptr heap,
				  unsigned char *input_data,
				  unsigned long input_len,
				  unsigned char *output,
				  unsigned long output_len);
```

内核解压完成以后，程序跳入 [arch/x86/kernel/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/head_64.S)中的 `startup_64`函数。在这个函数中，我们开始构建 `identity-mapped pages`，并在之后检查 [NX](http://en.wikipedia.org/wiki/NX_bit)位，配置 `Extended Feature Enable Register`(见链接)，使用 `lgdt`指令更新早期的`Global Descriptor Table`，在此之后我们还需要使用如下代码来设置 `gs`寄存器:

```assembly
movl    $MSR_GS_BASE,%ecx
movl    initial_gs(%rip),%eax
movl    initial_gs+4(%rip),%edx
wrmsr
```

这段代码在之前的 [章节](http://0xax.gitbooks.io/linux-insides/content/interrupts/interrupts-1.html)中也出现过。请注意代码最后的 `wrmsr`指令，这个指令将 `edx:eax`寄存器指定的地址中的数据写入到由 `ecx`寄存器指定的 [model specific register](http://en.wikipedia.org/wiki/Model-specific_register)中。由代码可以看到，`ecx`中的值是 `$MSR_GS_BASE`，该值在 [arch/x86/include/uapi/asm/msr-index.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/uapi/asm/msr-index.h)中定义:

```C
#define MSR_GS_BASE             0xc0000101
```

由此可见，`MSR_GS_BASE`定义了 `model specific register`的编号。由于 `cs`, `ds`, `es`,和 `ss`在64-bit模式中不再使用，这些寄存器中的值将会被忽略，但我们可以通过 `fs`和 `gs`寄存器来访问内存空间。`model specific register`提供了一种后门 `back door`来访问这些段寄存器，也让我们可以通过段寄存器 `fs`和 `gs`来访问64-bit的基地址。看起来这部分代码映射在 `GS.base`域中。再看到 `initial_gs`函数的定义:

```assembly
GLOBAL(initial_gs)
        .quad   INIT_PER_CPU_VAR(irq_stack_union)
```

5.10.13中为：
```asm
SYM_DATA(initial_gs,	.quad INIT_PER_CPU_VAR(fixed_percpu_data))
```

这段代码将 `irq_stack_union`传递给 `INIT_PER_CPU_VAR`宏，后者只是给输入参数添加了 `init_per_cpu__`前缀而已。在此得出了符号 `init_per_cpu__irq_stack_union`。再看到 [链接脚本](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/vmlinux.lds.S)，其中可以看到如下定义:

```
#define INIT_PER_CPU(x) init_per_cpu__##x = x + __per_cpu_load
INIT_PER_CPU(irq_stack_union);
```

这段代码告诉我们符号 `init_per_cpu__irq_stack_union`的地址将会是 `irq_stack_union + __per_cpu_load`。现在再来看看 `init_per_cpu__irq_stack_union`和 `__per_cpu_load`在哪里。`irq_stack_union`的定义出现在 [arch/x86/include/asm/processor.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/processor.h)中，其中的 `DECLARE_INIT_PER_CPU`宏展开后又调用了 `init_per_cpu_var`宏:

```C
DECLARE_INIT_PER_CPU(irq_stack_union);

#define DECLARE_INIT_PER_CPU(var) \
       extern typeof(per_cpu_var(var)) init_per_cpu_var(var)

#define init_per_cpu_var(var)  init_per_cpu__##var
```

将所有的宏展开之后我们可以得到与之前相同的名称 `init_per_cpu__irq_stack_union`，但此时它不再只是一个符号，而成了一个变量。请注意表达式 `typeof(per_cpu_var(var))`,在此时 `var`是 `irq_stack_union`，而 `per_cpu_var`宏在 [arch/x86/include/asm/percpu.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/percpu.h)中定义:

```C
#define PER_CPU_VAR(var)        %__percpu_seg:var
```

其中:

```C
#ifdef CONFIG_X86_64
    #define __percpu_seg gs
endif
```

因此，我们实际访问的是 `gs:irq_stack_union`，它的类型是 `irq_union`。到此为止，我们定义了上面所说的第一个变量并且知道了它的地址。再看到第二个符号 `__per_cpu_load`，该符号定义在 [include/asm-generic/sections.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/asm-generic-sections.h)，这个符号定义了一系列 `per-cpu`变量:

```C
extern char __per_cpu_load[], __per_cpu_start[], __per_cpu_end[];
```

同时，符号代表了这一系列变量的数据区域的基地址。因此我们知道了 `irq_stack_union`和 `__per_cpu_load`的地址，并且知道变量 `init_per_cpu__irq_stack_union`位于 `__per_cpu_load`。并且看到 [System.map](http://en.wikipedia.org/wiki/System.map):

```
...
...
...
ffffffff819ed000 D __init_begin
ffffffff819ed000 D __per_cpu_load
ffffffff819ed000 A init_per_cpu__irq_stack_union
...
...
...
```


这在5.10.13中大相径庭，
```c
[rongtao@localhost src]$ grep -n __init_begin System.map -A 10
103854:ffffffff83bdc000 D __init_begin
103855-ffffffff83bdc000 D __per_cpu_load
103856-ffffffff83bdc000 A init_per_cpu__fixed_percpu_data
103857-ffffffff83bde000 A init_per_cpu__irq_stack_backing_store
103858-ffffffff83beb000 A init_per_cpu__gdt_page
103859-ffffffff83c12000 T _sinittext
```

现在我们终于知道了 `initial_gs`是什么，回到之前的代码中:

```assembly
movl    $MSR_GS_BASE,%ecx
movl    initial_gs(%rip),%eax
movl    initial_gs+4(%rip),%edx
wrmsr
```

此时我们通过 `MSR_GS_BASE`指定了一个平台相关寄存器，然后将 `initial_gs`的64-bit地址放到了 `edx:eax`段寄存器中，然后执行 `wrmsr`指令，将 `init_per_cpu__irq_stack_union`的基地址放入了 `gs`寄存器，而这个地址将是中断栈的栈底地址。

在此之后我们将进入 `x86_64_start_kernel`函数的C语言代码中，此函数定义在 [arch/x86/kernel/head64.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/head64.c)。在这个函数中，我们将完成最后的准备工作，之后就要进入到与平台无关的通用内核代码。

如果你读过前文的 [早期中断和异常处理](http://0xax.gitbooks.io/linux-insides/content/Initialization/linux-initialization-2.html)章节，你可能记得其中之一的工作就是将中断服务程序入口地址填写到早期 `Interrupt Descriptor Table`中。

```C
for (i = 0; i < NUM_EXCEPTION_VECTORS; i++)
        set_intr_gate(i, early_idt_handlers[i]);

load_idt((const struct desc_ptr *)&idt_descr);
```

当我写 `早期中断和异常处理`章节时Linux内核版本是 `3.18`，而如今Linux内核版本已经生长到了 `4.1.0-rc6+`，并且 `Andy Lutomirski`提交了一个与 `early_idt_handlers`相关的修改 [patch](https://lkml.org/lkml/2015/6/2/106)，该修改即将并入内核代码主线中。**NOTE**在我写这一段时，这个 [patch](https://github.com/torvalds/linux/commit/425be5679fd292a3c36cb1fe423086708a99f11a)已经进入了Linux内核源代码中。现在这段代码变成了:

```C
/**
 * idt_setup_early_handler - Initializes the idt table with early handlers
 */
void __init idt_setup_early_handler(void)
{
	int i;
    //在整个初期设置阶段，中断是禁用的
    //`early_idt_handler_array` 数组中的每一项指向的都是同一个通用中断处理程序
	for (i = 0; i < NUM_EXCEPTION_VECTORS; i++)
		set_intr_gate(i, early_idt_handler_array[i]);
#ifdef CONFIG_X86_32
	for ( ; i < NR_VECTORS; i++)
		set_intr_gate(i, early_ignore_irq);
#endif
	load_idt(&idt_descr);
}
```
如你所见，这段代码与之前相比唯一的区别在于中断服务程序入口点数组的名称现在改为了 `early_idt_handler_array`:

```C
extern const char early_idt_handler_array[NUM_EXCEPTION_VECTORS][EARLY_IDT_HANDLER_SIZE];
```

其中 `NUM_EXCEPTION_VECTORS` 和 `EARLY_IDT_HANDLER_SIZE` 的定义如下:

```C
#define NUM_EXCEPTION_VECTORS 32
#define EARLY_IDT_HANDLER_SIZE 9
```

因此，数组 `early_idt_handler_array` 存放着中断服务程序入口，其中每个入口占据9个字节。`early_idt_handlers` 定义在文件[arch/x86/kernel/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/head_64.S)中。`early_idt_handler_array` 也定义在这个文件中:

```assembly
	__INIT
SYM_CODE_START(early_idt_handler_array) //这段代码自动生成为前 `32` 个异常生成了中断处理程序
	i = 0
	.rept NUM_EXCEPTION_VECTORS
	.if ((EXCEPTION_ERRCODE_MASK >> i) & 1) == 0
		UNWIND_HINT_IRET_REGS
		pushq $0	//# Dummy error code, to make stack frame uniform
	.else
		UNWIND_HINT_IRET_REGS offset=8
	.endif
	pushq $i		//# 72(%rsp) Vector number
	jmp early_idt_handler_common
	UNWIND_HINT_IRET_REGS
	i = i + 1
	.fill early_idt_handler_array + i*EARLY_IDT_HANDLER_SIZE - ., 1, 0xcc
	.endr
	UNWIND_HINT_IRET_REGS offset=16
SYM_CODE_END(early_idt_handler_array)
```

这里使用 `.rept NUM_EXCEPTION_VECTORS` 填充了 `early_idt_handler_array` ，其中也包含了 `early_make_pgtable` 的中断服务函数入口(关于该中断服务函数的实现请参考章节 [早期的中断和异常控制](http://0xax.gitbooks.io/linux-insides/content/Initialization/linux-initialization-2.html))。现在我们完成了所有`x86-64`平台相关的代码，即将进入通用内核代码中。当然，我们之后还会在 `setup_arch` 函数中重新回到平台相关代码，但这已经是 `x86_64` 平台早期代码的最后部分。

## 2.1. 为中断堆栈设置`Stack Canary`值

正如之前阅读过的关于Linux内核初始化过程的[章节](http://0xax.gitbooks.io/linux-insides/content/Initialization/index.html)，在[arch/x86/kernel/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/head_64.S)之后的下一步进入到了[init/main.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/init/main.c)中的函数体最大的函数 `start_kernel` 中。这个函数将完成内核以[pid](https://en.wikipedia.org/wiki/Process_identifier) - `1`运行第一个`init`进程
之前的所有初始化工作。其中，与中断和异常处理相关的第一件事是调用 `boot_init_stack_canary` 函数。这个函数通过设置[canary](http://en.wikipedia.org/wiki/Stack_buffer_overflow#Stack_canaries)值来防止中断栈溢出。前面我们已经看过了 `boot_init_stack_canary` 实现的一些细节，现在我们更进一步地认识它。你可以在[arch/x86/include/asm/stackprotector.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/stackprotector.h)中找到这个函数的实现，它的实现取决于 `CONFIG_CC_STACKPROTECTOR` 这个内核配置选项。如果该选项没有置位，那该函数将是一个空函数:

```C
#ifdef CONFIG_CC_STACKPROTECTOR
...
...
...
#else
static inline void boot_init_stack_canary(void)
{
}
#endif
```
在5.10.13中他的定义是：
```c
/*
 * Initialize the stackprotector canary value.
 *
 * NOTE: this must only be called from functions that never return
 * and it must always be inlined.
 *
 * In addition, it should be called from a compilation unit for which
 * stack protector is disabled. Alternatively, the caller should not end
 * with a function call which gets tail-call optimized as that would
 * lead to checking a modified canary value.
 */
static __always_inline void boot_init_stack_canary(void)    /*金丝雀，magic  */
{
	u64 canary;
	u64 tsc;

#ifdef CONFIG_X86_64
	BUILD_BUG_ON(offsetof(struct fixed_percpu_data, stack_canary) != 40);
#endif
	/*
	 * We both use the random pool and the current TSC as a source
	 * of randomness. The TSC only matters for very early init,
	 * there it already has some randomness on most systems. Later
	 * on during the bootup the random pool has true entropy too.
	 */
	get_random_bytes(&canary, sizeof(canary));
	tsc = rdtsc();  
	canary += tsc + (tsc << 32UL);
	canary &= CANARY_MASK;  /* 产生金丝雀 */

	current->stack_canary = canary;
#ifdef CONFIG_X86_64
    //将此值写入IRQ堆栈的顶部
    //如果canary被设置, 关闭本地中断注册bootstrap CPU以及CPU maps
	this_cpu_write(fixed_percpu_data.stack_canary, canary);
#else
//	this_cpu_write(stack_canary.canary, canary);
#endif
}
```


如果设置了内核配置选项 `CONFIG_CC_STACKPROTECTOR` ，那么函数`boot_init_stack_canary` 一开始将检查联合体 `irq_stack_union` 的状态，这个联合体代表了[per-cpu](http://0xax.gitbooks.io/linux-insides/content/Concepts/per-cpu.html)中断栈，其与 `stack_canary` 值中间有40个字节的 `offset` :

```C
#ifdef CONFIG_X86_64
        BUILD_BUG_ON(offsetof(union irq_stack_union, stack_canary) != 40);
#endif
```
5.10.13中：
```c
#ifdef CONFIG_X86_64
	BUILD_BUG_ON(offsetof(struct fixed_percpu_data, stack_canary) != 40);
#endif
```
如之前[章节](http://0xax.gitbooks.io/linux-insides/content/interrupts/interrupts-1.html)所描述， `irq_stack_union` 联合体的定义如下:

```C
union irq_stack_union {
        char irq_stack[IRQ_STACK_SIZE];

    struct {
                char gs_base[40];
                unsigned long stack_canary;
        };
};
```

以上定义位于文件[arch/x86/include/asm/processor.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/processor.h)。众所周知，[C语言](http://en.wikipedia.org/wiki/C_%28programming_language%29)中的[联合体](http://en.wikipedia.org/wiki/Union_type)是一种描述多个数据结构共用一片内存的数据结构。可以看到，第一个数据域 `gs_base` 大小为40 bytes，代表了 `irq_stack` 的栈底。因此，当我们使用 `BUILD_BUG_ON` 对该表达式进行检查时结果应为成功。(关于 `BUILD_BUG_ON` 宏的详细信息可见[Linux内核初始化过程章节](http://0xax.gitbooks.io/linux-insides/content/Initialization/linux-initialization-1.html))。

紧接着我们使用随机数和[时戳计数器](http://en.wikipedia.org/wiki/Time_Stamp_Counter)计算新的 `canary` 值:

```C
get_random_bytes(&canary, sizeof(canary));
tsc = __native_read_tsc();
canary += tsc + (tsc << 32UL);
```
并且通过 `this_cpu_write` 宏将 `canary` 值写入了 `irq_stack_union` 中:

```C
this_cpu_write(irq_stack_union.stack_canary, canary);
```

关于  `this_cpu_*` 系列宏的更多信息参见[Linux kernel documentation](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/this_cpu_ops.txt)。

## 2.2. 禁用/使能本地中断

```c
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)/* 启动内核 */
{
    ...
	local_irq_disable();    /* 关本地中断 x86- cli(close irq)*/
	early_boot_irqs_disabled = true;/* 置位 */
    ...
    ...
    /* 开启中断 */
	early_boot_irqs_disabled = false;
	local_irq_enable(); /*  */
```

在 [init/main.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/init/main.c) 中，与中断和中断处理相关的操作中，设置的 `canary` 的下一步是调用 `local_irq_disable` 宏。

这个宏定义在头文件 [include/linux/irqflags.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/irqflags.h) 中，宏如其名，调用这个宏将禁用本地CPU的中断。我们来仔细了解一下这个宏的实现，首先，它依赖于内核配置选项 `CONFIG_TRACE_IRQFLAGS_SUPPORT` :

```C
#ifdef CONFIG_TRACE_IRQFLAGS_SUPPORT
...
#define local_irq_disable() \
         do { raw_local_irq_disable(); trace_hardirqs_off(); } while (0)
...
#else
...
#define local_irq_disable()     do { raw_local_irq_disable(); } while (0)
...
#endif
```

如你所见，两者唯一的区别在于当 `CONFIG_TRACE_IRQFLAGS_SUPPORT` 选项使能时， `local_irq_disable` 宏将同时调用 `trace_hardirqs_off` 函数。在Linux死锁检测模块[lockdep](http://lwn.net/Articles/321663/)中有一项功能 `irq-flags tracing` 可以追踪 `hardirq` 和 `softirq` 的状态。在这种情况下， `lockdep` 死锁检测模块可以提供系统中关于硬/软中断的开/关事件的相关信息。函数 `trace_hardirqs_off` 的定义位于[kernel/locking/lockdep.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/lockdep.c):

```C
void trace_hardirqs_off(void)
{
	lockdep_hardirqs_off(CALLER_ADDR0);

	if (!this_cpu_read(tracing_irq_cpu)) {
		this_cpu_write(tracing_irq_cpu, 1);
		tracer_hardirqs_off(CALLER_ADDR0, CALLER_ADDR1);
		if (!in_nmi())
			trace_irq_disable_rcuidle(CALLER_ADDR0, CALLER_ADDR1);
	}
}
EXPORT_SYMBOL(trace_hardirqs_off);
NOKPROBE_SYMBOL(trace_hardirqs_off);
```

可见它只是调用了 `trace_hardirqs_off_caller` 函数。 `trace_hardirqs_off_caller` 函数,该函数检查了当前进程的 `tracing_irq_cpu` 域，如果本次 `local_irq_disable` 调用是冗余的话，便使 `redundant_hardirqs_off` 域的值增长，否则便使 `hardirqs_off_events` 域的值增加。这两个域或其它与死锁检测模块 `lockdep` 统计相关的域定义在文件[kernel/locking/lockdep_insides.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/lockdep_insides.h)中的 `lockdep_stats` 结构体中:

```C
struct lockdep_stats {
...
...
...
int     softirqs_off_events;
int     redundant_softirqs_off;
...
...
...
}
```

如果你使能了 `CONFIG_DEBUG_LOCKDEP` 内核配置选项，`lockdep_stats_debug_show`函数会将所有的调试信息写入 `/proc/lockdep` 文件中:

```C
static void lockdep_stats_debug_show(struct seq_file *m)
{
#ifdef CONFIG_DEBUG_LOCKDEP
    unsigned long long hi1 = debug_atomic_read(hardirqs_on_events),
        hi2 = debug_atomic_read(hardirqs_off_events),
        hr1 = debug_atomic_read(redundant_hardirqs_on),
        ...
    ...
    ...
    seq_printf(m, " hardirq on events:             %11llu\n", hi1);
    seq_printf(m, " hardirq off events:            %11llu\n", hi2);
    seq_printf(m, " redundant hardirq ons:         %11llu\n", hr1);
#endif
}
```

你可以如下命令查看其内容:

```
$ sudo cat /proc/lockdep
 hardirq on events:             12838248974
 hardirq off events:            12838248979
 redundant hardirq ons:               67792
 redundant hardirq offs:         3836339146
 softirq on events:                38002159
 softirq off events:               38002187
 redundant softirq ons:                   0
 redundant softirq offs:                  0
```

现在我们总算了解了调试函数 `trace_hardirqs_off` 的一些信息，下文将有独立的章节介绍 `lockdep` 和 `trancing`。`local_disable_irq` 宏的实现中都包含了一个宏 `raw_local_irq_disable` ，这个定义在 [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/irqflags.h) 中，其展开后的样子是:

```C
static inline void native_irq_disable(void)
{
    asm volatile("cli": : :"memory");
}
```

你可能还记得， `cli` 指令将清除[IF](http://en.wikipedia.org/wiki/Interrupt_flag) 标志位，这个标志位控制着处理器是否响应中断或异常。与 `local_irq_disable` 相对的还有宏 `local_irq_enable` ，这个宏的实现与 `local_irq_disable` 很相似，也具有相同的调试机制，区别在于使用 `sti` 指令使能了中断:

```C
static inline void native_irq_enable(void)
{
        asm volatile("sti": : :"memory");
}
```

如今我们了解了 `local_irq_disable` 和 `local_irq_enable` 宏的实现机理。此处是首次调用 `local_irq_disable` 宏，我们还将在Linux内核源代码中多次看到它的倩影。现在我们位于 [init/main.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/init/main.c) 中的 `start_kernel` 函数，并且刚刚禁用了`本地`中断。

```c
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)/* 启动内核 */
{
    ...
	local_irq_disable();    /* 关本地中断 x86- cli(close irq)*/
	early_boot_irqs_disabled = true;/* 置位 */
    ...
    ...
    /* 开启中断 */
	early_boot_irqs_disabled = false;
	local_irq_enable(); /*  */
```

**为什么叫"本地"中断？为什么要禁用本地中断呢？**

早期版本的内核中提供了一个叫做 `cli` 的函数来禁用所有处理器的中断，该函数已经被[移除](https://lwn.net/Articles/291956/)，替代它的是 `local_irq_{enabled,disable}` 宏，用于禁用或使能当前处理器的中断。我们在调用 `local_irq_disable` 宏禁用中断以后，接着设置了变量值:

```C
early_boot_irqs_disabled = true;
```

变量 `early_boot_irqs_disabled` 定义在文件 [include/linux/kernel.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/kernel.h) 中:

```C
extern bool early_boot_irqs_disabled;
```

并在另外的地方使用。例如在 [kernel/smp.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/smp.c) 中的 `smp_call_function_many` 函数中，通过这个变量来检查当前是否由于中断禁用而处于死锁状态:

```C
static void smp_call_function_many_cond(const struct cpumask *mask,
					smp_call_func_t func, void *info,
					bool wait, smp_cond_func_t cond_func)
{
    WARN_ON_ONCE(cpu_online(this_cpu) && irqs_disabled()
                         && !oops_in_progress && !early_boot_irqs_disabled);
}
```
此外，对于Xen，启动内核阶段也有：
```c
/* First C function to be called on Xen boot */
asmlinkage __visible void __init xen_start_kernel(void)
{
	local_irq_disable();
	early_boot_irqs_disabled = true;
}
```


## 2.3. 内核初始化过程中的早期 `trap` 初始化

在 `local_disable_irq` 之后执行的函数是 `boot_cpu_init` 和 `page_address_init`，但这两个函数与中断和异常处理无关(更多与这两个函数有关的信息请阅读内核初始化过程[章节](http://0xax.gitbooks.io/linux-insides/content/Initialization/index.html))。接下来是 `setup_arch` 函数。你可能还有印象，这个函数定义在[arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel.setup.c) 文件中，并完成了很多[架构相关的初始化工作](http://0xax.gitbooks.io/linux-insides/content/Initialization/linux-initialization-4.html)。在 `setup_arch` 函数中与中断相关的第一个函数是 `early_trap_init` 函数，该函数定义于 [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) ，其用许多对程序入口填充了中断描述符表 `Interrupt Descriptor Table` :

```C
void __init early_trap_init(void)
{
    set_intr_gate_ist(X86_TRAP_DB, &debug, DEBUG_STACK);
    set_system_intr_gate_ist(X86_TRAP_BP, &int3, DEBUG_STACK);
#ifdef CONFIG_X86_32
    set_intr_gate(X86_TRAP_PF, page_fault);
#endif
    load_idt(&idt_descr);
}
```

这里出现了三个不同的函数调用

* `set_intr_gate_ist`
* `set_system_intr_gate_ist`
* `set_intr_gate`

这些函数都定义在 [arch/x86/include/asm/desc.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/desc.h) 中，他们做的事情也差不多。第一个函数 `set_intr_gate_ist` 将一个新的中断门插入到`IDT`中，其实现如下:

```C
static inline void set_intr_gate_ist(int n, void *addr, unsigned ist)
{
        BUG_ON((unsigned)n > 0xFF);
        _set_gate(n, GATE_INTERRUPT, addr, 0, ist, __KERNEL_CS);
}
```

该函数首先检查了参数 `n` 即[中断向量编号](http://en.wikipedia.org/wiki/Interrupt_vector_table) 是否不大于 `0xff` 或 255。之前的 [章节] (http://0xax.gitbooks.io/linux-insides/content/interrupts/interrupts-1.html) 中提到过，中断的向量号必须处于 0 到 255 的闭区间。然后调用了 `_set_gate` 函数将中断门设置到了 `IDT` 表中:

```C
static inline void _set_gate(int gate, unsigned type, void *addr,
                             unsigned dpl, unsigned ist, unsigned seg)
{
        gate_desc s;

        pack_gate(&s, type, (unsigned long)addr, dpl, ist, seg);
        write_idt_entry(idt_table, gate, &s);
        write_trace_idt_entry(gate, &s);
}
```

首先，通过 `pack_gate` 函数填充了一个表示 `IDT` 入口项的 `gate_desc` 类型的结构体，参数包括基地址，限制范围，[中断栈表](https://www.kernel.org/doc/Documentation/x86/x86_64/kernel-stacks), [特权等级](http://en.wikipedia.org/wiki/Privilege_level) 和中断类型。中断类型的取值如下:

* `GATE_INTERRUPT`
* `GATE_TRAP`
* `GATE_CALL`
* `GATE_TASK`

并设置了该 `IDT` 项的`present`位域:

```C
static inline void pack_gate(gate_desc *gate, unsigned type, unsigned long func,
                             unsigned dpl, unsigned ist, unsigned seg)
{
        gate->offset_low        = PTR_LOW(func);
        gate->segment           = __KERNEL_CS;
        gate->ist               = ist;
        gate->p                 = 1;
        gate->dpl               = dpl;
        gate->zero0             = 0;
        gate->zero1             = 0;
        gate->type              = type;
        gate->offset_middle     = PTR_MIDDLE(func);
        gate->offset_high       = PTR_HIGH(func);
}
```

然后，我们把这个中断门通过 `write_idt_entry` 宏填入了 `IDT` 中。这个宏展开后是 `native_write_idt_entry` ，其将中断门信息通过索引拷贝到了 `idt_table` 之中:

```C
#define write_idt_entry(dt, entry, g)           native_write_idt_entry(dt, entry, g)

static inline void native_write_idt_entry(gate_desc *idt, int entry, const gate_desc *gate)
{
    memcpy(&idt[entry], gate, sizeof(*gate));
}
```
上述函数被下面函数调用：
```c
static __init void
idt_setup_from_table(gate_desc *idt, const struct idt_data *t, int size/* 个数 */, bool sys)/*  */
{
    /* 门 */
	gate_desc desc;

	for (; size > 0; t++, size--) {
		idt_init_desc(&desc, t);    /*  */
		write_idt_entry(idt, t->vector, &desc); /* 写入 CPU */
		if (sys)
			set_bit(t->vector, system_vectors);
	}
}
/**
 * @n       中断号
 * @addr    中断/异常处理函数的基地址
 */
static __init void set_intr_gate(unsigned int n, const void *addr)
{
	struct idt_data data;

    /*  */
	init_idt_data(&data, n/* 中断号 */, addr/* 处理地址 */);

    //将中断门插入至 `IDT` 表中
	idt_setup_from_table(idt_table, &data, 1, false);
}
```

其中 `idt_table` 是一个 `gate_desc` 类型的数组:

```C
extern gate_desc idt_table[];
```

函数 `set_intr_gate_ist` 的内容到此为止。第二个函数 `set_system_intr_gate_ist` 的实现仅有一个地方不同:

```C
static inline void set_system_intr_gate_ist(int n, void *addr, unsigned ist)
{
        BUG_ON((unsigned)n > 0xFF);
        _set_gate(n, GATE_INTERRUPT, addr, 0x3, ist, __KERNEL_CS);
}
```

注意 `_set_gate` 函数的第四个参数是 `0x3`，而在 `set_intr_gate_ist`函数中这个值是 `0x0`，这个参数代表的是 `DPL`或称为特权等级。其中，`0`代表最高特权等级而 `3`代表最低等级。现在我们了解了 `set_system_intr_gate_ist`, `set_intr_gate_ist`, `set_intr_gate`这三函数的作用并回到 `early_trap_init`函数中:

```C
set_intr_gate_ist(X86_TRAP_DB, &debug, DEBUG_STACK);
set_system_intr_gate_ist(X86_TRAP_BP, &int3, DEBUG_STACK);
```

我们设置了 `#DB`和 `int3`两个 `IDT`入口项。这些函数输入相同的参数组:

* vector number of an interrupt;
* address of an interrupt handler;
* interrupt stack table index.

这就是 `early_trap_init`函数的全部内容，你将在下一章节中看到更多与中断和服务函数相关的内容。

## 2.4. 总结

现在已经到了Linux内核中断和中断服务部分的第二部分的结尾。我们在之前的章节中了解了中断与异常处理的相关理论，并在本部分中开始深入阅读中断和异常处理的代码。我们从Linux内核启动最早期的代码中与中断相关的代码开始。下一部分中我们将继续深入这个有趣的主题，并学习更多关于中断处理相关的内容。

如果你有任何建议或疑问，请在我的 [twitter](https://twitter.com/0xAX)页面中留言或抖一抖我。

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 2.5. 链接

* [IDT](http://en.wikipedia.org/wiki/Interrupt_descriptor_table)
* [Protected mode](http://en.wikipedia.org/wiki/Protected_mode)
* [List of x86 calling conventions](http://en.wikipedia.org/wiki/X86_calling_conventions#List_of_x86_calling_conventions)
* [8086](http://en.wikipedia.org/wiki/Intel_8086)
* [Long mode](http://en.wikipedia.org/wiki/Long_mode)
* [NX](http://en.wikipedia.org/wiki/NX_bit)
* [Extended Feature Enable Register](http://en.wikipedia.org/wiki/Control_register#Additional_Control_registers_in_x86-64_series)
* [Model-specific register](http://en.wikipedia.org/wiki/Model-specific_register)
* [Process identifier](https://en.wikipedia.org/wiki/Process_identifier)
* [lockdep](http://lwn.net/Articles/321663/)
* [irqflags tracing](https://www.kernel.org/doc/Documentation/irqflags-tracing.txt)
* [IF](http://en.wikipedia.org/wiki/Interrupt_flag)
* [Stack canary](http://en.wikipedia.org/wiki/Stack_buffer_overflow#Stack_canaries)
* [Union type](http://en.wikipedia.org/wiki/Union_type)
* [this_cpu_* operations](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/this_cpu_ops.txt)
* [vector number](http://en.wikipedia.org/wiki/Interrupt_vector_table)
* [Interrupt Stack Table](https://www.kernel.org/doc/Documentation/x86/x86_64/kernel-stacks)
* [Privilege level](http://en.wikipedia.org/wiki/Privilege_level)
* [Previous part](http://0xax.gitbooks.io/linux-insides/content/interrupts/interrupts-1.html)




