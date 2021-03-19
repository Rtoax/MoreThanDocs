内核引导过程. Part 5.
================================================================================

内核解压
--------------------------------------------------------------------------------

这是`内核引导过程`系列文章的第五部分。在[前一部分](linux-bootstrap-4.md#transition-to-the-long-mode)我们看到了切换到64位模式的过程，在这一部分我们会从这里继续。我们会看到跳进内核代码的最后步骤：**内核解压前的准备、重定位和直接内核解压**。所以...让我们再次深入内核源码。

内核解压前的准备
--------------------------------------------------------------------------------

我们停在了跳转到`64位`入口点——`startup_64`的跳转之前，它在源文件 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S) 里面。在之前的部分，我们已经在`startup_32`里面看到了到`startup_64`的跳转：

```assembly
	pushl	$__KERNEL_CS
	leal	startup_64(%ebp), %eax
	...
	...
	...
	pushl	%eax
	...
	...
	...
	lret
```

由于我们加载了新的`全局描述符表`并且在其他模式有CPU的模式转换（在我们这里是`64位`模式），我们可以在`startup_64`的开头看到数据段的建立：

```assembly
	.code64
	.org 0x200
ENTRY(startup_64)
	xorl	%eax, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs
```

除`cs`之外的段寄存器在我们进入`长模式`时已经重置。

下一步是计算内核编译时的位置和它被加载的位置的差：

```assembly
#ifdef CONFIG_RELOCATABLE
	leaq	startup_32(%rip), %rbp
	movl	BP_kernel_alignment(%rsi), %eax
	decl	%eax
	addq	%rax, %rbp
	notq	%rax
	andq	%rax, %rbp
	cmpq	$LOAD_PHYSICAL_ADDR, %rbp
	jge	1f
#endif
	movq	$LOAD_PHYSICAL_ADDR, %rbp
1:
	movl	BP_init_size(%rsi), %ebx
	subl	$_end, %ebx
	addq	%rbp, %rbx
```

`rbp`包含了解压后内核的起始地址，在这段代码执行之后`rbx`会包含用于解压的重定位内核代码的地址。我们已经在`startup_32`看到类似的代码（你可以看之前的部分[计算重定位地址](https://github.com/0xAX/linux-insides/blob/master/Booting/linux-bootstrap-4.md#calculate-relocation-address)），但是我们需要再做这个计算，因为引导加载器可以用64位引导协议，而`startup_32`在这种情况下不会执行。

下一步，我们可以看到栈指针的设置和标志寄存器的重置：

```assembly
	leaq	boot_stack_end(%rbx), %rsp

	pushq	$0
	popfq
```

如上所述，`rbx`寄存器包含了内核解压代码的起始地址，我们把这个地址的`boot_stack_entry`偏移地址相加放到表示栈顶指针的`rsp`寄存器。在这一步之后，栈就是正确的。你可以在汇编源码文件 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S) 的末尾找到`boot_stack_end`的定义：

```assembly
/*
 * Stack and heap for uncompression
 */
	.bss
	.balign 4
SYM_DATA_LOCAL(boot_heap,	.fill BOOT_HEAP_SIZE, 1, 0)

SYM_DATA_START_LOCAL(boot_stack)
	.fill BOOT_STACK_SIZE, 1, 0
	.balign 16
SYM_DATA_END_LABEL(boot_stack, SYM_L_LOCAL, boot_stack_end)
```

它在`.bss`节的末尾，就在`.pgtable`前面。如果你查看 [arch/x86/boot/compressed/vmlinux.lds.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/vmlinux.lds.S) 链接脚本，你会找到`.bss`和`.pgtable`的定义。

由于我们设置了栈，在我们计算了解压了的内核的重定位地址后，我们可以复制压缩了的内核到以上地址。在查看细节之前，我们先看这段汇编代码：

```assembly
	pushq	%rsi
	leaq	(_bss-8)(%rip), %rsi
	leaq	(_bss-8)(%rbx), %rdi
	movq	$_bss, %rcx
	shrq	$3, %rcx
	std
	rep	movsq
	cld
	popq	%rsi
```
在5.10.13中为：
```asm
/*
 * Copy the compressed kernel to the end of our buffer
 * where decompression in place becomes safe.
 */
	pushq	%rsi
	leaq	(_bss-8)(%rip), %rsi
	leaq	rva(_bss-8)(%rbx), %rdi
	movl	$(_bss - startup_32), %ecx
	shrl	$3, %ecx
	std
	rep	movsq
	cld
	popq	%rsi
```


