#include "MemoryPool.h"

#include <cstring>
#include <memory>

#include "common.h"
#define TAG_LOG MemoryPool

// Memory allocation values
#define MAX_CELL_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* anything above this cell size is not put in an arena by the system. must be a power of 2! */
#define MAX_ARENA_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* we do not create arenas larger than this size (not including cell headers). the cell capacity of an arena will be reduced if needed so that it satisfies this restriction*/
#define MAX_CELLS_PER_ARENA 32 /* there can only be up to 64 cells in an arena. (but there can be less) */
#define OUTSIDE_SYSTEM_MARKER 0xBACBAA55
#define BYTE_ALIGNMENT 8
#define VALID_ARENA_HEADER_MARKER 0x0123ABCD0123ABCD
#define VALID_CELL_HEADER_MARKER 0xEEEEFFFFDDDD0123

#define COUNT_NUM_TRAILING_ZEROES_UINT32(bits) __builtin_ctz(bits)
#define COUNT_NUM_TRAILING_ZEROES_UINT64(bits) __builtin_ctz(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT32(bits) __builtin_clzl(bits)
#define COUNT_NUM_LEADING_ZEROES_UINT64(bits) __builtin_clzll(bits)

#define TO_POW2_UINT32(n)  \
  n == 1 ? 1 : 1 << (32 - COUNT_NUM_LEADING_ZEROES_UINT32(n-1));
#define TO_POW2_UINT64(n)  \
  n == 1 ? 1 : 1 << (64 - COUNT_NUM_LEADING_ZEROES_UINT64(n-1));


void* MemoryPool::allocate(size_t size) {
  if (size == 0) {
    MY_LOGD("zero size allocation is invalid");
    return nullptr;
  }
  GlobalState& gState = getGlobalState();
  unsigned int cellSize_wo_header = 0;
  unsigned int arenaIdx = 0;
  cellSize_wo_header = calcCellSizeAndArenaIndex(size, arenaIdx);

  ArenaCollection* arenaCollection = gState.mArenaCollections[arenaIdx];
  ArenaHeader* arena_header = nullptr;
  if (!arenaCollection) {
    arenaCollection = reinterpret_cast<ArenaCollection*>(
        calloc(1, sizeof(ArenaCollection)));
    arenaCollection->mCellSizeInBytes = cellSize_wo_header;
    gState.mArenaCollections[arenaIdx] = arenaCollection;
    arena_header = allocateArenaOfMemory(cellSize_wo_header, BYTE_ALIGNMENT, arenaCollection);
    arenaCollection->mFirst = arena_header;
  }
  arena_header = arenaCollection->mFirst;

  // now we're not ready to know if the arena is usable or not.
  while (arena_header->mNumOccupiedCells >= arena_header->mCellCapacity) {
    // all cells in the arena are occupied, allocate a new one.
    if (!arena_header->mNext) {
      arena_header->mNext = allocateArenaOfMemory(cellSize_wo_header, BYTE_ALIGNMENT, arenaCollection);
    }
    arena_header = arena_header->mNext;
  }

  // now an arena with unoccupied cells is found. find the cell and occupy it.
  unsigned int cell_index =
      static_cast<unsigned int>(COUNT_NUM_TRAILING_ZEROES_UINT64(~arena_header->mOccupationBits));
  arena_header->mNumOccupiedCells++;
  arena_header->mOccupationBits |= (1ULL << cell_index);

  unsigned char* ptr = arena_header->mArenaStart
                     + (cell_index * (sizeof(CellHeader) + arena_header->mCellSizeInBytes))
                     + sizeof(CellHeader);
  MY_LOGD("ptr=%p", ptr);
  print(*arena_header);
  return reinterpret_cast<void*>(ptr);
}

void MemoryPool::deallocate(void* data, size_t size) {
  if (data == nullptr) {
    MY_LOGD("null data is invalid");
    return;
  }
  // TODO() : identify if this is an outside system allocation.
  // free it if yse.
  MY_LOGD("deallocate 0x%p, size=%zu", data, size);
  MemoryPool::GlobalState& gState = getGlobalState();
  unsigned char* data_char = reinterpret_cast<unsigned char*>(data);
  CellHeader *cell_header = reinterpret_cast<CellHeader*>(data_char - sizeof(CellHeader));
  ArenaHeader *arena_header = nullptr;
  if (cell_header->mGuard == VALID_CELL_HEADER_MARKER) {
    arena_header = cell_header->mArena;
  }
  if (!arena_header || arena_header->mGuard != VALID_ARENA_HEADER_MARKER) {
    MY_LOGD("arena_heap = %p, guard=%llu", arena_header, arena_header->mGuard);
    return;
  }

  // here we have a valid arena header. and we could reset occupy state
  unsigned int cellSize_w_header = (sizeof(CellHeader) + arena_header->mCellSizeInBytes);
  unsigned char* cell_start = reinterpret_cast<unsigned char*>(data) - sizeof(CellHeader);
  unsigned int bit_position_for_cell = static_cast<unsigned int>((cell_start - arena_header->mArenaStart) / cellSize_w_header);
  unsigned long long bit = ~(1ULL << bit_position_for_cell);
  arena_header->mOccupationBits &= bit;
  arena_header->mNumOccupiedCells--;
  print(*arena_header);
}

