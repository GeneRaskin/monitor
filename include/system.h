#ifndef SYSTEM_H
#define SYSTEM_H

#include <string>

#include "mem_data.h"
#include "process_manager.h"

class System {
 public:
  static const struct MemData& MemoryUtilization();
  static unsigned long long UpTime();
  static std::string LoadAverage();
  static const std::vector<struct CPUData>& totalCpuUtilization();
  std::string Kernel();
  std::string OperatingSystem();
  ProcessManager processManager;

  System();

 private:
  std::string operating_system_;
  std::string kernel_;
  std::unique_ptr<MemData> memData_;
};

#endif