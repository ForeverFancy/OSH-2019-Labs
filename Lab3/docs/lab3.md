# Lab3

## 使用方法

在 files 目录下：

```bash
$ make
$ make clean
$ sudo ./server
```

## 设计思路

- Linux 中默认的 socket 都是阻塞 I/O 模式，但是在网络 I/O 的情况下效率非常低，一旦有数据没有准备好或者对方的连接暂时不可用，那么 I/O 进程就会阻塞，服务器无法处理其他客户的连接请求。
- 为了提高服务器的并发处理能力，本次实验设计采用 epoll 的方法来处理并发请求。epoll 的好处在于单线程即可处理高并发的 I/O 请求。
- epoll 属于事件触发型处理，且 I/O 处理都是非阻塞的，当有客户连接请求时，建立连接，并监听客户的发送请求。
- 当收到客户的发送请求时开始处理（`handle_read`函数处理），当收到 EAGAIN 信号时说明没有完全读完请求，但此时没有数据可读，再接收请求会发生阻塞，这时可以直接返回处理下一个客户，避免阻塞。
- 当请求全部接收完成之后，改为监听该客户的 EPOLLOUT 事件，由`handle_write`函数处理。`handle_write`会解析之前收到的客户的 HTTP 请求，返回请求的文件路径。根据打开文件是否成功返回 HTTP 相应头，如果请求资源合法，那么将会调用`sendfile`向客户发送请求的资源。
- `sendfile`是一个比较高效的传输文件的系统调用，速度比`read`文件之后再`write`要快一些。
- > `sendfile()` 系统调用利用 DMA 引擎将文件中的数据拷贝到操作系统内核缓冲区中，然后数据被拷贝到与 socket 相关的内核缓冲区中去。接下来，DMA 引擎将数据从内核 socket 缓冲区中拷贝到协议引擎中去。
- 调用成功时返回写入的字节数，和上面相似，返回 EAGAIN 时停止本次发送，处理下一个客户请求。下一次发送时，利用记录的`offset`从上次停止的地方接着发送。
- 发送完成之后客户的请求就处理完毕了，然后从`epfd`中删除客户的 socket，不再监听客户的事件。
- 事件处理过程中如果遇到客户方关闭了 socket (EPIPE), 重置链接(ECONNRESET)等异常，直接关闭 socket 。
- 如果并发量太大导致打开的文件描述符过多，会调用`getrlimit()`和`setrlimit()`将文件描述符上限调整至 4096 ，如果还是不够的话只能麻烦助教手动`ulimit -Sn 10000`将上限改大一些。
- 为了避免用户可能的跳出目录的请求，将根目录调整至当前工作目录，并切换至根目录，所以用户无法跳出当前目录。


## 树莓派测试

通过 ssh 远程连接树莓派，进行测试，网络环境是手机热点，可能比较差。

``` zsh
➜  [/home/bw] siege -c 800 -r 2 http://192.168.137.33:8000/1M.file
** SIEGE 4.0.4
** Preparing 800 concurrent users for battle.
The server is now under siege...
Transactions:		        1600 hits
Availability:		      100.00 %
Elapsed time:		      293.01 secs
Data transferred:	     1600.00 MB
Response time:		      145.77 secs
Transaction rate:	        5.46 trans/sec
Throughput:		        5.46 MB/sec
Concurrency:		      795.99
Successful transactions:        1600
Failed transactions:	           0
Longest transaction:	      154.40
Shortest transaction:	      138.48
```

在刘紫檀同学的树莓派（有网线）上测试的数据：

```bash
[libreliu@thinkpad-ssd tmp]$ siege -c 200 -r 20 http://192.168.10.2:8000/test16k.txt >/dev/null
** SIEGE 4.0.4
** Preparing 200 concurrent users for battle.
The server is now under siege...

Transactions:		        4000 hits
Availability:		      100.00 %
Elapsed time:		        3.79 secs
Data transferred:	       62.50 MB
Response time:		        0.19 secs
Transaction rate:	     1055.41 trans/sec
Throughput:		       16.49 MB/sec
Concurrency:		      196.46
Successful transactions:        4000
Failed transactions:	           0
Longest transaction:	        0.24
Shortest transaction:	        0.06


libreliu@thinkpad-ssd tmp]$ siege -c 200 -r 1 http://192.168.10.2:8000/test4096k.txt >/dev/null
** SIEGE 4.0.4
** Preparing 200 concurrent users for battle.
The server is now under siege...

Transactions:		         200 hits
Availability:		      100.00 %
Elapsed time:		       32.08 secs
Data transferred:	      800.00 MB
Response time:		       31.73 secs
Transaction rate:	        6.23 trans/sec
Throughput:		       24.94 MB/sec
Concurrency:		      197.80
Successful transactions:         200
Failed transactions:	           0
Longest transaction:	       32.07
Shortest transaction:	       30.46
```

速度基本符合预期。

## 参考文献

[1. epoll usage](https://blog.csdn.net/yusiguyuan/article/details/15027821)

[2. sendfile](https://www.ibm.com/developerworks/cn/linux/l-cn-zerocopy2/index.html)