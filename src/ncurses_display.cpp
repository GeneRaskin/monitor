#include "ncurses_display.h"

#include <cmath>
#include <thread>
#include <ncurses.h>
#include "format.h"
#include <mutex>
#include <csignal>
#include <sstream>
#include "system.h"
#include "process.h"
#include "globals.h"
#include "processor.h"
#include "event_queue.h"
#include <unistd.h>

#define PID_INDEX        0
#define USER_INDEX       1
#define PRI_INDEX        2
#define NI_INDEX         3
#define VIRT_INDEX       4
#define RES_INDEX        5
#define SHR_INDEX        6
#define S_INDEX          7
#define CPU_INDEX        8
#define MEM_INDEX        9
#define TIME_INDEX       10
#define COMMAND_INDEX    11

#define UPPER_PANEL_HEIGHT 10
#define MIN_UPPER_PANEL_BAR_WIDTH 6
#define PADDING_BETWEEN_BARS 2
#define UPPER_PANEL_LEFT_PADDING 5
#define UPPER_PANEL_RIGHT_PADDING 5
#define UPPER_PANEL_UP_PADDING 1
#define LOWER_PANEL_WIDTH 2
#define UPPER_PANEL_BARS_PER_COLUMN 4
#define UPPER_PANEL_SPACING_BETWEEN_COLUMNS 1

namespace NCursesDisplay {

enum class ColorPairs {
  black_green_pair = 1,
  green_black_pair,
  yellow_black_pair,
  red_black_pair,
  cyan_black_pair,
  white_black_pair,
  blue_black_pair,
  black_cyan_pair
};

class Bar {
private:
  int _startX;
  int _startY;
  int _barLength;
  std::string _leftLabel;
  std::string _rightLabel;
  std::vector<ColorPairs> _colorPairs;
  std::vector<float> _ratios;
  ColorPairs _bracketsColorPair;
  ColorPairs _rightLabelColorPair;
  ColorPairs _leftLabelColorPair;
  WINDOW *_owningPanel;
public:
  void drawBar() {

    wattron(_owningPanel, COLOR_PAIR(_leftLabelColorPair));
    mvwprintw(_owningPanel, _startY, _startX, "%s", _leftLabel.c_str());
    wattroff(_owningPanel, COLOR_PAIR(_leftLabelColorPair));

    wattron(_owningPanel, COLOR_PAIR(_bracketsColorPair) | A_BOLD);
    mvwprintw(_owningPanel, _startY, _startX + _leftLabel.size(), "[");
    wattroff(_owningPanel, COLOR_PAIR(_bracketsColorPair) | A_BOLD);

    int accumulatedLenOfBars = 0;

    for (int i = 0; i < (ssize_t)_colorPairs.size(); ++i) {
      ColorPairs currColorPair = _colorPairs[i];
      int cols_in_bar_to_fill =
          ((float)_barLength - 2 - _rightLabel.size() - _leftLabel.size()) *
          _ratios[i];

      wattron(_owningPanel, COLOR_PAIR(currColorPair));

      // Draw the bar for this ratio
      for (int col = 0; col < cols_in_bar_to_fill; ++col) {
        mvwaddch(_owningPanel, _startY, _startX + accumulatedLenOfBars + 1
                                            + _leftLabel.size() + col, '|');
      }

      accumulatedLenOfBars += cols_in_bar_to_fill;
      wattroff(_owningPanel, COLOR_PAIR(currColorPair));

    }

    wattron(_owningPanel, COLOR_PAIR(_rightLabelColorPair) | A_BOLD);
    mvwprintw(_owningPanel, _startY,
              _startX + _barLength - 1 - _rightLabel.size(), "%s",
              _rightLabel.c_str());
    wattroff(_owningPanel, COLOR_PAIR(_rightLabelColorPair));

    wattron(_owningPanel, COLOR_PAIR(_bracketsColorPair));
    mvwprintw(_owningPanel, _startY, _startX + _barLength - 1, "]");
    wattroff(_owningPanel, COLOR_PAIR(_bracketsColorPair) | A_BOLD);

  }

