#include "system.h"

#include "linux_parser.h"

System::System() {
  this->operating_system_ = LinuxParser::OperatingSystem();
  this->kernel_ = LinuxParser::Kernel();
  this->processManager.UpdateProcesses();
}

std::string System::Kernel() { return this->kernel_; }

const MemData& System::MemoryUtilization() {
  return LinuxParser::MemoryUtilization();
}

std::string System::OperatingSystem() { return this->operating_system_; }

unsigned long long System::UpTime() {
  return LinuxParser::UpTime();
}

std::string System::LoadAverage() {
  return LinuxParser::LoadAverage();
}

const std::vector<struct CPUData>& System::totalCpuUtilization() {
  return LinuxParser::totalCpuUtilization();
}
