#include "ThreadSafeQueue.hpp"

ThreadSafeQueue::ThreadSafeQueue() = default;

ThreadSafeQueue::~ThreadSafeQueue() {
    shutdown();
}

void ThreadSafeQueue::push(std::shared_ptr<ICommand> command) {
    if (!command) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        queue_.push(std::move(command));
    }

    condVar_.notify_one();
}

std::shared_ptr<ICommand> ThreadSafeQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condVar_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });

    if (shutdown_ && queue_.empty()) {
        return nullptr;
    }

    auto command = queue_.front();
    queue_.pop();
    return command;
}

void ThreadSafeQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    condVar_.notify_all();
}

bool ThreadSafeQueue::isShutdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

bool ThreadSafeQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

size_t ThreadSafeQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
