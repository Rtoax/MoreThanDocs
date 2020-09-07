<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>



# 1. 构建VPP
要开始使用VPP进行开发，您需要获取所需的VPP源，然后构建软件包。有关构建系统的更多详细信息，请参阅Build System。

## 1.1. 设置代理
根据您所使用的环境，可能需要设置代理。运行以下代理命令以指定代理服务器名称和相应的端口号：

```
$ export http_proxy=http://<proxy-server-name>.com:<port-number>
$ export https_proxy=https://<proxy-server-name>.com:<port-number>
```
## 1.2. 获取VPP来源
要获取用于创建内部版本的VPP源，请运行以下命令：

```
$ git clone https://gerrit.fd.io/r/vpp
$ cd vpp
```
## 1.3. 建立VPP依赖关系
在构建VPP映像之前，通过输入以下命令，确保没有安装FD.io VPP或DPDK软件包：

```
$ dpkg -l | grep vpp
$ dpkg -l | grep DPDK
```
运行上述命令后，应该没有输出或没有显示任何程序包。

运行以下make命令以安装FD.io VPP的依赖项。

如果下载在任何时候都挂起，则可能需要 设置代理才能使下载正常工作。

```
$ make install-dep
Hit:1 http://us.archive.ubuntu.com/ubuntu xenial InRelease
Get:2 http://us.archive.ubuntu.com/ubuntu xenial-updates InRelease [109 kB]
Get:3 http://security.ubuntu.com/ubuntu xenial-security InRelease [107 kB]
Get:4 http://us.archive.ubuntu.com/ubuntu xenial-backports InRelease [107 kB]
Get:5 http://us.archive.ubuntu.com/ubuntu xenial-updates/main amd64 Packages [803 kB]
Get:6 http://us.archive.ubuntu.com/ubuntu xenial-updates/main i386 Packages [732 kB]
...
...
Update-alternatives: using /usr/lib/jvm/java-8-openjdk-amd64/bin/jmap to provide /usr/bin/jmap (jmap) in auto mode
Setting up default-jdk-headless (2:1.8-56ubuntu2) ...
Processing triggers for libc-bin (2.23-0ubuntu3) ...
Processing triggers for systemd (229-4ubuntu6) ...
Processing triggers for ureadahead (0.100.0-19) ...
Processing triggers for ca-certificates (20160104ubuntu1) ...
Updating certificates in /etc/ssl/certs...
0 added, 0 removed; done.
Running hooks in /etc/ca-certificates/update.d...

done.
done.
```
## 1.4. 生成VPP（调试）
此构建版本包含调试符号，这些调试符号对于修改VPP非常有用。下面的 make命令可构建VPP的调试版本。生成调试映像时，二进制文件可以在/ build-root / vpp_debug-native中找到。

调试内部版本包含调试符号，这些符号对于故障排除或修改VPP非常有用。下面的make命令可生成VPP的调试版本。可以在/ build-root / vpp_debug-native中找到用于构建调试映像的二进制文件。

```
$ make build
make[1]: Entering directory '/home/vagrant/vpp-master/build-root'
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for dpdk @@@@
@@@@ Makefile fragment found in /home/vagrant/vpp-master/build-data/packages/dpdk.mk @@@@
@@@@ Source found in /home/vagrant/vpp-master/dpdk @@@@
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for vpp @@@@
@@@@ Makefile fragment found in /home/vagrant/vpp-master/build-data/packages/vpp.mk @@@@
@@@@ Source found in /home/vagrant/vpp-master/src @@@@
...
...
make[5]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp/vpp-api/java'
make[4]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp/vpp-api/java'
make[3]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp'
make[2]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp'
@@@@ Installing vpp: nothing to do @@@@
make[1]: Leaving directory '/home/vagrant/vpp-master/build-root'
```
## 1.5. `生成VPP`（发行版）
本节介绍如何构建FD.io VPP的常规发行版。该发行版本经过优化，不会创建任何调试符号。可在/ build-root / vpp-native中找到用于构建发行映像的二进制文件。

在下面使用以下make命令来构建FD.io VPP的发行版本。

```
$ make build-release
```
## 1.6. 构建必要的软件包
需要构建的软件包取决于VPP将在其上运行的系统类型：

* 在Debian软件包如果VPP是要在Ubuntu上运行是建立
* 该RPM软件包内置如果VPP是要在CentOS或红帽运行

## 1.7. 构建Debian软件包
要构建debian软件包，请使用以下命令：

```
$ make pkg-deb
```
## 1.8. 建立RPM套件
要生成rpm软件包，请根据系统使用以下命令之一：

```
$ make pkg-rpm
```
一旦构建了软件包，就可以在build-root目录中找到它们。

```
$ ls *.deb

If the packages are built correctly, then this should be the corresponding output:

vpp_18.07-rc0~456-gb361076_amd64.deb             vpp-dbg_18.07-rc0~456-gb361076_amd64.deb
vpp-dev_18.07-rc0~456-gb361076_amd64.deb         vpp-api-lua_18.07-rc0~456-gb361076_amd64.deb
vpp-lib_18.07-rc0~456-gb361076_amd64.deb         vpp-api-python_18.07-rc0~456-gb361076_amd64.deb
vpp-plugins_18.07-rc0~456-gb361076_amd64.deb
```
最后，可以使用以下命令安装创建的软件包。安装与VPP将在其上运行的操作系统相对应的软件包：

对于Ubuntu：

```
$ sudo bash
# 4. dpkg -i *.deb
```
对于Centos或Redhat：

```
$ sudo bash
# 4. rpm -ivh *.rpm
```
