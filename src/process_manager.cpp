#include "process_manager.h"

#include <algorithm>

#include "globals.h"
#include "linux_parser.h"
#include "process.h"

// Create a new process if not found in the existing map
void ProcessManager::AddProcessIfNotExists(pid_t pid) {

  auto it = processMap_.find(pid);
  if (it == processMap_.end()) {
    // Add new process to map
    std::shared_ptr<Process> newProcess = std::make_shared<Process>(pid);
    if (!newProcess->isKernelProcess())
      processMap_.emplace(pid, newProcess);
  }
}

// Remove stale processes not found in the current `/proc` scan
void ProcessManager::CleanupStaleProcesses(const std::vector<int>& currentPids) {
  std::unordered_map<pid_t, std::shared_ptr<Process>> updatedMap;

  for (pid_t pid : currentPids) {
    if (processMap_.find(pid) != processMap_.end()) {
      // Retain only active processes
      updatedMap.emplace(pid, processMap_.find(pid)->second);
    }
  }

  processMap_ = std::move(updatedMap);
}

// Update all processes by iterating through available PIDs
void ProcessManager::UpdateProcesses() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - lastUpdateTime_).count();

  if (elapsed < GLOBAL_REFRESH_RATE) {
    // Skip the update if less than 1500 ms have passed
    return;
  }
  // this should contain all PIDs currently in /proc
  std::vector<pid_t> currentPids = LinuxParser::Pids();

  // Update or add all current processes
  for (pid_t pid : currentPids) {
    AddProcessIfNotExists(pid);
  }

  // Clean up stale processes not in `currentPids`
  CleanupStaleProcesses(currentPids);
  _numOfTasks = processMap_.size();
  _updateNumOfThreads();
  lastUpdateTime_ = now;
}

std::vector<std::shared_ptr<Process>> ProcessManager::
    GetSortedProcessesForDisplay() {
  std::vector<std::shared_ptr<Process>> sortedProcesses;
  /*for (auto& [pid, process] : processMap_) {
    sortedProcesses.push_back(process);
  }*/

  for (const auto &it : processMap_) {
    sortedProcesses.push_back(it.second);
  }

  std::sort(sortedProcesses.begin(), sortedProcesses.end(),
            [] (const std::shared_ptr<Process>& l, const std::shared_ptr<Process>& r) {
              if (std::abs(l->CpuUtilization() - r->CpuUtilization()) > 1e-3)
                return l->CpuUtilization() > r->CpuUtilization();
              return l->Pid() < r->Pid();
            });
  return sortedProcesses;
}

ProcessManager::ProcessManager() {
  UpdateProcesses();
}

unsigned int ProcessManager::getNumOfTasks() {
  return _numOfTasks;
}

void ProcessManager::_updateNumOfThreads() {
  this->_numOfThreads = 0;
  this->_numOfRunningTasks = 0;
  for (const auto &curr_process : processMap_) {
    this->_numOfThreads += curr_process.second->getNumThreads();
    /*if (curr_process.second->State() == 'R')
      this->_numOfRunningTasks++;*/
  }
  this->_numOfRunningTasks = LinuxParser::numProcessesRunning();
}

unsigned int ProcessManager::getNumOfThreads() {
  return _numOfThreads;
}

unsigned int ProcessManager::getNumOfRunningTasks() {
  return this->_numOfRunningTasks;
}
