#include "MemoryPool4.h"

#include <memory>
namespace strm {

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

template<class _Tp, typename... _Args>
inline std::shared_ptr<_Tp>
make_shared(_Args&&... __args) {
  auto p = std::allocate_shared<_Tp>(sharedpool_allocator<_Tp>(),
                                     std::forward<_Args>(__args)...);
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