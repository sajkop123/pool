#include <mutex>
#include <array>
#include <atomic>

#include "common.h"
#define TAG_LOG MemoryPool4

/**
 * What flexibility/customization I should make?
 *   1. max cell size in an arena
 *   2. strategy if all cells are exhausting in an arena
 */
class MemoryPool4 {
 public:
  const static size_t MIN_CELL_BODY_SIZE = 8;
  const static size_t BYTE_ALIGNMENT = 8;
  const static uint32_t MAX_CELLS_PER_ARENA = 8;
  const static uint32_t MAX_CELL_SIZE_POW2_BASE = 23;
  const static uint64_t MAX_CELL_SIZE = 1ULL << MAX_CELL_SIZE_POW2_BASE;
  const static uint64_t MAX_ARENA_SIZE = 1ULL << MAX_CELL_SIZE_POW2_BASE;
  const static uint64_t OUTSIDE_SYSTEM_MARKER = 0x1234ABCD1234ABCD;
  const static uint64_t VALID_CELL_HEADER_MARKER = 0xFFFFAAAAFFFFAAAA;
  const static uint64_t VALID_ARENA_HEADER_MARKER = 0xAAAA1111FFFF8888;

  struct GlobalState;
  struct ArenaCollection;
  struct ArenaHeader;
  struct CellHeader;

  struct ArenaCollection {
    uint32_t mCellBodySize = 0;
    uint32_t mNumArenas = 0;
    std::mutex mMutex;
    std::unique_ptr<uint8_t[]> mRootArena = nullptr;
  };

  struct alignas(BYTE_ALIGNMENT) ArenaHeader {
    uint32_t mCellCapacity = 0;
    uint32_t mCellBodySize = 0;
    std::atomic<uint32_t> mOccupationBits = 0;
    ArenaCollection* mpCollection = nullptr;
    unsigned char* mCellStart = nullptr;
    unsigned char* mCellEnd = nullptr;
    std::unique_ptr<uint8_t[]> mNextArena = nullptr;
    uint64_t mGuard = VALID_ARENA_HEADER_MARKER;

    inline size_t getNumOccupiedCells() const {
      return __builtin_popcountl(mOccupationBits);
    }
  };

  struct alignas(BYTE_ALIGNMENT) CellHeader {
    ArenaHeader* mpArena = nullptr;
    uint64_t mGuard = VALID_CELL_HEADER_MARKER;
    template<typename T>
    std::string printMemory() const;
  };

  struct GlobalState {
    GlobalState() = default;
    ~GlobalState();
    GlobalState(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    // key = sizeof(cell) align to power of 2
    std::array<ArenaCollection,
               MAX_CELL_SIZE_POW2_BASE> mArenaCollections;
  };

 private:
  const static size_t CellHeaderSize = sizeof(CellHeader);
  const static size_t ArenaHeaderSize = sizeof(ArenaHeader);
  const static uint32_t ALL_CELL_IN_BIT = []() {
    uint32_t val = 0;
    for (size_t i = 0; i < MAX_CELLS_PER_ARENA; ++i) {
      val = val << 1;
      val |= 1UL;
    }
    return val;
  }();
  const static uint32_t INVALID_OCCUPY_BIT = []() {
    uint32_t val = 1;
    for (size_t i = 0; i < MAX_CELLS_PER_ARENA; ++i) {
      val = val << 1;
    }
    return val;
  }();
  const static uint32_t INVALID_CELL_INDEX = MAX_CELLS_PER_ARENA + 1;

 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);
  static void shutdown();

 private:
  static GlobalState& getGlobalState();
  static uint32_t callCellSizeAndArenaIdx(
      size_t allocSize, uint32_t& arenaIdx);
  static std::unique_ptr<uint8_t[]> allocateArenaOfMemory(
      uint32_t& cellBodySize, ArenaCollection* collection);
  static ArenaHeader* getArenaHeader(std::unique_ptr<uint8_t[]>& up) {
    return reinterpret_cast<ArenaHeader*>(up.get());
  }
};

// struct CellSizePolicy {
//   virtual size_t toCellSize(size_t size) = 0;
// };

// struct CellSizePolicy_PowerOf2 : public CellSizePolicy {
//   size_t toCellSize(size_t size) override;
// };

// struct CellSizePolicy_NoChange : public CellSizePolicy {
//   size_t toCellSize(size_t size) override;
// };

// template<class Policy>
// struct StaticPoolPolicy {
//   static size_t toCellSize(size_t size) {
//     return T::toCellSize(size);
//   }
// };

// struct GlobalMemoryPool {
//   friend class MemoryPool4;
//   static GlobalMemoryPool getGlobalState();
//   GlobalMemoryPool() = delete;
//   ~GlobalMemoryPool();
//   GlobalMemoryPool(const GlobalMemoryPool&) = delete;
//   GlobalMemoryPool(GlobalMemoryPool&&) = delete;
//   MemoryPool4::ArenaCollection* getArenaCollection(size_t id);
//   // key = sizeof(cell) align to power of 2
//   std::array<MemoryPool4::ArenaCollection,
//              MemoryPool4::MAX_CELL_SIZE_POW2_BASE> mArenaCollections;

// };