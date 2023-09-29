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
struct AllocInfo {
  // only used when allocate arena
  uint32_t mCellBodySize;
  uint32_t mMaxCellCountPerArena;
  uint32_t mFullCellBit;
  uint32_t mInvalidOccupyBit;
  uint32_t mInvalidCellIdx;

  constexpr static size_t sMinMemoryChunk = 1 << 7;  // 128bytes
  static uint32_t calcMaxCellCountPerArena(uint32_t cellBodySize,
                                           uint32_t userCount);

  AllocInfo() {}
  AllocInfo(uint32_t mCellBodySize,
            uint32_t maxCellCountPerArena);
  void print() const {
    MY_LOGD("cellBoldySize=%u mMaxCellCountPerArena=%u mFullCellBit=0x%lx "
            "mInvalidOccupyBit=0x%lx, mInvalidCellIdx=%u",
            mCellBodySize, mMaxCellCountPerArena, mFullCellBit,
            mInvalidOccupyBit, mInvalidCellIdx);
  }
};

class MemoryPool4 {
 public:
  const static size_t BYTE_ALIGNMENT = 8;
  const static uint32_t MAX_CELLS_PER_ARENA = 8;
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
    const ArenaCollection* mpCollection = nullptr;
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

 private:
  const static size_t CellHeaderSize = sizeof(CellHeader);
  const static size_t ArenaHeaderSize = sizeof(ArenaHeader);

 public:
  static void shutdown();
  static void* allocate(const AllocInfo& info,
                         ArenaCollection& collection);
  static void deallocate(void* data,
                          size_t size);

 private:
  static std::unique_ptr<uint8_t[]>
      allocateArenaOfMemory(const AllocInfo& info,
                            const ArenaCollection& collection);
};

struct GlobalMemPool {
  static GlobalMemPool& getInstance();
  GlobalMemPool();
  ~GlobalMemPool();
  GlobalMemPool(const GlobalMemPool&) = delete;
  GlobalMemPool(GlobalMemPool&&) = delete;
  GlobalMemPool operator=(const GlobalMemPool&) = delete;
  GlobalMemPool operator=(GlobalMemPool&&) = delete;

  void* allocate(size_t size);
  void deallocate(void* data, size_t size);

 private:
  constexpr static size_t BYTE_ALIGNMENT = (1 << 3);
  constexpr static size_t MAX_ARENA_COUNT = 20;
  constexpr static uint64_t MAX_CELL_BODY_SIZE =
      BYTE_ALIGNMENT << MAX_ARENA_COUNT;

 private:
  uint32_t calcCellSizeAndArenaId(
      size_t allocSize,
      uint32_t& arenaIdx);

 private:
  friend class MemoryPool4;
  // key = sizeof(cell) align to power of 2
  std::array<MemoryPool4::ArenaCollection, MAX_ARENA_COUNT> mArenaCollections;
  std::array<AllocInfo, MAX_ARENA_COUNT> mAllocInfo;
};

