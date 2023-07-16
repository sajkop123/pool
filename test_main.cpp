// #include "Pool1.h"
#include "PoolSingleton.h"
#include "common.h"

#include <vector>
#include <iostream>

struct Img : public Poolable<Img> {
  Img(int id) : mId(id) { MY_LOGD("ctor %d", mId); }
  ~Img() { MY_LOGD("dtor %d", mId); }
  Img(const Img&) { MY_LOGD("copy"); }
  Img(Img&&) { MY_LOGD("move"); }

  int mId;
};

void* operator new(size_t size) {
  void *p = malloc(size);
  MY_LOGD("allocate Poolable%p %zu",p , size);
  return p;
}

void operator delete(void* p) {
  MY_LOGD("free %p",p);
  free(p);
}

int main() {
  // std::shared_ptr<Img> pImg;
  // pImg = make_shared<Img>(1);

  // int *p = new int(2);
  // delete(p);

  {
    MY_LOGD("sizeof(global_pool<Img>)=%zu", sizeof(global_pool<Img>));
    std::unique_ptr<Img> p = std::make_unique<Img>(1);
  }
}