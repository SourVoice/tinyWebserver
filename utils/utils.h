#ifndef _UTILS_H
#define _UTILS_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../timer/timer.h"



class Utils {
public:
  Utils() {}
  ~Utils() {}

  void init(int timeslot);

  static void sig_handler(int sig);

  // 注册信号函数
  void addsig(int sig, void(handler)(int), bool restart);

  // 定时器处理函数(信号函数)
  void timer_handler();

  void show_error(int connfd, const char *info);

public:
  static int    *u_pipefd;
  sort_timer_lst m_timer_lst;
  static int     u_epollfd;
  int            m_timeslot;
};

#endif
