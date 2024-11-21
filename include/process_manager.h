#ifndef MONITOR_PROCESS_MANAGER_H
#define MONITOR_PROCESS_MANAGER_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>

class Process;

// Process manager for efficient parsing
class ProcessManager {
 public:
  void UpdateProcesses();
  ProcessManager();
  std::vector<std::shared_ptr<Process>> GetSortedProcessesForDisplay();

  unsigned int getNumOfTasks();
  unsigned int getNumOfThreads();
  unsigned int getNumOfRunningTasks();

 private:
  void AddProcessIfNotExists(pid_t pid);
  void CleanupStaleProcesses(const std::vector<pid_t>& currentPids);

  std::unordered_map<pid_t, std::shared_ptr<Process>> processMap_;    // Store process data by PID
  unsigned int _numOfTasks;
  unsigned int _numOfThreads;
  unsigned int _numOfRunningTasks;
  void _updateNumOfThreads();

  std::chrono::steady_clock::time_point lastUpdateTime_;
};

#endif
