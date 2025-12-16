#include "handlers/MetricsHandler.hpp"
#include <iostream>
#include <sstream>

MetricsHandler::MetricsHandler()
{
    std::cout << "[MetricsHandler] Created" << std::endl;
}

void MetricsHandler::handle(IRequest& req, IResponse& res)
{
    std::cout << "[MetricsHandler] Handling metrics request" << std::endl;
    
    res.setStatus(200);
    res.setHeader("Content-Type", "text/plain; version=0.0.4");
    res.setBody(buildPrometheusMetrics());
    
    std::cout << "[MetricsHandler] Response sent" << std::endl;
}

void MetricsHandler::recordRequest(const std::string& method, const std::string& path)
{
    ++httpRequestsTotal_;
}

void MetricsHandler::recordCacheHit()
{
    ++cacheHits_;
}

void MetricsHandler::recordCacheMiss()
{
    ++cacheMisses_;
}

std::string MetricsHandler::buildPrometheusMetrics() const
{
    std::ostringstream oss;
    
    // HTTP requests total
    oss << "# HELP http_requests_total Total HTTP requests\n";
    oss << "# TYPE http_requests_total counter\n";
    oss << "http_requests_total " << httpRequestsTotal_.load() << "\n\n";
    
    // Cache hits
    oss << "# HELP cache_hits_total Cache hits\n";
    oss << "# TYPE cache_hits_total counter\n";
    oss << "cache_hits_total " << cacheHits_.load() << "\n\n";
    
    // Cache misses
    oss << "# HELP cache_misses_total Cache misses\n";
    oss << "# TYPE cache_misses_total counter\n";
    oss << "cache_misses_total " << cacheMisses_.load() << "\n";
    
    return oss.str();
}