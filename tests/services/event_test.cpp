#include <gtest/gtest.h>
#include "services/event/event_queue.hpp"
#include "services/event/papo_event.hpp"
#include "services/event/papo_event_manager.hpp"

// --- trivially copyable test event types ---
struct PositionEvent { float x, y; };
struct CounterEvent  { int count; };
static_assert(std::is_trivially_copyable_v<PositionEvent>);
static_assert(std::is_trivially_copyable_v<CounterEvent>);

static constexpr PapoEvent::PapoEventTypeID EVT_POSITION = 1;
static constexpr PapoEvent::PapoEventTypeID EVT_COUNTER   = 2;

// Walk to the Nth (0-based) [header, payload] pair in the buffer.
// Safe as long as alignof(T) <= alignof(PapoEventHeader) (both <= 8 here).
static std::pair<const PapoEvent::PapoEventHeader *, const void *>
readEvent(std::byte *base, size_t n)
{
    constexpr uintptr_t hdrAlign = alignof(PapoEvent::PapoEventHeader);
    auto *cursor = base;
    for (size_t i = 0; i < n; ++i)
    {
        auto *hdr = reinterpret_cast<const PapoEvent::PapoEventHeader *>(cursor);
        auto raw = reinterpret_cast<uintptr_t>(cursor) + hdr->headerSize_ + hdr->payloadSize;
        cursor = reinterpret_cast<std::byte *>((raw + hdrAlign - 1) & ~(hdrAlign - 1));
    }
    auto *hdr = reinterpret_cast<const PapoEvent::PapoEventHeader *>(cursor);
    return {hdr, cursor + hdr->headerSize_};
}

// ============================================================
// EventQueue
// ============================================================

class EventQueueTest : public ::testing::Test
{
protected:
    static constexpr size_t CAPACITY = 4096;
    PapoEvent::EventQueue queue_{CAPACITY};
};

// --- push (write-handle overload) ---

TEST_F(EventQueueTest, PushWriteHandleReturnsNonNull)
{
    EXPECT_NE(queue_.push<PositionEvent>(EVT_POSITION), nullptr);
}

TEST_F(EventQueueTest, PushWriteHandleHeaderHasCorrectType)
{
    queue_.push<PositionEvent>(EVT_POSITION);
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_POSITION);
}

TEST_F(EventQueueTest, PushWriteHandleHeaderHasCorrectSizes)
{
    queue_.push<PositionEvent>(EVT_POSITION);
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->headerSize_, sizeof(PapoEvent::PapoEventHeader));
    EXPECT_EQ(hdr->payloadSize, sizeof(PositionEvent));
}

TEST_F(EventQueueTest, PushWriteHandlePayloadIsWritable)
{
    auto *pos = queue_.push<PositionEvent>(EVT_POSITION);
    pos->x = 1.5f;
    pos->y = 2.5f;
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    auto *p = static_cast<const PositionEvent *>(payload);
    EXPECT_FLOAT_EQ(p->x, 1.5f);
    EXPECT_FLOAT_EQ(p->y, 2.5f);
}

// --- push (by-value overload) ---

TEST_F(EventQueueTest, PushByValueSetsCorrectHeaderType)
{
    queue_.push(EVT_COUNTER, CounterEvent{7});
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_COUNTER);
}

TEST_F(EventQueueTest, PushByValueCopiesPayload)
{
    queue_.push(EVT_POSITION, PositionEvent{3.14f, 2.71f});
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    auto *p = static_cast<const PositionEvent *>(payload);
    EXPECT_FLOAT_EQ(p->x, 3.14f);
    EXPECT_FLOAT_EQ(p->y, 2.71f);
}

TEST_F(EventQueueTest, PushByValueSetsCorrectPayloadSize)
{
    queue_.push(EVT_COUNTER, CounterEvent{0});
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->payloadSize, sizeof(CounterEvent));
}

// --- multiple pushes ---

