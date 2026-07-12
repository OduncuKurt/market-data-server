#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <unordered_set>
#include <sys/socket.h>
#include <unistd.h>
#include "MarketDataGenerator.hpp"
#include <iomanip>
namespace
{
constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
constexpr int LISTEN_BACKLOG = 5;
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
            0
        );

        if (sentBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "Veri gönderme hatası: "
                      << std::strerror(errno)
                      << '\n';

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

    std::string response = "SUBSCRIPTIONS ";
    bool firstSymbol = true;

    for (const std::string& symbol : subscriptions)
    {
        if (!firstSymbol)
        {
            response += ',';
        }

        response += symbol;
        firstSymbol = false;
    }

    response += '\n';

    return response;
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

    commandStream >> command;
    commandStream >> argument;

    command = toUpper(command);
    argument = toUpper(argument);

    if (command == "PING")
    {
        return "PONG\n";
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

    	const double price = marketDataGenerator.updatePrice(argument);

	std::ostringstream response;

    	response << "PRICE "
                 << argument
      	  	 << ' '
             	 << std::fixed
             	 << std::setprecision(2)
            	 << price
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

        const auto [iterator, inserted] = subscriptions.insert(argument);
        static_cast<void>(iterator);

        if (!inserted)
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

        const std::size_t removedCount = subscriptions.erase(argument);

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

if (command == "SYMBOLS")
{
    const std::vector<std::string> symbols =
        marketDataGenerator.getSymbols();

    std::string response = "SYMBOLS ";

    for (std::size_t index = 0; index < symbols.size(); ++index)
    {
        if (index > 0)
        {
            response += ',';
        }

        response += symbols[index];
    }

    response += '\n';

    return response;
}
    if (command == "QUIT")
    {
        shouldDisconnect = true;
        return "BYE\n";
    }

    return "ERROR UNKNOWN_COMMAND\n";
}

int main()
{
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Socket oluşturulamadı: "
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
        std::cerr << "setsockopt başarısız: "
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
        std::cerr << "Bind başarısız: "
                  << std::strerror(errno)
                  << '\n';

        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, LISTEN_BACKLOG) == -1)
    {
        std::cerr << "Listen başarısız: "
                  << std::strerror(errno)
                  << '\n';

        close(serverSocket);
        return 1;
    }

    std::cout << "Piyasa veri sunucusu 0.0.0.0:"
              << PORT
              << " adresinde dinleniyor..."
              << '\n';

    sockaddr_in clientAddress {};
    socklen_t clientAddressLength = sizeof(clientAddress);

    const int clientSocket = accept(
        serverSocket,
        reinterpret_cast<sockaddr*>(&clientAddress),
        &clientAddressLength
    );

    if (clientSocket == -1)
    {
        std::cerr << "Accept başarısız: "
                  << std::strerror(errno)
                  << '\n';

        close(serverSocket);
        return 1;
    }

    char clientIp[INET_ADDRSTRLEN] {};

    const char* convertedIp = inet_ntop(
        AF_INET,
        &clientAddress.sin_addr,
        clientIp,
        sizeof(clientIp)
    );

    if (convertedIp != nullptr)
    {
        std::cout << "İstemci bağlandı: "
                  << clientIp
                  << ':'
                  << ntohs(clientAddress.sin_port)
                  << '\n';
    }

    const std::string welcomeMessage =
        "WELCOME MARKET_DATA_SERVER\n"
        "COMMANDS PING,SYMBOLS,PRICE,SUBSCRIBE,UNSUBSCRIBE,LIST,QUIT\n";

    if (!sendAll(clientSocket, welcomeMessage))
    {
        close(clientSocket);
        close(serverSocket);
        return 1;
    }

    std::unordered_set<std::string> subscriptions;
    MarketDataGenerator marketDataGenerator;
    std::string pendingData;
    char buffer[BUFFER_SIZE] {};

    bool shouldDisconnect = false;

    while (!shouldDisconnect)
    {
        const ssize_t receivedBytes = recv(
            clientSocket,
            buffer,
            sizeof(buffer),
            0
        );

        if (receivedBytes == 0)
        {
            std::cout << "İstemci bağlantıyı kapattı."
                      << '\n';

            break;
        }

        if (receivedBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "Veri alma hatası: "
                      << std::strerror(errno)
                      << '\n';

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

            std::cout << "Komut alındı: "
                      << command
                      << '\n';

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

    close(clientSocket);
    close(serverSocket);

    std::cout << "Sunucu kapatıldı."
              << '\n';

    return 0;
}
