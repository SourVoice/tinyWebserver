#ifndef _CONFIG_H
#define _CONFIG_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

class Config {
public:
  Config();
  ~Config();

  void parse_arg(int argc, char *argv[]);

  int PORT;

  int LOGWrite;        // 日志写入方式

  int TRIGMode;        // 触法组合模式

  int LISTENTrigmode;  // listenfd触发模式

  int CONNTrigmode;    // connfd触法

  int OPT_LINGER;      // 优雅的关闭连接

  int sql_num;         // 数据库连接池数量

  int thread_num;      // 线程池内线程数量

  int close_log;       // 是否关闭日志

  int actor_model;     // 并发模型选择
};


#endif