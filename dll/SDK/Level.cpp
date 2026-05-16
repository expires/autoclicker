#include "Level.h"
#include "Mappings.h"

jclass Level::GetClass() { return lc->GetClass(MC_ClientLevel); }

std::vector<Player> Level::players()
{
    std::vector<Player> out;

    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_ClientLevel_players, DESC_ClientLevel_players);
    if (!m) { lc->env->ExceptionClear(); return out; }

    jobject list = lc->env->CallObjectMethod(this->instance, m);
    if (!list || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return out; }

    jclass listCls = lc->env->FindClass("java/util/List");
    jmethodID sizeM = lc->env->GetMethodID(listCls, "size", "()I");
    jmethodID getM  = lc->env->GetMethodID(listCls, "get",  "(I)Ljava/lang/Object;");

    jint n = lc->env->CallIntMethod(list, sizeM);
    out.reserve(n);
    for (jint i = 0; i < n; ++i)
    {
        jobject p = lc->env->CallObjectMethod(list, getM, i);
        if (p) out.emplace_back(p);
    }

    lc->env->DeleteLocalRef(list);
    lc->env->DeleteLocalRef(listCls);
    return out;
}
