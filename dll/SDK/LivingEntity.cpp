#include "LivingEntity.h"
#include "Mappings.h"
#include <cctype>
#include <string>

bool LivingEntity::hasEffectNamed(const char* needle)
{
    if (!needle || !*needle || !this->GetInstance()) return false;
    if (!*FLD_LivingEntity_activeEffects || !*MTD_MobEffectInstance_getName) return false;

    static jfieldID effectsF = nullptr;
    JField(effectsF, this->GetClass(), FLD_LivingEntity_activeEffects, DESC_LivingEntity_activeEffects);
    if (!effectsF) return false;

    static jclass effectCls = nullptr;
    if (!JClass(effectCls, MC_MobEffectInstance)) return false;

    static jmethodID nameM = nullptr;
    JMethod(nameM, effectCls, MTD_MobEffectInstance_getName, "()Ljava/lang/String;");
    if (!nameM) { lc->env->ExceptionClear(); return false; }

    static jclass mapCls = nullptr, iterableCls = nullptr, iteratorCls = nullptr;
    static jmethodID valuesM = nullptr, iteratorM = nullptr, hasNextM = nullptr, nextM = nullptr;
    if (!mapCls)
    {
        jclass a = lc->env->FindClass("java/util/Map");
        jclass b = lc->env->FindClass("java/lang/Iterable");
        jclass c = lc->env->FindClass("java/util/Iterator");
        if (!a || !b || !c) { lc->env->ExceptionClear(); return false; }
        mapCls      = (jclass)lc->env->NewGlobalRef(a);
        iterableCls = (jclass)lc->env->NewGlobalRef(b);
        iteratorCls = (jclass)lc->env->NewGlobalRef(c);
        lc->env->DeleteLocalRef(a);
        lc->env->DeleteLocalRef(b);
        lc->env->DeleteLocalRef(c);
    }
    JMethod(valuesM,   mapCls,      "values",   "()Ljava/util/Collection;");
    JMethod(iteratorM, iterableCls, "iterator", "()Ljava/util/Iterator;");
    JMethod(hasNextM,  iteratorCls, "hasNext",  "()Z");
    JMethod(nextM,     iteratorCls, "next",     "()Ljava/lang/Object;");
    if (!valuesM || !iteratorM || !hasNextM || !nextM) { lc->env->ExceptionClear(); return false; }

    JNIEnv* env = lc->env;

    jobject map = env->GetObjectField(this->GetInstance(), effectsF);
    if (!map || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jobject values = env->CallObjectMethod(map, valuesM);
    env->DeleteLocalRef(map);
    if (!values || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jobject it = env->CallObjectMethod(values, iteratorM);
    env->DeleteLocalRef(values);
    if (!it || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    std::string want = needle;
    for (char& ch : want) ch = (char)std::tolower((unsigned char)ch);

    bool found = false;
    while (!found)
    {
        const jboolean more = env->CallBooleanMethod(it, hasNextM);
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
        if (more != JNI_TRUE) break;

        jobject effect = env->CallObjectMethod(it, nextM);
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
        if (!effect) continue;

        jstring js = (jstring)env->CallObjectMethod(effect, nameM);
        env->DeleteLocalRef(effect);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!js) continue;

        const char* chars = env->GetStringUTFChars(js, nullptr);
        if (chars)
        {
            std::string have = chars;
            for (char& ch : have) ch = (char)std::tolower((unsigned char)ch);
            if (have.find(want) != std::string::npos) found = true;
            env->ReleaseStringUTFChars(js, chars);
        }
        env->DeleteLocalRef(js);
    }

    env->DeleteLocalRef(it);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return found;
}

jclass LivingEntity::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_LivingEntity);
}

bool LivingEntity::isUsingItem()
{
	static jmethodID isUsingItem = nullptr;
	JMethod(isUsingItem, this->GetClass(), MTD_LivingEntity_isUsingItem, "()Z");
	if (!isUsingItem) { lc->env->ExceptionClear(); return false; }
	jboolean v = lc->env->CallBooleanMethod(this->GetInstance(), isUsingItem);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
	return v == JNI_TRUE;
}

float LivingEntity::getHealth()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_LivingEntity_getHealth, "()F");
	if (!m) { lc->env->ExceptionClear(); return -1.0f; }
	jfloat v = lc->env->CallFloatMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1.0f; }
	return v;
}

float LivingEntity::getMaxHealth()
{
	static jmethodID m = nullptr;
	JMethod(m, this->GetClass(), MTD_LivingEntity_getMaxHealth, "()F");
	if (!m) { lc->env->ExceptionClear(); return -1.0f; }
	jfloat v = lc->env->CallFloatMethod(this->GetInstance(), m);
	if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return -1.0f; }
	return v;
}