TEST_F(EventQueueTest, MultiplePushesStoredInOrder)
{
    queue_.push(EVT_POSITION, PositionEvent{1.f, 2.f});
    queue_.push(EVT_COUNTER,  CounterEvent{99});
    queue_.endFrame();

    auto *base = queue_.getReadHandle();
    auto [h0, p0] = readEvent(base, 0);
    auto [h1, p1] = readEvent(base, 1);

    EXPECT_EQ(h0->type_, EVT_POSITION);
    EXPECT_EQ(h1->type_, EVT_COUNTER);
    EXPECT_FLOAT_EQ(static_cast<const PositionEvent *>(p0)->x, 1.f);
    EXPECT_EQ(static_cast<const CounterEvent *>(p1)->count, 99);
}

TEST_F(EventQueueTest, ManyPushesAllRetainCorrectData)
{
    constexpr int N = 10;
    for (int i = 0; i < N; ++i)
        queue_.push(EVT_COUNTER, CounterEvent{i});
    queue_.endFrame();

    auto *base = queue_.getReadHandle();
    for (int i = 0; i < N; ++i)
    {
        auto [hdr, payload] = readEvent(base, i);
        EXPECT_EQ(static_cast<const CounterEvent *>(payload)->count, i);
    }
}

// --- endFrame sentinel ---

TEST_F(EventQueueTest, EndFrameWritesSentinelAfterEvents)
{
    queue_.push(EVT_POSITION, PositionEvent{1.f, 2.f});
    queue_.push(EVT_COUNTER,  CounterEvent{7});
    queue_.endFrame();

    // sentinel is the third entry (index 2)
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 2);
    EXPECT_EQ(hdr->type_, PapoEvent::PapoEventQueueEnd);
    EXPECT_EQ(hdr->payloadSize, 0u);
}

TEST_F(EventQueueTest, EndFrameWritesSentinelOnEmptyQueue)
{
    queue_.endFrame();

    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, PapoEvent::PapoEventQueueEnd);
    EXPECT_EQ(hdr->payloadSize, 0u);
}

// --- endFrame / buffer swap ---

TEST_F(EventQueueTest, EndFrameMovesDataToReadBuffer)
{
    queue_.push(EVT_COUNTER, CounterEvent{42});
    queue_.endFrame();
    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_COUNTER);
}

TEST_F(EventQueueTest, WritesAfterEndFrameDoNotCorruptReadBuffer)
{
    queue_.push(EVT_POSITION, PositionEvent{5.f, 6.f});
    queue_.endFrame();

    // write into the new write buffer — read buffer must be untouched
    queue_.push(EVT_COUNTER, CounterEvent{99});

    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_POSITION);
}

TEST_F(EventQueueTest, DoubleEndFrameMakesSecondFrameReadable)
{
    queue_.push(EVT_POSITION, PositionEvent{});
    queue_.endFrame();

    queue_.push(EVT_COUNTER, CounterEvent{7});
    queue_.endFrame();

    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_COUNTER);
    EXPECT_EQ(static_cast<const CounterEvent *>(payload)->count, 7);
}

TEST_F(EventQueueTest, AfterDoubleEndFrameOldWriteBufferIsReset)
{
    queue_.push(EVT_POSITION, PositionEvent{1.f, 2.f});
    queue_.endFrame();  // buffer A → read
    queue_.push(EVT_COUNTER, CounterEvent{1});
    queue_.endFrame();  // buffer B → read, buffer A reset

    // push into the freshly-reset buffer A — it should have room from offset 0
    queue_.push(EVT_POSITION, PositionEvent{9.f, 9.f});
    queue_.endFrame();

    auto [hdr, payload] = readEvent(queue_.getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_POSITION);
    EXPECT_FLOAT_EQ(static_cast<const PositionEvent *>(payload)->x, 9.f);
}

// ============================================================
// EventBus
// ============================================================

struct MockListener : public PapoEvent::IPapoEventListener
{
    int callCount = 0;
    void *lastPayload = nullptr;

    void eventCall(void *payload) override
    {
        ++callCount;
        lastPayload = payload;
    }
};

