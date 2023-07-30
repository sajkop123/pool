#include "MemoryPool2.h"

#include <cstring>
#include <memory>
#include <cassert>

#include "common.h"
#define TAG_LOG MemoryPool2

// // Memory allocation values
// #define MAX_CELL_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* anything above this cell size is not put in an arena by the system. must be a power of 2! */
// #define MAX_ARENA_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* we do not create arenas larger than this size (not including cell headers). the cell capacity of an arena will be reduced if needed so that it satisfies this restriction*/
// #define MAX_CELLS_PER_ARENA 32 /* there can only be up to 64 cells in an arena. (but there can be less) */
// #define OUTSIDE_SYSTEM_MARKER 0xBACBAA55
// #define BYTE_ALIGNMENT 8
// #define VALID_ARENA_HEADER_MARKER 0x0123ABCD0123ABCD
// #define VALID_CELL_HEADER_MARKER 0xEEEEFFFFDDDD0123

#define COUNT_NUM_TRAILING_ZEROES_UINT32(bits) __builtin_ctz(bits)
#define COUNT_NUM_TRAILING_ZEROES_UINT64(bits) __builtin_ctz(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT32(bits) __builtin_clzl(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT64(bits) __builtin_clzll(bits)

#define TO_POW2_UINT32(n)  \
  n == 1 ? 1 : 1 << (32 - COUNT_NUM_LEADING_ZEROES_UINT32(n-1));
#define TO_POW2_UINT64(n)  \
  n == 1 ? 1 : 1 << (64 - COUNT_NUM_LEADING_ZEROES_UINT64(n-1));

#define assertm(exp, msg) assert(((void)msg, exp))

void* MemoryPool2::allocate(size_t size) {
  if (size == 0) {
    MY_LOGD("zero size allocation is not reasonable, return nullptr");
    return nullptr;
  }
  uint32_t cellSizeNoHeader = 0;
  uint32_t arenaIdx = 0;
  cellSizeNoHeader = callCellSizeAndArenaIdx(size, arenaIdx);
  MY_LOGD("user size=%zu, cellbodysize = %zu, arenaIndex=%u ",
          size, cellSizeNoHeader, arenaIdx);

  // create a ArenaCollection if need
  GlobalState& state = getGlobalState();
  auto& arenaCollection = state.mArenaCollections[arenaIdx];

  // allocate an arena of memory if there still no allocated memory.
  auto& arenaHeader = arenaCollection.mRootArena;
  if (!arenaHeader) {
    // std::unique_lock<std::mutex> _l(state.mMutex);
    arenaHeader = allocateArenaOfMemory(
        cellSizeNoHeader, BYTE_ALIGNMENT, &arenaCollection);
    arenaCollection.mCellSizeInBytes = cellSizeNoHeader;
    arenaCollection.mRootArena = arenaHeader;
  }

  // all cells inside the arena are occupied, allocate another one and use
  // link-list to make connection.
  while (arenaHeader->mNumOccupiedCells >= arenaHeader->mCellCapacity) {
    // std::unique_lock<std::mutex> _l(state.mMutex);
    if (!arenaHeader->mNextArena) {
      arenaHeader->mNextArena = allocateArenaOfMemory(
          cellSizeNoHeader, BYTE_ALIGNMENT, &arenaCollection);
    }
    arenaHeader = arenaHeader->mNextArena;
  }

  // an arena with unoccupied cells is found. find the cell and occupy it.
  uint32_t cellIndex =
      static_cast<uint32_t>(COUNT_NUM_TRAILING_ZEROES_UINT64(~arenaHeader->mOccupationBits));
  arenaHeader->mOccupationBits |= (1ULL << cellIndex);
  arenaHeader->mNumOccupiedCells++;
  // unsigned char* ptr =
  //     arenaHeader->mArenaStart +
  //     (cellIndex * (sizeof(CellHeader) + arenaHeader->mCellSizeInBytes)) +
  //     sizeof(CellHeader);

  unsigned char* cellHeader_char =
      arenaHeader->mArenaStart +
      (cellIndex * (sizeof(CellHeader) + arenaHeader->mCellSizeInBytes));
  CellHeader* cellHeader = reinterpret_cast<CellHeader*>(cellHeader_char);
  unsigned char* cellBody_char = cellHeader_char + sizeof(CellHeader);
  {
    MY_LOGD("return cell[H:p=0x%p/guard=%llX][B:p=0x%p/id:%u/size:%u], occupy(%llX/num=%u(%p)), arena.guard=%llX",
            cellHeader, cellHeader->mGuard,
            cellBody_char, cellIndex, arenaHeader->mCellSizeInBytes,
            arenaHeader->mOccupationBits, arenaHeader->mNumOccupiedCells,
            std::addressof(arenaHeader->mNumOccupiedCells),
            arenaHeader->mGuard);
    assertm(cellHeader->mGuard == VALID_CELL_HEADER_MARKER, "cell guard is wrong");
    assertm(arenaHeader->mGuard == VALID_ARENA_HEADER_MARKER, "arena guard is wrong");
  }
  return reinterpret_cast<void*>(cellBody_char);
}

