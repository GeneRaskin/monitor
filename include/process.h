#ifndef PROCESS_H
#define PROCESS_H

#include <sched.h>

#include <string>
#include <chrono>
#include <memory>

/*
Basic class for Process representation
It contains relevant attributes as shown below
*/

struct ProcessMemUtilization;

class Process {
public:
  pid_t Pid();
  std::string User();
  const std::string& Command();
  float CpuUtilization(bool allowUpdate = true);
  const ProcessMemUtilization& MemUtilization();
  long NiceValue();
  long PriorityValue();
  char State();
  double UpTime();
  unsigned int getNumThreads();

  explicit Process(pid_t pid);
  bool isKernelProcess();

private:
  pid_t pid_;
  std::string user_;
  std::string _command;
  unsigned long _lastActiveJiffies;
  unsigned long _lastTotalSystemJiffies;
  float _cpuUtilization;
  std::chrono::time_point<std::chrono::steady_clock>
      _lastUpdate;
  std::unique_ptr<ProcessMemUtilization> _memUtilization;
  bool _is_kernel_process = false;
  long _nice_value;
  long _priority_value;
  bool _isUpdateNeeded();
  void _updateProcStatFileData();
  char _state;
  unsigned long _utime;
  unsigned long _stime;
  unsigned int _numThreads;
  void _updateProcStatusFileData();
  void _updateCpuUtilization();
};

#endif