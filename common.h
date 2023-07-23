#include <iostream>
#include <memory>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define MY_LOGD(fmt, arg...) printf("[%s/%d][%s] " fmt"\n", __FILENAME__, __LINE__, __func__, ##arg);