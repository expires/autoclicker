#pragma once
#include <jni.h>
#include <jvmti.h>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string>

class Lunar
{
public:
	// Per-thread JNIEnv. Each thread that wants to call JNI must AttachCurrentThread
	// to populate this for itself. Accessed as lc->env from any thread; the static
	// + thread_local combo means lc->env refers to the calling thread's storage.
	static thread_local JNIEnv *env;
	JavaVM *vm{nullptr};
	// Set after GetLoadedClasses() finishes populating the map. Other threads
	// must wait on this before calling GetClass() — the map isn't thread-safe.
	std::atomic<bool> classesLoaded{false};

public:
	void GetLoadedClasses();
	jclass GetClass(std::string classname);

private:
	std::unordered_map<std::string, jclass> classes;
};

inline auto lc = std::make_unique<Lunar>();
