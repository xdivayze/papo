#include <gtest/gtest.h>

#include "services/render/renderable_object.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "services/event/papo_event.hpp"
#include "services/services.hpp"
#include "services/time_manager.hpp"

namespace
{
    using Runtime::AbstractMoveableObject;
    using Runtime::RenderableObject;
    using Handle = Service::AssetManager::ModelHandle<Service::Model>;

    constexpr float kEps = 1e-4f;

    // RenderableObject only stores the handle (never dereferences it), so an
    // empty handle is sufficient and no Model / GL context is required.
    Handle nullHandle() { return Handle{UINT32_MAX, nullptr}; }

    // Build a LastFrameTimeUpdated event on the stack and publish it through
    // the global bus. The event carries no payload (matches TimeManager).
    void publishLastFrameTimeUpdated()
    {
        auto &bus = Engine::Root::get().getEventManager().getEventBus();
        PapoEvent::PapoEventHeader hdr{
            Service::TimeManager::LastFrameTimeUpdatedEventID,
            sizeof(PapoEvent::PapoEventHeader), 0};
        PapoEvent::PapoEventGeneric evt{&hdr, nullptr};
        bus.publish(&evt);
    }

    void expectVecNear(const glm::vec3 &a, const glm::vec3 &b)
    {
        EXPECT_NEAR(a.x, b.x, kEps);
        EXPECT_NEAR(a.y, b.y, kEps);
        EXPECT_NEAR(a.z, b.z, kEps);
    }

    void expectQuatNear(const glm::quat &a, const glm::quat &b)
    {
        EXPECT_NEAR(a.w, b.w, kEps);
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

    // Minimal concrete subclass so we can test the base class on its own.
    class MoveableProbe : public AbstractMoveableObject {};
} // namespace

// Friend of Service::TimeManager (see `friend class ::TimeManagerTest;` in
// time_manager.hpp). Drives the otherwise-private lastFramePeriod_ so we can
// verify the bus subscription pulls fresh values out of TimeManager.
class TimeManagerTest
{
public:
    static void setLastFramePeriodSeconds(Service::TimeManager &tm, float seconds)
    {
        tm.lastFramePeriod_ = Service::TimeManager::SecondsToCycles(seconds);
    }
};

// === AbstractMoveableObject =================================================

TEST(AbstractMoveableObjectTest, SetCoordinatesStoresValue)
{
    MoveableProbe obj;
    glm::vec3 pos(1.0f, -2.0f, 3.0f);
    obj.setCoordinates(pos);
    expectVecNear(obj.coordinates(), pos);
}

TEST(AbstractMoveableObjectTest, SetRotationEulerSyncsQuat)
{
    MoveableProbe obj;
    glm::vec3 eul(0.2f, 0.5f, -0.1f);
    obj.setRotation(eul);
    expectVecNear(obj.rotation(), eul);
    expectQuatNear(obj.rotationQuat(), glm::quat(eul));
}

TEST(AbstractMoveableObjectTest, SetRotationQuatDerivesEuler)
{
    MoveableProbe obj;
    glm::quat q = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
    obj.setRotation(q);
    expectQuatNear(obj.rotationQuat(), q);
    expectVecNear(obj.rotation(), glm::eulerAngles(q));
}

TEST(AbstractMoveableObjectTest, SetScalingStoresValue)
{
    MoveableProbe obj;
    glm::vec3 scl(2.0f, 0.5f, 3.0f);
    obj.setScaling(scl);
    expectVecNear(obj.scaling(), scl);
}

TEST(AbstractMoveableObjectTest, RotateLocalComposesAndNormalises)
{
    MoveableProbe obj;
    glm::quat start = glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 1, 0));
    obj.setRotation(start);

    glm::vec3 deltaEul(0.0f, glm::radians(60.0f), 0.0f);
    obj.rotateLocal(deltaEul);

    glm::quat expected = glm::normalize(start * glm::quat(deltaEul));
    expectQuatNear(obj.rotationQuat(), expected);
    EXPECT_NEAR(glm::length(obj.rotationQuat()), 1.0f, kEps);
}

TEST(AbstractMoveableObjectTest, RotateWorldPreMultiplies)
{
    MoveableProbe obj;
    glm::quat start = glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 1, 0));
    obj.setRotation(start);

    glm::vec3 deltaEul(0.0f, glm::radians(60.0f), 0.0f);
    obj.rotateWorld(deltaEul);

    glm::quat expected = glm::normalize(glm::quat(deltaEul) * start);
    expectQuatNear(obj.rotationQuat(), expected);
}

TEST(AbstractMoveableObjectTest, SetAndGetDeltaTime)
{
    MoveableProbe obj;
    EXPECT_NEAR(obj.deltaTime(), 0.0f, kEps);
    obj.setDeltaTime(0.016f);
    EXPECT_NEAR(obj.deltaTime(), 0.016f, kEps);
}

