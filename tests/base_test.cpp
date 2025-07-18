#include <gtest/gtest.h>
#include "gocxx/base/defer.h"
#include "gocxx/base/result.h"
#include "gocxx/base/chan.h"
#include <thread>
#include <chrono>

#include <vector>
#include <atomic>
#include <future>
#include <random>

using namespace gocxx::base;
using namespace gocxx::errors;

using namespace std::chrono_literals;

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

class ChanTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ChanTest, UnbufferedSendReceive) {
    auto ch = Chan<int>::Make();
    std::atomic<bool> receiver_started{false};
    std::atomic<bool> sender_started{false};
    
    int result = 0;
    std::thread receiver([&]() {
        receiver_started = true;
        ch >> result;
    });
    
    std::thread sender([&]() {
        sender_started = true;
        std::this_thread::sleep_for(50ms);
        ch << 42;
    });
    
    receiver.join();
    sender.join();
    
    EXPECT_TRUE(receiver_started);
    EXPECT_TRUE(sender_started);
    EXPECT_EQ(result, 42);
}

TEST_F(ChanTest, BufferedSendReceive) {
    auto ch = Chan < std::string >::Make(2);

    ch << std::string("hello"); ch << "world";

    std::string v1, v2;
    ch >> v1 >> v2;

    EXPECT_EQ(v1, "hello");
    EXPECT_EQ(v2, "world");
}

TEST_F(ChanTest, BufferedSendReceiveOrdered) {
    auto ch = Chan<int>::Make(3);

    for (int i = 1; i <= 3; ++i) {
        ch << i;
    }

    for (int i = 1; i <= 3; ++i) {
        int val;
        ch >> val;
        EXPECT_EQ(val, i);
    }
}

TEST_F(ChanTest, CloseAndReceive) {
    auto ch = Chan<int>::Make();
    std::atomic<bool> close_called{ false };

    std::thread closer([&]() {
        std::this_thread::sleep_for(100ms);
        close_called = true;
        ch->close();
        });

    auto val = ch->recv();
    EXPECT_FALSE(val.has_value());
    EXPECT_TRUE(close_called);

    closer.join();
}

TEST_F(ChanTest, SendOnClosedThrows) {
    auto ch = Chan<int>::Make();
    ch->close();

    EXPECT_THROW(ch << 1, std::runtime_error);
}

TEST_F(ChanTest, ReceiveOnClosedBufferedChannel) {
    auto ch = Chan<int>::Make(2);

    ch << 1 << 2;
    ch->close();

    int v1, v2;
    ch >> v1 >> v2;

    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);

    auto v3 = ch->recv();
    EXPECT_FALSE(v3.has_value());
}

TEST_F(ChanTest, OperatorReceiveOnClosedThrows) {
    auto ch = Chan<int>::Make();
    ch->close();

    int result = 0;
    EXPECT_THROW(ch >> result, std::runtime_error);
}

TEST_F(ChanTest, OperatorSendReceive) {
    auto ch = Chan<int>::Make();

    std::thread sender([&]() {
        std::this_thread::sleep_for(50ms);
        ch << 99;
        });

    int result = 0;
    ch >> result;

    sender.join();
    EXPECT_EQ(result, 99);
}

TEST_F(ChanTest, OperatorChaining) {
    auto ch = Chan<int>::Make(3);

    ch << 1 << 2 << 3;

    int a, b, c;
    ch >> a >> b >> c;

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST_F(ChanTest, MultipleProducersConsumers) {
    auto ch = Chan<int>::Make(10);
    constexpr int num_producers = 4;
    constexpr int num_consumers = 3;
    constexpr int items_per_producer = 25;

    std::atomic<int> total_sent{ 0 };
    std::atomic<int> total_received{ 0 };
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                ch << (i * 100 + j);
                total_sent++;
            }
            });
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            while (true) {
                auto val = ch->recv();
                if (!val.has_value()) break;
                total_received++;
            }
            });
    }

    for (auto& t : producers) t.join();
    ch->close();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(total_sent.load(), num_producers * items_per_producer);
    EXPECT_EQ(total_received.load(), num_producers * items_per_producer);
}

TEST_F(ChanTest, BufferedChannelBlocks) {
    auto ch = Chan<int>::Make(2);
    std::atomic<bool> send_completed{ false };

    ch << 1 << 2;

    std::thread sender([&]() {
        ch << 3;
        send_completed = true;
        });

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(send_completed);

    int val;
    ch >> val;
    EXPECT_EQ(val, 1);

    sender.join();
    EXPECT_TRUE(send_completed);
}

TEST_F(ChanTest, HighThroughputStressTest) {
    auto ch = Chan<int>::Make(100);
    constexpr int num_items = 10000;
    std::atomic<int> received_count{ 0 };

    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            ch << i;
        }
        ch->close();
        });

    std::thread consumer([&]() {
        while (true) {
            auto val = ch->recv();
            if (!val.has_value()) break;
            received_count++;
        }
        });

    producer.join();
    consumer.join();

    EXPECT_EQ(received_count.load(), num_items);
}

TEST_F(ChanTest, SendReceiveMovableOnly) {
    auto ch = Chan<std::unique_ptr<int>>::Make(2);

    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(84);

    ch << std::move(ptr1);
    ch << std::move(ptr2);

    auto recv1 = ch->recv();
    auto recv2 = ch->recv();

    ASSERT_TRUE(recv1.has_value());
    ASSERT_TRUE(recv2.has_value());
    EXPECT_EQ(**recv1, 42);
    EXPECT_EQ(**recv2, 84);
}

TEST_F(ChanTest, NonBlockingReceivePattern) {
    auto ch = Chan<int>::Make();

    std::future<std::optional<int>> future = std::async(std::launch::async, [&]() {
        return ch->recv();
        });

    auto status = future.wait_for(50ms);

    if (status == std::future_status::timeout) {
        ch << 123;
        auto result = future.get();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 123);
    }
    else {
        FAIL() << "Receive should have blocked";
    }
}

TEST_F(ChanTest, ExplicitZeroBufferSize) {
    auto ch = Chan<int>::Make(0);

    std::thread sender([&]() {
        std::this_thread::sleep_for(50ms);
        ch << 777;
        });

    int val;
    ch >> val;
    EXPECT_EQ(val, 777);

    sender.join();
}

TEST_F(ChanTest, IsClosedQuery) {
    auto ch = Chan<int>::Make();

    EXPECT_FALSE(ch->isClosed());
    ch->close();
    EXPECT_TRUE(ch->isClosed());
}

TEST_F(ChanTest, ExceptionSafety) {
    auto ch = Chan<int>::Make();

    ch->close();
    ch->close();

    EXPECT_TRUE(ch->isClosed());
}

TEST_F(ChanTest, ConcurrentCloseAndOperations) {
    auto ch = Chan<int>::Make(5);
    std::atomic<bool> exception_caught{ false };

    ch << 1 << 2;

    std::thread closer([&]() {
        std::this_thread::sleep_for(50ms);
        ch->close();
        });

    std::thread sender([&]() {
        try {
            for (int i = 0; i < 10; ++i) {
                ch << i;
                std::this_thread::sleep_for(10ms);
            }
        }
        catch (const std::runtime_error&) {
            exception_caught = true;
        }
        });

    closer.join();
    sender.join();

    EXPECT_TRUE(exception_caught);
}