void MemoryPool::shutdown() {
  MemoryPool::GlobalState& gState = getGlobalState();
  for (size_t i = 0; i < MAX_CELL_SIZE_POW2_BASE; ++i) {
    ArenaCollection* collection = gState.mArenaCollections[i];
    if (!collection) {
      continue;
    }
    ArenaHeader* arena_header = collection->mFirst;
    collection->mFirst = nullptr;
    while (arena_header) {
      if (arena_header->mNumOccupiedCells == 0) {
        // destroy the arena
        ArenaHeader* next_arena_header = arena_header->mNext;
        delete(arena_header);
        arena_header = next_arena_header;
      } else {
        // encounter some derelict memory. someone did not deallocate before
        // shutdown.
        MY_LOGD("meet derelict memory, check user");
      }
    }
    // ensure the terminating arena
  }
}

inline unsigned int MemoryPool::calcCellSizeAndArenaIndex(
    size_t alloc_size, unsigned int& arenaIdx) {
  if (alloc_size < BYTE_ALIGNMENT) {
    alloc_size = BYTE_ALIGNMENT;
  }

  uint32_t alloc_npow2 = static_cast<uint32_t>(alloc_size);
  uint32_t cellSize_wo_header = TO_POW2_UINT32(alloc_npow2);

  // arena index / cell size = 0/8, 1/16/ 2/32, ...
   arenaIdx = COUNT_NUM_TRAILING_ZEROES_UINT32(cellSize_wo_header) -
                 COUNT_NUM_TRAILING_ZEROES_UINT32(BYTE_ALIGNMENT);
   return cellSize_wo_header;
}

inline unsigned int
MemoryPool::calcCellCapacity(unsigned int cell_size,
                             unsigned int max_arena_size,
                             unsigned int max_cells_per_arena) {
  return std::min(std::max(max_arena_size / cell_size, 1u),
                  max_cells_per_arena);
}

inline MemoryPool::ArenaHeader*
MemoryPool::allocateArenaOfMemory(size_t cellSize_wo_header,
                                  size_t alignment_bytes,
                                  ArenaCollection* collection) {
  const size_t header_size = sizeof(ArenaHeader);
  // get how many cells allocated in a single arena

  unsigned int cellCapacity = calcCellCapacity(cellSize_wo_header, MAX_ARENA_SIZE, MAX_CELLS_PER_ARENA);
  // calculate total size of an arena and allocate memory
  size_t arena_size = header_size +
                      alignment_bytes +
                      ((sizeof(CellHeader) + cellSize_wo_header) * cellCapacity);
  unsigned char* raw_arena = reinterpret_cast<unsigned char*>(
      calloc(1, sizeof(arena_size)));
  MY_LOGD("allocate a raw_arena, addr=0x%p, size of arena=%zu "
          "(%zu+%zu+(%zu+%zu)*%u)",
          raw_arena, arena_size,
          header_size, alignment_bytes, sizeof(CellHeader), cellSize_wo_header, cellCapacity);
  // memset((void*)raw_arena, 0, header_size);
  // set Arena Header
  ArenaHeader *arena_header = reinterpret_cast<ArenaHeader*>(raw_arena);
  arena_header->mCollection = collection;
  arena_header->mCellCapacity = cellCapacity;
  arena_header->mCellSizeInBytes = static_cast<unsigned int>(cellSize_wo_header);

  // e.g we add alignment_bytes = 8 (1000), which is 0111 if we subtract 1, giving a mask.
  uint64_t padding_addr_mask = static_cast<uint64_t>(alignment_bytes - 1U);
  // figure out how many bytes past the arena_header, and we can start the arena in order to
  // be byte-aligned
  unsigned char* raw_arena_past_header = raw_arena + header_size;
  uint64_t masked_addr_past_header = (padding_addr_mask & reinterpret_cast<uint64_t>(raw_arena_past_header));
  arena_header->mPaddingSizeInBytes = static_cast<unsigned int>((alignment_bytes - masked_addr_past_header) % alignment_bytes);
  arena_header->mArenaStart = raw_arena
                            + header_size
                            + arena_header->mPaddingSizeInBytes;
  arena_header->mArenaEnd = arena_header->mArenaStart
                          + ((sizeof(CellHeader) + cellSize_wo_header)*cellCapacity)
                          - 1;
  arena_header->mGuard = VALID_ARENA_HEADER_MARKER;
  // set Cell Header
  for (uint_fast16_t i = 0; i < cellCapacity; ++i) {
    unsigned char* raw_cell_header = arena_header->mArenaStart
                                   + ((sizeof(CellHeader) + cellSize_wo_header) * i);
    CellHeader* cell_header = reinterpret_cast<CellHeader*>(raw_cell_header);
    cell_header->mArena = arena_header;
    cell_header->mGuard = VALID_CELL_HEADER_MARKER;
  }
  // set Arena Collection
  collection->mNumArenas++;
  return arena_header;
}

MemoryPool::GlobalState& MemoryPool::getGlobalState() {
  static GlobalState state;
  return state;
}

MemoryPool::GlobalState::GlobalState() {
}

MemoryPool::GlobalState::~GlobalState() {
}

// int main() {
//   MY_LOGD("============ programe start ============");
//   struct Test {
//     int a[10];
//   };

//   Test* p1 = static_cast<Test*>(MemoryPool::allocate(sizeof(Test)));
//   Test* p2 = static_cast<Test*>(MemoryPool::allocate(sizeof(Test)));

//   MY_LOGD("p1=%p", p1);
//   MY_LOGD("p2=%p", p2);

//   MemoryPool::deallocate(&(*p1), sizeof(Test));
// }