<center><font size='6'>Linux内核深入理解系统调用（1）：初始化-入口-处理-退出</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>


# 1. Linux 内核系统调用简介

这次提交为 [linux内核解密](http://xinqiu.gitbooks.io/linux-insides-cn/content/) 添加一个新的章节，从标题就可以知道, 这一章节将介绍Linux 内核中 [System Call](https://en.wikipedia.org/wiki/System_call) 的概念。章节内容的选择并非偶然。在前一[章节](http://xinqiu.gitbooks.io/linux-insides-cn/content/Interrupts/index.html)我们了解了中断及中断处理。系统调用的概念与中断非常相似，这是因为软件中断是执行系统调用最常见的方式。接下来我们将从不同的角度来审视系统调用相关概念。例如，从用户空间发起系统调用时会发生什么，Linux内核中一组系统调用处理器的实现，[VDSO](https://en.wikipedia.org/wiki/VDSO) 和 [vsyscall](https://lwn.net/Articles/446528/) 的概念以及其他信息。

在了解 Linux 内核系统调用执行过程之前，让我们先来了解一些系统调用的相关原理。

## 1.1. 什么是系统调用?

系统调用就是从用户空间发起的内核服务请求。操作系统内核其实会提供很多服务，比如当程序想要读写文件、监听某个[socket](https://en.wikipedia.org/wiki/Network_socket)端口、删除或创建目录或者程序结束时，都会执行系统调用。换句话说，系统调用其实就是一些由用户空间程序调用去处理某些请求的 [C] (https://en.wikipedia.org/wiki/C_%28programming_language%29) 内核空间函数。

Linux 内核提供一系列的函数，但这些函数与CPU架构相关。 例如：[x86_64](https://en.wikipedia.org/wiki/X86-64) 提供 [322](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl) 个系统调用，[x86](https://en.wikipedia.org/wiki/X86) 提供 [358](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_32.tbl) 个不同的系统调用（在5.10.13中已经达到了547个）。
系统调用仅仅是一些函数。 我们看一个使用汇编语言编写的简单 `Hello world` 示例:

```assembly
.data

msg:
    .ascii "Hello, world!\n"
    len = . - msg

.text
    .global _start

_start:
    movq  $1, %rax
    movq  $1, %rdi
    movq  $msg, %rsi
    movq  $len, %rdx
    syscall

    movq  $60, %rax
    xorq  %rdi, %rdi
    syscall
```

使用下面的命令可编译这些语句:

```
$ gcc -c test.S
$ ld -o test test.o
```

执行:

```
./test
Hello, world!
```

这些代码是 Linux `x86_64` 架构下 `Hello world` 简单的汇编程序，代码包含两段:

* `.data`
* `.text`

第一段 - `.data` 存储程序的初始数据 (在示例中为`Hello world` 字符串)，第二段 - `.text` 包含程序的代码。代码可分为两部分: 第一部分为第一个 `syscall` 指令之前的代码，第二部分为两个 `syscall` 指令之间的代码。在示例程序及一般应用中， `syscall` 指令有什么功能？[64-ia-32-architectures-software-developer-vol-2b-manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)中提到:



```
SYSCALL 可以以优先级0调起系统调用处理程序，它通过加载IA32_LSTAR MSR至RIP完成调用(在RCX中保存 SYSCALL 指令地址之后)。
(WRMSR 指令确保IA32_LSTAR MSR总是包含一个连续的地址。)
...
...
...
SYSCALL 将 IA32_STAR MSR 的 47：32 位加载至 CS 和 SS 段选择器。总之，CS 和 SS 描述符缓存不是从哪些选择器所引用的描述符(在 GDT 或者 LDT 中)加载的。

相反，描述符缓存用固定值加载。确保由段选择器得到的描述符与从固定值加载至描述符缓存的描述符保持一致是操作系统的本职工作，但 SYSCALL 指令不保证两者的一致。
```

使用[arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S)汇编程序中定义的 `entry_SYSCALL_64` 初始化 `syscalls`
同时 `SYSCALL` 指令进入[arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/cpu/common.c) 源码文件中的 `IA32_STAR` [Model specific register](https://en.wikipedia.org/wiki/Model-specific_register):

```C
/* May not be marked __init: used by software suspend */
void syscall_init(void)
{
    ...
    wrmsrl(MSR_LSTAR, entry_SYSCALL_64);
        ...
}
```

因此，`syscall` 指令唤醒一个系统调用对应的处理程序。

**但是如何确定调用哪个处理程序？**

事实上这些信息从通用目的[寄存器](https://en.wikipedia.org/wiki/Processor_register)得到。正如[系统调用表](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl)中描述，每个系统调用对应特定的编号。上面的示例中, 第一个系统调用是 - `write` 将数据写入指定文件。在系统调用表中查找 write 系统调用.[write](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L10) 系统调用的编号为 - `1`。在示例中通过`rax`寄存器传递该编号，接下来的几个通用目的寄存器: `%rdi`, `%rsi` 和 `%rdx` 分别保存 `write` 系统调用的三个参数。 在示例中它们分别是：

* [文件描述符](https://en.wikipedia.org/wiki/File_descriptor) (`1` 是[stdout](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_.28stdout.29))    
* 参数字符串指针 　　
* 数据的大小 　　

是的，你没有看错，这就是系统调用的参数。正如上文所示, 系统调用仅仅是内核空间的 `C` 函数。示例中第一个系统调用为 write ，在 [fs/read_write.c] (https://github.com/torvalds/linux/blob/master/fs/read_write.c) 源文件中定义如下:

```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	...
	...
	...
}
```

或者是:

```C
ssize_t write(int fd, const void *buf, size_t nbytes);
```

暂时不用担心宏 `SYSCALL_DEFINE3` ,稍后再做讨论。

示例的第二部分也是一样的, 但调用了另一系统调用[exit](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L69)。这个系统调用仅需一个参数:

* Return value

该参数说明程序退出的方式。[strace](https://en.wikipedia.org/wiki/Strace) 工具可根据程序的名称输出系统调用的过程:

```
$ strace test
execve("./test", ["./test"], [/* 62 vars */]) = 0
write(1, "Hello, world!\n", 14Hello, world!
)         = 14
_exit(0)                                = ?

+++ exited with 0 +++
```

 `strace` 输出的第一行, [execve](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L68) 系统调用来执行程序，第二、三行为程序中使用的系统调用: `write` 和 `exit`。注意示例中通过通用目的寄存器传递系统调用的参数。**`寄存器的顺序是指定的`**，该顺序在- [x86-64 calling conventions] (https://rtoax.blog.csdn.net/article/details/115236939)中定义。
 `x86_64` 架构的声明在另一个特别的文档中 - [System V Application Binary Interface. PDF](http://www.x86-64.org/documentation/abi.pdf)或者[System V Application Binary Interface](https://refspecs.linuxbase.org/elf/x86_64-abi-0.21.pdf)。通常，函数参数被置于寄存器或者堆栈中。正确的顺序为:

* `rdi` 
* `rsi` 
* `rdx` 
* `rcx` 
* `r8` 
* `r9` 

这六个寄存器分别对应函数的前六个参数。若函数多于六个参数，其他参数将被放在堆栈中。 

我们不会在代码中直接使用系统调用，但当我们想打印一些东西的时候肯定会用到，检测一个文件的权限或是读写数据都会用到系统调用。

例如:

```C
#include <stdio.h>

int main(int argc, char **argv)
{
   FILE *fp;
   char buff[255];

   fp = fopen("test.txt", "r");
   fgets(buff, 255, fp);
   printf("%s\n", buff);
   fclose(fp);

   return 0;
}
```

Linux内核中没有 `fopen`, `fgets`, `printf` 和 `fclose` 系统调用，而是 `open`, `read` `write` 和 `close`。`fopen`, `fgets`, `printf` 和 `fclose` 仅仅是 `C` [standard library](https://en.wikipedia.org/wiki/GNU_C_Library)中定义的函数。事实上这些函数是系统调用的封装。我们不会在代码中直接使用系统调用，而是通过标准库的[封装](https://en.wikipedia.org/wiki/Wrapper_function)函数。主要原因非常简单: 系统调用执行的要快，非常快。系统调用快的同时也要非常小。而标准库会在执行系统调用前，确保系统调用参数设置正确并且完成一些其他不同的检查。我们用以下命令编译下示例程序：

```
$ gcc test.c -o test
```

通过[ltrace](https://en.wikipedia.org/wiki/Ltrace)工具观察:

```
$ ltrace ./test
__libc_start_main([ "./test" ] <unfinished ...>
fopen("test.txt", "r")                                             = 0x602010
fgets("Hello World!\n", 255, 0x602010)                             = 0x7ffd2745e700
puts("Hello World!\n"Hello World!

)                                                                  = 14
fclose(0x602010)                                                   = 0
+++ exited (status 0) +++
```

`ltrace`工具显示出了程序在用户空间的调用。

```
NAME
       ltrace - A library call tracer

SYNOPSIS
       ltrace   [-e   filter|-L]   [-l|--library=library_pattern]   [-x   filter]  [-S]  [-b|--no-signals]  [-i]
       [-w|--where=nr] [-r|-t|-tt|-ttt] [-T] [-F pathlist] [-A maxelts] [-s strsize] [-C|--demangle] [-a|--align
       column]  [-n|--indent nr] [-o|--output filename] [-D|--debug mask] [-u username] [-f] [-p pid] [[--] com‐
       mand [arg ...]]
```

`fopen` 函数打开给定的文本文件,  `fgets` 函数读取文件内容至 `buf` 缓存，`puts` 输出文件内容至 `stdout` ， `fclose` 函数根据文件描述符关闭函数。如上文描述，这些函数调用特定的系统调用。例如： `puts` 内部调用 `write` 系统调用，`ltrace` 添加 `-S`可观察到这一调用:

```
write@SYS(1, "Hello World!\n\n", 14) = 14
```

系统调用是普遍存在的。每个程序都需要打开/写/读文件，网络连接，内存分配和许多其他功能，这些只能由内核提供。[proc](https://en.wikipedia.org/wiki/Procfs) 文件系统有一个具有特定格式的特殊文件: `/proc/${pid}/syscall`记录了正在被进程调用的系统调用的编号和参数寄存器。例如,进程号 1 的程序是[systemd](https://en.wikipedia.org/wiki/Systemd):

```
$ sudo cat /proc/1/comm
systemd

$ sudo cat /proc/1/syscall
232 0x4 0x7ffdf82e11b0 0x1f 0xffffffff 0x100 0x7ffdf82e11bf 0x7ffdf82e11a0 0x7f9114681193
```

编号为 `232` 的系统调用为 [epoll_wait](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L241)，该调用等待 [epoll](https://en.wikipedia.org/wiki/Epoll) 文件描述符的I/O事件. 例如我用来编写这一节的 `emacs` 编辑器:

```
$ ps ax | grep emacs
2093 ?        Sl     2:40 emacs

$ sudo cat /proc/2093/comm
emacs

$ sudo cat /proc/2093/syscall
270 0xf 0x7fff068a5a90 0x7fff068a5b10 0x0 0x7fff068a59c0 0x7fff068a59d0 0x7fff068a59b0 0x7f777dd8813c
```

编号为 `270` 的系统调用是 [sys_pselect6](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L279) ，该系统调用使 `emacs` 监控多个文件描述符。

现在我们对系统调用有所了解，知道什么是系统调用及为什么需要系统调用。接下来，讨论示例程序中使用的 `write` 系统调用

## 1.2. write系统调用的实现

查看Linux内核源文件中写系统调用的实现。[fs/read_write.c](https://github.com/torvalds/linux/blob/master/fs/read_write.c) 源码文件中的 `write` 系统调用定义如下：
```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);
		ret = vfs_write(f.file, buf, count, &pos);
		if (ret >= 0)
			file_pos_write(f.file, pos);
		fdput_pos(f);
	}

	return ret;
}
```
在5.10.13中是这样的：
```c
ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos, *ppos = file_ppos(f.file);
		if (ppos) {
			pos = *ppos;
			ppos = &pos;
		}
		ret = vfs_write(f.file, buf, count, ppos);
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
		fdput_pos(f);
	}

	return ret;
}

SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	return ksys_write(fd, buf, count);
}
```

首先，宏 `SYSCALL_DEFINE3` 在头文件 [include/linux/syscalls.h](https://github.com/torvalds/linux/blob/master/include/linux/syscalls.h) 中定义并且作为 `sys_name(...)` 函数定义的扩展。该宏的定义如下:

```C
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)

#define SYSCALL_DEFINEx(x, sname, ...)                \
        SYSCALL_METADATA(sname, x, __VA_ARGS__)       \
        __SYSCALL_DEFINEx(x, sname, __VA_ARGS__)
```

宏 `SYSCALL_DEFINE3` 的参数有代表系统调用的名称的 `name`  和可变个数的参数。 这个宏仅仅为 `SYSCALL_DEFINEx` 宏的扩展确定了传入宏的参数个数。 `_##name` 作为未来系统调用名称的存根 (更多关于 `##`符号连结可参阅[documentation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html) of [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection))。让我们来看看 `SYSCALL_DEFINEx` 这个宏，这个宏扩展为以下两个宏:

* `SYSCALL_METADATA`;
* `__SYSCALL_DEFINEx`.

第一个宏 `SYSCALL_METADATA` 的实现依赖于`CONFIG_FTRACE_SYSCALLS`内核配置选项。 从选项的名称可知，它允许 tracer 捕获系统调用的进入和退出。若该内核配置选项开启，宏 `SYSCALL_METADATA`  执行头文件[include/trace/syscall.h](https://github.com/torvalds/linux/blob/master/include/trace/syscall.h) 中`syscall_metadata` 结构的初始化，该结构中包含多种有用字段例如系统调用的名称, 系统调用[表](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl)中的编号、参数个数、参数类型列表等:

```C
#define SYSCALL_METADATA(sname, nb, ...)                             \
	...                                                              \
	...                                                              \
	...                                                              \
    struct syscall_metadata __used                                   \
              __syscall_meta_##sname = {                             \
                    .name           = "sys"#sname,                   \
                    .syscall_nr     = -1,                            \
                    .nb_args        = nb,                            \
                    .types          = nb ? types_##sname : NULL,     \
                    .args           = nb ? args_##sname : NULL,      \
                    .enter_event    = &event_enter_##sname,          \
                    .exit_event     = &event_exit_##sname,           \
                    .enter_fields   = LIST_HEAD_INIT(__syscall_meta_##sname.enter_fields), \
             };                                                                            \

    static struct syscall_metadata __used                           \
              __attribute__((section("__syscalls_metadata")))       \
             *__p_syscall_meta_##sname = &__syscall_meta_##sname;
```
5.10.13中：
```c

#define SYSCALL_METADATA(sname, nb, ...)			\
	static const char *types_##sname[] = {			\
		__MAP(nb,__SC_STR_TDECL,__VA_ARGS__)		\
	};							\
	static const char *args_##sname[] = {			\
		__MAP(nb,__SC_STR_ADECL,__VA_ARGS__)		\
	};							\
	SYSCALL_TRACE_ENTER_EVENT(sname);			\
	SYSCALL_TRACE_EXIT_EVENT(sname);			\
	static struct syscall_metadata __used			\
	  __syscall_meta_##sname = {				\
		.name 		= "sys"#sname,			\
		.syscall_nr	= -1,	/* Filled in at boot */	\
		.nb_args 	= nb,				\
		.types		= nb ? types_##sname : NULL,	\
		.args		= nb ? args_##sname : NULL,	\
		.enter_event	= &event_enter_##sname,		\
		.exit_event	= &event_exit_##sname,		\
		.enter_fields	= LIST_HEAD_INIT(__syscall_meta_##sname.enter_fields), \
	};							\
	static struct syscall_metadata __used			\
	  __section("__syscalls_metadata")			\
	 *__p_syscall_meta_##sname = &__syscall_meta_##sname;
```

若内核配置时 `CONFIG_FTRACE_SYSCALLS` 未开启，此时宏 `SYSCALL_METADATA`扩展为空字符串:

```C
#define SYSCALL_METADATA(sname, nb, ...)
```

第二个宏 `__SYSCALL_DEFINEx` 扩展为以下五个函数的定义:

```C
#define __SYSCALL_DEFINEx(x, name, ...)                                 \
        asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))       \
                __attribute__((alias(__stringify(SyS##name))));         \
                                                                        \
        static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));  \
                                                                        \
        asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));      \
                                                                        \
        asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))       \
        {                                                               \
                long ret = SYSC##name(__MAP(x,__SC_CAST,__VA_ARGS__));  \
                __MAP(x,__SC_TEST,__VA_ARGS__);                         \
                __PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));       \
                return ret;                                             \
        }                                                               \
                                                                        \
        static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

第一个函数 `sys##name` 是给定名称为 `sys_system_call_name` 的系统调用处理器函数的定义。 宏 `__SC_DECL` 的参数有 `__VA_ARGS__` 及组合调用传入参数系统类型和参数名称，因为宏定义中无法指定参数类型。宏 `__MAP` 应用宏 `__SC_DECL` 给 `__VA_ARGS__` 参数。其他的函数是 `__SYSCALL_DEFINEx`生成的，详细信息可以查阅[CVE-2009-0029](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2009-0029) 此处不再深究。

5.10.13内核中对名字做出了较大的改动：
```c
#define __SYSCALL_DEFINEx(x, name, ...)					\
	static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__));	\
	static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	__X64_SYS_STUBx(x, name, __VA_ARGS__)				\
	__IA32_SYS_STUBx(x, name, __VA_ARGS__)				\
	static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__))	\
	{								\
		long ret = __do_sys##name(__MAP(x,__SC_CAST,__VA_ARGS__));\
		__MAP(x,__SC_TEST,__VA_ARGS__);				\
		__PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));	\
		return ret;						\
	}								\
	static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```
总之，write的系统调用函数定义应该是长这样（5.10.13中为__do_sys_write）:
```C
asmlinkage long sys_write(unsigned int fd, const char __user * buf, size_t count);
```

现在我们对系统调用的定义有一定了解，再来回头看看 `write` 系统调用的实现:

```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);
		ret = vfs_write(f.file, buf, count, &pos);
		if (ret >= 0)
			file_pos_write(f.file, pos);
		fdput_pos(f);
	}

	return ret;
}
```

从代码可知，该调用有三个参数:

* `fd` - 文件描述符
* `buf` - 写缓冲区
* `count` - 写缓冲区大小

该调用的功能是将用户定义的缓冲中的数据写入指定的设备或文件。注意第二个参数 `buf`, 定义了 `__user` 属性。该属性的主要目的是通过 [sparse](https://en.wikipedia.org/wiki/Sparse) 工具检查 Linux 内核代码。sparse 定义于 [include/linux/compiler.h] (https://github.com/torvalds/linux/blob/master/include/linux/compiler.h) 头文件中，并依赖 Linux 内核中 `__CHECKER__` 的定义。以上全是关于 `sys_write` 系统调用的有用元信息。我们可以看到，它的实现开始于 `f` 结构的定义，`f` 结构包含 `fd` 结构类型，`fd`是 Linux 内核中的文件描述符，也是我们存放 `fdget_pos` 函数调用结果的地方。`fdget_pos` 函数在相同的[源文件](https://github.com/torvalds/linux/blob/master/fs/read_write.c)中定义，其实就是 `__to_fd` 函数的扩展:

```C
static inline struct fd fdget_pos(int fd)
{
        return __to_fd(__fdget_pos(fd));
}
```

 `fdget_pos` 的主要目的是将给定的只有数字的文件描述符转化为 `fd` 结构。 通过一长链的函数调用， `fdget_pos` 函数得到当前进程的文件描述符表, `current->files`, 并尝试从表中获取一致的文件描述符编号。当获取到给定文件描述符的 `fd` 结构后, 检查文件并返回文件是否存在。通过调用函数 `file_pos_read` 获取当前处于文件中的位置。函数返回文件的 `f_pos` 字段:

```C
static inline loff_t file_pos_read(struct file *file)
{
        return file->f_pos;
}
```

接下来再调用 `vfs_write` 函数。 `vfs_write` 函数在源码文件 [fs/read_write.c](https://github.com/torvalds/linux/blob/master/fs/read_write.c) 中定义。其功能为 - 向指定文件的指定位置写入指定缓冲中的数据。此处不深入 `vfs_write` 函数的细节，因为这个函数与`系统调用`没有太多联系，反而与另一章节[虚拟文件系统](https://en.wikipedia.org/wiki/Virtual_file_system)相关。`vfs_write` 结束相关工作后, 检查结果若成功执行，使用`file_pos_write` 函数改变在文件中的位置:

```C
if (ret >= 0)
	file_pos_write(f.file, pos);
```

这恰好使用给定的位置更新给定文件的 `f_pos`:

```C
static inline void file_pos_write(struct file *file, loff_t pos)
{
        file->f_pos = pos;
}
```

在 `write` 系统调用处理函数的结尾处, 我们可以看到以下函数调用:

```C
fdput_pos(f);
```

这是在解锁在共享文件描述符的线程并发写文件时保护文件位置的互斥量 `f_pos_lock`。

我们讨论了Linux内核提供的系统调用的部分实现。显然略过了 `write` 系统调用实现的部分内容，正如文中所述, 在该章节中仅关心系统调用的相关内容，不讨论与其他子系统相关的内容，例如[虚拟文件系统](https://en.wikipedia.org/wiki/Virtual_file_system).

## 1.3. 总结
 
第一部分介绍了Linux内核中的系统调用概念。到目前为止，我们已经介绍了系统调用的理论，在下一部分中，我们将继续深入这个主题，讨论与系统调用相关的Linux内核代码。  

若存在疑问及建议, 在twitter @[0xAX](https://twitter.com/0xAX), 通过[email](anotherworldofworld@gmail.com) 或者创建 [issue](https://github.com/MintCN/linux-insides-zh/issues/new).

**由于英语是我的第一语言由此造成的不便深感抱歉。若发现错误请提交 PR 至 [linux-insides](https://github.com/MintCN/linux-insides-zh).**

## 1.4. 链接

* [system call](https://en.wikipedia.org/wiki/System_call)
* [vdso](https://en.wikipedia.org/wiki/VDSO)
* [vsyscall](https://lwn.net/Articles/446528/)
* [general purpose registers](https://en.wikipedia.org/wiki/Processor_register)
* [socket](https://en.wikipedia.org/wiki/Network_socket)
* [C programming language](https://en.wikipedia.org/wiki/C_%28programming_language%29)
* [x86](https://en.wikipedia.org/wiki/X86)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [x86-64 calling conventions](https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions)
* [System V Application Binary Interface. PDF](http://www.x86-64.org/documentation/abi.pdf)
* [GCC](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [Intel manual. PDF](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [system call table](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl)
* [GCC macro documentation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html)
* [file descriptor](https://en.wikipedia.org/wiki/File_descriptor)
* [stdout](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_.28stdout.29)
* [strace](https://en.wikipedia.org/wiki/Strace)
* [standard library](https://en.wikipedia.org/wiki/GNU_C_Library)
* [wrapper functions](https://en.wikipedia.org/wiki/Wrapper_function)
* [ltrace](https://en.wikipedia.org/wiki/Ltrace)
* [sparse](https://en.wikipedia.org/wiki/Sparse)
* [proc file system](https://en.wikipedia.org/wiki/Procfs)
* [Virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system)
* [systemd](https://en.wikipedia.org/wiki/Systemd)
* [epoll](https://en.wikipedia.org/wiki/Epoll)
* [Previous chapter](http://xinqiu.gitbooks.io/linux-insides-cn/content/Interrupts/index.html)



# 2. Linux 内核如何处理系统调用

前一[小节](http://0xax.gitbooks.io/linux-insides/content/SysCall/linux-syscall-1.html) 作为本章节的第一部分描述了 Linux 内核[system call](https://en.wikipedia.org/wiki/System_call) 概念。
前一节中提到通常系统调用处于内核处于操作系统层面。前一节内容从用户空间的角度介绍，并且 [write](http://man7.org/linux/man-pages/man2/write.2.html)系统调用实现的一部分内容没有讨论。在这一小节继续关注系统调用，在深入 Linux 内核之前，从一些理论开始。

程序中一个用户程序并不直接使用系统调用。我们并未这样写 `Hello World`程序代码：

```C
int main(int argc, char **argv)
{
	...
	...
	...
	sys_write(fd1, buf, strlen(buf));
	...
	...
}
```

我们可以使用与 [C standard library](https://en.wikipedia.org/wiki/GNU_C_Library) 帮助类似的方式:

```C
#include <unistd.h>

int main(int argc, char **argv)
{
	...
	...
	...
	write(fd1, buf, strlen(buf));
	...
	...
}
```

不管怎样， `write` 不是直接的系统调用也不是内核函数。程序必须将通用目的寄存器按照正确的顺序存入正确的值，之后使用 `syscall` 指令实现真正的系统调用。在这一节我们关注 Linux 内核中，处理器执行 `syscall` 指令时的细节。

## 2.1. 系统调用表的初始化

从前一节可知系统调用与中断非常相似。深入的说，系统调用是软件中断的处理程序。因此，当处理器执行程序的 `syscall` 指令时，指令引起异常导致将控制权转移至异常处理。 众所周知，所有的异常处理 (或者内核 [C](https://en.wikipedia.org/wiki/C_%28programming_language%29) 函数将响应异常) 是放在内核代码中的。

但是 **Linux 内核如何查找对应系统调用的系统调用处理程序的地址？** 

Linux 内核由一个特殊的表：`system call table` 。 系统调用表是Linux内核源码文件 [arch/x86/entry/syscall_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscall_64.c) 中定义的数组`sys_call_table`的对应。其实现如下:

```C
asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
    #include <asm/syscalls_64.h>
};
```

 `sys_call_table` 数组的大小为 `__NR_syscall_max + 1` ， `__NR_syscall_max` 宏作为给定[架构](https://en.wikipedia.org/wiki/List_of_CPU_architectures) 的系统调用最大数量。 这本书关于 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构, 因此 `__NR_syscall_max` 为 `322` ，这也是本书编写时(当前 Linux 内核版本为 `4.2.0-rc8+`)的数字。编译内核时可通过 [Kbuild](https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt)产生的头文件查看该宏 - include/generated/asm-offsets.h`:

```C
#define __NR_syscall_max 322
```

对于 `x86_64` ， [arch/x86/entry/syscalls/syscall_64.tbl](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl#L331) 中也有相同的系统调用数量。这里存在两个重要的话题;  `sys_call_table` 数组的类型及数组中元数的初始值。首先，`sys_call_ptr_t` 为指向系统调用表的指针。 其是通过 [typedef] 定义的函数指针的(https://en.wikipedia.org/wiki/Typedef) ，返回值为空且无参数：

```C
typedef void (*sys_call_ptr_t)(void);
```

其次为 `sys_call_table` 数组中元素的初始化。从上面的代码中可知,数组中所有元素包含指向 `sys_ni_syscall` 的系统调用处理器的指针。 `sys_ni_syscall` 函数为 “not-implemented” 调用。 首先,  `sys_call_table` 的所有元素指向 “not-implemented” 系统调用。这是正确的初始化方法，因为我们仅仅初始化指向系统调用处理器的指针的存储位置，稍后再做处理。 `sys_ni_syscall` 的结果比较简单, 仅仅返回 [-errno](http://man7.org/linux/man-pages/man3/errno.3.html) 或者 `-ENOSYS` :

```C
asmlinkage long sys_ni_syscall(void)
{
	return -ENOSYS;
}
```

The `-ENOSYS` error tells us that:

```
ENOSYS          Function not implemented (POSIX.1)
```

在 `sys_call_table` 的初始化中同时也要注意 `...` 。可通过 [GCC](https://en.wikipedia.org/wiki/GNU_Compiler_Collection) 编译器插件 - [Designated Initializers](https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html) 处理。插件允许使用不固定的顺序初始化元素。 在数组结束处，我们引用 `asm/syscalls_64.h` 头文件在。头文件由特殊的脚本 [arch/x86/entry/syscalls/syscalltbl.sh](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscalltbl.sh) 从 [syscall table](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl) 产生。 

`syscalls/syscall_64.tbl`部分内容：
```
0	common	read			sys_read
1	common	write			sys_write
2	common	open			sys_open
3	common	close			sys_close
4	common	stat			sys_newstat
5	common	fstat			sys_newfstat
```

`asm/syscalls_64.h` 包括以下宏的定义:

```C
__SYSCALL_COMMON(0, sys_read, sys_read)
__SYSCALL_COMMON(1, sys_write, sys_write)
__SYSCALL_COMMON(2, sys_open, sys_open)
__SYSCALL_COMMON(3, sys_close, sys_close)
__SYSCALL_COMMON(5, sys_newfstat, sys_newfstat)
...
...
...
```

宏 `__SYSCALL_COMMON`  在相同的 [源码](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscall_64.c)中定义，作为宏 `__SYSCALL_64`的扩展:

```C
#define __SYSCALL_COMMON(nr, sym, compat) __SYSCALL_64(nr, sym, compat)
#define __SYSCALL_64(nr, sym, compat) [nr] = sym,
```

因而, 到此为止,  `sys_call_table` 为如下格式:

```C
asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
	[0] = sys_read,
	[1] = sys_write,
	[2] = sys_open,
	...
	...
	...
};
```

之后所有指向“ non-implemented ”系统调用元素的内容为 `sys_ni_syscall` 函数的地址，该函数仅返回 `-ENOSYS` 。 其他元素指向 `sys_syscall_name` 函数。

至此, 完成系统调用表的填充并且 Linux内核了解系统调用处理器的为值。但是 Linux 内核在处理用户空间程序的系统调用时并未立即调用 `sys_syscall_name` 函数。 记住关于中断及中断处理的 [章节](http://xinqiu.gitbooks.io/linux-insides-cn/content/Interrupts/index.html)。当 Linux 内核获得处理中断的控制权, 在调用中断处理程序前，必须做一些准备如保存用户空间寄存器，切换至新的堆栈及其他很多工作。系统调用处理也是相同的情形。第一件事是处理系统调用的准备，但是在 Linux 内核开始这些准备之前, 系统调用的入口必须完成初始化，同时只有 Linux 内核知道如何执行这些准备。在下一章节我们将关注 Linux 内核中关于系统调用入口的初始化过程。

## 2.2. 系统调用入口初始化

当系统中发生系统调用, 开始处理调用的代码的第一个字节在什么地方? 阅读 Intel 的手册 - [64-ia-32-architectures-software-developer-vol-2b-manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html) 或者参考链接[Intel® 64 and IA-32 Architectures Software Developer Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html):

```
SYSCALL 引起操作系统系统调用处理器处于特权级0，通过加载IA32_LSTAR MSR至RIP完成。
```

原文：
```
SYSCALL invokes an OS system-call handler at privilege level 0. It does so by loading RIP from the IA32_LSTAR
MSR (after saving the address of the instruction following SYSCALL into RCX). (The WRMSR instruction ensures
that the IA32_LSTAR MSR always contain a canonical address.)
```

这就是说我们需要将系统调用入口放置到 `IA32_LSTAR` [model specific register](https://en.wikipedia.org/wiki/Model-specific_register) 。 这一操作在 Linux 内核初始过程时完成。若已阅读关于 Linux 内核中断及中断处理中断的 [第四节](http://xinqiu.gitbooks.io/linux-insides-cn/content/Interrupts/linux-interrupts-4.html) , Linux 内核调用在初始化过程中调用 `trap_init` 函数。该函数在 [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup.c) 源代码文件中定义，执行 `non-early` 异常处理（如除法错误，[协处理器](https://en.wikipedia.org/wiki/Coprocessor) 错误等 ）的初始化。除了  `non-early` 异常处理的初始化外, 函数调用  [arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/master/blob/arch/x86/kernel/cpu/common.c) 中 `cpu_init` 函数，调用相同源码文件中的 `syscall_init` 完成`per-cpu` 状态初始化。

该函数执行系统调用入口的初始化。查看函数的实现，函数没有参数且首先填充两个特殊模块寄存器：

```C
wrmsrl(MSR_STAR,  ((u64)__USER32_CS)<<48  | ((u64)__KERNEL_CS)<<32);
wrmsrl(MSR_LSTAR, entry_SYSCALL_64);
```

第一个特殊模块集寄存器- `MSR_STAR` 的 `63:48` 为用户代码的代码段。这些数据将加载至 `CS` 和  `SS` 段选择符，由提供将系统调用返回至相应特权级的用户代码功能的 `sysret` 指令使用。 同时从内核代码来看， 当用户空间应用程序执行系统调用时，`MSR_STAR` 的 `47:32` 将作为 `CS` and `SS`段选择寄存器的基地址。第二行代码中我们将使用系统调用入口`entry_SYSCALL_64` 填充 `MSR_LSTAR` 寄存器。 `entry_SYSCALL_64` 在 [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S) 汇编文件中定义，包含系统调用执行前的准备(上面已经提及这些准备)。 目前不关注 `entry_SYSCALL_64` ,将在章节的后续讨论。

在设置系统调用的入口之后，需要以下特殊模式寄存器：

* `MSR_CSTAR` - target `rip` for the compability mode callers;
* `MSR_IA32_SYSENTER_CS` - target `cs` for the `sysenter` instruction;
* `MSR_IA32_SYSENTER_ESP` - target `esp` for the `sysenter` instruction;
* `MSR_IA32_SYSENTER_EIP` - target `eip` for the `sysenter` instruction.

这些特殊模式寄存器的值与内核配置选项 `CONFIG_IA32_EMULATION` 有关。 若开启该内核配置选项，允许64字节内核运行32字节的程序。 首先, 若 `CONFIG_IA32_EMULATION` 内合配置选项开启, 将使用兼容模式的系统调用入口填充这些特殊模式寄存器：

```C
wrmsrl(MSR_CSTAR, entry_SYSCALL_compat);
```

对于内核代码段, 将堆栈指针置零，`entry_SYSENTER_compat`字的地址写入[指令指针](https://en.wikipedia.org/wiki/Program_counter):

```C
wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)__KERNEL_CS);
wrmsrl_safe(MSR_IA32_SYSENTER_ESP, 0ULL);
wrmsrl_safe(MSR_IA32_SYSENTER_EIP, (u64)entry_SYSENTER_compat);
```

另一方面, 若 `CONFIG_IA32_EMULATION` 内核配置选项未开启, 将把 `ignore_sysret` 字写入`MSR_CSTAR`:

```C
wrmsrl(MSR_CSTAR, ignore_sysret);
```

其在[arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S) 汇编文件中定义，仅返回 `-ENOSYS` 错误代码:

```assembly
ENTRY(ignore_sysret)
	mov	$-ENOSYS, %eax
	sysret
END(ignore_sysret)
```

现在需要像之前代码一样填充 `MSR_IA32_SYSENTER_CS`, `MSR_IA32_SYSENTER_ESP`, `MSR_IA32_SYSENTER_EIP` 特殊模式寄存器，当`CONFIG_IA32_EMULATION` 内核配置选项打开时。 在这种情况( `CONFIG_IA32_EMULATION` 配置选项未设置) 将用零填充 `MSR_IA32_SYSENTER_ESP` 和 `MSR_IA32_SYSENTER_EIP` ，同时将 [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table) 的无效段加载至 `MSR_IA32_SYSENTER_CS` 特殊模式寄存器:

```C
wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)GDT_ENTRY_INVALID_SEG);
wrmsrl_safe(MSR_IA32_SYSENTER_ESP, 0ULL);
wrmsrl_safe(MSR_IA32_SYSENTER_EIP, 0ULL);
```

可以从描述 Linux 内核启动过程的[章节](http://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-2.html)阅读更多关于 `Global Descriptor Table` 的内容。

在`syscall_init` 函数的结束, 通过写入 `MSR_SYSCALL_MASK` 特殊寄存器的标志位，将 [标志寄存器](https://en.wikipedia.org/wiki/FLAGS_register) 中的标志位屏蔽:

```C
wrmsrl(MSR_SYSCALL_MASK,
	   X86_EFLAGS_TF|X86_EFLAGS_DF|X86_EFLAGS_IF|
	   X86_EFLAGS_IOPL|X86_EFLAGS_AC|X86_EFLAGS_NT);
```

这些标志位将在 syscall 初始化时清除。至此,  `syscall_init` 函数结束 也意味着系统调用已经可用。现在我们关注当用户程序执行 `syscall` 指令发生什么。

在5.10.13中的syscall_init注释代码：
```c

/* May not be marked __init: used by software suspend 
   执行系统调用入口的初始化 */
void syscall_init(void)
{
    //特殊模块集寄存器`MSR_STAR` 的 `63:48` 为用户代码的代码段
    //这些数据将加载至 `CS` 和  `SS` 段选择符，由提供将系统调用返回
    //至相应特权级的用户代码功能的 `sysret` 指令使用。 
    //同时从内核代码来看， 当用户空间应用程序执行系统调用时，`MSR_STAR` 
    //的 `47:32` 将作为 `CS` and `SS`段选择寄存器的基地址
	wrmsr(MSR_STAR, 0, (__USER32_CS << 16) | __KERNEL_CS);

    //使用系统调用入口`entry_SYSCALL_64` 填充 `MSR_LSTAR` 寄存器
	wrmsrl(MSR_LSTAR, (unsigned long)entry_SYSCALL_64);

    /**
     *  设置系统调用的入口之后，需要以下特殊模式寄存器
     *
     * `MSR_CSTAR` - target `rip` for the compability mode callers;
     * `MSR_IA32_SYSENTER_CS` - target `cs` for the `sysenter` instruction;
     * `MSR_IA32_SYSENTER_ESP` - target `esp` for the `sysenter` instruction;
     * `MSR_IA32_SYSENTER_EIP` - target `eip` for the `sysenter` instruction.
     */
#ifdef CONFIG_IA32_EMULATION    //允许64字节内核运行32字节的程序
	wrmsrl(MSR_CSTAR, (unsigned long)entry_SYSCALL_compat);
	/*
	 * This only works on Intel CPUs.
	 * On AMD CPUs these MSRs are 32-bit, CPU truncates MSR_IA32_SYSENTER_EIP.
	 * This does not cause SYSENTER to jump to the wrong location, because
	 * AMD doesn't allow SYSENTER in long mode (either 32- or 64-bit).
	 */
	wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)__KERNEL_CS);
	wrmsrl_safe(MSR_IA32_SYSENTER_ESP,
		    (unsigned long)(cpu_entry_stack(smp_processor_id()) + 1));

    //`entry_SYSENTER_compat`字的地址写入[指令指针]
	wrmsrl_safe(MSR_IA32_SYSENTER_EIP, (u64)entry_SYSENTER_compat);
#else
    //把 `ignore_sysret` 字写入`MSR_CSTAR`,仅返回 `-ENOSYS` 错误代码
	wrmsrl(MSR_CSTAR, (unsigned long)ignore_sysret);

    //将 [Global Descriptor Table]的无效段加载至 `MSR_IA32_SYSENTER_CS` 特殊模式寄存器
	wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)GDT_ENTRY_INVALID_SEG/*0*/);

    //用零填充 `MSR_IA32_SYSENTER_ESP` 和 `MSR_IA32_SYSENTER_EIP`
	wrmsrl_safe(MSR_IA32_SYSENTER_ESP, 0ULL);
	wrmsrl_safe(MSR_IA32_SYSENTER_EIP, 0ULL);
#endif

	/* Flags to clear on syscall */
    //通过写入 `MSR_SYSCALL_MASK` 特殊寄存器的标志位，将 [标志寄存器] 中的标志位屏蔽
    //这些标志位将在 syscall 初始化时清除
	wrmsrl(MSR_SYSCALL_MASK,
	       X86_EFLAGS_TF|X86_EFLAGS_DF|X86_EFLAGS_IF|
	       X86_EFLAGS_IOPL|X86_EFLAGS_AC|X86_EFLAGS_NT);

    /* 至此,  `syscall_init` 函数结束 也意味着系统调用已经可用 */
}
```


## 2.3. 系统调用处理执行前的准备

如之前写到, 系统调用或中断处理在被 Linux 内核调用前需要一些准备。 

* 宏 `idtentry` 完成异常处理被执行前的所需准备，
* 宏 `interrupt` 完成中断处理被调用前的所需准备 ，
* `entry_SYSCALL_64` 完成系统调用执行前的所需准备。

 `entry_SYSCALL_64` 在 [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S)  汇编文件中定义 ，从下面的宏开始:

```assembly
SWAPGS_UNSAFE_STACK
```

该宏在 [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/irqflags.h) 头文件中定义， 扩展 `swapgs` 指令:

```C
#define SWAPGS_UNSAFE_STACK	swapgs
```

宏将交换 GS 段选择符及 `MSR_KERNEL_GS_BASE ` 特殊模式寄存器中的值。换句话说，将其入内核堆栈 。之后使老的堆栈指针指向 `rsp_scratch` [per-cpu](http://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html) 变量设置堆栈指针指向当前处理器的栈顶：

```assembly
movq	%rsp, PER_CPU_VAR(rsp_scratch)
movq	PER_CPU_VAR(cpu_current_top_of_stack), %rsp
```

下一步中将堆栈段及老的堆栈指针入栈：

```assembly
pushq	$__USER_DS
pushq	PER_CPU_VAR(rsp_scratch)
```

之后使能中断, 因为入口，中断被关闭，保存通用目的 [寄存器](https://en.wikipedia.org/wiki/Processor_register) (除 `bp`, `bx` 及 `r12` 至 `r15`), 标志位, “ non-implemented ” 系统调用相关的 `-ENOSYS` 及代码段寄存器至堆栈:

```assembly
ENABLE_INTERRUPTS(CLBR_NONE)

pushq	%r11
pushq	$__USER_CS
pushq	%rcx
pushq	%rax
pushq	%rdi
pushq	%rsi
pushq	%rdx
pushq	%rcx
pushq	$-ENOSYS
pushq	%r8
pushq	%r9
pushq	%r10
pushq	%r11
sub	$(6*8), %rsp
```

当系统调用由用户空间程序引起时, 通用目的寄存器状态如下:

* `rax` - contains system call number;
* `rcx` - contains return address to the user space;
* `r11` - contains register flags;
* `rdi` - contains first argument of a system call handler;
* `rsi` - contains second argument of a system call handler;
* `rdx` - contains third argument of a system call handler;
* `r10` - contains fourth argument of a system call handler;
* `r8`  - contains fifth argument of a system call handler;
* `r9`  - contains sixth argument of a system call handler;

```c
 * Registers on entry:
 * rax  system call number
 * rcx  return address
 * r11  saved rflags (note: r11 is callee-clobbered register in C ABI)
 * rdi  arg0
 * rsi  arg1
 * rdx  arg2
 * r10  arg3 (needs to be moved to rcx to conform to C ABI)
 * r8   arg4
 * r9   arg5
 * (note: r12-r15, rbp, rbx are callee-preserved in C ABI)
```

其他通用目的寄存器 (如 `rbp`, `rbx` 和  `r12` 至 `r15`)  在[C ABI](http://www.x86-64.org/documentation/abi.pdf))保留。将寄存器标志位入栈，之后是 “non-implemented ”系统调用的用户代码段，用户空间返回地址，系统调用编号，三个参数，dump 错误代码和堆栈中的其他信息。

下一步检查当前 `thread_info` 中的 `_TIF_WORK_SYSCALL_ENTRY`:

```assembly
testl	$_TIF_WORK_SYSCALL_ENTRY, ASM_THREAD_INFO(TI_flags, %rsp, SIZEOF_PTREGS)
jnz	tracesys
```

宏 `_TIF_WORK_SYSCALL_ENTRY`在 [arch/x86/include/asm/thread_info.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/thread_info.h) 头文件中定义 ，提供一系列与系统调用跟踪有关的进程信息标志:

```C
#define _TIF_WORK_SYSCALL_ENTRY \
    (_TIF_SYSCALL_TRACE | _TIF_SYSCALL_EMU | _TIF_SYSCALL_AUDIT |   \
    _TIF_SECCOMP | _TIF_SINGLESTEP | _TIF_SYSCALL_TRACEPOINT |     \
    _TIF_NOHZ)
```

本章节中不讨论追踪/调试相关内容,将在关于 Linux 内核调试及追踪相关独立章节中讨论。 在 `tracesys` 标签之后, 下一标签为 `entry_SYSCALL_64_fastpath`.在 `entry_SYSCALL_64_fastpath` 中检查  头文件 [arch/x86/include/asm/unistd.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/unistd.h) 中定义的 `__SYSCALL_MASK` 

```C
# ifdef CONFIG_X86_X32_ABI
#  define __SYSCALL_MASK (~(__X32_SYSCALL_BIT))
# else
#  define __SYSCALL_MASK (~0)
# endif
```

 `__X32_SYSCALL_BIT` 为：

```C
#define __X32_SYSCALL_BIT	0x40000000
```

众所周知， `__SYSCALL_MASK` 与 `CONFIG_X86_X32_ABI` 内核配置选项相关， 作为 64位内核中32位[ABI](https://en.wikipedia.org/wiki/Application_binary_interface) 的掩码。

So we check the value of the `__SYSCALL_MASK` and if the `CONFIG_X86_X32_ABI` is disabled we compare the value of the `rax` register to the maximum syscall number (`__NR_syscall_max`), alternatively if the `CNOFIG_X86_X32_ABI` is enabled we mask the `eax` register with the `__X32_SYSCALL_BIT` and do the same comparison:

```assembly
#if __SYSCALL_MASK == ~0
	cmpq	$__NR_syscall_max, %rax
#else
	andl	$__SYSCALL_MASK, %eax
	cmpl	$__NR_syscall_max, %eax
#endif
```

至此检查最后一调比较指令的结果， `ja` 指令在  `CF` 和 `ZF` 标志为 0 时执行:

```assembly
ja	1f
```

若正确调用系统调用, 从 `r10` 移动第四个参数至 `rcx` ，保持 [x86_64 C ABI](http://www.x86-64.org/documentation/abi.pdf) 开启，同时以系统调用的处理程序的地址为参数执行 `call` 指令:

```assembly
movq	%r10, %rcx
call	*sys_call_table(, %rax, 8)
```

注意,  上文提到 `sys_call_table` 是一个数组。  `rax` 通用目的寄存器为系统调用的编号，且 `sys_call_table` 的每个元素为 8 字节。 因此使用 `*sys_call_table(, %rax, 8)` 符号找到指定系统调用处理在  `sys_call_table`  中的偏移。

就这样。完成了所需的准备，系统调用处理将被相应的中断处理调用。 例如 Linux 内核代码中 `SYSCALL_DEFINE[N]`宏定义的 `sys_read`, `sys_write` 和其他中断处理。

在5.10.13中上面很多部分都写成了c程序。

```c
#ifdef CONFIG_X86_64    /* entry_SYSCALL_64 调用 */
__visible noinstr void do_syscall_64(unsigned long nr, struct pt_regs *regs)
{
	nr = syscall_enter_from_user_mode(regs, nr);

	instrumentation_begin();
	if (likely(nr < NR_syscalls)) {
		nr = array_index_nospec(nr, NR_syscalls);
		regs->ax = sys_call_table[nr](regs);
#ifdef CONFIG_X86_X32_ABI
//	} else if (likely((nr & __X32_SYSCALL_BIT) &&
//			  (nr & ~__X32_SYSCALL_BIT) < X32_NR_syscalls)) {
//		nr = array_index_nospec(nr & ~__X32_SYSCALL_BIT,
//					X32_NR_syscalls);
//		regs->ax = x32_sys_call_table[nr](regs);
#endif
	}
	instrumentation_end();
	syscall_exit_to_user_mode(regs);
}
#endif
```

## 2.4. 退出系统调用

在系统调用处理完成任务后, 将退回[arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S), 正好在系统调用之后:

```assembly
call	*sys_call_table(, %rax, 8)
```
在5.10.13中如：
```c
regs->ax = sys_call_table[nr](regs);
```

在从系统调用处理返回之后，下一步是将系统调用处理的返回值入栈。系统调用将用户程序的返回结果放置在通用目的寄存器`rax` 中,因此在系统调用处理完成其工作后，将寄存器的值入栈： 

```C
movq	%rax, RAX(%rsp)
```

在 `RAX` 指定的位置。

之后调用在 [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/irqflags.h) 中定义的宏 `LOCKDEP_SYS_EXIT` :

```assembly
LOCKDEP_SYS_EXIT
```

宏的实现与 `CONFIG_DEBUG_LOCK_ALLOC` 内核配置选项相关，该配置允许在退出系统调用时调试锁。再次强调，在该章节不关注，将在单独的章节讨论相关内容。 在 `entry_SYSCALL_64` 函数的最后， 恢复除 `rxc` 和 `r11` 外所有通用寄存器, 因为 `rcx` 寄存器为调用系统调用的应用程序的返回地址， `r11` 寄存器为老的 [flags register](https://en.wikipedia.org/wiki/FLAGS_register). 在恢复所有通用寄存器之后， 将在 `rcx` 中装入返回地址, `r11` 寄存器装入标志 ， `rsp` 装入老的堆栈指针:

```assembly
RESTORE_C_REGS_EXCEPT_RCX_R11

movq	RIP(%rsp), %rcx
movq	EFLAGS(%rsp), %r11
movq	RSP(%rsp), %rsp

USERGS_SYSRET64
```

最后仅仅调用宏 `USERGS_SYSRET64` ，其扩展调用 `swapgs` 指令交换用户 `GS` 和内核`GS`， `sysretq` 指令执行从系统调用处理退出。

```C
#define USERGS_SYSRET64				\
	swapgs;	           				\
	sysretq;
```

现在我们知道，当用户程序使用系统调用时发生的一切。整个过程的步骤如下：

* 用户程序中的代码装入通用目的寄存器的值（系统调用编号和系统调用的参数）;
* 处理器从用户模式切换到内核模式 开始执行系统调用入口 - `entry_SYSCALL_64`;
* `entry_SYSCALL_64` 切换至内核堆栈，在堆栈中存通用目的寄存器, 老的堆栈，代码段, 标志位等;
* `entry_SYSCALL_64` 检查 `rax` 寄存器中的系统调用编号,系统调用编号正确时， 在 `sys_call_table` 中查找系统调用处理并调用;
* 若系统调用编号不正确, 跳至系统调用退出;
* 系统调用处理完成工作后, 恢复通用寄存器, 老的堆栈，标志位 及返回地址 ，通过`sysretq` 指令退出`entry_SYSCALL_64` .

在5.10.13中指定的为：
```c
syscall_exit_to_user_mode(regs);
```
该函数的实现：
```c
__visible noinstr void syscall_exit_to_user_mode(struct pt_regs *regs)
{
	instrumentation_begin();
	syscall_exit_to_user_mode_prepare(regs);
	local_irq_disable_exit_to_user();
	exit_to_user_mode_prepare(regs);
	instrumentation_end();
	exit_to_user_mode();
}//TODO
```

## 2.5. 结论

这是 Linux 内核相关概念的第二节。在前一 [节](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-1.html) ，从用户应用程序的角度讨论了这些概念的原理。在这一节继续深入系统调用概念的相关内容，讨论了系统调用发生时 Linux 内核执行的内容。

若存在疑问及建议, 在twitter @[0xAX](https://twitter.com/0xAX), 通过[email](anotherworldofworld@gmail.com) 或者创建 [issue](https://github.com/MintCN/linux-insides-zh/issues/new).

**由于英语是我的第一语言由此造成的不便深感抱歉。若发现错误请提交 PR 至 [linux-insides](https://github.com/MintCN/linux-insides-zh).**

## 2.6. 链接

* [system call](https://en.wikipedia.org/wiki/System_call)
* [write](http://man7.org/linux/man-pages/man2/write.2.html)
* [C standard library](https://en.wikipedia.org/wiki/GNU_C_Library)
* [list of cpu architectures](https://en.wikipedia.org/wiki/List_of_CPU_architectures)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [kbuild](https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt)
* [typedef](https://en.wikipedia.org/wiki/Typedef)
* [errno](http://man7.org/linux/man-pages/man3/errno.3.html)
* [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [model specific register](https://en.wikipedia.org/wiki/Model-specific_register)
* [intel 2b manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [coprocessor](https://en.wikipedia.org/wiki/Coprocessor)
* [instruction pointer](https://en.wikipedia.org/wiki/Program_counter)
* [flags register](https://en.wikipedia.org/wiki/FLAGS_register)
* [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table)
* [per-cpu](http://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html)
* [general purpose registers](https://en.wikipedia.org/wiki/Processor_register)
* [ABI](https://en.wikipedia.org/wiki/Application_binary_interface)
* [x86_64 C ABI](http://www.x86-64.org/documentation/abi.pdf)
* [previous chapter](http://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-1.html)


<center><font size='6'>英文原文</font></center>

System calls in the Linux kernel. Part 1.
================================================================================

Introduction
--------------------------------------------------------------------------------

This post opens up a new chapter in [linux-insides](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md) book, and as you may understand from the title, this chapter will be devoted to the [System call](https://en.wikipedia.org/wiki/System_call) concept in the Linux kernel. The choice of topic for this chapter is not accidental. In the previous [chapter](https://0xax.gitbook.io/linux-insides/summary/interrupts) we saw interrupts and interrupt handling. The concept of system calls is very similar to that of interrupts. This is because the most common way to implement system calls is as software interrupts. We will see many different aspects that are related to the system call concept. For example, we will learn what's happening when a system call occurs from userspace. We will see an implementation of a couple system call handlers in the Linux kernel, [VDSO](https://en.wikipedia.org/wiki/VDSO) and [vsyscall](https://lwn.net/Articles/446528/) concepts and many many more.

Before we dive into Linux system call implementation, it is good to know some theory about system calls. Let's do it in the following paragraph.

System call. What is it?
--------------------------------------------------------------------------------

A system call is just a userspace request of a kernel service. Yes, the operating system kernel provides many services. When your program wants to write to or read from a file, start to listen for connections on a [socket](https://en.wikipedia.org/wiki/Network_socket), delete or create directory, or even to finish its work, a program uses a system call. In other words, a system call is just a [C](https://en.wikipedia.org/wiki/C_%28programming_language%29) kernel space function that user space programs call to handle some request.

The Linux kernel provides a set of these functions and each architecture provides its own set. For example: the [x86_64](https://en.wikipedia.org/wiki/X86-64) provides [322](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl) system calls and the [x86](https://en.wikipedia.org/wiki/X86) provides [358](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_32.tbl) different system calls. Ok, a system call is just a function. Let's look on a simple `Hello world` example that's written in the assembly programming language:

```assembly
.data

msg:
    .ascii "Hello, world!\n"
    len = . - msg

.text
    .global _start

_start:
	movq  $1, %rax
    movq  $1, %rdi
    movq  $msg, %rsi
    movq  $len, %rdx
    syscall

    movq  $60, %rax
    xorq  %rdi, %rdi
    syscall
```

We can compile the above with the following commands:

```
$ gcc -c test.S
$ ld -o test test.o
```

and run it as follows:

```
./test
Hello, world!
```

Ok, what do we see here? This simple code represents `Hello world` assembly program for the Linux `x86_64` architecture. We can see two sections here:

* `.data`
* `.text`

The first section - `.data` stores initialized data of our program (`Hello world` string and its length in our case). The second section - `.text` contains the code of our program. We can split the code of our program into two parts: first part will be before the first `syscall` instruction and the second part will be between first and second `syscall` instructions. First of all what does the `syscall` instruction do in our code and generally? As we can read in the [64-ia-32-architectures-software-developer-vol-2b-manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html):

```
SYSCALL invokes an OS system-call handler at privilege level 0. It does so by
loading RIP from the IA32_LSTAR MSR (after saving the address of the instruction
following SYSCALL into RCX). (The WRMSR instruction ensures that the
IA32_LSTAR MSR always contain a canonical address.)
...
...
...
SYSCALL loads the CS and SS selectors with values derived from bits 47:32 of the
IA32_STAR MSR. However, the CS and SS descriptor caches are not loaded from the
descriptors (in GDT or LDT) referenced by those selectors.

Instead, the descriptor caches are loaded with fixed values. It is the respon-
sibility of OS software to ensure that the descriptors (in GDT or LDT) referenced
by those selector values correspond to the fixed values loaded into the descriptor
caches; the SYSCALL instruction does not ensure this correspondence.
```

To summarize, the `syscall` instruction jumps to the address stored in the `MSR_LSTAR` [Model specific register](https://en.wikipedia.org/wiki/Model-specific_register) (Long system target address register). The kernel is responsible for providing its own custom function for handling syscalls as well as writing the address of this handler function to the `MSR_LSTAR` register upon system startup.
The custom function is `entry_SYSCALL_64`, which is defined in [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S#L98). The address of this syscall handling function is written to the `MSR_LSTAR` register during startup in [arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/cpu/common.c#L1335).
```C
wrmsrl(MSR_LSTAR, entry_SYSCALL_64);
```

So, the `syscall` instruction invokes a handler of a given system call. But how does it know which handler to call? Actually it gets this information from the general purpose [registers](https://en.wikipedia.org/wiki/Processor_register). As you can see in the system call [table](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl), each system call has a unique number. In our example the first system call is `write`, which writes data to the given file. Let's look in the system call table and try to find the `write` system call. As we can see, the [write](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L10) system call has number `1`. We pass the number of this system call through the `rax` register in our example. The next general purpose registers: `%rdi`, `%rsi`, and `%rdx` take the three parameters of the `write` syscall. In our case, they are:

* [File descriptor](https://en.wikipedia.org/wiki/File_descriptor) (`1` is [stdout](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_.28stdout.29) in our case)
* Pointer to our string
* Size of data

Yes, you heard right. Parameters for a system call. As I already wrote above, a system call is a just `C` function in the kernel space. In our case first system call is write. This system call defined in the [fs/read_write.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/read_write.c) source code file and looks like:

```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	...
	...
	...
}
```

Or in other words:

```C
ssize_t write(int fd, const void *buf, size_t nbytes);
```

Don't worry about the `SYSCALL_DEFINE3` macro for now, we'll come back to it.

The second part of our example is the same, but we call another system call. In this case we call the [exit](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L69) system call. This system call gets only one parameter:

* Return value

and handles the way our program exits. We can pass the program name of our program to the [strace](https://en.wikipedia.org/wiki/Strace) util and we will see our system calls:

```
$ strace test
execve("./test", ["./test"], [/* 62 vars */]) = 0
write(1, "Hello, world!\n", 14Hello, world!
)         = 14
_exit(0)                                = ?

+++ exited with 0 +++
```

In the first line of the `strace` output, we can see the [execve](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L68) system call that executes our program, and the second and third are system calls that we have used in our program: `write` and `exit`. Note that we pass the parameter through the general purpose registers in our example. The order of the registers is not accidental. The order of the registers is defined by the following agreement - [x86-64 calling conventions](https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions). This, and the other agreement for the `x86_64` architecture are explained in the special document - [System V Application Binary Interface. PDF](https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-r252.pdf). In a general way, argument(s) of a function are placed either in registers or pushed on the stack. The right order is:

* `rdi`
* `rsi`
* `rdx`
* `rcx`
* `r8`
* `r9`

for the first six parameters of a function. If a function has more than six arguments, the remaining parameters will be placed on the stack.

We do not use system calls in our code directly, but our program uses them when we want to print something, check access to a file or just write or read something to it.

For example:

```C
#include <stdio.h>

int main(int argc, char **argv)
{
   FILE *fp;
   char buff[255];

   fp = fopen("test.txt", "r");
   fgets(buff, 255, fp);
   printf("%s\n", buff);
   fclose(fp);

   return 0;
}
```

There are no `fopen`, `fgets`, `printf`, and `fclose` system calls in the Linux kernel, but `open`, `read`, `write`, and `close` instead. I think you know that `fopen`, `fgets`, `printf`, and `fclose` are defined in the `C` [standard library](https://en.wikipedia.org/wiki/GNU_C_Library). Actually, these functions are just wrappers for the system calls. We do not call system calls directly in our code, but instead use these [wrapper](https://en.wikipedia.org/wiki/Wrapper_function) functions from the standard library. The main reason of this is simple: a system call must be performed quickly, very quickly. As a system call must be quick, it must be small. The standard library takes responsibility to perform system calls with the correct parameters and makes different checks before it will call the given system call. Let's compile our program with the following command:

```
$ gcc test.c -o test
```

and examine it with the [ltrace](https://en.wikipedia.org/wiki/Ltrace) util:

```
$ ltrace ./test
__libc_start_main([ "./test" ] <unfinished ...>
fopen("test.txt", "r")                                             = 0x602010
fgets("Hello World!\n", 255, 0x602010)                             = 0x7ffd2745e700
puts("Hello World!\n"Hello World!

)                                                                  = 14
fclose(0x602010)                                                   = 0
+++ exited (status 0) +++
```

The `ltrace` util displays a set of userspace calls of a program. The `fopen` function opens the given text file, the `fgets` function reads file content to the `buf` buffer, the `puts` function prints the buffer to `stdout`, and the `fclose` function closes the file given by the file descriptor. And as I already wrote, all of these functions call an appropriate system call. For example, `puts` calls the `write` system call inside, we can see it if we will add `-S` option to the `ltrace` program:

```
write@SYS(1, "Hello World!\n\n", 14) = 14
```

Yes, system calls are ubiquitous. Each program needs to open/write/read files and network connections, allocate memory, and many other things that can be provided only by the kernel. The [proc](https://en.wikipedia.org/wiki/Procfs) file system contains special files in a format: `/proc/${pid}/syscall` that exposes the system call number and argument registers for the system call currently being executed by the process. For example, pid 1 is [systemd](https://en.wikipedia.org/wiki/Systemd) for me:

```
$ sudo cat /proc/1/comm
systemd

$ sudo cat /proc/1/syscall
232 0x4 0x7ffdf82e11b0 0x1f 0xffffffff 0x100 0x7ffdf82e11bf 0x7ffdf82e11a0 0x7f9114681193
```

the system call with number - `232` which is [epoll_wait](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L241) system call that waits for an I/O event on an [epoll](https://en.wikipedia.org/wiki/Epoll) file descriptor. Or for example `emacs` editor where I'm writing this part:

```
$ ps ax | grep emacs
2093 ?        Sl     2:40 emacs

$ sudo cat /proc/2093/comm
emacs

$ sudo cat /proc/2093/syscall
270 0xf 0x7fff068a5a90 0x7fff068a5b10 0x0 0x7fff068a59c0 0x7fff068a59d0 0x7fff068a59b0 0x7f777dd8813c
```

the system call with the number `270` which is [sys_pselect6](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L279) system call that allows `emacs` to monitor multiple file descriptors.

Now we know a little about system call, what is it and why we need in it. So let's look at the `write` system call that our program used.

Implementation of write system call
--------------------------------------------------------------------------------

Let's look at the implementation of this system call directly in the source code of the Linux kernel. As we already know, the `write` system call is defined in the [fs/read_write.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/read_write.c) source code file and looks like this:

```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);
		ret = vfs_write(f.file, buf, count, &pos);
		if (ret >= 0)
			file_pos_write(f.file, pos);
		fdput_pos(f);
	}

	return ret;
}
```

First of all, the `SYSCALL_DEFINE3` macro is defined in the [include/linux/syscalls.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/syscalls.h) header file and expands to the definition of the `sys_name(...)` function. Let's look at this macro:

```C
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)

#define SYSCALL_DEFINEx(x, sname, ...)                \
        SYSCALL_METADATA(sname, x, __VA_ARGS__)       \
        __SYSCALL_DEFINEx(x, sname, __VA_ARGS__)
```

As we can see the `SYSCALL_DEFINE3` macro takes `name` parameter which will represent name of a system call and variadic number of parameters. This macro just expands to the `SYSCALL_DEFINEx` macro that takes the number of the parameters the given system call, the `_##name` stub for the future name of the system call (more about tokens concatenation with the `##` you can read in the [documentation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html) of [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)). Next we can see the `SYSCALL_DEFINEx` macro. This macro expands to the two following macros:

* `SYSCALL_METADATA`;
* `__SYSCALL_DEFINEx`.

Implementation of the first macro `SYSCALL_METADATA` depends on the `CONFIG_FTRACE_SYSCALLS` kernel configuration option. As we can understand from the name of this option, it allows to enable tracer to catch the syscall entry and exit events. If this kernel configuration option is enabled, the `SYSCALL_METADATA` macro executes initialization of the `syscall_metadata` structure that defined in the [include/trace/syscall.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/trace/syscall.h) header file and contains different useful fields as name of a system call, number of a system call in the system call [table](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl), number of parameters of a system call, list of parameter types and etc:

```C
#define SYSCALL_METADATA(sname, nb, ...)                             \
	...                                                              \
	...                                                              \
	...                                                              \
    struct syscall_metadata __used                                   \
              __syscall_meta_##sname = {                             \
                    .name           = "sys"#sname,                   \
                    .syscall_nr     = -1,                            \
                    .nb_args        = nb,                            \
                    .types          = nb ? types_##sname : NULL,     \
                    .args           = nb ? args_##sname : NULL,      \
                    .enter_event    = &event_enter_##sname,          \
                    .exit_event     = &event_exit_##sname,           \
                    .enter_fields   = LIST_HEAD_INIT(__syscall_meta_##sname.enter_fields), \
             };                                                                            \

    static struct syscall_metadata __used                           \
              __attribute__((section("__syscalls_metadata")))       \
             *__p_syscall_meta_##sname = &__syscall_meta_##sname;
```

If the `CONFIG_FTRACE_SYSCALLS` kernel option is not enabled during kernel configuration, the `SYSCALL_METADATA` macro expands to an empty string:

```C
#define SYSCALL_METADATA(sname, nb, ...)
```

The second macro `__SYSCALL_DEFINEx` expands to the definition of the five following functions:

```C
#define __SYSCALL_DEFINEx(x, name, ...)                                 \
        asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))       \
                __attribute__((alias(__stringify(SyS##name))));         \
                                                                        \
        static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));  \
                                                                        \
        asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));      \
                                                                        \
        asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))       \
        {                                                               \
                long ret = SYSC##name(__MAP(x,__SC_CAST,__VA_ARGS__));  \
                __MAP(x,__SC_TEST,__VA_ARGS__);                         \
                __PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));       \
                return ret;                                             \
        }                                                               \
                                                                        \
        static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

The first `sys##name` is definition of the syscall handler function with the given name - `sys_system_call_name`. The `__SC_DECL` macro takes the `__VA_ARGS__` and combines call input parameter system type and the parameter name, because the macro definition is unable to determine the parameter types. And the `__MAP` macro applies `__SC_DECL` macro to the `__VA_ARGS__` arguments. The other functions that are generated by the `__SYSCALL_DEFINEx` macro are need to protect from the [CVE-2009-0029](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2009-0029) and we will not dive into details about this here. Ok, as result of the `SYSCALL_DEFINE3` macro, we will have:

```C
asmlinkage long sys_write(unsigned int fd, const char __user * buf, size_t count);
```

Now we know a little about the system call's definition and we can go back to the implementation of the `write` system call. Let's look on the implementation of this system call again:

```C
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);
		ret = vfs_write(f.file, buf, count, &pos);
		if (ret >= 0)
			file_pos_write(f.file, pos);
		fdput_pos(f);
	}

	return ret;
}
```

As we already know and can see from the code, it takes three arguments:

* `fd`    - file descriptor;
* `buf`   - buffer to write;
* `count` - length of buffer to write.

and writes data from a buffer declared by the user to a given device or a file. Note that the second parameter `buf`, defined with the `__user` attribute. The main purpose of this attribute is for checking the Linux kernel code with the [sparse](https://en.wikipedia.org/wiki/Sparse) util. It is defined in the [include/linux/compiler.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/compiler.h) header file and depends on the `__CHECKER__` definition in the Linux kernel. That's all about useful meta-information related to our `sys_write` system call, let's try to understand how this system call is implemented. As we can see it starts from the definition of the `f` structure that has `fd` structure type that represents file descriptor in the Linux kernel and we put the result of the call of the `fdget_pos` function. The `fdget_pos` function defined in the same [source](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/read_write.c) code file and just expands the call of the `__to_fd` function:

```C
static inline struct fd fdget_pos(int fd)
{
        return __to_fd(__fdget_pos(fd));
}
```

The main purpose of the `fdget_pos` is to convert the given file descriptor which is just a number to the `fd` structure. Through the long chain of function calls, the `fdget_pos` function gets the file descriptor table of the current process, `current->files`, and tries to find a corresponding file descriptor number there. As we got the `fd` structure for the given file descriptor number, we check it and return if it does not exist. We get the current position in the file with the call of the `file_pos_read` function that just returns `f_pos` field of our file:

```C
static inline loff_t file_pos_read(struct file *file)
{
        return file->f_pos;
}
```

and calls the `vfs_write` function. The `vfs_write` function defined in the [fs/read_write.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/read_write.c) source code file and does the work for us - writes given buffer to the given file starting from the given position. We will not dive into details about the `vfs_write` function, because this function is weakly related to the `system call` concept but mostly about [Virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system) concept which we will see in another chapter. After the `vfs_write` has finished its work, we check the result and if it was finished successfully we change the position in the file with the `file_pos_write` function:

```C
if (ret >= 0)
	file_pos_write(f.file, pos);
```

that just updates `f_pos` with the given position in the given file:

```C
static inline void file_pos_write(struct file *file, loff_t pos)
{
        file->f_pos = pos;
}
```

At the end of the our `write` system call handler, we can see the call of the following function:

```C
fdput_pos(f);
```

unlocks the `f_pos_lock` mutex that protects file position during concurrent writes from threads that share file descriptor.

That's all.

We have seen the partial implementation of one system call provided by the Linux kernel. Of course we have missed some parts in the implementation of the `write` system call, because as I mentioned above, we will see only system calls related stuff in this chapter and will not see other stuff related to other subsystems, such as [Virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system).

Conclusion
--------------------------------------------------------------------------------

This concludes the first part covering system call concepts in the Linux kernel. We have covered the theory of system calls so far and in the next part we will continue to dive into this topic, touching Linux kernel code related to system calls.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](mailto:anotherworldofworld@gmail.com) or just create [issue](https://github.com/0xAX/linux-insides/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

Links
--------------------------------------------------------------------------------

* [system call](https://en.wikipedia.org/wiki/System_call)
* [vdso](https://en.wikipedia.org/wiki/VDSO)
* [vsyscall](https://lwn.net/Articles/446528/)
* [general purpose registers](https://en.wikipedia.org/wiki/Processor_register)
* [socket](https://en.wikipedia.org/wiki/Network_socket)
* [C programming language](https://en.wikipedia.org/wiki/C_%28programming_language%29)
* [x86](https://en.wikipedia.org/wiki/X86)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [x86-64 calling conventions](https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions)
* [System V Application Binary Interface. PDF](http://www.x86-64.org/documentation/abi.pdf)
* [GCC](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [Intel manual. PDF](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [system call table](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl)
* [GCC macro documentation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html)
* [file descriptor](https://en.wikipedia.org/wiki/File_descriptor)
* [stdout](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_.28stdout.29)
* [strace](https://en.wikipedia.org/wiki/Strace)
* [standard library](https://en.wikipedia.org/wiki/GNU_C_Library)
* [wrapper functions](https://en.wikipedia.org/wiki/Wrapper_function)
* [ltrace](https://en.wikipedia.org/wiki/Ltrace)
* [sparse](https://en.wikipedia.org/wiki/Sparse)
* [proc file system](https://en.wikipedia.org/wiki/Procfs)
* [Virtual file system](https://en.wikipedia.org/wiki/Virtual_file_system)
* [systemd](https://en.wikipedia.org/wiki/Systemd)
* [epoll](https://en.wikipedia.org/wiki/Epoll)
* [Previous chapter](https://0xax.gitbook.io/linux-insides/summary/interrupts)


System calls in the Linux kernel. Part 2.
================================================================================

How does the Linux kernel handle a system call
--------------------------------------------------------------------------------

The previous [part](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-1) was the first part of the chapter that describes the [system call](https://en.wikipedia.org/wiki/System_call) concepts in the Linux kernel.
In the previous part we learned what a system call is in the Linux kernel, and in operating systems in general. This was introduced from a user-space perspective, and part of the [write](http://man7.org/linux/man-pages/man2/write.2.html) system call implementation was discussed. In this part we continue our look at system calls, starting with some theory before moving onto the Linux kernel code.

A user application does not make the system call directly from our applications. We did not write the `Hello world!` program like:

```C
int main(int argc, char **argv)
{
	...
	...
	...
	sys_write(fd1, buf, strlen(buf));
	...
	...
}
```

We can use something similar with the help of [C standard library](https://en.wikipedia.org/wiki/GNU_C_Library) and it will look something like this:

```C
#include <unistd.h>

int main(int argc, char **argv)
{
	...
	...
	...
	write(fd1, buf, strlen(buf));
	...
	...
}
```

But anyway, `write` is not a direct system call and not a kernel function. An application must fill general purpose registers with the correct values in the correct order and use the `syscall` instruction to make the actual system call. In this part we will look at what occurs in the Linux kernel when the `syscall` instruction is met by the processor.

Initialization of the system calls table
--------------------------------------------------------------------------------

From the previous part we know that system call concept is very similar to an interrupt. Furthermore, system calls are implemented as software interrupts. So, when the processor handles a `syscall` instruction from a user application, this instruction causes an exception which transfers control to an exception handler. As we know, all exception handlers (or in other words kernel [C](https://en.wikipedia.org/wiki/C_%28programming_language%29) functions that will react on an exception) are placed in the kernel code. But how does the Linux kernel search for the address of the necessary system call handler for the related system call? The Linux kernel contains a special table called the `system call table`. The system call table is represented by the `sys_call_table` array in the Linux kernel which is defined in the [arch/x86/entry/syscall_64.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscall_64.c) source code file. Let's look at its implementation:

```C
asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
    #include <asm/syscalls_64.h>
};
```

As we can see, the `sys_call_table` is an array of `__NR_syscall_max + 1` size where the `__NR_syscall_max` macro represents the maximum number of system calls for the given [architecture](https://en.wikipedia.org/wiki/List_of_CPU_architectures). This book is about the [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture, so for our case the `__NR_syscall_max` is `547` and this is the correct number at the time of writing (current Linux kernel version is `5.0.0-rc7`). We can see this macro in the header file generated by [Kbuild](https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt) during kernel compilation - include/generated/asm-offsets.h`:

```C
#define __NR_syscall_max 547
```

There will be the same number of system calls in the [arch/x86/entry/syscalls/syscall_64.tbl](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl#L331) for the `x86_64`. There are two important topics here; the type of the `sys_call_table` array, and the initialization of elements in this array. First of all, the type. The `sys_call_ptr_t` represents a pointer to a system call table. It is defined as [typedef](https://en.wikipedia.org/wiki/Typedef) for a function pointer that returns nothing and does not take arguments:

```C
typedef void (*sys_call_ptr_t)(void);
```

The second thing is the initialization of the `sys_call_table` array. As we can see in the code above, all elements of our array that contain pointers to the system call handlers point to the `sys_ni_syscall`. The `sys_ni_syscall` function represents not-implemented system calls. To start with, all elements of the `sys_call_table` array point to the not-implemented system call. This is the correct initial behaviour, because we only initialize storage of the pointers to the system call handlers, it is populated later on. Implementation of the `sys_ni_syscall` is pretty easy, it just returns [-errno](http://man7.org/linux/man-pages/man3/errno.3.html) or `-ENOSYS` in our case:

```C
asmlinkage long sys_ni_syscall(void)
{
	return -ENOSYS;
}
```

The `-ENOSYS` error tells us that:

```
ENOSYS          Function not implemented (POSIX.1)
```

Also a note on `...` in the initialization of the `sys_call_table`. We can do it with a [GCC](https://en.wikipedia.org/wiki/GNU_Compiler_Collection) compiler extension called - [Designated Initializers](https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html). This extension allows us to initialize elements in non-fixed order. As you can see, we include the `asm/syscalls_64.h` header at the end of the array. This header file is generated by the special script at [arch/x86/entry/syscalls/syscalltbl.sh](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscalltbl.sh) and generates our header file from the [syscall table](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscalls/syscall_64.tbl). The `asm/syscalls_64.h` contains definitions of the following macros:

```C
__SYSCALL_COMMON(0, sys_read, sys_read)
__SYSCALL_COMMON(1, sys_write, sys_write)
__SYSCALL_COMMON(2, sys_open, sys_open)
__SYSCALL_COMMON(3, sys_close, sys_close)
__SYSCALL_COMMON(5, sys_newfstat, sys_newfstat)
...
...
...
```

The `__SYSCALL_COMMON` macro is defined in the same source code [file](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/syscall_64.c) and expands to the `__SYSCALL_64` macro which expands to the function definition:

```C
#define __SYSCALL_COMMON(nr, sym, compat) __SYSCALL_64(nr, sym, compat)
#define __SYSCALL_64(nr, sym, compat) [nr] = sym,
```

So, after this, our `sys_call_table` takes the following form:

```C
asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
	[0] = sys_read,
	[1] = sys_write,
	[2] = sys_open,
	...
	...
	...
};
```

After this all elements that point to the non-implemented system calls will contain the address of the `sys_ni_syscall` function that just returns `-ENOSYS` as we saw above, and other elements will point to the `sys_syscall_name` functions.

At this point, we have filled the system call table and the Linux kernel knows where each system call handler is. But the Linux kernel does not call a `sys_syscall_name` function immediately after it is instructed to handle a system call from a user space application. Remember the [chapter](https://0xax.gitbook.io/linux-insides/summary/interrupts) about interrupts and interrupt handling. When the Linux kernel gets the control to handle an interrupt, it had to do some preparations like save user space registers, switch to a new stack and many more tasks before it will call an interrupt handler. There is the same situation with the system call handling. The preparation for handling a system call is the first thing, but before the Linux kernel will start these preparations, the entry point of a system call must be initialized and only the Linux kernel knows how to perform this preparation. In the next paragraph we will see the process of the initialization of the system call entry in the Linux kernel.

Initialization of the system call entry
--------------------------------------------------------------------------------

When a system call occurs in the system, where are the first bytes of code that starts to handle it? As we can read in the Intel manual - [64-ia-32-architectures-software-developer-vol-2b-manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html):

```
SYSCALL invokes an OS system-call handler at privilege level 0.
It does so by loading RIP from the IA32_LSTAR MSR
```

it means that we need to put the system call entry in to the `IA32_LSTAR` [model specific register](https://en.wikipedia.org/wiki/Model-specific_register). This operation takes place during the Linux kernel initialization process. If you have read the fourth [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-4) of the chapter that describes interrupts and interrupt handling in the Linux kernel, you know that the Linux kernel calls the `trap_init` function during the initialization process. This function is defined in the [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup.c) source code file and executes the initialization of the `non-early` exception handlers like divide error, [coprocessor](https://en.wikipedia.org/wiki/Coprocessor) error etc. Besides the initialization of the `non-early` exceptions handlers, this function calls the `cpu_init` function from the [arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/cpu/common.c) source code file which besides initialization of `per-cpu` state, calls the `syscall_init` function from the same source code file.

This function performs the initialization of the system call entry point. Let's look on the implementation of this function. It does not take parameters and first of all it fills two model specific registers:

```C
wrmsrl(MSR_STAR,  ((u64)__USER32_CS)<<48  | ((u64)__KERNEL_CS)<<32);
wrmsrl(MSR_LSTAR, entry_SYSCALL_64);
```

The first model specific register - `MSR_STAR` contains `63:48` bits of the user code segment. These bits will be loaded to the `CS` and `SS` segment registers for the `sysret` instruction which provides functionality to return from a system call to user code with the related privilege. Also the `MSR_STAR` contains `47:32` bits from the kernel code that will be used as the base selector for `CS` and `SS` segment registers when user space applications execute a system call. In the second line of code we fill the `MSR_LSTAR` register with the `entry_SYSCALL_64` symbol that represents system call entry. The `entry_SYSCALL_64` is defined in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly file and contains code related to the preparation performed before a system call handler will be executed (I already wrote about these preparations, read above). We will not consider the `entry_SYSCALL_64` now, but will return to it later in this chapter.

After we have set the entry point for system calls, we need to set the following model specific registers:

* `MSR_CSTAR` - target `rip` for the compatibility mode callers;
* `MSR_IA32_SYSENTER_CS` - target `cs` for the `sysenter` instruction;
* `MSR_IA32_SYSENTER_ESP` - target `esp` for the `sysenter` instruction;
* `MSR_IA32_SYSENTER_EIP` - target `eip` for the `sysenter` instruction.

The values of these model specific register depend on the `CONFIG_IA32_EMULATION` kernel configuration option. If this kernel configuration option is enabled, it allows legacy 32-bit programs to run under a 64-bit kernel. In the first case, if the `CONFIG_IA32_EMULATION` kernel configuration option is enabled, we fill these model specific registers with the entry point for the system calls the compatibility mode:

```C
wrmsrl(MSR_CSTAR, entry_SYSCALL_compat);
```

and with the kernel code segment, put zero to the stack pointer and write the address of the `entry_SYSENTER_compat` symbol to the [instruction pointer](https://en.wikipedia.org/wiki/Program_counter):

```C
wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)__KERNEL_CS);
wrmsrl_safe(MSR_IA32_SYSENTER_ESP, 0ULL);
wrmsrl_safe(MSR_IA32_SYSENTER_EIP, (u64)entry_SYSENTER_compat);
```

In another way, if the `CONFIG_IA32_EMULATION` kernel configuration option is disabled, we write `ignore_sysret` symbol to the `MSR_CSTAR`:

```C
wrmsrl(MSR_CSTAR, ignore_sysret);
```

that is defined in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly file and just returns `-ENOSYS` error code:

```assembly
ENTRY(ignore_sysret)
	mov	$-ENOSYS, %eax
	sysret
END(ignore_sysret)
```

Now we need to fill `MSR_IA32_SYSENTER_CS`, `MSR_IA32_SYSENTER_ESP`, `MSR_IA32_SYSENTER_EIP` model specific registers as we did in the previous code when the `CONFIG_IA32_EMULATION` kernel configuration option was enabled. In this case (when the `CONFIG_IA32_EMULATION` configuration option is not set) we fill the `MSR_IA32_SYSENTER_ESP` and the `MSR_IA32_SYSENTER_EIP` with zero and put the invalid segment of the [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table) to the `MSR_IA32_SYSENTER_CS` model specific register:

```C
wrmsrl_safe(MSR_IA32_SYSENTER_CS, (u64)GDT_ENTRY_INVALID_SEG);
wrmsrl_safe(MSR_IA32_SYSENTER_ESP, 0ULL);
wrmsrl_safe(MSR_IA32_SYSENTER_EIP, 0ULL);
```

You can read more about the `Global Descriptor Table` in the second [part](https://0xax.gitbook.io/linux-insides/summary/booting/linux-bootstrap-2) of the chapter that describes the booting process of the Linux kernel.

At the end of the `syscall_init` function, we just mask flags in the [flags register](https://en.wikipedia.org/wiki/FLAGS_register) by writing the set of flags to the `MSR_SYSCALL_MASK` model specific register:

```C
wrmsrl(MSR_SYSCALL_MASK,
	   X86_EFLAGS_TF|X86_EFLAGS_DF|X86_EFLAGS_IF|
	   X86_EFLAGS_IOPL|X86_EFLAGS_AC|X86_EFLAGS_NT);
```

These flags will be cleared during syscall initialization. That's all, it is the end of the `syscall_init` function and it means that system call entry is ready to work. Now we can see what will occur when a user application executes the `syscall` instruction.

Preparation before system call handler will be called
--------------------------------------------------------------------------------

As I already wrote, before a system call or an interrupt handler will be called by the Linux kernel we need to do some preparations. The `idtentry` macro performs the preparations required before an exception handler will be executed, the `interrupt` macro performs the preparations required before an interrupt handler will be called and the `entry_SYSCALL_64` will do the preparations required before a system call handler will be executed.

The `entry_SYSCALL_64` is defined in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S)  assembly file and starts from the following macro:

```assembly
SWAPGS_UNSAFE_STACK
```

This macro is defined in the [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/irqflags.h) header file and expands to the `swapgs` instruction:

```C
#define SWAPGS_UNSAFE_STACK	swapgs
```

which exchanges the current GS base register value with the value contained in the `MSR_KERNEL_GS_BASE ` model specific register. In other words we moved it on to the kernel stack. After this we point the old stack pointer to the `rsp_scratch` [per-cpu](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-1) variable and setup the stack pointer to point to the top of stack for the current processor:

```assembly
movq	%rsp, PER_CPU_VAR(rsp_scratch)
movq	PER_CPU_VAR(cpu_current_top_of_stack), %rsp
```

In the next step we push the stack segment and the old stack pointer to the stack:

```assembly
pushq	$__USER_DS
pushq	PER_CPU_VAR(rsp_scratch)
```

After this we enable interrupts, because interrupts are `off` on entry and save the general purpose [registers](https://en.wikipedia.org/wiki/Processor_register) (besides `bp`, `bx` and from `r12` to `r15`), flags, `-ENOSYS` for the non-implemented system call and code segment register on the stack:

```assembly
ENABLE_INTERRUPTS(CLBR_NONE)

pushq	%r11
pushq	$__USER_CS
pushq	%rcx
pushq	%rax
pushq	%rdi
pushq	%rsi
pushq	%rdx
pushq	%rcx
pushq	$-ENOSYS
pushq	%r8
pushq	%r9
pushq	%r10
pushq	%r11
sub	$(6*8), %rsp
```

When a system call occurs from the user's application, general purpose registers have the following state:

* `rax` - contains system call number;
* `rcx` - contains return address to the user space;
* `r11` - contains register flags;
* `rdi` - contains first argument of a system call handler;
* `rsi` - contains second argument of a system call handler;
* `rdx` - contains third argument of a system call handler;
* `r10` - contains fourth argument of a system call handler;
* `r8`  - contains fifth argument of a system call handler;
* `r9`  - contains sixth argument of a system call handler;

Other general purpose registers (as `rbp`, `rbx` and from `r12` to `r15`) are callee-preserved in [C ABI](http://www.x86-64.org/documentation/abi.pdf)). So we push register flags on the top of the stack, then user code segment, return address to the user space, system call number, first three arguments, dump error code for the non-implemented system call and other arguments on the stack.

In the next step we check the `_TIF_WORK_SYSCALL_ENTRY` in the current `thread_info`:

```assembly
testl	$_TIF_WORK_SYSCALL_ENTRY, ASM_THREAD_INFO(TI_flags, %rsp, SIZEOF_PTREGS)
jnz	tracesys
```

The `_TIF_WORK_SYSCALL_ENTRY` macro is defined in the [arch/x86/include/asm/thread_info.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/thread_info.h) header file and provides set of the thread information flags that are related to the system calls tracing:

```C
#define _TIF_WORK_SYSCALL_ENTRY \
    (_TIF_SYSCALL_TRACE | _TIF_SYSCALL_EMU | _TIF_SYSCALL_AUDIT |   \
    _TIF_SECCOMP | _TIF_SINGLESTEP | _TIF_SYSCALL_TRACEPOINT |     \
    _TIF_NOHZ)
```

We will not consider debugging/tracing related stuff in this chapter, but will see it in the separate chapter that will be devoted to the debugging and tracing techniques in the Linux kernel. After the `tracesys` label, the next label is the `entry_SYSCALL_64_fastpath`. In the `entry_SYSCALL_64_fastpath` we check the `__SYSCALL_MASK` that is defined in the [arch/x86/include/asm/unistd.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/unistd.h) header file and

```C
# ifdef CONFIG_X86_X32_ABI
#  define __SYSCALL_MASK (~(__X32_SYSCALL_BIT))
# else
#  define __SYSCALL_MASK (~0)
# endif
```

where the `__X32_SYSCALL_BIT` is

```C
#define __X32_SYSCALL_BIT	0x40000000
```

As we can see the `__SYSCALL_MASK` depends on the `CONFIG_X86_X32_ABI` kernel configuration option and represents the mask for the 32-bit [ABI](https://en.wikipedia.org/wiki/Application_binary_interface) in the 64-bit kernel.

So we check the value of the `__SYSCALL_MASK` and if the `CONFIG_X86_X32_ABI` is disabled we compare the value of the `rax` register to the maximum syscall number (`__NR_syscall_max`), alternatively if the `CONFIG_X86_X32_ABI` is enabled we mask the `eax` register with the `__X32_SYSCALL_BIT` and do the same comparison:

```assembly
#if __SYSCALL_MASK == ~0
	cmpq	$__NR_syscall_max, %rax
#else
	andl	$__SYSCALL_MASK, %eax
	cmpl	$__NR_syscall_max, %eax
#endif
```

After this we check the result of the last comparison with the `ja` instruction that executes if `CF` and `ZF` flags are zero:

```assembly
ja	1f
```

and if we have the correct system call for this, we move the fourth argument from the `r10` to the `rcx` to keep [x86_64 C ABI](http://www.x86-64.org/documentation/abi.pdf) compliant and execute the `call` instruction with the address of a system call handler:

```assembly
movq	%r10, %rcx
call	*sys_call_table(, %rax, 8)
```

Note, the `sys_call_table` is an array that we saw above in this part. As we already know the `rax` general purpose register contains the number of a system call and each element of the `sys_call_table` is 8-bytes. So we are using `*sys_call_table(, %rax, 8)` this notation to find the correct offset in the `sys_call_table` array for the given system call handler.

That's all. We did all the required preparations and the system call handler was called for the given interrupt handler, for example `sys_read`, `sys_write` or other system call handler that is defined with the `SYSCALL_DEFINE[N]` macro in the Linux kernel code.

Exit from a system call
--------------------------------------------------------------------------------

After a system call handler finishes its work, we will return back to the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S), right after where we have called the system call handler:

```assembly
call	*sys_call_table(, %rax, 8)
```

The next step after we've returned from a system call handler is to put the return value of a system handler on to the stack. We know that a system call returns the result to the user program in the general purpose `rax` register, so we are moving its value on to the stack after the system call handler has finished its work:

```C
movq	%rax, RAX(%rsp)
```

on the `RAX` place.

After this we can see the call of the `LOCKDEP_SYS_EXIT` macro from the [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/irqflags.h):

```assembly
LOCKDEP_SYS_EXIT
```

The implementation of this macro depends on the `CONFIG_DEBUG_LOCK_ALLOC` kernel configuration option that allows us to debug locks on exit from a system call. And again, we will not consider it in this chapter, but will return to it in a separate one. In the end of the `entry_SYSCALL_64` function we restore all general purpose registers besides `rcx` and `r11`, because the `rcx` register must contain the return address to the application that called system call and the `r11` register contains the old [flags register](https://en.wikipedia.org/wiki/FLAGS_register). After all general purpose registers are restored, we fill `rcx` with the return address, `r11` register with the flags and `rsp` with the old stack pointer:

```assembly
RESTORE_C_REGS_EXCEPT_RCX_R11

movq	RIP(%rsp), %rcx
movq	EFLAGS(%rsp), %r11
movq	RSP(%rsp), %rsp

USERGS_SYSRET64
```

In the end we just call the `USERGS_SYSRET64` macro that expands to the call of the `swapgs` instruction which exchanges again the user `GS` and kernel `GS` and the `sysretq` instruction which executes on exit from a system call handler:

```C
#define USERGS_SYSRET64				\
	swapgs;	           				\
	sysretq;
```

Now we know what occurs when a user application calls a system call. The full path of this process is as follows:

* User application contains code that fills general purpose register with the values (system call number and arguments of this system call);
* Processor switches from the user mode to kernel mode and starts execution of the system call entry - `entry_SYSCALL_64`;
* `entry_SYSCALL_64` switches to the kernel stack and saves some general purpose registers, old stack and code segment, flags and etc... on the stack;
* `entry_SYSCALL_64` checks the system call number in the `rax` register, searches a system call handler in the `sys_call_table` and calls it, if the number of a system call is correct;
* If a system call is not correct, jump on exit from system call;
* After a system call handler will finish its work, restore general purpose registers, old stack, flags and return address and exit from the `entry_SYSCALL_64` with the `sysretq` instruction.

That's all.

Conclusion
--------------------------------------------------------------------------------

This is the end of the second part about the system calls concept in the Linux kernel. In the previous [part](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-1) we saw theory about this concept from the user application view. In this part we continued to dive into the stuff which is related to the system call concept and saw what the Linux kernel does when a system call occurs.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](mailto:anotherworldofworld@gmail.com) or just create [issue](https://github.com/0xAX/linux-insides/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

Links
--------------------------------------------------------------------------------

* [system call](https://en.wikipedia.org/wiki/System_call)
* [write](http://man7.org/linux/man-pages/man2/write.2.html)
* [C standard library](https://en.wikipedia.org/wiki/GNU_C_Library)
* [list of cpu architectures](https://en.wikipedia.org/wiki/List_of_CPU_architectures)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [kbuild](https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt)
* [typedef](https://en.wikipedia.org/wiki/Typedef)
* [errno](http://man7.org/linux/man-pages/man3/errno.3.html)
* [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [model specific register](https://en.wikipedia.org/wiki/Model-specific_register)
* [intel 2b manual](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
* [coprocessor](https://en.wikipedia.org/wiki/Coprocessor)
* [instruction pointer](https://en.wikipedia.org/wiki/Program_counter)
* [flags register](https://en.wikipedia.org/wiki/FLAGS_register)
* [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table)
* [per-cpu](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-1)
* [general purpose registers](https://en.wikipedia.org/wiki/Processor_register)
* [ABI](https://en.wikipedia.org/wiki/Application_binary_interface)
* [x86_64 C ABI](http://www.x86-64.org/documentation/abi.pdf)
* [previous chapter](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-1)
