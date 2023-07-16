#include <string>
#include <sstream>

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
    // the number of different sizes of arenas we have
    unsigned int mNumsArenaCollections;
    // 1-d array of pointers where the where the first dimension represents arena size (powers of two doubling for every slot)
    // the contents of which is a pointer to an ArenaCollection which holds the head of a linked list of arenas. This array is fixed in size up to the maximum
    // number of arena sizes we support (controlled by MAX_CELL_SIZE). It is allocated at startup, and nullified.
    // It is a sparse array, so only indices which have had allocations will have a non-null ArenaCollection pointer.
    ArenaCollection** mArenaCollections;
  };

 public:
  static void* allocate(size_t size);
  static void deallocate(void* data, size_t size);

 private:
  static unsigned int
  calcCellSizeAndArenaIndex(size_t alloc_size,
                            unsigned int& arena_index);
  static ArenaHeader*
  allocateArenaOfMemory(size_t cellSize_wo_header,
                        size_t alignment_bytes,
                        ArenaCollection* collection);
  static GlobalState& getGlobalState();

 private:
  inline std::string
  toString(const MemoryPool::ArenaCollection& in) const {
    std::ostringstream os;
    os << "mCellSizeInBytes:" << in.mCellSizeInBytes << " "
       << "mNumArenas:" << in.mNumArenas << " "
       << "mFirst:" << in.mFirst;
    return os.str();
  }
  static inline std::string
  toString(const MemoryPool::ArenaHeader& in) {
    std::ostringstream os;
    os << "mCollection:" << in.mCollection << " "
       << "mCellCapacity:" << in.mCellCapacity << " "
       << "mCellSizeInBytes:" << in.mCellSizeInBytes << " "
       << "mOccupationBits:" << std::hex << in.mCellCapacity << " "
       << "mNumOccupiedCells:" << in.mNumOccupiedCells << " "
       << "mArenaStart:" << in.mArenaStart << " "
       << "mArenaEnd:" << in.mArenaEnd;
    return os.str();
  }
};
