#include <iostream>
#include <memory>
#include <string.h>
#include <cassert>
#include <thread>
#include <syncstream>
#include <thread>
#include <sstream>

#define LOG_LEVEL 1
#define N_DEBUG 1

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define MY_LOGD(fmt, arg...) if (LOG_LEVEL >= 2) { printf("[%s/%d][%s] " fmt"\n", __FILENAME__, __LINE__, __func__, ##arg); }
#define MY_LOGI(fmt, arg...) if (LOG_LEVEL >= 1) { printf("[%s/%d][%s] " fmt"\n", __FILENAME__, __LINE__, __func__, ##arg); }

#define assertm(exp, msg) assert(((void)msg, exp))


