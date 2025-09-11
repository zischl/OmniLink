#ifndef OMNIAPI_H
#define OMNIAPI_H

#pragma once
#include "OmniTypes.h"
#include <iostream>
#include <chrono>

class OmniLink;

class OmniAPI
{
public:
	static void Ignite(OmniLink& OmniLinkInstance);

	static void SwapDeviceLayout();

	static void Scan();

	inline static void perf_test_start() { 
		t1 = std::chrono::high_resolution_clock::now();
	}

	inline static void perf_test_end() {
		std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - t1).count() << "Time Taken : : : : :\n";
	}

private:
	static inline OmniLink* App = nullptr;

	static inline std::chrono::time_point<std::chrono::high_resolution_clock> t1;


};

#endif