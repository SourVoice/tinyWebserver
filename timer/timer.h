#ifndef _TIMER_H
#define _TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include "../utils/log.h"
#include "../http/http_conn.h"

class util_timer;

struct client_data {
  sockaddr_in address;
  int         sockfd;
  util_timer *timer;
};

class util_timer {
public:
  util_timer() : prev(nullptr), next(nullptr){};
  ~util_timer(){};

  // 回调函数
  void (*cb_func)(client_data *);

public:
  time_t expire;           // 任务超时时间, 使用绝对是时间

  client_data *user_data;  // 回调函数处理客户数据, 由定时器执行者传递给回调函数
  util_timer  *prev;
  util_timer  *next;
};

// 定时器链表, 双向, 升序链表
class sort_timer_lst {
public:
  sort_timer_lst();
  ~sort_timer_lst();

  void add_timer(util_timer *timer);
  void adjust_timer(util_timer *timer);
  void del_timer(util_timer *timer);
  void tick();

private:
  void add_timer(util_timer *timer, util_timer *lst_head);  // 重载函数

  util_timer *head;
  util_timer *tail;
};


void cb_func(client_data *user_data);
#endif