  Bar(int startX, int startY, int barLength, std::string& leftLabel,
      std::string& rightLabel, std::vector<ColorPairs>& colorPairs,
      std::vector<float>& ratios, WINDOW *owningPanel,
      ColorPairs bracketsColorPair = ColorPairs::white_black_pair,
      ColorPairs leftLabelColorPair = ColorPairs::cyan_black_pair,
      ColorPairs rightLabelColorPair = ColorPairs::white_black_pair) :
        _startX(startX), _startY(startY),
        _barLength(barLength), _leftLabel(std::move(leftLabel)),
        _rightLabel(std::move(rightLabel)), _bracketsColorPair(bracketsColorPair),
        _rightLabelColorPair(rightLabelColorPair), _leftLabelColorPair(leftLabelColorPair) {
    _owningPanel = owningPanel;
    _colorPairs = std::move(colorPairs);
    _ratios = std::move(ratios);
  }
};

static std::vector<std::string> headers = {"    PID", "USER    ", "PRI",
                                           " NI", "  VIRT", "  RES", "  SHR",
                                           "S", "  CPU%", "  MEM%", "   TIME+ ", "COMMAND"};

static std::vector<int> column_positions;

static float truncateTo1Decimal(float value) {
  return std::round(value * 10) / 10.0f;
}

template <typename T>
static std::string to_string_with_precision(const T a_value, const int n = 1) {
  std::ostringstream out;
  out.precision(n);
  out << std::fixed << a_value;
  return std::move(out).str();
}

#define KB_TO_MB(kb) ((float)(kb) / 1024.0f)
#define KB_TO_GB(kb) ((float)(kb) / (1024.0f * 1024.0f))
#define IS_LESS_THAN_1MB(kb) ((kb) < 1024)
#define IS_LESS_THAN_1GB(kb) ((kb) < (1024 * 1024))

static std::string convertMemoryToStr(uint64_t mem_value_in_kb,
                                      int precision = 1) {
  std::string memStr;
  if (IS_LESS_THAN_1GB(mem_value_in_kb)) {
    if (IS_LESS_THAN_1MB(mem_value_in_kb)) {
      return std::to_string(mem_value_in_kb) + "K";
    }
    memStr = to_string_with_precision(KB_TO_MB(mem_value_in_kb), precision) + "M";
    return memStr;
  }
  memStr = to_string_with_precision(KB_TO_GB(mem_value_in_kb), precision) + "G";
  return memStr;
}

static void displayTableHeader(WINDOW* headerWindow) {
  wattron(headerWindow, COLOR_PAIR(ColorPairs::black_green_pair));
  int width = getmaxx(headerWindow);

  for (int col = 0; col < width; ++col) {
    mvwaddch(headerWindow, 0, col, ' ');
  }

  for (size_t i = 0; i < headers.size(); ++i) {
    /*if (i == CPU_INDEX || i == MEM_INDEX) {
      mvwprintw(headerWindow, 0, column_positions[i], "%s",
                headers[i].substr(0, headers[i].size() - 1).c_str());
    } else {*/
    mvwprintw(headerWindow, 0, column_positions[i],
                "%s", headers[i].c_str());
  }

  wattroff(headerWindow, COLOR_PAIR(ColorPairs::black_green_pair));
}

// Calculate column positions based on header lengths and spacing
static void calculateColumnPositions() {
  int col_position = 0;

  for (const auto& header : headers) {
    column_positions.push_back(col_position);
    col_position += header.size() + UPPER_PANEL_SPACING_BETWEEN_COLUMNS;
  }
}

// Helper to right-align text
static int rightAlignPosition(int start_position, int field_width,
                              const std::string& text) {
  return start_position + field_width - text.size();
}

static void printRightAligned(WINDOW* window, int row, int col_start,
                              int col_width, const std::string& text) {
  int pos = rightAlignPosition(col_start, col_width, text);
  mvwprintw(window, row, pos, "%s", text.c_str());
}

static void displayProcesses(
    WINDOW* processesWin,
    const std::vector<std::shared_ptr<Process>>& processes,
    const MemData& memData, int max_rows,
    int current_selection, int scroll_offset) {
  int window_width = getmaxx(processesWin);

  for (int i = 0; i < max_rows; ++i) {
    move(i, 0);
    wclrtoeol(processesWin);
    int process_index = scroll_offset + i;
    if (process_index > (ssize_t)processes.size() - 1) {
      break;
    }
    if (process_index == current_selection) {
      wattron(processesWin, COLOR_PAIR(ColorPairs::black_cyan_pair));
    }

    std::string pid = std::to_string(processes[process_index]->Pid());
    printRightAligned(processesWin, i, column_positions[PID_INDEX],
                      headers[PID_INDEX].size(), pid);

    std::string user =
        processes[process_index]->User().substr(0, headers[USER_INDEX].size());
    printRightAligned(processesWin, i, column_positions[USER_INDEX],
                      headers[USER_INDEX].size(), user);

    std::string priority = std::to_string(processes[process_index]->PriorityValue());
    printRightAligned(processesWin, i, column_positions[PRI_INDEX],
                      headers[PRI_INDEX].size(), priority);

    std::string nice = std::to_string(processes[process_index]->NiceValue());
    printRightAligned(processesWin, i, column_positions[NI_INDEX],
                      headers[NI_INDEX].size(), nice);

    const struct ProcessMemUtilization &memUtilization = processes[process_index]->MemUtilization();
    std::string virt_memory_str = convertMemoryToStr(memUtilization.virtual_mem, 0);
    printRightAligned(processesWin, i, column_positions[VIRT_INDEX],
                      headers[VIRT_INDEX].size(), virt_memory_str);

    std::string res_memory_str = convertMemoryToStr(memUtilization.resident_mem, 0);
    printRightAligned(processesWin, i, column_positions[RES_INDEX],
                      headers[RES_INDEX].size(), res_memory_str);

    std::string shr_memory_str = convertMemoryToStr(memUtilization.shared_mem, 0);
    printRightAligned(processesWin, i, column_positions[SHR_INDEX],
                      headers[SHR_INDEX].size(), shr_memory_str);

    printRightAligned(processesWin, i, column_positions[S_INDEX],
                      headers[S_INDEX].size(), std::string(1, processes[process_index]->State()));

    float cpu_utilization_f =
        truncateTo1Decimal(processes[process_index]->CpuUtilization());
    std::string cpu_utilization =
        to_string_with_precision<float>(cpu_utilization_f);
    printRightAligned(processesWin, i, column_positions[CPU_INDEX],
                      headers[CPU_INDEX].size(), cpu_utilization);

    float mem_utilization_f = truncateTo1Decimal(((double)processes[process_index]->MemUtilization().resident_mem
                                                 / memData.memTotal) * 100.0f);
    std::string mem_utilization_str = to_string_with_precision<float>(mem_utilization_f);
    printRightAligned(processesWin, i, column_positions[MEM_INDEX],
                      headers[MEM_INDEX].size(), mem_utilization_str);

    double uptime = processes[process_index]->UpTime();
    std::string uptime_str = Format::ElapsedTime(uptime);
    printRightAligned(processesWin, i, column_positions[TIME_INDEX],
                      headers[TIME_INDEX].size(), uptime_str);

    std::string command = processes[process_index]->Command().substr(
        0, window_width - column_positions[COMMAND_INDEX]);
    mvwprintw(processesWin, i, column_positions[COMMAND_INDEX], "%s",
              command.c_str());

    if (process_index == current_selection) {
      for (int col = 0; col < getmaxx(processesWin); ++col) {
        chtype ch = mvwinch(processesWin, i, col);
        char character = ch & A_CHARTEXT;
        if (character == ' ')
          mvwaddch(processesWin, i, col, ' ');
      }
      wattroff(processesWin, COLOR_PAIR(ColorPairs::black_cyan_pair));
    }
  }
}

static void drawSingleCpuBar(
    WINDOW* upperPanel, int start_y, int start_x, int core_idx, int bar_length,
    const std::vector<struct CPUDataWithHistory>& cpu_data) {
  const struct CPUData& currCoreData = cpu_data[core_idx].current;
  std::vector<float> utilizationVec;
  std::vector<ColorPairs> colorPairsVec;
  float utilization =
      (1.0f - (double)(currCoreData.idletime) / currCoreData.totaltime);

  if (cpu_data[core_idx].previous.has_value()) {
    const struct CPUData& prevCoreData =
        cpu_data[core_idx].previous.value();
    uint64_t totaltime_delta = currCoreData.totaltime - prevCoreData.totaltime;
    uint64_t idletime_delta = currCoreData.idletime - prevCoreData.idletime;
    utilization = (1.0f - (double)(idletime_delta) / totaltime_delta);
  }

  std::string rightLabel = to_string_with_precision<float>(utilization * 100.0f) + "%";

  utilizationVec.push_back(utilization);

  // Set color based on utilization level
  ColorPairs color_pair = (utilization < 0.5f)
                       ? ColorPairs::green_black_pair
                       : (utilization < 0.8f ? ColorPairs::yellow_black_pair
                                             : ColorPairs::red_black_pair);
  colorPairsVec.push_back(color_pair);

  std::string leftLabel = std::to_string(core_idx - 1);
  if (core_idx == 0) {
    leftLabel = "CPU";
  }
  ColorPairs leftLabelCol = ColorPairs::cyan_black_pair;
  ColorPairs bracketsCol = ColorPairs::white_black_pair;
  ColorPairs rightLabelCol = ColorPairs::white_black_pair;
  Bar barToDraw(start_x, start_y, bar_length, leftLabel, rightLabel, colorPairsVec,
                utilizationVec, upperPanel, bracketsCol, leftLabelCol, rightLabelCol);
  barToDraw.drawBar();
}

static void drawCpuBars(WINDOW* upperPanel, const std::vector<struct CPUDataWithHistory>& cpu_data) {
  int window_width = getmaxx(upperPanel);
  int num_cores = std::max((int)cpu_data.size() - 1, 1);
  werase(upperPanel);

  // calculate number of columns (independent of window width)
  int num_columns = std::max(1, (num_cores + UPPER_PANEL_BARS_PER_COLUMN - 1) /
                                    UPPER_PANEL_BARS_PER_COLUMN);
  if (num_cores == 1) {
    int bar_width =
        std::max(MIN_UPPER_PANEL_BAR_WIDTH + PADDING_BETWEEN_BARS,
                 window_width - UPPER_PANEL_LEFT_PADDING -
                   UPPER_PANEL_RIGHT_PADDING);
    int curr_start_x = UPPER_PANEL_LEFT_PADDING - 2;
    int curr_start_y = UPPER_PANEL_UP_PADDING + 2;
    drawSingleCpuBar(upperPanel, curr_start_y, curr_start_x, 0,
                     bar_width - PADDING_BETWEEN_BARS, cpu_data);
  } else {
    for (int curr_col = 0; curr_col < num_columns; ++curr_col) {
      for (int curr_row = 0; curr_row < UPPER_PANEL_BARS_PER_COLUMN;
           ++curr_row) {
        int curr_core_idx =
            curr_col * UPPER_PANEL_BARS_PER_COLUMN + curr_row + 1;
        if (curr_core_idx > (ssize_t)cpu_data.size() - 1) {
          break;
        }
        int bar_width =
            std::max(MIN_UPPER_PANEL_BAR_WIDTH + PADDING_BETWEEN_BARS,
                     ((window_width - UPPER_PANEL_LEFT_PADDING -
                       UPPER_PANEL_RIGHT_PADDING) /
                      (num_columns)));
        int curr_start_x = bar_width * curr_col + UPPER_PANEL_LEFT_PADDING;
        int curr_start_y = curr_row + UPPER_PANEL_UP_PADDING;
        drawSingleCpuBar(upperPanel, curr_start_y, curr_start_x, curr_core_idx,
                         bar_width - PADDING_BETWEEN_BARS, cpu_data);
      }
    }
  }
}

static std::string memoryUtilizationStr(const struct MemData& mem_data) {
  uint64_t mem_total = mem_data.memTotal;
  uint64_t total_used_mem = mem_data.memTotal - mem_data.memFree;
  uint64_t non_cache_buffer_mem =
      total_used_mem - (mem_data.buffers + mem_data.cached);

  std::string memoryStr = convertMemoryToStr(non_cache_buffer_mem) + "/" +
                          convertMemoryToStr(mem_total);

  return memoryStr;
}

static std::string swapUtilizationStr(const struct MemData& mem_data) {
  uint64_t swap_total = mem_data.swapTotal;
  uint64_t swap_used = mem_data.swapTotal - mem_data.swapFree;
  std::string swapStr = convertMemoryToStr(swap_used) + "/" +
                        convertMemoryToStr(swap_total);

  return swapStr;
}

static void drawMemUtilization(WINDOW* upperPanel,
                               const struct MemData& mem_data) {
  /* Draw memory utilization bar */
  int window_width = getmaxx(upperPanel);
  std::string bar_label = "Mem";
  std::string mem_utilization_str = memoryUtilizationStr(mem_data);
  int bar_width = std::max(
      MIN_UPPER_PANEL_BAR_WIDTH + PADDING_BETWEEN_BARS,
      (window_width - UPPER_PANEL_LEFT_PADDING - UPPER_PANEL_RIGHT_PADDING) /
          2);
  int start_y = UPPER_PANEL_UP_PADDING + UPPER_PANEL_BARS_PER_COLUMN;
  int start_x = UPPER_PANEL_LEFT_PADDING - bar_label.size() + 1;

  uint64_t total_used_mem = mem_data.memTotal - mem_data.memFree;
  uint64_t non_cache_buffer_mem =
      total_used_mem - (mem_data.buffers + mem_data.cached);  // green
  uint64_t buffers_mem = mem_data.buffers;                       // blue
  uint64_t cached_mem = mem_data.cached + mem_data.sReclaimable
      - mem_data.shmem; // yellow

  float non_cache_buffer_mem_r =
      (double)(non_cache_buffer_mem) / mem_data.memTotal;           // green
  float buffers_mem_r = (double)(buffers_mem) / mem_data.memTotal;  // blue
  float cached_mem_r = (double)(cached_mem) / mem_data.memTotal;    // yellow
  std::vector<ColorPairs> colorPairsVec = {ColorPairs::green_black_pair, ColorPairs::blue_black_pair,
                                           ColorPairs::yellow_black_pair};
  std::vector<float> ratiosVec = {non_cache_buffer_mem_r, buffers_mem_r, cached_mem_r};
  Bar memBar(start_x, start_y, bar_width, bar_label, mem_utilization_str,
             colorPairsVec, ratiosVec, upperPanel);
  memBar.drawBar();

  /* Draw Swp Utilization bar */
  bar_label = "Swp";
  start_y++;
  std::string swap_utilization_str = swapUtilizationStr(mem_data);
  colorPairsVec = {ColorPairs::red_black_pair};
  uint64_t swap_total = mem_data.swapTotal;
  uint64_t swap_used = mem_data.swapTotal - mem_data.swapFree;
  float swap_used_r = (double)(swap_used) / swap_total;
  ratiosVec = {swap_used_r};
  Bar swpBar(start_x, start_y, bar_width, bar_label, swap_utilization_str,
             colorPairsVec, ratiosVec, upperPanel);
  swpBar.drawBar();
}

static void initColors() {
  init_pair(static_cast<short>(ColorPairs::black_green_pair),
            COLOR_BLACK, COLOR_GREEN);
  init_pair(static_cast<short>(ColorPairs::green_black_pair),
            COLOR_GREEN, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::blue_black_pair),
            COLOR_BLUE, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::yellow_black_pair),
            COLOR_YELLOW, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::cyan_black_pair),
            COLOR_CYAN, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::white_black_pair),
            COLOR_WHITE, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::red_black_pair),
            COLOR_RED, COLOR_BLACK);
  init_pair(static_cast<short>(ColorPairs::black_cyan_pair),
            COLOR_BLACK, COLOR_CYAN);
}

