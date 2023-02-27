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
    Channel(int sock=0, bool a=0, bool b=0) : fd(sock), read(a), write(b) {}
};
// 维护建立的连接,便于增添和删除
map<int, Channel> channels;
// 保存发生事件的fd
vector<Channel> eventChannels;

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cout << "usage:" << basename(argv[0]) << " ip_address pot_number.\n";
    }
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    // 设置地址可重用便于调试
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

    int ret = bind(listenfd, (sockaddr *)&serverAddress, sizeof(serverAddress));
    if (ret < 0)
    {
        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
    }
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    channels[listenfd] = {listenfd, true, false};

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
                if (fd == listenfd)
                {
                    sockaddr_in clientAddress;
                    socklen_t clientAddressLength = sizeof(clientAddress);
                    int connfd = accept(fd, (sockaddr *)&clientAddress, &clientAddressLength);

                    if (connfd < 0)
                    {
                        cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                        close(listenfd);
                        break;
                    }

                    // 打印连接地址
                    char *clientAddressString = inet_ntoa(clientAddress.sin_addr);
                    int clientAddressPort = ntohs(clientAddress.sin_port);
                    cout << "Accept new connection--->[" << clientAddressString << ":" << clientAddressPort << "]\n";
                    // 构造连接,监听数据
                    channels[connfd] = {connfd, true, false};
                }
                // 连接有信息
                else
                {
                    cout << "Trying to receive data...\n";
                    memset(buf, 0, sizeof(buf));
                    ret = recv(fd, buf, sizeof(buf) - 1, 0);
                    cout << "Received " << ret << " bytes.\n";
                    if (ret == 0)
                    {
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
                int len = 0;
                while ((len += send(fd, message + len, messageLength - len, 0)) < messageLength)
                {
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