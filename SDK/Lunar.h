#pragma once
#include <jni.h>
#include <jvmti.h>
#include <mutex>
#include <unordered_map>
#include <string>
#include <iostream>

class Lunar
{
public:
	JNIEnv* env;
	JavaVM* vm;
public:
	void GetLoadedClasses();
	jclass GetClass(std::string classname);
private:
	std::unordered_map<std::string, jclass> classes;
};

inline auto lc = std::make_unique<Lunar>();