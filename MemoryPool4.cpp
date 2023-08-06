#include "MemoryPool4.h"

#include <cstring>
#include <memory>
#include <cassert>

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

#define assertm(exp, msg) assert(((void)msg, exp))

void* MemoryPool4::allocate(size_t size) {
  if (size == 0) {
    MY_LOGD("zero size allocation is not reasonable, return nullptr");
    return nullptr;
  }
  uint32_t cellBodySize = 0;
  uint32_t arenaIdx = 0;
  cellBodySize = callCellSizeAndArenaIdx(size, arenaIdx);

  GlobalState& state = getGlobalState();
  auto& collection = state.mArenaCollections[arenaIdx];
  // FIX(BUG?) may have race cond here. need refer mOccupyBits
  if (!collection.mRootArena) {
    std::unique_lock<std::mutex> _l(collection.mMutex);
    collection.mRootArena = allocateArenaOfMemory(cellBodySize, &collection);
    collection.mCellBodySize = cellBodySize;
    collection.mNumArenas++;
  }

  // all cells inside the arena are occupied, allocate another one and use
  // link-list to make connection.
  ArenaHeader* arenaHeader = getArenaHeader(collection.mRootArena);
  uint32_t cellIdx = INVALID_CELL_INDEX;
  uint32_t oldOccupyBit = INVALID_OCCUPY_BIT;
  uint32_t newOccupyBit = INVALID_OCCUPY_BIT;
  do {
    oldOccupyBit = arenaHeader->mOccupationBits.load(std::memory_order_acquire);

    while (oldOccupyBit == ALL_CELL_IN_BIT) {
      std::unique_lock<std::mutex> _l(collection.mMutex);
      if (!arenaHeader->mNextArena) {
        arenaHeader->mNextArena = allocateArenaOfMemory(cellBodySize, &collection);
      }
      arenaHeader = getArenaHeader(arenaHeader->mNextArena);
      oldOccupyBit = 0;
    }

    cellIdx = COUNT_NUM_TRAILING_ZEROES_UINT32(~oldOccupyBit);
    newOccupyBit = oldOccupyBit | (1UL << cellIdx);
    {
      assertm(cellIdx != INVALID_CELL_INDEX, "invalid cell index");
      assertm(oldOccupyBit | INVALID_OCCUPY_BIT, "invalid old occupy bit");
      assertm(newOccupyBit | INVALID_OCCUPY_BIT, "invalid old occupy bit");
    }
  } while (!arenaHeader->mOccupationBits.compare_exchange_weak(
            oldOccupyBit, newOccupyBit, std::memory_order_release));

  // now we get a valid cell index
  unsigned char* cellHeader_char =
      arenaHeader->mCellStart +
      (CellHeaderSize + arenaHeader->mCellBodySize) * cellIdx;
  CellHeader* cellHeader = reinterpret_cast<CellHeader*>(cellHeader_char);
  unsigned char* cellBody_char = cellHeader_char + CellHeaderSize;
  {
    MY_LOGD("return cell[idx=%u/size=%u][H:p=0x%p/guard=%llX][B:p=0x%p] "
            "occupy(%lX/num=%u), arena.guard=%llX",
            cellIdx, arenaHeader->mCellBodySize, cellHeader, cellHeader->mGuard, cellBody_char,
            newOccupyBit, arenaHeader->getNumOccupiedCells(),
            arenaHeader->mGuard);
    assertm(cellHeader->mGuard == VALID_CELL_HEADER_MARKER, "cell guard is wrong");
    assertm(arenaHeader->mGuard == VALID_ARENA_HEADER_MARKER, "arena guard is wrong");
  }
  return reinterpret_cast<void*>(cellBody_char);
}

void MemoryPool4::deallocate(void* p, size_t size) {
  if (p == nullptr) {
    MY_LOGD("ERROR, null data is invalid");
    return;
  }

  // check the cell and arena are both valid
  GlobalState& state = getGlobalState();
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
    MY_LOGD("user(data=0x%p/size=%zu), cell[header_ptr=0x%x], occupy(%lX/num=%u)",
            p, size, cellHeader_char,
            occupyBit, arenaHeader->getNumOccupiedCells());
  }
}

void MemoryPool4::shutdown() {
}

uint32_t MemoryPool4::callCellSizeAndArenaIdx(
    size_t allocSize, uint32_t& arenaIdx) {
  if (allocSize < BYTE_ALIGNMENT) {
    allocSize = BYTE_ALIGNMENT;
  }
  uint32_t alloc_npow2 = static_cast<uint32_t>(allocSize);
  uint32_t cellSizeNoHeader = TO_POW2_UINT32(alloc_npow2);

  // arena index <-> cell size_wo_header =
  // 0/8, 1/16/ 2/32, ...
  // arenaIdx = log(size_wo_header) - log(8)
   arenaIdx = COUNT_NUM_TRAILING_ZEROES_UINT32(cellSizeNoHeader) -
              COUNT_NUM_TRAILING_ZEROES_UINT32(BYTE_ALIGNMENT);
   return cellSizeNoHeader;
}

std::unique_ptr<uint8_t[]> MemoryPool4::allocateArenaOfMemory(
    uint32_t& cellBodySize, ArenaCollection* collection) {
  if (cellBodySize < BYTE_ALIGNMENT) {
    cellBodySize = BYTE_ALIGNMENT;
  }
  size_t memSize = ArenaHeaderSize
                  + (CellHeaderSize + cellBodySize) * MAX_CELLS_PER_ARENA;
  std::unique_ptr<uint8_t[]> memory = std::make_unique<uint8_t[]>(memSize);
  if (!memory) {
    MY_LOGD("ERROR, failed to make_unique");
    return nullptr;
  }
  unsigned char* p = reinterpret_cast<unsigned char*>(memory.get());
  {
    MY_LOGD("allocate arena of memory size: %zu+(%zu+%u)*%u=%zu "
            "arena addr:0x%p - 0x%p",
            ArenaHeaderSize, CellHeaderSize, cellBodySize, MAX_CELLS_PER_ARENA,
            memSize, p, p + memSize);
  }
  // set arena header
  ArenaHeader* arenaHeader = reinterpret_cast<ArenaHeader*>(p);
  arenaHeader->mCellCapacity = MAX_CELLS_PER_ARENA;
  arenaHeader->mCellBodySize = cellBodySize;
  arenaHeader->mOccupationBits = 0;
  arenaHeader->mpCollection = collection;
  arenaHeader->mCellStart = p + ArenaHeaderSize;
  arenaHeader->mCellEnd = arenaHeader->mCellStart
                        + (CellHeaderSize + cellBodySize) * arenaHeader->mCellCapacity
                        - 1;
  arenaHeader->mNextArena = nullptr;
  arenaHeader->mGuard = VALID_ARENA_HEADER_MARKER;

  // set cell
  for (size_t i = 0; i < arenaHeader->mCellCapacity; ++i) {
    unsigned char* cellRaw = arenaHeader->mCellStart
                           + (CellHeaderSize + cellBodySize) * i;
    CellHeader* cellHeader = reinterpret_cast<CellHeader*>(cellRaw);
    cellHeader->mpArena = reinterpret_cast<ArenaHeader*>(p);
    cellHeader->mGuard = VALID_CELL_HEADER_MARKER;
  }
  return std::move(memory);
}

MemoryPool4::GlobalState& MemoryPool4::getGlobalState() {
  static GlobalState state;
  return state;
}

MemoryPool4::GlobalState::~GlobalState() {
}