// Helper: build a generic event on the stack and publish it.
static void publishOn(PapoEvent::EventBus &bus,
                      PapoEvent::PapoEventTypeID type,
                      void *payload)
{
    PapoEvent::PapoEventHeader hdr{type, sizeof(PapoEvent::PapoEventHeader), 0};
    PapoEvent::PapoEventGeneric evt{&hdr, payload};
    bus.publish(&evt);
}

class EventBusTest : public ::testing::Test
{
protected:
    PapoEvent::EventBus bus;
};

TEST_F(EventBusTest, PublishWithNoListenersDoesNotCrash)
{
    PositionEvent p{};
    EXPECT_NO_THROW(publishOn(bus, EVT_POSITION, &p));
}

TEST_F(EventBusTest, PublishCallsMatchingListener)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(EventBusTest, PublishPassesCorrectPayloadPointer)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);

    PositionEvent p{3.f, 4.f};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.lastPayload, &p);
}

TEST_F(EventBusTest, PublishDoesNotCallListenerForDifferentType)
{
    MockListener listener;
    bus.subscribe(EVT_COUNTER, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(EventBusTest, PublishCallsAllListenersForSameType)
{
    MockListener a, b;
    bus.subscribe(EVT_POSITION, a);
    bus.subscribe(EVT_POSITION, b);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(a.callCount, 1);
    EXPECT_EQ(b.callCount, 1);
}

TEST_F(EventBusTest, PublishOnlyCallsListenersForMatchingTypeAmongMultiple)
{
    MockListener posListener, ctrListener;
    bus.subscribe(EVT_POSITION, posListener);
    bus.subscribe(EVT_COUNTER,  ctrListener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(posListener.callCount, 1);
    EXPECT_EQ(ctrListener.callCount, 0);
}

TEST_F(EventBusTest, SubscribeSameListenerTwiceCallsItTwice)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);
    bus.subscribe(EVT_POSITION, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 2);
}

TEST_F(EventBusTest, UnsubscribeStopsListenerFromBeingCalled)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);
    bus.unsubscribe(EVT_POSITION, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(EventBusTest, UnsubscribeLeavesOtherListenersIntact)
{
    MockListener a, b;
    bus.subscribe(EVT_POSITION, a);
    bus.subscribe(EVT_POSITION, b);
    bus.unsubscribe(EVT_POSITION, a);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(a.callCount, 0);
    EXPECT_EQ(b.callCount, 1);
}

TEST_F(EventBusTest, UnsubscribeForDifferentTypeDoesNotRemoveListener)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);
    bus.unsubscribe(EVT_COUNTER, listener);  // wrong type — should be a no-op

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(EventBusTest, UnsubscribeNotRegisteredListenerDoesNotCrash)
{
    MockListener listener;
    EXPECT_NO_THROW(bus.unsubscribe(EVT_POSITION, listener));
}

