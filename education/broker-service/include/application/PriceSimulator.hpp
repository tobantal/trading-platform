// include/application/PriceSimulator.hpp
#pragma once

#include "adapters/secondary/broker/PriceSimulator.hpp"

namespace broker::application {

// Re-export from adapters
using PriceSimulator = adapters::secondary::PriceSimulator;

} // namespace broker::application
