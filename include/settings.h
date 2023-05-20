//
// Created by Nathan Tormaschy on 4/19/23.
//

#ifndef ARGUS_SETTINGS_H
#define ARGUS_SETTINGS_H

//#define DEBUGGING
#define ARGUS_HIGH_PRECISION
#define ARGUS_RUNTIME_ASSERT
#define ARGUS_STRIP
//#define ARGUS_BROKER_ACCOUNT_TRACKING
//#define ARGUS_HISTORY

static double constexpr ARGUS_PORTFOLIO_MAX_LEVERAGE  = 2;
static double constexpr ARGUS_MP_PORTFOLIO_MAX_LEVERAGE = 1.75;

#include <stdexcept>
#include <string>

enum ArgusErrorCode {
  NotImplemented,
  NotWarm,
  NotBuilt,
  AlreadyBuilt,

  IndexOutOfBounds,
  
  InvalidTracerType,
  InvalidAssetFrequency,
  InvalidTracerAsset,
  InvalidDataRequest,
  InvalidDatetime,
  InvalidId,
  InvalidArrayLength,
  InvalidArrayValues
};

static const std::string EnumStrings[] = 
{
  "Not implemented",
  "Not Warm"
  "Object is not built",
  "Object is already built",

  "Index Out of Bounds Error",

  "Invalid tracer type"
  "Invalid asset frequency",
  "Invalid tracer asset passed",
  "Invalid data request",
  "Invalid datetime passed",
  "Invalid id passed"
  "Invalid array length",
  "Invalid array values"
};

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(const std::string& message, const char* file, int line)
        : std::runtime_error(message + " (" + file + ":" + std::to_string(line) + ")")
    {}
    RuntimeError(ArgusErrorCode error_code, const char* file, int line)
        : std::runtime_error(EnumStrings[error_code] + " (" + file + ":" + std::to_string(line) + ")")
    {}
};

#define ARGUS_RUNTIME_ERROR(msg) throw RuntimeError(msg, __FILE__, __LINE__)

#endif //ARGUS_SETTINGS_H
