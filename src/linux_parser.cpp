#include "linux_parser.h"

#include <algorithm>
#include <fstream>

#include "cache.h"
#include "globals.h"
#include "mem_data.h"
#include <sstream>
#include <unordered_map>

// Define a macro to switch between modern and legacy code
#if __cplusplus >= 201703L  // C++17 and newer
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <dirent.h>
#include <cstring>
#endif

namespace LinuxParser {

const std::string kProcDirectory{"/proc/"};
const std::string kCmdlineFilename{"/cmdline"};
const std::string kStatusFilename{"/status"};
const std::string kStatFilename{"/stat"};
const std::string kUptimeFilename{"uptime"};
const std::string kMeminfoFilename{"/meminfo"};
const std::string kVersionFilename{"/version"};
const std::string kOSPath{"/etc/os-release"};
const std::string kPasswordPath{"/etc/passwd"};
const std::string kLoadAvgPath("loadavg");

static std::chrono::milliseconds cacheDuration =
    std::chrono::milliseconds(GLOBAL_REFRESH_RATE);

// Utility function to open a file and handle errors
std::ifstream OpenFileStream(const std::string& filepath) {
  std::ifstream filestream(filepath);
  if (!filestream.is_open()) {
    perror(("error while opening file " + filepath).c_str());
  }
  return filestream;
}

// Utility function to process a file line by line with a callback
template <typename Func>
void ProcessFileLines(std::ifstream& filestream,
                                   const std::string& filepath,
                                   Func&& processLine, bool processAllLines = false) {
  std::string line;
  while (std::getline(filestream, line)) {
    std::istringstream curr_line(line);
    bool stop = processLine(curr_line);
    if (!processAllLines && stop) {
      break;
    }
  }

  if (filestream.bad()) {
    perror(("error while reading file" + filepath).c_str());
  }
}

std::string OperatingSystem() {
  std::string line;
  std::string key;
  std::string value;

  std::ifstream filestream = OpenFileStream(kOSPath);

  ProcessFileLines(
      filestream, kOSPath, [&](std::istringstream& curr_line) -> bool {
        std::getline(curr_line, key, '=');
        if (key == "PRETTY_NAME") {
          std::getline(curr_line, value, '=');
          value.erase(std::remove(value.begin(), value.end(), '\"'),
                      value.end());
          return true;
        }
        return false;
      });

  return value;
}

std::string Kernel() {
  std::string os, version, kernel;
  std::string line;
  std::ifstream stream(kProcDirectory + kVersionFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    if (stream.bad()) {
      perror(("error while reading file " + kProcDirectory + kVersionFilename)
                 .c_str());
      return kernel;
    }
    std::istringstream linestream(line);
    linestream >> os >> version >> kernel;
  } else {
    perror(("error while opening file " + kProcDirectory + kVersionFilename)
               .c_str());
  }
  stream.close();
  return kernel;
}

std::vector<int> Pids() {
  std::vector<int> pids;

#if __cplusplus >= 201703L  // Use modern C++17 filesystem API
  const fs::path directory_path = "/proc";
  if (fs::exists(directory_path) && fs::is_directory(directory_path)) {
    for (const auto& entry : fs::directory_iterator(directory_path)) {
      if (entry.is_directory()) {
        std::string curr_file = entry.path().filename().string();
        if (std::all_of(curr_file.begin(), curr_file.end(), ::isdigit)) {
          pids.push_back(std::stoi(curr_file));
        }
      }
    }
  }
#else  // Use POSIX API for older systems
  const char* directory_path = "/proc";
  DIR* dir = opendir(directory_path);
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      // Check if the entry is a directory and the name is numeric
      if (entry->d_type == DT_DIR && std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name), ::isdigit)) {
        pids.push_back(std::stoi(entry->d_name));
      }
    }
    closedir(dir);
  }
#endif

  return pids;
}

const struct MemData& MemoryUtilization() {
  static Cache<struct MemData> memDataCache(cacheDuration);
  if (memDataCache.IsCacheValid()) {
    return memDataCache.GetValue();
  }

  struct MemData memData {};
  std::ifstream filestream = OpenFileStream(kProcDirectory + kMeminfoFilename);

  if (filestream.is_open()) {
    std::string line, key;
    uint64_t value;

    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);
      linestream >> key >> value;

      // Remove the trailing ':' from key
      key = key.substr(0, key.size() - 1);

      if (key == "MemTotal") {
        memData.memTotal = value;
      } else if (key == "MemFree") {
        memData.memFree = value;
      } else if (key == "Buffers") {
        memData.buffers = value;
      } else if (key == "MemAvailable") {
        memData.memAvailable = value;
      } else if (key == "Cached") {
        memData.cached = value;
      } else if (key == "SwapCached") {
        memData.swapCached = value;
      } else if (key == "SReclaimable") {
        memData.sReclaimable = value;
      } else if (key == "Shmem") {
        memData.shmem = value;
      } else if (key == "SwapTotal") {
        memData.swapTotal = value;
      } else if (key == "SwapFree") {
        memData.swapFree = value;
      }
    }
  }

  memDataCache.UpdateCache(memData);

  return memDataCache.GetValue();
}

