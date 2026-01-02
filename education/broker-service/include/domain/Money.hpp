#pragma once

#include <string>
#include <cstdint>
#include <cmath>

namespace broker::domain {

/**
 * @brief Ð”ÐµÐ½ÐµÐ¶Ð½Ð°Ñ ÑÑƒÐ¼Ð¼Ð° (ÐºÐ°Ðº Ð² Tinkoff Invest API)
 * 
 * Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÑ‚ Ñ†ÐµÐ»Ð¾Ñ‡Ð¸ÑÐ»ÐµÐ½Ð½Ð¾Ðµ Ð¿Ñ€ÐµÐ´ÑÑ‚Ð°Ð²Ð»ÐµÐ½Ð¸Ðµ Ð´Ð»Ñ Ñ‚Ð¾Ñ‡Ð½Ñ‹Ñ… Ð²Ñ‹Ñ‡Ð¸ÑÐ»ÐµÐ½Ð¸Ð¹:
 * - units: Ñ†ÐµÐ»Ð°Ñ Ñ‡Ð°ÑÑ‚ÑŒ
 * - nano: Ð´Ñ€Ð¾Ð±Ð½Ð°Ñ Ñ‡Ð°ÑÑ‚ÑŒ (10^-9)
 * 
 * ÐŸÑ€Ð¸Ð¼ÐµÑ€: 265.50 RUB = {units: 265, nano: 500000000, currency: "RUB"}
 */
struct Money {
    int64_t units = 0;          ///< Ð¦ÐµÐ»Ð°Ñ Ñ‡Ð°ÑÑ‚ÑŒ ÑÑƒÐ¼Ð¼Ñ‹
    int32_t nano = 0;           ///< Ð”Ñ€Ð¾Ð±Ð½Ð°Ñ Ñ‡Ð°ÑÑ‚ÑŒ (Ð½Ð°Ð½Ð¾ÑÐµÐºÑƒÐ½Ð´Ñ‹, 10^-9)
    std::string currency = "RUB"; ///< ÐšÐ¾Ð´ Ð²Ð°Ð»ÑŽÑ‚Ñ‹ (ISO 4217)

    Money() = default;
    
    Money(int64_t u, int32_t n, const std::string& curr = "RUB")
        : units(u), nano(n), currency(curr) {}

    /**
     * @brief ÐŸÑ€ÐµÐ¾Ð±Ñ€Ð°Ð·Ð¾Ð²Ð°Ñ‚ÑŒ Ð² double
     */
    double toDouble() const {
        return static_cast<double>(units) + static_cast<double>(nano) / 1e9;
    }

    /**
     * @brief Ð¡Ð¾Ð·Ð´Ð°Ñ‚ÑŒ Money Ð¸Ð· double
     */
    static Money fromDouble(double value, const std::string& curr = "RUB") {
        Money m;
        m.units = static_cast<int64_t>(value);
        m.nano = static_cast<int32_t>((value - m.units) * 1e9);
        m.currency = curr;
        return m;
    }

    /**
     * @brief Ð¡Ð»Ð¾Ð¶ÐµÐ½Ð¸Ðµ
     */
    Money operator+(const Money& other) const {
        Money result;
        result.currency = currency;
        
        int64_t totalNano = static_cast<int64_t>(nano) + other.nano;
        result.units = units + other.units + totalNano / 1000000000;
        result.nano = static_cast<int32_t>(totalNano % 1000000000);
        
        // ÐžÐ±Ñ€Ð°Ð±Ð¾Ñ‚ÐºÐ° Ð¾Ñ‚Ñ€Ð¸Ñ†Ð°Ñ‚ÐµÐ»ÑŒÐ½Ñ‹Ñ… Ð·Ð½Ð°Ñ‡ÐµÐ½Ð¸Ð¹
        if (result.nano < 0) {
            result.units -= 1;
            result.nano += 1000000000;
        }
        
        return result;
    }

    /**
     * @brief Ð’Ñ‹Ñ‡Ð¸Ñ‚Ð°Ð½Ð¸Ðµ
     */
    Money operator-(const Money& other) const {
        Money negOther = other;
        negOther.units = -negOther.units;
        negOther.nano = -negOther.nano;
        return *this + negOther;
    }

    /**
     * @brief Ð£Ð¼Ð½Ð¾Ð¶ÐµÐ½Ð¸Ðµ Ð½Ð° ÐºÐ¾Ð»Ð¸Ñ‡ÐµÑÑ‚Ð²Ð¾
     */
    Money operator*(int64_t qty) const {
        double val = toDouble() * qty;
        return fromDouble(val, currency);
    }

    /**
     * @brief Ð¡Ñ€Ð°Ð²Ð½ÐµÐ½Ð¸Ðµ
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
     * @brief ÐŸÑ€Ð¾Ð²ÐµÑ€ÐºÐ° Ð½Ð° Ð½Ð¾Ð»ÑŒ
     */
    bool isZero() const {
        return units == 0 && nano == 0;
    }

    /**
     * @brief ÐŸÑ€Ð¾Ð²ÐµÑ€ÐºÐ° Ð½Ð° Ð¾Ñ‚Ñ€Ð¸Ñ†Ð°Ñ‚ÐµÐ»ÑŒÐ½Ð¾Ðµ Ð·Ð½Ð°Ñ‡ÐµÐ½Ð¸Ðµ
     */
    bool isNegative() const {
        return units < 0 || (units == 0 && nano < 0);
    }
};

} // namespace broker::domain
