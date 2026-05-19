#include <gtest/gtest.h>

#include "services/render/renderable_object.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace
{
    using Service::RenderableObject;
    using Handle = Service::AssetManager::ModelHandle<Service::Model>;

    constexpr float kEps = 1e-4f;

    // RenderableObject only stores the handle (never dereferences it), so an
    // empty handle is sufficient and no Model / GL context is required.
    Handle nullHandle() { return Handle{UINT32_MAX, nullptr}; }

    void expectVecNear(const glm::vec3 &a, const glm::vec3 &b)
    {
        EXPECT_NEAR(a.x, b.x, kEps);
        EXPECT_NEAR(a.y, b.y, kEps);
        EXPECT_NEAR(a.z, b.z, kEps);
    }

    void expectMatNear(const glm::mat4 &a, const glm::mat4 &b)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                EXPECT_NEAR(a[c][r], b[c][r], kEps) << "mismatch at [" << c << "][" << r << "]";
    }

    glm::mat4 expectedTransform(const glm::vec3 &pos, const glm::quat &rot,
                                const glm::vec3 &scale)
    {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 R = glm::mat4_cast(rot);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        return T * R * S;
    }
} // namespace

// --- construction ---

TEST(RenderableObjectTest, EulerCtorStoresStateAndSyncsQuat)
{
    glm::vec3 pos(1.0f, 2.0f, 3.0f);
    glm::vec3 eul(0.1f, 0.2f, 0.3f);
    glm::vec3 scl(2.0f, 2.0f, 2.0f);

    RenderableObject obj(nullHandle(), pos, eul, scl);

    expectVecNear(obj.coordinates(), pos);
    expectVecNear(obj.rotation(), eul);
    expectVecNear(obj.scaling(), scl);
    // Euler ctor must derive a matching quaternion.
    glm::quat expected(eul);
    EXPECT_NEAR(obj.rotationQuat().w, expected.w, kEps);
    EXPECT_NEAR(obj.rotationQuat().x, expected.x, kEps);
    EXPECT_NEAR(obj.rotationQuat().y, expected.y, kEps);
    EXPECT_NEAR(obj.rotationQuat().z, expected.z, kEps);
    expectMatNear(obj.getTransform(), expectedTransform(pos, expected, scl));
}

TEST(RenderableObjectTest, QuatCtorStoresStateAndDerivesEuler)
{
    glm::vec3 pos(-1.0f, 0.5f, 4.0f);
    glm::quat rot = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
    glm::vec3 scl(1.0f, 3.0f, 1.0f);

    RenderableObject obj(nullHandle(), pos, rot, scl);

    expectVecNear(obj.coordinates(), pos);
    expectVecNear(obj.rotation(), glm::eulerAngles(rot));
    expectVecNear(obj.scaling(), scl);
    expectMatNear(obj.getTransform(), expectedTransform(pos, rot, scl));
}

TEST(RenderableObjectTest, IdentityStateYieldsIdentityTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    expectMatNear(obj.getTransform(), glm::mat4(1.0f));
}

// --- setters re-derive the transform ---

TEST(RenderableObjectTest, SetCoordinatesUpdatesTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    glm::vec3 pos(5.0f, -2.0f, 7.0f);
    obj.setCoordinates(pos);

    expectVecNear(obj.coordinates(), pos);
    expectMatNear(obj.getTransform(),
                  expectedTransform(pos, glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f)));
}

TEST(RenderableObjectTest, SetRotationEulerSyncsQuatAndTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    glm::vec3 eul(0.4f, -0.2f, 0.6f);
    obj.setRotation(eul);

    expectVecNear(obj.rotation(), eul);
    glm::quat q(eul);
    EXPECT_NEAR(obj.rotationQuat().w, q.w, kEps);
    expectMatNear(obj.getTransform(),
                  expectedTransform(glm::vec3(0.0f), q, glm::vec3(1.0f)));
}

TEST(RenderableObjectTest, SetRotationQuatDerivesEulerAndTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    glm::quat q = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0));
    obj.setRotation(q);

    expectVecNear(obj.rotation(), glm::eulerAngles(q));
    expectMatNear(obj.getTransform(),
                  expectedTransform(glm::vec3(0.0f), q, glm::vec3(1.0f)));
}

TEST(RenderableObjectTest, SetScalingUpdatesTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    glm::vec3 scl(3.0f, 0.5f, 2.0f);
    obj.setScaling(scl);

    expectVecNear(obj.scaling(), scl);
    expectMatNear(obj.getTransform(),
                  expectedTransform(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), scl));
}

// --- transform composition order is T * R * S ---

TEST(RenderableObjectTest, TransformAppliesScaleThenRotateThenTranslate)
{
    glm::vec3 pos(10.0f, 0.0f, 0.0f);
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
    glm::vec3 scl(2.0f, 2.0f, 2.0f);

    RenderableObject obj(nullHandle(), pos, rot, scl);

    // (1,0,0) -> scale*2 -> (2,0,0) -> rot 90deg z -> (0,2,0) -> +pos
    glm::vec4 p = obj.getTransform() * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    expectVecNear(glm::vec3(p), glm::vec3(10.0f, 2.0f, 0.0f));
}

// --- virtual destructor: deleting through a base pointer is well-defined ---

TEST(RenderableObjectTest, DestructibleThroughBasePointer)
{
    RenderableObject *obj =
        new RenderableObject(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                             glm::vec3(1.0f));
    EXPECT_NO_FATAL_FAILURE(delete obj);
}
