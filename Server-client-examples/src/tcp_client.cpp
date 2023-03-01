#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<libgen.h>
#include<string.h>
#include<iostream>
#include<assert.h>
#include<unistd.h>
using std::cout;

int main(int argc,char** argv){
    if(argc<3){
        cout<<"usage:"<<basename(argv[0])<<" ip_address pot_number.\n";
        exit(1);
    }
    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    assert(sockfd>=0);

    const char* ip=argv[1];
    int port=atoi(argv[2]);
    sockaddr_in serverAddress;
    memset(&serverAddress,0,sizeof(serverAddress));
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(port);
    inet_pton(AF_INET,ip,&serverAddress.sin_addr);

    if(connect(sockfd,(sockaddr*)&serverAddress,sizeof(serverAddress))<0){
        cout<<"connection failed.\n";
    }
    else{
        const char* problem="1 + 1 = ?";
        cout<<"Trying to send data...\n";
        int n=send(sockfd,problem,strlen(problem),0);
        cout<<"Send "<<n<<" bytes.\n";
        char ans[255]{0};
        ssize_t ret=0;
        cout<<"Trying to receive data...\n";
        recv(sockfd,ans,sizeof(ans)-1,0);
        
        if(atoi((const char*)ans)==2){
            cout<<"Success.\n";
        }
        else cout<<"Failed.\n";
    }

    close(sockfd);
}