static void drawGlobalSystemStats(WINDOW *upperPanel, System& system) {
  int window_width = getmaxx(upperPanel);
  int start_y = UPPER_PANEL_UP_PADDING + UPPER_PANEL_BARS_PER_COLUMN;
  int start_x = window_width / 2;
  unsigned int numTasks = system.processManager.getNumOfTasks();
  unsigned int numThreads = system.processManager.getNumOfThreads();
  unsigned int numRunning = system.processManager.getNumOfRunningTasks();

  wattron(upperPanel, COLOR_PAIR(ColorPairs::cyan_black_pair));
  mvwprintw(upperPanel, start_y, start_x, "%s", ("OS: " + system.OperatingSystem()).c_str());
  mvwprintw(upperPanel, start_y + 1, start_x, "%s", ("Kernel: " + system.Kernel()).c_str());
  mvwprintw(upperPanel, start_y + 2, start_x, "Tasks: %u, %u thr; %u running", numTasks, numThreads - numTasks,
            numRunning);
  mvwprintw(upperPanel, start_y + 3, start_x, "Load average: %s", System::LoadAverage().c_str());
  mvwprintw(upperPanel, start_y + 4, start_x, "Uptime: %s",
           Format::FormatUptime(System::UpTime()).c_str());
  wattroff(upperPanel, COLOR_PAIR(ColorPairs::cyan_black_pair));
}

