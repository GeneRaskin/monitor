#include "process.h"

#include <vector>
#include <chrono>
#include <unistd.h>
#include "processor.h"

#include "linux_parser.h"
#include "globals.h"

Process::Process(pid_t pid) {

  this->pid_ = pid;
  this->user_ = LinuxParser::Uid(pid);
  this->_lastUpdate = std::chrono::steady_clock::now();
  this->_command = LinuxParser::Command(pid);
  if (this->_command.empty()) {
    this->_is_kernel_process = true;
  }

  std::vector<struct CPUDataWithHistory> totalCpuUtilizationValues =
      LinuxParser::totalCpuUtilization();
  this->_lastTotalSystemJiffies = totalCpuUtilizationValues[0].current.totaltime;

  _updateProcStatFileData();
  _updateProcStatusFileData();
  this->_lastActiveJiffies = this->_utime + this->_stime;
  this->_cpuUtilization = 0.0f;
}

int Process::Pid() { return this->pid_; }

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

float Process::CpuUtilization(bool allowUpdate) {
  if (allowUpdate)
    _isUpdateNeeded();
  return this->_cpuUtilization;
}

const std::string& Process::Command() {
  return this->_command;
}

const ProcessMemUtilization& Process::MemUtilization() {
  _isUpdateNeeded();
  return *_memUtilization;
}

std::string Process::User() { return this->user_; }

double Process::UpTime() {
  _isUpdateNeeded();
  return (double)(this->_utime + this->_stime) / sysconf(_SC_CLK_TCK);
}

bool Process::isKernelProcess() {
  return this->_is_kernel_process;
}

bool Process::_isUpdateNeeded() {
  auto now = std::chrono::steady_clock::now();
  if (now - _lastUpdate >= std::chrono::milliseconds(GLOBAL_REFRESH_RATE)) {
    _updateProcStatFileData();
    _updateProcStatusFileData();
    _updateCpuUtilization();
    _lastUpdate = now;
    return true;
  }
  return false;
}

void Process::_updateProcStatFileData() {
  struct LinuxParser::procStatFileData procStatFileData =
      LinuxParser::parseProcStatFilePid(this->pid_);

  this->_nice_value = procStatFileData.niceval;
  this->_priority_value = procStatFileData.priorityval;
  this->_state = procStatFileData.state;
  this->_utime = procStatFileData.utime;
  this->_stime = procStatFileData.stime;
}

void Process::_updateProcStatusFileData() {
  LinuxParser::procStatusFileData data = LinuxParser::parseProcStatusFilePid(pid_);
  this->_memUtilization = std::make_unique<ProcessMemUtilization>(
      data.memData);
  this->_numThreads = data.numThreads;
}

long Process::NiceValue() {
  _isUpdateNeeded();
  return this->_nice_value;
}

long Process::PriorityValue() {
  _isUpdateNeeded();
  return this->_priority_value;
}

char Process::State() {
  _isUpdateNeeded();
  return this->_state;
}

unsigned int Process::getNumThreads() {
  _isUpdateNeeded();
  return this->_numThreads;
}

void Process::_updateCpuUtilization() {
  unsigned long currActiveJiffies = this->_utime + this->_stime;
  unsigned long deltaActiveJiffies = currActiveJiffies - _lastActiveJiffies;

  // Retrieve and cache total CPU usage (global) every cache duration period
  const std::vector<struct CPUDataWithHistory>& currTotalCpuUtilizationValues =
      LinuxParser::totalCpuUtilization();  // Uses cached global data
  unsigned long currTotalCpuUtilization = currTotalCpuUtilizationValues[0].current.totaltime;
  int num_cpus = std::max(1, (int)currTotalCpuUtilizationValues.size() - 1);

  // Calculate CPU usage as a ratio of the process' delta to system delta
  float usage = static_cast<float>(deltaActiveJiffies) /
                (currTotalCpuUtilization - _lastTotalSystemJiffies) * num_cpus * 100.0;
  usage = CLAMP(usage, 0.0, 100.0 * num_cpus);

  // Update the last recorded values
  _lastTotalSystemJiffies = currTotalCpuUtilization;
  _lastActiveJiffies = currActiveJiffies;
  this->_cpuUtilization = usage;
}
