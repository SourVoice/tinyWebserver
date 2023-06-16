#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <nana/gui.hpp>
#include <nana/gui/widgets/textbox.hpp>

#include "CGImysql/sql_connection_pool.h"
#include "http/http_conn.h"
#include "locker/locker.h"
#include "threadpool/threadpool.h"
#include "timer/timer.h"
#include "utils/log.h"
#include "utils/utils.h"

using namespace std;

#define MAX_FD 5                // 最大文件描述符
#define MAX_EVENT_NUMBER 10000  // 最大事件数
#define TIMESLOT 5              // 最小超时单位

extern void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
extern void removefd(int epollfd, int fd);

class WebServer {
public:
  WebServer();
  ~WebServer();

  void init(int port, string databaseName, string user, string passwd, int sql_num, int log_write, int close_log, int actormodel, int thread_numbers,
            int opt_linger, int TRIGMode, int listen_trig_mode, int conn_trig_mode);

  void thread_pool();

  void sql_pool();

  void log_write(nana::textbox *log_box);
  nana::textbox *m_log_box;

  void trig_mode();

  void event_listen();

  void event_loop();

  void timer(int connfd, sockaddr_in client_address);

  void adjust_timer(util_timer *timer);

  void delete_timer(util_timer *timer, int sockfd);

  bool dealclientdata();

  bool dealwithsignal(bool &timeout, bool &stop_server);

  void dealwithread(int sockfd);

  void dealwithwrite(int sockfd);

public:
  /* 模式和路径 */
  int   m_port;
  char *m_root;
  int   m_log_write;
  int   m_close_log;
  int   m_actor_model;

  int        m_pipefd[2];  // 信号事件读写管道
  int        m_epollfd;
  http_conn *users;

  /* 数据库 */
  connection_pool *m_connPool;
  string           m_databaseName;
  string           m_user;
  string           m_passwd;
  int              m_sql_num;

  /* 线程池 */
  threadpool<http_conn> *m_pool;
  int                    m_thread_num;

  epoll_event events[MAX_EVENT_NUMBER];

  int m_listenfd;
  int m_OPT_linger;      // 连接关闭方式
  int m_TRIGMode;        // 触法模式的组合
  int m_ListenTrigMode;  // listenfd触发模式
  int m_ConnTrigMode;    // 数据库连接触发模式

  // 定时器
  client_data *users_timer;  // 针对每一个http_conn连接的定时器
  Utils        utils;
};

#endif