struct DisplayState {
  std::mutex mtx;
  int current_selection = 0;
  int scroll_offset = 0;
  std::vector<std::shared_ptr<Process>> processes;
  int numProcessesToDisplay = 0;
  bool running = true;
};

static int signal_pipe[2];

static void handleResize(int signal) {
  if (signal == SIGWINCH) {
    char data = 1;
    write(signal_pipe[1], &data, sizeof(data));
  }
}

static void screenResizer(DisplayState& state, EventQueue<Event>& eventQueue) {
  pipe(signal_pipe);

  struct sigaction sa;
  sa.sa_handler = handleResize;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  //sigaddset(&sa.sa_mask, SIGWINCH);
  sigaction(SIGWINCH, &sa, nullptr);

  fd_set read_fds;
  while (true) {
    {
      std::lock_guard<std::mutex> lock(state.mtx);
      if (!state.running) {
        close(signal_pipe[0]);
        close(signal_pipe[1]);
        return;
      }
    }

    FD_ZERO(&read_fds);
    FD_SET(signal_pipe[0], &read_fds);

    // Wait for signal on pipe
    if (select(signal_pipe[0] + 1, &read_fds, nullptr, nullptr, nullptr) > 0) {
      char data;
      read(signal_pipe[0], &data, sizeof(data));
      if (data == 'q') {
        close(signal_pipe[0]);
        close(signal_pipe[1]);
        return;
      }
      eventQueue.push({EventType::RESIZE, 0});
    }
  }
}

