#pragma once

#include "adapters/secondary/HttpBrokerGateway.hpp"
#include <gmock/gmock.h>

namespace trading::tests
{

class MockBrokerClientSettings : public settings::IBrokerClientSettings
{
public:
    std::string getHost() const override { return "test-host"; }
    int getPort() const override { return 9999; }
};

class MockHttpClient : public IHttpClient
{
public:
    MOCK_METHOD(bool, send, (const IRequest& req, IResponse& res), (override));
};


/**
 * @brief Mock для HttpBrokerGateway
 */
class MockHttpBrokerGateway : public adapters::secondary::HttpBrokerGateway
{
public:
    MockHttpBrokerGateway()
        : HttpBrokerGateway(
            std::make_shared<MockHttpClient>(),
            std::make_shared<MockBrokerClientSettings>()
        ) {}

    MOCK_METHOD(std::optional<domain::Quote>, getQuote, (const std::string &figi), (override));
    MOCK_METHOD(std::vector<domain::Quote>, getQuotes, (const std::vector<std::string> &figis), (override));
    MOCK_METHOD(std::optional<domain::Instrument>, getInstrumentByFigi, (const std::string &figi), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, getAllInstruments, (), (override));
    MOCK_METHOD(std::vector<domain::Instrument>, searchInstruments, (const std::string &query), (override));
    MOCK_METHOD(domain::Portfolio, getPortfolio, (const std::string &accountId), (override));
    MOCK_METHOD(std::vector<domain::Order>, getOrders, (const std::string &accountId), (override));
    MOCK_METHOD(std::optional<domain::Order>, getOrder, (const std::string &accountId, const std::string &orderId), (override));
};

} // namespace trading::tests
