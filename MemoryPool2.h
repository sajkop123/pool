#include <mutex>
#include <array>

#include "common.h"
#define TAG_LOG MemoryPool2

#define COUNT_NUM_TRAILING_ZEROES_UINT32(bits) __builtin_ctz(bits)
#define COUNT_NUM_TRAILING_ZEROES_UINT64(bits) __builtin_ctz(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT32(bits) __builtin_clzl(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT64(bits) __builtin_clzll(bits)

class MemoryPool2 {
  const static size_t BYTE_ALIGNMENT = 8;
  const static uint32_t MAX_CELLS_PER_ARENA = 32;
  const static uint32_t MAX_CELL_SIZE_POW2_BASE = 23;
  const static uint64_t MAX_CELL_SIZE = 1ULL << 23;
  const static uint64_t MAX_ARENA_SIZE = 1ULL << 23;
  const static uint64_t OUTSIDE_SYSTEM_MARKER = 0x0123ABCD0123ABCD;
  const static uint64_t VALID_CELL_HEADER_MARKER = 0xEEEEFFFFDDDD0123;
  const static uint64_t VALID_ARENA_HEADER_MARKER = 0x0123ABCD0123ABCD;

  struct GlobalState;
  struct ArenaCollection;
  struct ArenaHeader;
  struct CellHeader;

  struct ArenaCollection {
    uint32_t mCellSizeInBytes = 0;
    uint32_t mNumArenas = 0;
    // std::mutex mMutex;
    ArenaHeader* mRootArena = nullptr;
  };

  struct ArenaHeader {
    uint32_t mCellCapacity = 0;
    uint32_t mCellSizeInBytes = 0;
    uint32_t mPaddingSizeInBytes = 0;
    uint64_t mOccupationBits = 0;
    uint32_t mNumOccupiedCells = 0;
    size_t mArenaSizeInBytes = 0;  // for free memory
    unsigned char* mArenaStart = nullptr;
    unsigned char* mArenaEnd = nullptr;
    ArenaCollection* mpCollection = nullptr;
    ArenaHeader* mNextArena = nullptr;
    uint64_t mGuard = VALID_ARENA_HEADER_MARKER;
  };

  struct CellHeader {
    ArenaHeader* mpArena = nullptr;
    uint64_t mGuard = VALID_CELL_HEADER_MARKER;
  };

  struct GlobalState {
    GlobalState() = default;
    ~GlobalState();
    GlobalState(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    std::array<ArenaCollection, MAX_CELL_SIZE_POW2_BASE> mArenaCollections;
    std::mutex mMutex;
  };

 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);
  static void shutdown();

 private:
  static GlobalState& getGlobalState();
  static uint32_t callCellSizeAndArenaIdx(
      size_t allocSize, uint32_t& arenaIdx);
  static ArenaHeader* allocateArenaOfMemory(
      size_t cellSizeNoHeader, size_t byte_alignment,
      ArenaCollection* collection);
  static uint32_t calcCellCapacity(
      uint32_t cellSizeNoHeader, uint32_t maxArenaSize,
      uint32_t maxCellsPerArena);
};