static void scanKeys(DisplayState& state, EventQueue<Event>& eventQueue) {
  while (true) {
    {
      std::lock_guard<std::mutex> lck(state.mtx);
      if (!state.running) return;
    }
    int ch = getch();
    if (ch == 'q') {
      {
        std::lock_guard<std::mutex> lck(state.mtx);
        state.running = false;
      }
      eventQueue.push({EventType::NONE, 0});
      return;
    } else {
      eventQueue.push({EventType::KEY_PRESS, ch});
    }
  }
}

static void screenRedrawer(DisplayState& state, EventQueue<Event>& eventQueue) {

  const std::chrono::milliseconds redrawInterval(GLOBAL_REFRESH_RATE);

  while (true) {
    {
      std::lock_guard<std::mutex> lck(state.mtx);
      if (!state.running) return;
    }
    eventQueue.push({EventType::REDRAW, 0});
    std::this_thread::sleep_for(redrawInterval);
  }
}

static bool resizeOrReallocateWindow(WINDOW **win, int newHeight,
                                     int newWidth, int startY, int startX) {
  if (newHeight < 1 || newWidth < 1) {
    return false; // Cannot allocate windows with invalid dimensions
  }

  if (*win == nullptr || getmaxy(*win) < 1 || getmaxx(*win) < 1) {
    // Reallocate if the window doesn't exist or has invalid dimensions
    if (*win) {
      delwin(*win);
    }
    *win = newwin(newHeight, newWidth, startY, startX);
    return true; // Reallocation occurred
  }

  // Otherwise, just resize the window
  wresize(*win, newHeight, newWidth);
  mvwin(*win, startY, startX);
  return false; // No reallocation needed
}

