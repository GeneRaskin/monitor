#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdint>
#include <memory>

struct CPUData {
  uint64_t usertime;
  uint64_t nicetime;
  uint64_t systemtime;
  uint64_t idletime;
  uint64_t iowaittime;
  uint64_t irqtime;
  uint64_t softirqtime;
  uint64_t stealtime;
  uint64_t guesttime;
  uint64_t guestnicetime;
  uint64_t totaltime;
  std::shared_ptr<struct CPUData> prev_measurement = nullptr;
};

#endif