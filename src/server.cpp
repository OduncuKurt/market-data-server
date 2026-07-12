#include "MarketDataGenerator.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>
#include <vector>

namespace
{
constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
constexpr int LISTEN_BACKLOG = 10;
constexpr auto PRICE_UPDATE_INTERVAL = std::chrono::seconds(1);

std::mutex logMutex;
}

void logMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << message << '\n';
}

bool sendAll(int socketFd, const std::string& message)
{
    std::size_t totalSentBytes = 0;

    while (totalSentBytes < message.size())
    {
        const ssize_t sentBytes = send(
            socketFd,
            message.data() + totalSentBytes,
            message.size() - totalSentBytes,
            MSG_NOSIGNAL
        );

        if (sentBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return false;
        }

        totalSentBytes += static_cast<std::size_t>(sentBytes);
    }

    return true;
}

std::string toUpper(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::toupper(character));
        }
    );

    return value;
}

std::string buildSubscriptionList(
    const std::unordered_set<std::string>& subscriptions)
{
    if (subscriptions.empty())
    {
        return "SUBSCRIPTIONS EMPTY\n";
    }

    std::vector<std::string> sortedSubscriptions(
        subscriptions.begin(),
        subscriptions.end()
    );

    std::sort(
        sortedSubscriptions.begin(),
        sortedSubscriptions.end()
    );

    std::ostringstream response;
    response << "SUBSCRIPTIONS ";

    for (std::size_t index = 0;
         index < sortedSubscriptions.size();
         ++index)
    {
        if (index > 0)
        {
            response << ',';
        }

        response << sortedSubscriptions[index];
    }

    response << '\n';

    return response.str();
}

std::string buildSymbolsResponse(
    const MarketDataGenerator& marketDataGenerator)
{
    std::vector<std::string> symbols =
        marketDataGenerator.getSymbols();

    std::sort(symbols.begin(), symbols.end());

    std::ostringstream response;
    response << "SYMBOLS ";

    for (std::size_t index = 0; index < symbols.size(); ++index)
    {
        if (index > 0)
        {
            response << ',';
        }

        response << symbols[index];
    }

    response << '\n';

    return response.str();
}

std::string buildMarketDataMessage(
    const std::unordered_set<std::string>& subscriptions,
    const MarketDataGenerator& marketDataGenerator)
{
    if (subscriptions.empty())
    {
        return {};
    }

    std::vector<std::string> sortedSubscriptions(
        subscriptions.begin(),
        subscriptions.end()
    );

    std::sort(
        sortedSubscriptions.begin(),
        sortedSubscriptions.end()
    );

    const auto now = std::chrono::system_clock::now();

    const auto timestamp = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

    std::ostringstream response;

    for (const std::string& symbol : sortedSubscriptions)
    {
        response << "MARKET_DATA "
                 << symbol
                 << ' '
                 << std::fixed
                 << std::setprecision(2)
                 << marketDataGenerator.getPrice(symbol)
                 << ' '
                 << timestamp
                 << '\n';
    }

    return response.str();
}

std::string processCommand(
    const std::string& rawCommand,
    std::unordered_set<std::string>& subscriptions,
    MarketDataGenerator& marketDataGenerator,
    bool& shouldDisconnect)
{
    std::istringstream commandStream(rawCommand);

    std::string command;
    std::string argument;
    std::string extraArgument;

    commandStream >> command;
    commandStream >> argument;
    commandStream >> extraArgument;

    command = toUpper(command);
    argument = toUpper(argument);

    if (!extraArgument.empty())
    {
        return "ERROR TOO_MANY_ARGUMENTS\n";
    }

    if (command == "PING")
    {
        return "PONG\n";
    }

    if (command == "SYMBOLS")
    {
        return buildSymbolsResponse(marketDataGenerator);
    }

    if (command == "PRICE")
    {
        if (argument.empty())
        {
            return "ERROR SYMBOL_REQUIRED\n";
        }

        if (!marketDataGenerator.hasSymbol(argument))
        {
            return "ERROR UNKNOWN_SYMBOL\n";
        }

        std::ostringstream response;

        response << "PRICE "
                 << argument
                 << ' '
                 << std::fixed
                 << std::setprecision(2)
                 << marketDataGenerator.getPrice(argument)
                 << '\n';

        return response.str();
    }

    if (command == "SUBSCRIBE")
    {
        if (argument.empty())
        {
            return "ERROR SYMBOL_REQUIRED\n";
        }

        if (!marketDataGenerator.hasSymbol(argument))
        {
            return "ERROR UNKNOWN_SYMBOL\n";
        }

        const auto insertResult = subscriptions.insert(argument);

        if (!insertResult.second)
        {
            return "ERROR ALREADY_SUBSCRIBED\n";
        }

        return "OK SUBSCRIBED " + argument + "\n";
    }

    if (command == "UNSUBSCRIBE")
    {
        if (argument.empty())
        {
            return "ERROR SYMBOL_REQUIRED\n";
        }

        const std::size_t removedCount =
            subscriptions.erase(argument);

        if (removedCount == 0)
        {
            return "ERROR NOT_SUBSCRIBED\n";
        }

        return "OK UNSUBSCRIBED " + argument + "\n";
    }

    if (command == "LIST")
    {
        return buildSubscriptionList(subscriptions);
    }

    if (command == "QUIT")
    {
        shouldDisconnect = true;
        return "BYE\n";
    }

    return "ERROR UNKNOWN_COMMAND\n";
}

