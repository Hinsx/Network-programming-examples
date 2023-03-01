#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <libgen.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <map>
using std::cout;
using std::map;
using std::max;
using std::vector;
int addSolution(char *problem)
{
    int addition_1 = 0;
    bool flag = false;
    int addition_2 = 0;
    char *cur = problem;
    while (*cur != '\0')
    {
        if (*cur == ' ')
        {
            cur++;
            continue;
        }
        if (*cur >= '0' && *cur <= '9')
        {
            if (!flag)
            {
                while (*cur >= '0' && *cur <= '9')
                {
                    addition_1 = addition_1 * 10 + (*cur) - '0';
                    cur++;
                }
                flag = true;
            }
            else
            {
                while (*cur >= '0' && *cur <= '9')
                {
                    addition_2 = addition_2 * 10 + (*cur) - '0';
                    cur++;
                }
            }
        }
        *cur = '\0';
        cur++;
    }
    return addition_1 + addition_2;
}
// 维护每个fd的感兴趣事件
struct Channel
{
    int fd;
    bool read;
    bool write;
    int ans = -1;
    Channel(int sock = 0, bool a = 0, bool b = 0) : fd(sock), read(a), write(b) {}
};
// 维护建立的tcp连接,便于增添和删除
map<int, Channel> channels;
// 保存发生事件的fd
vector<Channel> eventChannels;

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cout << "usage:" << basename(argv[0]) << " ip_address pot_number.\n";
    }
    int tcpListenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(tcpListenfd >= 0);
    int udpListenfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpListenfd >= 0);
    // tcp设置地址可重用便于调试
    int reuse = 1;
    setsockopt(tcpListenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

    int ret = bind(tcpListenfd, (sockaddr *)&serverAddress, sizeof(serverAddress));
    if (ret < 0)
    {
        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
    }
    assert(ret != -1);
    ret = bind(udpListenfd, (sockaddr *)&serverAddress, sizeof(serverAddress));
    if (ret < 0)
    {
        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
    }
    assert(ret != -1);
    // udp不需要listen
    ret = listen(tcpListenfd, 5);
    assert(ret != -1);

    channels[tcpListenfd] = {tcpListenfd, true, false};
    channels[udpListenfd] = {udpListenfd, true, false};

    fd_set read_fds;
    fd_set write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    // 接收信息
    char buf[1024]{0};
    while (1)
    {
        // 每次select前必须重新设置fds_set，因为事件发生后内核会修改
        // 某些fd已经关闭，不能监听
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        for (auto &item : channels)
        {
            Channel &channel = item.second;
            int fd = channel.fd;
            if (channel.read)
                FD_SET(fd, &read_fds);
            if (channel.write)
                FD_SET(fd, &write_fds);
        }
        // 除非事件发生，否则阻塞
        cout << "Start to select...\n";
        ret = select(1024, &read_fds, &write_fds, nullptr, nullptr);
        if (ret < 0)
        {
            cout << "select failure.\n";
            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
            break;
        }
        // 拷贝发生事件的fd
        eventChannels.clear();
        for (auto &item : channels)
        {
            Channel &channel = item.second;
            int fd = channel.fd;
            if (FD_ISSET(fd, &read_fds) || FD_ISSET(fd, &write_fds))
            {
                eventChannels.push_back(channel);
            }
        }

        for (auto &channel : eventChannels)
        {
            int fd = channel.fd;
            if (FD_ISSET(fd, &read_fds))
            {
                // 接收新连接
                if (fd == tcpListenfd)
                {
                    sockaddr_in clientAddress;
                    socklen_t clientAddressLength = sizeof(clientAddress);
                    int connfd = accept(fd, (sockaddr *)&clientAddress, &clientAddressLength);

                    if (connfd < 0)
                    {
                        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                        close(tcpListenfd);
                        break;
                    }

                    // 打印连接地址
                    char *clientAddressString = inet_ntoa(clientAddress.sin_addr);
                    int clientAddressPort = ntohs(clientAddress.sin_port);
                    cout << "Accept new connection--->[" << clientAddressString << ":" << clientAddressPort << "]\n";
                    // 构造连接,监听数据
                    channels[connfd] = {connfd, true, false};
                }
                // 收到udp数据报
                else if (fd == udpListenfd)
                {
                    cout << "Trying to receive udp data...\n";
                    memset(buf, 0, sizeof(buf));
                    sockaddr_in fromAddress;
                    socklen_t fromAddressLength = sizeof(fromAddress);
                    // 使用recvfrom而不是recv，会接受任何udp数据报，并将地址解析到后两个参数
                    // recv(sockfd,ans,sizeof(ans)-1,0);
                    int n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (sockaddr *)&fromAddress, &fromAddressLength);
                    // 打印连接地址
                    char *fromAddressString = inet_ntoa(fromAddress.sin_addr);
                    int fromAddressPort = ntohs(fromAddress.sin_port);
                    cout << "Received UDP datagram from [" << fromAddressString << ":" << fromAddressPort << "]\n";
                    if (n == -1)
                    {
                        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                    }
                    else
                    {
                        cout << "Rececived message:" << buf << "\n";
                        char message[128]{0};
                        snprintf(message, sizeof(message), "%d", addSolution(buf));
                        size_t messageLength = strlen(message);
                        int ret = sendto(fd, message, messageLength, 0,(const sockaddr *)&fromAddress, fromAddressLength);
                        if (ret < 0)
                        {
                            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                        }
                    }
                }
                // 维护的tcp连接有信息
                else
                {
                    cout << "Trying to receive tcp data...\n";
                    memset(buf, 0, sizeof(buf));
                    ret = recv(fd, buf, sizeof(buf) - 1, 0);
                    cout << "Received " << ret << " bytes.\n";
                    if (ret <= 0)
                    {
                        if (ret == -1)
                        {
                            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                        }
                        cout << "Close connection.\n";
                        close(fd);
                        channels.erase(fd);
                    }
                    else
                    {
                        cout << "Rececived message:" << buf << "\n";
                        channels[fd].ans = addSolution(buf);
                        channels[fd].write = true;
                    }
                }
            }
            if (FD_ISSET(fd, &write_fds))
            {
                char message[128]{0};
                snprintf(message, sizeof(message), "%d", channels[fd].ans);
                size_t messageLength = strlen(message);
                int ret = send(fd, message, messageLength, 0);
                if (ret < 0)
                {
                    cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                    close(fd);
                    channels.erase(fd);
                    continue;
                }

                channels[fd].write = false;
            }
        }
    }

    for (auto &item : channels)
    {
        close(item.second.fd);
    }
}