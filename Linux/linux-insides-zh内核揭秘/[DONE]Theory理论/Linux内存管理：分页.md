<center><font size='6'>Linux内存管理：分页</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>



# 1. 简介


在 Linux 内核启动过程中的[第五部分](https://rtoax.blog.csdn.net/article/details/114923811)，我们学到了内核在启动的最早阶段都做了哪些工作。接下来，在我们明白内核如何运行第一个 init 进程之前，内核初始化其他部分，比如加载 `initrd` ，初始化 lockdep ，以及许多许多其他的工作。

是的，那将有很多不同的事，但是还有更多更多更多关于**内存**的工作。

在我看来，一般而言，内存管理是 Linux 内核和系统编程最复杂的部分之一。这就是为什么在我们学习内核初始化过程之前，需要了解分页。

**分页**是**将线性地址转换为物理地址的机制**。如果我们已经读过了这本书之前的部分，你可能记得我们在实模式下有分段机制，当时物理地址是由左移四位段寄存器加上偏移算出来的。我们也看了保护模式下的分段机制，其中我们使用描述符表得到描述符，进而得到基地址，然后加上偏移地址就获得了实际物理地址。由于我们在 64 位模式，我们将看分页机制。

正如 Intel 手册中说的：

> 分页机制提供一种机制，为了实现常见的按需分页，比如虚拟内存系统就是将一个程序执行环境中的段按照需求被映射到物理地址。

所以本文将尝试解释分页背后的理论。当然它将与64位版本的 Linux 内核关系密切，但是不会深入太多细节。

# 2. 开启分页

有三种分页模式：

* 32 位分页模式；
* PAE 分页；
* IA-32e 分页。

我们这里将只解释最后一种模式。为了开启 `IA-32e 分页模式`，我们需要做如下事情：

* 设置 `CR0.PG` 位`X86_CR0_PG`；
* 设置 `CR4.PAE` 位`X86_CR0_PE`；
* 设置 `IA32_EFER.LME` 位`EFER_LME`。

我们已经在 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/boot/compressed/head_64.S) 中看见了这些位被设置了：

```assembly
movl	$(X86_CR0_PG | X86_CR0_PE), %eax
movl	%eax, %cr0
```
其中：
```c
#define X86_CR0_PE_BIT		0 /* Protection Enable */
#define X86_CR0_PE		_BITUL(X86_CR0_PE_BIT)  /* 设置保护位，用于保护模式 */
#define X86_CR0_PG_BIT		31 /* Paging */
#define X86_CR0_PG		_BITUL(X86_CR0_PG_BIT)

/* x86-64 specific MSRs */
#define MSR_EFER		0xc0000080 /* extended feature register */

#define _EFER_LME		8  /* Long mode enable */
#define EFER_LME		(1<<_EFER_LME)
```
可见，

```assembly
movl	$MSR_EFER, %ecx
rdmsr
btsl	$_EFER_LME, %eax
wrmsr
```

# 3. 分页数据结构


分页将线性地址分为固定尺寸的页。页会被映射进入物理地址空间或外部存储设备。这个固定尺寸在 `x86_64` 内核中是 `4096` 字节。为了将线性地址转换位物理地址，需要使用到一些特殊的数据结构。每个结构都是 `4096` 字节并包含 `512` 项（这只为 `PAE` 和 `IA32_EFER.LME` 模式）。分页结构是层次级的， Linux 内核在 `x86_64` 框架中使用4层的分层机制。CPU使用一部分线性地址去确定另一个分页结构中的项，这个分页结构可能在最低层，物理内存区域（页框），在这个区域的物理地址（页偏移）。最高层的分页结构的地址存储在 `cr3` 寄存器中。我们已经从 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/boot/compressed/head_64.S) 这个文件中已经看到了。

```assembly
leal	pgtable(%ebx), %eax
movl	%eax, %cr3
```

我们构建页表结构并且将这个最高层结构的地址存放在 `cr3` 寄存器中。这里 `cr3` 用于存储最高层结构的地址，在 Linux 内核中被称为 `PML4` 或 `Page Global Directory` 。 `cr3` 是一个64位的寄存器，并且有着如下的结构：

