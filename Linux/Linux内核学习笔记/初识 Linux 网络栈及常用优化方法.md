<div align=center>
	<img src="_v_images/20200915124435478_1737.jpg" width="600"> 
</div>
<br/>
<br/>

<center><font size='10'>初识 Linux 网络栈及常用优化方法</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>

[初识 Linux 网络栈及常用优化方法](https://mp.weixin.qq.com/s/mky3ieoKVbCUPFsBFMEleg)

# 1. 文章简介
基于 `ping 流程窥探 Linux 网络子系统`，同时介绍各个模块的优化方法。

# 2. ping 基本原理
Client 端发送 `ICMP ECHO_REQUEST `报文给 Server
Server 端回应` ICMP ECHO_REPLY `报文给 Client
这其中涉及基本的二三层转发原理，比如：**直接路由、间接路由、ARP** 等概念。

这部分不是本文重点，最基本的网络通信原理可以参考这篇文章：

👉 [TCP/IP 入门指导](https://mp.weixin.qq.com/s?__biz=MzU0Mjc1MTcxMg==&mid=2247483848&idx=1&sn=f2dbbb5eb2350a4acdc3bf6059fa3b48&scene=21#wechat_redirect) 

# 3. ping 报文发送流程
## 3.1. 系统调用层

* `sendmsg` 系统调用
* 根据目标地址获取路由信息，决定报文出口
* `ip_append_data() `函数构建` skb`（这时才将报文从用户态拷贝到内核态），将报文放入 `socket buffer`

```c
/*
 *	ip_append_data() and ip_append_page() can make one large IP datagram
 *	from many pieces of data. Each pieces will be holded on the socket
 *	until ip_push_pending_frames() is called. Each piece can be a page
 *	or non-page data.
 *
 *	Not only UDP, other transport protocols - e.g. raw sockets - can use
 *	this interface potentially.
 *
 *	LATER: length must be adjusted by pad at tail, when it is required.
 */
int ip_append_data(struct sock *sk, struct flowi4 *fl4,
		   int getfrag(void *from, char *to, int offset, int len,
			       int odd, struct sk_buff *skb),
		   void *from, int length, int transhdrlen,
		   struct ipcm_cookie *ipc, struct rtable **rtp,
		   unsigned int flags)
{
	struct inet_sock *inet = inet_sk(sk);
	int err;

	if (flags&MSG_PROBE)
		return 0;

	if (skb_queue_empty(&sk->sk_write_queue)) {
		err = ip_setup_cork(sk, &inet->cork.base, ipc, rtp);
		if (err)
			return err;
	} else {
		transhdrlen = 0;
	}

	return __ip_append_data(sk, fl4, &sk->sk_write_queue, &inet->cork.base,
				sk_page_frag(sk), getfrag,
				from, length, transhdrlen, flags);
}

```

* 调用 `ip_push_pending_frames()`，进入 IP 层处理

```c
int ip_push_pending_frames(struct sock *sk, struct flowi4 *fl4)
{
	struct sk_buff *skb;

	skb = ip_finish_skb(sk, fl4);
	if (!skb)
		return 0;

	/* Netfilter gets whole the not fragmented skb. */
	return ip_send_skb(sock_net(sk), skb);
}
```

## 3.2. IP 层

* 填充 IP 头
* 根据 `neighbour` 信息，填充 MAC 头，调用 `dev_queue_xmit()` 进入网络设备层

```c
int dev_queue_xmit(struct sk_buff *skb)
{
	return __dev_queue_xmit(skb, NULL);
}
EXPORT_SYMBOL(dev_queue_xmit);

/**
 *	__dev_queue_xmit - transmit a buffer
 *	@skb: buffer to transmit
 *	@sb_dev: suboordinate device used for L2 forwarding offload
 *
 *	Queue a buffer for transmission to a network device. The caller must
 *	have set the device and priority and built the buffer before calling
 *	this function. The function can be called from an interrupt.
 *
 *	A negative errno code is returned on a failure. A success does not
 *	guarantee the frame will be transmitted as it may be dropped due
 *	to congestion or traffic shaping.
 *
 * -----------------------------------------------------------------------------------
 *      I notice this method can also return errors from the queue disciplines,
 *      including NET_XMIT_DROP, which is a positive value.  So, errors can also
 *      be positive.
 *
 *      Regardless of the return value, the skb is consumed, so it is currently
 *      difficult to retry a send to this method.  (You can bump the ref count
 *      before sending to hold a reference for retry if you are careful.)
 *
 *      When calling this method, interrupts MUST be enabled.  This is because
 *      the BH enable code must have IRQs enabled so that it will not deadlock.
 *          --BLG
 */
static int __dev_queue_xmit(struct sk_buff *skb, struct net_device *sb_dev);
```

## 3.3. 网络设备层

* 选择发送队列
* `__dev_xmit_skb() `尝试直接发送报文，如果网卡繁忙就将报文放入目标发送队列的 `qdisc` 队列，并触发 `NET_TX_SOFTIRQ` 软中断，在后续软中断处理中发送 qdisc 队列中的报文

```c
static inline int __dev_xmit_skb(struct sk_buff *skb, struct Qdisc *q,
				 struct net_device *dev,
				 struct netdev_queue *txq)
{
	spinlock_t *root_lock = qdisc_lock(q);
	struct sk_buff *to_free = NULL;
	bool contended;
	int rc;

	qdisc_calculate_pkt_len(skb, q);

	if (q->flags & TCQ_F_NOLOCK) {
		rc = q->enqueue(skb, q, &to_free) & NET_XMIT_MASK;
		qdisc_run(q);

		if (unlikely(to_free))
			kfree_skb_list(to_free);
		return rc;
	}

	/*
	 * Heuristic to force contended enqueues to serialize on a
	 * separate lock before trying to get qdisc main lock.
	 * This permits qdisc->running owner to get the lock more
	 * often and dequeue packets faster.
	 */
	contended = qdisc_is_running(q);
	if (unlikely(contended))
		spin_lock(&q->busylock);

	spin_lock(root_lock);
	if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
		__qdisc_drop(skb, &to_free);
		rc = NET_XMIT_DROP;
	} else if ((q->flags & TCQ_F_CAN_BYPASS) && !qdisc_qlen(q) &&
		   qdisc_run_begin(q)) {
		/*
		 * This is a work-conserving queue; there are no old skbs
		 * waiting to be sent out; and the qdisc is not running -
		 * xmit the skb directly.
		 */

		qdisc_bstats_update(q, skb);

		if (sch_direct_xmit(skb, q, dev, txq, root_lock, true)) {
			if (unlikely(contended)) {
				spin_unlock(&q->busylock);
				contended = false;
			}
			__qdisc_run(q);
		}

		qdisc_run_end(q);
		rc = NET_XMIT_SUCCESS;
	} else {
		rc = q->enqueue(skb, q, &to_free) & NET_XMIT_MASK;
		if (qdisc_run_begin(q)) {
			if (unlikely(contended)) {
				spin_unlock(&q->busylock);
				contended = false;
			}
			__qdisc_run(q);
			qdisc_run_end(q);
		}
	}
	spin_unlock(root_lock);
	if (unlikely(to_free))
		kfree_skb_list(to_free);
	if (unlikely(contended))
		spin_unlock(&q->busylock);
	return rc;
}
```

* 驱动层发包函数，将 `skb` 指向的报文地址填入 `tx descriptor`，网卡发送报文
* 发送完成后触发中断，回收 `tx descriptor`（实际实现中一般放在驱动 poll 函数中）

## 3.4. 优化点
* socket buffer

应用程序报文首先放入 socket buffer，socket buffer 空间受 `wmem_default` 限制，而 wmem_default 最大值受 wmem_max 限制。

调整方法：

```bash
sysctl -w net.core.wmem_max=xxxxxx -- 限制最大值
sysctl -w net.core.wmem_default=yyyyyy
```

* `qdisc` 队列长度

经过网络协议栈到达网络设备层的时候，报文从 socket buffer 转移至发送队列 qdisc。

调整方法：

```bash
ifconfig eth0 txqueuelen 10000
```
* qdisc 权重

即一次 `NET_TX_SOFTIRQ` 软中断最大发送报文数，默认**64**。

调整方法：

```
sysctl -w net.core.dev_weight=600
```

* tx descriptor 数量

即，多少个发送描述符。

调整方法：

```
ethtool -G eth0 rx 1024 tx 1024
```

* 高级特性：`TSO/GSO/XPS`

参考这篇文章：[Linux环境中的网络分段卸载技术 GSO/TSO/UFO/LRO/GRO](https://rtoax.blog.csdn.net/article/details/108748689)

TSO（TCP Segmentation Offload）：超过 MTU 大小的报文不需要在协议栈分段，直接由网卡分段，降低 CPU 负载。

GSO（Generic Segmentation Offload）：TSO 的软件实现，延迟大报文分段时机到 IP 层结束或者设备层发包前，不同版本内核实现不同。

开启方法：

```
ethtool -K eth0 gso on
ethtool -K eth0 tso on
```
XPS（Transmit Packet Steering）：对于有多队列的网卡，XPS 可以建立 CPU 与 tx queue 的对应关系，对于单队列网卡，XPS 没啥用。

开启方法：

内核配置CONFIG_XPS
建立cpu与发送队列映射：
```bash
echo cpu_mask > /sys/class/net/eth0/queues/tx-<n>/xps_cpus
```

# 4. ping 报文收取过程
## 4.1. 驱动层

* 网卡驱动初始化，分配收包内存（ring buffer），初始化 rx descriptor
* 网卡收到报文，将报文 DMA 到 rx descriptor 指向的内存，并中断通知 CPU
* CPU 中断处理函数：关闭网卡中断开关，触发 NET_RX_SOFTIRQ 软中断
* 软中断调用网卡驱动注册的 poll 函数，收取报文（著名的 NAPI 机制）
* 驱动 poll 函数退出前将已经收取的 rx descriptor 回填给网卡

## 4.2. 协议栈层

* 驱动层调用 `napi_gro_receive()`，开始协议栈处理，对于 ICMP 报文，经过的处理函数：`ip_rcv() -> raw_local_deliver() -> raw_rcv() -> __sock_queue_rcv_skb()`

napi_gro_receive
```c
gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	gro_result_t ret;

	skb_mark_napi_id(skb, napi);
	trace_napi_gro_receive_entry(skb);

	skb_gro_reset_offset(skb);

	ret = napi_skb_finish(napi, skb, dev_gro_receive(napi, skb));
	trace_napi_gro_receive_exit(ret);

	return ret;
}
EXPORT_SYMBOL(napi_gro_receive);
```
ip_rcv
```c
/*
 * IP receive entry point
 */
int ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt,
	   struct net_device *orig_dev)
{
	struct net *net = dev_net(dev);

	skb = ip_rcv_core(skb, net);
	if (skb == NULL)
		return NET_RX_DROP;

	return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING,
		       net, NULL, skb, dev, NULL,
		       ip_rcv_finish);
}
```
raw_local_deliver
```c
int raw_local_deliver(struct sk_buff *skb, int protocol)
{
	int hash;
	struct sock *raw_sk;

	hash = protocol & (RAW_HTABLE_SIZE - 1);
	raw_sk = sk_head(&raw_v4_hashinfo.ht[hash]);

	/* If there maybe a raw socket we must check - if not we
	 * don't care less
	 */
	if (raw_sk && !raw_v4_input(skb, ip_hdr(skb), hash))
		raw_sk = NULL;

	return raw_sk != NULL;

}
```
raw_rcv
```c
int raw_rcv(struct sock *sk, struct sk_buff *skb)
{
	if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb)) {
		atomic_inc(&sk->sk_drops);
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	nf_reset_ct(skb);

	skb_push(skb, skb->data - skb_network_header(skb));

	raw_rcv_skb(sk, skb);
	return 0;
}
```
__sock_queue_rcv_skb
```c
int __sock_queue_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	unsigned long flags;
	struct sk_buff_head *list = &sk->sk_receive_queue;

	if (atomic_read(&sk->sk_rmem_alloc) >= sk->sk_rcvbuf) {
		atomic_inc(&sk->sk_drops);
		trace_sock_rcvqueue_full(sk, skb);
		return -ENOMEM;
	}

	if (!sk_rmem_schedule(sk, skb, skb->truesize)) {
		atomic_inc(&sk->sk_drops);
		return -ENOBUFS;
	}

	skb->dev = NULL;
	skb_set_owner_r(skb, sk);

	/* we escape from rcu protected region, make sure we dont leak
	 * a norefcounted dst
	 */
	skb_dst_force(skb);

	spin_lock_irqsave(&list->lock, flags);
	sock_skb_set_dropcount(sk, skb);
	__skb_queue_tail(list, skb);
	spin_unlock_irqrestore(&list->lock, flags);

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_data_ready(sk);
	return 0;
}
EXPORT_SYMBOL(__sock_queue_rcv_skb);
```

>现在内核都使用 `GRO` 机制将驱动层 skb 上送协议栈，GRO 全称` Generic Receive Offload`，**是网卡硬件的 LRO 功能（Intel 手册使用 RSC 描述）的软件实现**，可以将同一条流的报文聚合后再上送协议栈处理，降低 CPU 消耗，提高网络吞吐量。

* 送入 socket buffer

* 基于 poll/epoll 机制唤醒等待 socket 的进程

## 4.3. 应用读取报文

* 从 socket buffer 的读取文，并拷贝到用户态

## 4.4. 优化点

* rx descriptor 长度

瞬时流量太大，软件收取太慢，rx descriptor 耗尽后肯定丢包。增大 rx descriptor 长度可以减少因为瞬时流量大造成的丢包。但是不能解决性能不足造成的丢包。

调整方法：

```
查看 rx descriptor 长度：ethtool -g eth0
调整 rx descriptor 长度：ethtool -G eth0 rx 1024 tx 1024
```

* 中断

为了充分利用多核 CPU 性能，高性能（现代）网卡一般支持多队列。配合底层 RSS（Receive-Side Scaling）机制，将报文分流到不同的队列。每个队列对应不同的中断，进而可以通过中断亲和性将不同的队列绑定到不同的 CPU。

>PS：只有 `MSI-X` 才支持多队列/多中断。

调整方法：

```
中断亲和性设置：echo cpu_mask > /proc/irq/<irq_no>/smp_affinity
查看队列数：ethtool -l eth0
调整队列数：ethtool -L eth0
```
进阶：队列分流算法设置

* NAPI

网卡中断中触发 `NET_RX_SOFTIRQ` 软中断，软中断中调用驱动 poll 函数，进行轮询收包。NAPI 的好处在于避免了每个报文都触发中断，避免了无意义的上下文切换带来的 Cache/TLB miss 对性能的影响。

但是 Linux 毕竟是通用操作系统，NAPI 轮询收包也要有限制，不能长时间收包，不干其他活。所以 `NET_RX_SOFTIRQ` 软中断有收包 budget 概念。即，一次最大收取的报文数。

收包数超过 `netdev_budget`（默认300）或者收包时间超过2个 jiffies 后就退出，等待下次软中断执行时再继续收包。驱动层 poll 收包函数默认一次收64个报文。

netdev_budget 调整方法：

```
sysctl -w net.core.netdev_budget=600
```
驱动层 poll 函数收包个数只能修改代码。一般不需要修改。


* 开启高级特性 GRO/RPS/RFS

GRO（Generic Receive Offload）：驱动送协议栈时，实现同条流报文汇聚后再上送，提高吞吐量。对应 napi_gro_receive 函数。

开启方法：

```
ethtool -K eth0 gro on
```
RPS（Receive Packet Steering）：对于单队列网卡，RPS 特性可以根据报文 hash 值将报文推送到其它 CPU 处理（把报文压入其它 CPU 队列，然后给其它 CPU 发送 IPI 中断，使其进行收包处理），提高多核利用率，对于多队列网卡，建议使用网卡自带的 RSS 特性分流。

RFS（Receive Flow Steering）：RFS 在 RPS 基础上考虑了报文流的处理程序运行在哪个 CPU 上的问题。比如报文流 A 要被运行在 CPU 2 的 APP A 处理，RPS 特性会根据报文 hash 值送入 CPU 1，而 RFS 特性会识别到 APP A 运行在 CPU 2 信息，将报文流 A 送入CPU 2。

PS： RFS 的核心是感知报文流的处理程序运行在哪个 CPU，核心原理是在几个收发包函数（`inet_recvmsg(), inet_sendmsg(), inet_sendpage()、tcp_splice_read()`）中识别并记录不同报文流的处理程序运行在哪个CPU。

RPS 开启方法：

```
echo cpu_mask > /sys/class/net/eth0/queues/rx-0/rps_cpus
```
RFS 设置方法：

```
sysctl -w net.core.rps_sock_flow_entries=32768
echo 2048 > /sys/class/net/eth0/queues/rx-0/rps_flow_cnt
```
PS： RFS 有些复杂，这里只说举例配置，不讲述原理（其实就是不会。。。）

* socket buffer

__sock_queue_rcv_skb() 中会将 skb 放入 socket buffer，其中会检查 socket buffer 是否溢出。所以要保证 socket buffer 足够大。

设置方法：

```
sysctl -w net.core.rmem_max=xxxxxx -- 限制最大值
sysctl -w net.core.rmem_default=yyyyyy
```

# 5. 参考文档
* scaling: 👉https://github.com/torvalds/linux/blob/v3.13/Documentation/networking/scaling.txt
* monitoring-tuning-linux-networking-stack-receiving-data: 👉https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/
* monitoring-tuning-linux-networking-stack-sending-data: 👉https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data/


>作者简介：刘立超，靠 Linux 内核，驱动吃饭的工程师，喜欢探究事物本质，喜欢分享。
>版权声明：本文最先发表于 “泰晓科技” 微信公众号，欢迎转载，转载时请在文章的开头保留本声明。


<br/>
<div align=right>以上内容由RTOAX翻译整理自网络。
	<img src="_v_images/20200915124533834_25268.jpg" width="40"> 
</div>