void MemoryPool2::deallocate(void* data, size_t size) {
  if (data == nullptr) {
    MY_LOGD("ERROR, null data is invalid");
    return;
  }

  // check the cell and arena are both valid
  GlobalState& state = getGlobalState();
  unsigned char* data_char = reinterpret_cast<unsigned char*>(data);
  CellHeader* cellHeader =
      reinterpret_cast<CellHeader*>(data_char - sizeof(CellHeader));
  if (cellHeader->mGuard != VALID_CELL_HEADER_MARKER) {
    MY_LOGD("ERROR, cell guard not match");
    return;
  }
  ArenaHeader* arenaHeader = cellHeader->mpArena;
  if (!arenaHeader || arenaHeader->mGuard != VALID_ARENA_HEADER_MARKER) {
    MY_LOGD("ERROR, arena guard not match");
    return;
  }

  // reset occupy status of the cell
  uint32_t cellSizeWithHeader =
      sizeof(CellHeader) + arenaHeader->mCellSizeInBytes;
  unsigned char* cellStart = data_char - sizeof(CellHeader);
  uint32_t bitPosOfCell =
      static_cast<uint32_t>((cellStart - arenaHeader->mArenaStart) / cellSizeWithHeader);
  uint64_t bit = ~(1ULL << bitPosOfCell);
  arenaHeader->mOccupationBits &= bit;
  arenaHeader->mNumOccupiedCells--;
  MY_LOGD("user(data=0x%p/size=%zu), cell[header_ptr=0x%x], occupy(%llX/num=%u)",
          data, size, cellStart,
          arenaHeader->mOccupationBits, arenaHeader->mNumOccupiedCells);
}

void MemoryPool2::shutdown() {
  MY_LOGD("++++");
  GlobalState& state = getGlobalState();
  for (auto& collection : state.mArenaCollections) {
    ArenaHeader* arenaHeader = collection.mRootArena;
    while (arenaHeader) {
      ArenaHeader* temp = arenaHeader;
      arenaHeader = arenaHeader->mNextArena;
      size_t heapSize = temp->mArenaSizeInBytes;
      // ::operator delete(temp);
      temp = nullptr;
    }
  }
  MY_LOGD("----");
}
uint32_t MemoryPool2::callCellSizeAndArenaIdx(
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

MemoryPool2::ArenaHeader* MemoryPool2::allocateArenaOfMemory(
    size_t cellSizeNoHeader, size_t byte_alignment,
    ArenaCollection* collection) {
  size_t headerSize = sizeof(ArenaHeader);
  uint32_t cellCapacity = calcCellCapacity(cellSizeNoHeader, MAX_ARENA_SIZE,
                                           MAX_CELLS_PER_ARENA);
  // calculate total size of arena and allocate it.
  size_t arenaSize =
      headerSize + byte_alignment +
      ((sizeof(CellHeader) + cellSizeNoHeader) * cellCapacity);
  unsigned char* rawArena =
      reinterpret_cast<unsigned char*>(calloc(1, sizeof(arenaSize)));
  if (rawArena == nullptr) {
    MY_LOGD("ERROR: calloc failed");
  }
  MY_LOGD("allocate arena of memory size: %zu+%zu+(%zu+%zu)*%u=%u "
          "arena addr:0x%p - 0x%p",
          headerSize, byte_alignment, sizeof(CellHeader),
          cellSizeNoHeader, cellCapacity, arenaSize,
          rawArena, rawArena+arenaSize);
  // inplacement new ArenaHeader
  ArenaHeader* arenaHeader = reinterpret_cast<ArenaHeader*>(rawArena);
  arenaHeader->mpCollection = collection;
  arenaHeader->mCellCapacity = reinterpret_cast<uint32_t>(cellCapacity);
  arenaHeader->mCellSizeInBytes = static_cast<uint32_t>(cellSizeNoHeader);

  // calculate padding size
  // e.g we add alignment_bytes = 8 (1000), which is 0111 if we subtract 1, giving a mask.
  uint64_t paddingAddrMask = static_cast<uint64_t>(byte_alignment - 1U);
  // figure out how many bytes past the arenaHeader, and we can start the arena in order to
  // be byte-aligned
  unsigned char* rawArenaPastHeader = rawArena + headerSize;
  uint64_t maskedAddrPastHeader = (paddingAddrMask & reinterpret_cast<uint64_t>(rawArenaPastHeader));
  arenaHeader->mPaddingSizeInBytes =
      static_cast<unsigned int>((byte_alignment - maskedAddrPastHeader) % byte_alignment);
  arenaHeader->mOccupationBits = 0;
  arenaHeader->mNumOccupiedCells = 0;
  arenaHeader->mArenaSizeInBytes = arenaSize;
  arenaHeader->mArenaStart = rawArena + headerSize + arenaHeader->mPaddingSizeInBytes;
  arenaHeader->mArenaEnd =
      arenaHeader->mArenaStart +
      ((sizeof(CellHeader) + cellSizeNoHeader) * cellCapacity) - 1;
  arenaHeader->mNextArena = nullptr;
  arenaHeader->mGuard = VALID_ARENA_HEADER_MARKER;

  // set CellHeader
  for (uint32_t i = 0; i < cellCapacity; ++i) {
    unsigned char* rawCell =
        arenaHeader->mArenaStart + ((sizeof(CellHeader) + cellSizeNoHeader) * i);
    CellHeader* cellHeader = reinterpret_cast<CellHeader*>(rawCell);
    cellHeader->mpArena = arenaHeader;
    cellHeader->mGuard = VALID_CELL_HEADER_MARKER;
  }

  // update cellection
  collection->mNumArenas++;
  return arenaHeader;
}


inline uint32_t
MemoryPool2::calcCellCapacity(
    uint32_t cellSize, uint32_t maxArenaSize, uint32_t maxCellsPerArena) {
  return std::min(std::max(maxArenaSize / cellSize, 1u),
                  maxCellsPerArena);
}

MemoryPool2::GlobalState& MemoryPool2::getGlobalState() {
  static GlobalState state;
  return state;
}

MemoryPool2::GlobalState::~GlobalState() {
}