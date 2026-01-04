cmake_minimum_required(VERSION 3.15)

project(root-trading-platform
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# КРИТИЧНО: включить тестовую инфраструктуру на верхнем уровне
# чтобы CTest видел тесты из всех подпроектов
enable_testing()

# общая библиотека со специальными стурктурами, например, потокобезопасная карта
add_subdirectory(common)

# проект HelloWorld - стартовый проект для проверки коррктного подключения библиотек и фреймворков
#add_subdirectory(hello-world)

# проект MVP - минимальный реально работающий проект торговой платформы
#add_subdirectory(mvp)

# Проект education - то, что будет на защите
add_subdirectory(education)

