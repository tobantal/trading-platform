#pragma once

#include <IHttpHandler.hpp>
#include <IRequest.hpp>
#include <IResponse.hpp>
#include <sstream>
#include <mutex>
#include <map>

namespace broker::adapters::primary {

class MetricsHandler : public IHttpHandler {
public:
    void handle(IRequest& req, IResponse& res) override {
        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; version=0.0.4");
        res.setBody(serialize());
    }

    void incrementHttpRequests(const std::string& method, const std::string& path, int status) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = "http_requests_total{service=\"broker\",method=\"" + method + 
                          "\",path=\"" + path + "\",status=\"" + std::to_string(status) + "\"}";
        counters_[key]++;
    }

    void incrementOrdersExecuted(const std::string& direction) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = "orders_executed_total{direction=\"" + direction + "\"}";
        counters_[key]++;
    }

private:
    std::mutex mutex_;
    std::map<std::string, int64_t> counters_;

    std::string serialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        
        oss << "# HELP http_requests_total Total HTTP requests\n";
        oss << "# TYPE http_requests_total counter\n";
        
        oss << "# HELP orders_executed_total Total orders executed\n";
        oss << "# TYPE orders_executed_total counter\n";

        for (const auto& [key, value] : counters_) {
            oss << key << " " << value << "\n";
        }

        return oss.str();
    }
};

} // namespace broker::adapters::primary
