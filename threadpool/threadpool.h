#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <pthread.h>
#include <assert.h>
#include <mutex>
#include <list>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool {
public:
  threadpool(int actor_model, connection_pool *connPool, int threads_number = 8, int max_requests = 100);
  ~threadpool();
  bool append(T *request, int state);
  bool append_p(T *request);

private:
  // @brief 工作线程运行的函数, 不断从工作队列中取出任务并执行
  static void *worker(void *arg);
  void         run();

private:
  int            m_threads_nums;  //
  int            m_max_requests;  // 请求队列最大请求数
  pthread_t     *m_threads;       // 线程池数组
  std::list<T *> m_wokerqueue;    // 请求队列
  locker         m_queuelocker;   // 保护请求队列
  sem            m_queuestate;    // 资源数
  cond           m_cond;

  connection_pool *m_connPool;     // 数据库连接池
  int              m_actor_model;  // 并发模型选择
  bool             m_stop;         // 是否结束线程
};


/* 创建thread_num个线程, 并都设置为脱离线程 */
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int threads_number, int max_requests)
    : m_threads_nums(threads_number), m_max_requests(max_requests), m_connPool(connPool), m_actor_model(actor_model) {
  if (threads_number <= 0 || max_requests <= 0)
    throw std::exception();
  m_threads = new pthread_t[m_threads_nums];
  if (m_threads == nullptr)
    throw std::exception();

  m_stop = false;
  int i;
  for (i = 0; i < m_threads_nums; i++) {
    if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
      delete[] m_threads;
      throw std::exception();
    }
    if (pthread_detach(m_threads[i]) != 0) {  // fail
      delete[] m_threads;
      throw std::exception();
    }
  }
}

template <typename T>
threadpool<T>::~threadpool() {
  delete[] m_threads;
  m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request, int state) {
  m_queuelocker.lock();
  if (m_wokerqueue.size() >= static_cast<size_t>(m_max_requests)) {
    m_queuelocker.unlock();
    return false;
  }
  request->m_state = state;
  m_wokerqueue.push_back(request);
  m_queuelocker.unlock();
  m_queuestate.post();
  return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request) {
  m_queuelocker.lock();
  if (m_wokerqueue.size() >= static_cast<size_t>(m_max_requests)) {
    m_queuelocker.unlock();
    return false;
  }
  m_wokerqueue.push_back(request);
  m_queuelocker.unlock();
  m_queuestate.post();
  return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg) {
  threadpool *pool = (threadpool *)arg;
  pool->run();
  return pool;
}

template <typename T>
void threadpool<T>::run() {
  while (!m_stop) {
    m_queuestate.wait();
    m_queuelocker.lock();
    if (m_wokerqueue.empty()) {
      m_queuelocker.unlock();
      continue;
    }
    T *request = m_wokerqueue.front();
    m_wokerqueue.pop_front();
    m_queuelocker.unlock();
    if (!request) {
      continue;
    }
    if (m_actor_model == 0) {  // Reactor事件处理模式
      connectionRAII mysqlconn(&request->mysql, m_connPool);
      request->process();
    } else if (m_actor_model == 1) {  // Proactor事件处理模式
      if (request->m_state == 0) {    //  读事件
        if (request->read_once()) {
          request->improv = 1;
          connectionRAII mysqlconn(&request->mysql, m_connPool);
          request->process();
        } else {
          request->improv = 1;
          request->timer_flag = 1;
        }
      } else {  // 写事件
        if (request->write()) {
          request->improv = 1;
        } else {
          request->improv = 1;
          request->timer_flag = 1;
        }
      }
    }
  }
}


#endif
