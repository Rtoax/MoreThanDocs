<div align=center>
	<img src="_v_images/20200910110325796_584.png" width="600"> 
</div>
<br/>
<br/>

<center><font size='6'>DPDK笔记 helloworld示例代码解析</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2021年4月</font></center>
<br/>
<br/>


代码很简单：
```c
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

static int
lcore_hello(__rte_unused void *arg)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	/* call lcore_hello() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}

	/* call it on master lcore too */
	lcore_hello(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
```
`rte_eal_init`初始化EAL环境，在所有的slave上执行lcore_hello函数：
```c
	/* call lcore_hello() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}
```

然后在master上也执行hello函数，最终执行`rte_eal_wait_lcore`，等待slave结束。

```c
int
rte_eal_wait_lcore(unsigned slave_id)
{
	if (lcore_config[slave_id].state == WAIT)
		return 0;

	while (lcore_config[slave_id].state != WAIT &&
	       lcore_config[slave_id].state != FINISHED)
		rte_pause();

	rte_rmb();

	/* we are in finished state, go to wait state */
	lcore_config[slave_id].state = WAIT;
	return lcore_config[slave_id].ret;
}
```

而`rte_eal_remote_launch`很简单，通过使用pipe和master进行读写：

```c
int
rte_eal_remote_launch(int (*f)(void *), void *arg, unsigned slave_id)
{
	int n;
	char c = 0;
	int m2s = lcore_config[slave_id].pipe_master2slave[1];
	int s2m = lcore_config[slave_id].pipe_slave2master[0];
	int rc = -EBUSY;

	if (lcore_config[slave_id].state != WAIT)
		goto finish;

	lcore_config[slave_id].f = f;
	lcore_config[slave_id].arg = arg;

	/* send message */
	n = 0;
	while (n == 0 || (n < 0 && errno == EINTR))
		n = write(m2s, &c, 1);
	if (n < 0)
		rte_panic("cannot write on configuration pipe\n");

	/* wait ack */
	do {
		n = read(s2m, &c, 1);   
	} while (n < 0 && errno == EINTR);

	if (n <= 0)
		rte_panic("cannot read on configuration pipe\n");

	rc = 0;
finish:
	rte_eal_trace_thread_remote_launch(f, arg, slave_id, rc);
	return rc;
}
```




















