#include <iostream>
#include <memory>

#define MY_LOGD(fmt, arg...) printf("[%s] " fmt"\n", __func__, ##arg);