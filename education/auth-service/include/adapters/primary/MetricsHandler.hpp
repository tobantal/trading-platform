#pragma once

#include <IHttpHandler.hpp>
#include <sstream>
#include <atomic>
#include <chrono>

namespace auth::adapters::primary {

class MetricsHandler : public IHttpHandler {
public:
    MetricsHandler() : startTime_(std::chrono::steady_clock::now()) {}

    void handle(IRequest& req, IResponse& res) override {
        std::ostringstream oss;
        
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count();

        oss << "# HELP auth_uptime_seconds Service uptime\n";
        oss << "# TYPE auth_uptime_seconds gauge\n";
        oss << "auth_uptime_seconds " << uptime << "\n\n";

        oss << "# HELP http_requests_total Total HTTP requests\n";
        oss << "# TYPE http_requests_total counter\n";
        oss << "http_requests_total{service=\"auth\"} " << requestsTotal_.load() << "\n";

        res.setStatus(200);
        res.setHeader("Content-Type", "text/plain; charset=utf-8");
        res.setBody(oss.str());
    }

    void incrementRequests() { requestsTotal_++; }

private:
    std::chrono::steady_clock::time_point startTime_;
    std::atomic<int64_t> requestsTotal_{0};
};

} // namespace auth::adapters::primary
