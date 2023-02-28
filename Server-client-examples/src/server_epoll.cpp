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
    auto &ptr = fdToData[listenfd] = std::make_unique<Data>(listenfd,true,false);
    // epoll内核事件表
    int epollfd = epoll_create(5);
    epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = ptr.get();
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);
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
                    assert(fdToData.find(connfd) == fdToData.end());
                    auto &ptr = fdToData[connfd] = std::make_unique<Data>(connfd,true);
                    epoll_event tmp;
                    tmp.events = EPOLLIN;
                    tmp.data.ptr = ptr.get();
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &tmp);
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
                        data->write=true;

                        epoll_event tmp;
                        memset(&tmp,0,sizeof(tmp));
                        if(data->read)tmp.events |= EPOLLIN;
                        if(data->write)tmp.events |= EPOLLOUT;
                        tmp.data=event.data;
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
                data->write=false;
                epoll_event tmp;
                memset(&tmp,0,sizeof(tmp));
                if(data->read)tmp.events |= EPOLLIN;
                if(data->write)tmp.events |= EPOLLOUT;
                tmp.data=event.data;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &tmp);
            }
        }
    }

    for (auto &data : fdToData)
    {
        close((data.second)->fd);
    }
}