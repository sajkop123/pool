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

int main() {
  std::shared_ptr<Img> pImg;
  pImg = make_shared<Img>(1);
}