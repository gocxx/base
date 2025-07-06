#include <gtest/gtest.h>
#include "gocxx/defer.h"
#include "gocxx/result.h"

using namespace gocxx::base;
using namespace gocxx::errors;

TEST(ResultTest, OkState) {
    Result<int> r{42, nullptr};
    EXPECT_TRUE(r.Ok());
    EXPECT_EQ(r.value, 42);
}

TEST(DeferTest, ExecutesOnScopeExit) {
    bool called = false;

    {
        defer([&]() {
            called = true;
        });
        EXPECT_FALSE(called);
    }

    EXPECT_TRUE(called);
}

TEST(ResultTest, OkResult) {
    Result<int> r{42, nullptr};

    EXPECT_TRUE(r.Ok());
    EXPECT_FALSE(r.Failed());
    EXPECT_EQ(r.value, 42);
    EXPECT_EQ(r.UnwrapOr(99), 42);
    EXPECT_EQ(r.UnwrapOrMove(99), 42);
}

TEST(ResultTest, ErrorResult) {
    auto err = New("fail");
    Result<int> r{0, err};

    EXPECT_FALSE(r.Ok());
    EXPECT_TRUE(r.Failed());
    EXPECT_EQ(r.UnwrapOr(77), 77);
    EXPECT_EQ(r.UnwrapOrMove(88), 88);
}

TEST(ResultTest, BoolConversion) {
    Result<int> ok{10, nullptr};
    Result<int> bad{0, New("fail")};

    EXPECT_TRUE(ok);
    EXPECT_FALSE(bad);
}

TEST(ResultVoid, OkCase) {
    Result<void> r{nullptr};

    EXPECT_TRUE(r.Ok());
    EXPECT_FALSE(r.Failed());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(ResultVoid, ErrorCase) {
    Result<void> r{New("bad")};

    EXPECT_FALSE(r.Ok());
    EXPECT_TRUE(r.Failed());
    EXPECT_FALSE(static_cast<bool>(r));
}