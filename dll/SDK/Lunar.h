#pragma once
#include <jni.h>
#include <jvmti.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>

class Lunar
{
public:
	static thread_local JNIEnv *env;
	JavaVM *vm{nullptr};
	std::atomic<bool> classesLoaded{false};

public:
	void GetLoadedClasses();
	jclass GetClass(const std::string &classname);

private:
	void RefreshLocked();

	std::unordered_map<std::string, jclass> classes;
	std::mutex classesMutex;
	std::chrono::steady_clock::time_point lastRefresh{};
};

inline auto lc = std::make_unique<Lunar>();
