// include/application/QuoteService.hpp
#pragma once

#include "ports/input/IQuoteService.hpp"
#include "ports/output/IInstrumentRepository.hpp"
#include "ports/output/IQuoteRepository.hpp"
#include "ports/output/IBrokerGateway.hpp"
#include <memory>

namespace broker::application {

/**
 * @brief Quote service implementation
 */
class QuoteService : public ports::input::IQuoteService {
public:
    QuoteService(
        std::shared_ptr<ports::output::IInstrumentRepository> instrumentRepo,
        std::shared_ptr<ports::output::IQuoteRepository> quoteRepo,
        std::shared_ptr<ports::output::IBrokerGateway> broker
    ) : instrumentRepo_(std::move(instrumentRepo))
      , quoteRepo_(std::move(quoteRepo))
      , broker_(std::move(broker))
    {}

    std::vector<domain::Instrument> getInstruments() override {
        return broker_->getAllInstruments();
    }

    std::optional<domain::Instrument> getInstrument(const std::string& figi) override {
        return broker_->getInstrumentByFigi(figi);
    }

    std::optional<domain::Quote> getQuote(const std::string& figi) override {
        return broker_->getQuote(figi);
    }

private:
    std::shared_ptr<ports::output::IInstrumentRepository> instrumentRepo_;
    std::shared_ptr<ports::output::IQuoteRepository> quoteRepo_;
    std::shared_ptr<ports::output::IBrokerGateway> broker_;
};

} // namespace broker::application
