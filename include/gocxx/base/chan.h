#pragma once
#include <gocxx/sync/sync.h>  
#include <queue>
#include <optional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace gocxx::base {

    template<typename T>
    class Chan : public std::enable_shared_from_this<Chan<T>> {
    public:
        using value_type = T;

        static std::shared_ptr<Chan<T>> Make(std::size_t bufferSize = 0) {
            return std::shared_ptr<Chan<T>>(new Chan<T>(bufferSize));
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

                if (!hasData_ && closed_) return std::nullopt;

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

                if (queue_.empty() && closed_) return std::nullopt;

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

        bool isClosed() const {
            gocxx::sync::Lock lock(mutex_);
            return closed_;
        }

    private:
        explicit Chan(std::size_t bufferSize)
            : bufferSize_(bufferSize), closed_(false) {
        }

        Chan() = delete;
        Chan(const Chan&) = delete;
        Chan& operator=(const Chan&) = delete;
        Chan(Chan&&) = delete;
        Chan& operator=(Chan&&) = delete;

        mutable gocxx::sync::Mutex mutex_;
        gocxx::sync::Cond cond_recv_, cond_send_;
        std::size_t bufferSize_;
        bool closed_;

        // For unbuffered channel
        std::optional<T> data_;
        bool hasData_ = false;
        bool hasReceiver_ = false;

        // For buffered channel
        std::queue<T> queue_;
    };

    // Aliases and operator sugar
    template<typename T>
    using ChanPtr = std::shared_ptr<Chan<T>>;

    template<typename T>
    ChanPtr<T>& operator<<(ChanPtr<T>& ch, const T& val) {
        ch->send(val);
        return ch;
    }

    // Generic, SFINAE-guarded << overload
    template<typename T, typename U>
    std::enable_if_t<std::is_constructible_v<T, U>, ChanPtr<T>&> operator<<(ChanPtr<T>& ch, U&& value) {
        ch->send(T(std::forward<U>(value)));
        return ch;
    }

    template<typename T>
    ChanPtr<T>& operator<<(ChanPtr<T>& ch, T&& val) {
        ch->send(std::move(val));
        return ch;
    }

    template<typename T>
    ChanPtr<T>& operator>>(ChanPtr<T>& ch, T& out) {
        auto val = ch->recv();
        if (!val) throw std::runtime_error("receive on closed channel");
        out = std::move(*val);
        return ch;
    }

    template<typename T>
    ChanPtr<T> make_chan(std::size_t bufferSize = 0) {
        return Chan<T>::Make(bufferSize);
    }

} // namespace gocxx::base