```
63                  52 51                                                        32
 --------------------------------------------------------------------------------
|                     |                                                          |
|    Reserved MBZ     |            Address of the top level structure            |
|                     |                                                          |
 --------------------------------------------------------------------------------
31                                  12 11            5     4     3 2             0
 --------------------------------------------------------------------------------
|                                     |               |  P  |  P  |              |
|  Address of the top level structure |   Reserved    |  C  |  W  |    Reserved  |
|                                     |               |  D  |  T  |              |
 --------------------------------------------------------------------------------
```

这些字段有着如下的意义：

* 第 0 到第 2 位 - 忽略； 
* 第 12 位到第 51 位 - 存储最高层分页结构的地址；
* 第 3 位 到第 4 位 - PWT 或 Page-Level Writethrough 和 PCD 或 Page-level Cache Disable 显示。这些位控制页或者页表被硬件缓存处理的方式；
* 保留位 - 保留，但必须为 0 ；
* 第 52 到第 63 位 - 保留，但必须为 0 ；

简单的cr写：
```c
static inline void native_write_cr3(unsigned long val)/* 页全局目录写入 CR3 寄存器 */
{
	asm volatile("mov %0,%%cr3": : "r" (val) : "memory");
}
```

线性地址转换过程如下所示：

* 一个给定的线性地址传递给 [MMU](http://en.wikipedia.org/wiki/Memory_management_unit) 而不是存储器总线；
* 64位线性地址分为很多部分。只有低 48 位是有意义的，它意味着 `2^48` 或 256TB 的线性地址空间在任意给定时间内都可以被访问；
* `cr3` 寄存器存储这个最高层分页数据结构的地址；
* 给定的线性地址中的第 39 位到第 47 位存储一个第 4 级分页结构的索引，第 30 位到第 38 位存储一个第3级分页结构的索引，第 29 位到第 21 位存储一个第 2 级分页结构的索引，第 12 位到第 20 位存储一个第 1 级分页结构的索引，第 0 位到第 11 位提供物理页的字节偏移；

按照图示，我们可以这样想象它：

![四层分页](_v_images/20210323101034800_7562.png)

每一个对线性地址的访问不是一个**管态访问**就是**用户态访问**。这个访问是被 `CPL (Current Privilege Level)` 所决定。如果 `CPL < 3` ，那么它是管态访问级，否则，它就是用户态访问级。比如，最高级页表项包含访问位和如下的结构：

```
63  62                  52 51                                                    32
 --------------------------------------------------------------------------------
| N |                     |                                                     |
|   |     Available       |     Address of the paging structure on lower level  |
| X |                     |                                                     |
 --------------------------------------------------------------------------------
31                                              12 11  9 8 7 6 5   4   3 2 1     0
 --------------------------------------------------------------------------------
|                                                |     | M |I| | P | P |U|W|    |
| Address of the paging structure on lower level | AVL | B |G|A| C | W | | |  P |
|                                                |     | Z |N| | D | T |S|R|    |
 --------------------------------------------------------------------------------
```

其中：

* 第 63 位 - N/X 位（不可执行位）显示被这个页表项映射的所有物理页执行代码的能力；
* 第 52 位到第 62 位 - 被CPU忽略，被系统软件使用；
* 第 12 位到第 51 位 - 存储低级分页结构的物理地址；
* 第 9 位到第 11 位 - 被 CPU 忽略；
* MBZ - 必须为 0 ；
* 忽略位；
* A - 访问位暗示物理页或者页结构被访问；
* PWT 和 PCD 用于缓存；
* U/S - 用户/管理位控制对被这个页表项映射的所有物理页用户访问；
* R/W - 读写位控制着被这个页表项映射的所有物理页的读写权限
* P - 存在位。当前位表示页表或物理页是否被加载进内存；

好的，我们知道了分页结构和它们的表项。现在我们来看一下 Linux 内核中的 4 级分页机制的一些细节。

# 4. Linux 内核中的分页结构


就如我们已经看到的那样， `x86_64`Linux 内核使用4级页表。它们的名字是：

* 全局页目录
* 上层页目录
* 中间页目录
* 页表项

在你已经编译和安装 Linux 内核之后，你可以看到保存了内核函数的虚拟地址的文件 `System.map`。例如：

```
[rongtao@localhost src]$ grep "start_kernel" System.map
ffffffff83c125a0 T x86_64_start_kernel
ffffffff83c1314e T start_kernel
ffffffff83c1fb7a T xen_start_kernel
```

这里我们可以看见 `ffffffff83c1314e` 。我怀疑你是否真的有安装这么多内存。但是无论如何， `start_kernel` 和  `x86_64_start_kernel` 将会被执行。在 `x86_64` 中，地址空间的大小是 `2^64` ，但是它太大了，这就是为什么我们使用一个较小的地址空间，只是 48 位的宽度。所以一个情况出现，虽然物理地址空间限制到 48 位，但是寻址仍然使用 64 位指针。 这个问题是如何解决的？看下面的这个表。

```
0xffffffffffffffff  +-----------+
                    |           |
                    |           | Kernelspace
                    |           |
0xffff800000000000  +-----------+
                    |           |
                    |           |
                    |   hole    |
                    |           |
                    |           |
0x00007fffffffffff  +-----------+
                    |           |
                    |           |  Userspace
                    |           |
0x0000000000000000  +-----------+
```

这个解决方案是 `sign extension` 。这里我们可以看到一个虚拟地址的低 48 位可以被用于寻址。第 48 位到第 63 位全是 0 或 1 。注意这个虚拟地址空间被分为两部分：

* 内核空间
* 用户空间

用户空间占用虚拟地址空间的低部分，从 `0x000000000000000` 到 `0x00007fffffffffff` ，而内核空间占据从 `0xffff8000000000` 到 `0xffffffffffffffff` 的高部分。注意，第 48 位到第 63 位是对于用户空间是 0 ，对于内核空间是 1 。内核空间和用户空间中的所有地址是标准地址，而在这些内存区域中间有非标准区域。这两块内存区域（内核空间和用户空间）合起来是 48 位宽度。我们可以在 [Documentation/x86/x86_64/mm.txt](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt) 找到 4 级页表下的虚拟内存映射:

```
0000000000000000 - 00007fffffffffff (=47 bits) user space, different per mm
hole caused by [48:63] sign extension
ffff800000000000 - ffff87ffffffffff (=43 bits) guard hole, reserved for hypervisor
ffff880000000000 - ffffc7ffffffffff (=64 TB) direct mapping of all phys. memory
ffffc80000000000 - ffffc8ffffffffff (=40 bits) hole
ffffc90000000000 - ffffe8ffffffffff (=45 bits) vmalloc/ioremap space
ffffe90000000000 - ffffe9ffffffffff (=40 bits) hole
ffffea0000000000 - ffffeaffffffffff (=40 bits) virtual memory map (1TB)
... unused hole ...
ffffec0000000000 - fffffc0000000000 (=44 bits) kasan shadow memory (16TB)
... unused hole ...
ffffff0000000000 - ffffff7fffffffff (=39 bits) %esp fixup stacks
... unused hole ...
ffffffff80000000 - ffffffffa0000000 (=512 MB)  kernel text mapping, from phys 0
ffffffffa0000000 - ffffffffff5fffff (=1525 MB) module mapping space
ffffffffff600000 - ffffffffffdfffff (=8 MB) vsyscalls
ffffffffffe00000 - ffffffffffffffff (=2 MB) unused hole
```

这里我们可以看到用户空间，内核空间和非标准空间的内存映射。用户空间的内存映射很简单。让我们来更近地查看内核空间。我们可以看到它始于为管理程序 (hypervisor) 保留的防御空洞 (guard hole) 。我们可以在 [arch/x86/include/asm/page_64_types.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/page_64_types.h) 这个文件中看到防御空洞的概念！

```C
#define __PAGE_OFFSET _AC(0xffff880000000000, UL)
```

以前防御空洞和 `__PAGE_OFFSET` 是从 `0xffff800000000000` 到 `0xffff80ffffffffff` ，用来防止对非标准区域的访问，但是后来为了管理程序扩展了 3 位。

紧接着是内核空间中最低的可用空间 - `ffff880000000000` 。这个虚拟地址空间是为了所有的物理内存的直接映射。在这块空间之后，还是防御空洞。它位于所有物理内存的直接映射地址和被 vmalloc 分配的地址之间。在第一个 1TB 的虚拟内存映射和无用的空洞之后，我们可以看到 `ksan` 影子内存 (shadow memory) 。它是通过 [commit](https://github.com/torvalds/linux/commit/ef7f0d6a6ca8c9e4b27d78895af86c2fbfaeedb2) 提交到内核中，并且保持内核空间无害。在紧接着的无用空洞之后，我们可以看到 `esp` 固定栈（我们会在本书其他部分讨论它）。内核代码段的开始从物理地址 - `0` 映射。我们可以在相同的文件中找到将这个地址定义为 `__PAGE_OFFSET` 。

```C
#define __START_KERNEL_map      _AC(0xffffffff80000000, UL)
```

通常内核的 `.text` 段开始于 `CONFIG_PHYSICAL_START` 偏移。我们已经在 [ELF64](https://rtoax.blog.csdn.net/article/details/115112282) 相关帖子中看见。

```
readelf -s vmlinux | grep ffffffff81000000
     1: ffffffff81000000     0 SECTION LOCAL  DEFAULT    1 
 65099: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 _text
 90766: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 startup_64
```

这里我将 `CONFIG_PHYSICAL_START` 设置为 `0x1000000` 来检查 `vmlinux` 。所以我们有内核代码段的起始点 - `0xffffffff80000000` 和 偏移 - `0x1000000` ，计算出来的虚拟地址将会是 `0xffffffff80000000 + 1000000 = 0xffffffff81000000` 。

在内核代码段之后有一个为内核模块 `vsyscalls` 准备的虚拟内存区域和 2M 无用的空洞。

我们已经看见内核虚拟内存映射是如何布局的以及虚拟地址是如何转换位物理地址。让我们以下面的地址为例：

```
0xffffffff81000000
```
 
在二进制内它将是：

```
1111111111111111 111111111 111111110 000001000 000000000 000000000000
      63:48        47:39     38:30     29:21     20:12      11:0
```

这个虚拟地址将被分为如下描述的几部分：

* `48-63` - 不使用的位；
* `37-49` - 给定线性地址的这些位描述一个 4 级分页结构的索引；
* `30-38` - 这些位存储一个 3 级分页结构的索引；
* `21-29` - 这些位存储一个 2 级分页结构的索引；
* `12-20` - 这些位存储一个 1 级分页结构的索引；
* `0-11`  - 这些位提供物理页的偏移；


就这样了。现在你知道了一些关于分页理论，而且我们可以在内核源码上更近一步，查看那些最先的初始化步骤。

# 5. 总结

这简短的关于分页理论的部分至此已经结束了。当然，这个帖子不可能包含分页的所有细节，但是我们很快会看到在实践中 Linux 内核如何构建分页结构以及使用它们工作。

# 6. 链接

* [Paging on Wikipedia](http://en.wikipedia.org/wiki/Paging)
* [Intel 64 and IA-32 architectures software developer's manual volume 3A](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [MMU](http://en.wikipedia.org/wiki/Memory_management_unit)
* [ELF64](https://github.com/0xAX/linux-insides/blob/master/Theory/ELF.md)
* [Documentation/x86/x86_64/mm.txt](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt)
* [Last part - Kernel booting process](http://0xax.gitbooks.io/linux-insides/content/Booting/linux-bootstrap-5.html)


<center><font size='6'>英文原文</font></center>


# 7. Introduction

In the fifth [part](https://0xax.gitbook.io/linux-insides/summary/booting/linux-bootstrap-5) of the series `Linux kernel booting process` we learned about what the kernel does in its earliest stage. In the next step the kernel will initialize different things like `initrd` mounting, lockdep initialization, and many many other things, before we can see how the kernel runs the first init process.

Yeah, there will be many different things, but many many and once again many work with **memory**.

In my view, memory management is one of the most complex parts of the Linux kernel and in system programming in general. This is why we need to get acquainted with paging, before we proceed with the kernel initialization stuff.

`Paging` is a mechanism that translates a linear memory address to a physical address. If you have read the previous parts of this book, you may remember that we saw segmentation in real mode when physical addresses are calculated by shifting a segment register by four and adding an offset. We also saw segmentation in protected mode, where we used the descriptor tables and base addresses from descriptors with offsets to calculate the physical addresses. Now we will see paging in 64-bit mode.

As the Intel manual says:

> Paging provides a mechanism for implementing a conventional demand-paged, virtual-memory system where sections of a program’s execution environment are mapped into physical memory as needed.

So... In this post I will try to explain the theory behind paging. Of course it will be closely related to the `x86_64` version of the Linux kernel, but we will not go into too much details (at least in this post).

# 8. Enabling paging

There are three paging modes:

* 32-bit paging;
* PAE paging;
* IA-32e paging.

We will only explain the last mode here. To enable the `IA-32e paging` paging mode we need to do following things:

* set the `CR0.PG` bit;
* set the `CR4.PAE` bit;
* set the `IA32_EFER.LME` bit.

We already saw where those bits were set in [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S):

```assembly
movl	$(X86_CR0_PG | X86_CR0_PE), %eax
movl	%eax, %cr0
```

and

```assembly
movl	$MSR_EFER, %ecx
rdmsr
btsl	$_EFER_LME, %eax
wrmsr
```

# 9. Paging structures

Paging divides the linear address space into fixed-size pages. Pages can be mapped into the physical address space or external storage. This fixed size is `4096` bytes for the `x86_64` Linux kernel. To perform the translation from linear address to physical address, special structures are used. Every structure is `4096` bytes and contains `512` entries (this only for `PAE` and `IA32_EFER.LME` modes). Paging structures are hierarchical and the Linux kernel uses 4 level of paging in the `x86_64` architecture. The CPU uses a part of linear addresses to identify the entry in another paging structure which is at the lower level, physical memory region (`page frame`) or physical address in this region (`page offset`). The address of the top level paging structure located in the `cr3` register. We have already seen this in [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S):

```assembly
leal	pgtable(%ebx), %eax
movl	%eax, %cr3
```

We build the page table structures and put the address of the top-level structure in the `cr3` register. Here `cr3` is used to store the address of the top-level structure, the `PML4` or `Page Global Directory` as it is called in the Linux kernel. `cr3` is 64-bit register and has the following structure:

```
63                  52 51                                                        32
 --------------------------------------------------------------------------------
|                     |                                                          |
|    Reserved MBZ     |            Address of the top level structure            |
|                     |                                                          |
 --------------------------------------------------------------------------------
31                                  12 11            5     4     3 2             0
 --------------------------------------------------------------------------------
|                                     |               |  P  |  P  |              |
|  Address of the top level structure |   Reserved    |  C  |  W  |    Reserved  |
|                                     |               |  D  |  T  |              |
 --------------------------------------------------------------------------------
```

These fields have the following meanings:

* Bits 63:52 - reserved must be 0.
* Bits 51:12 - stores the address of the top level paging structure;
* Bits 11: 5 - reserved must be 0;
* Bits 4 : 3 - PWT or Page-Level Writethrough and PCD or Page-level cache disable indicate. These bits control the way the page or Page Table is handled by the hardware cache;
* Bits 2 : 0 - ignored;

The linear address translation is following:

* A given linear address arrives to the [MMU](http://en.wikipedia.org/wiki/Memory_management_unit) instead of memory bus.
* 64-bit linear address is split into some parts. Only low 48 bits are significant, it means that `2^48` or 256 TBytes of linear-address space may be accessed at any given time.
* `cr3` register stores the address of the 4 top-level paging structure.
* `47:39` bits of the given linear address store an index into the paging structure level-4, `38:30` bits store index into the paging structure level-3, `29:21` bits store an index into the paging structure level-2, `20:12` bits store an index into the paging structure level-1 and `11:0` bits provide the offset into the physical page in byte.

schematically, we can imagine it like this:

![四层分页](_v_images/20210323101034800_7562.png)

Every access to a linear address is either a supervisor-mode access or a user-mode access. This access is determined by the `CPL` (current privilege level). If `CPL < 3` it is a supervisor mode access level, otherwise it is a user mode access level. For example, the top level page table entry contains access bits and has the following structure (See [arch/x86/include/asm/pgtable_types.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/pgtable_types.h) for the bit offset definitions):

```
63  62                  52 51                                                    32
 --------------------------------------------------------------------------------
| N |                     |                                                     |
|   |     Available       |     Address of the paging structure on lower level  |
| X |                     |                                                     |
 --------------------------------------------------------------------------------
31                                              12 11  9 8 7 6 5   4   3 2 1     0
 --------------------------------------------------------------------------------
|                                                |     | M |I| | P | P |U|W|    |
| Address of the paging structure on lower level | AVL | B |G|A| C | W | | |  P |
|                                                |     | Z |N| | D | T |S|R|    |
 --------------------------------------------------------------------------------
```

Where:

* 63 bit - N/X bit (No Execute Bit) which presents ability to execute the code from physical pages mapped by the table entry;
* 62:52 bits - ignored by CPU, used by system software;
* 51:12 bits - stores physical address of the lower level paging structure;
* 11: 9 bits - ignored by CPU;
* MBZ - must be zero bits;
* Ignored bits;
* A - accessed bit indicates was physical page or page structure accessed;
* PWT and PCD used for cache;
* U/S - user/supervisor bit controls user access to all the physical pages mapped by this table entry;
* R/W - read/write bit controls read/write access to all the physical pages mapped by this table entry;
* P - present bit. Current bit indicates was page table or physical page loaded into primary memory or not.

Ok, we know about the paging structures and their entries. Now let's see some details about 4-level paging in the Linux kernel.

# 10. Paging structures in the Linux kernel

As we've seen, the Linux kernel in `x86_64` uses 4-level page tables. Their names are:

* Page Global Directory
* Page Upper  Directory
* Page Middle Directory
* Page Table Entry

After you've compiled and installed the Linux kernel, you can see the `System.map` file which stores the virtual addresses of the functions that are used by the kernel. For example:

```
$ grep "start_kernel" System.map
ffffffff81efe497 T x86_64_start_kernel
ffffffff81efeaa2 T start_kernel
```

We can see `0xffffffff81efe497` here. I doubt you really have that much RAM installed. But anyway, `start_kernel` and `x86_64_start_kernel` will be executed. The address space in `x86_64` is `2^64` wide, but it's too large, that's why a smaller address space is used, only 48-bits wide. So we have a situation where the physical address space is limited to 48 bits, but addressing still performs with 64 bit pointers. How is this problem solved? Look at this diagram:

```
0xffffffffffffffff  +-----------+
                    |           |
                    |           | Kernelspace
                    |           |
0xffff800000000000  +-----------+
                    |           |
                    |           |
                    |   hole    |
                    |           |
                    |           |
0x00007fffffffffff  +-----------+
                    |           |
                    |           |  Userspace
                    |           |
0x0000000000000000  +-----------+
```

This solution is `sign extension`. Here we can see that the lower 48 bits of a virtual address can be used for addressing. Bits `63:48` can be either only zeroes or only ones. Note that the virtual address space is split into 2 parts:

* Kernel space
* Userspace

Userspace occupies the lower part of the virtual address space, from `0x000000000000000` to `0x00007fffffffffff` and kernel space occupies the highest part from `0xffff8000000000` to `0xffffffffffffffff`. Note that bits `63:48` is 0 for userspace and 1 for kernel space. All addresses which are in kernel space and in userspace or in other words which higher `63:48` bits are zeroes or ones are called `canonical` addresses. There is a `non-canonical` area between these memory regions. Together these two memory regions (kernel space and user space) are exactly `2^48` bits wide. We can find the virtual memory map with 4 level page tables in the [Documentation/x86/x86_64/mm.txt](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/x86/x86_64/mm.txt):

```
0000000000000000 - 00007fffffffffff (=47 bits) user space, different per mm
hole caused by [48:63] sign extension
ffff800000000000 - ffff87ffffffffff (=43 bits) guard hole, reserved for hypervisor
ffff880000000000 - ffffc7ffffffffff (=64 TB) direct mapping of all phys. memory
ffffc80000000000 - ffffc8ffffffffff (=40 bits) hole
ffffc90000000000 - ffffe8ffffffffff (=45 bits) vmalloc/ioremap space
ffffe90000000000 - ffffe9ffffffffff (=40 bits) hole
ffffea0000000000 - ffffeaffffffffff (=40 bits) virtual memory map (1TB)
... unused hole ...
ffffec0000000000 - fffffc0000000000 (=44 bits) kasan shadow memory (16TB)
... unused hole ...
ffffff0000000000 - ffffff7fffffffff (=39 bits) %esp fixup stacks
... unused hole ...
ffffffff80000000 - ffffffffa0000000 (=512 MB)  kernel text mapping, from phys 0
ffffffffa0000000 - ffffffffff5fffff (=1525 MB) module mapping space
ffffffffff600000 - ffffffffffdfffff (=8 MB) vsyscalls
ffffffffffe00000 - ffffffffffffffff (=2 MB) unused hole
```

We can see here the memory map for user space, kernel space and the non-canonical area in-between them. The user space memory map is simple. Let's take a closer look at the kernel space. We can see that it starts from the guard hole which is reserved for the hypervisor. We can find the definition of this guard hole in [arch/x86/include/asm/page_64_types.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/page_64_types.h):

```C
#define __PAGE_OFFSET _AC(0xffff880000000000, UL)
```

Previously this guard hole and `__PAGE_OFFSET` was from `0xffff800000000000` to `0xffff87ffffffffff` to prevent access to non-canonical area, but was later extended by 3 bits for the hypervisor.

Next is the lowest usable address in kernel space - `ffff880000000000`. This virtual memory region is for direct mapping of all the physical memory. After the memory space which maps all the physical addresses, the guard hole. It needs to be between the direct mapping of all the physical memory and the vmalloc area. After the virtual memory map for the first terabyte and the unused hole after it, we can see the `kasan` shadow memory. It was added by [commit](https://github.com/torvalds/linux/commit/ef7f0d6a6ca8c9e4b27d78895af86c2fbfaeedb2) and provides the kernel address sanitizer. After the next unused hole we can see the `esp` fixup stacks (we will talk about it in other parts of this book) and the start of the kernel text mapping from the physical address - `0`. We can find the definition of this address in the same file as the `__PAGE_OFFSET`:

```C
#define __START_KERNEL_map      _AC(0xffffffff80000000, UL)
```

Usually kernel's `.text` starts here with the `CONFIG_PHYSICAL_START` offset. We have seen it in the post about [ELF64](https://github.com/0xAX/linux-insides/blob/master/Theory/ELF.md):

```
readelf -s vmlinux | grep ffffffff81000000
     1: ffffffff81000000     0 SECTION LOCAL  DEFAULT    1 
 65099: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 _text
 90766: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 startup_64
```

Here I check `vmlinux` with `CONFIG_PHYSICAL_START` is `0x1000000`. So we have the start point of the kernel `.text` - `0xffffffff80000000` and offset - `0x1000000`, the resulted virtual address will be `0xffffffff80000000 + 1000000 = 0xffffffff81000000`.

After the kernel `.text` region there is the virtual memory region for kernel module, `vsyscalls` and an unused hole of 2 megabytes.

We've seen how virtual memory map in the kernel is laid out and how a virtual address is translated into a physical one. Let's take the following address as example:

```
0xffffffff81000000
```

In binary it will be:

```
1111111111111111 111111111 111111110 000001000 000000000 000000000000
      63:48        47:39     38:30     29:21     20:12      11:0
```

This virtual address is split in parts as described above:

* `63:48` - bits not used;
* `47:39` - bits store an index into the paging structure level-4;
* `38:30` - bits store index into the paging structure level-3;
* `29:21` - bits store an index into the paging structure level-2;
* `20:12` - bits store an index into the paging structure level-1;
* `11:0`  - bits provide the offset into the physical page in byte.

That is all. Now you know a little about theory of `paging` and we can go ahead in the kernel source code and see the first initialization steps.

# 11. Conclusion

It's the end of this short part about paging theory. Of course this post doesn't cover every detail of paging, but soon we'll see in practice how the Linux kernel builds paging structures and works with them.

**Please note that English is not my first language and I am really sorry for any inconvenience. If you've found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

# 12. Links

* [Paging on Wikipedia](http://en.wikipedia.org/wiki/Paging)
* [Intel 64 and IA-32 architectures software developer's manual volume 3A](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [MMU](http://en.wikipedia.org/wiki/Memory_management_unit)
* [ELF64](https://github.com/0xAX/linux-insides/blob/master/Theory/ELF.md)
* [Documentation/x86/x86_64/mm.txt](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/x86/x86_64/mm.txt)
* [Last part - Kernel booting process](https://0xax.gitbook.io/linux-insides/summary/booting/linux-bootstrap-5)
