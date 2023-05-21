#ifndef _LOCKER_H
#define _LOCKER_H

#include <pthread.h>
#include <memory>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>

struct Noncopyable {
  Noncopyable() = default;

  ~Noncopyable() = default;

  Noncopyable(const Noncopyable &) = delete;

  Noncopyable &operator=(const Noncopyable &) = delete;
};

// 信号量
class sem {
public:
  sem() {
    if (sem_init(&m_sem, 0, 0) != 0) {
      throw std::exception();
    }
  }
  sem(int num) {
    if (sem_init(&m_sem, 0, num) != 0) {
      throw std::exception();
    }
  }
  ~sem() { sem_destroy(&m_sem); }

  // @brief 原子操作信号量减一
  bool wait() { return sem_wait(&m_sem) == 0; }

  // @brief 原子操作信号量加一
  bool post() { return sem_post(&m_sem) == 0; }

private:
  sem_t m_sem;
};

// 互斥锁
class locker : Noncopyable {
public:
  locker() {
    if (pthread_mutex_init(&m_mutex, NULL) != 0) {
      throw std::exception();
    }
  }
  ~locker() { pthread_mutex_destroy(&m_mutex); }

  bool lock() { return pthread_mutex_lock(&m_mutex) == 0; }

  bool unlock() { return pthread_mutex_unlock(&m_mutex) == 0; }

  pthread_mutex_t *get() { return &m_mutex; }

private:
  pthread_mutex_t m_mutex;
};

// 条件变量
class cond {
public:
  cond() {
    if (pthread_cond_init(&m_cond, nullptr) != 0) {
      throw std::exception();
    }
  }
  ~cond() { pthread_cond_destroy(&m_cond); }

  bool wait(pthread_mutex_t *m_mutex) { return pthread_cond_wait(&m_cond, m_mutex) == 0; }

  bool timewait(pthread_mutex_t *m_mutex, struct timespec t) { return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0; }

  bool signal() { return pthread_cond_signal(&m_cond); }

  bool broadcast() { return pthread_cond_broadcast(&m_cond); }

private:
  pthread_cond_t m_cond;
};

#endif