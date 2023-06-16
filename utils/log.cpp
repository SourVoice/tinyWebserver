#include "log.h"

Log::Log() {
  m_count = 0;
  m_is_async = false;
}

Log::~Log() {
  if (m_fp != nullptr) {
    fclose(m_fp);
  }
}

Log *Log::get_instance() {
  static Log log;
  return &log;
}

void *Log::flush_log_thread(void *args) { return Log::get_instance()->async_write_log(); }

// 异步需要设置阻塞队列长度
bool Log::init(const char *file_name, textbox *log_box, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
  if (max_queue_size >= 1) {
    m_is_async = true;  // 设置写入方式flag
    m_log_queue = new block_queue<std::string>(max_queue_size);
    pthread_t tid;
    pthread_create(&tid, NULL, flush_log_thread, NULL);
  }
  m_close_log = close_log;
  m_log_buf_size = log_buf_size;
  m_buf = new char[m_log_buf_size];
  memset(m_buf, '\0', m_log_buf_size);
  m_split_lines = split_lines;
  m_log_box = log_box;

  time_t     t = time(NULL);
  struct tm *sys_tm = localtime(&t);
  struct tm  my_tm = *sys_tm;

  // 找到最后一个/的位置
  const char *p = strrchr(file_name, '/');
  char        log_full_name[256] = {0};

  // 若输入的文件名没有/，则直接将时间+文件名作为日志名
  if (p == nullptr) {
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
  } else {
    memcpy(log_name, p + 1, strlen(p + 1));           // file_name的去掉/后的名字加入log_name
    strncpy(dir_name, file_name, p - file_name + 1);  // 日志所在目录的位置
    int ret = snprintf(log_full_name, sizeof(log_full_name), "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                       log_name);
    if (ret < 0) {
      assert(ret >= 0);
    }
  }
  m_today = my_tm.tm_mday;
  m_fp = fopen(log_full_name, "a");
  if (m_fp == nullptr) {
    return false;
  }
  return true;
}

void Log::write_log(int level, const char *format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, NULL);
  time_t     t = now.tv_sec;
  struct tm *sys_tm = localtime(&t);
  struct tm  my_tm = *sys_tm;
  char       s[16] = {0};  // 日志类型

  switch (level) {
  case 0:
    strcpy(s, "[debug]:");
    break;

  case 1:
    strcpy(s, "[info]:");
    break;

  case 2:
    strcpy(s, "[warn]:");
    break;

  case 3:
    strcpy(s, "[erro]:");
    break;

  default:
    strcpy(s, "[info]:");
    break;
  }

  m_mutex.lock();
  m_count++;

  //  文件创建
  // 日志不是今天或写入的日志行数是最大行的倍数
  if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {
    char new_log[278] = {0};
    fflush(m_fp);  // m_fp缓冲区内容输入到文件
    fclose(m_fp);
    char tail[16] = {0};

    int ret = snprintf(tail, sizeof(tail), "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
    if (ret < 0) {
      assert(ret >= 0);
    }
    // 时间不为今天, 则创建今天的日志, 更新m_today和m_count
    if (m_today != my_tm.tm_mday) {
      snprintf(new_log, sizeof(new_log), "%s%s%s", dir_name, tail, log_name);
      m_today = my_tm.tm_mday;
      m_count = 0;
    } else {
      // 超过最大行, 在之前的日志名基础上加后缀m_split_lines
      int ret = snprintf(new_log, sizeof(new_log), "%s%s%s.%lld", dir_name, tail, log_name, (m_count % m_split_lines));
      if (ret < 0) {
        assert(ret >= 0);
      }
    }

    m_fp = fopen(new_log, "a");
  }
  m_mutex.unlock();

  // 写入内容
  va_list valst;
  va_start(valst, format);

  std::string log_str;
  m_mutex.lock();

  int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, my_tm.tm_hour,
                   my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

  int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
  m_buf[n + m] = '\n';
  m_buf[n + m + 1] = '\0';

  log_str = m_buf;
  m_mutex.unlock();

  // m_is_async表示异步, 异步时日志信息加入阻塞队列, 同步时加锁向文件中写入
  if (m_is_async && !m_log_queue->empty()) {
    m_log_queue->push(log_str);
  } else {  // 同步写入加锁
    m_mutex.lock();
    fputs(log_str.c_str(), m_fp);
    // m_log_box->append(log_str, true);
    m_mutex.unlock();
  }

  m_mutex.unlock();
  va_end(valst);
}

void Log::flush(void) {
  m_mutex.lock();
  fflush(m_fp);  // 强制刷新写入流缓冲区
  m_mutex.unlock();
}

void *Log::async_write_log() {
  std::string single_log;
  // 从阻塞队列中取出日志
  while (m_log_queue->pop(single_log)) {
    m_mutex.lock();
    fputs(single_log.c_str(), m_fp);
    // m_log_box->append(single_log, true);
    m_mutex.unlock();
  }
  char *ptr = new char[1];
  return ptr;
}
