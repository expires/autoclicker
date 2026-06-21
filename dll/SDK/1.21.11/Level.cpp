#include "Level.h"
#include "Mappings.h"

std::vector<Player> Level::players()
{
    std::vector<Player> out;

    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_ClientLevel_players, DESC_ClientLevel_players);
    if (!m) { lc->env->ExceptionClear(); return out; }

    jobject list = lc->env->CallObjectMethod(this->instance, m);
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
