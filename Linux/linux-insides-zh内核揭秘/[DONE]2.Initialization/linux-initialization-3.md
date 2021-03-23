内核初始化 第三部分
================================================================================

进入内核入口点之前最后的准备工作
--------------------------------------------------------------------------------


这是 Linux 内核初始化过程的第三部分。在[上一个部分](https://github.com/MintCN/linux-insides-zh/blob/master/Initialization/linux-initialization-2.md) 中我们接触到了初期中断和异常处理，而在这个部分中我们要继续看一看 Linux 内核的初始化过程。在之后的章节我们将会关注“内核入口点”—— [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 文件中的`start_kernel` 函数。没错，从技术上说这并不是内核的入口点，只是不依赖于特定架构的通用内核代码的开始。不过，在我们调用 `start_kernel` 之前，有些准备必须要做。下面我们就来看一看。

boot_params again
--------------------------------------------------------------------------------

在上一个部分中我们讲到了设置中断描述符表，并将其加载进 `IDTR` 寄存器。下一步是调用 `copy_bootdata` 函数：

```C
copy_bootdata(__va(real_mode_data));
```

这个函数接受一个参数—— `read_mode_data` 的虚拟地址。`boot_params` 结构体是在 [arch/x86/include/uapi/asm/bootparam.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/uapi/asm/bootparam.h#L114) 作为第一个参数传递到  [arch/x86/kernel/head_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/head_64.S) 中的 `x86_64_start_kernel` 函数的：

```
	/* rsi is pointer to real mode structure with interesting info.
	   pass it to C */
	movq	%rsi, %rdi
```

下面我们来看一看 `__va` 宏。 这个宏定义在 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c)：

```C
#define __va(x)                 ((void *)((unsigned long)(x)+PAGE_OFFSET))
```

其中 `PAGE_OFFSET` 就是 `__PAGE_OFFSET`（即 `0xffff880000000000`），也是所有对物理地址进行直接映射后的虚拟基地址。因此我们就得到了 `boot_params` 结构体的虚拟地址，并把他传入 `copy_bootdata` 函数中。在这个函数里我们把 `real_mod_data` （定义在 [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup.c)） 拷贝进 `boot_params`：

```C
struct boot_params boot_params;
```

`copy_bootdata` 的实现如下:

```C
static void __init copy_bootdata(char *real_mode_data)
{
	char * command_line;
	unsigned long cmd_line_ptr;

	memcpy(&boot_params, real_mode_data, sizeof boot_params);
	sanitize_boot_params(&boot_params);
	cmd_line_ptr = get_cmd_line_ptr();
	if (cmd_line_ptr) {
		command_line = __va(cmd_line_ptr);
		memcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);
	}
}
```
5.10.13为：
```c
//real_mode_data = `boot_params` 结构体的虚拟地址
static void __init copy_bootdata(char *real_mode_data)
{
	char * command_line;
	unsigned long cmd_line_ptr;

	/*
	 * If SME is active, this will create decrypted mappings of the
	 * boot data in advance of the copy operations.
	 */
	sme_map_bootdata(real_mode_data);

	memcpy(&boot_params, real_mode_data, sizeof(boot_params));
	sanitize_boot_params(&boot_params);
	cmd_line_ptr = get_cmd_line_ptr();
	if (cmd_line_ptr) {
		command_line = __va(cmd_line_ptr);
		memcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);
	}

	/*
	 * The old boot data is no longer needed and won't be reserved,
	 * freeing up that memory for use by the system. If SME is active,
	 * we need to remove the mappings that were created so that the
	 * memory doesn't remain mapped as decrypted.
	 */
	sme_unmap_bootdata(real_mode_data);
}
```
首先，这个函数的声明中有一个 `__init` 前缀，这表示这个函数只在初始化阶段使用，并且它所使用的内存将会被释放。

在这个函数中首先声明了两个用于解析内核命令行的变量，然后使用`memcpy` 函数将 `real_mode_data` 拷贝进 `boot_params`。如果系统引导工具（bootloader）没能正确初始化 `boot_params` 中的某些成员的话，那么在接下来调用的 `sanitize_boot_params` 函数中将会对这些成员进行清零，比如 `ext_ramdisk_image` 等。此后我们通过调用 `get_cmd_line_ptr` 函数来得到命令行的地址：

```C
static unsigned long get_cmd_line_ptr(void)
{
	unsigned long cmd_line_ptr = boot_params.hdr.cmd_line_ptr;

	cmd_line_ptr |= (u64)boot_params.ext_cmd_line_ptr << 32;

	return cmd_line_ptr;
}
```

`get_cmd_line_ptr` 函数将会从 `boot_params` 中获得命令行的64位地址并返回。最后，我们检查一下是否正确获得了 `cmd_line_ptr`，并把它的虚拟地址拷贝到一个字节数组 `boot_command_line` 中：

```C
extern char __initdata boot_command_line[];
```

这一步完成之后，我们就得到了内核命令行和 `boot_params` 结构体。之后，内核通过调用 `load_ucode_bsp` 函数来加载处理器微代码（microcode），不过我们目前先暂时忽略这一步。

微代码加载之后，内核会对 `console_loglevel` 进行检查，同时通过 `early_printk` 函数来打印出字符串 `Kernel Alive`。不过这个输出不会真的被显示出来，因为这个时候 `early_printk` 还没有被初始化。这是目前内核中的一个小bug，作者已经提交了补丁 [commit](http://git.kernel.org/cgit/linux/kernel/git/tip/tip.git/commit/?id=91d8f0416f3989e248d3a3d3efb821eda10a85d2)，补丁很快就能应用在主分支中了。所以你可以先跳过这段代码。

> **微代码(Microcode)**:用简单的硬件操作来模拟当时无法用技术直接实现的复杂指令.微代码首先于1951年由英国的M.V.Wilkes引入,它是计算机工程学上的重大创新.它把计算机清楚的分成了两个概念层:体系结构和实现。
> 微码是许多计算机或处理器对硬件结构的一层抽象，或是为实现复杂机器指令的一种数据结构。它一般驻留在高速存储器中并将机器指令翻译成详细的电路操作序列。它有助于将机器指令和底层电子器件分开，以使指令的设计和更改更加自由。
> Microcode：拥有智能功能的计算机部件所包含的程序称为微码。


初始化内存页
--------------------------------------------------------------------------------

至此，我们已经拷贝了 `boot_params` 结构体，接下来将对初期页表进行一些设置以便在初始化内核的过程中使用。我们之前已经对初始化了初期页表，以便支持换页，这在之前的[部分](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-1.html)中已经讨论过。现在则通过调用 `reset_early_page_tables` 函数将初期页表中大部分项清零（在之前的部分也有介绍），只保留内核高地址的映射。然后我们调用：
```c

    //real_mode_data = boot_params
asmlinkage __visible void __init x86_64_start_kernel(char * real_mode_data)
{
	/* Kill off the identity-map trampoline 
    重置了所有的全局页目录项，同时向 `cr3` 中重新写入了的全局页目录表的地址*/
	reset_early_page_tables();
}
```
```C
	clear_page(init_level4_pgt);
```

`init_level4_pgt` 同样定义在 [arch/x86/kernel/head_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/head_64.S):

```assembly
NEXT_PAGE(init_level4_pgt)
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE
	.org    init_level4_pgt + L4_PAGE_OFFSET*8, 0
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE
	.org    init_level4_pgt + L4_START_KERNEL*8, 0
	.quad   level3_kernel_pgt - __START_KERNEL_map + _PAGE_TABLE
```
这两段代码在5.10.13中已经改名为：
```c
	clear_page(init_top_pgt);
```
```c
SYM_DATA_START_PTI_ALIGNED(init_top_pgt)
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.org    init_top_pgt + L4_PAGE_OFFSET*8, 0
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.org    init_top_pgt + L4_START_KERNEL*8, 0
	/* (2^48-(2*1024*1024*1024))/(2^39) = 511 */
	.quad   level3_kernel_pgt - __START_KERNEL_map + _PAGE_TABLE_NOENC
	.fill	PTI_USER_PGD_FILL,8,0
SYM_DATA_END(init_top_pgt)
```
这段代码为内核的代码段、数据段和 bss 段映射了前 2.5G 个字节。`clear_page` 函数定义在 [arch/x86/lib/clear_page_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/lib/clear_page_64.S)：

```assembly
ENTRY(clear_page)
	CFI_STARTPROC
	xorl %eax,%eax
	movl $4096/64,%ecx
	.p2align 4
	.Lloop:
    decl	%ecx
#define PUT(x) movq %rax,x*8(%rdi)
	movq %rax,(%rdi)
	PUT(1)
	PUT(2)
	PUT(3)
	PUT(4)
	PUT(5)
	PUT(6)
	PUT(7)
	leaq 64(%rdi),%rdi
	jnz	.Lloop
	nop
	ret
	CFI_ENDPROC
	.Lclear_page_end:
	ENDPROC(clear_page)
```

顾名思义，这个函数会将页表清零。这个函数的开始和结束部分有两个宏 `CFI_STARTPROC` 和 `CFI_ENDPROC`，他们会展开成 GNU 汇编指令，用于调试：

```C
#define CFI_STARTPROC           .cfi_startproc
#define CFI_ENDPROC             .cfi_endproc
```

在 `CFI_STARTPROC` 之后我们将 `eax` 寄存器清零，并将 `ecx` 赋值为 64（用作计数器）。接下来从 `.Lloop` 标签开始循环，首先就是将 `ecx` 减一。然后将 `rax` 中的值（目前为0）写入 `rdi` 指向的地址，`rdi` 中保存的是 `init_level4_pgt` 的基地址。接下来重复7次这个步骤，但是每次都相对 `rdi` 多偏移8个字节。之后 `init_level4_pgt` 的前64个字节就都被填充为0了。接下来我们将 `rdi` 中的值加上64，重复这个步骤，直到 `ecx` 减至0。最后就完成了将 `init_level4_pgt` 填零。

在将 `init_level4_pgt` 填0之后，再把它的最后一项设置为内核高地址的映射：

```C
init_level4_pgt[511] = early_level4_pgt[511];
```

在前面我们已经使用 `reset_early_page_table` 函数清除 `early_level4_pgt` 中的大部分项，而只保留内核高地址的映射。
在5.10.13中是这样的：
```c
//arch\x86\include\asm\page_64.h
static inline void clear_page(void *page)
{
	alternative_call_2(clear_page_orig,
			   clear_page_rep, X86_FEATURE_REP_GOOD,
			   clear_page_erms, X86_FEATURE_ERMS,
			   "=D" (page),
			   "0" (page)
			   : "cc", "memory", "rax", "rcx");
}
```
其中一些函数的定义如下：
```c
/* alternative.h	arch\x86\include\asm	9789	2021/2/4	6
 * Like alternative_call, but there are two features and respective functions.
 * If CPU has feature2, function2 is used.
 * Otherwise, if CPU has feature1, function1 is used.
 * Otherwise, old function is used.
 */
#define alternative_call_2(oldfunc, newfunc1, feature1, newfunc2, feature2,   \
			   output, input...)				      \
	asm_inline volatile (ALTERNATIVE_2("call %P[old]", "call %P[new1]", feature1,\
		"call %P[new2]", feature2)				      \
		: output, ASM_CALL_CONSTRAINT				      \
		: [old] "i" (oldfunc), [new1] "i" (newfunc1),		      \
		  [new2] "i" (newfunc2), ## input)
```
```c
//	arch\x86\lib\clear_page_64.S	993	2021/2/4	
/*
 * Zero a page.
 * %rdi	- page
 */
SYM_FUNC_START(clear_page_rep)
	movl $4096/8,%ecx
	xorl %eax,%eax
	rep stosq
	ret
SYM_FUNC_END(clear_page_rep)
EXPORT_SYMBOL_GPL(clear_page_rep)

SYM_FUNC_START(clear_page_orig)
	xorl   %eax,%eax
	movl   $4096/64,%ecx
	.p2align 4
.Lloop:
	decl	%ecx
#define PUT(x) movq %rax,x*8(%rdi)
	movq %rax,(%rdi)
	PUT(1)
	PUT(2)
	PUT(3)
	PUT(4)
	PUT(5)
	PUT(6)
	PUT(7)
	leaq	64(%rdi),%rdi
	jnz	.Lloop
	nop
	ret
SYM_FUNC_END(clear_page_orig)
EXPORT_SYMBOL_GPL(clear_page_orig)

SYM_FUNC_START(clear_page_erms)
	movl $4096,%ecx
	xorl %eax,%eax
	rep stosb
	ret
SYM_FUNC_END(clear_page_erms)
EXPORT_SYMBOL_GPL(clear_page_erms)
```

`x86_64_start_kernel` 函数的最后一步是调用：

```C
x86_64_start_reservations(real_mode_data);
```

并传入 `real_mode_data` 参数。 `x86_64_start_reservations` 函数与 `x86_64_start_kernel` 函数定义在同一个文件中：

```C
void __init x86_64_start_reservations(char *real_mode_data)
{
	if (!boot_params.hdr.version)
		copy_bootdata(__va(real_mode_data));

	reserve_ebda_region();

	start_kernel();
}
```

这就是进入内核入口点之前的最后一个函数了。下面我们就来介绍一下这个函数。

内核入口点前的最后一步
--------------------------------------------------------------------------------

```c
//real_mode_data = `boot_params` 结构体
void __init x86_64_start_reservations(char *real_mode_data)
{
	/* version is always not zero if it is copied */
	if (!boot_params.hdr.version)
		copy_bootdata(__va(real_mode_data));

	x86_early_init_platform_quirks();

	switch (boot_params.hdr.hardware_subarch) {
	case X86_SUBARCH_INTEL_MID:
		x86_intel_mid_early_setup();
		break;
	default:
		break;
	}

	start_kernel();
}
```
在 `x86_64_start_reservations` 函数中首先检查了 `boot_params.hdr.version`：

```C
if (!boot_params.hdr.version)
	copy_bootdata(__va(real_mode_data));
```

如果它为0，则再次调用 `copy_bootdata`，并传入 `real_mode_data` 的虚拟地址。

接下来则调用了 `reserve_ebda_region` 函数(5.10.13中已经没了，将其移动至start_kernel->setup_arch中)，它定义在 [arch/x86/kernel/head.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/head.c)。这个函数为 `EBDA`（即Extended BIOS Data Area，扩展BIOS数据区域）预留空间。扩展BIOS预留区域位于常规内存顶部（译注：常规内存（Conventiional Memory）是指前640K字节内存），包含了端口、磁盘参数等数据。

接下来我们来看一下 `reserve_ebda_region` 函数。
在5.10.13中如下：

```c
void __init reserve_bios_regions(void)
{
	unsigned int bios_start, ebda_start;

	/*
	 * NOTE: In a paravirtual environment the BIOS reserved
	 * area is absent. We'll just have to assume that the
	 * paravirt case can handle memory setup correctly,
	 * without our help.
	 */
	if (!x86_platform.legacy.reserve_bios_regions)
		return;

	/*
	 * BIOS RAM size is encoded in kilobytes, convert it
	 * to bytes to get a first guess at where the BIOS
	 * firmware area starts:
	 */
	bios_start = *(unsigned short *)__va(BIOS_RAM_SIZE_KB_PTR);
	bios_start <<= 10;

	/*
	 * If bios_start is less than 128K, assume it is bogus
	 * and bump it up to 640K.  Similarly, if bios_start is above 640K,
	 * don't trust it.
	 */
	if (bios_start < BIOS_START_MIN || bios_start > BIOS_START_MAX)
		bios_start = BIOS_START_MAX;

	/* Get the start address of the EBDA page: */
	ebda_start = get_bios_ebda();

	/*
	 * If the EBDA start address is sane and is below the BIOS region,
	 * then also reserve everything from the EBDA start address up to
	 * the BIOS region.
	 */
	if (ebda_start >= BIOS_START_MIN && ebda_start < bios_start)
		bios_start = ebda_start;

	/* Reserve all memory between bios_start and the 1MB mark: */
	memblock_reserve(bios_start, 0x100000 - bios_start);
}
```

它首先会检查是否启用了半虚拟化：

```C
if (paravirt_enabled())
	return;
```

如果开启了半虚拟化，那么就退出 `reserve_ebda_region` 函数，因为此时没有扩展BIOS数据区域。下面我们首先得到低地址内存的末尾地址：

```C
lowmem = *(unsigned short *)__va(BIOS_LOWMEM_KILOBYTES);
lowmem <<= 10;
```

然后，检测BIOS的起始地址是否越界：

```c
	/*
	 * If bios_start is less than 128K, assume it is bogus
	 * and bump it up to 640K.  Similarly, if bios_start is above 640K,
	 * don't trust it.
	 */
	if (bios_start < BIOS_START_MIN || bios_start > BIOS_START_MAX)
		bios_start = BIOS_START_MAX;
```

这里的宏定义为：

```c
#define BIOS_RAM_SIZE_KB_PTR	0x413

#define BIOS_START_MIN		0x20000U	/* 128K, less than this is insane */
#define BIOS_START_MAX		0x9f000U	/* 640K, absolute maximum */
```

首先我们得到了BIOS地地址内存的虚拟地址，以KB为单位，然后将其左移10位（即乘以1024）转换为以字节为单位。然后我们需要获得扩展BIOS数据区域的地址：

```C
ebda_addr = get_bios_ebda();
```

其中， `get_bios_ebda` 函数定义在 [arch/x86/include/asm/bios_ebda.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/bios_ebda.h)：

```C
/*
 * Returns physical address of EBDA.  Returns 0 if there is no EBDA.
 */
static inline unsigned int get_bios_ebda(void)
{
	/*
	 * There is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E.
	 */
	unsigned int address = *(unsigned short *)phys_to_virt(0x40E);
	address <<= 4;
	return address;	/* 0 means none */
}
```

下面我们来尝试理解一下这段代码。这段代码中，首先我们将物理地址 `0x40E` 转换为虚拟地址，`0x0040:0x000e` 就是包含有扩展BIOS数据区域基地址的代码段。这里我们使用了 `phys_to_virt` 函数进行地址转换，而不是之前使用的 `__va` 宏。不过，事实上他们两个基本上是一样的：

```C
static inline void *phys_to_virt(phys_addr_t address)
{
         return __va(address);
}
```

而不同之处在于，`phys_to_virt` 函数的参数类型 `phys_addr_t` 的定义依赖于 `CONFIG_PHYS_ADDR_T_64BIT`：

```C
#ifdef CONFIG_PHYS_ADDR_T_64BIT
	typedef u64 phys_addr_t;
#else
	typedef u32 phys_addr_t;
#endif
```

具体的类型是由 `CONFIG_PHYS_ADDR_T_64BIT` 设置选项控制的。此后我们得到了包含扩展BIOS数据区域虚拟基地址的段，把它左移4位后返回。这样，`ebda_addr` 变量就包含了扩展BIOS数据区域的基地址。

下一步我们来检查扩展BIOS数据区域与低地址内存的地址，看一看它们是否小于 `INSANE_CUTOFF` 宏：

```C
if (ebda_addr < INSANE_CUTOFF)
	ebda_addr = LOWMEM_CAP;

if (lowmem < INSANE_CUTOFF)
	lowmem = LOWMEM_CAP;
```

`INSANE_CUTOFF` 为：

```C
#define INSANE_CUTOFF		0x20000U
```
而在前面已经描述过，在5.10.13中上面两端代码也做出了改变：
```c
	/*
	 * If the EBDA start address is sane and is below the BIOS region,
	 * then also reserve everything from the EBDA start address up to
	 * the BIOS region.
	 */
	if (ebda_start >= BIOS_START_MIN && ebda_start < bios_start)
		bios_start = ebda_start;
```
BIOS_START_MIN为：
```c
#define BIOS_START_MIN		0x20000U	/* 128K, less than this is insane */
#define BIOS_START_MAX		0x9f000U	/* 640K, absolute maximum */
```
BIOS_START_MIN 和 INSANE_CUTOFF数值相同，即 128 KB. 上一步我们得到了低地址内存中的低地址部分以及扩展BIOS数据区域，然后调用 `memblock_reserve` 函数来在低内存地址与1MB之间为扩展BIOS数据预留内存区域。

```C
lowmem = min(lowmem, ebda_addr);
lowmem = min(lowmem, LOWMEM_CAP);
memblock_reserve(lowmem, 0x100000 - lowmem);
```

`memblock_reserve` 函数定义在 [mm/block.c](https://github.com/torvalds/linux/blob/master/mm/block.c)，它接受两个参数：

* 基物理地址
* 区域大小

然后在给定的基地址处预留指定大小的内存。`memblock_reserve` 是在这本书中我们接触到的第一个Linux内核内存管理框架中的函数。我们很快会详细地介绍内存管理，不过现在还是先来看一看这个函数的实现。

```c
/* memblock.c	mm	60119	2021/3/1	965
    在给定的基地址处预留指定大小的内存 
    @ base = 基物理地址
    @ size = 区域大小
*/
int __init_memblock memblock_reserve(phys_addr_t base, phys_addr_t size)/* 内存预留 */
{
	phys_addr_t end = base + size - 1;/* 结束地址 */

	memblock_dbg("%s: [%pa-%pa] %pS\n", __func__,
		     &base, &end, (void *)_RET_IP_);

	return memblock_add_range(&memblock.reserved, base, size, MAX_NUMNODES, 0);
}
```


Linux内核管理框架初探
--------------------------------------------------------------------------------

在上一段中我们遇到了对 `memblock_reserve` 函数的调用。现在我们来尝试理解一下这个函数是如何工作的。 `memblock_reserve` 函数只是调用了：

```C
memblock_reserve_region(base, size, MAX_NUMNODES, 0);
```

`memblock_reserve_region` 接受四个参数（显然，在5.10.13中也发生改变）：

```c
/**
 * memblock_add_range - add new memblock region
 * @type: memblock type to add new region into
 * @base: base address of the new region
 * @size: size of the new region
 * @nid: nid of the new region
 * @flags: flags of the new region
 *
 * Add new memblock region [@base, @base + @size) into @type.  The new region
 * is allowed to overlap with existing ones - overlaps don't affect already
 * existing regions.  @type is guaranteed to be minimal (all neighbouring
 * compatible regions are merged) after the addition.
 *
 * Return:
 * 0 on success, -errno on failure.
 *//*  */
static int __init_memblock memblock_add_range(struct memblock_type *type,
				phys_addr_t base, phys_addr_t size,
				int nid, enum memblock_flags flags);
```

* 内存区域的物理基地址
* 内存区域的大小
* 最大 NUMA 节点数
* 标志参数 flags

在 `memblock_reserve_region` 函数一开始，就是一个 `memblock_type` 结构体类型的变量，而在`memblock_add_range`中作为入参：

```C
struct memblock_type *_rgn = &memblock.reserved;
```

`memblock_type` 类型代表了一块内存，定义如下：

```C
/**
 * struct memblock_type - collection of memory regions of certain type
 * @cnt: number of regions
 * @max: size of the allocated array
 * @total_size: size of all regions
 * @regions: array of regions
 * @name: the memory type symbolic name
 */
struct memblock_type {
	unsigned long cnt;
	unsigned long max;
	phys_addr_t total_size;
	struct memblock_region *regions;
	char *name;
};
```

因为我们要为扩展BIOS数据区域预留内存块，所以当前内存区域的类型就是预留。`memblock` 结构体的定义为：

```C
/**
 * struct memblock - memblock allocator metadata
 *  Memblock是早期引导期间管理内存区域的方法之一，而通常的内核内存分配器尚未启动并运行
 *
 * @bottom_up: is bottom up direction?
 * @current_limit: physical address of the current allocation limit
 * @memory: usable memory regions
 * @reserved: reserved memory regions
 */
struct memblock {
     bool bottom_up;
     phys_addr_t current_limit;
     struct memblock_type memory;
     struct memblock_type reserved;
#ifdef CONFIG_HAVE_MEMBLOCK_PHYS_MAP
     struct memblock_type physmem;
#endif
};
```
Memblock是早期引导期间管理内存区域的方法之一，而通常的内核内存分配器尚未启动并运行，它描述了一块通用的数据块。我们用 `memblock.reserved` 的值来初始化 `_rgn`。`memblock` 全局变量定义如下：

```C
struct memblock memblock __initdata_memblock = {
	.memory.regions		= memblock_memory_init_regions,
	.memory.cnt		= 1,
	.memory.max		= INIT_MEMBLOCK_REGIONS,
	.reserved.regions	= memblock_reserved_init_regions,
	.reserved.cnt		= 1,
	.reserved.max		= INIT_MEMBLOCK_REGIONS,
#ifdef CONFIG_HAVE_MEMBLOCK_PHYS_MAP
	.physmem.regions	= memblock_physmem_init_regions,
	.physmem.cnt		= 1,
	.physmem.max		= INIT_PHYSMEM_REGIONS,
#endif
	.bottom_up		= false,
	.current_limit		= MEMBLOCK_ALLOC_ANYWHERE,
};
```

我们现在不会继续深究这个变量，但在内存管理部分的中我们会详细地对它进行介绍。需要注意的是，这个变量的声明中使用了 `__initdata_memblock`：

```C
#define __initdata_memblock __meminitdata
```

而 `__meminit_data` 为：

```C
#define __meminitdata    __section(.meminit.data)
```

自此我们得出这样的结论：所有的内存块都将定义在 `.meminit.data` 区段中。在我们定义了 `_rgn` 之后，使用了 `memblock_dbg` 宏来输出相关的信息。你可以在从内核命令行传入参数 `memblock=debug` 来开启这些输出。

在输出了这些调试信息后，是对下面这个函数的调用：

```C
memblock_add_range(_rgn, base, size, nid, flags);
```
> 在5.10.13中该函数为`memblock_insert_region`.

它向 `.meminit.data` 区段添加了一个新的内存块区域。由于 `_rgn` 的值是 `&memblock.reserved`，下面的代码就直接将扩展BIOS数据区域的基地址、大小和标志填入 `_rgn` 中：

```C
if (type->regions[0].size == 0) {
    WARN_ON(type->cnt != 1 || type->total_size);
    type->regions[0].base = base;
    type->regions[0].size = size;
    type->regions[0].flags = flags;
    memblock_set_region_node(&type->regions[0], nid);
    type->total_size = size;
    return 0;
}
```

在填充好了区域后，接着是对 `memblock_set_region_node` 函数的调用。它接受两个参数：

* 填充好的内存区域的地址
* NUMA节点ID

其中我们的区域由 `memblock_region` 结构体来表示：

```C
struct memblock_region {
    phys_addr_t base;
	phys_addr_t size;
	unsigned long flags;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
    int nid;
#endif
};
```

> 这几个数据结构之间的关系为：memblock->memblock_type->memblock_region.
>    memblock->reserved->regions.

NUMA节点ID依赖于 `MAX_NUMNODES` 宏，定义在 [include/linux/numa.h](https://github.com/torvalds/linux/blob/master/include/linux/numa.h)

```C
#define MAX_NUMNODES    (1 << NODES_SHIFT)
```

其中 `NODES_SHIFT` 依赖于 `CONFIG_NODES_SHIFT` 配置参数，定义如下：

```C
#ifdef CONFIG_NODES_SHIFT
  #define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
  #define NODES_SHIFT     0
#endif
```

`memblick_set_region_node` 函数只是填充了 `memblock_region` 中的 `nid` 成员：

```C
static inline void memblock_set_region_node(struct memblock_region *r, int nid)
{
         r->nid = nid;
}
```

在这之后我们就在 `.meminit.data` 区段拥有了为扩展BIOS数据区域预留的第一个  `memblock`。`reserve_ebda_region` 已经完成了它该做的任务，我们回到 [arch/x86/kernel/head64.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/head64.c) 继续。

至此我们已经结束了进入内核之前所有的准备工作。`x86_64_start_reservations` 的最后一步是调用 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 中的：

```C
start_kernel()
```

这一部分到此结束。

小结
--------------------------------------------------------------------------------

本书的第三部分到这里就结束了。在下一部分中，我们将会见到内核入口点处的初始化工作 —— 位于 `start_kernel` 函数中。这些工作是在启动第一个进程 `init` 之前首先要完成的工作。

如果你有任何问题或建议，请在twitter上联系我 [0xAX](https://twitter.com/0xAX)，或者通过[邮件](anotherworldofworld@gmail.com)与我沟通，还可以新开[issue](https://github.com/MintCN/linux-insides-zh/issues/new)。

相关链接
--------------------------------------------------------------------------------

* [BIOS data area](http://stanislavs.org/helppc/bios_data_area.html)
* [What is in the extended BIOS data area on a PC?](http://www.kryslix.com/nsfaq/Q.6.html)
* [Previous part](https://github.com/MintCN/linux-insides-zh/blob/master/Initialization/linux-initialization-2.md)
