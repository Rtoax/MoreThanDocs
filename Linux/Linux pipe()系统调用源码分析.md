<center><font size='6'>Linux pipe()系统调用源码分析</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年4月29日</font></center>
<br/>


* 内核版本：linux-5.10.13
* 注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* Pipe示例代码：[https://github.com/Rtoax/test/tree/master/c/glibc/unistd/pipe-demo2.c](https://github.com/Rtoax/test/tree/master/c/glibc/unistd/pipe-demo2.c)

# 1. 函数原型
## 1.1. 用户态封装

```c
#include <unistd.h>

int pipe(int pipefd[2]);

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>

int pipe2(int pipefd[2], int flags);
```

## 1.2. 内核态入口
```c
SYSCALL_DEFINE2(pipe2, int __user *, fildes, int, flags)
{
	return do_pipe2(fildes, flags);
}

SYSCALL_DEFINE1(pipe, int __user *, fildes) /* pipe() 系统调用 */
{
	return do_pipe2(fildes, 0);
}
```

关于`flags`的定义，支持一下两种：

* `O_NONBLOCK`：非阻塞
* `O_CLOEXEC`：fork和exec时是否关闭

# 2. do_pipe2

```c
static int do_pipe2(int __user *fildes, int flags);
```

这个函数并不长，调用`__do_pipe_flags`分配两个`struct file`数据结构，一个用来读，一个用来写。然后调用`copy_to_user`将两个`fd`拷贝至用户态，如果失败了就是用`fput`和`put_unused_fd`分别将`fd`和`file`归还。如果成功，那么就将`fd`和`file`安装到当前进程的打开文件表中。到此`do_pipe2`函数就结束了，怎么样，简单吧。下面来看`__do_pipe_flags`。


## 2.1. __do_pipe_flags

函数开头检查标志位
```c
	if (flags & ~(O_CLOEXEC | O_NONBLOCK | O_DIRECT | O_NOTIFICATION_PIPE))
		return -EINVAL;
```
接着使用`create_pipe_files`创建两个`file`结构。然后，使用`get_unused_fd_flags`分别获取两个未使用的文件描述符`fdr`和`fdw`，分别对应读和写。关于审计`audit_fd_pair`本文不做讨论。然后`__do_pipe_flags`也结束了，是不是仍旧很简单。

接下来分析`create_pipe_files`。


### 2.1.1. create_pipe_files

首先为管道分配一个`inode`：
```c
struct inode *inode = get_pipe_inode();
if (!inode)
	return -ENFILE;
```
如果分配失败，返回文件表溢出错误。如果内核编译选项定义了`CONFIG_WATCH_QUEUE`，这里会有一段监控这个pipe文件的watch动作，本不做讨论。然后，使用`alloc_file_pseudo`为写端申请一个file：

```c
    /* 分配 file 写端 */
	f = alloc_file_pseudo(inode, pipe_mnt, "",
				O_WRONLY | (flags & (O_NONBLOCK | O_DIRECT)),
				&pipefifo_fops);    /*  */
```
file的私有数据指向inode的i_pipe：
```c
	f->private_data = inode->i_pipe;    /* file的私有数据为 inode pipe */
```
在inode结构中，有一个联合体：
```c
	union {
		struct pipe_inode_info	*i_pipe;    /* pipe info */
		struct block_device	*i_bdev;
		struct cdev		*i_cdev;
		char			*i_link;
		unsigned		i_dir_seq;
	};
```
接着，调用`alloc_file_clone`分配一个读端file结构：
```c
    /* 分配 file 读端 */
	res[0] = alloc_file_clone(f, O_RDONLY | (flags & O_NONBLOCK),
				  &pipefifo_fops);
```
可见，读写公用同一个文件操作符结构`pipefifo_fops`，我们看看他的定义：
```c
const struct file_operations pipefifo_fops = {  /* pipe 管道 操作符 */
	.open		= fifo_open,    /* 打开管道 */
	.llseek		= no_llseek,    /*  */
	.read_iter	= pipe_read,    /* 读 */
	.write_iter	= pipe_write,   /* 写 */
	.poll		= pipe_poll,    /*  */
	.unlocked_ioctl	= pipe_ioctl,   /*  */
	.release	= pipe_release,     /*  */
	.fasync		= pipe_fasync,      /*  */
	.splice_write	= iter_file_splice_write,   /*  */
};
```
对于这个结构，本文只关注`fifo_open`、`pipe_read`和`pipe_write`这三个函数，对于`iter_file_splice_write`，这涉及到了`splice`系统调用，本文不做讨论。

接着，将对应的file结构赋值返回。

## 2.2. get_unused_fd_flags

这个函数调用比较复杂，但是原理很简单，就是从本进程的文件描述符表中获取下一个没有使用的fd，可参见函数`find_next_fd`。


接着`__do_pipe_flags`执行：

```c
	fd[0] = fdr;    /* 读 */
	fd[1] = fdw;    /* 写 */
```
然后返回，这就创建好了管道。


# 3. 打开管道

根据文章开头给出的实例代码，当使用系统调用pipe创建了管道后，可以使用`fdopen`函数打开管道描述符

```c
#include <stdio.h>
FILE *fdopen(int fildes, const char *mode);
```
其底层是系统调用open。

> 注意
> 关于open系统调用，会单独讲解，此处简要说明。

```c
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile()) /* x86_64 恒定为 true */
		flags |= O_LARGEFILE;
	return do_sys_open(AT_FDCWD, filename, flags, mode);
}
```

其调用关系为：
```c
do_sys_open
    do_sys_openat2
        do_filp_open
            path_openat
                do_open
                    vfs_open
                        do_dentry_open
                            执行 pipefifo_fops->pipe_open
```

## 3.1. pipe_open

```c
static int fifo_open(struct inode *inode, struct file *filp)
```

关于pipe文件，有一个magic用于区分：
```c
bool is_pipe = inode->i_sb->s_magic == PIPEFS_MAGIC;
```
首先判断`inode->i_pipe`是否为空，如果为空，使用`alloc_pipe_info`申请一个`struct pipe_inode_info`结构并将其赋值`inode->i_pipe = pipe;`将file私有数据指向这个分配的数据结构`filp->private_data = pipe;`，下面我们先看一下`alloc_pipe_info`函数。

### 3.1.1. alloc_pipe_info

使用`kzalloc`分配，所以，注意此时结构`pipe_inode_info`的所有字段为0，出去接下来需要填充的字段，这两个字段初始化后为0：
```c
	unsigned int head;
	unsigned int tail;
```
这里有个默认值`unsigned long pipe_bufs = PIPE_DEF_BUFFERS;`大小为`16`，也就是pipe队列的缓冲区大小默认为16个page大小。同时，系统中还有个全局变量`unsigned int pipe_max_size = 1048576`，在page大小为4K的配置下，这个数值等于256个page大小。当然，代码中对这进行了审计：
```c
	if (pipe_bufs * PAGE_SIZE > max_size && !capable(CAP_SYS_RESOURCE))
		pipe_bufs = max_size >> PAGE_SHIFT;
```
接下来使用`kcalloc`分配pipe_buffer结构。并进行初始值设定。

```c
    /* 分配 pipe_buffer 数据结构 */
	pipe->bufs = kcalloc(pipe_bufs, sizeof(struct pipe_buffer),
			     GFP_KERNEL_ACCOUNT);
    /* 如果分配成功 */
	if (pipe->bufs) {
		init_waitqueue_head(&pipe->rd_wait);
		init_waitqueue_head(&pipe->wr_wait);
		pipe->r_counter = pipe->w_counter = 1;
		pipe->max_usage = pipe_bufs;
		pipe->ring_size = pipe_bufs;    /* 16 */
		pipe->nr_accounted = pipe_bufs;
		pipe->user = user;
		mutex_init(&pipe->mutex);
		return pipe;
	}
```
下面回到`fifo_open`函数。

在申请完数据结构后，首先加锁`__pipe_lock(pipe);`，这是个mutex互斥锁。

接着判断`switch (filp->f_mode & (FMODE_READ | FMODE_WRITE))`，在对应的case分别进行`pipe->readers++`和`pipe->writers++`，接着释放锁。

> wake_up_partner
> 这将激活/告知其他的读者或者写者。

> 注意
> 请注意，至此，管道底层还没有申请page页。

# 4. 写管道

关于系统调用`write`不做过多说明，只给出调用栈：

```c
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	return ksys_write(fd, buf, count);
}

ksys_write
    vfs_write
        if (file->f_op->write)
            file->f_op->write(...)
        else if (file->f_op->write_iter)
            new_sync_write
                call_write_iter
                    file->f_op->write_iter
                        pipefifo_fops.pipe_write()
```

## 4.1. pipe_write

* 计算写长度`size_t total_len = iov_iter_count(from);`
* 给队列加锁`__pipe_lock(pipe);`。

如果`pipe->readers<=0`，返回broken pipe错误。这里给一个小程序，在创建管道后，我将读方关闭，然后在写方写入数据，这时，收到SIGPIPE信号（SIGPIPE默认忽略）：
```c
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

void sig_handler(int signum) {
	switch(signum) {
		case SIGPIPE:
			printf("Broken Pipe.\n");
		break;
		default:
		break;
	}
}

int main() {
	int fds[2];

	signal(SIGPIPE, sig_handler);
	pipe(fds);

	close(fds[0]);
	write(fds[1], "hello", 5);
}
```
运行结果：
```c
[rongtao@localhost unistd]$ gcc pipe-EPIPE.c
[rongtao@localhost unistd]$ ./a.out 
Broken Pipe.
```
上面的代码也就是由于下面的代码造成的：
```c
	if (!pipe->readers) {
		send_sig(SIGPIPE, current, 0);  /* broken pipe */
		ret = -EPIPE;
		goto out;
	}
```
而这个readers是在pipe_release中递减的：
```c
	if (file->f_mode & FMODE_READ)
		pipe->readers--;
	if (file->f_mode & FMODE_WRITE)
		pipe->writers--;
```
总体的意思就是，当写者写的时候，必须有读者存在。

接着判断需要写的数据不为空，并且当前队列不为空：
```c
if (chars && !was_empty)
```
调用`copy_page_from_iter`将用户buffer拷贝至内核的page页中，并作出相应的标记。记录长度`buf->len += ret;`。接着判断是否已经将全部的用户buffer写入，如果是，那么直接返回，如果不是，那么继续执行。
```c
    /* 如果把东西都写完了，直接退出
     * 如果没写完，继续下面的执行，将会分配 page 页*/
	if (!iov_iter_count(from))
		goto out;
```
上述代码如果不成立，也就是说没有跳转到out label处，就进入一个死循环`for (;;)`。

真啰嗦，再次检测是不是还有读者`if (!pipe->readers)`。如果队列不是满的`if (!pipe_full(head, pipe->tail, pipe->max_usage))`获取`pipe_buffer`结构，并判断page是否为空，如果为空，使用`alloc_page`分配一个page。
如果此时队列是满的`if (pipe_full(head, pipe->tail, pipe->max_usage))`，调到for循环起始处再次执行，否则，更新头指针`pipe->head = head + 1;`，还是使用`copy_page_from_iter`将剩下的用户buffer拷贝至page中。如果没有更所的用户数据需要写入，就可以退出for循环了：
```c
if (!iov_iter_count(from))  /* 如果都写完了，退出循环 */
    break;
```
每一次循环都会价差当前进程是否信号挂起，如果是，就先退出，转而处理信号

```c
if (signal_pending(current)) {
	if (!ret)
		ret = -ERESTARTSYS;
	break;
}
```
接着，释放锁`__pipe_unlock(pipe);`，接下来根据队列是否为空标志判断是否需要唤醒读者，然后退出。

# 5. 读管道

同样只给出简单的调用关系：
```c
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
	return ksys_read(fd, buf, count);
}

ksys_read
    vfs_read
        if (file->f_op->read)
            file->f_op->read();
        else if (file->f_op->read_iter)
            new_sync_read
                call_read_iter
                    file->f_op->read_iter(kio, iter)
                        pipe_read()
```

## 5.1. pipe_read

整体上与写基本相同，有以下几点不同之处：

* 读者不再产生SIGPIPE信号；
* 读者根据用户态buffer长度来决定具体读取多少数据；
* 若数据不能一次性读取，本读者将通知下一个读者（唤醒）；


# 6. 思考

综上所述，有没有什么是值得优化的地方呢？

比如说这个操作：

```bash
ls -a | grep mm
```
以上的管道操作，当ls -a产生的数据量非常少，那么当grep读取数据时创建的整个page有没有优化的余地。可不可以申请一个slab告诉缓存对直接分配page进行优化呢？

这篇文章就写到这里吧。