static void reinitWindows(WINDOW** processesListWindow, WINDOW** headerWindow,
                          WINDOW** upperPanel) {
  werase(stdscr);
  wnoutrefresh(stdscr);

  int windowHeight, windowWidth;
  getmaxyx(stdscr, windowHeight, windowWidth);

  werase(*processesListWindow);
  werase(*headerWindow);
  werase(*upperPanel);

  int processesListWindowHeight = std::max(1, windowHeight - LOWER_PANEL_WIDTH - UPPER_PANEL_HEIGHT);

  // Resize or reallocate windows as needed
  resizeOrReallocateWindow(processesListWindow, processesListWindowHeight, windowWidth,
                           UPPER_PANEL_HEIGHT + 1, 0);
  resizeOrReallocateWindow(headerWindow, 1, windowWidth, UPPER_PANEL_HEIGHT, 0);
  resizeOrReallocateWindow(upperPanel, UPPER_PANEL_HEIGHT, windowWidth, 0, 0);

  doupdate();
}

static void redrawWindow(DisplayState& state,
                         WINDOW* processesListWindow, WINDOW* headerWindow,
                         WINDOW* upperPanel, System& system,
                         bool redrawUpperPanel = true) {
  std::lock_guard<std::mutex> lck(state.mtx);

  //getmaxyx(stdscr, windowHeight, windowWidth);
  //state.numProcessesToDisplay = std::max(0, windowHeight - UPPER_PANEL_HEIGHT - 2);
  const auto& memData = system.MemoryUtilization();

  if (state.numProcessesToDisplay > 0) {
    werase(processesListWindow);
    system.processManager.UpdateProcesses();

    state.processes = system.processManager.GetSortedProcessesForDisplay();
    displayProcesses(processesListWindow, state.processes, memData,
                     state.numProcessesToDisplay, state.current_selection,
                     state.scroll_offset);
    wrefresh(processesListWindow);
  }

  if (redrawUpperPanel) {
    werase(upperPanel);
    werase(headerWindow);
    displayTableHeader(headerWindow);

    const auto& cpuData = System::totalCpuUtilization();
    drawCpuBars(upperPanel, cpuData);

    drawMemUtilization(upperPanel, memData);
    drawGlobalSystemStats(upperPanel, system);

    wrefresh(headerWindow);
    wrefresh(upperPanel);
  }

}

