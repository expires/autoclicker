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

// Reads the Style.getColor().getValue() chain for a single Component instance.
// Returns the fallback color if any link is null/missing/throws.
static uint32_t extractColor(jobject component, uint32_t fallback)
{
    if (!component) return fallback;

    jclass compCls = lc->GetClass(MC_Component);
    if (!compCls) return fallback;
    jmethodID getStyleM = lc->env->GetMethodID(compCls,
        MTD_Component_getStyle, DESC_Component_getStyle);
    if (!getStyleM) { lc->env->ExceptionClear(); return fallback; }

    jobject style = lc->env->CallObjectMethod(component, getStyleM);
    if (lc->env->ExceptionCheck() || !style) {
        lc->env->ExceptionClear();
        return fallback;
    }

    jclass styleCls = lc->GetClass(MC_Style);
    if (!styleCls) { lc->env->DeleteLocalRef(style); return fallback; }
    jmethodID getColorM = lc->env->GetMethodID(styleCls,
        MTD_Style_getColor, DESC_Style_getColor);
    if (!getColorM) {
        lc->env->ExceptionClear();
        lc->env->DeleteLocalRef(style);
        return fallback;
    }

    jobject textColor = lc->env->CallObjectMethod(style, getColorM);
    lc->env->DeleteLocalRef(style);
    if (lc->env->ExceptionCheck() || !textColor) {
        lc->env->ExceptionClear();
        return fallback;
    }

    jclass colorCls = lc->GetClass(MC_TextColor);
    if (!colorCls) { lc->env->DeleteLocalRef(textColor); return fallback; }
    jmethodID getValueM = lc->env->GetMethodID(colorCls,
        MTD_TextColor_getValue, "()I");
    if (!getValueM) {
        lc->env->ExceptionClear();
        lc->env->DeleteLocalRef(textColor);
        return fallback;
    }

    jint rgb = lc->env->CallIntMethod(textColor, getValueM);
    lc->env->DeleteLocalRef(textColor);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return fallback; }

    // MC's TextColor.getValue() returns 0x00RRGGBB. ImGui's ImU32 is ABGR
    // (R in the low byte, B in the high byte) — swap R and B here so colors
    // render correctly. Without this swap gold (0xFFAA00) prints as blue.
    uint32_t r = (uint32_t)((rgb >> 16) & 0xFFu);
    uint32_t g = (uint32_t)((rgb >>  8) & 0xFFu);
    uint32_t b = (uint32_t)( rgb        & 0xFFu);
    return (0xFFu << 24) | (b << 16) | (g << 8) | r;
}

// Multiply RGB channels by `factor` (alpha preserved). Used to make team
// prefix/suffix render slightly dimmer than the player's name so the name
// stays visually dominant.
static uint32_t darkenRGB(uint32_t argb, float factor)
{
    uint32_t a = (argb >> 24) & 0xFFu;
    uint32_t r = (uint32_t)(((argb >> 16) & 0xFFu) * factor);
    uint32_t g = (uint32_t)(((argb >>  8) & 0xFFu) * factor);
    uint32_t b = (uint32_t)(( argb        & 0xFFu) * factor);
    if (r > 0xFF) r = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (b > 0xFF) b = 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

std::vector<std::pair<std::string, uint32_t>> Entity::getFormattedNameChunks()
{
    std::vector<std::pair<std::string, uint32_t>> chunks;
    const uint32_t DEFAULT_COLOR = 0xFFFFFFFFu; // white

    Component nameC = getName();
    if (nameC.GetInstance() == nullptr) return chunks;

    // Resolve the team and produce the formatted MutableComponent. If anything
    // fails we fall back to the bare name as a single white chunk.
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

    // Use the formatted component if available, otherwise the bare name.
    jobject root = formatted ? formatted : nameC.GetInstance();
    const uint32_t rootColor = extractColor(root, DEFAULT_COLOR);

    // Walk top-level siblings — formatNameForTeam typically produces an empty
    // root with [prefix, name, suffix] siblings, each carrying its own Style.
    jclass compCls = lc->GetClass(MC_Component);
    if (compCls)
    {
        jmethodID getSiblingsM = lc->env->GetMethodID(compCls,
            MTD_Component_getSiblings, DESC_Component_getSiblings);
        if (getSiblingsM)
        {
            jobject siblings = lc->env->CallObjectMethod(root, getSiblingsM);
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); siblings = nullptr; }
            if (siblings)
            {
                jclass listCls = lc->env->FindClass("java/util/List");
                if (listCls)
                {
                    jmethodID sizeM = lc->env->GetMethodID(listCls, "size", "()I");
                    jmethodID getM  = lc->env->GetMethodID(listCls, "get",  "(I)Ljava/lang/Object;");
                    if (sizeM && getM)
                    {
                        jint n = lc->env->CallIntMethod(siblings, sizeM);
                        // formatNameForTeam appends the bare `name` component
                        // by reference as one of the siblings — its identity
                        // is preserved, so IsSameObject picks it out cleanly
                        // and everything else (prefix/suffix) gets dimmed.
                        jobject nameInst = nameC.GetInstance();
                        for (jint i = 0; i < n; ++i)
                        {
                            jobject sib = lc->env->CallObjectMethod(siblings, getM, i);
                            if (!sib) continue;
                            uint32_t col = extractColor(sib, rootColor);
                            const bool isName = lc->env->IsSameObject(sib, nameInst);
                            if (!isName) col = darkenRGB(col, 0.65f);
                            Component sibC(sib);
                            std::string s = sibC.getString();
                            lc->env->DeleteLocalRef(sib);
                            if (!s.empty()) chunks.emplace_back(std::move(s), col);
                        }
                    }
                    lc->env->DeleteLocalRef(listCls);
                }
                lc->env->DeleteLocalRef(siblings);
            }
        }
        else { lc->env->ExceptionClear(); }
    }

    // No siblings (or all empty) — fall back to the whole component as one chunk.
    if (chunks.empty())
    {
        Component rootC(root);
        std::string s = rootC.getString();
        if (!s.empty()) chunks.emplace_back(std::move(s), rootColor);
    }

    if (formatted) lc->env->DeleteLocalRef(formatted);
    return chunks;
}

// 1.21.11+: Entity.x/y/z no longer exist as primitive fields; position is a
// Vec3 reference on Entity. Each accessor reads the Vec3 and pulls a coord.
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

bool Entity::setGlowingTag(bool glowing)
{
    // Direct field write to Entity.hasGlowingTag (a local-only boolean). Avoids
    // the setter method entirely so anything that method does beyond the field
    // assignment (e.g. setting the synced flag bit) is bypassed. isCurrentlyGlowing
    // ORs hasGlowingTag with the synced flag, so writing this field alone is
    // sufficient on the client side.
    jfieldID f = lc->env->GetFieldID(this->GetClass(), FLD_Entity_hasGlowingTag, "Z");
    if (!f) { lc->env->ExceptionClear(); return false; }
    lc->env->SetBooleanField(this->instance, f, (jboolean)glowing);
    if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }
    return true;
}
