#include "webserver.h"


WebServer::WebServer() {
  users = new http_conn[MAX_FD];

  char *server_path = new char[200];
  assert(getcwd(server_path, 200) != NULL);
  // if() {
  //   perror("error getcwd");
  // } else {
  //   printf("server_path %s", server_path);
  // }

  const char *root = "/root";
  strcat(server_path, root);
  m_root = new char[200];
  strcpy(m_root, server_path);
  delete[] server_path;

  // 定时器
  users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
  close(m_epollfd);
  close(m_listenfd);
  close(m_pipefd[0]);
  close(m_pipefd[1]);
  delete[] users;
  delete[] users_timer;
  delete m_pool;
}

void WebServer::init(int port, string databaseName, string user, string passwd, int sql_num, int log_write, int close_log, int actormodel,
                     int thread_numbers, int opt_linger, int TRIGMode, int listen_trig_mode, int conn_trig_mode) {

  // http连接初始化
  m_port = port;
  m_log_write = log_write;
  m_close_log = close_log;
  m_actor_model = actormodel;

  // 数据库部分初始化
  m_databaseName = databaseName;
  m_user = user;
  m_passwd = passwd;
  m_sql_num = sql_num;

  m_thread_num = thread_numbers;

  // 触发模式初始化
  m_OPT_linger = opt_linger;
  m_TRIGMode = TRIGMode;
  m_ListenTrigMode = listen_trig_mode;
  m_ConnTrigMode = conn_trig_mode;
}

void WebServer::thread_pool() { m_pool = new threadpool<http_conn>(m_actor_model, m_connPool, m_thread_num); }

void WebServer::sql_pool() {
  m_connPool = connection_pool::GetInstance();
  m_connPool->init("localhost", m_user, m_passwd, m_databaseName, 3300, m_sql_num, m_close_log);

  users->initmysql_result(m_connPool);
}

// 初始化日志
void WebServer::log_write() {
  if (m_close_log == 0) {
    if (m_log_write == 1)
      Log::get_instance()->init("./log/ServerLog.log", m_close_log, 2000, 800000, 800);
    else
      Log::get_instance()->init("./log/ServerLog.log", m_close_log, 2000, 800000, 0);
  }
}

void WebServer::trig_mode() {
  switch (m_TRIGMode) {
  case 0: {  // LT(http连接模式) + LT(数据库)
    m_ListenTrigMode = 0;
    m_ConnTrigMode = 0;
  }

  case 1: {  // ET + LT
    m_ListenTrigMode = 1;
    m_ConnTrigMode = 0;
  }

  case 2: {  // LT + ET
    m_ListenTrigMode = 0;
    m_ConnTrigMode = 1;
  }
  case 3: {  // ET + ET
    m_ListenTrigMode = 1;
    m_ConnTrigMode = 1;
  }
  }
}

void WebServer::event_listen() {
  m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(m_listenfd >= 0);

  // 关闭连接
  if (m_OPT_linger == 0) {
    struct linger tmp = {0, 1};  // 结构体linger的第一个参数为0，表示关闭socket时立即返回，第二个参数为1，表示在关闭socket时强制发送未发送的数据
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  } else if (m_OPT_linger == 1) {
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }


  int                ret = 0;
  struct sockaddr_in addr;
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(m_port);

  int flag = 1;
  ret = setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  assert(ret == 0);
  ret = bind(m_listenfd, (struct sockaddr *)&addr, sizeof(addr));
  assert(ret >= 0);
  ret = listen(m_listenfd, 5);
  assert(ret >= 0);

  utils.init(TIMESLOT);

  // epoll创建内核事件表
  m_epollfd = epoll_create(5);
  assert(m_epollfd != -1);

  addfd(m_epollfd, m_listenfd, false, m_ListenTrigMode);
  http_conn::m_epollfd = m_epollfd;

  // 创建全双工管道, 注册m_pipefd[0]上的可读事件
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
  assert(ret != -1);
  setnonblocking(m_pipefd[1]);
  addfd(m_epollfd, m_pipefd[0], false, 0);

  // 注册信号
  utils.addsig(SIGPIPE, SIG_IGN, false);
  utils.addsig(SIGALRM, utils.sig_handler, false);
  utils.addsig(SIGTERM, utils.sig_handler, false);


  alarm(TIMESLOT);

  Utils::u_pipefd = m_pipefd;
  Utils::u_epollfd = m_epollfd;
}

