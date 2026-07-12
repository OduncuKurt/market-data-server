#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
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
            MSG_NOSIGNAL
        );

        if (sentBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "Veri gönderme hatası: "
                << std::strerror(errno)
                << '\n';

            return false;
        }

        totalSentBytes +=
            static_cast<std::size_t>(sentBytes);
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
            return static_cast<char>(
                std::toupper(character)
            );
        }
    );

    return value;
}

void receiveMessages(
    int socketFd,
    std::atomic<bool>& running)
{
    char buffer[BUFFER_SIZE] {};

    while (running)
    {
        const ssize_t receivedBytes = recv(
            socketFd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (receivedBytes == 0)
        {
            std::cout
                << "\nSunucu bağlantıyı kapattı."
                << '\n';

            running = false;
            break;
        }

        if (receivedBytes == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (running)
            {
                std::cerr
                    << "\nVeri alma hatası: "
                    << std::strerror(errno)
                    << '\n';
            }

            running = false;
            break;
        }

        buffer[receivedBytes] = '\0';

        std::cout
            << '\n'
            << buffer
            << "> "
            << std::flush;
    }
}

int main()
{
    const int clientSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (clientSocket == -1)
    {
        std::cerr
            << "Socket oluşturulamadı: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    sockaddr_in serverAddress {};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &serverAddress.sin_addr) <= 0)
    {
        std::cerr
            << "IP adresi dönüştürülemedi."
            << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout
        << SERVER_IP
        << ':'
        << SERVER_PORT
        << " adresine bağlanılıyor..."
        << '\n';

    if (connect(
            clientSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1)
    {
        std::cerr
            << "Sunucuya bağlanılamadı: "
            << std::strerror(errno)
            << '\n';

        close(clientSocket);
        return 1;
    }

    std::cout
        << "Sunucuya bağlanıldı."
        << '\n';

    std::atomic<bool> running {true};

    std::thread receiverThread(
        receiveMessages,
        clientSocket,
        std::ref(running)
    );

    std::string command;

    while (running)
    {
        std::cout << "> ";

        if (!std::getline(std::cin, command))
        {
            command = "QUIT";
        }

        if (command.empty())
        {
            continue;
        }

        const std::string message = command + '\n';

        if (!sendAll(clientSocket, message))
        {
            running = false;
            break;
        }

        if (toUpper(command) == "QUIT")
        {
            break;
        }
    }

    if (!running)
    {
        shutdown(clientSocket, SHUT_RDWR);
    }

    if (receiverThread.joinable())
    {
        receiverThread.join();
    }

    close(clientSocket);

    std::cout
        << "İstemci kapatıldı."
        << '\n';

    return 0;
}
