#include "common.h"

#include <string>
#include <sstream>

#define MAX_CELL_SIZE_POW2_BASE (23)
class MemoryPool {
 private:

  struct ArenaHeader;
  struct ArenaCollection;

  // hold a list of arena
  struct ArenaCollection {
    // all arenas in the collection have the same size cells
    unsigned int mCellSizeInBytes;
    // total number of arenas
    unsigned int mNumArenas;
    // pointer to the first arena in the link-list
    ArenaHeader* mFirst;
  };

  // A contiguous chunk of memory which contains individual cell of memory
  struct ArenaHeader {
    // a pointer to the collection the arena belong to.
    ArenaCollection *mCollection;
    unsigned int mCellCapacity;
    unsigned int mCellSizeInBytes;
    unsigned int mPaddingSizeInBytes;
    // 64bit value, each bit represents a cell of memory in the arena is marked
    // as occupied or not. The lower-ordered bit in this value represents cells
    // at the begining of the arena while the higher-ordered one represents the
    // cells at the end of the arena.
    unsigned long long mOccupationBits;
    // total number of occupied cells
    unsigned int mNumOccupiedCells;
    // point to the start of the data arena, the first cell.
    unsigned char* mArenaStart;
    // point to the last byte of the last cell in the arena.
    unsigned char* mArenaEnd;
    // the
    ArenaHeader* mNext;
    // just occupy 8bytes as a guard
    unsigned long long mGuard;
  };

  // header for a single cell of memory that is the samllest managed chuck,
  // which is the blocks used by application
  struct CellHeader {
    // point to the arena the cell belong to
    ArenaHeader* mArena;
    // just occupy 8bytes as a guard
    unsigned long long mGuard;
  };

  struct GlobalState {
    // global state constructor
    GlobalState();
    GlobalState(GlobalState const&) = delete;
    void operator= (GlobalState const&) = delete;
    ~GlobalState();
    // 1-d array of pointers where the where the first dimension represents arena size (powers of two doubling for every slot)
    // the contents of which is a pointer to an ArenaCollection which holds the head of a linked list of arenas. This array is fixed in size up to the maximum
    // number of arena sizes we support (controlled by MAX_CELL_SIZE). It is allocated at startup, and nullified.
    // It is a sparse array, so only indices which have had allocations will have a non-null ArenaCollection pointer.
    ArenaCollection* mArenaCollections[MAX_CELL_SIZE_POW2_BASE] = {nullptr};
  };

 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);

  static void shutdown();

 private:
  static GlobalState&
  getGlobalState();

  static unsigned int
  calcCellSizeAndArenaIndex(size_t alloc_size,
                            unsigned int& arena_index);
  static ArenaHeader*
  allocateArenaOfMemory(size_t cellSize_wo_header,
                        size_t alignment_bytes,
                        ArenaCollection* collection);
  static unsigned int
  calcCellCapacity(unsigned int cell_size,
                   unsigned int max_arena_size,
                   unsigned int max_cells_per_arena);

 private:
  static inline void print(const ArenaHeader& in) {
    MY_LOGD("mCollection:0x%p mCellCapacity:%u mCellSizeInBytes:%u "
            "mPaddingSizeInBytes:%u mOccupationBits:%llu(0x%llx) "
            "mNumOccupiedCells:%u mArenaStart:0x%p mArenaEnd:0x%p",
            in.mCollection, in.mCellCapacity, in.mCellSizeInBytes,
            in.mPaddingSizeInBytes, in.mOccupationBits, in.mOccupationBits,
            in.mNumOccupiedCells, in.mArenaStart, in.mArenaEnd);
  }
};
