## lock类
封装条件变量, 互斥锁, 信号量

## 线程池
线程池采用半同步/半反应堆, 反应堆具体为Proactor事件处理模式, 主线程为异步线程, 负责监听文件描述符, 接收socket新连接, 若当前监听socket发生读写事件, 则将任务插入到请求队列. 工作线程从请求队列中取出任务, 完成数据的读写处理.


## http连接处理
### ET、LT、EPOLLONESHOT
> LT水平触发模式

epoll_wait检测到文件描述符有事件发生，则将其通知给应用程序，应用程序可以不立即处理该事件。

当下一次调用epoll_wait时，epoll_wait还会再次向应用程序报告此事件，直至被处理

> ET边缘触发模式

epoll_wait检测到文件描述符有事件发生，则将其通知给应用程序，应用程序必须立即处理该事件

必须要一次性将数据读取完，使用非阻塞I/O，读取到出现eagain

> EPOLLONESHOT

一个线程读取某个socket上的数据后开始处理数据，在处理过程中该socket上又有新数据可读，此时另一个线程被唤醒读取，此时出现两个线程处理同一个socket

我们期望的是一个socket连接在任一时刻都只被一个线程处理，通过epoll_ctl对该文件描述符注册epolloneshot事件，一个线程处理socket时，其他线程将无法处理，当该线程处理完后，需要通过epoll_ctl重置epolloneshot事件

### http报文处理流程
1. 浏览器端发出http连接请求, 主线程创建http对象接收请求并将所有数据读入对应的buffer, 将该对象插入任务队列, 工作线程从任务队列中取出一个任务进行处理.
2. 工作线程取出任务后, 调用process_read函数, 通过主从状态机对请求报文进行解析.
3. 解析完成后, 跳转do_request函数生成响应报文, 通过process_write写入buffer, 返回给浏览器端.

#### process_read分析主从状态机
<!-- ![](/picture/http%E7%8A%B6%E6%80%81%E6%9C%BA.jpg) -->
从状态机负责读取报文的一行，主状态机负责对该行数据进行解析，主状态机内部调用从状态机，从状态机驱动主状态机。

1. `parser_line()` 函数使用:

在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态`(line_status=parse_line())==LINE_OK`语句即可。

但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。

使用POST请求, 请求内容位于请求体
回车和换行符(CRLF)作为间隔

