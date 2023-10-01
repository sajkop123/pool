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

static void wirte_data(void* p, size_t size) {
  memset(p, 1, size);
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
  struct A {
    // std::shared_ptr<int> m;
    int m[3];
    A(int a) {
      m[0] = a;
      m[1] = a;
      m[2] = a;
    }
  };

  // A* p = nullptr;
  // {
  //   std::shared_ptr<A> a = std::make_shared<A>(1);
  //   p = a.get();
  //   std::shared_ptr<A> a1 = strm::make_shared<A>(2);
  //   std::shared_ptr<A> a2 = strm::make_shared<A>(3);
  // }
  // MY_LOGI("%d, %d, %d", p->m[0], p->m[1], p->m[2]);


  // {
  //   strm::PoolConfig config;
  //   config.mCapacity = 4;
  //   strm::ObjectPool<A> pool(config);
  //   auto _f = [&pool](int a, int b, int c) {
  //     auto p = pool.acquire();
  //     p->m[0] = a;
  //     p->m[1] = b;
  //     p->m[2] = c;
  //     assertm(p->m[0] == a && p->m[1] == b && p->m[2] == c,
  //             "value not match");
  //     std::this_thread::sleep_for(std::chrono::milliseconds(500));
  //   };
  //   run(8, 4, _f, 1, 2, 3);
  // }

  // {
  //   int concurrency = 8;
  //   int loop = 10;

  //   auto _l2 = [&loop]() {
  //     for (int i = 0; i < loop; ++i) {
  //       auto p = strm::make_shared<A>(i);
  //       std::this_thread::sleep_for(std::chrono::milliseconds(50));
  //       assertm(p->m[0] == i && p->m[1] == i && p->m[2] == i,
  //               "value not match");
  //     }
  //   };

  //   std::vector<std::thread> threads;
  //   threads.reserve(concurrency);
  //   for (int i = 0; i < concurrency; ++i) {
  //     threads.emplace_back(_l2);
  //   }
  //   for (size_t i = 0; i < concurrency; ++i) {
  //     threads[i].join();
  //   }
  // }

  {
    strm::Debug debugA;
    debugA.mUserName = std::string("asdasd");
    std::shared_ptr<A> p = strm::make_shared2<A>(debugA, 10);
  }

  return 0;
}