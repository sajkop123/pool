// // #include "Pool1.h"
// #include "PoolSingleton.h"
// #include "common.h"

// #include <vector>
// #include <iostream>

// struct Img : public Poolable<Img> {
//   Img(int id) : mId(id) { MY_LOGD("ctor %d", mId); }
//   ~Img() { MY_LOGD("dtor %d", mId); }
//   Img(const Img&) { MY_LOGD("copy"); }
//   Img(Img&&) { MY_LOGD("move"); }

//   int mId;
// };

// void* operator new(size_t size) {
//   void *p = malloc(size);
//   MY_LOGD("allocate Poolable%p %zu",p , size);
//   return p;
// }

// void operator delete(void* p) {
//   MY_LOGD("free %p",p);
//   free(p);
// }

// int main() {
//   // std::shared_ptr<Img> pImg;
//   // pImg = make_shared<Img>(1);

//   // int *p = new int(2);
//   // delete(p);

//   {
//     MY_LOGD("sizeof(global_pool<Img>)=%zu", sizeof(global_pool<Img>));
//     std::unique_ptr<Img> p = std::make_unique<Img>(1);
//   }
// }


///////////////////////////////////////////////////////////////
// test memory pool + shared pool
///////////////////////////////////////////////////////////////

// #include "Pool1.h"
#include "ObjectPool.h"

#include <vector>
#include <iostream>

#include "common.h"
#define LOG_TAG MAIN

struct Img {
  Img() : mId(0) { MY_LOGD("ctor %d", mId); }
  ~Img() { MY_LOGD("dtor %d", mId); }
  Img(const Img&) { MY_LOGD("copy"); }
  Img(Img&&) { MY_LOGD("move"); }

  int mId;
  // int a[10];
};

// template class sharedpool<Img>;
// template class sharedpool_allocator<Img>;
// template class sharedpool_deleter<Img>;

// default std::bad_alloc exception throwing version
// void* operator new(size_t size) {
//   void* r = MemoryPool::allocate(size);
//   return r;
// }


// void operator delete(void* data) {
//   MemoryPool::deallocate(data, 0);
// }

// void operator delete(void* data, size_t size) {
//   MemoryPool::deallocate(data, size);
// }

int main() {
  MY_LOGD("sizeof(Img)=%zu", sizeof(Img));
  sharedpool<Img> pool;
  MY_LOGD("---------------------------------------------------------");
  std::shared_ptr<Img> spImg(pool.acquire());
  MY_LOGD("---------------------------------------------------------");
  spImg = nullptr;
  MY_LOGD("---------------------------------------------------------");

  // MemoryPool2::shutdown();
  return 0;
}