TEST(AbstractMoveableObjectTest, StepTranslatesBySpeedTimesDt)
{
    MoveableProbe obj;
    obj.setCoordinates(glm::vec3(1.0f, 2.0f, 3.0f));
    obj.setDeltaTime(0.5f);

    obj.step(glm::vec3(2.0f, 0.0f, -4.0f));

    expectVecNear(obj.coordinates(), glm::vec3(2.0f, 2.0f, 1.0f));
}

TEST(AbstractMoveableObjectTest, StepWithZeroDtIsNoOp)
{
    MoveableProbe obj;
    obj.setCoordinates(glm::vec3(5.0f, 5.0f, 5.0f));
    // deltaTime defaults to 0
    obj.step(glm::vec3(100.0f, 100.0f, 100.0f));
    expectVecNear(obj.coordinates(), glm::vec3(5.0f, 5.0f, 5.0f));
}

TEST(AbstractMoveableObjectTest, RotateStepWorldAdvancesByAngularSpeedTimesDt)
{
    MoveableProbe obj;
    obj.setDeltaTime(0.25f);

    glm::vec3 axis(0.0f, 1.0f, 0.0f);
    float angularSpeed = glm::radians(360.0f); // 1 turn / sec
    obj.rotateStepWorld(axis, angularSpeed);

    // 0.25s * 360 deg/s = 90 deg around world Y.
    glm::quat expected = glm::normalize(
        glm::angleAxis(angularSpeed * 0.25f, axis) * glm::quat(glm::vec3(0.0f)));
    expectQuatNear(obj.rotationQuat(), expected);
    // No orbit: coordinates unchanged.
    expectVecNear(obj.coordinates(), glm::vec3(0.0f));
}

TEST(AbstractMoveableObjectTest, RotateAroundPivotOrbitsAndRotates)
{
    MoveableProbe obj;
    obj.setCoordinates(glm::vec3(1.0f, 0.0f, 0.0f));

    // 90deg around world Y, pivoting at origin: (1,0,0) -> (0,0,-1).
    glm::vec3 pivot(0.0f);
    glm::vec3 eul(0.0f, glm::radians(90.0f), 0.0f);
    obj.rotateAroundPivot(pivot, eul);

    expectVecNear(obj.coordinates(), glm::vec3(0.0f, 0.0f, -1.0f));
    glm::quat expectedRot = glm::normalize(glm::quat(eul));
    expectQuatNear(obj.rotationQuat(), expectedRot);
}

TEST(AbstractMoveableObjectTest, RotateAroundPivotOffOriginOrbitsAroundPivot)
{
    MoveableProbe obj;
    obj.setCoordinates(glm::vec3(3.0f, 0.0f, 0.0f));

    // 180deg around Y pivoting at (1,0,0): (3,0,0) -> (-1,0,0).
    glm::vec3 pivot(1.0f, 0.0f, 0.0f);
    glm::vec3 eul(0.0f, glm::radians(180.0f), 0.0f);
    obj.rotateAroundPivot(pivot, eul);

    expectVecNear(obj.coordinates(), glm::vec3(-1.0f, 0.0f, 0.0f));
}

TEST(AbstractMoveableObjectTest, RotateStepAroundPivotUsesDeltaTime)
{
    MoveableProbe obj;
    obj.setCoordinates(glm::vec3(1.0f, 0.0f, 0.0f));
    obj.setDeltaTime(0.5f);

    glm::vec3 pivot(0.0f);
    glm::vec3 axis(0.0f, 1.0f, 0.0f);
    float angularSpeed = glm::radians(180.0f); // half-turn / sec
    // 0.5s * 180 = 90deg around Y at origin: (1,0,0) -> (0,0,-1).
    obj.rotateStepAroundPivot(pivot, axis, angularSpeed);

    expectVecNear(obj.coordinates(), glm::vec3(0.0f, 0.0f, -1.0f));
    glm::quat expectedRot =
        glm::normalize(glm::angleAxis(angularSpeed * 0.5f, axis));
    expectQuatNear(obj.rotationQuat(), expectedRot);
}

// === Event-bus subscription ================================================

TEST(AbstractMoveableObjectTest, CtorSubscribesAndPullsDtFromTimeManager)
{
    auto &tm = Engine::Root::get().getTimeManager();
    TimeManagerTest::setLastFramePeriodSeconds(tm, 0.016f);

    MoveableProbe obj;
    EXPECT_NEAR(obj.deltaTime(), 0.0f, kEps); // default until first event fires
    publishLastFrameTimeUpdated();
    EXPECT_NEAR(obj.deltaTime(), 0.016f, kEps);

    TimeManagerTest::setLastFramePeriodSeconds(tm, 0.033f);
    publishLastFrameTimeUpdated();
    EXPECT_NEAR(obj.deltaTime(), 0.033f, kEps);
}

