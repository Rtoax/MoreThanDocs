<center><font size='5'>Linux内核深入理解系统调用（2）</font></center>
<center><font size='6'>vsyscall 和 vDSO 以及程序是如何运行的（execve）</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>



# 1. vsyscalls 和 vDSO

这是讲解 Linux 内核中系统调用[章节](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/index.html)的第三部分，[前一节](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-2.html)讨论了用户空间应用程序发起的系统调用的准备工作及系统调用的处理过程。在这一节将讨论两个与系统调用十分相似的概念，这两个概念是`vsyscall` 和 `vdso`。

我们已经了解什么是`系统调用`。这是 Linux 内核一种特殊的运行机制，使得用户空间的应用程序可以请求，像写入文件和打开套接字等特权级下的任务。正如你所了解的，在 Linux 内核中发起一个系统调用是特别昂贵的操作，因为处理器需要中断当前正在执行的任务，切换内核模式的上下文，在系统调用处理完毕后跳转至用户空间。以下的两种机制  - **`vsyscall` 和d `vdso` 被设计用来加速系统调用的处理**，在这一节我们将了解两种机制的工作原理。

## 1.1. vsyscalls 介绍

 `vsyscall` 或 `virtual system call` 是第一种也是最古老的一种用于加快系统调用的机制。 `vsyscall` 的工作原则其实十分简单。Linux 内核在用户空间映射一个包含一些变量及一些系统调用的实现的内存页。 对于 [X86_64](https://en.wikipedia.org/wiki/X86-64) 架构可以在 Linux 内核的 [文档] (https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt) 找到关于这一内存区域的信息：

```
ffffffffff600000 - ffffffffffdfffff (=8 MB) vsyscalls
```

或:

```
~$ sudo cat /proc/1/maps | grep vsyscall
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
```

因此, 这些系统调用将在用户空间下执行，这意味着将不发生 [上下文切换](https://en.wikipedia.org/wiki/Context_switch)。 `vsyscall` 内存页的映射在 [arch/x86/entry/vsyscall/vsyscall_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vsyscall/vsyscall_64.c) 源代码中定义的 `map_vsyscall` 函数中实现。这一函数在 Linux 内核初始化时被 [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup.c) 源代码中定义的函数`setup_arch` (我们在[第五章](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-5.html) Linux 内核的初始化中讨论过该函数)。

注意 `map_vsyscall` 函数的实现依赖于内核配置选项 `CONFIG_X86_VSYSCALL_EMULATION` :

```C
#ifdef CONFIG_X86_VSYSCALL_EMULATION
extern void map_vsyscall(void);
#else
static inline void map_vsyscall(void) {}
#endif
```

正如帮助文档中所描述的,  `CONFIG_X86_VSYSCALL_EMULATION` 配置选项: `使能 vsyscall 模拟`. 为何模拟 `vsyscall`? 事实上,  `vsyscall` 由于安全原因是一种遗留 [ABI](https://en.wikipedia.org/wiki/Application_binary_interface) 。虚拟系统调用具有绑定的地址, 意味着 `vsyscall` 的内存页的位置在任何时刻是相同，这一位置是在  `map_vsyscall` 函数中指定的。这一函数的实现如下: 

```C
void __init map_vsyscall(void)
{
    extern char __vsyscall_page;
    unsigned long physaddr_vsyscall = __pa_symbol(&__vsyscall_page);
	...
	...
	...
}
```

在 `map_vsyscall` 函数的开始，通过宏 `__pa_symbol` 获取了 `vsyscall` 内存页的物理地址(我们已在[第四章](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-4.html) of the Linux kernel initialization process)讨论了该宏的实现）。`__vsyscall_page` 在  [arch/x86/entry/vsyscall/vsyscall_emu_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vsyscall/vsyscall_emu_64.S) 汇编源代码文件中定义， 具有如下的 [虚拟地址](https://en.wikipedia.org/wiki/Virtual_address_space):

```
ffffffff81881000 D __vsyscall_page
```

在5.10.13编译结果可以查到：
```c
[rongtao@localhost src]$ grep __vsyscall_page -rn
arch/x86/kernel/vmlinux.lds.S:199:         *  ffffffff82c09000 D __vsyscall_page
arch/x86/entry/vsyscall/vsyscall_64.c:380:	extern char __vsyscall_page;
arch/x86/entry/vsyscall/vsyscall_64.c:382:    //stores physical address of the `__vsyscall_page` symbol
arch/x86/entry/vsyscall/vsyscall_64.c:384:	unsigned long physaddr_vsyscall = __pa_symbol(&__vsyscall_page);
arch/x86/entry/vsyscall/vsyscall_emu_64.S:15:	.globl __vsyscall_page  /* ffffffff82c09000 */
arch/x86/entry/vsyscall/vsyscall_emu_64.S:17:	.type __vsyscall_page, @object
arch/x86/entry/vsyscall/vsyscall_emu_64.S:18:__vsyscall_page:    /* ffffffff82c09000 */
arch/x86/entry/vsyscall/vsyscall_emu_64.S:36:	.size __vsyscall_page, 4096
System.map:87735:ffffffff82c09000 D __vsyscall_page
```

在 `.data..page_aligned, aw` [段](https://en.wikipedia.org/wiki/Memory_segmentation) 中 包含如下三中系统调用: 

* `gettimeofday`;
* `time`;
* `getcpu`.

或:

```assembly
__vsyscall_page:
	mov $__NR_gettimeofday, %rax
	syscall
	ret

	.balign 1024, 0xcc
	mov $__NR_time, %rax
	syscall
	ret

	.balign 1024, 0xcc
	mov $__NR_getcpu, %rax
	syscall
	ret
```

回到 `map_vsyscall` 函数及 `__vsyscall_page` 的实现，在得到 `__vsyscall_page` 的物理地址之后，使用 `__set_fixmap` 为  `vsyscall` 内存页 检查设置 [fix-mapped](http://xinqiu.gitbooks.io/linux-insides-cn/content/MM/linux-mm-2.html)地址的变量`vsyscall_mode`:

```C
if (vsyscall_mode != NONE)
	__set_fixmap(VSYSCALL_PAGE, physaddr_vsyscall,
                 vsyscall_mode == NATIVE
                             ? PAGE_KERNEL_VSYSCALL
                             : PAGE_KERNEL_VVAR);
```

`__set_fixmap` 三个参数: 

第一个是 `fixed_addresses` [enum](https://en.wikipedia.org/wiki/Enumerated_type)的索引. 在我们的情况中`VSYSCALL_PAGE` 是 `fixed_addresses` 枚举的第一个元素（`x86_64`架构）:

```C
enum fixed_addresses {
#ifdef CONFIG_X86_VSYSCALL_EMULATION
	VSYSCALL_PAGE = (FIXADDR_TOP - VSYSCALL_ADDR) >> PAGE_SHIFT,
#endif
    ...
```

该变量值为 `511`。

第二个参数为映射内存页的物理地址，第三个参数为内存页的标志位。注意 `VSYSCALL_PAGE` 标志位依赖于变量 `vsyscall_mode` 。当 `vsyscall_mode` 变量为 `NATIVE` 时，  标志位为 `PAGE_KERNEL_VSYSCALL`，其他情况则是`PAGE_KERNEL_VVAR` 。两个宏 ( `PAGE_KERNEL_VSYSCALL` 及 `PAGE_KERNEL_VVAR`) 都将被扩展以下标志:

```C
#define __PAGE_KERNEL_VSYSCALL          (__PAGE_KERNEL_RX | _PAGE_USER)
#define __PAGE_KERNEL_VVAR              (__PAGE_KERNEL_RO | _PAGE_USER)
```

标志反映了 `vsyscall` 内存页的访问权限。两个标志都带有 `_PAGE_USER` 标志， 这意味着内存页可被运行于低特权级的用户模式进程访问。第二个标志位取决于 `vsyscall_mode` 变量的值。第一个标志  (`__PAGE_KERNEL_VSYSCALL`) 在 `vsyscall_mode` 为 `NATIVE` 时被设定。这意味着虚拟系统调用将以本地 `syscall` 指令的方式执行。另一情况下，在 `vsyscall_mode` 为 `emulate` 时 vsyscall  为 `PAGE_KERNEL_VVAR`，此时系统调用将被置于陷阱并被合理的模拟。  `vsyscall_mode` 变量通过 `vsyscall_setup` 获取值： 

```C
static int __init vsyscall_setup(char *str)
{
	if (str) {
		if (!strcmp("emulate", str))
			vsyscall_mode = EMULATE;
		else if (!strcmp("native", str))
			vsyscall_mode = NATIVE;
		else if (!strcmp("none", str))
			vsyscall_mode = NONE;
		else
			return -EINVAL;

		return 0;
	}

	return -EINVAL;
}
early_param("vsyscall", vsyscall_setup);
```

函数将在早期的内核分析时被调用：

```C
early_param("vsyscall", vsyscall_setup);
```

关于 `early_param` 宏的更多信息可以在[第六章](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html) Linux 内核初始化中找到。

在函数 `vsyscall_map` 的最后仅通过 [BUILD_BUG_ON](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-1.html) 宏检查  `vsyscall` 内存页的虚拟地址是否等于变量 `VSYSCALL_ADDR` :

```C
BUILD_BUG_ON((unsigned long)__fix_to_virt(VSYSCALL_PAGE) !=
             (unsigned long)VSYSCALL_ADDR);
```

 就这样`vsyscall` 内存页设置完毕。上述的结果如下： 若设置 `vsyscall=native` 内核命令行参数，虚拟内存调用将以 [arch/x86/entry/vsyscall/vsyscall_emu_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vsyscall/vsyscall_emu_64.S) 文件中本地 `系统调用` 指令的方式执行。  [glibc](https://en.wikipedia.org/wiki/GNU_C_Library) 知道虚拟系统调用处理器的地址。注意虚拟系统调用的地址以 `1024` (或 `0x400`) 比特对齐。

```assembly
__vsyscall_page:
	mov $__NR_gettimeofday, %rax
	syscall
	ret

	.balign 1024, 0xcc
	mov $__NR_time, %rax
	syscall
	ret

	.balign 1024, 0xcc
	mov $__NR_getcpu, %rax
	syscall
	ret
```

 `vsyscall` 内存页的起始地址为 `ffffffffff600000` 。因此,  [glibc](https://en.wikipedia.org/wiki/GNU_C_Library) 知道所有虚拟系统调用处理器的地址。可以在 `glibc` 源码中找到这些地址的定义：

```C
#define VSYSCALL_ADDR_vgettimeofday   0xffffffffff600000
#define VSYSCALL_ADDR_vtime 	      0xffffffffff600400
#define VSYSCALL_ADDR_vgetcpu	      0xffffffffff600800
```

所有的虚拟系统调用请求都将映射至 `__vsyscall_page` + `VSYSCALL_ADDR_vsyscall_name` 偏置, 将虚拟内存系统调用的编号置于通用目的[寄存器](https://en.wikipedia.org/wiki/Processor_register)，本地的 x86_64 `系统调用`指令将被执行。

在第二种情况中, 若将 `vsyscall=emulate` 参数传递给内核命令行, 提升虚拟系统调用处理器的尝试导致一个 [page fault](https://en.wikipedia.org/wiki/Page_fault) 异常。 谨记,  `vsyscall` 内存页 具有 `__PAGE_KERNEL_VVAR` 的访问权限，这将禁止执行。 `do_page_fault` 函数是 `#PF` 或 page fault 的处理器。它将尝试了解最后一次 page fault 的原因。一种可能的场景是 `vsyscall` 模式为 `emulate` 情况下的虚拟系统调用。此时 `vsyscall` 将被 [arch/x86/entry/vsyscall/vsyscall_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vsyscall/vsyscall_64.c) 源码中定义的 `emulate_vsyscall` 函数处理。

`emulate_vsyscall`函数 获取系统调用号并检测, 打印错误 [segementation fault](https://en.wikipedia.org/wiki/Segmentation_fault) :

```C
...
    vsyscall_nr = addr_to_vsyscall_nr(address);
    if (vsyscall_nr < 0) {
    	warn_bad_vsyscall(KERN_WARNING, regs, "misaligned vsyscall...");
    	goto sigsegv;
    }
...
sigsegv:
	force_sig(SIGSEGV, current);
	reutrn true;
```

As it checked number of a virtual system call, it does some yet another checks like `access_ok` violations and execute system call function depends on the number of a virtual system call:

```C
	switch (vsyscall_nr) {
	case 0:
		/* this decodes regs->di and regs->si on its own */
		ret = __x64_sys_gettimeofday(regs);
		break;

	case 1:
		/* this decodes regs->di on its own */
		ret = __x64_sys_time(regs);
		break;

	case 2:
		/* while we could clobber regs->dx, we didn't in the past... */
		orig_dx = regs->dx;
		regs->dx = 0;
		/* this decodes regs->di, regs->si and regs->dx on its own */
		ret = __x64_sys_getcpu(regs);
		regs->dx = orig_dx;
		break;
	}
```

In the end we put the result of the `sys_gettimeofday` or another virtual system call handler to the `ax` general purpose register, as we did it with the normal system calls and restore the [instruction pointer](https://en.wikipedia.org/wiki/Program_counter) register and add `8` bytes to the [stack pointer](https://en.wikipedia.org/wiki/Stack_register) register. This operation emulates `ret` instruction.

```C
	regs->ax = ret;

do_ret:
	regs->ip = caller;
	regs->sp += 8;
	return true;
```		

That's all. Now let's look on the modern concept - `vDSO`.

## 1.2. vDSO 介绍

> vsyscall已经被VDSO替代。

As I already wrote above, `vsyscall` is an obsolete concept and replaced by the `vDSO` or **`virtual dynamic shared object`**. The main difference between the `vsyscall` and `vDSO` mechanisms is that `vDSO` maps memory pages into each process in a shared object [form](https://en.wikipedia.org/wiki/Library_%28computing%29#Shared_libraries), but `vsyscall` is static in memory and has the same address every time. For the `x86_64` architecture it is called -`linux-vdso.so.1`. All userspace applications linked with this shared library via the `glibc`. For example:

```
~$ ldd /bin/uname
	linux-vdso.so.1 (0x00007ffe014b7000)
	libc.so.6 => /lib64/libc.so.6 (0x00007fbfee2fe000)
	/lib64/ld-linux-x86-64.so.2 (0x00005559aab7c000)
```

Or:

```
~$ sudo cat /proc/1/maps | grep vdso
7fff39f73000-7fff39f75000 r-xp 00000000 00:00 0       [vdso]
```

Here we can see that [uname](https://en.wikipedia.org/wiki/Uname) util was linked with the three libraries:

* `linux-vdso.so.1`;
* `libc.so.6`;
* `ld-linux-x86-64.so.2`.

The first provides `vDSO` functionality, the second is `C` [standard library](https://en.wikipedia.org/wiki/C_standard_library) and the third is the program interpreter (more about this you can read in the part that describes [linkers](https://xinqiu.gitbooks.io/linux-insides-cn/content/Misc/linux-misc-3.html)). So, the `vDSO` solves limitations of the `vsyscall`. Implementation of the `vDSO` is similar to `vsyscall`.

Initialization of the `vDSO` occurs in the `init_vdso` function that defined in the [arch/x86/entry/vdso/vma.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vdso/vma.c) source code file. This function starts from the initialization of the `vDSO` images for 32-bits and 64-bits depends on the `CONFIG_X86_X32_ABI` kernel configuration option:

```C
static int __init init_vdso(void)
{
	init_vdso_image(&vdso_image_64);

#ifdef CONFIG_X86_X32_ABI
	init_vdso_image(&vdso_image_x32);
#endif
	return 0;
}
subsys_initcall(init_vdso);
```

Both function initialize the `vdso_image` structure. This structure is defined in the two generated source code files: the [arch/x86/entry/vdso/vdso-image-64.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vdso/vdso-image-64.c) and the [arch/x86/entry/vdso/vdso-image-64.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vdso/vdso-image-64.c). These source code files generated by the [vdso2c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vdso/vdso2c.c) program from the different source code files, represent different approaches to call a system call like `int 0x80`, `sysenter` and etc. The full set of the images depends on the kernel configuration.

For example for the `x86_64` Linux kernel it will contain `vdso_image_64`:

```C
#ifdef CONFIG_X86_64
extern const struct vdso_image vdso_image_64;
#endif
```

But for the `x86` - `vdso_image_32`:

```C
#ifdef CONFIG_X86_X32
extern const struct vdso_image vdso_image_x32;
#endif
```

If our kernel is configured for the `x86` architecture or for the `x86_64` and compability mode, we will have ability to call a system call with the `int 0x80` interrupt, if compability mode is enabled, we will be able to call a system call with the native `syscall instruction` or `sysenter` instruction in other way:

```C
#if defined CONFIG_X86_32 || defined CONFIG_COMPAT
  extern const struct vdso_image vdso_image_32_int80;
#ifdef CONFIG_COMPAT
  extern const struct vdso_image vdso_image_32_syscall;
#endif
 extern const struct vdso_image vdso_image_32_sysenter;
#endif
```

As we can understand from the name of the `vdso_image` structure, it represents image of the `vDSO` for the certain mode of the system call entry. This structure contains information about size in bytes of the `vDSO` area that always a multiple of `PAGE_SIZE` (`4096` bytes), pointer to the text mapping, start and end address of the `alternatives` (set of instructions with better alternatives for the certain type of the processor) and etc. For example `vdso_image_64` looks like this:

```C
const struct vdso_image vdso_image_64 = {
	.data = raw_data,
	.size = 8192,
	.text_mapping = {
		.name = "[vdso]",
		.pages = pages,
	},
	.alt = 3145,
	.alt_len = 26,
	.sym_vvar_start = -8192,
	.sym_vvar_page = -8192,
	.sym_hpet_page = -4096,
};
```

Where the `raw_data` contains raw binary code of the 64-bit `vDSO` system calls which are `2` page size:

```C
static struct page *pages[2];
```

or 8 Kilobytes.

The `init_vdso_image` function is defined in the same source code file and just initializes the `vdso_image.text_mapping.pages`. First of all this function calculates the number of pages and initializes each `vdso_image.text_mapping.pages[number_of_page]` with the `virt_to_page` macro that converts given address to the `page` structure:

```C
void __init init_vdso_image(const struct vdso_image *image)
{
	int i;
	int npages = (image->size) / PAGE_SIZE;

	for (i = 0; i < npages; i++)
		image->text_mapping.pages[i] =
			virt_to_page(image->data + i*PAGE_SIZE);
	...
	...
	...
}
```

The `init_vdso` function passed to the `subsys_initcall` macro adds the given function to the `initcalls` list. All functions from this list will be called in the `do_initcalls` function from the [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) source code file:

```C
subsys_initcall(init_vdso);
```

Ok, we just saw initialization of the `vDSO` and initialization of `page` structures that are related to the memory pages that contain `vDSO` system calls. But to where do their pages map? Actually they are mapped by the kernel, when it loads binary to the memory. The Linux kernel calls the `arch_setup_additional_pages` function from the [arch/x86/entry/vdso/vma.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/vdso/vma.c) source code file that checks that `vDSO` enabled for the `x86_64` and calls the `map_vdso` function:

```C
int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	if (!vdso64_enabled)
		return 0;

	return map_vdso(&vdso_image_64, true);
}
```

The `map_vdso` function is defined in the same source code file and maps pages for the `vDSO` and for the shared `vDSO` variables. That's all. The main differences between the `vsyscall` and the `vDSO` concepts is that `vsyscal` has a static address of `ffffffffff600000` and implements `3` system calls, whereas the `vDSO` loads dynamically and implements four system calls:

* `__vdso_clock_gettime`;
* `__vdso_getcpu`;
* `__vdso_gettimeofday`;
* `__vdso_time`.


That's all.

## 1.3. 结论

This is the end of the third part about the system calls concept in the Linux kernel. In the previous [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-2.html) we discussed the implementation of the preparation from the Linux kernel side, before a system call will be handled and implementation of the `exit` process from a system call handler. In this part we continued to dive into the stuff which is related to the system call concept and learned two new concepts that are very similar to the system call - the `vsyscall` and the `vDSO`.

After all of these three parts, we know almost all things that are related to system calls, we know what system call is and why user applications need them.  We also know what occurs when a user application calls a system call and how the kernel handles system calls.

The next part will be the last part in this [chapter](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/index.html) and we will see what occurs when a user runs the program.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](anotherworldofworld@gmail.com) or just create [issue](https://github.com/MintCN/linux-insides-zh/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/MintCN/linux-insides-zh).**

## 1.4. 链接

* [x86_64 memory map](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [context switching](https://en.wikipedia.org/wiki/Context_switch)
* [ABI](https://en.wikipedia.org/wiki/Application_binary_interface)
* [virtual address](https://en.wikipedia.org/wiki/Virtual_address_space)
* [Segmentation](https://en.wikipedia.org/wiki/Memory_segmentation)
* [enum](https://en.wikipedia.org/wiki/Enumerated_type)
* [fix-mapped addresses](http://xinqiu.gitbooks.io/linux-insides-cn/content/MM/linux-mm-2.html)
* [glibc](https://en.wikipedia.org/wiki/GNU_C_Library)
* [BUILD_BUG_ON](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-1.html)
* [Processor register](https://en.wikipedia.org/wiki/Processor_register)
* [Page fault](https://en.wikipedia.org/wiki/Page_fault)
* [segementation fault](https://en.wikipedia.org/wiki/Segmentation_fault)
* [instruction pointer](https://en.wikipedia.org/wiki/Program_counter)
* [stack pointer](https://en.wikipedia.org/wiki/Stack_register)
* [uname](https://en.wikipedia.org/wiki/Uname)
* [Linkers](https://xinqiu.gitbooks.io/linux-insides-cn/content/Misc/linux-misc-3.html)
* [Previous part](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-2.html)



# 2. Linux内核如何启动程序

This is the fourth part of the [chapter](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/index.html) that describes [system calls](https://en.wikipedia.org/wiki/System_call) in the Linux kernel and as I wrote in the conclusion of the [previous](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-3.html) - this part will be last in this chapter. In the previous part we stopped at the two new concepts:

* `vsyscall`;
* `vDSO`;

that are related and very similar on system call concept.

This part will be last part in this chapter and as you can understand from the part's title - we will see what does occur in the Linux kernel when we run our programs. So, let's start.

## 2.1. how do we launch our programs?

There are many different ways to launch an application from an user perspective. For example we can run a program from the [shell](https://en.wikipedia.org/wiki/Unix_shell) or double-click on the application icon. It does not matter. The Linux kernel handles application launch regardless how we do launch this application.

In this part we will consider the way when we just launch an application from the `shell`. As you know, the standard way to launch an application from shell is the following: We just launch a [terminal emulator](https://en.wikipedia.org/wiki/Terminal_emulator) application and just write the name of the program and pass or not arguments to our program, for example:

```c
[rongtao@localhost linux-5.10.13]$ ls --version
ls (GNU coreutils) 8.22
Copyright (C) 2013 Free Software Foundation, Inc.
许可证：GPLv3+：GNU 通用公共许可证第3 版或更新版本<http://gnu.org/licenses/gpl.html>。
本软件是自由软件：您可以自由修改和重新发布它。
在法律范围内没有其他保证。

由Richard M. Stallman 和David MacKenzie 编写。
```

Let's consider what does occur when we launch an application from the shell, what does shell do when we write program name, what does Linux kernel do etc. But before we will start to consider these interesting things, I want to warn that this book is about the Linux kernel. That's why we will see Linux kernel insides related stuff mostly in this part. We will not consider in details what does shell do, we will not consider complex cases, for example subshells etc.

My default shell is - [bash](https://en.wikipedia.org/wiki/Bash_%28Unix_shell%29), so I will consider how do bash shell launches a program. So let's start. The `bash` shell as well as any program that written with [C](https://en.wikipedia.org/wiki/C_%28programming_language%29) programming language starts from the [main](https://en.wikipedia.org/wiki/Entry_point) function. If you will look on the source code of the `bash` shell, you will find the `main` function in the [shell.c](https://github.com/bminor/bash/blob/master/shell.c#L357) source code file. This function makes many different things before the main thread loop of the `bash` started to work. For example this function:

* 打开tty；
* 检测是否是debug模式；
* 解析命令行；
* 读取shell环境变量；
* 加载`.bashrc`, `.profile`等配置文件；
* 其他。

* checks and tries to open `/dev/tty`;
* check that shell running in debug mode;
* parses command line arguments;
* reads shell environment;
* loads `.bashrc`, `.profile` and other configuration files;
* and many many more.

After all of these operations we can see the call of the `reader_loop` function. This function defined in the [eval.c](https://github.com/bminor/bash/blob/master/eval.c#L67) source code file and represents main thread loop or in other words it reads and executes commands. As the `reader_loop` function made all checks and read the given program name and arguments, it calls the `execute_command` function from the [execute_cmd.c](https://github.com/bminor/bash/blob/master/execute_cmd.c#L378) source code file. The `execute_command` function through the chain of the functions calls:

```
execute_command
--> execute_command_internal
----> execute_simple_command
------> execute_disk_command
--------> shell_execve
```

makes different checks like do we need to start `subshell`, was it builtin `bash` function or not etc. As I already wrote above, we will not consider all details about things that are not related to the Linux kernel. In the end of this process, the `shell_execve` function calls the `execve` system call:

```C
execve (command, args, env);
```

The `execve` system call has the following signature:

```
int execve(const char *filename, char *const argv [], char *const envp[]);   
```

and executes a program by the given filename, with the given arguments and [environment variables](https://en.wikipedia.org/wiki/Environment_variable). This system call is the first in our case and only, for example:

```
$ strace ls
execve("/bin/ls", ["ls"], [/* 62 vars */]) = 0

$ strace echo
execve("/bin/echo", ["echo"], [/* 62 vars */]) = 0

$ strace uname
execve("/bin/uname", ["uname"], [/* 62 vars */]) = 0
```

So, an user application (`bash` in our case) calls the system call and as we already know the next step is Linux kernel.

## 2.2. execve system call

We saw preparation before a system call called by an user application and after a system call handler finished its work in the second [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-2.html) of this chapter. We stopped at the call of the `execve` system call in the previous paragraph. This system call defined in the [fs/exec.c](https://github.com/torvalds/linux/blob/master/fs/exec.c) source code file and as we already know it takes three arguments:

```
SYSCALL_DEFINE3(execve,
		const char __user *, filename,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp)
{
	return do_execve(getname(filename), argv, envp);
}
```

Implementation of the `execve` is pretty simple here, as we can see it just returns the result of the `do_execve` function. The `do_execve` function defined in the same source code file and do the following things:

* Initialize two pointers on a userspace data with the given arguments and environment variables;
* return the result of the `do_execveat_common`.

We can see its implementation:

```C
struct user_arg_ptr argv = { .ptr.native = __argv };
struct user_arg_ptr envp = { .ptr.native = __envp };
return do_execveat_common(AT_FDCWD, filename, argv, envp, 0);
```

The `do_execveat_common` function does main work - it executes a new program. This function takes similar set of arguments, but as you can see it takes five arguments instead of three. The first argument is the file descriptor that represent directory with our application, in our case the `AT_FDCWD` means that the given pathname is interpreted relative to the current working directory of the calling process. The fifth argument is flags. In our case we passed `0` to the `do_execveat_common`. We will check in a next step, so will see it latter.

First of all the `do_execveat_common` function checks the `filename` pointer and returns if it is `NULL`. After this we check flags of the current process that limit of running processes is not exceed:

```C
if (IS_ERR(filename))
	return PTR_ERR(filename);

if ((current->flags & PF_NPROC_EXCEEDED) &&
	atomic_read(&current_user()->processes) > rlimit(RLIMIT_NPROC)) {
	retval = -EAGAIN;
	goto out_ret;
}

current->flags &= ~PF_NPROC_EXCEEDED;
```

If these two checks were successful we unset `PF_NPROC_EXCEEDED` flag in the flags of the current process to prevent fail of the `execve`. You can see that in the next step we call the `unshare_files` function that defined in the [kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c) and unshares the files of the current task and check the result of this function:

```C
retval = unshare_files(&displaced);
if (retval)
	goto out_ret;
```

We need to call this function to eliminate potential leak of the execve'd binary's [file descriptor](https://en.wikipedia.org/wiki/File_descriptor). In the next step we start preparation of the `bprm` that represented by the `struct linux_binprm` structure (defined in the [include/linux/binfmts.h](https://github.com/torvalds/linux/blob/master/linux/binfmts.h) header file). The `linux_binprm` structure is used to hold the arguments that are used when loading binaries. For example it contains `vma` field which has `vm_area_struct` type and represents single memory area over a contiguous interval in a given address space where our application will be loaded, `mm` field which is memory descriptor of the binary, pointer to the top of memory and many other different fields.

First of all we allocate memory for this structure with the `kzalloc` function and check the result of the allocation:

```C
bprm = kzalloc(sizeof(*bprm), GFP_KERNEL);
if (!bprm)
	goto out_files;
```

After this we start to prepare the `binprm` credentials with the call of the `prepare_bprm_creds` function:

```C
retval = prepare_bprm_creds(bprm);
	if (retval)
		goto out_free;

check_unsafe_exec(bprm);
current->in_execve = 1;
```

Initialization of the `binprm` credentials in other words is initialization of the `cred` structure that stored inside of the `linux_binprm` structure. The `cred` structure contains the security context of a task for example [real uid](https://en.wikipedia.org/wiki/User_identifier#Real_user_ID) of the task, real [guid](https://en.wikipedia.org/wiki/Globally_unique_identifier) of the task, `uid` and `guid` for the [virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system) operations etc. In the next step as we executed preparation of the `bprm` credentials we check that now we can safely execute a program with the call of the `check_unsafe_exec` function and set the current process to the `in_execve` state.

After all of these operations we call the `do_open_execat` function that checks the flags that we passed to the `do_execveat_common` function (remember that we have `0` in the `flags`) and searches and opens executable file on disk, checks that our we will load a binary file from `noexec` mount points (we need to avoid execute a binary from filesystems that do not contain executable binaries like [proc](https://en.wikipedia.org/wiki/Procfs) or [sysfs](https://en.wikipedia.org/wiki/Sysfs)), intializes `file` structure and returns pointer on this structure. Next we can see the call the `sched_exec` after this:

```C
file = do_open_execat(fd, filename, flags);
retval = PTR_ERR(file);
if (IS_ERR(file))
	goto out_unmark;

sched_exec();
```

The `sched_exec` function is used to determine the least loaded processor that can execute the new program and to migrate the current process to it.

After this we need to check [file descriptor](https://en.wikipedia.org/wiki/File_descriptor) of the give executable binary. We try to check does the name of the our binary file starts from the `/` symbol or does the path of the given executable binary is interpreted relative to the current working directory of the calling process or in other words file descriptor is `AT_FDCWD` (read above about this).

If one of these checks is successfull we set the binary parameter filename:

```C
bprm->file = file;

if (fd == AT_FDCWD || filename->name[0] == '/') {
	bprm->filename = filename->name;
}
```

Otherwise if the filename is empty we set the binary parameter filename to the `/dev/fd/%d` or `/dev/fd/%d/%s` depends on the filename of the given executable binary which means that we will execute the file to which the file descriptor refers:

```C
} else {
	if (filename->name[0] == '\0')
		pathbuf = kasprintf(GFP_TEMPORARY, "/dev/fd/%d", fd);
	else
		pathbuf = kasprintf(GFP_TEMPORARY, "/dev/fd/%d/%s",
		                    fd, filename->name);
	if (!pathbuf) {
		retval = -ENOMEM;
		goto out_unmark;
	}

	bprm->filename = pathbuf;
}

bprm->interp = bprm->filename;
```

Note that we set not only the `bprm->filename` but also `bprm->interp` that will contain name of the program interpreter. For now we just write the same name there, but later it will be updated with the real name of the program interpreter depends on binary format of a program. You can read above that we already prepared `cred` for the `linux_binprm`. The next step is initalization of other fields of the `linux_binprm`.  First of all we call the `bprm_mm_init` function and pass the `bprm` to it:

```C
retval = bprm_mm_init(bprm);
if (retval)
	goto out_unmark;
```

The `bprm_mm_init` defined in the same source code file and as we can understand from the function's name, it makes initialization of the memory descriptor or in other words the `bprm_mm_init` function initializes `mm_struct` structure. This structure defined in the [include/linux/mm_types.h](https://github.com/torvalds/linux/blob/master/include/mm_types.h) header file and represents address space of a process. We will not consider implementation of the `bprm_mm_init` function because we do not know many important stuff related to the Linux kernel memory manager, but we just need to know that this function initializes `mm_struct` and populate it with a temporary stack `vm_area_struct`.

After this we calculate the count of the command line arguments which are were passed to the our executable binary, the count of the environment variables and set it to the `bprm->argc` and `bprm->envc` respectively:

```C
bprm->argc = count(argv, MAX_ARG_STRINGS);
if ((retval = bprm->argc) < 0)
	goto out;

bprm->envc = count(envp, MAX_ARG_STRINGS);
if ((retval = bprm->envc) < 0)
	goto out;
```

As you can see we do this operations with the help of the `count` function that defined in the [same](https://github.com/torvalds/linux/blob/master/fs/exec.c) source code file and calculates the count of strings in the `argv` array. The `MAX_ARG_STRINGS` macro defined in the [include/uapi/linux/binfmts.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/binfmts.h) header file and as we can understand from the macro's name, it represents maximum number of strings that were passed to the `execve` system call. The value of the `MAX_ARG_STRINGS`:

```C
#define MAX_ARG_STRINGS 0x7FFFFFFF
```

After we calculated the number of the command line arguments and environment variables, we call the `prepare_binprm` function. We already call the function with the similar name before this moment. This function is called `prepare_binprm_cred` and we remember that this function initializes `cred` structure in the `linux_bprm`. Now the `prepare_binprm` function:

```C
retval = prepare_binprm(bprm);
if (retval < 0)
	goto out;
```

fills the `linux_binprm` structure with the `uid` from [inode](https://en.wikipedia.org/wiki/Inode) and read `128` bytes from the binary executable file. We read only first `128` from the executable file because we need to check a type of our executable. We will read the rest of the executable file in the later step. After the preparation of the `linux_bprm` structure we copy the filename of the executable binary file, command line arguments and enviroment variables to the `linux_bprm` with the call of the `copy_strings_kernel` function:

```C
retval = copy_strings_kernel(1, &bprm->filename, bprm);
if (retval < 0)
	goto out;

retval = copy_strings(bprm->envc, envp, bprm);
if (retval < 0)
	goto out;

retval = copy_strings(bprm->argc, argv, bprm);
if (retval < 0)
	goto out;
```

And set the pointer to the top of new program's stack that we set in the `bprm_mm_init` function:

```C
bprm->exec = bprm->p;
```

The top of the stack will contain the program filename and we store this fileneme tothe `exec` field of the `linux_bprm` structure.

Now we have filled `linux_bprm` structure, we call the `exec_binprm` function:

```C
retval = exec_binprm(bprm);
if (retval < 0)
	goto out;
```

First of all we store the [pid](https://en.wikipedia.org/wiki/Process_identifier) and `pid` that seen from the [namespace](https://en.wikipedia.org/wiki/Cgroups) of the current task in the `exec_binprm`:

```C
old_pid = current->pid;
rcu_read_lock();
old_vpid = task_pid_nr_ns(current, task_active_pid_ns(current->parent));
rcu_read_unlock();
```

and call the:

```C
search_binary_handler(bprm);
```

function. This function goes through the list of handlers that contains different binary formats. Currently the Linux kernel supports following binary formats:

* `binfmt_script` - support for interpreted scripts that are starts from the [#!](https://en.wikipedia.org/wiki/Shebang_%28Unix%29) line;
* `binfmt_misc` - support differnt binary formats, according to runtime configuration of the Linux kernel;
* `binfmt_elf` - support [elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) format;
* `binfmt_aout` - support [a.out](https://en.wikipedia.org/wiki/A.out) format;
* `binfmt_flat` - support for [flat](https://en.wikipedia.org/wiki/Binary_file#Structure) format;
* `binfmt_elf_fdpic` - Support for [elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) [FDPIC](http://elinux.org/UClinux_Shared_Library#FDPIC_ELF) binaries;
* `binfmt_em86` - support for Intel [elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) binaries running on [Alpha](https://en.wikipedia.org/wiki/DEC_Alpha) machines.

So, the search-binary_handler tries to call the `load_binary` function and pass `linux_binprm` to it. If the binary handler supports the given executable file format, it starts to prepare the executable binary for execution:

```C
int search_binary_handler(struct linux_binprm *bprm)
{
	...
	...
	...
	list_for_each_entry(fmt, &formats, lh) {
		retval = fmt->load_binary(bprm);
		if (retval < 0 && !bprm->mm) {
			force_sigsegv(SIGSEGV, current);
			return retval;
		}
	}
	
	return retval;	
```

Where the `load_binary` for example for the [elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) checks the magic number (each `elf` binary file contains magic number in the header) in the `linux_bprm` buffer (remember that we read first `128` bytes from the executable binary file): and exit if it is not `elf` binary:

```C
static int load_elf_binary(struct linux_binprm *bprm)
{
	...
	...
	...
	loc->elf_ex = *((struct elfhdr *)bprm->buf);

	if (memcmp(elf_ex.e_ident, ELFMAG, SELFMAG) != 0)
		goto out;
```

If the given executable file is in `elf` format, the `load_elf_binary` continues to execute. The `load_elf_binary` does many different things to prepare on execution executable file. For example it checks the architecture and type of the executable file:

```C
if (loc->elf_ex.e_type != ET_EXEC && loc->elf_ex.e_type != ET_DYN)
	goto out;
if (!elf_check_arch(&loc->elf_ex))
	goto out;
```

and exit if there is wrong architecture and executable file non executable non shared. Tries to load the `program header table`:

```C
elf_phdata = load_elf_phdrs(&loc->elf_ex, bprm->file);
if (!elf_phdata)
	goto out;
```

that describes [segments](https://en.wikipedia.org/wiki/Memory_segmentation). Read the `program interpreter` and libraries that linked with the our executable binary file from disk and load it to memory. The `program interpreter` specified in the `.interp` section of the executable file and as you can read in the part that describes [Linkers](https://xinqiu.gitbooks.io/linux-insides-cn/content/Misc/linux-misc-3.html) it is - `/lib64/ld-linux-x86-64.so.2` for the `x86_64`. It setups the stack and map `elf` binary into the correct location in memory. It maps the [bss](https://en.wikipedia.org/wiki/.bss) and the [brk](http://man7.org/linux/man-pages/man2/sbrk.2.html) sections and does many many other different things to prepare executable file to execute.

```c
static struct linux_binfmt elf_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_elf_binary,
	.load_shlib	= load_elf_library,
	.core_dump	= elf_core_dump,
	.min_coredump	= ELF_EXEC_PAGESIZE,
};
```

In the end of the execution of the `load_elf_binary` we call the `start_thread` function and pass three arguments to it:

```C
	finalize_exec(bprm);
	start_thread(regs, elf_entry, bprm->p);
	retval = 0;
out:
	return retval;
```

These arguments are:

* Set of [registers](https://en.wikipedia.org/wiki/Processor_register) for the new task;
* Address of the entry point of the new task;
* Address of the top of the stack for the new task.

As we can understand from the function's name, it starts new thread, but it is not so. The `start_thread` function just prepares new task's registers to be ready to run. Let's look on the implementation of this function:

```C
void
start_thread(struct pt_regs *regs, unsigned long new_ip, unsigned long new_sp)
{
        start_thread_common(regs, new_ip, new_sp,
                            __USER_CS, __USER_DS, 0);
}
```

As we can see the `start_thread` function just makes a call of the `start_thread_common` function that will do all for us:

```C
static void
start_thread_common(struct pt_regs *regs, unsigned long new_ip,
                    unsigned long new_sp,
                    unsigned int _cs, unsigned int _ss, unsigned int _ds)
{
        loadsegment(fs, 0);
        loadsegment(es, _ds);
        loadsegment(ds, _ds);
        load_gs_index(0);
        regs->ip                = new_ip;
        regs->sp                = new_sp;
        regs->cs                = _cs;
        regs->ss                = _ss;
        regs->flags             = X86_EFLAGS_IF;
        force_iret();
}
```

The `start_thread_common` function fills `fs` segment register with zero and `es` and `ds` with the value of the data segment register. After this we set new values to the [instruction pointer](https://en.wikipedia.org/wiki/Program_counter), `cs` segments etc. In the end of the `start_thread_common` function we can see the `force_iret` macro that force a system call return via `iret` instruction. Ok, we prepared new thread to run in userspace and now we can return from the `exec_binprm` and now we are in the `do_execveat_common` again. After the `exec_binprm` will finish its execution we release memory for structures that was allocated before and return.

After we returned from the `execve` system call handler, execution of our program will be started. We can do it, because all context related information already configured for this purpose. As we saw the `execve` system call does not return control to a process, but code, data and other segments of the caller process are just overwritten of the program segments. The exit from our application will be implemented through the `exit` system call.

That's all. From this point our programm will be executed.

## 2.3. Conclusion

This is the end of the fourth and last part of the about the system calls concept in the Linux kernel. We saw almost all related stuff to the `system call` concept in these four parts. We started from the understanding of the `system call` concept, we have learned what is it and why do users applications need in this concept. Next we saw how does the Linux handle a system call from an user application. We met two similar concepts to the `system call` concept, they are `vsyscall` and `vDSO` and finally we saw how does Linux kernel run an user program.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](anotherworldofworld@gmail.com) or just create [issue](https://github.com/MintCN/linux-insides-zh/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/MintCN/linux-insides-zh).**

# 3. 链接

* [System call](https://en.wikipedia.org/wiki/System_call)
* [shell](https://en.wikipedia.org/wiki/Unix_shell)
* [bash](https://en.wikipedia.org/wiki/Bash_%28Unix_shell%29)
* [entry point](https://en.wikipedia.org/wiki/Entry_point) 
* [C](https://en.wikipedia.org/wiki/C_%28programming_language%29)
* [environment variables](https://en.wikipedia.org/wiki/Environment_variable)
* [file descriptor](https://en.wikipedia.org/wiki/File_descriptor)
* [real uid](https://en.wikipedia.org/wiki/User_identifier#Real_user_ID)
* [virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system)
* [procfs](https://en.wikipedia.org/wiki/Procfs)
* [sysfs](https://en.wikipedia.org/wiki/Sysfs)
* [inode](https://en.wikipedia.org/wiki/Inode)
* [pid](https://en.wikipedia.org/wiki/Process_identifier)
* [namespace](https://en.wikipedia.org/wiki/Cgroups)
* [#!](https://en.wikipedia.org/wiki/Shebang_%28Unix%29)
* [elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
* [a.out](https://en.wikipedia.org/wiki/A.out)
* [flat](https://en.wikipedia.org/wiki/Binary_file#Structure) 
* [Alpha](https://en.wikipedia.org/wiki/DEC_Alpha)
* [FDPIC](http://elinux.org/UClinux_Shared_Library#FDPIC_ELF)
* [segments](https://en.wikipedia.org/wiki/Memory_segmentation)
* [Linkers](https://xinqiu.gitbooks.io/linux-insides-cn/content/Misc/linux-misc-3.html)
* [Processor register](https://en.wikipedia.org/wiki/Processor_register)
* [instruction pointer](https://en.wikipedia.org/wiki/Program_counter)
* [Previous part](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-3.html)
