#include "Entity.h"
#include "Mappings.h"
#include <mutex>

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    static jclass c = nullptr;
    return JClass(c, MC_Entity);
}

void Entity::Cleanup()
{
    lc->env->DeleteLocalRef(this->instance);
}

jobject Entity::GetInstance()
{
    return this->instance;
}

Component Entity::getName()
{
    static jmethodID name = nullptr;
    JMethod(name, this->GetClass(), MTD_Entity_getName, DESC_Entity_getName);

    if (!name || lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return Component(nullptr);
    }

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), name);

    if (lc->env->ExceptionCheck())
    {
        lc->env->ExceptionClear();
        return Component(nullptr);
    }

    return Component(rtn);
}

std::string Entity::getUUID()
{
    static jmethodID getUUIDMethod = nullptr;
    JMethod(getUUIDMethod, this->GetClass(), MTD_Entity_getUUID, DESC_Entity_getUUID);
    if (!getUUIDMethod || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    jobject uuidObj = lc->env->CallObjectMethod(this->GetInstance(), getUUIDMethod);
    if (!uuidObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    jclass uuidClass = lc->env->GetObjectClass(uuidObj);
    jmethodID toStringMethod = lc->env->GetMethodID(uuidClass, "toString", "()Ljava/lang/String;");
    if (!toStringMethod) { lc->env->ExceptionClear(); return ""; }

    jstring javaString = (jstring)lc->env->CallObjectMethod(uuidObj, toStringMethod);
    if (!javaString || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return ""; }

    const char* chars = lc->env->GetStringUTFChars(javaString, nullptr);
    std::string result(chars ? chars : "");
    lc->env->ReleaseStringUTFChars(javaString, chars);
    return result;
}

Component Entity::getTypeName()
{
    static jmethodID typeName = nullptr;
    JMethod(typeName, this->GetClass(), MTD_Entity_getTypeName, DESC_Entity_getTypeName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}

static uint32_t readLocalColor(jobject component)
{
    if (!component) return 0;

    static jclass compCls = nullptr;
    static jmethodID getStyleM = nullptr;
    JMethod(getStyleM, JClass(compCls, MC_Component), MTD_Component_getStyle, DESC_Component_getStyle);
    if (!getStyleM) { lc->env->ExceptionClear(); return 0; }

    jobject style = lc->env->CallObjectMethod(component, getStyleM);
    if (lc->env->ExceptionCheck() || !style) {
        lc->env->ExceptionClear();
        return 0;
    }

    static jclass styleCls = nullptr;
    static jmethodID getColorM = nullptr;
    JMethod(getColorM, JClass(styleCls, MC_Style), MTD_Style_getColor, DESC_Style_getColor);
    if (!getColorM) {
        lc->env->ExceptionClear();
        lc->env->DeleteLocalRef(style);
        return 0;
    }

    jobject textColor = lc->env->CallObjectMethod(style, getColorM);
    lc->env->DeleteLocalRef(style);
    if (lc->env->ExceptionCheck() || !textColor) {
        lc->env->ExceptionClear();
        return 0;
    }

    static jclass colorCls = nullptr;
    static jmethodID getValueM = nullptr;
    JMethod(getValueM, JClass(colorCls, MC_TextColor), MTD_TextColor_getValue, "()I");
    if (!getValueM) {
        lc->env->ExceptionClear();
        lc->env->DeleteLocalRef(textColor);
        return 0;
    }

    jint rgb = lc->env->CallIntMethod(textColor, getValueM);
    lc->env->DeleteLocalRef(textColor);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return 0; }

    uint32_t r = (uint32_t)((rgb >> 16) & 0xFFu);
    uint32_t g = (uint32_t)((rgb >>  8) & 0xFFu);
    uint32_t b = (uint32_t)( rgb        & 0xFFu);
    return (0xFFu << 24) | (b << 16) | (g << 8) | r;
}

static void flattenComponent(jobject component, uint32_t inheritedColor,
                             std::vector<std::pair<std::string, uint32_t>>& out)
{
    if (!component) return;

    const uint32_t local     = readLocalColor(component);
    const uint32_t effective = local ? local : inheritedColor;

    static jclass compCls = nullptr;
    static jmethodID getSiblingsM = nullptr;
    JMethod(getSiblingsM, JClass(compCls, MC_Component), MTD_Component_getSiblings, DESC_Component_getSiblings);
    if (!getSiblingsM) {
        lc->env->ExceptionClear();
        Component c(component);
        std::string s = c.getString();
        if (!s.empty()) out.emplace_back(std::move(s), effective);
        return;
    }

    jobject siblings = lc->env->CallObjectMethod(component, getSiblingsM);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); siblings = nullptr; }

    jint n = 0;
    static jclass    listCls = nullptr;
    static jmethodID sizeM   = nullptr;
    static jmethodID getM    = nullptr;
    if (siblings && JListOps(listCls, sizeM, getM))
        n = lc->env->CallIntMethod(siblings, sizeM);

    if (n == 0) {

        Component c(component);
        std::string s = c.getString();
        if (!s.empty()) out.emplace_back(std::move(s), effective);
    } else {

        for (jint i = 0; i < n; ++i) {
            jobject sib = lc->env->CallObjectMethod(siblings, getM, i);
            if (sib) {
                flattenComponent(sib, effective, out);
                lc->env->DeleteLocalRef(sib);
            }
        }
    }

    if (siblings) lc->env->DeleteLocalRef(siblings);
}

