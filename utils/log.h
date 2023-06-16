#ifndef _LOG_H
#define _LOG_H

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include <nana/gui.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/form.hpp>

#include "block_queue.h"

using namespace nana;
class Log {
private:
  Log();

  virtual ~Log();

public:
  // 使用局部静态变量实现单例模式(c++11中不用再加锁)
  static Log *get_instance();

  static void *flush_log_thread(void *args);

  // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
  bool init(const char *file_name, textbox *log_box, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

  void write_log(int level, const char *format, ...);

  void flush(void);

private:
  void *async_write_log();

private:
  char                      dir_name[128];
  char                      log_name[128];
  int                       m_split_lines;   // 日志最大行数
  int                       m_log_buf_size;  // 日志缓冲区大小
  long long                 m_count;         // 日志行数记录(行号)
  int                       m_today;         // 按天分类
  FILE                     *m_fp;
  char                     *m_buf;           // 日志内容
  block_queue<std::string> *m_log_queue;
  bool                      m_is_async;      // 是否同步标志位
  locker                    m_mutex;
  int                       m_close_log;     // 关闭日志标志位
  textbox                  *m_log_box;
};

#define LOG_DEBUG(format, ...)                                                                                                                       \
  if (m_close_log == 0) {                                                                                                                            \
    Log::get_instance()->write_log(0, format, ##__VA_ARGS__);                                                                                        \
    Log::get_instance()->flush();                                                                                                                    \
  }
#define LOG_INFO(format, ...)                                                                                                                        \
  if (m_close_log == 0) {                                                                                                                            \
    Log::get_instance()->write_log(1, format, ##__VA_ARGS__);                                                                                        \
    Log::get_instance()->flush();                                                                                                                    \
  }
#define LOG_WARN(format, ...)                                                                                                                        \
  if (m_close_log == 0) {                                                                                                                            \
    Log::get_instance()->write_log(2, format, ##__VA_ARGS__);                                                                                        \
    Log::get_instance()->flush();                                                                                                                    \
  }
#define LOG_ERROR(format, ...)                                                                                                                       \
  if (m_close_log == 0) {                                                                                                                            \
    Log::get_instance()->write_log(3, format, ##__VA_ARGS__);                                                                                        \
    Log::get_instance()->flush();                                                                                                                    \
  }

#endif