首先我们把`rsi`压进栈。我们需要保存`rsi`的值，因为这个寄存器现在存放指向`boot_params`的指针，这是包含引导相关数据的实模式结构体（你一定记得这个结构体，我们在开始设置内核的时候就填充了它）。在代码的结尾，我们会重新恢复指向`boot_params`的指针到`rsi`.

接下来两个`leaq`指令用`_bss - 8`偏移和`rip`和`rbx`计算有效地址并存放到`rsi`和`rdi`. 我们为什么要计算这些地址？实际上，压缩了的代码镜像存放在这份复制了的代码（从`startup_32`到当前的代码）和解压了的代码之间。你可以通过查看链接脚本 [arch/x86/boot/compressed/vmlinux.lds.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/vmlinux.lds.S) 验证：

```
SECTIONS
{
	/* Be careful parts of head_64.S assume startup_32 is at
	 * address 0.
	 */
	. = 0;
	.head.text : {  /* __HEAD */
		_head = . ;
		HEAD_TEXT   /*  */
		_ehead = . ;
	}
	.rodata..compressed : {
		*(.rodata..compressed)
	}
	.text :	{
		_text = .; 	/* Text */
		*(.text)
		*(.text.*)
		_etext = . ;
	}
```

注意`.head.text`节包含了`startup_32`. 你可以从之前的部分回忆起它：

```assembly
	__HEAD
	.code32
ENTRY(startup_32)
...
...
...
```

`.text`节包含解压代码：

```assembly
	.text
relocated:
...
...
...
/*
 * Do the decompression, and jump to the new kernel..
 */
...
```

`.rodata..compressed`包含了压缩了的内核镜像。所以`rsi`包含`_bss - 8`的绝对地址，`rdi`包含`_bss - 8`的重定位的相对地址。在我们把这些地址放入寄存器时，我们把`_bss`的地址放到了`rcx`寄存器。正如你在`vmlinux.lds.S`链接脚本中看到了一样，它和设置/内核代码一起在所有节的末尾。现在我们可以开始用`movsq`指令每次8字节地从`rsi`到`rdi`复制代码。

注意在数据复制前有`std`指令：它设置`DF`标志，意味着`rsi`和`rdi`会递减。换句话说，我们会从后往前复制这些字节。最后，我们用`cld`指令清除`DF`标志，并恢复`boot_params`到`rsi`.

现在我们有`.text`节的重定位后的地址，我们可以跳到那里：

```assembly
/*
 * Jump to the relocated address.
 *  现在我们有`.text`节的重定位后的地址，我们可以跳到那里
 */
	leaq	rva(.Lrelocated)(%rbx), %rax
	jmp	*%rax
SYM_CODE_END(startup_64)    /* 结束 */
```

在内核解压前的最后准备
--------------------------------------------------------------------------------

在上一段我们看到了`.text`节从`relocated`标签开始。它做的第一件事是清空`.bss`节：

```assembly
	xorl	%eax, %eax
	leaq    _bss(%rip), %rdi
	leaq    _ebss(%rip), %rcx
	subq	%rdi, %rcx
	shrq	$3, %rcx
	rep	stosq
```

