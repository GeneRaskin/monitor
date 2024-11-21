#include "format.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

using std::string;

std::string Format::ElapsedTime(double uptime) {
  // Check if uptime is at least one hour
  if (uptime >= 3600) {
    // Format as "HHh:MM:SS" for hours
    int hours = static_cast<int>(uptime) / 3600;
    int minutes = (static_cast<int>(uptime) % 3600) / 60;
    int seconds = static_cast<int>(uptime) % 60;

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << hours << "h" << ":"
        << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds;

    return oss.str();

  } else {
    // Format as "MM:SS.xx" for minutes and seconds with fractional seconds
    int minutes = static_cast<int>(uptime) / 60;
    int seconds = static_cast<int>(uptime) % 60;
    double fractionalSeconds = uptime - std::floor(uptime);

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds << "."
        << std::setw(2) << std::setfill('0') << static_cast<int>(fractionalSeconds * 100);

    return oss.str();
  }
}

std::string Format::FormatUptime(unsigned long long uptimeSeconds) {
  unsigned long long days = uptimeSeconds / (24 * 3600);
  uptimeSeconds %= (24 * 3600);
  unsigned long long hours = uptimeSeconds / 3600;
  uptimeSeconds %= 3600;
  unsigned long long minutes = uptimeSeconds / 60;
  unsigned long long seconds = uptimeSeconds % 60;

  std::ostringstream formattedUptime;

  // Format the output
  if (days > 0) {
    formattedUptime << days << " day" << (days > 1 ? "s, " : ", ");
  }
  formattedUptime << std::setfill('0') << std::setw(2) << hours << ":"
                  << std::setfill('0') << std::setw(2) << minutes << ":"
                  << std::setfill('0') << std::setw(2) << seconds;

  return formattedUptime.str();
}
