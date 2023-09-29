#include "MemoryPool4.h"

#include <cstring>
#include <memory>
#include <cassert>
#include <sstream>

#include "common.h"
#define TAG_LOG MemoryPool4

#define COUNT_NUM_TRAILING_ZEROES_UINT32(bits) __builtin_ctz(bits)
#define COUNT_NUM_TRAILING_ZEROES_UINT64(bits) __builtin_ctz(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT32(bits) __builtin_clzl(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT64(bits) __builtin_clzll(bits)

#define TO_POW2_UINT32(n)  \
  n == 1 ? 1 : 1 << (32 - COUNT_NUM_LEADING_ZEROES_UINT32(n-1));
#define TO_POW2_UINT64(n)  \
  n == 1 ? 1 : 1 << (64 - COUNT_NUM_LEADING_ZEROES_UINT64(n-1));


const char* getTid() {
  auto myid = std::this_thread::get_id();
  std::stringstream ss;
  ss << myid;
  // std::string mystring = ss.str();
  return ss.str().c_str();
};


AllocInfo::AllocInfo(uint32_t cellBodySize,
                     uint32_t maxCellCountPerArena)
  : mCellBodySize(cellBodySize)
  , mMaxCellCountPerArena(
      calcMaxCellCountPerArena(mCellBodySize, maxCellCountPerArena))
  , mInvalidCellIdx(mMaxCellCountPerArena)
  , mFullCellBit((1UL << mMaxCellCountPerArena) - 1)
  , mInvalidOccupyBit((1UL << mMaxCellCountPerArena)) {}

uint32_t AllocInfo::calcMaxCellCountPerArena(uint32_t cellBodySize,
                                             uint32_t countPerArena) {
  uint32_t size_align8 = (cellBodySize + 7) & (~7);
  return ((size_align8 * countPerArena) / sMinMemoryChunk) > 0 ?
          countPerArena : (sMinMemoryChunk / cellBodySize);
}

void* MemoryPool4::allocate(const AllocInfo& info,
                            ArenaCollection& collection) {
  // double-checked lock pattern - avoid allocate twice.
  if (!collection.mRootArena) {
    std::unique_lock<std::mutex> _l(collection.mMutex);
    if (!collection.mRootArena) {
      collection.mRootArena = allocateArenaOfMemory(info, collection);
      collection.mCellBodySize = info.mCellBodySize;
      collection.mNumArenas++;
    }
  }

  ArenaHeader* arenaHeader =
      reinterpret_cast<ArenaHeader*>(collection.mRootArena.get());
  uint32_t cellIdx = info.mInvalidCellIdx;
  uint32_t oldOccupyBit = info.mInvalidOccupyBit;
  uint32_t newOccupyBit = info.mInvalidCellIdx;
  do {
    oldOccupyBit = arenaHeader->mOccupationBits.load(std::memory_order_acquire);

    // all cells inside the arena are occupied, allocate another one and use
    // link-list to make connection.
    while (oldOccupyBit == info.mFullCellBit) {
      if (!arenaHeader->mNextArena) {
        std::unique_lock<std::mutex> _l(collection.mMutex);
        if (!arenaHeader->mNextArena) {
          arenaHeader->mNextArena = allocateArenaOfMemory(info, collection);
        }
        arenaHeader =
            reinterpret_cast<ArenaHeader*>(collection.mRootArena.get());
        oldOccupyBit = 0;
      }
    }

    cellIdx = COUNT_NUM_TRAILING_ZEROES_UINT32(~oldOccupyBit);
    newOccupyBit = oldOccupyBit | (1UL << cellIdx);
#ifdef DEBUG_ENABLE
    assertm(cellIdx != INVALID_CELL_INDEX, "invalid cell index");
    assertm(oldOccupyBit | INVALID_OCCUPY_BIT, "invalid old occupy bit");
    assertm(newOccupyBit | INVALID_OCCUPY_BIT, "invalid old occupy bit");
#endif  // DEBUG_ENABLE
  } while (!arenaHeader->mOccupationBits.compare_exchange_weak(
            oldOccupyBit, newOccupyBit, std::memory_order_release));

  // now we get a valid cell index
  unsigned char* cellHeader_char =
      arenaHeader->mCellStart +
      (CellHeaderSize + arenaHeader->mCellBodySize) * cellIdx;
  CellHeader* cellHeader = reinterpret_cast<CellHeader*>(cellHeader_char);
  unsigned char* cellBody_char = cellHeader_char + CellHeaderSize;
  MY_LOGI(" %d return cell[id=%u/size=%u][Header:p=0x%p][Body:p=0x%p] "
          "occupy(0x%lX/nums=%u)",
          getTid(), cellIdx, arenaHeader->mCellBodySize, cellHeader, cellBody_char,
          newOccupyBit, arenaHeader->getNumOccupiedCells());
#ifdef DEBUG_ENABLE
  assertm(cellHeader->mGuard == VALID_CELL_HEADER_MARKER, "cell guard is wrong");
  assertm(arenaHeader->mGuard == VALID_ARENA_HEADER_MARKER, "arena guard is wrong");
#endif  // DEBUG_ENABLE
  return reinterpret_cast<void*>(cellBody_char);
}

