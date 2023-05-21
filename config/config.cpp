#include "config.h"


Config::Config() {
  PORT = 9090;
  LOGWrite = 0;        // 默认同步写入日志
  TRIGMode = 0;        // 默认listenfd LT + connfd LT
  LISTENTrigmode = 0;  // listenfd触法模式, 默认LT
  CONNTrigmode = 0;    // connfd 触法模式, 默认LT
  OPT_LINGER = 0;      // 优雅关闭连接, 默认不使用
  sql_num = 8;         // 数据库连接池数量, 默认8
  thread_num = 8;      // 线程池内的线程数量, 默认8
  close_log = 0;       // 默认关闭
  actor_model = 0;     // 默认proactor
}

Config::~Config() {}

void Config::parse_arg(int argc, char *argv[]) {
  int         opt;
  const char *str = "p:l:m:o:s:t:c:a:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
    case 'p':
      PORT = atoi(optarg);
      break;
    case 'l':
      LOGWrite = atoi(optarg);
      break;
    case 'm':
      TRIGMode = atoi(optarg);
      break;
    case 'o':
      OPT_LINGER = atoi(optarg);
      break;
    case 's':
      sql_num = atoi(optarg);
      break;
    case 't':
      thread_num = atoi(optarg);
      break;
    case 'c':
      close_log = atoi(optarg);
      break;
    case 'a':
      actor_model = atoi(optarg);
      break;
    default:
      fprintf(stderr,
              "Usage: %s [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }
}