std::vector<std::pair<std::string, uint32_t>> Entity::getFormattedNameChunks()
{
    std::vector<std::pair<std::string, uint32_t>> chunks;
    const uint32_t DEFAULT_COLOR = 0xFFFFFFFFu;

    static std::mutex s_teamReadMutex;
    std::lock_guard<std::mutex> teamLock(s_teamReadMutex);

    Component nameC = getName();
    if (nameC.GetInstance() == nullptr) return chunks;

    jobject formatted = nullptr;
    static jmethodID getTeamM = nullptr;
    JMethod(getTeamM, this->GetClass(), MTD_Entity_getTeam, DESC_Entity_getTeam);
    if (getTeamM)
    {
        jobject team = lc->env->CallObjectMethod(this->instance, getTeamM);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); team = nullptr; }
        if (team)
        {
            static jclass teamCls = nullptr;
            JClass(teamCls, MC_PlayerTeam);
            if (teamCls)
            {
                static jmethodID formatM = nullptr;
                JStaticMethod(formatM, teamCls,
                    MTD_PlayerTeam_formatNameForTeam, DESC_PlayerTeam_formatNameForTeam);
                if (formatM)
                {
                    formatted = lc->env->CallStaticObjectMethod(teamCls, formatM,
                        team, nameC.GetInstance());
                    if (lc->env->ExceptionCheck()) {
                        lc->env->ExceptionClear();
                        formatted = nullptr;
                    }
                }
                else { lc->env->ExceptionClear(); }
            }
            lc->env->DeleteLocalRef(team);
        }
    }
    else { lc->env->ExceptionClear(); }

    jobject root = formatted ? formatted : nameC.GetInstance();
    flattenComponent(root, DEFAULT_COLOR, chunks);

    if (formatted) lc->env->DeleteLocalRef(formatted);
    return chunks;
}

Vec3 Entity::getPosition()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_position, DESC_Entity_position)) return Vec3(nullptr);
    jobject v = lc->env->GetObjectField(this->instance, f);
    return Vec3(v);
}
double Entity::getX() { return getPosition().getX(); }
double Entity::getY() { return getPosition().getY(); }
double Entity::getZ() { return getPosition().getZ(); }

double Entity::getXo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_xo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getYo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_yo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getZo()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_zo, "D")) return 0.0;
    return lc->env->GetDoubleField(this->instance, f);
}

float Entity::getYRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_yRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}
float Entity::getXRot()
{
    static jfieldID f = nullptr;
    if (!JField(f, this->GetClass(), FLD_Entity_xRot, "F")) return 0.0f;
    return lc->env->GetFloatField(this->instance, f);
}

AABB Entity::getBoundingBox()
{
    static jmethodID m = nullptr;
    if (!JMethod(m, this->GetClass(), MTD_Entity_getBoundingBox, DESC_Entity_getBoundingBox)) return AABB(nullptr);
    jobject b = lc->env->CallObjectMethod(this->instance, m);
    return AABB(b);
}

jobject Entity::getTeamRaw()
{
    static jmethodID m = nullptr;
    JMethod(m, this->GetClass(), MTD_Entity_getTeam, DESC_Entity_getTeam);
    if (!m) { lc->env->ExceptionClear(); return nullptr; }
    jobject t = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
    return t;
}