struct procStatFileData parseProcStatFilePid(
    pid_t pid) {
  struct procStatFileData procStatFileData{};
  std::ifstream filestream =
      OpenFileStream(kProcDirectory + std::to_string(pid) + kStatFilename);

  if (filestream.is_open()) {
    std::string fileline;
    if (std::getline(filestream, fileline)) {
      std::istringstream linestream(fileline);

      int fieldIndex = 0;
      std::string field;

      while (linestream >> field) {
        fieldIndex++;
        if (fieldIndex == 3) {
          procStatFileData.state = field[0];
        } else if (fieldIndex == 14) {
          procStatFileData.utime = std::stoul(field);
        } else if (fieldIndex == 15) {
          procStatFileData.stime = std::stoul(field);
        } else if (fieldIndex == 18) {
          procStatFileData.priorityval = std::stol(field);
        } else if (fieldIndex == 19) {
          procStatFileData.niceval = std::stol(field);
        } else if (fieldIndex == 22) {
          procStatFileData.starttime = std::stoull(field);
        }
      }
    }

    if (filestream.bad()) {
      perror(("error while reading file" + kProcDirectory +
              std::to_string(pid) + kStatFilename)
                 .c_str());
    }
  }

  return procStatFileData;
}

unsigned long long UpTime() {
  static Cache<unsigned long long> uptimeCache(cacheDuration);

  if (uptimeCache.IsCacheValid()) {
    return uptimeCache.GetValue();
  }

  std::ifstream filestream = OpenFileStream(kProcDirectory + kUptimeFilename);
  unsigned long long uptime = 0;

  if (filestream.is_open()) {
    std::string fileline;
    if (std::getline(filestream, fileline)) {
      std::istringstream linestream(fileline);
      linestream >> uptime;
    }
    if (filestream.bad()) {
      perror(("error while reading file" + kProcDirectory + kUptimeFilename)
                 .c_str());
    }
  }

  uptimeCache.UpdateCache(uptime);
  return uptimeCache.GetValue();
}

unsigned int numProcessesRunning() {
  static Cache<unsigned int> numProcessesRunningCache(cacheDuration);

  if (numProcessesRunningCache.IsCacheValid()) {
    return numProcessesRunningCache.GetValue();
  }

  std::string statFilePath = "/proc/stat";
  std::ifstream filestream = OpenFileStream(statFilePath);

  if (filestream.is_open()) {
    std::string fileline;
    while (std::getline(filestream, fileline)) {
      std::istringstream linestream(fileline);
      std::string field;
      linestream >> field;

      if (field == "procs_running") {
        unsigned int numProcs;
        linestream >> numProcs;
        numProcessesRunningCache.UpdateCache(numProcs);
        break;
      }
    }

    if (filestream.bad()) {
      perror(("error while reading file" + statFilePath)
                 .c_str());
    }
  }

  return numProcessesRunningCache.GetValue();
}

const std::vector<struct CPUData>& totalCpuUtilization() {
  static Cache<std::vector<struct CPUData>> cpuCache(cacheDuration);

  // Check if cache is valid
  if (cpuCache.IsCacheValid()) {
    return cpuCache.GetValue();  // Return cached data if valid
  }

  std::string statFilePath = "/proc/stat";
  std::ifstream filestream(statFilePath);
  std::vector<struct CPUData> cpuUtilizationStats;
  int curr_core_idx = 0;
  const std::vector<struct CPUData>& cacheValue = cpuCache.GetValue();

  // Process each line in the /proc/stat file
  ProcessFileLines(
      filestream, statFilePath,
      [&](std::istringstream& curr_line) -> bool {
        std::string curr_field;
        curr_line >> curr_field;

        // Check for "cpu" or "cpuN" labels to parse CPU data
        if (curr_field.find("cpu") == 0) {
          struct CPUData cpuDataPerCore = {};
          if (!cacheValue.empty() && (ssize_t)cacheValue.size() > curr_core_idx) {
            const struct CPUData& prevData = cacheValue[curr_core_idx];
            cpuDataPerCore.prev_measurement =
                std::make_shared<struct CPUData>(prevData);
          }
          curr_core_idx++;

          // Parse each field from the line as an unsigned long
          curr_line >> cpuDataPerCore.usertime >> cpuDataPerCore.nicetime >>
              cpuDataPerCore.systemtime >> cpuDataPerCore.idletime >>
              cpuDataPerCore.iowaittime >> cpuDataPerCore.irqtime >>
              cpuDataPerCore.softirqtime >> cpuDataPerCore.stealtime >>
              cpuDataPerCore.guesttime >> cpuDataPerCore.guestnicetime;

          // Calculate total time
          cpuDataPerCore.usertime =
              cpuDataPerCore.usertime - cpuDataPerCore.guesttime;
          cpuDataPerCore.nicetime =
              cpuDataPerCore.nicetime - cpuDataPerCore.guestnicetime;
          uint64_t idlealltime =
              cpuDataPerCore.idletime + cpuDataPerCore.iowaittime;
          uint64_t systemalltime = cpuDataPerCore.systemtime +
                                   cpuDataPerCore.irqtime +
                                   cpuDataPerCore.softirqtime;
          uint64_t virtalltime =
              cpuDataPerCore.guesttime + cpuDataPerCore.guestnicetime;
          uint64_t totaltime = cpuDataPerCore.usertime +
                               cpuDataPerCore.nicetime + systemalltime +
                               idlealltime + cpuDataPerCore.stealtime +
                               virtalltime;
          cpuDataPerCore.totaltime = totaltime;

          // Add the parsed CPU data to the vector
          cpuUtilizationStats.emplace_back(cpuDataPerCore);
          return true;  // continue processing lines
        }
        return false;
      },
      true);

  // Update cache with new CPU data
  cpuCache.UpdateCache(cpuUtilizationStats);
  return cpuCache.GetValue();
}

