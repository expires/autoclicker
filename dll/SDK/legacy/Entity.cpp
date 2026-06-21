#include "Entity.h"
#include "Mappings.h"

std::vector<std::pair<std::string, uint32_t>> Entity::getFormattedNameChunks()
{
    std::vector<std::pair<std::string, uint32_t>> chunks;
    Component nameC = getName();
    if (nameC.GetInstance() != nullptr) {
        std::string s = nameC.getString();
        if (!s.empty()) chunks.emplace_back(std::move(s), 0xFFFFFFFFu);
    }
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