TEST(AbstractMoveableObjectTest, DestructionUnsubscribesQuietly)
{
    auto &tm = Engine::Root::get().getTimeManager();
    TimeManagerTest::setLastFramePeriodSeconds(tm, 0.1f);
    {
        MoveableProbe obj;
        publishLastFrameTimeUpdated();
        ASSERT_NEAR(obj.deltaTime(), 0.1f, kEps);
    }
    // The dtor must have unsubscribed: publishing now reaches no live listener.
    EXPECT_NO_FATAL_FAILURE(publishLastFrameTimeUpdated());
}

TEST(RenderableObjectTest, SubscribesAndStepConsumesDtFromTimeManager)
{
    auto &tm = Engine::Root::get().getTimeManager();
    TimeManagerTest::setLastFramePeriodSeconds(tm, 0.025f);

    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    publishLastFrameTimeUpdated();
    EXPECT_NEAR(obj.deltaTime(), 0.025f, kEps);

    obj.step(glm::vec3(2.0f, 0.0f, 0.0f));
    expectVecNear(obj.coordinates(), glm::vec3(0.05f, 0.0f, 0.0f));
}

// Verify the derived RenderableObject's transform refreshes when the inherited
// step() runs (it routes through the virtual setCoordinates override).
TEST(RenderableObjectTest, StepUpdatesTransformThroughOverride)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    obj.setDeltaTime(2.0f);
    obj.step(glm::vec3(1.0f, 0.0f, 0.0f));

    expectVecNear(obj.coordinates(), glm::vec3(2.0f, 0.0f, 0.0f));
    expectMatNear(obj.getTransform(),
                  expectedTransform(glm::vec3(2.0f, 0.0f, 0.0f),
                                    glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f)));
}

// === RenderableObject =======================================================

TEST(RenderableObjectTest, EulerCtorStoresStateAndSyncsQuat)
{
    glm::vec3 pos(1.0f, 2.0f, 3.0f);
    glm::vec3 eul(0.1f, 0.2f, 0.3f);
    glm::vec3 scl(2.0f, 2.0f, 2.0f);

    RenderableObject obj(nullHandle(), pos, eul, scl);

    expectVecNear(obj.coordinates(), pos);
    expectVecNear(obj.rotation(), eul);
    expectVecNear(obj.scaling(), scl);
    glm::quat expected(eul);
    expectQuatNear(obj.rotationQuat(), expected);
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
    expectQuatNear(obj.rotationQuat(), rot);
    expectVecNear(obj.scaling(), scl);
    expectMatNear(obj.getTransform(), expectedTransform(pos, rot, scl));
}

// The 5-arg ctor takes both Euler and quat; current behavior treats the quat
// as authoritative (transform built from the quat, Euler is derived from it).
TEST(RenderableObjectTest, EulerAndQuatCtorTreatsQuatAsAuthoritative)
{
    glm::vec3 pos(0.0f, 0.0f, 0.0f);
    glm::vec3 ignoredEul(0.0f, 0.0f, 0.0f);
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0));
    glm::vec3 scl(1.0f, 1.0f, 1.0f);

    RenderableObject obj(nullHandle(), pos, ignoredEul, rot, scl);

    expectQuatNear(obj.rotationQuat(), rot);
    expectVecNear(obj.rotation(), glm::eulerAngles(rot));
    expectMatNear(obj.getTransform(), expectedTransform(pos, rot, scl));
}

TEST(RenderableObjectTest, IdentityStateYieldsIdentityTransform)
{
    RenderableObject obj(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                         glm::vec3(1.0f));
    expectMatNear(obj.getTransform(), glm::mat4(1.0f));
}

TEST(RenderableObjectTest, HandleOnlyCtorDefaultsToIdentity)
{
    RenderableObject obj(nullHandle());

    expectVecNear(obj.coordinates(), glm::vec3(0.0f));
    expectVecNear(obj.rotation(), glm::vec3(0.0f));
    expectQuatNear(obj.rotationQuat(), glm::quat(glm::vec3(0.0f)));
    expectVecNear(obj.scaling(), glm::vec3(1.0f));
    expectMatNear(obj.getTransform(), glm::mat4(1.0f));
}

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
    expectQuatNear(obj.rotationQuat(), q);
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
    expectQuatNear(obj.rotationQuat(), q);
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

// Tests polymorphic destruction through the base pointer (now safe because
// AbstractMoveableObject has a virtual destructor).
TEST(RenderableObjectTest, DestructibleThroughAbstractBase)
{
    AbstractMoveableObject *obj =
        new RenderableObject(nullHandle(), glm::vec3(0.0f), glm::vec3(0.0f),
                             glm::vec3(1.0f));
    EXPECT_NO_FATAL_FAILURE(delete obj);
}