void handleClient(
    int clientSocket,
    sockaddr_in clientAddress,
    MarketDataGenerator& marketDataGenerator)
{
    char clientIp[INET_ADDRSTRLEN] {};

    inet_ntop(
        AF_INET,
        &clientAddress.sin_addr,
        clientIp,
        sizeof(clientIp)
    );

    const std::string clientDescription =
        std::string(clientIp) +
        ":" +
        std::to_string(ntohs(clientAddress.sin_port));

    logMessage("İstemci bağlandı: " + clientDescription);

    const std::string welcomeMessage =
        "WELCOME MARKET_DATA_SERVER\n"
        "COMMANDS PING,SYMBOLS,PRICE,SUBSCRIBE,"
        "UNSUBSCRIBE,LIST,QUIT\n";

    if (!sendAll(clientSocket, welcomeMessage))
    {
        close(clientSocket);
        return;
    }

    std::unordered_set<std::string> subscriptions;
    std::string pendingData;
    char buffer[BUFFER_SIZE] {};

    bool shouldDisconnect = false;

    auto nextMarketDataSend =
        std::chrono::steady_clock::now() +
        PRICE_UPDATE_INTERVAL;

    while (!shouldDisconnect)
    {
        const auto now = std::chrono::steady_clock::now();

        const auto remainingTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                nextMarketDataSend - now
            );

        const int timeoutMilliseconds =
            remainingTime.count() > 0
                ? static_cast<int>(remainingTime.count())
                : 0;

        pollfd socketPollDescriptor {};

        socketPollDescriptor.fd = clientSocket;
        socketPollDescriptor.events = POLLIN;

        const int pollResult = poll(
            &socketPollDescriptor,
            1,
            timeoutMilliseconds
        );

        if (pollResult == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            break;
        }

        if (
            socketPollDescriptor.revents &
            (POLLERR | POLLHUP | POLLNVAL))
        {
            break;
        }

        if (socketPollDescriptor.revents & POLLIN)
        {
            const ssize_t receivedBytes = recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );

            if (receivedBytes == 0)
            {
                break;
            }

            if (receivedBytes == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                break;
            }

            pendingData.append(
                buffer,
                static_cast<std::size_t>(receivedBytes)
            );

            std::size_t newlinePosition = std::string::npos;

            while (
                (newlinePosition = pendingData.find('\n'))
                != std::string::npos)
            {
                std::string command = pendingData.substr(
                    0,
                    newlinePosition
                );

                pendingData.erase(
                    0,
                    newlinePosition + 1
                );

                if (!command.empty() && command.back() == '\r')
                {
                    command.pop_back();
                }

                if (command.empty())
                {
                    continue;
                }

                logMessage(
                    "[" +
                    clientDescription +
                    "] Komut: " +
                    command
                );

                const std::string response = processCommand(
                    command,
                    subscriptions,
                    marketDataGenerator,
                    shouldDisconnect
                );

                if (!sendAll(clientSocket, response))
                {
                    shouldDisconnect = true;
                    break;
                }
            }
        }

        const auto currentTime =
            std::chrono::steady_clock::now();

        if (
            !shouldDisconnect &&
            currentTime >= nextMarketDataSend)
        {
            const std::string marketDataMessage =
                buildMarketDataMessage(
                    subscriptions,
                    marketDataGenerator
                );

            if (
                !marketDataMessage.empty() &&
                !sendAll(clientSocket, marketDataMessage))
            {
                break;
            }

            do
            {
                nextMarketDataSend += PRICE_UPDATE_INTERVAL;
            }
            while (nextMarketDataSend <= currentTime);
        }
    }

    close(clientSocket);

    logMessage(
        "İstemci ayrıldı: " +
        clientDescription
    );
}

void runPriceUpdater(MarketDataGenerator& marketDataGenerator)
{
    auto nextUpdate =
        std::chrono::steady_clock::now() +
        PRICE_UPDATE_INTERVAL;

    while (true)
    {
        std::this_thread::sleep_until(nextUpdate);

        marketDataGenerator.updateAllPrices();

        nextUpdate += PRICE_UPDATE_INTERVAL;
    }
}

int main()
{
    const int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (serverSocket == -1)
    {
        std::cerr
            << "Socket oluşturulamadı: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    int reuseAddress = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) == -1)
    {
        std::cerr
            << "setsockopt başarısız: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress {};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1)
    {
        std::cerr
            << "Bind başarısız: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, LISTEN_BACKLOG) == -1)
    {
        std::cerr
            << "Listen başarısız: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    MarketDataGenerator marketDataGenerator;

    std::thread priceUpdaterThread(
        runPriceUpdater,
        std::ref(marketDataGenerator)
    );

    priceUpdaterThread.detach();

    logMessage(
        "Piyasa veri sunucusu 0.0.0.0:" +
        std::to_string(PORT) +
        " adresinde dinleniyor..."
    );

    while (true)
    {
        sockaddr_in clientAddress {};
        socklen_t clientAddressLength =
            sizeof(clientAddress);

        const int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength
        );

        if (clientSocket == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "Accept başarısız: "
                << std::strerror(errno)
                << '\n';

            continue;
        }

        std::thread clientThread(
            handleClient,
            clientSocket,
            clientAddress,
            std::ref(marketDataGenerator)
        );

        clientThread.detach();
    }

    close(serverSocket);

    return 0;
}
