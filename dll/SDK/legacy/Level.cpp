#include "Level.h"
#include "Mappings.h"

std::vector<Player> Level::players()
{
    std::vector<Player> out;

    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_World_playerEntities, DESC_World_playerEntities)) return out;
    jobject list = lc->env->GetObjectField(this->instance, f);
    if (!list || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return out; }

    static jclass listCls = nullptr;
    static jmethodID sizeM = nullptr;
    static jmethodID getM  = nullptr;
    if (!JListOps(listCls, sizeM, getM)) { lc->env->DeleteLocalRef(list); return out; }

    jint n = lc->env->CallIntMethod(list, sizeM);
    out.reserve(n);
    for (jint i = 0; i < n; ++i)
    {
        jobject p = lc->env->CallObjectMethod(list, getM, i);
        if (p) out.emplace_back(p);
    }

    lc->env->DeleteLocalRef(list);
    return out;
}