std::string Command(pid_t pid) {
  std::string filepath =
      kProcDirectory + std::to_string(pid) + kCmdlineFilename;
  std::ifstream filestream = OpenFileStream(filepath);
  std::string cmd;

  std::getline(filestream, cmd);

  if (filestream.bad()) {
    perror(("error while reading file" + filepath).c_str());
  }

  filestream.close();
  return cmd;
}

procStatusFileData parseProcStatusFilePid(int pid) {
  struct ProcessMemUtilization processMemUtilization {};
  struct procStatusFileData procStatusFileData {};
  std::string filepath = kProcDirectory + std::to_string(pid) + kStatusFilename;
  auto filestream = OpenFileStream(filepath);

  if (filestream.is_open()) {
    std::string line, key;
    uint64_t value;

    while (std::getline(filestream, line)) {
      std::istringstream linestream(line);
      linestream >> key >> value;

      // Remove the trailing ':' from key
      key = key.substr(0, key.size() - 1);

      if (key == "VmSize") {
        processMemUtilization.virtual_mem = value;
      } else if (key == "VmRSS") {
        processMemUtilization.resident_mem = value;
      } else if (key == "RssShmem") {
        processMemUtilization.shared_mem = value;
      } else if (key == "Threads") {
        procStatusFileData.numThreads = value;
      }
    }
    if (filestream.bad()) {
      perror(("error while reading file" + filepath).c_str());
    }
  }
  procStatusFileData.memData = processMemUtilization;

  return procStatusFileData;
}

std::string Uid(pid_t pid) {
  std::string filepath = kProcDirectory + std::to_string(pid) + kStatusFilename;
  std::ifstream filestream = OpenFileStream(filepath);

  // lambda to process each line and extract UID
  uid_t user_uid;
  ProcessFileLines(filestream, filepath,
                   [&](std::istringstream& curr_line) -> bool {
                     std::string key, value;
                     curr_line >> key;
                     if (key == "Uid:") {
                       curr_line >> value;
                       user_uid = stoi(value);
                       return true;
                     }
                     return false;
                   });

  filestream.close();
  filestream = OpenFileStream(kPasswordPath);

  static std::unordered_map<uid_t, std::string> mapOfUIDs;
  if (mapOfUIDs.find(user_uid) != mapOfUIDs.end()) {
    return mapOfUIDs[user_uid];
  }

  std::string user;
  ProcessFileLines(filestream, kPasswordPath,
                   [&](std::istringstream& curr_line) -> bool {
                     std::string uid_in_file, passwd;
                     std::getline(curr_line, user, ':');
                     std::getline(curr_line, passwd, ':');
                     std::getline(curr_line, uid_in_file, ':');
                     if (stoi(uid_in_file) == static_cast<int>(user_uid)) {
                       return true;
                     }
                     return false;
                   });

  mapOfUIDs.emplace(user_uid, user);

  filestream.close();
  return mapOfUIDs[user_uid];
}

std::string LoadAverage() {
  static Cache<std::string> loadAvgCache(cacheDuration);
  if (loadAvgCache.IsCacheValid()) {
    return loadAvgCache.GetValue();
  }

  std::ifstream filestream = OpenFileStream(kProcDirectory + kLoadAvgPath);
  std::string loadAvg;

  if (filestream.is_open()) {
    std::string fileline;
    if (std::getline(filestream, fileline)) {
      std::istringstream linestream(fileline);
      std::string field1, field2, field3;
      linestream >> field1 >> field2 >> field3;
      loadAvg = field1 + " " + field2 + " " + field3;
    }
    if (filestream.bad()) {
      perror(("error while reading file" + kProcDirectory + kUptimeFilename)
                 .c_str());
    }
  }

  loadAvgCache.UpdateCache(loadAvg);
  return loadAvgCache.GetValue();
}

}