void MemoryPool4::deallocate(void* p, size_t size) {
  unsigned char* p_char = reinterpret_cast<unsigned char*>(p);
  CellHeader* cellHeader = reinterpret_cast<CellHeader*>(p_char - CellHeaderSize);
  if (cellHeader->mGuard != VALID_CELL_HEADER_MARKER) {
    MY_LOGD("ERROR, cell guard not match");
    return;
  }
  ArenaHeader* arenaHeader = cellHeader->mpArena;
  if (!arenaHeader || arenaHeader->mGuard != VALID_ARENA_HEADER_MARKER) {
    MY_LOGD("ERROR, arena guard not match");
    return;
  }

  uint32_t cellFullSize = CellHeaderSize + arenaHeader->mCellBodySize;
  unsigned char* cellHeader_char = p_char - CellHeaderSize;
  uint32_t bitPosOfCell = static_cast<uint32_t>((cellHeader_char - arenaHeader->mCellStart) / cellFullSize);
  uint32_t bit = ~(1UL << bitPosOfCell);
  arenaHeader->mOccupationBits &= bit;
  {
    uint32_t occupyBit = arenaHeader->mOccupationBits.load(std::memory_order_acquire);
    MY_LOGI(" %d user(data=0x%p/size=%zu), cell[header_ptr=0x%p/id=%lu], occupy(0x%lX/num=%u)",
            getTid(), p, size, cellHeader_char, bitPosOfCell,
            occupyBit, arenaHeader->getNumOccupiedCells());
  }

#ifdef DEBUG_ENABLE
  assertm(cellHeader->mGuard == VALID_CELL_HEADER_MARKER, "cell guard is wrong");
  assertm(arenaHeader->mGuard == VALID_ARENA_HEADER_MARKER, "arena guard is wrong");
#endif  // DEBUG_ENABLE
}

std::unique_ptr<uint8_t[]> MemoryPool4::allocateArenaOfMemory(
    const AllocInfo& info, const ArenaCollection& collection) {
  size_t memSize = ArenaHeaderSize
                  + (CellHeaderSize + info.mCellBodySize) * MAX_CELLS_PER_ARENA;
  std::unique_ptr<uint8_t[]> memory = std::make_unique<uint8_t[]>(memSize);
  if (!memory) {
    MY_LOGD("ERROR, failed to make_unique");
    return nullptr;
  }
  unsigned char* p = reinterpret_cast<unsigned char*>(memory.get());
  {
    MY_LOGD("allocate arena of memory size: %zu+(%zu+%u)*%u=%zu "
            "arena addr:0x%p - 0x%p",
            ArenaHeaderSize, CellHeaderSize, info.mCellBodySize, MAX_CELLS_PER_ARENA,
            memSize, p, p + memSize);
  }
  // set arena header
  ArenaHeader* arenaHeader = reinterpret_cast<ArenaHeader*>(p);
  arenaHeader->mCellCapacity = info.mMaxCellCountPerArena;
  arenaHeader->mCellBodySize = info.mCellBodySize;
  arenaHeader->mOccupationBits = 0;
  arenaHeader->mpCollection = &collection;
  arenaHeader->mCellStart = p + ArenaHeaderSize;
  arenaHeader->mCellEnd = arenaHeader->mCellStart
                        + (CellHeaderSize + info.mCellBodySize) * arenaHeader->mCellCapacity
                        - 1;
  arenaHeader->mNextArena = nullptr;
  arenaHeader->mGuard = VALID_ARENA_HEADER_MARKER;

  // set cell
  for (size_t i = 0; i < arenaHeader->mCellCapacity; ++i) {
    unsigned char* cellRaw = arenaHeader->mCellStart
                           + (CellHeaderSize + info.mCellBodySize) * i;
    CellHeader* cellHeader = reinterpret_cast<CellHeader*>(cellRaw);
    cellHeader->mpArena = reinterpret_cast<ArenaHeader*>(p);
    cellHeader->mGuard = VALID_CELL_HEADER_MARKER;
  }
  return std::move(memory);
}

////////////////////////////////////////////////////////////

GlobalMemPool& GlobalMemPool::getInstance() {
  static GlobalMemPool gPool;
  return gPool;
}

GlobalMemPool::GlobalMemPool() {
  uint32_t cellBodySize = 8;
  uint32_t cellCountPerArena = 8;
  for (size_t i = 0; i < MAX_ARENA_COUNT; ++i) {
    mAllocInfo[i] = AllocInfo(cellBodySize, cellCountPerArena);
    cellBodySize <<= 1;
  }
}

GlobalMemPool::~GlobalMemPool() {
}

void* GlobalMemPool::allocate(size_t size) {
  if (size == 0) {
    MY_LOGD("zero size allocation is invalid");
    return nullptr;
  }
  uint32_t cellBodySize = 0;
  uint32_t arenaId = 0;
  cellBodySize = calcCellSizeAndArenaId(size, arenaId);
  return MemoryPool4::allocate(mAllocInfo[arenaId], mArenaCollections[arenaId]);
}

void GlobalMemPool::deallocate(void* data, size_t size) {
  MemoryPool4::deallocate(data, size);
}

uint32_t GlobalMemPool::calcCellSizeAndArenaId(
    size_t allocSize, uint32_t& arenaIdx) {
  if (allocSize < BYTE_ALIGNMENT) {
    allocSize = BYTE_ALIGNMENT;
  }
  uint32_t allocSize_mpow2 = static_cast<uint32_t>(allocSize);
  uint32_t cellBodySize = TO_POW2_UINT32(allocSize_mpow2);

  // arena index <-> cell size_wo_header =
  // 0/8, 1/16/ 2/32, ...
  // arenaIdx = log(size_wo_header) - log(8)
   arenaIdx = COUNT_NUM_TRAILING_ZEROES_UINT32(cellBodySize) -
              COUNT_NUM_TRAILING_ZEROES_UINT32(BYTE_ALIGNMENT);
   return cellBodySize;
}