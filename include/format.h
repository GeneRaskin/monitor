#ifndef FORMAT_H
#define FORMAT_H

#include <string>

namespace Format {
std::string ElapsedTime(double uptime_in_seconds);
std::string FormatUptime(unsigned long long uptimeSeconds);
} // namespace Format

#endif