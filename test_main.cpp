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

#include "common.h"
#define LOG_TAG MAIN

struct Img {
  Img() : mId(0) { MY_LOGD("ctor %d", mId); }
  ~Img() { MY_LOGD("dtor %d", mId); }
  Img(const Img&) { MY_LOGD("copy"); }
  Img(Img&&) { MY_LOGD("move"); }

  int mId;
  int a[20];
};

const static size_t USER_SIZE = 100;

void test_StandardAllocDealloc() {
  struct TestStruct_Tiny {
    uint32_t thing1;
  };
  struct TestStruct_Medium {
    uint32_t thing1[100];
  };
  struct TestStruct_Large {
    uint32_t thing1[100000];
  };
}

int main() {
  MY_LOGD("sizeof(Img)=%zu", sizeof(Img));
  sharedpool<Img> pool;
  std::clock_t start, end;

  start = clock();
  {
    std::shared_ptr<Img> spImg[USER_SIZE];
    for (size_t i = 0; i < USER_SIZE; ++i) {
      spImg[i] = pool.acquire();
    }
  }
  end = clock();
  printf("%d\n", end-start);

  start = clock();
  {
    std::shared_ptr<Img> spImg[USER_SIZE];
    for (size_t i = 0; i < USER_SIZE; ++i) {
      spImg[i] = std::make_shared<Img>();
    }
  }
  end = clock();
  printf("%d\n", end-start);

  return 0;
}