#pragma once

#include "ports/output/IJwtProvider.hpp"
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <mutex>
#include <chrono>

namespace auth::adapters::secondary
{

    /**
     * @brief Fake JWT провайдер для разработки
     *
     * TODO: В production заменить на реальную JWT библиотеку
     */
    class FakeJwtAdapter : public ports::output::IJwtProvider
    {
    public:
        FakeJwtAdapter() = default;

        std::string createSessionToken(const std::string &userId, int lifetimeSeconds) override
        {
            nlohmann::json payload;
            payload["type"] = "session";
            payload["userId"] = userId;
            payload["exp"] = std::chrono::system_clock::now().time_since_epoch().count() + lifetimeSeconds * 1000000000LL;

            return "eyJ." + base64UrlEncode(payload.dump()) + ".sig";
        }

        std::string createAccessToken(const std::string &userId,
                                      const std::string &accountId,
                                      int lifetimeSeconds) override
        {
            nlohmann::json payload;
            payload["type"] = "access";
            payload["userId"] = userId;
            payload["accountId"] = accountId;
            payload["exp"] = std::chrono::system_clock::now().time_since_epoch().count() + lifetimeSeconds * 1000000000LL;

            return "eyJ." + base64UrlEncode(payload.dump()) + ".sig";
        }

        bool isValidToken(const std::string &token) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (blacklist_.count(token) > 0)
                return false;

            auto payload = decodePayload(token);
            if (!payload)
                return false;

            int64_t exp = (*payload).value("exp", 0LL);
            int64_t now = std::chrono::system_clock::now().time_since_epoch().count();
            return exp > now;
        }

        std::optional<std::string> getUserId(const std::string &token) override
        {
            auto payload = decodePayload(token);
            if (!payload)
                return std::nullopt;
            return (*payload).value("userId", "");
        }

        std::optional<std::string> getAccountId(const std::string &token) override
        {
            auto payload = decodePayload(token);
            if (!payload)
                return std::nullopt;

            std::string type = (*payload).value("type", "");
            if (type != "access")
                return std::nullopt;

            return (*payload).value("accountId", "");
        }

        void invalidateToken(const std::string &token) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            blacklist_.insert(token);
        }

    private:
        std::unordered_set<std::string> blacklist_;
        mutable std::mutex mutex_;

        std::optional<nlohmann::json> decodePayload(const std::string &token)
        {
            try
            {
                size_t first = token.find('.');
                size_t last = token.rfind('.');
                if (first == std::string::npos || first == last)
                    return std::nullopt;

                std::string payload = token.substr(first + 1, last - first - 1);
                return nlohmann::json::parse(base64UrlDecode(payload));
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        static std::string base64UrlEncode(const std::string &input)
        {
            static const char *chars =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string result;
            int val = 0, valb = -6;
            for (unsigned char c : input)
            {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0)
                {
                    result.push_back(chars[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6)
                result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
            return result;
        }

        static std::string base64UrlDecode(const std::string &input)
        {
            static const int lookup[] = {
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1,
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
                -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
                -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};
            std::string result;
            int val = 0, valb = -8;
            for (unsigned char c : input)
            {
                if (c >= 128 || lookup[c] == -1)
                    continue;
                val = (val << 6) + lookup[c];
                valb += 6;
                if (valb >= 0)
                {
                    result.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return result;
        }
    };

} // namespace auth::adapters::secondary