void WebServer::event_loop() {
  bool timeout = false;
  bool stop_server = false;

  while (!stop_server) {
    int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if (number < 0 && errno != EINTR) {
      LOG_ERROR("epoll failure");
      break;
    }

    // 轮询文件描述符, epoll_wait()将所有就绪文件描述符放入events
    for (int i = 0; i < number; i++) {
      int sockfd = events[i].data.fd;

      if (sockfd == m_listenfd) {  // 处理到新客户连接, 使用dealclientdata注客户端连接和事件表, 计时器
        bool flag = dealclientdata();
        if (flag == false)
          continue;
      } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 服务端关闭连接, 移除对应的定时器
        util_timer *timer = users_timer[sockfd].timer;
        delete_timer(timer, sockfd);
      } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
        bool flag = dealwithsignal(timeout, stop_server);
        if (flag == false)
          LOG_ERROR("dealclientdate failure");
      } else if (events[i].events & EPOLLIN) {
        dealwithread(sockfd);
      } else if (events[i].events & EPOLLOUT) {
        dealwithwrite(sockfd);
      }
    }

    if (timeout) {
      utils.timer_handler();
      LOG_INFO("timer tick");
      timeout = false;
    }
  }
}

// 初始化客户端连接, 同时设置定时器, 注册内核事件表
void WebServer::timer(int connfd, sockaddr_in client_address) {
  // 初始化连接
  users[connfd].init(connfd, client_address, m_root, m_TRIGMode, m_close_log, m_user, m_passwd, m_databaseName);

  // 配置该连接的定时器
  users_timer[connfd].address = client_address;
  users_timer[connfd].sockfd = connfd;

  util_timer *timer = new util_timer;
  timer->user_data = &users_timer[connfd];
  timer->cb_func = cb_func;
  time_t cur = time(NULL);
  timer->expire = cur + 3 * TIMESLOT;
  users_timer[connfd].timer = timer;
  utils.m_timer_lst.add_timer(timer);
}

// 定时器延迟, 并调整定时器链表
void WebServer::adjust_timer(util_timer *timer) {
  time_t cur = time(NULL);
  timer->expire = cur * 3 + TIMESLOT;
  utils.m_timer_lst.adjust_timer(timer);

  LOG_INFO("adjust timer once");
}

void WebServer::delete_timer(util_timer *timer, int sockfd) {
  timer->cb_func(&users_timer[sockfd]);
  if (timer)
    utils.m_timer_lst.del_timer(timer);
  LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

// accept, 建立客户端连接
bool WebServer::dealclientdata() {

  struct sockaddr_in client_address;
  socklen_t          client_addr_len = sizeof(client_address);
  if (m_ListenTrigMode == 0) {  // LT
    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addr_len);
    if (connfd < 0) {
      LOG_ERROR("line : %d\t %s:errno is : %d", __LINE__, "accept error", errno);
      return false;
    }
    if (http_conn::m_user_count >= MAX_FD) {
      utils.show_error(connfd, "Internal server busy");
      LOG_ERROR("line : %d\t %s:errno is : %d", __LINE__, "accept error", errno);
      return false;
    }
    timer(connfd, client_address);
  } else {  // ET 模式
    while (1) {
      int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addr_len);
      if (connfd < 0) {
        LOG_ERROR("line : %d\t %s:errno is : %d", __LINE__, "accept error", errno);
        return false;
      }
      if (http_conn::m_user_count >= MAX_FD) {
        utils.show_error(connfd, "Internal server busy");
        LOG_ERROR("line : %d\t %s:errno is : %d", __LINE__, "accept error", errno);
        return false;
      }
      timer(connfd, client_address);
    }
    return false;
  }
  return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server) {

  int  ret = 0;
  char signals[1024];

  // 从管道端读出信号值
  ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
  if (ret == -1)
    return false;
  else if (ret == 0)
    return false;
  else {
    for (int i = 0; i < ret; i++) {
      switch (signals[i]) {
      case SIGALRM:
        timeout = true;
        break;

      case SIGTERM:
        stop_server = true;
        break;

      default:
        break;
      }
    }
  }
  return true;
}

// 处理读事件
void WebServer::dealwithread(int sockfd) {
  util_timer *timer = users_timer[sockfd].timer;

  // Reactor
  if (m_actor_model == 1) {
    if (timer)
      adjust_timer(timer);

    // 添加到请求队列, 设置http的状态为读
    m_pool->append(users + sockfd, 0);
    while (true) {
      if (users[sockfd].improv == 1) {
        delete_timer(timer, sockfd);
        users[sockfd].timer_flag = 0;
      }
      users[sockfd].improv = 0;
      break;
    }
  }
  // Proactor, 使用同步IO模拟Proator
  else {
    if (users[sockfd].read_once()) {
      LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

      m_pool->append_p(users + sockfd);

      if (timer)
        adjust_timer(timer);
    } else {
      delete_timer(timer, sockfd);
    }
  }
}

// 处理写事件
void WebServer::dealwithwrite(int sockfd) {
  util_timer *timer = users_timer[sockfd].timer;
  if (m_actor_model == 1) {  // Reactor
    if (timer)
      adjust_timer(timer);
    m_pool->append(users + sockfd, 1);
    while (true) {
      if (users[sockfd].improv == 1) {
        if (users[sockfd].timer_flag == 1) {
          delete_timer(timer, sockfd);
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  } else {  // Proactor
    if (users[sockfd].write()) {
      LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

      if (timer)
        adjust_timer(timer);

    } else {
      delete_timer(timer, sockfd);
    }
  }
}
