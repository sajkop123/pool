#include "MemoryPool3.h"

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
    T* p = static_cast<T*>(MemoryPool3::allocate(n * sizeof(T)));
    MY_LOGD("--- p=0x%p, sizeof(T)=%zu", p, n*sizeof(T));
    return p;
  }

  void deallocate(T* p, size_t n) {
    MY_LOGD("p=0x%p, size=%zu", p, n*sizeof(T));
    MemoryPool3::deallocate((void*)p, n*sizeof(T));
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
    MemoryPool3::deallocate(vp, sizeof(T));
  }
};

template<typename T>
class sharedpool {
 public:
  sharedpool() = default;

  std::shared_ptr<T> acquire() {
    using MyAllocator = sharedpool_allocator<T>;
    using MyDeleter = sharedpool_deleter<T>;
    T* rawPtr = MyAllocator().allocate(1);
    MyDeleter del;
    MyAllocator alloc;
    // Creating shared_ptr using the custom allocator and custom deleter
    std::shared_ptr<T> p(
        rawPtr,
        del,
        alloc
    );
    return p;
  }

 private:
  sharedpool_deleter<T> mdel;
  sharedpool_allocator<T> malloc;
  simple_allocator<T> msalloc;
};