TEST_F(EventBusTest, UnsubscribeRemovesAllInstancesOfDuplicateSubscription)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);
    bus.subscribe(EVT_POSITION, listener);
    bus.unsubscribe(EVT_POSITION, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(EventBusTest, PublishMultipleEventsCallsListenerEachTime)
{
    MockListener listener;
    bus.subscribe(EVT_POSITION, listener);

    PositionEvent p{};
    publishOn(bus, EVT_POSITION, &p);
    publishOn(bus, EVT_POSITION, &p);
    publishOn(bus, EVT_POSITION, &p);

    EXPECT_EQ(listener.callCount, 3);
}

// ============================================================
// PapoEventManager
// ============================================================
// Note: queue_.endFrame() is called via friendship to move events into the
// read buffer before publishAll() can consume them. bus_.subscribe() is also
// accessed via friendship since PapoEventManager has no public subscribe proxy.

class PapoEventManagerTest : public ::testing::Test
{
protected:
    PapoEvent::PapoEventManager manager_;

    void endFrame() { manager_.queue_.endFrame(); }
    std::byte *getReadHandle() { return manager_.queue_.getReadHandle(); }
    void subscribe(PapoEvent::PapoEventTypeID id, PapoEvent::IPapoEventListener &l)
    {
        manager_.bus_.subscribe(id, l);
    }
};

// --- getEventWriteHandle ---

TEST_F(PapoEventManagerTest, GetEventWriteHandleReturnsNonNull)
{
    EXPECT_NE(manager_.getEventWriteHandle<PositionEvent>(EVT_POSITION), nullptr);
}

TEST_F(PapoEventManagerTest, GetEventWriteHandlePayloadIsWritable)
{
    auto *pos = manager_.getEventWriteHandle<PositionEvent>(EVT_POSITION);
    pos->x = 7.f;
    pos->y = 8.f;
    endFrame();

    auto [hdr, payload] = readEvent(getReadHandle(), 0);
    auto *p = static_cast<const PositionEvent *>(payload);
    EXPECT_FLOAT_EQ(p->x, 7.f);
    EXPECT_FLOAT_EQ(p->y, 8.f);
}

TEST_F(PapoEventManagerTest, GetEventWriteHandleSetsCorrectHeaderType)
{
    (void)manager_.getEventWriteHandle<PositionEvent>(EVT_POSITION);
    endFrame();

    auto [hdr, payload] = readEvent(getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_POSITION);
}

// --- pushEvent ---

TEST_F(PapoEventManagerTest, PushEventCopiesPayload)
{
    manager_.pushEvent(EVT_POSITION, PositionEvent{3.f, 4.f});
    endFrame();

    auto [hdr, payload] = readEvent(getReadHandle(), 0);
    auto *p = static_cast<const PositionEvent *>(payload);
    EXPECT_FLOAT_EQ(p->x, 3.f);
    EXPECT_FLOAT_EQ(p->y, 4.f);
}

TEST_F(PapoEventManagerTest, PushEventSetsCorrectHeaderType)
{
    manager_.pushEvent(EVT_COUNTER, CounterEvent{5});
    endFrame();

    auto [hdr, payload] = readEvent(getReadHandle(), 0);
    EXPECT_EQ(hdr->type_, EVT_COUNTER);
}

// --- publishAll ---

TEST_F(PapoEventManagerTest, PublishAllWithEmptyQueueDoesNotCrash)
{
    endFrame();
    EXPECT_NO_THROW(manager_.publishAll());
}

TEST_F(PapoEventManagerTest, PublishAllDispatchesToMatchingListener)
{
    MockListener listener;
    subscribe(EVT_POSITION, listener);

    manager_.pushEvent(EVT_POSITION, PositionEvent{});
    endFrame();
    manager_.publishAll();

    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(PapoEventManagerTest, PublishAllDoesNotCallListenerForDifferentType)
{
    MockListener listener;
    subscribe(EVT_COUNTER, listener);

    manager_.pushEvent(EVT_POSITION, PositionEvent{});
    endFrame();
    manager_.publishAll();

    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(PapoEventManagerTest, PublishAllDispatchesMultipleEventsToCorrectListeners)
{
    MockListener posListener, ctrListener;
    subscribe(EVT_POSITION, posListener);
    subscribe(EVT_COUNTER,  ctrListener);

    manager_.pushEvent(EVT_POSITION, PositionEvent{});
    manager_.pushEvent(EVT_COUNTER,  CounterEvent{1});
    manager_.pushEvent(EVT_POSITION, PositionEvent{});
    endFrame();
    manager_.publishAll();

    EXPECT_EQ(posListener.callCount, 2);
    EXPECT_EQ(ctrListener.callCount, 1);
}

TEST_F(PapoEventManagerTest, PublishAllPassesCorrectPayloadToListener)
{
    MockListener listener;
    subscribe(EVT_POSITION, listener);

    manager_.pushEvent(EVT_POSITION, PositionEvent{9.f, 10.f});
    endFrame();
    manager_.publishAll();

    ASSERT_NE(listener.lastPayload, nullptr);
    auto *p = static_cast<const PositionEvent *>(listener.lastPayload);
    EXPECT_FLOAT_EQ(p->x, 9.f);
    EXPECT_FLOAT_EQ(p->y, 10.f);
}
