#include "MemoryPool3.h"

#include <cstring>
#include <memory>
#include <cassert>

#include "common.h"
#define TAG_LOG MemoryPool3

// // Memory allocation values
// #define MAX_CELL_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* anything above this cell size is not put in an arena by the system. must be a power of 2! */
// #define MAX_ARENA_SIZE (1 << MAX_CELL_SIZE_POW2_BASE) /* we do not create arenas larger than this size (not including cell headers). the cell capacity of an arena will be reduced if needed so that it satisfies this restriction*/
// #define MAX_CELLS_PER_ARENA 32 /* there can only be up to 64 cells in an arena. (but there can be less) */
// #define OUTSIDE_SYSTEM_MARKER 0xBACBAA55
// #define BYTE_ALIGNMENT 8
// #define VALID_ARENA_HEADER_MARKER 0x0123ABCD0123ABCD
// #define VALID_CELL_HEADER_MARKER 0xEEEEFFFFDDDD0123


void* MemoryPool3::allocate(size_t size) {
  if (size == 0) {
    MY_LOGD("zero size allocation is invalid");
    return nullptr;
  }

  GlobalState& state = getGlobalState();
  // std::unique_lock<std::mutex> _l(state.sMutex);
  auto &dataInfo = state.map[size];
  if (!dataInfo.mMemory) {
    size_t mem_size = static_cast<size_t>(dataInfo.mCellSizeInBytes * CELL_NUMS);
    // set data info
    dataInfo.mCellSizeInBytes = size;
    dataInfo.mOccupationBits = 0;
    dataInfo.mNumOccupiedCells = 0;
    dataInfo.mMemory = std::make_unique<uint8_t[]>(mem_size);
    MY_LOGD("memory:cellsize=%zu, 0x%p - 0x%p",
            dataInfo.mCellSizeInBytes,
            dataInfo.mMemory.get(), dataInfo.mMemory.get() + mem_size);
  }

  uint32_t cellIndex =
      static_cast<uint32_t>(COUNT_NUM_TRAILING_ZEROES_UINT64(~dataInfo.mOccupationBits));
  dataInfo.mOccupationBits |= (1ULL << cellIndex);
  dataInfo.mNumOccupiedCells++;

  unsigned char* p = dataInfo.mMemory.get();
  p = p + cellIndex * dataInfo.mCellSizeInBytes;
  MY_LOGD("cellidx=%u p=0x%p, size=%zu occupy(%llX/num=%u)",
          cellIndex, p, dataInfo.mCellSizeInBytes,
          dataInfo.mOccupationBits, dataInfo.mNumOccupiedCells);
  return reinterpret_cast<void*>(p);
}

void MemoryPool3::deallocate(void* p, size_t size) {
  GlobalState& state = getGlobalState();
  // std::unique_lock<std::mutex> _l(state.sMutex);
  auto &dataInfo = state.map[size];
  if (!dataInfo.mMemory) {
    MY_LOGD("ERROR: not owned memory");
  }
  unsigned char* pCell_char = reinterpret_cast<unsigned char*>(p);
  unsigned char* pMem_char = dataInfo.mMemory.get();
  uint32_t bitPosOfCell = static_cast<uint32_t>(pCell_char - pMem_char) /
                          dataInfo.mCellSizeInBytes;
  uint32_t bit = ~(1ULL << bitPosOfCell);
  dataInfo.mOccupationBits &= bit;
  dataInfo.mNumOccupiedCells--;
  MY_LOGD("cellIndex=%u occupy(%llX/num=%u)",
          bitPosOfCell, dataInfo.mOccupationBits, dataInfo.mNumOccupiedCells);
}

MemoryPool3::GlobalState& MemoryPool3::getGlobalState() {
  static GlobalState state;
  return state;
}