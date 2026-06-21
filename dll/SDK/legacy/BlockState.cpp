#include "BlockState.h"
#include "Mappings.h"

bool BlockState::blocksMotion()
{
    if (!instance) return false;
    static jmethodID getBlock = nullptr;
    if (!JMethod(getBlock, this->GetClass(), MTD_BlockState_getBlock, DESC_BlockState_getBlock)) { lc->env->ExceptionClear(); return false; }
    jobject block = lc->env->CallObjectMethod(this->instance, getBlock);
    if (!block || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }

    jclass blockCls = lc->env->GetObjectClass(block);
    static jmethodID getMaterial = nullptr;
    JMethod(getMaterial, blockCls, MTD_Block_getMaterial, DESC_Block_getMaterial);
    lc->env->DeleteLocalRef(blockCls);
    if (!getMaterial) { lc->env->ExceptionClear(); lc->env->DeleteLocalRef(block); return false; }
    jobject material = lc->env->CallObjectMethod(block, getMaterial);
    lc->env->DeleteLocalRef(block);
    if (!material || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return false; }

    jclass matCls = lc->env->GetObjectClass(material);
    static jmethodID blocksMovement = nullptr;
    JMethod(blocksMovement, matCls, MTD_Material_blocksMovement, "()Z");
    lc->env->DeleteLocalRef(matCls);
    bool r = false;
    if (blocksMovement) {
        r = lc->env->CallBooleanMethod(material, blocksMovement) == JNI_TRUE;
        if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); r = false; }
    } else {
        lc->env->ExceptionClear();
    }
    lc->env->DeleteLocalRef(material);
    return r;
}
