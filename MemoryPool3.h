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
 private:
  const static size_t BYTE_ALIGNMENT = 8;
  const static uint32_t MAX_CELLS_PER_ARENA = 32;
  const static uint32_t MAX_CELL_SIZE_POW2_BASE = 23;
  const static uint64_t MAX_CELL_SIZE = 1ULL << 23;
  const static uint64_t MAX_ARENA_SIZE = 1ULL << 23;
  const static uint64_t OUTSIDE_SYSTEM_MARKER = 0x0123ABCD0123ABCD;
  const static uint64_t VALID_CELL_HEADER_MARKER = 0xEEEEFFFFDDDD0123;
  const static uint64_t VALID_ARENA_HEADER_MARKER = 0x0123ABCD0123ABCD;

 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);

 private:
  const static uint32_t CELL_NUMS = 8;
  struct DataInfo {
    size_t mCellSizeInBytes;
    uint64_t mOccupationBits = 0;
    uint64_t mNumOccupiedCells = 0;
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

