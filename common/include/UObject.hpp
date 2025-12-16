#pragma once

#include "IUObject.hpp"
#include <unordered_map>
#include <stdexcept>

/**
 * @file UObject.hpp
 * @brief Реализация универсального объекта для хранения параметров
 * @author Anton Tobolkin
 * @version 1.0
 */

/**
 * @brief Реализация IUObject на основе hash-map
 * 
 * Простая реализация универсального объекта для хранения
 * произвольных свойств. Используется для параметров приказов
 * и других случаев, где нужно key-value хранилище.
 */
class UObject : public IUObject {
public:
    /**
     * @brief Конструктор по умолчанию
     */
    UObject() = default;
    
    /**
     * @brief Виртуальный деструктор
     */
    ~UObject() override = default;
    
    /**
     * @brief Получить значение свойства
     * 
     * @param key Ключ свойства
     * @return std::any Значение свойства
     * @throws std::runtime_error если свойство не найдено
     */
    std::any getProperty(const std::string& key) const override {
        auto it = properties_.find(key);
        if (it == properties_.end()) {
            throw std::runtime_error("Property not found: " + key);
        }
        return it->second;
    }
    
    /**
     * @brief Установить значение свойства
     * 
     * @param key Ключ свойства
     * @param value Значение свойства
     */
    void setProperty(const std::string& key, const std::any& value) override {
        properties_[key] = value;
    }
    
    /**
     * @brief Проверить наличие свойства
     * 
     * @param key Ключ свойства
     * @return true если свойство существует
     */
    bool hasProperty(const std::string& key) const {
        return properties_.find(key) != properties_.end();
    }

private:
    std::unordered_map<std::string, std::any> properties_;  ///< Хранилище свойств
};