#pragma once

#include <chrono>

namespace a7az0th {

class Timer {
public:
	Timer() { init(); }
	~Timer() {};

	void start() { init(); }
	void stop() { endPoint = std::chrono::system_clock::now(); }

	// Returns the elapsed time in seconds
	float elapsedSeconds() {
		auto& total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endPoint - startPoint);
		return total_ms.count() * 0.001f;
	}

private:
	void init() {
		startPoint = endPoint = std::chrono::system_clock::now();
	}
	// Disallow evil contructors
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	std::chrono::system_clock::time_point startPoint;
	std::chrono::system_clock::time_point endPoint;
};

}
