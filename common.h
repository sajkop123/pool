#include <iostream>
#include <memory>
#include <string.h>

#define LOG_ON 1
#define N_DEBUG 0

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define MY_LOGD(fmt, arg...) if (LOG_ON) { printf("[%s/%d][%s] " fmt"\n", __FILENAME__, __LINE__, __func__, ##arg); }