#include "common.h"
#define TAG_LOG PoolSingleton

#include <memory>
#include <type_traits>
#include <queue>

template<typename T>
class global_pool {
 private:
  struct Deleter {
    Deleter() = default;
    void operator()(T* p) const noexcept {
      if (p) {
        p->onReturnTolPool();
      }
    }
  };

 public:
  static global_pool<T>* instance () {
    static global_pool<T> instance;
    return &instance;
  }

  std::shared_ptr<T> acquire() {
    if (m_free.empty()) {
      m_free.emplace(T(0));
    }
    // return std::make_shared<T>(1);
    return std::shared_ptr<T>(&m_free.front(), Deleter());
  }

 private:
  std::queue<T> m_free;
  std::queue<T> m_use;
};


template<typename T>
struct Poolable {
  /**
   * optional
   */
  virtual void onReturnTolPool() noexcept {};
  virtual void onTakenFromPool() noexcept {};

 private:  // instantiation
  global_pool<T>* g_pool = global_pool<T>::instance();
};

template<typename T,
         bool is_base_of_poolable = std::is_base_of_v<Poolable<T>, T>,
         typename... Args>
std::shared_ptr<T> make_shared(Args&&... __args) {
  if constexpr (is_base_of_poolable) {
    MY_LOGD("use acquire from pool make_shared");
    auto p_pool = global_pool<T>::instance();
    std::shared_ptr<T> p = p_pool->acquire();
    return p;
  } else {
    MY_LOGD("use std::make_shared");
    return std::make_shared<T>(std::forward<Args>(__args)...);
  }
}