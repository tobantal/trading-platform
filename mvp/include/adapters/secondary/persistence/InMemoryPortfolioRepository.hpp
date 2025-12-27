#pragma once

#include "ports/output/IPortfolioRepository.hpp"
#include <ThreadSafeMap.hpp>
#include <mutex>
#include <algorithm>
#include <unordered_set>

namespace trading::adapters::secondary {

/**
 * @brief In-memory реализация репозитория портфелей
 */
class InMemoryPortfolioRepository : public ports::output::IPortfolioRepository {
public:
    /**
     * @brief Сохранить портфель
     */
    void save(const domain::Portfolio& portfolio) override {
        // Копируем портфель для сохранения
        auto portfolioToSave = std::make_shared<domain::Portfolio>(portfolio);
        
        // Пересчитываем общую стоимость перед сохранением
        portfolioToSave->recalculateTotalValue();
        
        // Сохраняем портфель
        portfolios_.insert(portfolio.accountId, portfolioToSave);
        
        // Обновляем индексы
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            userPortfolios_[getUserIdFromAccountId(portfolio.accountId)].insert(portfolio.accountId);
        }
    }

    /**
     * @brief Найти портфель по ID счёта
     */
    std::optional<domain::Portfolio> findByAccountId(const std::string& accountId) override {
        auto portfolio = portfolios_.find(accountId);
        if (portfolio) {
            // Возвращаем копию портфеля
            return std::optional<domain::Portfolio>(*portfolio);
        }
        return std::nullopt;
    }

    /**
     * @brief Удалить портфель по ID счёта
     */
    bool deleteByAccountId(const std::string& accountId) override {
        auto portfolio = portfolios_.find(accountId);
        if (!portfolio) {
            return false;
        }
        
        // Удаляем из индексов
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            userPortfolios_[getUserIdFromAccountId(accountId)].erase(accountId);
        }
        
        // Удаляем портфель
        portfolios_.remove(accountId);
        return true;
    }

    /**
     * @brief Обновить денежные средства портфеля
     */
    void updateCash(const std::string& accountId, const domain::Money& cash) override {
        auto portfolio = portfolios_.find(accountId);
        if (portfolio) {
            auto updated = std::make_shared<domain::Portfolio>(*portfolio);
            updated->cash = cash;
            updated->recalculateTotalValue();
            portfolios_.insert(accountId, updated);
        }
    }

    /**
     * @brief Обновить позицию в портфеле
     */
    void updatePosition(const std::string& accountId, const domain::Position& position) override {
        auto portfolio = portfolios_.find(accountId);
        if (portfolio) {
            auto updated = std::make_shared<domain::Portfolio>(*portfolio);
            
            // Ищем существующую позицию
            bool found = false;
            for (auto& pos : updated->positions) {
                if (pos.figi == position.figi) {
                    pos = position;
                    found = true;
                    break;
                }
            }
            
            // Если позиция не найдена, добавляем новую
            if (!found) {
                updated->positions.push_back(position);
            }
            
            updated->recalculateTotalValue();
            portfolios_.insert(accountId, updated);
        }
    }

    /**
     * @brief Удалить позицию из портфеля
     */
    bool deletePosition(const std::string& accountId, const std::string& figi) override {
        auto portfolio = portfolios_.find(accountId);
        if (!portfolio) {
            return false;
        }
        
        auto updated = std::make_shared<domain::Portfolio>(*portfolio);
        
        // Удаляем позицию с указанным FIGI
        auto it = std::remove_if(
            updated->positions.begin(),
            updated->positions.end(),
            [&figi](const domain::Position& pos) { return pos.figi == figi; }
        );
        
        if (it != updated->positions.end()) {
            updated->positions.erase(it, updated->positions.end());
            updated->recalculateTotalValue();
            portfolios_.insert(accountId, updated);
            return true;
        }
        
        return false;
    }

    /**
     * @brief Получить все портфели пользователя
     */
    std::vector<domain::Portfolio> findByUserId(const std::string& userId) override {
        std::vector<domain::Portfolio> result;
        
        std::unordered_set<std::string> accountIds;
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            auto it = userPortfolios_.find(userId);
            if (it != userPortfolios_.end()) {
                accountIds = it->second;
            }
        }
        
        for (const auto& accountId : accountIds) {
            if (auto portfolio = portfolios_.find(accountId)) {
                result.push_back(*portfolio);
            }
        }
        
        return result;
    }

    /**
     * @brief Очистить репозиторий
     */
    void clear() {
        portfolios_.clear();
        std::lock_guard<std::mutex> lock(indexMutex_);
        userPortfolios_.clear();
    }

    /**
     * @brief Получить количество портфелей
     */
    size_t count() const {
        auto all = portfolios_.getAll();
        return all.size();
    }

private:
    ThreadSafeMap<std::string, domain::Portfolio> portfolios_;
    mutable std::mutex indexMutex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> userPortfolios_; // userId -> accountIds

    /**
     * @brief Извлекает userId из accountId
     * 
     * В реальной системе это может быть сложнее, но для тестов
     * используем простую логику: account-{userId}-{number}
     * Например: account-123-456 -> userId=123
     */
    std::string getUserIdFromAccountId(const std::string& accountId) {
        // Простая эвристика: если accountId содержит user-, берем часть после него
        size_t pos = accountId.find("user-");
        if (pos != std::string::npos) {
            // Ищем следующий дефис
            size_t start = pos + 5; // длина "user-"
            size_t end = accountId.find("-", start);
            if (end != std::string::npos) {
                return "user-" + accountId.substr(start, end - start);
            }
        }
        
        // Если не нашли, возвращаем accountId как есть (для совместимости)
        return accountId;
    }
};

} // namespace trading::adapters::secondary