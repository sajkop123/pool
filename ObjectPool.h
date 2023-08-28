#include "MemoryPool4.h"

#include <memory>

// template<typename T>
// class simple_allocator {
//  public:
//   using value_type = T;

//   simple_allocator() = default;
//   simple_allocator(const simple_allocator&) = default;
//   simple_allocator(simple_allocator&&) = default;

//   template<typename U>
//   simple_allocator(const simple_allocator<U>& other) noexcept {
//   }

//   template<typename U>
//   struct rebind {
//     typedef simple_allocator<U> other;
//   };

//   [[nodiscard]] T* allocate(size_t n) {
//     T* p = reinterpret_cast<T*>(::operator new(n * sizeof(T)));
//     MY_LOGD("p=0x%p, sizeof(T)=%zu", p, n*sizeof(T));
//     return p;
//   }

//   void deallocate(T* p, size_t n) {
//     MY_LOGD("p=0x%p, size=%zu", p, n*sizeof(T));
//     ::operator delete(p);
//   }
// };


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
    T* p = static_cast<T*>(MemoryPool4::allocate(n * sizeof(T)));
    return p;
  }

  void deallocate(T* p, size_t n) {
    MemoryPool4::deallocate((void*)p, n*sizeof(T));
  }
};

template<class _Tp, typename... _Args>
inline std::shared_ptr<_Tp>
make_shared(_Args&&... __args) {
  auto p = std::allocate_shared<_Tp>(sharedpool_allocator<_Tp>(),
              std::forward<_Args>(__args)...);
  return p;
}

// template<typename T>
// struct Poolable {
//   /**
//    * optional
//    */
//   virtual void onReturnTolPool() noexcept {};
//   virtual void onTakenFromPool() noexcept {};

//  private:  // instantiation
//   global_pool<T>* g_pool = global_pool<T>::instance();
// };

// template<typename T,
//          bool is_base_of_poolable = std::is_base_of_v<Poolable<T>, T>,
//          typename... Args>
// std::shared_ptr<T> make_shared(Args&&... __args) {
//   if constexpr (is_base_of_poolable) {
//     MY_LOGD("use acquire from pool make_shared");
//     auto p_pool = global_pool<T>::instance();
//     std::shared_ptr<T> p = p_pool->acquire();
//     return p;
//   } else {
//     MY_LOGD("use std::make_shared");
//     return std::make_shared<T>(std::forward<Args>(__args)...);
//   }
// }
};