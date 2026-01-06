#pragma once

#include <string>
#include <cstdint>
#include <cmath>

namespace broker::domain {

/**
 * @brief Денежная сумма (как в Tinkoff Invest API)
 * 
 * Использует целочисленное представление для точных вычислений:
 * - units: целая часть
 * - nano: дробная часть (10^-9)
 * 
 * Пример: 265.50 RUB = {units: 265, nano: 500000000, currency: "RUB"}
 */
struct Money {
    int64_t units = 0;          ///< Целая часть суммы
    int32_t nano = 0;           ///< Дробная часть (наносекунды, 10^-9)
    std::string currency = "RUB"; ///< Код валюты (ISO 4217)

    Money() = default;
    
    Money(int64_t u, int32_t n, const std::string& curr = "RUB")
        : units(u), nano(n), currency(curr) {}

    /**
     * @brief Преобразовать в double
     */
    double toDouble() const {
        return static_cast<double>(units) + static_cast<double>(nano) / 1e9;
    }

    /**
     * @brief Создать Money из double
     */
    static Money fromDouble(double value, const std::string& curr = "RUB") {
        Money m;
        m.units = static_cast<int64_t>(value);
        m.nano = static_cast<int32_t>((value - m.units) * 1e9);
        m.currency = curr;
        return m;
    }

    /**
     * @brief Сложение
     */
    Money operator+(const Money& other) const {
        Money result;
        result.currency = currency;
        
        int64_t totalNano = static_cast<int64_t>(nano) + other.nano;
        result.units = units + other.units + totalNano / 1000000000;
        result.nano = static_cast<int32_t>(totalNano % 1000000000);
        
        // Обработка отрицательных значений
        if (result.nano < 0) {
            result.units -= 1;
            result.nano += 1000000000;
        }
        
        return result;
    }

    /**
     * @brief Вычитание
     */
    Money operator-(const Money& other) const {
        Money negOther = other;
        negOther.units = -negOther.units;
        negOther.nano = -negOther.nano;
        return *this + negOther;
    }

    /**
     * @brief Умножение на количество
     */
    Money operator*(int64_t qty) const {
        double val = toDouble() * qty;
        return fromDouble(val, currency);
    }

    /**
     * @brief Сравнение
     */
    bool operator==(const Money& other) const {
        return units == other.units && nano == other.nano && currency == other.currency;
    }

    bool operator!=(const Money& other) const {
        return !(*this == other);
    }

    bool operator<(const Money& other) const {
        if (units != other.units) return units < other.units;
        return nano < other.nano;
    }

    bool operator>(const Money& other) const {
        return other < *this;
    }

    bool operator<=(const Money& other) const {
        return !(other < *this);
    }

    bool operator>=(const Money& other) const {
        return !(*this < other);
    }

    /**
     * @brief Проверка на ноль
     */
    bool isZero() const {
        return units == 0 && nano == 0;
    }

    /**
     * @brief Проверка на отрицательное значение
     */
    bool isNegative() const {
        return units < 0 || (units == 0 && nano < 0);
    }
};

} // namespace broker::domain