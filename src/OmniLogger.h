#ifndef OMNILOGGER_H
#define OMNILOGGER_H

#pragma once

#include <fstream>
#include <ctime>
#include <format>

class Logger {
public:
	static void log(const std::string_view text);

	template <typename... ArgTypes>
	static void log(const std::string_view string, ArgTypes&&... args) {
		log(std::vformat(string, std::make_format_args(args...)));

	}
};


#endif