#include "Entity.h"
#include "Mappings.h"

static uint32_t mcFormatColor(char code)
{
    switch (code)
    {
        case '0': return 0xFF000000u;
        case '1': return 0xFFAA0000u;
        case '2': return 0xFF00AA00u;
        case '3': return 0xFFAAAA00u;
        case '4': return 0xFF0000AAu;
        case '5': return 0xFFAA00AAu;
        case '6': return 0xFF00AAFFu;
        case '7': return 0xFFAAAAAAu;
        case '8': return 0xFF555555u;
        case '9': return 0xFFFF5555u;
        case 'a': return 0xFF55FF55u;
        case 'b': return 0xFFFFFF55u;
        case 'c': return 0xFF5555FFu;
        case 'd': return 0xFFFF55FFu;
        case 'e': return 0xFF55FFFFu;
        case 'f': return 0xFFFFFFFFu;
        default:  return 0xFFFFFFFFu;
    }
}

std::vector<std::pair<std::string, uint32_t>> Entity::getFormattedNameChunks()
{
    std::vector<std::pair<std::string, uint32_t>> chunks;

    std::string s;
    static jmethodID dispM = nullptr;
    if (JMethod(dispM, this->GetClass(), MTD_Entity_getDisplayName, DESC_Entity_getDisplayName))
    {
        jobject d = lc->env->CallObjectMethod(this->instance, dispM);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); d = nullptr; }
        if (d != nullptr) { s = Component(d).getString(); lc->env->DeleteLocalRef(d); }
    }
    else { lc->env->ExceptionClear(); }

    if (s.empty())
    {
        Component nameC = getName();
        if (nameC.GetInstance() != nullptr) s = nameC.getString();
    }
    if (s.empty()) return chunks;

    uint32_t cur = 0xFFFFFFFFu;
    std::string buf;
    auto flush = [&]() {
        if (!buf.empty()) { chunks.emplace_back(buf, cur); buf.clear(); }
    };

    for (size_t i = 0; i < s.size(); )
    {
        const unsigned char c0 = (unsigned char)s[i];
        char code = 0;
        if (c0 == 0xC2u && i + 2 < s.size() && (unsigned char)s[i + 1] == 0xA7u) { code = s[i + 2]; i += 3; }
        else if (c0 == 0xA7u && i + 1 < s.size()) { code = s[i + 1]; i += 2; }
        else { buf.push_back(s[i]); ++i; continue; }

        if (code >= 'A' && code <= 'Z') code = (char)(code - 'A' + 'a');
        if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) { flush(); cur = mcFormatColor(code); }
        else if (code == 'r') { flush(); cur = 0xFFFFFFFFu; }
    }
    flush();

    return chunks;
}

Vec3 Entity::getPosition()
{
    static jmethodID m = nullptr;
    if (!JMethod(m, this->GetClass(), MTD_Entity_getPositionVector, DESC_Entity_getPositionVector)) return Vec3(nullptr);
    jobject v = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return Vec3(nullptr); }
    return Vec3(v);
}

double Entity::getX()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_posX, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getY()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_posY, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getZ()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_posZ, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
