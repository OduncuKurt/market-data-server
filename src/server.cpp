#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
constexpr int LISTEN_BACKLOG = 5;
}

int main()
{
    // IPv4 ve TCP kullanan bir socket oluşturur.
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::cerr << "Socket oluşturulamadı: "
                  << std::strerror(errno)
                  << '\n';

        return 1;
    }

    // Program kapatılıp hemen tekrar açıldığında
    // aynı portun yeniden kullanılabilmesini sağlar.
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

    // Socket'i IP adresi ve porta bağlar.
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

    // Socket'i bağlantı dinleyen bir sunucu socket'ine dönüştürür.
    if (listen(serverSocket, LISTEN_BACKLOG) == -1)
    {
        std::cerr << "Listen başarısız: "
                  << std::strerror(errno)
                  << '\n';

        close(serverSocket);
        return 1;
    }

    std::cout << "Sunucu 0.0.0.0:"
              << PORT
              << " adresinde dinleniyor..."
              << '\n';

    sockaddr_in clientAddress {};
    socklen_t clientAddressLength = sizeof(clientAddress);

    // Yeni istemci bağlantısı gelene kadar burada bekler.
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
    else
    {
        std::cout << "Bir istemci bağlandı."
                  << '\n';
    }

    char buffer[BUFFER_SIZE] {};

    while (true)
    {
        const ssize_t receivedBytes = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
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
            std::cerr << "Veri alma hatası: "
                      << std::strerror(errno)
                      << '\n';

            break;
        }

        buffer[receivedBytes] = '\0';

        std::cout << "Alınan mesaj: "
                  << buffer;

        // recv() gelen mesajın sonunda yeni satır olduğunu garanti etmez.
        if (buffer[receivedBytes - 1] != '\n')
        {
            std::cout << '\n';
        }

        ssize_t totalSentBytes = 0;

        // send() tek çağrıda tüm veriyi göndermek zorunda değildir.
        while (totalSentBytes < receivedBytes)
        {
            const ssize_t sentBytes = send(
                clientSocket,
                buffer + totalSentBytes,
                receivedBytes - totalSentBytes,
                0
            );

            if (sentBytes == -1)
            {
                std::cerr << "Veri gönderme hatası: "
                          << std::strerror(errno)
                          << '\n';

                close(clientSocket);
                close(serverSocket);
                return 1;
            }

            totalSentBytes += sentBytes;
        }
    }

    close(clientSocket);
    close(serverSocket);

    std::cout << "Sunucu kapatıldı."
              << '\n';

    return 0;
}
