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
#include <ctime>
#include <thread>

#include "common.h"
#define LOG_TAG MAIN

struct Img {
  Img() : mId(0) { MY_LOGD("ctor %d", mId); }
  Img(int id) : mId(id) { MY_LOGD("ctor %d", mId); }
  ~Img() { MY_LOGD("dtor %d", mId); }
  Img(const Img&) { MY_LOGD("copy"); }
  Img(Img&&) { MY_LOGD("move"); }

  int mId;
  int a[20];
};

const static size_t TOTAL_SIZE = 5;
const static size_t USER_SIZE = 2;

template<typename FUNC, typename...Args>
void run(size_t concurrency, size_t bound, FUNC&& func, Args... args) {
  auto&& worker = [&]() {
    for (size_t i = 0; i < bound; ++i) {
      func(std::forward<Args>(args)...);
    }
  };
  std::vector<std::thread> threads;
  threads.reserve(concurrency);
  for (size_t i = 0; i < concurrency; ++i) {
    threads.emplace_back(worker);
  }
  for (size_t i = 0; i < concurrency; ++i) {
    threads[i].join();
  }
}

static void make_shared_stl() {
  std::shared_ptr<Img> sp[USER_SIZE] = {nullptr};
  for (size_t i = 0; i < TOTAL_SIZE; ++i) {
    sp[i%USER_SIZE] = std::make_shared<Img>();
  }
}
static void make_shared_strm() {
  std::shared_ptr<Img> sp[USER_SIZE] = {nullptr};
  for (size_t i = 0; i < TOTAL_SIZE; ++i) {
    sp[i%USER_SIZE] = strm::make_shared<Img>();
    printf("%zu\n", i);
  }
}

int main() {
  // MY_LOGD("sizeof(Img)=%zu", sizeof(Img));
  // sharedpool<Img> pool;
  std::clock_t start, end;

  // {
  //   start = clock();
  //   std::shared_ptr<Img> spImg[USER_SIZE];
  //   for (size_t i = 0; i < TOTAL_SIZE; ++i) {
  //     spImg[i%USER_SIZE] = std::make_shared<Img>(i);
  //     // std::shared_ptr<Img> p = std::make_shared<Img>(i);
  //   }
  //   end = clock();
  //   printf("%d\n", end-start);
  // }
  // {
  //   start = clock();
  //   std::shared_ptr<Img> spImg[USER_SIZE];
  //   for (size_t i = 0; i < TOTAL_SIZE; ++i) {
  //     spImg[i%USER_SIZE] = strm::make_shared<Img>(i);
  //     // std::shared_ptr<Img> p = strm::make_shared<Img>(i);
  //   }
  //   end = clock();
  //   printf("%d\n", end-start);
  // }
  // {
  //   start = clock();
  //   run(8, 1, make_shared_stl);
  //   end = clock();
  //   printf("%d\n", end-start);
  // }
  {
    start = clock();
    run(8, 1, make_shared_strm);
    end = clock();
    printf("elapsed time = %d\n", end-start);
  }

  return 0;
}