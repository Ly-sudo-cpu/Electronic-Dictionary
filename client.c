#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define N 32
#define R 1 // 用户注册
#define L 2 // 用户登录 
#define Q 3 // 单词查询
#define H 4 // 历史记录查询
 
// 通信消息结构体
typedef struct{
    int type;          // 消息类型（注册/登录/查询/历史）
    char name[N];      // 用户名
    char data[256];    // 密码/单词/结果等数据
} MSG;

// 注册功能
void do_register(int sockfd, MSG *msg) {
    msg->type = R;
    printf("Input name: ");
    scanf("%s", msg->name);
    getchar();  // 清理输入缓冲区换行符
    
    printf("Input passwd: ");
    scanf("%s", msg->data);
    getchar();  // 清理输入缓冲区换行符

    // 发送注册请求
    if (send(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("注册请求发送失败\n");
        return;
    }

    // 接收注册结果
    if (recv(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("注册结果接收失败\n");
        return;
    }

    printf("%s\n", msg->data);
}

// 登录功能（返回1表示成功，0表示失败）
int do_login(int sockfd, MSG *msg) {
    msg->type = L;
    printf("Input name: ");
    scanf("%s", msg->name);
    getchar();  // 清理输入缓冲区换行符
    
    printf("Input passwd: ");
    scanf("%s", msg->data);
        // 发送注册请求
    if (send(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("注册请求发送失败\n");
        return -1;
    }

    // 接收注册结果
    if (recv(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("注册结果接收失败\n");
        return -1;
    }
    printf("%s\n", msg->data);
// 原代码里的返回逻辑有问题，修正如下：
if (strncmp(msg->data, "Login successful", 16) == 0) { 
    printf("Login ok!\n");
    return 1;  // 确保登录成功返回 1
} else {
    printf("%s\n", msg->data);
    return 0;
}
    
    
    return 0;
}

// 单词查询功能
void do_query(int sockfd, MSG *msg)
{
    msg->type = Q;

    while(1)
    {
    printf("Input word: ");
    scanf("%s", msg->data);
    //客户端，输入#号返回上一级菜单
    if(strncmp(msg->data,"#",1)==0)
    {
        break;
    }
    //将要查询的单词发送给服务器
    if(send(sockfd, msg, sizeof(MSG), 0)<0)
    {
         printf("Failed to send.\n");
        return -1;
    }
    //等待接收服务器返回的结果
    if(recv(sockfd, msg, sizeof(MSG), 0)<0)
    {
        printf("Failed to recv.\n");
        return -1;
    }
    printf("%s\n",msg->data);
    //getchar();  // 清理输入缓冲区换行符
    }
    return 1;
}

// 历史记录查询功能
void do_history(int sockfd, MSG *msg) {
    msg->type = H;

    // 发送历史查询请求
    if (send(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("历史记录请求发送失败\n");
        return;
    }

    // 接收历史记录结果
    if (recv(sockfd, msg, sizeof(MSG), 0) < 0) {
        printf("历史记录接收失败\n");
        return;
    }

    printf("History records:\n%s\n", msg->data);
}

int main(int argc, const char *argv[]) {
    int sockfd;          // 套接字描述符
    int n;               // 用户输入的功能选项
    MSG msg;             // 通信消息结构体
    struct sockaddr_in serveraddr;  // 服务器地址

    // 检查命令行参数（需要服务器IP和端口）
    if (argc != 3) {
        printf("Usage: %s serverip port\n", argv[0]);
        return -1;
    }

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("套接字创建失败");
        return -1;
    }

    // 初始化服务器地址
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);  // 服务器IP
    serveraddr.sin_port = htons(atoi(argv[2]));       // 服务器端口

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("服务器连接失败");
        return -1;
    }

   // 一级菜单（注册/登录/退出）
int login_success = 0;  // 新增：登录状态标志
while (1) {
    printf("*******************************\n");
    printf("* 1.register  2.login  3.quit  *\n");
    printf("*******************************\n");
    printf("Please choose: ");
    scanf("%d", &n);
    getchar();  

    switch (n) {
        case 1:
            do_register(sockfd, &msg);
            break;
        case 2:
            // 登录成功则标记状态
            if (do_login(sockfd, &msg) == 1) {
                login_success = 1;
                break;  // 退出一级菜单的 switch
            }
            break;
        case 3:
            close(sockfd);
            exit(0);
        default:
            printf("Invalid command.\n");
    }

    // 登录成功后，自动进入二级菜单
    if (login_success) {
        break;  // 退出一级菜单的 while 循环
    }
}

// 二级菜单（查询/历史/退出）
if (login_success) {
    while (1) {
        printf("*******************************\n");
        printf("* 1.query_word  2.history_record  3.quit *\n");
        printf("*******************************\n");
        printf("Please choose: ");
        scanf("%d", &n);
        getchar();  

        switch (n) {
            case 1:
                do_query(sockfd, &msg);
                break;
            case 2:
                do_history(sockfd, &msg);
                break;
            case 3:
                close(sockfd);
                exit(0);
            default:
                printf("Invalid command.\n");
        }
    }
}

//     // 二级菜单（查询/历史/退出）
// next:
//     while (1) {
//         printf("*******************************\n");
//         printf("* 1.query_word  2.history_record  3.quit *\n");
//         printf("*******************************\n");
//         printf("Please choose: ");
//         scanf("%d", &n);
//         getchar();  // 清理输入缓冲区换行符

//         switch (n) {
//             case 1:
//                 do_query(sockfd, &msg);
//                 break;
//             case 2:
//                 do_history(sockfd, &msg);
//                 break;
//             case 3:
//                 close(sockfd);
//                 exit(0);
//             default:
//                 printf("Invalid command.\n");
//         }
//     }

//     return 0;
// }
