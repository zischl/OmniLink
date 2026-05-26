#ifndef OMNILOGGER_H
#define OMNILOGGER_H

#pragma once

#include <comdef.h>
#include <string>
#include <string_view>
#include <winerror.h>

#include <ctime>
#include <format>
#include <fstream>
#include <iostream>

#define HRCheck(hr)                                                            \
  if (FAILED(hr)) {                                                            \
    Logger::logHR(hr);                                                         \
  }

class Logger {
public:
  static void log(const std::string_view text);

  // Variadic template log function, it should accept any number of arguments,
  // use with ( text {}, x )
  template <typename... ArgTypes>
  static void log(std::format_string<ArgTypes...> fString, ArgTypes &&...args) {
    log(std::format(fString, std::forward<ArgTypes>(args)...));
  }

  static void logHR(const HRESULT hr);
};

#endif
