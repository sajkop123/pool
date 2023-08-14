#include "MemoryPool4.h"

#include <memory>

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

  template<typename U>
  struct rebind {
    typedef simple_allocator<U> other;
  };

  [[nodiscard]] T* allocate(size_t n) {
    T* p = reinterpret_cast<T*>(::operator new(n * sizeof(T)));
    MY_LOGD("p=0x%p, sizeof(T)=%zu", p, n*sizeof(T));
    return p;
  }

  void deallocate(T* p, size_t n) {
    MY_LOGD("p=0x%p, size=%zu", p, n*sizeof(T));
    ::operator delete(p);
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
    MY_LOGD("+++ sizeof(T)=%zu", n*sizeof(T));
    T* p = static_cast<T*>(MemoryPool4::allocate(n * sizeof(T)));
    MY_LOGD("--- p=0x%p, sizeof(T)=%zu", p, n*sizeof(T));
    return p;
  }

  void deallocate(T* p, size_t n) {
    MY_LOGD("p=0x%p, size=%zu", p, n*sizeof(T));
    MemoryPool4::deallocate((void*)p, n*sizeof(T));
  }
};

template<typename T>
class sharedpool_deleter {
 public:
  sharedpool_deleter() = default;
  sharedpool_deleter(const sharedpool_deleter&) = default;
  sharedpool_deleter(sharedpool_deleter&&) = default;

  void operator()(T* p) const noexcept {
    void* vp = static_cast<void*>(p);
    MY_LOGD("p=0x%p, size=%zu", p, sizeof(T));
    MemoryPool4::deallocate(vp, sizeof(T));
  }
};

template<typename T>
class sharedpool {
 public:
  sharedpool() = default;

  std::shared_ptr<T> acquire() {
    return std::allocate_shared<T>(sharedpool_allocator<T>());
  }

 private:
  sharedpool_allocator<T> malloc;
};

namespace strm {
  template<class _Tp, typename... _Args>
    inline std::shared_ptr<_Tp>
    make_shared(_Args&&... __args) {
      return std::allocate_shared<_Tp>(sharedpool_allocator<_Tp>(),
                  std::forward<_Args>(__args)...);
    }
};