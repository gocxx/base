#pragma once
#include <gocxx/sync/sync.h>
#include <queue>
#include <optional>
#include <stdexcept>
#include <utility> // for std::move

namespace gocxx::base {

    template<typename T>
    class Chan {
    public:
        explicit Chan(std::size_t bufferSize = 0)
            : bufferSize_(bufferSize), closed_(false) {
        }

        void send(T&& value) {
            gocxx::sync::UniqueLock lock(mutex_);
            if (closed_) throw std::runtime_error("send on closed channel");

            if (bufferSize_ == 0) {
                while (!closed_ && !hasReceiver_) {
                    cond_send_.Wait(lock);
                }
                if (closed_) throw std::runtime_error("send on closed channel");

                data_ = std::move(value);
                hasData_ = true;
                cond_recv_.NotifyOne();

                while (!closed_ && hasData_) {
                    cond_send_.Wait(lock);
                }
                if (closed_) throw std::runtime_error("send on closed channel");

            }
            else {
                while (!closed_ && queue_.size() >= bufferSize_) {
                    cond_send_.Wait(lock);
                }
                if (closed_) throw std::runtime_error("send on closed channel");

                queue_.push(std::move(value));
                cond_recv_.NotifyOne();
            }
        }

        void send(const T& value) {
            T copy = value;
            send(std::move(copy));
        }

        std::optional<T> recv() {
            gocxx::sync::UniqueLock lock(mutex_);

            if (bufferSize_ == 0) {
                hasReceiver_ = true;
                cond_send_.NotifyOne();

                while (!closed_ && !hasData_) {
                    cond_recv_.Wait(lock);
                }

                hasReceiver_ = false;

                if (!hasData_ && closed_) {
                    return std::nullopt;
                }

                auto val = std::move(data_.value());
                data_.reset();
                hasData_ = false;
                cond_send_.NotifyOne();
                return val;

            }
            else {
                while (!closed_ && queue_.empty()) {
                    cond_recv_.Wait(lock);
                }

                if (queue_.empty() && closed_) {
                    return std::nullopt;
                }

                auto val = std::move(queue_.front());
                queue_.pop();
                cond_send_.NotifyOne();
                return val;
            }
        }

        void close() {
            gocxx::sync::Lock lock(mutex_);
            closed_ = true;
            cond_recv_.NotifyAll();
            cond_send_.NotifyAll();
        }

        bool isClosed() {
            gocxx::sync::Lock lock(mutex_);
            return closed_;
        }

        Chan<T>& operator<<(const T& value) {
            send(value);
            return *this;
        }

        Chan<T>& operator<<(T&& value) {
            send(std::move(value));
            return *this;
        }

        Chan<T>& operator>>(T& out) {
            auto val = recv();
            if (!val) throw std::runtime_error("receive on closed channel");
            out = std::move(*val);
            return *this;
        }

    private:
        gocxx::sync::Mutex mutex_;
        gocxx::sync::Cond cond_recv_, cond_send_;
        std::size_t bufferSize_;
        bool closed_;

        std::optional<T> data_;
        bool hasData_ = false;
        bool hasReceiver_ = false;

        std::queue<T> queue_;
    };

} // namespace gocxx::base
