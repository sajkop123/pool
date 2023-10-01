#include "MemoryPool4.h"

#include <memory>
namespace strm {

template<typename T>
void func(T t) {
  puts(__PRETTY_FUNCTION__);
}

template<typename T>
class simple_allocator {
 public:
  using value_type = T;

  simple_allocator() = default;
  simple_allocator(const simple_allocator&) = default;
  simple_allocator(simple_allocator&&) = default;

  template<typename U>
  simple_allocator(const simple_allocator<U>& other) noexcept {
  }

  [[nodiscard]] T* allocate(size_t n) {
    MY_LOGD("%zu", sizeof(T)*n);
    return static_cast<T*>(::operator new(n*sizeof(T)));
  }

  void deallocate(T* p, size_t n) {
    MY_LOGD("%zu", sizeof(T)*n);
    ::free(p);
  }
};

template<typename T>
class sharedpool_allocator {
 public:
  using value_type = T;

  sharedpool_allocator() = default;
  sharedpool_allocator(const sharedpool_allocator&) = default;
  sharedpool_allocator(sharedpool_allocator&&) = default;

  template<typename U>
  sharedpool_allocator(const sharedpool_allocator<U>& other) noexcept {
  }

  [[nodiscard]] T* allocate(size_t n) {
    T* p = static_cast<T*>(GlobalMemPool::getInstance().allocate(n * sizeof(T)));
    return p;
  }

  void deallocate(T* p, size_t n) {
    GlobalMemPool::getInstance().deallocate((void*)p, n*sizeof(T));
  }
};

struct Debug {
  std::string mUserName;
};


template<typename T>
class sharedpool_allocator_debug {
 public:
  using value_type = T;

  sharedpool_allocator_debug() = default;
  sharedpool_allocator_debug(const sharedpool_allocator_debug&) = default;
  sharedpool_allocator_debug(sharedpool_allocator_debug&&) = default;

  template<typename U>
  sharedpool_allocator_debug(const sharedpool_allocator_debug<U>& other) noexcept {
  }

  [[nodiscard]] T* allocate(size_t n) {
    T* p = static_cast<T*>(GlobalMemPool::getInstance().allocate(n * sizeof(T)));
    return p;
  }

  void deallocate(T* p, size_t n) {
    GlobalMemPool::getInstance().deallocate((void*)p, n*sizeof(T));
  }
};


template<class _Tp, typename... _Args>
inline std::shared_ptr<_Tp>
make_shared(_Args&&... __args) {
  auto p = std::allocate_shared<_Tp>(sharedpool_allocator<_Tp>(),
                                     std::forward<_Args>(__args)...);
  return p;
}


template<class _Tp, typename... _Args>
inline std::shared_ptr<_Tp>
make_shared2(const Debug& debug, _Args&&... __args) {
  struct _Tp_Debug : public _Tp, public Debug {
    using _Tp::_Tp;
  };
  auto p = std::allocate_shared<_Tp_Debug>(
               sharedpool_allocator_debug<_Tp_Debug>(),
               std::forward<_Args>(__args)...);
  p->mUserName = debug.mUserName;
  std::shared_ptr<_Tp> p_res = std::shared_ptr<_Tp>(p.get(),
      [p](_Tp* raw_p) {
        MY_LOGI("%s", p->mUserName.c_str());
      });
  return p;
}
struct PoolConfig {
  uint32_t mCapacity;
};
template<class _Tp>
class ObjectPool {
 public:

 public:
  ObjectPool(const PoolConfig& config)
      : mAllocInfo(sizeof(_Tp), config.mCapacity) {
    mAllocInfo.print();
  }

  template<typename ..._Args>
  std::shared_ptr<_Tp> acquire(_Args&&... __args) {
    return std::allocate_shared<_Tp>(sharedpool_allocator<_Tp>(),
                                     std::forward<_Args>(__args)...);
  }

 private:
  AllocInfo mAllocInfo;
  MemoryPool4::ArenaCollection mArenaCollection;
};

};