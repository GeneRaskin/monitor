#ifndef SYSTEM_PARSER_H
#define SYSTEM_PARSER_H

#include <chrono>
#include <string>
#include <vector>

#include "mem_data.h"
#include "processor.h"

namespace LinuxParser {

unsigned long long UpTime();
std::vector<int> Pids();
std::string OperatingSystem();
std::string Kernel();

struct procStatFileData {
  unsigned long utime;
  unsigned long stime;
  long niceval;
  long priorityval;
  char state;
  unsigned long long starttime;
};

struct procStatusFileData {
  struct ProcessMemUtilization memData;
  unsigned int numThreads;
};

const std::vector<struct CPUData>& totalCpuUtilization();
const struct MemData& MemoryUtilization();
std::string LoadAverage();
unsigned int numProcessesRunning();

// Processes
std::string Command(pid_t pid);
procStatusFileData parseProcStatusFilePid(int pid);
std::string Uid(pid_t pid);
struct procStatFileData parseProcStatFilePid(pid_t pid);

}  // namespace LinuxParser

#endif