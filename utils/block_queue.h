#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>

#include "../locker/locker.h"

template <typename T>
class block_queue {
public:
  block_queue(int max_size = 1000);

  ~block_queue();

  void clear();

  bool full();

  bool empty();

  bool front(T &value);

  bool back(T &value);

  int size();

  int max_size();

  bool push(const T &item);

  bool pop(T &item);

  bool pop(T &item, int ms_timeout);

private:
  locker m_mutex;
  cond   m_cond;

  T  *m_array;
  int m_size;
  int m_max_size;
  int m_front;
  int m_back;
};

#endif

template <typename T>
inline block_queue<T>::block_queue(int max_size) {
  if (max_size <= 0) {
    throw std::exception();
    exit(-1);
  }

  m_max_size = max_size;
  m_array = new T[max_size];
  m_size = 0;
  m_front = -1;
  m_back = -1;
}

template <typename T>
inline block_queue<T>::~block_queue() {
  m_mutex.lock();
  if (m_array != nullptr)
    delete[] m_array;

  m_mutex.unlock();
}

template <typename T>
inline void block_queue<T>::clear() {
  m_mutex.lock();
  m_size = 0;
  m_front = -1;
  m_back = -1;
  m_mutex.unlock();
}

template <typename T>
inline bool block_queue<T>::full() {
  m_mutex.lock();
  if (m_size >= m_max_size) {
    m_mutex.unlock();
    return true;
  }
  m_mutex.unlock();
  return false;
}

template <typename T>
inline bool block_queue<T>::empty() {
  m_mutex.lock();
  if (m_size == 0) {
    m_mutex.unlock();
    return true;
  }
  m_mutex.unlock();
  return false;
}

template <typename T>
inline bool block_queue<T>::front(T &value) {
  m_mutex.lock();
  if (m_size == 0) {
    m_mutex.unlock();
    return false;
  }
  value = m_array[m_front];
  m_mutex.unlock();
  return true;
}

template <typename T>
inline bool block_queue<T>::back(T &value) {
  m_mutex.lock();
  if (m_size == 0) {
    m_mutex.unlock();
    return false;
  }
  value = m_array[m_back];
  m_mutex.unlock();
  return true;
}

template <typename T>
inline int block_queue<T>::size() {
  int ret = 0;
  m_mutex.lock();
  ret = m_size;
  m_mutex.unlock();
  return ret;
}

template <typename T>
inline int block_queue<T>::max_size() {
  int ret = 0;
  m_mutex.lock();
  ret = m_size;
  m_mutex.unlock();
  return ret;
}

template <typename T>
inline bool block_queue<T>::push(const T &item) {
  m_mutex.lock();
  if (m_size >= m_max_size) {
    m_cond.broadcast();
    m_mutex.unlock();
    return false;
  }
  // 循环队列计算方式, 需要更新位置
  m_back = (1 + m_back) % m_max_size;
  m_array[m_back] = item;
  m_size++;
  m_cond.broadcast();
  m_mutex.unlock();
  return true;
}

template <typename T>
inline bool block_queue<T>::pop(T &item) {
  m_mutex.lock();
  // 如果当前队列没有元素, 则等待条件变量
  while (m_size <= 0) {
    if (!m_cond.wait(m_mutex.get())) {
      m_mutex.unlock();
      return false;
    }
  }
  m_front = (1 + m_front) % m_max_size;
  m_array[m_front] = item;
  m_size--;
  m_mutex.unlock();
  return true;
}

template <typename T>
inline bool block_queue<T>::pop(T &item, int ms_timeout) {
  struct timespec t = {0, 0};
  struct timeval  now = {0, 0};
  gettimeofday(&now, NULL);

  m_mutex.lock();
  if (m_size <= 0) {
    t.tv_sec = now.tv_sec + ms_timeout / 1000;
    t.tv_nsec = (ms_timeout % 1000) * 1000;
    if (!m_cond.timewait(m_mutex.get(), t)) {
      m_mutex.unlock();
      return false;
    }
  }

  if (m_size <= 0) {
    m_mutex.unlock();
    return false;
  }

  m_front = (1 + m_front) % m_max_size;
  item = m_array[m_front];
  m_size--;
  m_mutex.unlock();
  return true;
}
