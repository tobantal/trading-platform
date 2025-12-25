#pragma once

#include "ports/output/IOrderRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <algorithm>
#include <set>

namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория ордеров
 */
class InMemoryOrderRepository : public ports::output::IOrderRepository {
public:
    /**
     * @brief Сохранить ордер
     */
    void save(const domain::Order& order) override {
        orders_.insert(order.id, std::make_shared<domain::Order>(order));
        
        std::lock_guard<std::mutex> lock(indexMutex_);
        accountOrders_[order.accountId].insert(order.id);
    }

    /**
     * @brief Найти ордер по ID
     */
    std::optional<domain::Order> findById(const std::string& id) override {
        auto order = orders_.find(id);
        return order ? std::optional(*order) : std::nullopt;
    }

    /**
     * @brief Найти все ордера счёта
     */
    std::vector<domain::Order> findByAccountId(const std::string& accountId) override {
        std::vector<domain::Order> result;
        
        std::set<std::string> orderIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = accountOrders_.find(accountId);
            if (it != accountOrders_.end()) {
                orderIds = it->second;
            }
        }
        
        for (const auto& id : orderIds) {
            if (auto order = orders_.find(id)) {
                result.push_back(*order);
            }
        }
        
        // Сортируем по дате создания (новые первые)
        std::sort(result.begin(), result.end(), 
            [](const domain::Order& a, const domain::Order& b) {
                return a.createdAt > b.createdAt;
            });
        
        return result;
    }

    /**
     * @brief Найти ордера по статусу
     */
    std::vector<domain::Order> findByStatus(
        const std::string& accountId,
        domain::OrderStatus status
    ) override {
        auto all = findByAccountId(accountId);
        std::vector<domain::Order> result;
        
        std::copy_if(all.begin(), all.end(), std::back_inserter(result),
            [status](const domain::Order& o) { return o.status == status; });
        
        return result;
    }

    /**
     * @brief Найти ордера за период
     */
    std::vector<domain::Order> findByPeriod(
        const std::string& accountId,
        const domain::Timestamp& from,
        const domain::Timestamp& to
    ) override {
        auto all = findByAccountId(accountId);
        std::vector<domain::Order> result;
        
        std::copy_if(all.begin(), all.end(), std::back_inserter(result),
            [&from, &to](const domain::Order& o) {
                return o.createdAt >= from && o.createdAt <= to;
            });
        
        return result;
    }

    /**
     * @brief Обновить статус ордера
     */
    void updateStatus(const std::string& orderId, domain::OrderStatus status) override {
        auto order = orders_.find(orderId);
        if (order) {
            auto updated = std::make_shared<domain::Order>(*order);
            updated->updateStatus(status);
            orders_.insert(orderId, updated);
        }
    }

    /**
     * @brief Удалить ордер
     */
    bool deleteById(const std::string& id) override {
        auto order = orders_.find(id);
        if (!order) {
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            accountOrders_[order->accountId].erase(id);
        }
        
        orders_.remove(id);
        return true;
    }

    /**
     * @brief Очистить репозиторий
     */
    void clear() {
        orders_.clear();
        std::lock_guard<std::mutex> lock(indexMutex_);
        accountOrders_.clear();
    }

    /**
     * @brief Получить количество ордеров
     */
    size_t count() const {
        auto all = orders_.getAll(); // Получаем все элементы
        return all.size();           // Возвращаем размер
    }

private:
    ThreadSafeMap<std::string, domain::Order> orders_;
    mutable std::mutex indexMutex_;
    std::unordered_map<std::string, std::set<std::string>> accountOrders_; // accountId -> orderIds
};

} // namespace trading::adapters::secondary
