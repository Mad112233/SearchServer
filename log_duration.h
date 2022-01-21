#pragma once

#include <string>
#include <iostream>
#include <chrono>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x,y) LogDuration UNIQUE_VAR_NAME_PROFILE(x,y)
#define LOG_DURATION_STREAM(x,y) LOG_DURATION(x,y)

class LogDuration {
public:
	using Clock = std::chrono::steady_clock;

	LogDuration(std::string task, std::ostream& out) :name_task_(task), out_(out) {
	}

	~LogDuration() {
		const auto dur = Clock::now() - start_;
		out_ << name_task_ << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << " ms" << std::endl;
	}

private:
	const std::string name_task_;
	std::ostream& out_ = std::cerr;
	const Clock::time_point start_ = Clock::now();
};