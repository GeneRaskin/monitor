#ifndef MONITOR_MEM_DATA_H
#define MONITOR_MEM_DATA_H

#include <cstdint>

struct MemData {
  uint64_t memTotal;
  uint64_t memFree;
  uint64_t memAvailable;
  uint64_t buffers;
  uint64_t cached;
  uint64_t swapCached;
  uint64_t sReclaimable;
  uint64_t shmem;
  uint64_t swapTotal;
  uint64_t swapFree;
};

struct ProcessMemUtilization {
  uint64_t virtual_mem;
  uint64_t resident_mem;
  uint64_t shared_mem;
};

#endif
