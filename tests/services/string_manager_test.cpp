#include <gtest/gtest.h>
#include "services/string_manager.hpp"
#include "utils/exception.hpp"

// ============================================================
// StringId tests
// ============================================================

TEST(StringIdTest, DefaultConstructorZeroId)
{
    StringId sid;
    EXPECT_EQ(sid.id(), 0u);
    EXPECT_EQ(sid.str(), nullptr);
}

TEST(StringIdTest, ConstructorStoresValues)
{
    const char *s = "hello";
    StringId sid(42u, s);
    EXPECT_EQ(sid.id(), 42u);
    EXPECT_EQ(sid.str(), s);
}

TEST(StringIdTest, EqualityOperatorSameId)
{
    StringId a(1u, "a");
    StringId b(1u, "b");
    EXPECT_TRUE(a == b);
}

TEST(StringIdTest, InequalityOperatorDifferentId)
{
    StringId a(1u, "a");
    StringId b(2u, "a");
    EXPECT_TRUE(a != b);
}

TEST(StringIdTest, UdlProducesConsistentHash)
{
    StringId a = "player"_sid;
    StringId b = "player"_sid;
    EXPECT_EQ(a.id(), b.id());
    EXPECT_TRUE(a == b);
}

TEST(StringIdTest, UdlDifferentStringsProduceDifferentIds)
{
    StringId a = "player"_sid;
    StringId b = "enemy"_sid;
    EXPECT_NE(a.id(), b.id());
}

TEST(StringIdTest, UdlPreservesStringPointer)
{
    StringId sid = "weapon"_sid;
    EXPECT_NE(sid.str(), nullptr);
}

TEST(StringIdTest, SidMacroMatchesUdl)
{
    StringId via_macro = SID("health");
    StringId via_udl   = "health"_sid;
    EXPECT_EQ(via_macro.id(), via_udl.id());
}

TEST(StringIdTest, FnvEmptyStringIsOffsetBasis)
{
    // FNV-1a of an empty string is the offset basis: 2166136261
    constexpr uint32_t h = fnv1a("", 0);
    EXPECT_EQ(h, 2166136261u);
}

TEST(StringIdTest, FnvIsConstexpr)
{
    // Compile-time evaluation — if this compiles, the function is constexpr.
    constexpr uint32_t h = fnv1a("test", 4);
    EXPECT_NE(h, 0u);
}

// ============================================================
// StringManager tests
// ============================================================

class StringManagerTest : public ::testing::Test
{
protected:
    Service::StringManager manager_;
};

TEST_F(StringManagerTest, InsertThenGetReturnsCorrectString)
{
    StringId sid = "position"_sid;
    manager_.insertStringID(sid);
    const char *result = manager_.getStrFromID(sid.id());
    EXPECT_EQ(result, sid.str());
}

TEST_F(StringManagerTest, GetUnknownIdThrows)
{
    EXPECT_THROW(manager_.getStrFromID(9999u), Util::LogicException);
}

TEST_F(StringManagerTest, RemoveExistingIdSucceeds)
{
    StringId sid = "velocity"_sid;
    manager_.insertStringID(sid);
    ASSERT_NO_THROW(manager_.removeStringID(sid));
}

TEST_F(StringManagerTest, GetAfterRemoveThrows)
{
    StringId sid = "rotation"_sid;
    manager_.insertStringID(sid);
    manager_.removeStringID(sid);
    EXPECT_THROW(manager_.getStrFromID(sid.id()), Util::LogicException);
}

TEST_F(StringManagerTest, RemoveUnknownIdThrows)
{
    StringId sid = "unknown"_sid;
    EXPECT_THROW(manager_.removeStringID(sid), Util::LogicException);
}

TEST_F(StringManagerTest, InsertMultipleDistinctIds)
{
    StringId a = "alpha"_sid;
    StringId b = "beta"_sid;
    StringId c = "gamma"_sid;

    manager_.insertStringID(a);
    manager_.insertStringID(b);
    manager_.insertStringID(c);

    EXPECT_EQ(manager_.getStrFromID(a.id()), a.str());
    EXPECT_EQ(manager_.getStrFromID(b.id()), b.str());
    EXPECT_EQ(manager_.getStrFromID(c.id()), c.str());
}

TEST_F(StringManagerTest, InsertSameIdTwiceOverwrites)
{
    StringId first  = SID("tag");
    const char *new_str = "overwritten";
    StringId second(first.id(), new_str);

    manager_.insertStringID(first);
    manager_.insertStringID(second);

    EXPECT_EQ(manager_.getStrFromID(first.id()), new_str);
}

TEST_F(StringManagerTest, RemoveOneDoesNotAffectOthers)
{
    StringId a = "x"_sid;
    StringId b = "y"_sid;

    manager_.insertStringID(a);
    manager_.insertStringID(b);
    manager_.removeStringID(a);

    EXPECT_THROW(manager_.getStrFromID(a.id()), Util::LogicException);
    EXPECT_EQ(manager_.getStrFromID(b.id()), b.str());
}

TEST_F(StringManagerTest, ReinsertAfterRemoveSucceeds)
{
    StringId sid = "respawn"_sid;
    manager_.insertStringID(sid);
    manager_.removeStringID(sid);
    ASSERT_NO_THROW(manager_.insertStringID(sid));
    EXPECT_EQ(manager_.getStrFromID(sid.id()), sid.str());
}
