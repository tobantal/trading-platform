#pragma once

#include <stdexcept>
#include <string>

/**
 * @file CommandException.hpp
 * @brief Исключение для команд
 * @author Anton Tobolkin
 * @version 1.0
 */

 /**
  * @brief Исключение, выбрасываемое при ошибках выполнения команд
  */
class CommandException : public std::runtime_error {
public:
    explicit CommandException(const std::string& message)
        : std::runtime_error(message) {}
};