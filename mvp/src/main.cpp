#include "TradingApp.hpp"
#include <iostream>
#include <csignal>

// Глобальный указатель для обработки сигналов
TradingApp* g_app = nullptr;

void signalHandler(int signal)
{
    std::cout << "\n[main] Received signal " << signal << std::endl;
    if (g_app)
    {
        g_app->stop();
    }
}

int main(int argc, char* argv[])
{
    try
    {
        // Создаём приложение
        TradingApp app;
        g_app = &app;

        // Устанавливаем обработчики сигналов для graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "========================================" << std::endl;
        std::cout << "  Trading Platform MVP Starting" << std::endl;
        std::cout << "  Press Ctrl+C to stop" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;

        // Запускаем приложение (Template Method вызывает:
        // 1. loadEnvironment()
        // 2. configureInjection()
        // 3. start()
        app.run(argc, argv);

        std::cout << "\n========================================" << std::endl;
        std::cout << "  Trading Platform MVP Stopped" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
