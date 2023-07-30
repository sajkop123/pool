#include <mutex>
#include <unordered_map>
#include <memory>

#include "common.h"
#define TAG_LOG MemoryPool3

#define COUNT_NUM_TRAILING_ZEROES_UINT32(bits) __builtin_ctz(bits)
#define COUNT_NUM_TRAILING_ZEROES_UINT64(bits) __builtin_ctz(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT32(bits) __builtin_clzl(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT64(bits) __builtin_clzll(bits)

class MemoryPool3 {
 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);

 private:
  const static uint32_t CELL_NUMS = 8;
  struct DataInfo {
    size_t mCellSizeInBytes;
    uint32_t mOccupationBits = 0;
    uint32_t mNumOccupiedCells = 0;
    std::unique_ptr<uint8_t[]> mMemory;
  };
  struct GlobalState {
    GlobalState() = default;
    GlobalState(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    std::unordered_map<size_t, DataInfo> map;
    std::mutex sMutex;
  };

 private:
  static GlobalState& getGlobalState();
};

