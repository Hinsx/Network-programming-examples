# 什么是IO多路复用
多路指的是多个文件句柄，复用指的是使用同一个线程。其含义是**多个句柄使用同一线程维护**。在服务端中，常常需要和多个客户端保持通信，当某一个句柄收到客户端数据，或者某一句柄可以发送数据时，服务端如何发现？如果不使用IO复用，可以有两种方式。
## 不使用IO多路复用
### 同步阻塞（BIO）
服务端的一个线程阻塞在`recv/send`调用上，那么一个线程仅仅只能维护一个连接，效率很低。**阻塞指的是线程等待在一个函数上，直到该函数返回。同步指的是由线程主动去检查句柄是否可用。**
### 同步非阻塞（NIO）
服务端的一个线程轮询所有句柄，对每个句柄执行非阻塞的`recv/send`。虽然一个线程能够维护多个连接了，但是很可能大部分的轮询都是无用的，因为可能所有句柄的状态都没有改变，再加上每一次`recv/send`都是一次系统调用，白白浪费了CPU。
## 使用IO多路复用（更好的NIO）
当线程调用select/poll/epoll时，线程阻塞，句柄由内核代为监听。当某一句柄发生事件时，内核唤醒线程。通过IO多路复用，线程不必频繁执行系统调用，也不必占用大量CPU。每次线程被唤醒时，必定有句柄的状态发生改变。

在POSIX标准的定义下，IO多路复用是一种**同步IO**，所谓同步IO即系统不会主动将数据拷贝到用户态，用户态需要主动去移动数据（用户态到内核态或者内核态到用户态）并执行相应操作。而**异步IO**则是由内核接管读写操作，比如数据从内核拷贝到用户态，然后执行逻辑（如`aio.h`中的API），最后再通知程序。可以这么说：**同步IO向程序通知的是IO就绪事件，异步IO通知的是IO完成事件**-----《Linux高性能服务器编程》

# 区分同步/异步线程和同步/异步IO
同步线程指线程执行（函数的顺序）已经由代码确定，异步线程则依靠事件通知，其顺序不可预料，比如信号。
`同步线程`
![!](pic/%E5%90%8C%E6%AD%A5%E7%BA%BF%E7%A8%8B.png)
`异步线程`
![!](pic/%E5%BC%82%E6%AD%A5%E7%BA%BF%E7%A8%8B.png)
同步/异步IO则是描述系统通知程序IO事件的方式。

其实同步/异步IO就已经说明了线程的执行流程，同步IO如select/poll/epoll都是由线程**主动**调用，而异步IO如`aio_read`则让线程的执行顺序变得不确定。

# IO多路复用API及其原理
## select
使用三个`fd_set`分别表示读/写/异常事件，将一个文件描述符映射为一bit。其缺点有：
1. 因为头文件宏定义`_FD_SETSIZE`，只能支持监听1024个文件描述符，超过这个限制就会导致数组越界。
2. 每次调用都需要将监听数组拷贝到内核，调用返回时也需要拷贝回用户态。
3. 使用轮询检查句柄是否有事件，当连接多且活跃量少时效率不高。
4. 内核会修改`fd_set`以表示有事件产生，在下次调用select时需要重新设置`fd_set`。
5. 程序需要对返回的`fd_set`进行遍历，以检查哪些句柄有事件。
6. 仅能处理一种异常事件：socket带外数据。
## poll
相比`select`改进的地方：
1. 使用`pollfd`存储文件描述符，可以支持最大65535个文件描述符的监听。
2. poll返回不会改变程序设置的感兴趣事件，所以下一次poll不需要重新设置。
## epoll
相比`select/poll`的改进
1. 使用就绪链表组织io事件就绪的socket（epitem），**减少用户层不必要的遍历**
2. 使用红黑树管理socket（epitem）**，避免用户空间和内核之间频繁大量的拷贝**
3. 使用回调的方式，让就绪的socket**主动**存储到就绪链表，**不需要有内核轮询**寻找就绪socket