void Display(System& system) {
  initscr();  // Start ncurses mode
  raw();
  noecho();  // Don't echo keystrokes
  keypad(stdscr, TRUE);
  curs_set(0);    // Hide cursor
  start_color();  // Enable colors
  initColors();

  calculateColumnPositions();
  int windowHeight, windowWidth;
  getmaxyx(stdscr, windowHeight, windowWidth);
  WINDOW* processesListWindow = newwin(windowHeight - LOWER_PANEL_WIDTH - UPPER_PANEL_HEIGHT,
                                       windowWidth, 1 + UPPER_PANEL_HEIGHT, 0);
  WINDOW* headerWindow = newwin(1, windowWidth, UPPER_PANEL_HEIGHT, 0);
  WINDOW* upperPanel = newwin(UPPER_PANEL_HEIGHT, windowWidth, 0, 0);

  DisplayState displayState;
  displayState.numProcessesToDisplay = std::max(1, windowHeight - LOWER_PANEL_WIDTH - UPPER_PANEL_HEIGHT);
  EventQueue<Event> queue;
  std::thread keysScanner(scanKeys, std::ref(displayState), std::ref(queue));
  std::thread refreshTimer(screenRedrawer, std::ref(displayState), std::ref(queue));
  std::thread screenResizerT(screenResizer, std::ref(displayState), std::ref(queue));

  while (true) {
    {
      std::lock_guard<std::mutex> lck(displayState.mtx);
      if (!displayState.running) break;
    }
    // Process events from the queue
    Event event = queue.pop();

    if (event.type == EventType::KEY_PRESS) {
      std::unique_lock<std::mutex> lock(displayState.mtx);
      switch (event.key) {
        case KEY_UP:
          if (displayState.current_selection > 0) {
            if (displayState.current_selection == displayState.scroll_offset) {
              displayState.scroll_offset--;
            }
            displayState.current_selection--;
            lock.unlock();
            redrawWindow(displayState, processesListWindow,
                         headerWindow, upperPanel, system,
                         true);
          }
          break;

        case KEY_DOWN:
          if (displayState.current_selection < static_cast<ssize_t>(displayState.processes.size()) - 1) {
            if (displayState.current_selection == displayState.numProcessesToDisplay
                                                      + displayState.scroll_offset - 1) {
              displayState.scroll_offset++;
            }
            displayState.current_selection++;
            lock.unlock();
            redrawWindow(displayState, processesListWindow,
                         headerWindow, upperPanel, system,
                         true);
          }
          break;

      }
    } else if (event.type == EventType::RESIZE || event.type == EventType::REDRAW) {
      if (event.type == EventType::RESIZE) {
        endwin();
        getmaxyx(stdscr, windowHeight, windowWidth);
        // Update numProcessesToDisplay based on the new window height
        int newNumProcessesToDisplay = std::max(1, windowHeight - LOWER_PANEL_WIDTH - UPPER_PANEL_HEIGHT);

        {
          std::lock_guard<std::mutex> lck(displayState.mtx);

          // If the selection is out of view, adjust it
          if (displayState.current_selection >= displayState.scroll_offset +
                                                    newNumProcessesToDisplay) {
            displayState.current_selection = newNumProcessesToDisplay + displayState.scroll_offset - 1;
          }

          displayState.numProcessesToDisplay = newNumProcessesToDisplay;
        }
        reinitWindows(&processesListWindow, &headerWindow, &upperPanel);
      }
      redrawWindow(displayState, processesListWindow,
                   headerWindow, upperPanel, system, true);
    } else if (event.type == EventType::NONE) {
      break;
    }
  }

  keysScanner.join();
  refreshTimer.join();
  write(signal_pipe[1], "q", 1);
  screenResizerT.join();
  delwin(processesListWindow);
  delwin(headerWindow);
  delwin(upperPanel);
  endwin();
}

}  // namespace NCursesDisplay
