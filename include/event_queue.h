#ifndef MONITOR_EVENT_QUEUE_H
#define MONITOR_EVENT_QUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>

enum class EventType { NONE, KEY_PRESS, RESIZE, REDRAW };

struct Event {
  EventType type;
  int key;
};

template <class T>
class EventQueue
{
 public:
  T pop() {
    std::unique_lock<std::mutex> uLock(_mutex);
    _cond.wait(uLock, [this] { return !_messages.empty();});
    T msg = std::move(_messages.back());
    _messages.pop_back();
    return msg;
  }

  void push(T &&msg) {
    std::lock_guard<std::mutex> uLock(_mutex);
    _messages.push_back(std::move(msg));
    _cond.notify_one();
  }

  std::size_t getQueueLength() {
    return _messages.size();
  }

 private:
  std::mutex _mutex;
  std::condition_variable _cond;
  std::deque<T> _messages;

};

#endif
