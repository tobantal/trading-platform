#pragma once

#include <string>
#include <cstdint>
#include <cmath>

namespace trading::domain {

/**
 * @brief Денежное значение с валютой
 * 
 * Хранит значение в минорных единицах (копейки, центы) для точности.
 */
class Money {
public:
    int64_t units = 0;      // Целая часть
    int32_t nano = 0;       // Дробная часть (наносекунды, 10^-9)
    std::string currency = "RUB";

    Money() = default;
    
    Money(int64_t u, int32_t n, const std::string& cur = "RUB")
        : units(u), nano(n), currency(cur) {}

    static Money fromDouble(double value, const std::string& cur = "RUB") {
        Money m;
        m.currency = cur;
        m.units = static_cast<int64_t>(value);
        m.nano = static_cast<int32_t>((value - m.units) * 1e9);
        return m;
    }

    double toDouble() const {
        return static_cast<double>(units) + static_cast<double>(nano) / 1e9;
    }

    Money operator+(const Money& other) const {
        Money result;
        result.currency = currency;
        result.units = units + other.units;
        result.nano = nano + other.nano;
        
        // Нормализация
        if (result.nano >= 1000000000) {
            result.units++;
            result.nano -= 1000000000;
        } else if (result.nano < 0) {
            result.units--;
            result.nano += 1000000000;
        }
        
        return result;
    }

    Money operator-(const Money& other) const {
        Money result;
        result.currency = currency;
        result.units = units - other.units;
        result.nano = nano - other.nano;
        
        if (result.nano < 0) {
            result.units--;
            result.nano += 1000000000;
        }
        
        return result;
    }

    Money operator*(int64_t multiplier) const {
        return Money::fromDouble(toDouble() * multiplier, currency);
    }

    bool operator<(const Money& other) const {
        return toDouble() < other.toDouble();
    }

    bool operator>(const Money& other) const {
        return toDouble() > other.toDouble();
    }

    bool operator==(const Money& other) const {
        return units == other.units && nano == other.nano && currency == other.currency;
    }
};

} // namespace trading::domain