2. `parser_request_line()`

    可以在[rfc文档中看到请求行的格式](https://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html)


3. `do_request()`

 将返回结果所在文件映射到内存中, 将网站根目录和url文件拼接, 通过stat判断文件属性. 
m_url为请求报文中解析出的请求资源，以/开头，也就是/xxx，项目中解析后的m_url有8种情况。

- /
    - GET请求，跳转到judge.html，即欢迎访问页面
- /0
    - 注册请求, POST请求, 跳转到register.html
- /1 
    - 登录请求, POST 请求, 跳转到log.html
- /2CGISQL.cgi
    - POST请求, 进行登录校验
    - 成功跳转到welcome.html
    - 失败跳转到logError.html
- /3CGISQL.cgi
    - POST请求, 进行注册校验
    - 注册成功跳转到log.html
    - 注册失败跳转到registerError.html
- /5
    - POST请求，跳转到picture.html，即图片请求页面
- /6
    - POST请求，跳转到video.html，即视频请求页面
- /7
    - POST请求，跳转到fans.html，即关注页面

4. `http_conn::write`

`process_write()`完成响应报文, 注册epollout事件, 服务器主线程检测写事件, 调用`http::write()`将响应报文传回


## 日志
在C++11以前，实现懒汉模式（Lazy Singleton Pattern）的经典做法是在静态成员函数中使用一个静态成员变量来存储单例对象，同时在访问该变量之前需要使用锁保证线程安全性。这种做法的缺点是需要在每次访问单例对象时都进行加锁和解锁操作，对性能造成影响。

C++11引入了线程安全的局部静态变量（Thread-safe Local Static）的概念，使得实现懒汉模式更加方便。使用局部静态变量实现的懒汉模式不需要显式加锁，因为C++11规定，在多线程环境下，局部静态变量的初始化是线程安全的。

因此，使用局部变量懒汉模式不需要加锁，即在单例类的静态成员函数中，通过将单例对象定义为静态局部变量的方式来实现单例模式，这样可以保证单例对象的唯一性，同时也可以避免锁的开销。但是，需要注意局部变量懒汉模式可能会存在一些线程安全性问题，例如静态变量的初始化顺序等，需要谨慎使用。

`日志`，由服务器自动创建，并记录运行状态，错误信息，访问数据的文件。

`同步日志`，日志写入函数与工作线程串行执行，由于涉及到I/O操作，当单条日志比较大的时候，同步模式会阻塞整个处理流程，服务器所能处理的并发能力将有所下降，尤其是在峰值的时候，写日志可能成为系统的瓶颈。

`生产者-消费者模型`，并发编程中的经典模型。以多线程为例，为了实现线程间数据同步，生产者线程与消费者线程共享一个缓冲区，其中生产者线程往缓冲区中push消息，消费者线程从缓冲区中pop消息。

`阻塞队列`，将生产者-消费者模型进行封装，使用循环数组实现队列，作为两者共享的缓冲区。

`异步日志`，将所写的日志内容先存入阻塞队列，写线程从阻塞队列中取出内容，写入日志。

`单例模式`，最简单也是被问到最多的设计模式之一，保证一个类只创建一个实例，同时提供全局访问的方法。

日志写入前会判断当前day是否为创建日志的时间，行数是否超过最大行限制

> 若为创建日志时间，写入日志，否则按当前时间创建新log，更新创建时间和行数

> 若行数超过最大行限制，在当前日志的末尾加count/max_lines为后缀创建新log

## 阻塞队列
C++阻塞队列是一种线程安全的队列，可以在多线程环境下使用。阻塞队列的特点是当队列为空或已满时，插入和删除操作会被阻塞，直到队列不为空或不满才会继续执行。

阻塞队列的主要操作包括入队（push）、出队（pop）、获取队首元素（front）和获取队列大小（size）等。在多线程环境下，为了保证线程安全，需要对这些操作进行加锁处理。

阻塞队列的实现可以使用互斥锁（mutex）和条件变量（condition variable）来实现。当队列为空时，出队操作会等待条件变量，直到队列中有新元素加入；当队列已满时，入队操作会等待条件变量，直到队列中有元素被取出。

示例
```c++
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class BlockingQueue
{
public:
    BlockingQueue(int maxSize) : m_maxSize(maxSize) {}

    void push(const T& item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.size() == m_maxSize)
        {
            m_notFull.wait(lock);
        }
        m_queue.push(item);
        m_notEmpty.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())
        {
            m_notEmpty.wait(lock);
        }
        T item = m_queue.front();
        m_queue.pop();
        m_notFull.notify_one();
        return item;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    int m_maxSize;
};
```
往队列添加元素，需要将所有使用队列的线程先唤醒
当有元素push进队列，相当于生产者生产了一个元素
若当前没有线程等待条件变量,则唤醒无意义

### 定时器处理非活动连接
- 定时事件 指固定一段时间之后触法某段代码, 由该段代码处理一个事件, 如从内核事件表删除事件, 并关闭文件描述符, 释放连接资源


本项目中，服务器主循环为每一个连接创建一个定时器，并对每个连接进行定时。另外，利用升序时间链表容器将所有定时器串联起来，若主循环接收到定时通知，则在链表中依次执行定时任务。

定时器容器采用升序链表

定时器容器基本逻辑, SIGALARM触法后使用tick对容器中的所有定时器进行处理, 到时的定时器调用回调函数以执行任务, 将其删除, 知道碰到尚未到时的定时器时退出.

使用定时器容器的主要目的是管理非活动连接, 服务器通常要处理非活动连接: 给客户端发了一个重连请求, 或者关闭连接.
Linux内核中提供了对连接是否处于活动那个状态的定期检查机制, 可以通过socket的KEEPALIVE来激活. 使用内核管理会使得我们的应用程序对连接的管理变得复杂. 我们采用在应用层实现类似的KEEPALIVE机制, 以管理所有处于非活动连接状态的连接.

构建Utils类, 利用alram函数周期性触法SIGALARM信号, 该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.


### 数据库连接池

池可以看作资源的容器, 通过单例模式和链表创建数据库连接池, 实现对数据库连接资源的复用.

数据库模块分为两部分, 其一是数据库连接池的定义
其二利用连接池完成登录和注册的校验功能

工作线程从数据库连接池取得一个链接, 访问数据库中的数据, 访问完毕后讲连接交换连接池

*单例模式创建*
*连接池代码实现*
*RAII机制释放数据库连接*

#### 连接池实现:
实现的功能: 初始化, 获取连接, 释放连接, 销毁连接池

- 初始化
    销毁连接池不被外界直接调用, 通过RAII机制来完成自动释放
    使用信号量完成多线程争夺连接的同步机制, 这里讲信号量初始化为数据库的连接总量

- 获得, 释放连接
    当线程数量大于数据库连接数量时, 使用信号量进行同步, 每次取出连接, 信号量原子减一, 释放连接原子加一, 若连接池内没有连接, 则阻塞等待.

- 销毁连接池
通过迭代器遍历连接池链表，关闭对应数据库连接，清空链表并重置空闲连接和现有连接数量。


### 信号

信号为异步事件, 信号处理函数和程序徐的主循环是两条不同的执行线路. 很显然, 信号处理函数需要尽可能的快地执行完毕, 以确保信号不被屏蔽太久(为避免竞态条件, 信号在处理期间, 系统不会再触法它). 一种典型的解决方案是: 把信号的主要处理逻辑放到程序的主循环中, 当信号处理函数被触法时, 它只是简单的通过主循环程序接收到信号, 并把信号值传递给主循环:
    - 信号处理函数往管道的写端写入信号值
    - 主循环从管道的读端读出该信号值.
同样的, 主循环采用I/O复用系统调用来监听管道的读端文件描述符上的可读事件.
使得信号事件也能和其他I/O事件一同被处理, 即: *统一事件源*

信号处理函数发送信号, (其他逻辑)并处理定时器
```c++
/* Timer/timer.c Utils::sig_handler() */
// 信号处理函数
void Utils::sig_handler(int sig) {
  // 保证函数的可重入性, 保留原errno
  int save_errno = errno;
  int msg = sig;
  send(u_pipefd[1], (char *)&msg, 1, 0);    /* 信号写入管道 */
  errno = save_errno;
}
```
主循环处理信号事件
```c++
void Webserver::event_listen(){
  /* 初始化服务端listenfd */ 
  ...
  // 创建全双工管道, 注册m_pipefd[0]上的可读事件
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
  assert(ret != -1);
  setnonblocking(m_pipefd[1]);
  addfd(m_epollfd, m_pipefd[0], false, 0);
  ...
  // 注册信号
  /* 设置Utils信号处理类中的管道 */
}

void Webserver::event_loop() {
// 轮询文件描述符, epoll_wait()将所有就绪文件描述符放入events
  for (int i = 0; i < number; i++) {
    int sockfd = events[i].data.fd;
    /* 其他读写事件的判断和逻辑 */
    ...
    if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
      bool flag = dealwithsignal(timeout, stop_server);
      if (flag == false)
        LOG_ERROR("dealclientdate failure");
      }
   }
   ...
   /* 超时处理 */
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server) {
  /* 变量声明 */
  ...
  // 从管道端读出信号值
  ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
  for (int i = 0; i < ret; i++) {
    switch (signals[i]) {
      /* 改变当前状态, timeout 和 stop_server */
    }
  }
  return true;
}
```
