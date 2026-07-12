#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
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

bool receiveResponse(int socketFd)
{
    char buffer[BUFFER_SIZE] {};

    const ssize_t receivedBytes = recv(
        socketFd,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (receivedBytes == 0)
    {
        std::cout << "Sunucu bağlantıyı kapattı."
                  << '\n';

        return false;
    }

    if (receivedBytes == -1)
    {
        if (errno == EINTR)
        {
            return true;
        }

        std::cerr << "Veri alma hatası: "
                  << std::strerror(errno)
                  << '\n';

        return false;
    }

    buffer[receivedBytes] = '\0';

    std::cout << buffer;

    if (buffer[receivedBytes - 1] != '\n')
    {
        std::cout << '\n';
    }

    return true;
}

int main()
{
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1)
    {
        std::cerr << "Socket oluşturulamadı: "
                  << std::strerror(errno)
                  << '\n';

        return 1;
    }

    sockaddr_in serverAddress {};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    const int conversionResult = inet_pton(
        AF_INET,
        SERVER_IP,
        &serverAddress.sin_addr
    );

    if (conversionResult <= 0)
    {
        std::cerr << "IP adresi dönüştürülemedi."
                  << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout << SERVER_IP
              << ':'
              << SERVER_PORT
              << " adresine bağlanılıyor..."
              << '\n';

    if (connect(
            clientSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1)
    {
        std::cerr << "Sunucuya bağlanılamadı: "
                  << std::strerror(errno)
                  << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout << "Sunucuya bağlanıldı."
              << '\n';

    if (!receiveResponse(clientSocket))
    {
        close(clientSocket);
        return 1;
    }

    std::string command;

    while (true)
    {
        std::cout << "> ";

        if (!std::getline(std::cin, command))
        {
            std::cout << "\nGirdi akışı kapatıldı."
                      << '\n';

            break;
        }

        if (command.empty())
        {
            continue;
        }

        const std::string message = command + '\n';

        if (!sendAll(clientSocket, message))
        {
            break;
        }

        if (!receiveResponse(clientSocket))
        {
            break;
        }

        if (command == "QUIT" || command == "quit")
        {
            break;
        }
    }

    close(clientSocket);

    std::cout << "İstemci kapatıldı."
              << '\n';

    return 0;
}
