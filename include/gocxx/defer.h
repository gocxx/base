#pragma once
#include <utility>
#include <functional>

#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)
#define defer auto CONCAT(_defer_, __COUNTER__) = gocxx::sync::Defer

namespace gocxx::sync {

/**
 * @brief A utility that runs a function when it goes out of scope.
 *
 * This mimics Go's `defer` behavior using RAII.
 */
class Defer {
    std::function<void()> fn_;
public:
    explicit Defer(std::function<void()> fn) : fn_(std::move(fn)) {}
    ~Defer() { fn_(); }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;
};

} // namespace gocxx::sync
