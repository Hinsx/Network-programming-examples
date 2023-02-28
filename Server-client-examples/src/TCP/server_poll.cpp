#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <libgen.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include<poll.h>
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
//一个fd对应一个Data结构体
struct Data{
    using PollfdsIndex=size_t;
    // 文件描述符对应的pfd在pollfds中的位置
    PollfdsIndex index=-1;
    //待发送数据
    int ans=-1;
};

using FD=int;
map<FD, Data>fdToData;
// 保存发生事件的fd
vector<pollfd> eventPollfds;

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

    fdToData[listenfd] = {0,-1};

    vector<pollfd>fds;
    pollfd pfd;
    pfd.fd=listenfd;pfd.events=POLLIN;
    fds.push_back(pfd);
    // 接收信息
    char buf[1024]{0};
    while (1)
    {

        // 除非事件发生，否则阻塞
        cout << "Start to poll...\n";
        assert(!fds.empty());
        //时间负数表阻塞等待
        ret = poll(&(fds[0]),fds.size(),-1);
        if (ret < 0)
        {
            cout << "poll failure.\n";
            cout << "Error " << errno << ": " << strerror(errno) << ".\n";
            break;
        }
        // 拷贝发生事件的fd
        eventPollfds.clear();
        for (auto &pfd : fds)
        {
            if ((pfd.revents & POLLIN) || (pfd.revents & POLLOUT))
            {
                eventPollfds.push_back(pfd);
            }
        }

        for (auto &pfd : eventPollfds)
        {
            int fd=pfd.fd;
            if ((pfd.revents & POLLIN))
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
                    pollfd tmp;
                    tmp.fd=connfd;
                    tmp.events=POLLIN;
                    fds.push_back(tmp);
                    assert(fdToData.find(connfd)==fdToData.end());
                    fdToData[connfd] = {fds.size()-1,-1};
                }
                // 连接有信息
                else
                {
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

                        int newIndex=fdToData[fd].index;
                        std::swap(fds[newIndex],fds.back());
                        fdToData[fds[newIndex].fd].index=newIndex;

                        fds.pop_back();
                        fdToData.erase(fd);
                        
                    }
                    else
                    {
                        cout << "Rececived message:" << buf << "\n";
                        fdToData[fd].ans = addSolution(buf);
                        fds[fdToData[fd].index].events|=POLLOUT;
                    }
                }
            }
            if (pfd.revents & POLLOUT)
            {
                char message[128]{0};
                snprintf(message, sizeof(message), "%d", fdToData[fd].ans);
                size_t messageLength = strlen(message);
                int ret = send(fd, message, messageLength, 0);
                if (ret < 0)
                {
                    cout << "Error " << errno << ": " << strerror(errno) << ".\n";
                    close(fd);

                    int newIndex=fdToData[fd].index;
                    std::swap(fds[newIndex],fds.back());
                    fdToData[fds[newIndex].fd].index=newIndex;

                    fds.pop_back();
                    fdToData.erase(fd);
                    
                    continue;
                }

                fds[fdToData[fd].index].events&=~POLLOUT;

            }
        }
    }

    for(pollfd &pfd:fds){
        close(pfd.fd);
    }
}