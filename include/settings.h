//
// Created by Nathan Tormaschy on 4/19/23.
//

#ifndef ARGUS_SETTINGS_H
#define ARGUS_SETTINGS_H

//#define DEBUGGING
#define ARGUS_RUNTIME_ASSERT
#define ARGUS_STRIP
//#define ARGUS_BROKER_ACCOUNT_TRACKING
//#define ARGUS_HISTORY

static double constexpr ARGUS_PORTFOLIO_MAX_LEVERAGE  = 2;
static double constexpr ARGUS_MP_PORTFOLIO_MAX_LEVERAGE = 1.75;

// Number of decimal places for fixed-point representation
static int constexpr DECIMAL_PLACES = 8;

// Scale factor for converting between fixed-point and double values
static double constexpr SCALE_FACTOR = 1e8;

#include <stdexcept>
#include <string>

enum class ErrorCode {
  Success = 0,
  FileNotFound = 1,
  InvalidParameter = 2,
  AccessDenied = 3
};

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(const std::string& message, const char* file, int line)
        : std::runtime_error(message + " (" + file + ":" + std::to_string(line) + ")")
    {}
};

#define ARGUS_RUNTIME_ERROR(msg) throw RuntimeError(msg, __FILE__, __LINE__)

#endif //ARGUS_SETTINGS_H
