#pragma once
#include <jni.h>
#include <jvmti.h>
#include <mutex>
#include <unordered_map>
#include <string>

class Lunar
{
public:
	JNIEnv* env {nullptr};
	JavaVM* vm {nullptr};
public:
	void GetLoadedClasses();
	jclass GetClass(std::string classname);
private:
	std::unordered_map<std::string, jclass> classes;
};

inline auto lc = std::make_unique<Lunar>();