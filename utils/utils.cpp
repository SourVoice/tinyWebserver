#include "utils.h"

void cb_func(client_data *user) {
  epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user->sockfd, 0);
  close(user->sockfd);
  http_conn::m_user_count--;
}

// 设置非阻塞
int setnonblocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_GETFL, new_option);
  return old_option;
}

// 注册到内核事件表, one_shot添加EPOLLONESHOT标志, TRIGMode = 0默认使用LT模式, =1切换模式
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  if (TRIGMode == 0) {  // 默认LT
    event.events = EPOLLIN | EPOLLRDHUP;
  } else {
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  }

  if (one_shot)
    event.events |= EPOLLONESHOT;

  assert(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == 0);
  setnonblocking(fd);
}

// 从内核事件表中删除
void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  if (TRIGMode == 0) {
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  } else {
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  }

  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


/**
 * @brief Utils类定义
 */
int *Utils::u_pipefd = 0;
int  Utils::u_epollfd = 0;

void Utils::init(int timeslot) { m_timeslot = timeslot; }

// 信号处理函数
void Utils::sig_handler(int sig) {
  // 保证函数的可重入性, 保留原errno
  int save_errno = errno;
  int msg = sig;
  send(u_pipefd[1], (char *)&msg, 1, 0);
  errno = save_errno;
}

// 配置信号处理函数(注册)
void Utils::addsig(int sig, void(handler)(int), bool restart) {

  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = sig_handler;
  if (restart)
    sa.sa_flags |= SA_RESTART;
  sigfillset(&sa.sa_mask);  // 将所有信号添加到信号集
  // 执行sigaction函数, 注册信号处理函数, 使用assert防御
  assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler() {
  m_timer_lst.tick();
  // 重新定时
  alarm(m_timeslot);
}

void Utils::show_error(int connfd, const char *info) {
  send(connfd, info, strlen(info), 0);
  close(connfd);
}