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

bool sendAll(int socketFd, const char* data, std::size_t dataLength)
{
    std::size_t totalSentBytes = 0;

    while (totalSentBytes < dataLength)
    {
        const ssize_t sentBytes = send(
            socketFd,
            data + totalSentBytes,
            dataLength - totalSentBytes,
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

    if (conversionResult == 0)
    {
        std::cerr << "Geçersiz IP adresi: "
                  << SERVER_IP
                  << '\n';

        close(clientSocket);
        return 1;
    }

    if (conversionResult == -1)
    {
        std::cerr << "IP adresi dönüştürme hatası: "
                  << std::strerror(errno)
                  << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout << SERVER_IP
              << ':'
              << SERVER_PORT
              << " adresine bağlanılıyor..."
              << '\n';

    const int connectionResult = connect(
        clientSocket,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)
    );

    if (connectionResult == -1)
    {
        std::cerr << "Sunucuya bağlanılamadı: "
                  << std::strerror(errno)
                  << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout << "Sunucuya bağlanıldı."
              << '\n';

    std::cout << "Mesaj yaz. Çıkmak için 'quit' yaz."
              << '\n';

    std::string message;
    char buffer[BUFFER_SIZE] {};

    while (true)
    {
        std::cout << "> ";

        if (!std::getline(std::cin, message))
        {
            std::cout << "\nGirdi akışı kapatıldı."
                      << '\n';

            break;
        }

        if (message == "quit")
        {
            std::cout << "Bağlantı kapatılıyor..."
                      << '\n';

            break;
        }

        if (message.empty())
        {
            continue;
        }

        message.push_back('\n');

        if (!sendAll(
                clientSocket,
                message.data(),
                message.size()))
        {
            break;
        }

        const ssize_t receivedBytes = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (receivedBytes == 0)
        {
            std::cout << "Sunucu bağlantıyı kapattı."
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

        buffer[receivedBytes] = '\0';

        std::cout << "Sunucudan gelen: "
                  << buffer;

        if (buffer[receivedBytes - 1] != '\n')
        {
            std::cout << '\n';
        }
    }

    close(clientSocket);

    std::cout << "İstemci kapatıldı."
              << '\n';

    return 0;
}
