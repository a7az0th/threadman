#pragma once

#include <chrono>

namespace a7az0th {

typedef long long int int64 ;

class Timer {
public:
	enum Precision {
		Seconds = 0,
		Milliseconds,
		Nanoseconds,
	};

	Timer() { init(); }
	~Timer() {};

	void start() { init(); }
	void stop() { endPoint = std::chrono::system_clock::now(); }

	// Returns the elapsed time in the precision specified
	int64 elapsed(Precision prec) {
		int64 val = 0.f;
		switch (prec) {
		case Seconds: {
			const std::chrono::seconds& total_s = std::chrono::duration_cast<std::chrono::seconds>(endPoint - startPoint);
			val = total_s.count();
			break;
		}
		case Milliseconds: {
			const std::chrono::milliseconds& total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endPoint - startPoint);
			val = total_ms.count();
			break;
		}
		case Nanoseconds: {
			const std::chrono::nanoseconds& total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(endPoint - startPoint);
			val = total_ns.count();
			break;
		}
		default:
			assert(false);
		}
		return val;
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
