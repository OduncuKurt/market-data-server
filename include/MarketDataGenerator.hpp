#pragma once

#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class MarketDataGenerator
{
public:
    MarketDataGenerator();

    bool hasSymbol(const std::string& symbol) const;

    double getPrice(const std::string& symbol) const;

    double updatePrice(const std::string& symbol);

    void updateAllPrices();

    std::vector<std::string> getSymbols() const;

private:
    mutable std::mutex pricesMutex_;

    std::unordered_map<std::string, double> prices_;

    std::mt19937 randomEngine_;

    std::uniform_real_distribution<double> priceChangeDistribution_;
};
