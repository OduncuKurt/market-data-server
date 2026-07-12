#include "MarketDataGenerator.hpp"

#include <stdexcept>

MarketDataGenerator::MarketDataGenerator()
    : prices_ {
          {"THYAO", 312.50},
          {"ASELS", 178.20},
          {"GARAN", 142.80},
          {"AKBNK", 67.40},
          {"SISE", 41.15}
      },
      randomEngine_(std::random_device {}()),
      priceChangeDistribution_(-0.50, 0.50)
{
}

bool MarketDataGenerator::hasSymbol(const std::string& symbol) const
{
    std::lock_guard<std::mutex> lock(pricesMutex_);

    return prices_.find(symbol) != prices_.end();
}

double MarketDataGenerator::getPrice(const std::string& symbol) const
{
    std::lock_guard<std::mutex> lock(pricesMutex_);

    const auto iterator = prices_.find(symbol);

    if (iterator == prices_.end())
    {
        throw std::invalid_argument("Unknown market symbol");
    }

    return iterator->second;
}

double MarketDataGenerator::updatePrice(const std::string& symbol)
{
    std::lock_guard<std::mutex> lock(pricesMutex_);

    const auto iterator = prices_.find(symbol);

    if (iterator == prices_.end())
    {
        throw std::invalid_argument("Unknown market symbol");
    }

    double& currentPrice = iterator->second;

    currentPrice += priceChangeDistribution_(randomEngine_);

    if (currentPrice < 0.01)
    {
        currentPrice = 0.01;
    }

    return currentPrice;
}

void MarketDataGenerator::updateAllPrices()
{
    std::lock_guard<std::mutex> lock(pricesMutex_);

    for (auto& [symbol, price] : prices_)
    {
        static_cast<void>(symbol);

        price += priceChangeDistribution_(randomEngine_);

        if (price < 0.01)
        {
            price = 0.01;
        }
    }
}

std::vector<std::string> MarketDataGenerator::getSymbols() const
{
    std::lock_guard<std::mutex> lock(pricesMutex_);

    std::vector<std::string> symbols;
    symbols.reserve(prices_.size());

    for (const auto& [symbol, price] : prices_)
    {
        static_cast<void>(price);
        symbols.push_back(symbol);
    }

    return symbols;
}
