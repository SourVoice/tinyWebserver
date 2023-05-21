#include "timer.h"



sort_timer_lst::sort_timer_lst() : head(nullptr), tail(nullptr) {}

// 销毁链表
sort_timer_lst::~sort_timer_lst() {
  util_timer *cur = head;
  while (cur) {
    head = cur->next;
    delete cur;
    cur = head;
  }
}

void sort_timer_lst::add_timer(util_timer *timer) {
  if (!timer)
    return;

  if (!head) {
    head = tail = timer;
    return;
  }
  if (timer->expire < head->expire) {  // 目标定时器的超时时间小于容器内所有定时器(升序链表)
    timer->next = head;
    head->prev = timer;
    head = timer;
    return;
  }
  add_timer(timer, head);
}

// 定时任务发生变化, 调整位置, 只考虑定时器超时时间延长情况
void sort_timer_lst::adjust_timer(util_timer *timer) {

  if (!timer)
    return;
  util_timer *cur = timer->next;
  // 目标定时器位于链表尾部, 或者目标仍然小于下一个, 不用调整
  if (!cur || (timer->expire < cur->expire))
    return;

  if (timer == head) {  // 取出节点重新加入
    head = head->next;
    head->prev = nullptr;
    timer->next = nullptr;
    add_timer(timer, head);
  } else {  // 同理, 取出节点重新加入, 只不过目标节点不是头节点
    timer->prev->next = cur;
    cur->prev = timer->prev;
    add_timer(timer, cur);
  }
}

void sort_timer_lst::del_timer(util_timer *timer) {
  if (!timer)
    return;

  if ((timer == head) && (timer == tail)) {
    delete timer;
    head = nullptr;
    tail = nullptr;
  } else if (timer == head) {
    head = head->next;
    head->prev = nullptr;
    delete timer;
  } else if (timer == tail) {
    tail = tail->prev;
    tail->next = nullptr;
    delete timer;
  } else {  // 链表中间
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
  }
  return;
}

// SIGLARM信号每次被触发, 就在其信号处理函数中执行一次tick
void sort_timer_lst::tick() {
  if (!head)
    return;
  printf("timer tick\n");

  time_t      cur_t = time(NULL);
  util_timer *cur = head;

  // 处理全部超时定时器
  while (cur) {
    // 定时器尚未到时, 退出
    if (cur_t < cur->expire)
      break;

    // 执行定时器的定时任务, 之后将其删除, 并重置头节点
    cur->cb_func(cur->user_data);
    head = cur->next;
    if (head) {
      head->prev = nullptr;
    }
    delete cur;
    cur = head;
  }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
  util_timer *_prev = lst_head;
  util_timer *cur = _prev->next;
  while (cur) {
    if (timer->expire < cur->expire) {  // 插入timer
      _prev->next = timer;
      timer->next = cur;
      cur->prev = timer;
      timer->prev = _prev;
      break;
    }
    _prev = cur;
    cur = cur->next;
  }
  if (!cur) {
    _prev->next = timer;
    timer->prev = _prev;
    timer->next = nullptr;
    tail = timer;
  }
}