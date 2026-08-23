#include "ParticleEngine.h"
#include "Minecraft.h"
#include "Mappings.h"

namespace Particles
{
    static bool s_mapped()
    {
        return MC_ParticleEngine[0] != '\0'
            && MC_Particle[0]       != '\0'
            && FLD_Particle_x[0]    != '\0';
    }

    bool Supported() { return s_mapped(); }

    static jclass gcls(jclass& slot, const char* name)
    {
        if (!slot)
        {
            jclass local = lc->env->FindClass(name);
            if (!local) { lc->env->ExceptionClear(); return nullptr; }
            slot = (jclass)lc->env->NewGlobalRef(local);
            lc->env->DeleteLocalRef(local);
        }
        return slot;
    }

    void CollectSmoke(std::vector<Point>& out,
                      double camX, double camY, double camZ,
                      double maxDist, size_t cap)
    {
        if (!s_mapped()) return;

        JLocalFrame frame(64);
        if (!frame.ok()) return;

        const double maxDistSq = maxDist * maxDist;

        Minecraft mc;
        jobject mcInst = mc.GetInstance();
        jclass  mcCls  = mc.GetClass();
        if (mcInst == nullptr || mcCls == nullptr) return;

        static jfieldID peField = nullptr;
        JField(peField, mcCls, FLD_Minecraft_particleEngine, DESC_Minecraft_particleEngine);
        if (!peField) return;

        jobject peInst = lc->env->GetObjectField(mcInst, peField);
        if (!peInst || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return; }

        static jclass peCls = nullptr;
        if (!peCls) { peCls = (jclass)lc->env->NewGlobalRef(lc->env->GetObjectClass(peInst)); }

        static jfieldID partField = nullptr;
        JField(partField, peCls, FLD_ParticleEngine_particles, DESC_ParticleEngine_particles);
        if (!partField) return;

        jobject mapObj = lc->env->GetObjectField(peInst, partField);
        if (!mapObj || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return; }

        static jclass    mapCls = nullptr, colCls = nullptr, itCls = nullptr;
        static jmethodID mapValues = nullptr, colIter = nullptr, itHasNext = nullptr, itNext = nullptr;
        if (!gcls(mapCls, "java/util/Map"))        return;
        if (!gcls(colCls, "java/util/Collection")) return;
        if (!gcls(itCls,  "java/util/Iterator"))   return;
        JMethod(mapValues, mapCls, "values",  "()Ljava/util/Collection;");
        JMethod(colIter,   colCls, "iterator","()Ljava/util/Iterator;");
        JMethod(itHasNext, itCls,  "hasNext", "()Z");
        JMethod(itNext,    itCls,  "next",    "()Ljava/lang/Object;");
        if (!mapValues || !colIter || !itHasNext || !itNext) return;

        static jclass    grpCls = nullptr;
        static jmethodID grpGetAll = nullptr;
        JClass(grpCls, MC_ParticleGroup);
        if (grpCls) JMethod(grpGetAll, grpCls, MTD_ParticleGroup_getAll, DESC_ParticleGroup_getAll);

        static jclass    partCls = nullptr;
        static jfieldID  fx = nullptr, fy = nullptr, fz = nullptr;
        JClass(partCls, MC_Particle);
        if (!partCls) return;
        JField(fx, partCls, FLD_Particle_x, "D");
        JField(fy, partCls, FLD_Particle_y, "D");
        JField(fz, partCls, FLD_Particle_z, "D");
        if (!fx || !fy || !fz) return;

        static jclass smoke[4] = { nullptr, nullptr, nullptr, nullptr };
        JClass(smoke[0], MC_BaseAshSmokeParticle);
        JClass(smoke[1], MC_SmokeParticle);
        JClass(smoke[2], MC_LargeSmokeParticle);
        JClass(smoke[3], MC_CampfireSmokeParticle);

        jobject groups = lc->env->CallObjectMethod(mapObj, mapValues);
        if (!groups || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return; }

        jobject gIt = lc->env->CallObjectMethod(groups, colIter);
        lc->env->DeleteLocalRef(groups);
        if (!gIt || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); return; }

        while (out.size() < cap && lc->env->CallBooleanMethod(gIt, itHasNext) == JNI_TRUE)
        {
            if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); break; }
            jobject group = lc->env->CallObjectMethod(gIt, itNext);
            if (lc->env->ExceptionCheck() || !group) { lc->env->ExceptionClear(); if (group) lc->env->DeleteLocalRef(group); break; }

            jobject queue = nullptr;
            if (grpGetAll && grpCls && lc->env->IsInstanceOf(group, grpCls) == JNI_TRUE)
                queue = lc->env->CallObjectMethod(group, grpGetAll);
            lc->env->DeleteLocalRef(group);
            if (lc->env->ExceptionCheck() || !queue) { lc->env->ExceptionClear(); if (queue) lc->env->DeleteLocalRef(queue); continue; }

            jobject pIt = lc->env->CallObjectMethod(queue, colIter);
            lc->env->DeleteLocalRef(queue);
            if (!pIt || lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); if (pIt) lc->env->DeleteLocalRef(pIt); continue; }

            while (out.size() < cap && lc->env->CallBooleanMethod(pIt, itHasNext) == JNI_TRUE)
            {
                if (lc->env->ExceptionCheck()) { lc->env->ExceptionClear(); break; }
                jobject part = lc->env->CallObjectMethod(pIt, itNext);
                if (lc->env->ExceptionCheck() || !part) { lc->env->ExceptionClear(); if (part) lc->env->DeleteLocalRef(part); break; }

                const double px = lc->env->GetDoubleField(part, fx);
                const double py = lc->env->GetDoubleField(part, fy);
                const double pz = lc->env->GetDoubleField(part, fz);

                const double dx = px - camX, dy = py - camY, dz = pz - camZ;
                if (dx*dx + dy*dy + dz*dz > maxDistSq) { lc->env->DeleteLocalRef(part); continue; }

                bool isSmoke = false;
                for (int i = 0; i < 4 && !isSmoke; ++i)
                    if (smoke[i] && lc->env->IsInstanceOf(part, smoke[i]) == JNI_TRUE)
                        isSmoke = true;

                if (isSmoke) out.push_back({ px, py, pz });
                lc->env->DeleteLocalRef(part);
            }
            lc->env->DeleteLocalRef(pIt);
            if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
        }
        lc->env->DeleteLocalRef(gIt);
        if (lc->env->ExceptionCheck()) lc->env->ExceptionClear();
    }
}
