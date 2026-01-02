#include "BrokerApp.hpp"
#include <iostream>
#include <csignal>

// Global pointer for signal handler
broker::BrokerApp* g_app = nullptr;

void signalHandler(int signal) {
    std::cout << "\n[main] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_app) {
        g_app->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        broker::BrokerApp app;
        g_app = &app;

        // Signal handlers для graceful shutdown в Kubernetes
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "========================================" << std::endl;
        std::cout << "  Broker Service v1.0.0 Starting" << std::endl;
        std::cout << "  Press Ctrl+C to stop" << std::endl;
        std::cout << "========================================" << std::endl;

        // Template Method вызывает:
        // 1. loadEnvironment()
        // 2. configureInjection()
        // 3. start()
        app.run(argc, argv);

        g_app = nullptr;
        std::cout << "[main] Broker Service stopped" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
