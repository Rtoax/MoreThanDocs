<center><font size='5'>Linux内核深入理解系统调用（3）</font></center>
<center><font size='6'>open 系统调用实现以及资源限制(setrlimit/getrlimit/prlimit)</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

# 1. `open` 系统调用实现

## 1.1. 简介

本节是详述 Linux 内核中的 [系统调用](https://en.wikipedia.org/wiki/System_call) 机制章节的第五部分。之前的内容部分概述了这个机制，现在我将试着详细讲解 Linux 内核中不同系统调用的实现。本章之前的部分和本书其他章节描述的 Linux 内核机制大部分对用户空间是隐约可见或完全不可见。但是 Linux 内核代码不仅仅是有关内核的。大量的内核代码为我们的应用代码提供了支持。通过 Linux 内核，我们的程序可以在不知道 sector,tracks 和磁盘的其他结构的情况下对文件进行读写操作，我们也不需要手动去构造和封装网络数据包就可以通过网络发送数据。

你觉得怎么样，我认为这些非常有趣耶，操作系统如何工作，我们的软件如何与（系统）交互呢。你或许了解，我们的程序通过特定的机制和内核进行交互，这个机制就是[系统调用](https://en.wikipedia.org/wiki/System_call)。因此，我决定去写一些系统调用的实现及其行为，比如我们每天会用到的 `read`,`write`,`open`,`close`,`dup` 等等。

我决定从 [open](http://man7.org/linux/man-pages/man2/open.2.html) 系统调用开始。如果你对 C 程序有了解，你应该知道在我们能对一个文件进行读写或执行其他操作前，我们需要使用 `open` 函数打开这个文件：

```C
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv) {
    int fd = open("test", O_RDONLY);

    if fd < 0 {
        perror("Opening of the file is failed\n");
    }
    else {
        printf("file sucessfully opened\n");
    }
    close(fd); 
    return 0;
}
```

在这样的情况下，`open` 仅是来自标准库中的函数，而不是系统调用。标准库将为我们调用相关的系统调用。`open` 调用将返回一个 [文件描述符](https://en.wikipedia.org/wiki/File_descriptor)。这个文件描述符仅是一个独一无二的数值，在我们的程序里和被打开的文件息息相关。现在我们使用 `open` 调用打开了一个文件并且得到了文件描述符，我们可以和这个文件开始交互了。我们可以写入，读取等等操作。程序中已打开的文件列表可通过 [proc](https://en.wikipedia.org/wiki/Procfs) 文件系统获取：

```
$ sudo ls /proc/1/fd/

0  10  12  14  16  2   21  23  25  27  29  30  32  34  36  38  4   41  43  45  47  49  50  53  55  58  6   61  63  67  8
1  11  13  15  19  20  22  24  26  28  3   31  33  35  37  39  40  42  44  46  48  5   51  54  57  59  60  62  65  7   9
```

我并不打算在这篇文章中以用户空间的视角来描述更多 `open` 例程细节，会更多地从内核的角度来分析。如果你不是很熟悉 `open` 函数，你可以在 [man 手册](http://man7.org/linux/man-pages/man2/open.2.html)获取更多信息。

开始吧！

## 1.2. `open` 系统调用的定义

```C
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile())
		flags |= O_LARGEFILE;

	return do_sys_open(AT_FDCWD, filename, flags, mode);
}
```

如果你阅读过[上一节](https://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-4.html)，你应该知道系统调用通过 `SYSCALL_DEFINE` 宏定义实现。因此，`open` 系统调用也不例外。

`open` 系统调用位于 [fs/open.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/open.c) 源文件中，粗看非常简短

```C
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile())
		flags |= O_LARGEFILE;

	return do_sys_open(AT_FDCWD, filename, flags, mode);
}
```

你或许已经猜到了，同一个[源文件](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/open.c)中的 `do_sys_open` 函数才是主要的。但是在进入这个函数被调用前，我们来看看 `open` 系统调用定义的实现代码中 `if` 分支语句

```C
if (force_o_largefile())
	flags |= O_LARGEFILE;
```

这里可以看到如果 `force_o_largefile()` 返回 true，传递给 `open` 系统调用的 flags 参数会加上了 `O_LARGEFILE` 标志。`O_LARGEFILE` 是什么？阅读 `open(2)` [man 手册](http://man7.org/linux/man-pages/man2/open.2.html) 可以了解到：

> O_LARGEFILE
>
> (LFS) Allow files whose sizes cannot be represented in an off_t (but can be represented in an off64_t) to be opened.

在 [GNU C 标准库参考手册](https://www.gnu.org/software/libc/manual/html_mono/libc.html#File-Position-Primitive)中可以获取更多信息：

> off_t
>
>    This is a signed integer type used to represent file sizes. 
>    In the GNU C Library, this type is no narrower than int.
>    If the source is compiled with _FILE_OFFSET_BITS == 64 this 
>    type is transparently replaced by off64_t.

和

> off64_t
>
>    This type is used similar to off_t. The difference is that 
>    even on 32 bit machines, where the off_t type would have 32 bits,
>    off64_t has 64 bits and so is able to address files up to 2^63 bytes
>    in length. When compiling with _FILE_OFFSET_BITS == 64 this type 
>    is available under the name off_t.

因此不难猜到 `off_t`,`off64_t` 和 `O_LARGEFILE` 是关于文件大小的。就 Linux 内核而言，在32 位系统中打开大文件时如果调用者没有加上 `O_LARGEFILE` 标志，打开大文件的操作就会被禁止。在 64 位系统上，我们在 `open` 系统调用时强制加上了这个标志。[include/linux/fcntl.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/fcntl.h#L7) linux 内核头文件中详述了 `force_o_largefile` 宏：

```C
#ifndef force_o_largefile
#define force_o_largefile() (BITS_PER_LONG != 32)
#endif
```
在5.10.13中为：
```c
#ifndef force_o_largefile
#define force_o_largefile() (!IS_ENABLED(CONFIG_ARCH_32BIT_OFF_T))
#endif
```
这个宏因 CPU 架构有所不同，但在我们当前的情况即 [x86_64](https://en.wikipedia.org/wiki/X86-64) 下，没有提供 `force_o_largefile` 宏的定义，但这个宏在 [include/linux/fcntl.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/fcntl.h#L7)出现了。

因此，正如我们当前了解的， `force_o_largefile` 在我们当前的 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构下就是一个展开为 "true" 值的宏。因此我们正考虑的是 64 位的情况，因此 `force_o_largefile` 将展开为 true 并且 `O_LARGEFILE` 标志将被添加到 `open` 系统调用的 flags 参数中。

现在我们了解 `O_LARGEFILE` 标志和 `force_o_largefile` 宏的意义，我们可以继续讨论 `do_sys_open` 函数的实现。正如我之前所写的，这个函数被定义在[同一个源文件](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/open.c)中，如下：

```C
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_flags op;
	int fd = build_open_flags(flags, mode, &op);
	struct filename *tmp;

	if (fd)
		return fd;

	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}
```

在5.10.13中为：
```c
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_how how = build_open_how(flags, mode);
	return do_sys_openat2(dfd, filename, &how);
}
static long do_sys_openat2(int dfd, const char __user *filename,
			   struct open_how *how)
{
	struct open_flags op;
	int fd = build_open_flags(how, &op);
	struct filename *tmp;

	if (fd)
		return fd;

	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	fd = get_unused_fd_flags(how->flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}
```

让我们试着一步一步理解 `do_sys_open` 如何工作。

## 1.3. open(2) flags 参数

现在你已经知道 `open` 系统调用通过设置第二个参数 flags 来控制打开一个文件并且第三个参数 `mode` 规定创建文件的权限。`do_sys_open` 函数开头调用了 `build_open_flags` 函数，这个函数检查给定的 flags 参数是否有效，并处理不同的 flags 和 mode 条件。

让我们看看 `build_open_flags` 的实现，这个函数被定义在[同一个内核文件](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/open.c)并且需要三个参数：
- flags - 控制打开一个文件
- mode - 新建文件的权限

最后一个参数 - `op` 在 `open_flags` 结构体中表示如下：

```C
struct open_flags {
        int open_flag;
        umode_t mode;
        int acc_mode;
        int intent;
        int lookup_flags;
};
```

这个结构体定义在 [fs/internal.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/internal.h#L99) 头文件中并且我们可以看到这个结构体保存了给内核的 flags 和 权限模式信息，你或许已经猜到了 `build_open_flags` 函数的主要目的就是生成一个 `open_flags` 结构体实例。

`build_open_flags` 函数的实现里定义了一系列局部变量，其中一个是：

```C
int acc_mode = ACC_MODE(flags);
```

这个局部变量表示权限模式，它的初始值会等于 `ACC_MODE` 宏展开的值，这个宏定义在 [include/linux/fs.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/fs.h),看起来非常有趣：

```C
#define ACC_MODE(x) ("\004\002\006\006"[(x)&O_ACCMODE])
#define O_ACCMODE   00000003
```

`"\004\002\006\006"` 是一个四字符的数组：
```
"\004\002\006\006" == {'\004', '\002', '\006', '\006'}
```

因此，`ACC_MODE` 宏就是通过 `[(x) & O_ACCMODE]` 索引展开这个数组里的值。我们可以看到，`O_ACCMODE` == 00000003.通进行 `x & O_ACCMODE`，我们拿最后两个重要的位来表示 `read`,`write` 或 `read/weite` 权限：

```C
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
```
这里给出个示例：
```c
#include <stdio.h>

#define O_ACCMODE	00000003
#define O_RDONLY	00000000
#define O_WRONLY	00000001
#define O_RDWR		00000002

#define ACC_MODE(x) ("\004\002\006\006"[(x)&O_ACCMODE]) 

int main() {
	printf("0x%x\n", ACC_MODE(O_RDONLY));
	printf("0x%x\n", ACC_MODE(O_WRONLY));
	printf("0x%x\n", ACC_MODE(O_RDWR));
}
```

再从数组中计算索引得到值后，`ACC_MODE` 会展开一个文件的权限标志，包含 `MAY_WRITE`,`MAY_READ` 和其他信息。

在我们计算得到初始权限模式后，我们会看到以下条件判断语句：
```C
if (flags & (O_CREAT | __O_TMPFILE))
	op->mode = (mode & S_IALLUGO) | S_IFREG;
else
	op->mode = 0;
```

如果一个被打开的文件不是临时文件并且不是以新建文件方式打开的，我们可以在 `open_flags` 实例中重置模式。这是因为：

> if  neither O_CREAT nor O_TMPFILE is specified, then mode is ignored.

在其他情况下，如果 `O_CREAT` 和 `O_TMPFILE` 标志被传递，我们可以把这个转换为一个规则文件因为 `opendir`(http://man7.org/linux/man-pages/man3/opendir.3.html) 系统调用会创建一个目录。

在接下来的步骤，我们检查一个文件是否被 [fanotify](http://man7.org/linux/man-pages/man7/fanotify.7.html)打开过并且没有 `O_CLOSEXEC` 标志：

```C
flags &= ~FMODE_NONOTIFY & ~O_CLOEXEC;
```

确定没有泄露 [文件描述符](https://en.wikipedia.org/wiki/File_descriptor)。默认地，通过一个 `execve` 系统调用，新的文件描述符会被设置为保持打开（状态），但 `open` 系统调用支持 `O_CLOSEXEC` 标志，这样可以被用来改变默认的操作行为。我们做这些是用来保护文件描述符，这样即使在一个线程中打开一个文件并设置 `O_CLOSEXEC` 标志并且同时第二个程序 [fork](https://en.wikipedia.org/wiki/Fork_\(system_call\)) + [execve](https://en.wikipedia.org/wiki/Exec_\(system_call\)) 操作时不会泄露文件描述符。你应该还记得子程序会有一份父程序文件描述符的副本。

接下来检查 flags 参数是否包含 `O_SYNC` 标志，（如果包含）则外加 `O_DSYNC` 标志：

```
if (flags & __O_SYNC)
	flags |= O_DSYNC;
```

`O_SYNC` 标志确保在所有的数据写入到磁盘前，任何关于写的调用不会返回。`O_DSYNC` 和 `O_SYNC` 类似，但 (`O_DSYNC`) 没有要求所有将被写入的元数据（像 `atime`,`mtime` 等等）等待。所以在 Linux 内核里把 `O_DSYNC` + `__O_SYNC`,实现为 `__O_SYNC|O_DSYNC`。

接下来，必须确认用户是否想要创建一个临时文件，flags 参数应该会包含 `O_TMPFILE_MASK` 或者说，会包含 `O_CREAT` | `O_TMPFILE` 或者 `O_CREAT` & `O_TMPFILE` 的运算结果，并且确保（文件）可写

```C
if (flags & __O_TMPFILE) {
	if ((flags & O_TMPFILE_MASK) != O_TMPFILE)
		return -EINVAL;
	if (!(acc_mode & MAY_WRITE))
		return -EINVAL;
} else if (flags & O_PATH) {
       	flags &= O_DIRECTORY | O_NOFOLLOW | O_PATH;
        acc_mode = 0;
}
```

因为在 man 手册中有提及：

> O_TMPFILE  must  be  specified  with one of O_RDWR or O_WRONLY

如果没有传递 `O_TMPFILE` 标志去创建一个临时文件，在接下来的判断中检查 `O_PATH` 标志。`O_PATH` 标志允许我们在下列情形获得文件描述符：

* 在文件系统(目录)树中指示一个位置
* 仅仅只在文件描述符层面执行操作

在这种情况下文件自身是没有被打开的，但是像 `dup`, `fcntl` 等操作能被使用。因此如果想使用所有与文件内容相关的操作，像 `read`, `write` 等，就（必须）使用 `O_DIRECTORY | O_NOFOLLOW | O_PATH` 标志。现在我们已经在 `build_open_flags` 函数中分析完成了这些标志，我们可以使用下列代码填充我们的 `open_flags->open_flag` ：

```C
op->open_flag = flags;
```

现在我们已经填完了 `open_flag` 中表示对打开文件操作各种控制的 flags 字段和表示新建一个文件的 `umask` 的 `mode` 字段。接下来填充 `open_flags` 结构体中后面的字段。`op->acc_mode` 表示打开文件的权限，我们在 `build_open_flags` 里已经用初始值填完了 `acc_mode` 中的局部变量，接下来检查后面两个与权限相关的 flag：

```C
if (flags & O_TRUNC)
        acc_mode |= MAY_WRITE;
if (flags & O_APPEND)
	acc_mode |= MAY_APPEND;
op->acc_mode = acc_mode;
```

`O_TRUNC` 标志表示如果已打开的文件之前已经存在则删节为 0 ，`O_APPEND` 标志允许以 append mode (追加模式) 打开一个文件。因此在写已打开的文件会追加，而不是覆写。

`open_flags` 中接下来的字段是 - `intent`。它允许我们知道我们的目的，换句话说就是我们真正想对文件做什么，打开，新建，重命名等等操作。如果我们的 flags 参数包含这个 `O_PATH` 标志，即我们不能对文件内容做任何事情，`open_flags` 会被设置为 0 ：

```C
op->intent = flags & O_PATH ? 0 : LOOKUP_OPEN;
```

否则 `open_flags` 会被设置为 `LOOKUP_OPEN`。如果我们想要新建文件，我们可以设置 `LOOKUP_CREATE`，并且使用 `O_EXEC` 标志来确认文件之前不存在：

```C
if (flags & O_CREAT) {
	op->intent |= LOOKUP_CREATE;
	if (flags & O_EXCL)
		op->intent |= LOOKUP_EXCL;
}
```

`open_flags` 结构体里最后的标志是 `lookup_flags`:

```C
if (flags & O_DIRECTORY)
	lookup_flags |= LOOKUP_DIRECTORY;
if (!(flags & O_NOFOLLOW))
	lookup_flags |= LOOKUP_FOLLOW;
op->lookup_flags = lookup_flags;

return 0;
```

如果我们想要打开一个目录，我们可以使用 `LOOKUP_DIRECTORY`；如果想要遍历但不想使用[软链接](https://en.wikipedia.org/wiki/Symbolic_link)，可以使用 `LOOKUP_FOLLOW`。这就是 `build_open_flags` 函数的全部内容了。`open_flags` 结构体也用各种与打开文件相关的 modes 和 flags 填完了。我们可以返回到 `do_sys_open` 函数。


## 1.4. 打开文件的实际操作

在 `build_open_flags` 函数完成后，我们为我们的文件建立了 flags 和 modes ，接下来我们在 `getname` 函数的帮助下得到 `filename` 结构体，得到传递给 `open` 系统调用的文件名：

```C
tmp = getname(filename);
if (IS_ERR(tmp))
	return PTR_ERR(tmp);
```

getname 函数在 [fs/namei.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/namei.c) 源码文件中定义，如下：

```C
struct filename *
getname(const char __user * filename)
{
        return getname_flags(filename, 0, NULL);
}
```

这个函数仅仅调用 `getname_flags` 函数然后返回它的结果。`getname_flags` 函数的主要目的是从用户空间复制文件路径到内核空间。`filename` 结构体被定义在 [include/linux/fs.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/fs.h) 头文件中，包含以下字段：

```c
struct filename {   /*  */
	const char		*name;	    /* 指向内核空间的文件路径指针 pointer to actual string */
	const __user char	*uptr;	/* 用户空间的原始指针 original userland pointer */
	int			refcnt;         /* 引用计数 */
	struct audit_names	*aname; /* 来自 audit 上下文的文件名 */
	const char		iname[];    /* 文件名，长度小于 `PATH_MAX` */
};
```

* name - 指向内核空间的文件路径指针
* uptr - 用户空间的原始指针
* aname - 来自 audit 上下文的文件名
* refcnt - 引用计数
* iname - 文件名，长度小于 `PATH_MAX`

如上所述，`getname_flags` 函数使用 `strncpy_from_user` 函数复制传递给 `open` 系统调用的用户空间的文件名到内核空间。接下来就是获取新的空闲文件描述符：

```C
fd = get_unused_fd_flags(flags);
```

`get_unused_fd_flags` 函数获取当前程序打开文件的（文件描述符）表，系统中文件描述符 minimum (`0`) 和 maximum (`RLIMIT_NOFILE`) 可能的值和我们已传递到 `open` 系统调用的标志，并分配文件描述符，将其在当前进程的文件描述符表中的标记为忙碌状态。`get_unused_fd_flags` 函数设置或清除 `O_CLOEXEC` 标志取决于传递过来 flags 参数状态。

`do_sys_open` 最后主要的步骤就是 `do_filp_open`函数:

```C
struct file *f = do_filp_open(dfd, tmp, &op);

if (IS_ERR(f)) {
	put_unused_fd(fd);
	fd = PTR_ERR(f);
} else {
	fsnotify_open(f);
	fd_install(fd, f);
}
```

`do_filp_open()` 函数主要解析给定的文件路径名到 `file` 结构体，`file` 结构体描述一个程序里已打开的文件。如果传过来的参数有误，则 `do_filp_open` 执行失败，并使用 `put_unused_fd` 释放文件描述符。如果 `do_filp_open()` 执行成功并返回 `file` 结构体，将会在当前程序的文件描述符表中存储这个 `file` 结构体。

现在让我们来简短看下 `do_filp_open()` 函数的实现。这个函数定义在 [fs/namei.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/namei.c) Linux 内核源码中，函数开始就初始化了 `nameidata` 结构体。这个结构体提供了一个链接到文件 [inode](https://en.wikipedia.org/wiki/Inode)。事实上，这就是一个 `do_filp_open()` 函数指针，这个函数通过传递到 `open` 系统调用的的文件名获取 `inode` ，在 `nameidata` 结构体被初始化后，`path_openat` 函数会被调用。

```C
filp = path_openat(&nd, op, flags | LOOKUP_RCU);

if (unlikely(filp == ERR_PTR(-ECHILD)))
	filp = path_openat(&nd, op, flags);
if (unlikely(filp == ERR_PTR(-ESTALE)))
	filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
```

注意 `path_openat` 会被调用了三次。事实上，Linux 内核会以 [RCU](https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt) 模式打开文件。这是最有效的打开文件的方式。如果打开失败，内核进入正常模式。第三次调用相对较少（出现），仅在 [nfs](https://en.wikipedia.org/wiki/Network_File_System)  文件系统中使用。`path_openat` 函数执行 `path lookup`，换句话说就是尝试寻找一个与路径相符合的 `dentry` (目录数据结构，Linux 内核用来追踪记录文件在目录里层次结构)。

`path_openat` 函数从调用 `get_empty_flip()` 函数开始。`get_empty_flip()` 分配一个新 `file` 结构体并做一些额外的检查，像我们是否打开超出了系统中能打开的文件的数量等。在我们获得了已分配的新 `file` 结构体后，如果我们给 `open` 系统调用传递了 `O_TMPFILE` | `O_CREATE` 或 `O_PATH` 标志，则调用 `do_tmpfile` 或 `do_o_path` 函数。在我们想要打开已存在的文件和想要读写时这些情况是非常特殊的，因此我们仅考虑常见的情形。

正常情况下，会调用 `path_init` 函数。这个函数在进行真正的路径寻找前执行一些预备工作。包括寻找路径遍历中的开始的位置和元数据像路径中的 `inode` ，`dentry inode` 等。我们可能会遇到根目录的和当前目录的情形，因为我们使用 `AT_CWD` 作为开始指针（查阅本文前面调用 `do_sys_open` 部分）。

`path_init` 之后是 [loop](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/namei.c#L3457)。loop 执行 `link_path_walk` 和 `do_last` 。`link_path_walk` 执行（文件）名解析，也就是说就是开始处理一个给定的路径。这个程序一步一步处理除了最后一个组成部分的文件路径。这个处理包括检查权限和获得文件组成。一旦一个文件的组成部分被获得，它会被传递给 `walk_component` ，这个函数从 `dcache` 更新当前的目录入口或询问底层文件系统。这样的处理过程一直重复到所有的路径组成部分。`link_path_walk` 执行后，`do_last` 函数会基于 `link_path_walk` 返回的结果填入一个 `file` 文件结构体。当我们处理完给定的文件路径中的最后一个组成部分，`do_last` 中的 `vfs_open` 函数将会被调用。

`vfs_open` 这个函数定义在 [fs/open.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/fs/open.c) Linux 内核源文件中，主要的目的是调用一个底层文件系统的打开操作。
```c
/**
 * vfs_open - open the file at the given path
 * @path: path to open
 * @file: newly allocated file with f_flag initialized
 * @cred: credentials to use
 *//*  */
int vfs_open(const struct path *path, struct file *file)
{
	file->f_path = *path;
	return do_dentry_open(file, d_backing_inode(path->dentry), NULL);
}
```
自此我们的讨论结束了，我们不考虑**完整**的 `open` 系统调用的实现。我们跳过了一些内容，像从挂载的文件系统打开文件的处理条件，解析软链接等，但去查阅这些处理特征应该不会很难。这些要素不包括在**通用的** `open` 系统调用实现中，具体特征取决于底层文件系统。如果你对此感兴趣，可查阅 `file_operations.open` 回调函数获得关于 [filesystem](https://github.com/torvalds/linux/tree/master/fs) 更确切的描述。


## 1.5. 总结

Linux 内核中关于不同系统调用的实现的第五部分已经完成了。如果你有任何问题, 可通过 twitter 或邮箱与我联系，[@0xAX](https://twitter.com/0xAX)/[email](anotherworldofworld@gmail.com), 或者提交一个 [issue](https://github.com/0xAX/linux-internals/issues/new). 在接下来的部分, 我们将继续深究 Linux 内核中的系统调用并且看看 [read](http://man7.org/linux/man-pages/man2/read.2.html) 系统调用的实现。

**请谅解英语不是我的母语，对于任何不恰当的表述我深感抱歉。如果你发现任何错误，请在 [linux-insides](https://github.com/0xAX/linux-internals) 给我发 PR  。**

## 1.6. 参考链接

* [system call](https://en.wikipedia.org/wiki/System_call)
* [open](http://man7.org/linux/man-pages/man2/open.2.html)
* [file descriptor](https://en.wikipedia.org/wiki/File_descriptor)
* [proc](https://en.wikipedia.org/wiki/Procfs)
* [GNU C Library Reference Manual](https://www.gnu.org/software/libc/manual/html_mono/libc.html#File-Position-Primitive)
* [IA-64](https://en.wikipedia.org/wiki/IA-64) 
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [opendir](http://man7.org/linux/man-pages/man3/opendir.3.html)
* [fanotify](http://man7.org/linux/man-pages/man7/fanotify.7.html)
* [fork](https://en.wikipedia.org/wiki/Fork_\(system_call\))
* [execve](https://en.wikipedia.org/wiki/Exec_\(system_call\))
* [symlink](https://en.wikipedia.org/wiki/Symbolic_link)
* [audit](https://linux.die.net/man/8/auditd)
* [inode](https://en.wikipedia.org/wiki/Inode)
* [RCU](https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt)
* [read](http://man7.org/linux/man-pages/man2/read.2.html)
* [previous part](https://0xax.gitbooks.io/linux-insides/content/SysCall/syscall-4.html)


# 2. Limits on resources in Linux

Each process in the system uses certain amount of different resources like files, CPU time, memory and so on.

Such resources are not infinite and each process and we should have an instrument to manage it. Sometimes it is useful to know current limits for a certain resource or to change it's value. In this post we will consider such instruments that allow us to get information about limits for a process and increase or decrease such limits.

We will start from userspace view and then we will look how it is implemented in the Linux kernel.

There are three main fundamental [system calls](https://en.wikipedia.org/wiki/System_call) to manage resource limit for a process:

  * `getrlimit`
  * `setrlimit`
  * `prlimit`

The first two allows a process to read and set limits on a system resource. The last one is extension for previous functions. The `prlimit` allows to set and read the resource limits of a process specified by [PID](https://en.wikipedia.org/wiki/Process_identifier). Definitions of these functions looks:

The `getrlimit` is:

```C
int getrlimit(int resource, struct rlimit *rlim);
```

The `setrlimit` is:

```C
int setrlimit(int resource, const struct rlimit *rlim);
```

And the definition of the `prlimit` is:

```C
int prlimit(pid_t pid, int resource, const struct rlimit *new_limit,
            struct rlimit *old_limit);
```

In the first two cases, functions takes two parameters:

  * `resource` - represents resource type (we will see available types later);
  * `rlim` - combination of `soft` and `hard` limits.

There are two types of limits:

  * `soft`
  * `hard`

The first provides actual limit for a resource of a process. The second is a ceiling value of a `soft` limit and can be set only by superuser. So, `soft` limit can never exceed related `hard` limit.

Both these values are combined in the `rlimit` structure:

```C
struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};
```

The last one function looks a little bit complex and takes `4` arguments. Besides `resource` argument, it takes:

  * `pid` - specifies an ID of a process on which the `prlimit` should be executed;
  * `new_limit` - provides new limits values if it is not `NULL`;
  * `old_limit` - current `soft` and `hard` limits will be placed here if it is not `NULL`.

Exactly `prlimit` function is used by [ulimit](https://www.gnu.org/software/bash/manual/html_node/Bash-Builtins.html#index-ulimit) util. We can verify this with the help of [strace](https://linux.die.net/man/1/strace) util.

For example:

```
~$ strace ulimit -s 2>&1 | grep rl

prlimit64(0, RLIMIT_NPROC, NULL, {rlim_cur=63727, rlim_max=63727}) = 0
prlimit64(0, RLIMIT_NOFILE, NULL, {rlim_cur=1024, rlim_max=4*1024}) = 0
prlimit64(0, RLIMIT_STACK, NULL, {rlim_cur=8192*1024, rlim_max=RLIM64_INFINITY}) = 0
```

Here we can see `prlimit64`, but not the `prlimit`. The fact is that we see underlying system call here instead of library call.

Now let's look at list of available resources:

| Resource          | Description
|-------------------|------------------------------------------------------------------------------------------|
| RLIMIT_CPU        | CPU time limit given in seconds                                                          |
| RLIMIT_FSIZE      | the maximum size of files that a process may create                                      |
| RLIMIT_DATA       | the maximum  size  of  the process's data segment                                        |
| RLIMIT_STACK      | the maximum size of the process stack in bytes                                           |
| RLIMIT_CORE       | the maximum size of a [core](http://man7.org/linux/man-pages/man5/core.5.html) file.     |
| RLIMIT_RSS        | the number of bytes that can be allocated for a process in RAM                           |
| RLIMIT_NPROC      | the maximum number of processes that can be created by a user                            |
| RLIMIT_NOFILE     | the maximum number of a file descriptor that can be opened by a process                  |
| RLIMIT_MEMLOCK    | the maximum number of bytes of memory that may be locked into RAM by [mlock](http://man7.org/linux/man-pages/man2/mlock.2.html).|
| RLIMIT_AS         | the maximum size of virtual memory in bytes.                                             |
| RLIMIT_LOCKS      | the maximum number [flock](https://linux.die.net/man/1/flock) and locking related [fcntl](http://man7.org/linux/man-pages/man2/fcntl.2.html) calls|
| RLIMIT_SIGPENDING | maximum number of [signals](http://man7.org/linux/man-pages/man7/signal.7.html) that may be queued for a user of the calling process|
| RLIMIT_MSGQUEUE   | the number of bytes that can be allocated for [POSIX message queues](http://man7.org/linux/man-pages/man7/mq_overview.7.html) |
| RLIMIT_NICE       | the maximum [nice](https://linux.die.net/man/1/nice) value that can be set by a process  |
| RLIMIT_RTPRIO     | maximum real-time priority value                                                         |
| RLIMIT_RTTIME     | maximum number of microseconds that a process may be scheduled under real-time scheduling policy without making blocking system call|

If you're looking into source code of open source projects, you will note that reading or updating of a resource limit is quite widely used operation.

For example: [systemd](https://github.com/systemd/systemd/blob/01a45898fce8def67d51332bccc410eb1e8710e7/src/core/main.c)

```C
/* Don't limit the coredump size */
(void) setrlimit(RLIMIT_CORE, &RLIMIT_MAKE_CONST(RLIM_INFINITY));
```

Or [haproxy](https://github.com/haproxy/haproxy/blob/25f067ccec52f53b0248a05caceb7841a3cb99df/src/haproxy.c):

```C
getrlimit(RLIMIT_NOFILE, &limit);
if (limit.rlim_cur < global.maxsock) {
	Warning("[%s.main()] FD limit (%d) too low for maxconn=%d/maxsock=%d. Please raise 'ulimit-n' to %d or more to avoid any trouble.\n",
		argv[0], (int)limit.rlim_cur, global.maxconn, global.maxsock, global.maxsock);
}
```

We've just saw a little bit about resources limits related stuff in the userspace, now let's look at the same system calls in the Linux kernel.

# 3. Limits on resource in the Linux kernel

Both implementation of `getrlimit` system call and `setrlimit` looks similar. Both they execute `do_prlimit` function that is core implementation of the `prlimit` system call and copy from/to given `rlimit` from/to userspace:

The `getrlimit`:

```C
SYSCALL_DEFINE2(getrlimit, unsigned int, resource, struct rlimit __user *, rlim)
{
	struct rlimit value;
	int ret;

	ret = do_prlimit(current, resource, NULL, &value);
	if (!ret)
		ret = copy_to_user(rlim, &value, sizeof(*rlim)) ? -EFAULT : 0;

	return ret;
}
```

and `setrlimit`:

```C
SYSCALL_DEFINE2(setrlimit, unsigned int, resource, struct rlimit __user *, rlim)
{
	struct rlimit new_rlim;

	if (copy_from_user(&new_rlim, rlim, sizeof(*rlim)))
		return -EFAULT;
	return do_prlimit(current, resource, &new_rlim, NULL);
}
```

Implementations of these system calls are defined in the [kernel/sys.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/sys.c) kernel source code file.

First of all the `do_prlimit` function executes a check that the given resource is valid:

```C
if (resource >= RLIM_NLIMITS)
	return -EINVAL;
```

and in a failure case returns `-EINVAL` error. After this check will pass successfully and new limits was passed as non `NULL` value, two following checks:

```C
if (new_rlim) {
	if (new_rlim->rlim_cur > new_rlim->rlim_max)
		return -EINVAL;
	if (resource == RLIMIT_NOFILE &&
			new_rlim->rlim_max > sysctl_nr_open)
		return -EPERM;
}
```

check that the given `soft` limit does not exceed `hard` limit and in a case when the given resource is the maximum number of a file descriptors that hard limit is not greater than `sysctl_nr_open` value. The value of the `sysctl_nr_open` can be found via [procfs](https://en.wikipedia.org/wiki/Procfs):

```
~$ cat /proc/sys/fs/nr_open 
1048576
```

After all of these checks we lock `tasklist` to be sure that [signal]() handlers related things will not be destroyed while we updating limits for a given resource:

```C
read_lock(&tasklist_lock);
...
...
...
read_unlock(&tasklist_lock);
```

We need to do this because `prlimit` system call allows us to update limits of another task by the given pid. As task list is locked, we take the `rlimit` instance that is responsible for the given resource limit of the given process:

```C
rlim = tsk->signal->rlim + resource;
```

where the `tsk->signal->rlim` is just array of `struct rlimit` that represents certain resources. And if the `new_rlim` is not `NULL` we just update its value. If `old_rlim` is not `NULL` we fill it:

```C
if (old_rlim)
    *old_rlim = *rlim;
```

That's all.

# 4. Conclusion

This is the end of the second part that describes implementation of the system calls in the Linux kernel. If you have questions or suggestions, ping me on Twitter [0xAX](https://twitter.com/0xAX), drop me an [email](anotherworldofworld@gmail.com), or just create an [issue](https://github.com/0xAX/linux-internals/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-internals).**

# 5. Links

* [system calls](https://en.wikipedia.org/wiki/System_call)
* [PID](https://en.wikipedia.org/wiki/Process_identifier)
* [ulimit](https://www.gnu.org/software/bash/manual/html_node/Bash-Builtins.html#index-ulimit)
* [strace](https://linux.die.net/man/1/strace)
* [POSIX message queues](http://man7.org/linux/man-pages/man7/mq_overview.7.html)
