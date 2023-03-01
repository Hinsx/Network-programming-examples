#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <libgen.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <cmath>
#include <map>
#include <memory>
using std::cout;
using std::map;
using std::max;
using std::vector;
const int MAXEVENTSIZE = 16;
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
// 一个fd对应一个Data结构体
struct Data
{
    int fd = -1;
    // 读写感兴趣
    bool read = false;
    bool write = false;
    // 待发送数据
    int ans = -1;

    Data(int a = -1, bool b = false, bool c = false, int d = -1) : fd(a), read(b), write(c), ans(d) {}
};

using FD = int;
using DataPtr = std::unique_ptr<Data>;
map<FD, DataPtr> fdToData;
// 保存发生事件的epoll_event
vector<epoll_event> epollEvents(MAXEVENTSIZE);

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cout << "usage:" << basename(argv[0]) << " ip_address pot_number.\n";
    }
    int tcpListenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(tcpListenfd >= 0);
    // 设置地址可重用便于调试
    int reuse = 1;
    setsockopt(tcpListenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int udpListenfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpListenfd >= 0);

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

    ret = listen(tcpListenfd, 5);
    assert(ret != -1);
    // epoll内核事件表
    int epollfd = epoll_create(5);

    auto &ptr1 = fdToData[tcpListenfd] = std::make_unique<Data>(tcpListenfd, true, false);
    epoll_event event1;
    event1.events = EPOLLIN;
    event1.data.ptr = ptr1.get();
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tcpListenfd, &event1);

    auto &ptr2 = fdToData[udpListenfd] = std::make_unique<Data>(udpListenfd, true, false);
    epoll_event event2;
    event2.events = EPOLLIN;
    event2.data.ptr = ptr2.get();
    epoll_ctl(epollfd, EPOLL_CTL_ADD, udpListenfd, &event2);
    // 接收信息
    char buf[1024]{0};
    while (1)
    {

        // 除非事件发生，否则阻塞
        cout << "Start to epoll...\n";
        assert(!epollEvents.empty());
        // 时间负数表阻塞等待
        int num = epoll_wait(epollfd, &(epollEvents[0]), epollEvents.size(), -1);
        if (num < 0)
        {
            cout << "epoll failure.\n";
            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
            break;
        }
        for (int i = 0; i < num; i++)
        {
            auto &event = epollEvents[i];
            Data *data = reinterpret_cast<Data *>(event.data.ptr);
            int fd = data->fd;
            if ((event.events & EPOLLIN))
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
                    assert(fdToData.find(connfd) == fdToData.end());
                    auto &ptr = fdToData[connfd] = std::make_unique<Data>(connfd, true);
                    epoll_event tmp;
                    tmp.events = EPOLLIN;
                    tmp.data.ptr = ptr.get();
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &tmp);
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
                        int ret = sendto(fd, message, messageLength, 0, (const sockaddr *)&fromAddress, fromAddressLength);
                        if (ret < 0)
                        {
                            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                        }
                    }
                }
                // 连接有信息
                else
                {
                    assert(fdToData.find(fd) != fdToData.end());
                    cout << "Trying to receive data...\n";
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

                        fdToData.erase(fd);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
                    }
                    else
                    {
                        cout << "Rececived message:" << buf << "\n";
                        data->ans = addSolution(buf);
                        data->write = true;

                        epoll_event tmp;
                        memset(&tmp, 0, sizeof(tmp));
                        if (data->read)
                            tmp.events |= EPOLLIN;
                        if (data->write)
                            tmp.events |= EPOLLOUT;
                        tmp.data = event.data;
                        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &tmp);
                    }
                }
            }
            if (event.events & EPOLLOUT)
            {
                char message[128]{0};
                snprintf(message, sizeof(message), "%d", data->ans);
                size_t messageLength = strlen(message);
                int ret = send(fd, message, messageLength, 0);
                if (ret < 0)
                {
                    cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                    close(fd);

                    fdToData.erase(fd);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
                    continue;
                }
                data->write = false;
                epoll_event tmp;
                memset(&tmp, 0, sizeof(tmp));
                if (data->read)
                    tmp.events |= EPOLLIN;
                if (data->write)
                    tmp.events |= EPOLLOUT;
                tmp.data = event.data;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &tmp);
            }
        }
    }

    for (auto &data : fdToData)
    {
        close((data.second)->fd);
    }
}