我们要初始化`.bss`节，因为我们很快要跳转到[C](https://en.wikipedia.org/wiki/C_%28programming_language%29)代码。这里我们就清空`eax`，把`_bss`的地址放到`rdi`，把`_ebss`放到`rcx`，然后用`rep stosq`填零。

最后，我们可以调用`extract_kernel`函数：

```assembly
/*
 * Do the extraction, and jump to the new kernel..
 */
	pushq	%rsi			/* Save the real mode argument */
	movq	%rsi, %rdi		/* real mode address */
	leaq	boot_heap(%rip), %rsi	/* malloc area for uncompression */
	leaq	input_data(%rip), %rdx  /* input_data */
	movl	input_len(%rip), %ecx	/* input_len */
	movq	%rbp, %r8		/* output target address */
	movl	output_len(%rip), %r9d	/* decompressed length, end of relocs */
	call	extract_kernel		/* returns kernel location in %rax */
	popq	%rsi
```

我们再一次设置`rdi`为指向`boot_params`结构体的指针并把它保存到栈中。同时我们设置`rsi`指向用于内核解压的区域。最后一步是准备`extract_kernel`的参数并调用这个解压内核的函数。`extract_kernel`函数在 [arch/x86/boot/compressed/misc.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/misc.c) 源文件定义并有六个参数：

* `rmode` - 指向 [boot_params](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973//arch/x86/include/uapi/asm/bootparam.h#L114) 结构体的指针，`boot_params`被引导加载器填充或在早期内核初始化时填充
* `heap` - 指向早期启动堆的起始地址 `boot_heap` 的指针
* `input_data` - 指向压缩的内核，即 `arch/x86/boot/compressed/vmlinux.bin.bz2` 的指针
* `input_len` - 压缩的内核的大小
* `output` - 解压后内核的起始地址
* `output_len` - 解压后内核的大小

所有参数根据 [System V Application Binary Interface](http://www.x86-64.org/documentation/abi.pdf) 通过寄存器传递。我们已经完成了所有的准备工作，现在我们可以看内核解压的过程。

内核解压
--------------------------------------------------------------------------------

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
				  unsigned long output_len)
{
	const unsigned long kernel_total_size = VO__end - VO__text;
	unsigned long virt_addr = LOAD_PHYSICAL_ADDR;
	unsigned long needed_size;

	/* Retain x86 boot parameters pointer passed from startup_32/64. */
	boot_params = rmode;

	/* Clear flags intended for solely in-kernel use. */
	boot_params->hdr.loadflags &= ~KASLR_FLAG;

    //再次执行此操作，因为我们不知道是否以实模式启动，或者是否使用了引导加载程序，
    //32或者64-bit引导加载程序是否使用或引导协议
	sanitize_boot_params(boot_params);

	if (boot_params->screen_info.orig_video_mode == 7) {
		vidmem = (char *) 0xb0000;
		vidport = 0x3b4;
	} else {
		vidmem = (char *) 0xb8000;
		vidport = 0x3d4;
	}

	lines = boot_params->screen_info.orig_video_lines;
	cols = boot_params->screen_info.orig_video_cols;

    //再次执行此操作，因为我们不知道是否以实模式启动，或者是否使用了引导加载程序，
    //32或者64-bit引导加载程序是否使用或引导协议
	console_init();

	/*
	 * Save RSDP address for later use. Have this after console_init()
	 * so that early debugging output from the RSDP parsing code can be
	 * collected.
	 */
	boot_params->acpi_rsdp_addr = get_rsdp_addr();

	debug_putstr("early console in extract_kernel\n");

    //将指针存储到空闲内存的开始和结尾
	free_mem_ptr     = heap;	/* Heap: leaq    boot_heap(%rip), %rsi(arch/x86/boot/compressed/head_64.S) */
	free_mem_end_ptr = heap + BOOT_HEAP_SIZE;

	/*
	 * The memory hole needed for the kernel is the larger of either
	 * the entire decompressed kernel plus relocation table, or the
	 * entire decompressed kernel plus .bss and .brk sections.
	 *
	 * On X86_64, the memory is mapped with PMD pages. Round the
	 * size up so that the full extent of PMD pages mapped is
	 * included in the check against the valid memory table
	 * entries. This ensures the full mapped area is usable RAM
	 * and doesn't include any reserved areas.
	 */
	needed_size = max(output_len, kernel_total_size);
#ifdef CONFIG_X86_64
	needed_size = ALIGN(needed_size, MIN_KERNEL_ALIGN);
#endif

	/* Report initial kernel position details. */
	debug_putaddr(input_data);
	debug_putaddr(input_len);
	debug_putaddr(output);
	debug_putaddr(output_len);
	debug_putaddr(kernel_total_size);
	debug_putaddr(needed_size);

#ifdef CONFIG_X86_64
	/* Report address of 32-bit trampoline */
	debug_putaddr(trampoline_32bit);
#endif

    /* 选择一个存储位置来写入解压缩的内核 */
	choose_random_location((unsigned long)input_data, input_len,
				(unsigned long *)&output,
				needed_size,
				&virt_addr);

    /* 检查所获取的随机地址是否正确对齐 */
	/* Validate memory location choices. */
	if ((unsigned long)output & (MIN_KERNEL_ALIGN - 1))
		error("Destination physical address inappropriately aligned");
	if (virt_addr & (MIN_KERNEL_ALIGN - 1))
		error("Destination virtual address inappropriately aligned");
#ifdef CONFIG_X86_64
	if (heap > 0x3fffffffffffUL)
		error("Destination address too large");
	if (virt_addr + max(output_len, kernel_total_size) > KERNEL_IMAGE_SIZE)
		error("Destination virtual address is beyond the kernel mapping area");
#else
	if (heap > ((-__PAGE_OFFSET-(128<<20)-1) & 0x7fffffff))
		error("Destination address too large");
#endif
#ifndef CONFIG_RELOCATABLE
	if ((unsigned long)output != LOAD_PHYSICAL_ADDR)
		error("Destination address does not match LOAD_PHYSICAL_ADDR");
	if (virt_addr != LOAD_PHYSICAL_ADDR)
		error("Destination virtual address changed when not relocatable");
#endif

    /* 开始解压内核 */
	debug_putstr("\nDecompressing Linux... ");

    /* 解压缩内核: 取决于内核编译过程采用哪种编译方法 */
	__decompress(input_data, input_len, NULL, NULL, output, output_len,
			NULL, error);

    //将解压缩的内核映像移至其在内存中的正确位置, 因为解压缩是就地完成的，
    //我们仍然需要将内核移到正确的地址. 内核映像是ELF可执行文件
	parse_elf(output); //parse_elf功能的主要目标是将可加载的段移动到正确的地址
	handle_relocations(output, output_len, virt_addr); //调整内核映像中的地址
    
	debug_putstr("done.\nBooting the kernel.\n");

	/*
	 * Flush GHCB from cache and map it encrypted again when running as
	 * SEV-ES guest.
	 */
	sev_es_shutdown_ghcb();

	return output;
}
```

就像我们在之前的段落中看到了那样，`extract_kernel`函数在源文件 [arch/x86/boot/compressed/misc.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/misc.c) 定义并有六个参数。正如我们在之前的部分看到的，这个函数从图形/控制台初始化开始。我们要再次做这件事，因为我们不知道我们是不是从[实模式](https://en.wikipedia.org/wiki/Real_mode)开始，或者是使用了引导加载器，或者引导加载器用了32位还是64位启动协议。

在最早的初始化步骤后，我们保存空闲内存的起始和末尾地址。

```C
free_mem_ptr     = heap;
free_mem_end_ptr = heap + BOOT_HEAP_SIZE;
```

在这里 `heap` 是我们在 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S) 得到的 `extract_kernel` 函数的第二个参数：

```assembly
leaq	boot_heap(%rip), %rsi
```

如上所述，`boot_heap`定义为：

```assembly
boot_heap:
	.fill BOOT_HEAP_SIZE, 1, 0
```

在这里`BOOT_HEAP_SIZE`是一个展开为`0x10000`(对`bzip2`内核是`0x400000`)的宏，代表堆的大小。
```c
#if defined(CONFIG_KERNEL_BZIP2)
# define BOOT_HEAP_SIZE		0x400000
#elif defined(CONFIG_KERNEL_ZSTD)
/*
 * Zstd needs to allocate the ZSTD_DCtx in order to decompress the kernel.
 * The ZSTD_DCtx is ~160KB, so set the heap size to 192KB because it is a
 * round number and to allow some slack.
 *
 * 在这里`BOOT_HEAP_SIZE`是一个展开为`0x10000`(对`bzip2`内核是`0x400000`)的宏，代表堆的大小
 */
# define BOOT_HEAP_SIZE		 0x30000
#else
# define BOOT_HEAP_SIZE		 0x10000
#endif
```
在堆指针初始化后，下一步是从 [arch/x86/boot/compressed/kaslr.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/kaslr.c#L425) 调用`choose_random_location`函数。

```c
/*
 * Since this function examines addresses much more numerically,
 * it takes the input and output pointers as 'unsigned long'.
 *
 * 选择了一个存储位置来写入解压缩的内核
 * 需要找到choose压缩的内核映像，甚至在哪里解压缩，这看起来很奇怪，
 * 但是出于安全考虑，Linux内核支持kASLR，它允许将内核解压缩为一个随机地址。
 * 
 * 实际上，随机化内核加载地址的第一步是建立新的身份映射页表。
 *
 * KALSR - Kernel Address Space Layout Randomization 内核地址空间随机分布
 *
 *
 * @input    extract_kernel(input,...) 
 *          此参数从arch/x86/boot/compressed/head_64.S源代码文件通过汇编传递：
 *          leaq    input_data(%rip), %rdx
 * @input_size
 * @output
 * @output_size
 * @virt_addr
 * @
 *
 *  为解压缩内核随机分配了基本物理地址*output和虚拟*virt_addr地址
 */
void choose_random_location(unsigned long input,
			    unsigned long input_size,
			    unsigned long *output,
			    unsigned long output_size,
			    unsigned long *virt_addr)
{
	unsigned long random_addr, min_addr;

    /* 内核​​命令行 nokaslr */
	if (cmdline_find_option_bool("nokaslr")) {
		warn("KASLR disabled: 'nokaslr' on cmdline.");
		return;
	}

    //将kASLR标志添加到内核​​加载标志
	boot_params->hdr.loadflags |= KASLR_FLAG;

	if (IS_ENABLED(CONFIG_X86_32))
		mem_limit = KERNEL_IMAGE_SIZE;
	else
		mem_limit = MAXMEM;

	/* Record the various known unsafe memory ranges. */
    /**
     *  在初始化与身份页表相关的内容之后，我们可以选择一个随机的内存位置来提取内核映像。
     *  但是，正如您可能已经猜到的，我们不能只选择任何地址。有一些重新存储的内存区域，
     *  这些区域被重要的东西占用，例如initrd和内核命令行，必须避免。
     *  该mem_avoid_init功能将帮助我们做到这一点：
     */
	mem_avoid_init(input, input_size, *output);

	/*
	 * Low end of the randomization range should be the
	 * smaller of 512M or the initial kernel image
	 * location:
	 */
	min_addr = min(*output, 512UL << 20);
	/* Make sure minimum is aligned. */
	min_addr = ALIGN(min_addr, CONFIG_PHYSICAL_ALIGN);

	/* Walk available memory entries to find a random address. 
        选择要加载内核的随机物理地址 
        现在，我们有一个随机的物理地址将内核解压缩到该地址 */
	random_addr = find_random_phys_addr(min_addr, output_size);
	if (!random_addr) {
		warn("Physical KASLR disabled: no suitable memory region!");
	} else {
		/* Update the new physical address location. */
		if (*output != random_addr)
			*output = random_addr;
	}


	/* Pick random virtual address starting from LOAD_PHYSICAL_ADDR. */
    //在x86_64架构上随机化虚拟地址
    //在x86_64以外的架构中，随机化的物理和虚拟地址是相同的
	if (IS_ENABLED(CONFIG_X86_64))
		random_addr = find_random_virt_addr(LOAD_PHYSICAL_ADDR/*100000*/, output_size);
	*virt_addr = random_addr;
}
```

我们可以从函数名猜到，它选择内核镜像解压到的内存地址。看起来很奇怪，我们要寻找甚至是`选择`内核解压的地址，但是Linux内核支持[kASLR](https://en.wikipedia.org/wiki/Address_space_layout_randomization)，为了安全，它**允许解压内核到随机的地址**。

在这一部分，我们不会考虑Linux内核的加载地址的随机化，我们会在下一部分讨论。

现在我们回头看 [misc.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/misc.c#L404). 在获得内核镜像的地址后，需要有一些检查以确保获得的随机地址是正确对齐的，并且地址没有错误：

```C
if ((unsigned long)output & (MIN_KERNEL_ALIGN - 1))
	error("Destination physical address inappropriately aligned");

if (virt_addr & (MIN_KERNEL_ALIGN - 1))
	error("Destination virtual address inappropriately aligned");

if (heap > 0x3fffffffffffUL)
	error("Destination address too large");

if (virt_addr + max(output_len, kernel_total_size) > KERNEL_IMAGE_SIZE)
	error("Destination virtual address is beyond the kernel mapping area");

if ((unsigned long)output != LOAD_PHYSICAL_ADDR)
    error("Destination address does not match LOAD_PHYSICAL_ADDR");

if (virt_addr != LOAD_PHYSICAL_ADDR)
	error("Destination virtual address changed when not relocatable");
```

在所有这些检查后，我们可以看到熟悉的消息：

```
Decompressing Linux... 
```

然后调用解压内核的`__decompress`函数：

```C
__decompress(input_data, input_len, NULL, NULL, output, output_len, NULL, error);
```

`__decompress`函数的实现取决于在内核编译期间选择什么压缩算法：

```C
#ifdef CONFIG_KERNEL_GZIP //默认
#include "../../../../lib/decompress_inflate.c"
#endif

#ifdef CONFIG_KERNEL_BZIP2
#include "../../../../lib/decompress_bunzip2.c"
#endif

#ifdef CONFIG_KERNEL_LZMA
#include "../../../../lib/decompress_unlzma.c"
#endif

#ifdef CONFIG_KERNEL_XZ
#include "../../../../lib/decompress_unxz.c"
#endif

#ifdef CONFIG_KERNEL_LZO
#include "../../../../lib/decompress_unlzo.c"
#endif

#ifdef CONFIG_KERNEL_LZ4
#include "../../../../lib/decompress_unlz4.c"
#endif
```
其中gunzip的解压方式为：

```c
#ifndef PREBOOT
STATIC int INIT gunzip(unsigned char *buf, long len,
		       long (*fill)(void*, unsigned long),
		       long (*flush)(void*, unsigned long),
		       unsigned char *out_buf,
		       long *pos,
		       void (*error)(char *x))
{
	return __gunzip(buf, len, fill, flush, out_buf, 0, pos, error);
}
#else
STATIC int INIT __decompress(unsigned char *buf, long len,
			   long (*fill)(void*, unsigned long),
			   long (*flush)(void*, unsigned long),
			   unsigned char *out_buf, long out_len,
			   long *pos,
			   void (*error)(char *x))
{
	return __gunzip(buf, len, fill, flush, out_buf, out_len, pos, error);
}
#endif
```

在内核解压之后，最后两个函数是`parse_elf`和`handle_relocations`.这些函数的主要用途是把解压后的内核移动到正确的位置。事实上，解压过程会[原地](https://en.wikipedia.org/wiki/In-place_algorithm)解压，我们还是要把内核移动到正确的地址。我们已经知道，内核镜像是一个[ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)可执行文件，所以`parse_elf`的主要目标是移动可加载的段到正确的地址。我们可以在`readelf`的输出看到可加载的段：

```c
readelf -l vmlinux

Elf file type is EXEC (Executable file)
Entry point 0x1000000
There are 5 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000200000 0xffffffff81000000 0x0000000001000000
                 0x0000000000893000 0x0000000000893000  R E    200000
  LOAD           0x0000000000a93000 0xffffffff81893000 0x0000000001893000
                 0x000000000016d000 0x000000000016d000  RW     200000
  LOAD           0x0000000000c00000 0x0000000000000000 0x0000000001a00000
                 0x00000000000152d8 0x00000000000152d8  RW     200000
  LOAD           0x0000000000c16000 0xffffffff81a16000 0x0000000001a16000
                 0x0000000000138000 0x000000000029b000  RWE    200000
```
我在 5.10.13中的结果为：
```c
$ readelf -l vmlinux

Elf 文件类型为 EXEC (可执行文件)
入口点 0x1000000
共有 5 个程序头，开始于偏移量64

程序头：
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000200000 0xffffffff81000000 0x0000000001000000
                 0x0000000001b93dc0 0x0000000001b93dc0  R E    200000
  LOAD           0x0000000001e00000 0xffffffff82c00000 0x0000000002c00000
                 0x0000000000fdc000 0x0000000000fdc000  RW     200000
  LOAD           0x0000000002e00000 0x0000000000000000 0x0000000003bdc000
                 0x0000000000036000 0x0000000000036000  RW     200000
  LOAD           0x0000000003012000 0xffffffff83c12000 0x0000000003c12000
                 0x0000000001dee000 0x0000000001dee000  RWE    200000
  NOTE           0x0000000001d93bec 0xffffffff82b93bec 0x0000000002b93bec
                 0x00000000000001d4 0x00000000000001d4         4

 Section to Segment mapping:
  段节...
   00     .text .rodata .pci_fixup .tracedata __ksymtab __ksymtab_gpl __kcrctab __kcrctab_gpl __ksymtab_strings __p
aram __modver __ex_table .notes    01     .data __bug_table .orc_unwind_ip .orc_unwind .orc_lookup .vvar 
   02     .data..percpu 
   03     .init.text .altinstr_aux .init.data .x86_cpu_dev.init .parainstructions .altinstructions .altinstr_replac
ement .iommu_table .apicdrivers .exit.text .exit.data .smp_locks .data_nosave .bss .brk .init.scratch    04     .notes 
```
`parse_elf`函数的目标是加载这些段到从`choose_random_location`函数得到的`output`地址。这个函数从检查ELF签名标志开始：

```C
Elf64_Ehdr ehdr;
Elf64_Phdr *phdrs, *phdr;

memcpy(&ehdr, output, sizeof(ehdr));

if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
    ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
    ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
    ehdr.e_ident[EI_MAG3] != ELFMAG3) {
        error("Kernel is not a valid ELF file");
        return;
}
```

如果是无效的，它会打印一条错误消息并停机。如果我们得到一个有效的`ELF`文件，我们从给定的`ELF`文件遍历所有程序头，并用正确的地址复制所有可加载的段到输出缓冲区：

```C
	for (i = 0; i < ehdr.e_phnum; i++) {
		phdr = &phdrs[i];

		switch (phdr->p_type) {
		case PT_LOAD:
#ifdef CONFIG_RELOCATABLE
			dest = output;
			dest += (phdr->p_paddr - LOAD_PHYSICAL_ADDR);
#else
			dest = (void *)(phdr->p_paddr);
#endif
			memmove(dest, output + phdr->p_offset, phdr->p_filesz);
			break;
		default:
			break;
		}
	}
```

这就是全部的工作。

**从现在开始，所有可加载的段都在正确的位置。**

在`parse_elf`函数之后是调用`handle_relocations`函数。这个函数的实现依赖于`CONFIG_X86_NEED_RELOCS`内核配置选项，如果它被启用，这个函数调整内核镜像的地址，只有在内核配置时启用了`CONFIG_RANDOMIZE_BASE`配置选项才会调用。`handle_relocations`函数的实现足够简单。

```c

#if CONFIG_X86_NEED_RELOCS
/**
 *  调整内核映像中的地址
 *
 *  此函数减去值 LOAD_PHYSICAL_ADDR ,从内核的基本加载地址的值中可以得出内核链接到加载的位置
 *  与内核实际加载的位置之间的差。
 *  此后，由于我们知道内核的实际加载地址，链接运行的地址以及位于内核映像末尾的重定位表，因此可以对内核进行重定位。
 */
static void handle_relocations(void *output, unsigned long output_len,
			       unsigned long virt_addr)
{
	int *reloc;
	unsigned long delta, map, ptr;
	unsigned long min_addr = (unsigned long)output;
	unsigned long max_addr = min_addr + (VO___bss_start - VO__text);

	/*
	 * Calculate the delta between where vmlinux was linked to load
	 * and where it was actually loaded.
	 */
	delta = min_addr - LOAD_PHYSICAL_ADDR/* 1000000 */;

	/*
	 * The kernel contains a table of relocation addresses. Those
	 * addresses have the final load address of the kernel in virtual
	 * memory. We are currently working in the self map. So we need to
	 * create an adjustment for kernel memory addresses to the self map.
	 * This will involve subtracting out the base address of the kernel.
	 */
	map = delta - __START_KERNEL_map/* 0xffffffff80000000 */;

	/*
	 * 32-bit always performs relocations. 64-bit relocations are only
	 * needed if KASLR has chosen a different starting address offset
	 * from __START_KERNEL_map.
	 */
	if (IS_ENABLED(CONFIG_X86_64))
		delta = virt_addr - LOAD_PHYSICAL_ADDR/* 1000000 */;

	if (!delta) {
		debug_putstr("No relocation needed... ");
		return;
	}
	debug_putstr("Performing relocations... ");

	/*
	 * Process relocations: 32 bit relocations first then 64 bit after.
	 * Three sets of binary relocations are added to the end of the kernel
	 * before compression. Each relocation table entry is the kernel
	 * address of the location which needs to be updated stored as a
	 * 32-bit value which is sign extended to 64 bits.
	 *
	 * Format is:
	 *
	 * kernel bits...
	 * 0 - zero terminator for 64 bit relocations
	 * 64 bit relocation repeated
	 * 0 - zero terminator for inverse 32 bit relocations
	 * 32 bit inverse relocation repeated
	 * 0 - zero terminator for 32 bit relocations
	 * 32 bit relocation repeated
	 *
	 * So we work backwards from the end of the decompressed image.
	 */
	for (reloc = output + output_len - sizeof(*reloc); *reloc; reloc--) {
		long extended = *reloc;
		extended += map;

		ptr = (unsigned long)extended;
		if (ptr < min_addr || ptr > max_addr)
			error("32-bit relocation outside of kernel!\n");

		*(uint32_t *)ptr += delta;
	}
#ifdef CONFIG_X86_64
	while (*--reloc) {
		long extended = *reloc;
		extended += map;

		ptr = (unsigned long)extended;
		if (ptr < min_addr || ptr > max_addr)
			error("inverse 32-bit relocation outside of kernel!\n");

		*(int32_t *)ptr -= delta;
	}
	for (reloc--; *reloc; reloc--) {
		long extended = *reloc;
		extended += map;

		ptr = (unsigned long)extended;
		if (ptr < min_addr || ptr > max_addr)
			error("64-bit relocation outside of kernel!\n");

		*(uint64_t *)ptr += delta;
	}
#endif
}
#else
/*  */
#endif
```

这个函数从基准内核加载地址的值减掉`LOAD_PHYSICAL_ADDR`的值，从而我们获得内核链接后要加载的地址和实际加载地址的差值。在这之后我们可以进行内核重定位，因为我们知道内核加载的实际地址、它被链接的运行的地址和内核镜像末尾的重定位表。

在内核重定位后，我们从`extract_kernel`回来，到 [arch/x86/boot/compressed/head_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/boot/compressed/head_64.S).

内核的地址在`rax`寄存器，我们跳到那里：

```assembly
	call	extract_kernel		/* returns kernel location in %rax */
	//从 extract_kernel 返回
	popq	%rsi
/*
 * Jump to the decompressed kernel.
 */
	jmp	*%rax
```

就是这样。现在我们就在内核里！

结论
--------------------------------------------------------------------------------

这是关于内核引导过程的第五部分的结尾。我们不会再看到关于内核引导的文章（可能有这篇和前面的文章的更新），但是会有关于其他内核内部细节的很多文章。

下一章会描述更高级的关于内核引导过程的细节，如加载地址随机化等等。

如果你有什么问题或建议，写个评论或在 [twitter](https://twitter.com/0xAX) 找我。

**如果你发现文中描述有任何问题，请提交一个 PR 到 [linux-insides-zh](https://github.com/MintCN/linux-insides-zh) 。**

链接
--------------------------------------------------------------------------------

* [address space layout randomization](https://en.wikipedia.org/wiki/Address_space_layout_randomization)
* [initrd](https://en.wikipedia.org/wiki/Initrd)
* [long mode](https://en.wikipedia.org/wiki/Long_mode)
* [bzip2](http://www.bzip.org/)
* [RDRand instruction](https://en.wikipedia.org/wiki/RdRand)
* [Time Stamp Counter](https://en.wikipedia.org/wiki/Time_Stamp_Counter)
* [Programmable Interval Timers](https://en.wikipedia.org/wiki/Intel_8253)
* [Previous part](https://github.com/0xAX/linux-insides/blob/master/Booting/linux-bootstrap-4.md)
