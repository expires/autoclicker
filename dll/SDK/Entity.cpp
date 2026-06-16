#include "Entity.h"
#include "Mappings.h"

Entity::Entity(jobject instance)
{
    this->instance = instance;
}

jclass Entity::GetClass()
{
    return lc->GetClass(MC_Entity);
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
    jmethodID name = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getName, DESC_Entity_getName);

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
    jmethodID getUUIDMethod = lc->env->GetMethodID(this->GetClass(), MTD_Entity_getUUID, DESC_Entity_getUUID);
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
    jmethodID typeName = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getTypeName, DESC_Entity_getTypeName);

    jobject rtn = lc->env->CallObjectMethod(this->GetInstance(), typeName);

    return Component(rtn);
}

static uint32_t readLocalColor(jobject component)
{
    if (!component) return 0;

    jclass compCls = lc->GetClass(MC_Component);
    if (!compCls) return 0;
    jmethodID getStyleM = lc->env->GetMethodID(compCls,
        MTD_Component_getStyle, DESC_Component_getStyle);
    if (!getStyleM) { lc->env->ExceptionClear(); return 0; }

    jobject style = lc->env->CallObjectMethod(component, getStyleM);
    if (lc->env->ExceptionCheck() || !style) {
        lc->env->ExceptionClear();
        return 0;
    }

    jclass styleCls = lc->GetClass(MC_Style);
    if (!styleCls) { lc->env->DeleteLocalRef(style); return 0; }
    jmethodID getColorM = lc->env->GetMethodID(styleCls,
        MTD_Style_getColor, DESC_Style_getColor);
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

    jclass colorCls = lc->GetClass(MC_TextColor);
    if (!colorCls) { lc->env->DeleteLocalRef(textColor); return 0; }
    jmethodID getValueM = lc->env->GetMethodID(colorCls,
        MTD_TextColor_getValue, "()I");
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

    jclass compCls = lc->GetClass(MC_Component);
    if (!compCls) return;
    jmethodID getSiblingsM = lc->env->GetMethodID(compCls,
        MTD_Component_getSiblings, DESC_Component_getSiblings);
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
    jclass    listCls = nullptr;
    jmethodID sizeM   = nullptr;
    jmethodID getM    = nullptr;
    if (siblings) {
        listCls = lc->env->FindClass("java/util/List");
        if (listCls) {
            sizeM = lc->env->GetMethodID(listCls, "size", "()I");
            getM  = lc->env->GetMethodID(listCls, "get",  "(I)Ljava/lang/Object;");
            if (sizeM && getM) n = lc->env->CallIntMethod(siblings, sizeM);
        }
    }

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

    if (listCls) lc->env->DeleteLocalRef(listCls);
    if (siblings) lc->env->DeleteLocalRef(siblings);
}

std::vector<std::pair<std::string, uint32_t>> Entity::getFormattedNameChunks()
{
    std::vector<std::pair<std::string, uint32_t>> chunks;
    const uint32_t DEFAULT_COLOR = 0xFFFFFFFFu;

    Component nameC = getName();
    if (nameC.GetInstance() == nullptr) return chunks;

    jobject formatted = nullptr;
    jmethodID getTeamM = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getTeam, DESC_Entity_getTeam);
    if (getTeamM)
    {
        jobject team = lc->env->CallObjectMethod(this->instance, getTeamM);
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); team = nullptr; }
        if (team)
        {
            jclass teamCls = lc->GetClass(MC_PlayerTeam);
            if (teamCls)
            {
                jmethodID formatM = lc->env->GetStaticMethodID(teamCls,
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
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_position, DESC_Entity_position);
    jobject v = lc->env->GetObjectField(this->instance, f);
    return Vec3(v);
}
double Entity::getX() { return getPosition().getX(); }
double Entity::getY() { return getPosition().getY(); }
double Entity::getZ() { return getPosition().getZ(); }

double Entity::getXo()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_xo, "D");
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getYo()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_yo, "D");
    return lc->env->GetDoubleField(this->instance, f);
}
double Entity::getZo()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_zo, "D");
    return lc->env->GetDoubleField(this->instance, f);
}

float Entity::getYRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_yRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}
float Entity::getXRot()
{
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_xRot, "F");
    return lc->env->GetFloatField(this->instance, f);
}

AABB Entity::getBoundingBox()
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getBoundingBox, DESC_Entity_getBoundingBox);
    jobject b = lc->env->CallObjectMethod(this->instance, m);
    return AABB(b);
}

jobject Entity::getTeamRaw()
{
    jmethodID m = lc->env->GetMethodID(this->GetClass(),
        MTD_Entity_getTeam, DESC_Entity_getTeam);
    if (!m) { lc->env->ExceptionClear(); return nullptr; }
    jobject t = lc->env->CallObjectMethod(this->instance, m);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return nullptr; }
    return t;
}

