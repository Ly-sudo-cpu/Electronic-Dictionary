#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <signal.h>
#include <time.h>
#define N 32
#define R 1 // usr-register
#define L 2 // usr-login 
#define Q 3 // usr-query
#define H 4 // usr-history
#define DATABASE "my.db"

typedef struct {
    int type;
    char name[N];
    char data[256];
} MSG;

int do_client(int acceptfd, sqlite3 *db);
void do_register(int acceptfd, MSG *msg, sqlite3 *db);
int do_login(int acceptfd, MSG *msg, sqlite3 *db);
int do_query(int acceptfd, MSG *msg, sqlite3 *db);
int do_history(int acceptfd, MSG *msg, sqlite3 *db);
int do_searchword(int acceptfd, MSG *msg, char *word);
int get_date(char *date);

int main(int argc, const char *argv[]) {
    int sockfd;
    struct sockaddr_in serveraddr; 
    sqlite3 *db;
    int acceptfd;
    pid_t pid;
    char *errmsg;
    char sql[128];

    if(argc != 2) {
        printf("Usage: %s port\n", argv[0]);
        return -1;
    }

    if(sqlite3_open(DATABASE, &db) != SQLITE_OK) {
        printf("%s\n", sqlite3_errmsg(db));
        return -1;
    } else {
        printf("Open DATABASE success\n");
    }

    strcpy(sql, "create table if not exists usr(name text primary key, pass text)");
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        printf("usr table: %s\n", errmsg);
    } else {
        printf("usr table created or exists\n");
    }

    strcpy(sql, "create table if not exists history(name text, word text, time text)");
    if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        printf("history table: %s\n", errmsg);
    } else {
        printf("history table created or exists\n");
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("fail to socket");
        return -1;
    }
    
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(atoi(argv[1]));

    if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("fail to bind");
        return -1;
    }

    if(listen(sockfd, 5) < 0) {
        perror("fail to listen");
        return -1;
    }

    signal(SIGCHLD, SIG_IGN);

    while(1) {
        if((acceptfd = accept(sockfd, NULL, NULL)) < 0) {
            perror("fail to accept");
            continue;
        }

        if((pid = fork()) < 0) {
            perror("fail to fork");
            close(acceptfd);
            continue;
        } else if (pid == 0) {
            close(sockfd);
            do_client(acceptfd, db);
            sqlite3_close(db);
            exit(0);
        } else {
            close(acceptfd);
        }
    }
    
    sqlite3_close(db);
    close(sockfd);
    return 0;
}

int do_client(int acceptfd, sqlite3 *db) {
    MSG msg;
    int n;

    while((n = recv(acceptfd, &msg, sizeof(msg), 0)) > 0) {
        switch(msg.type) {
            case R:
                do_register(acceptfd, &msg, db);
                break;
            case L:
                do_login(acceptfd, &msg, db);
                break;
            case Q:
                do_query(acceptfd, &msg, db);
                break;
            case H:
                do_history(acceptfd, &msg, db);
                break;
            default:
                printf("Invalid data msg type: %d\n", msg.type);
                strcpy(msg.data, "Invalid request type");
                send(acceptfd, &msg, sizeof(msg), 0);
        }
    }

    if(n < 0) {
        perror("recv error");
    }

    close(acceptfd);
    return 0;
}

void do_register(int acceptfd, MSG *msg, sqlite3 *db) {
    printf("Handling registration for: %s\n", msg->name);
    printf("用户 %s 注册的密码是：%s\n", msg->name, msg->data); 
    char sql[256];
    char *errmsg;
    int ret;

    sprintf(sql, "select * from usr where name='%s'", msg->name);
    ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    
    if(ret == SQLITE_OK) {
        sprintf(sql, "insert into usr values('%s', '%s')", msg->name, msg->data);
        ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
        
        if(ret == SQLITE_OK) {
            strcpy(msg->data, "Registration successful");
        } else {
            sprintf(msg->data, "Registration failed: %s", errmsg);
            printf("Registration error: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
    } else {
        strcpy(msg->data, "usrname already exists");
        sqlite3_free(errmsg);
    }

    if(send(acceptfd, msg, sizeof(MSG), 0) < 0) {
        perror("fail to send");
    }
}

int do_login(int acceptfd, MSG *msg, sqlite3 *db) {
    char sql[256];
    char *errmsg;
    char **resultp;
    int nrow;
    int ncolumn;

    sprintf(sql, "select * from usr where name='%s' and pass='%s'", msg->name, msg->data);
    printf("%s\n", sql);
    
    if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncolumn, &errmsg) != SQLITE_OK) {
        printf("Login error: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    
    if(nrow == 0) {
        strcpy(msg->data, "Login failed");
    } else if(nrow == 1) {
        strcpy(msg->data, "Login successful");
    }
    
    if(send(acceptfd, msg, sizeof(MSG), 0) < 0) {
        perror("fail to send");
        return -1;
    }
    
    sqlite3_free_table(resultp);
    return (nrow == 1) ? 1 : 0;
}

int do_searchword(int acceptfd, MSG *msg, char *word) {
    FILE *fp;
    char temp[512] = {0};
    int len = 0;
    int result;
    char *p;

    if((fp = fopen("dict.txt", "r")) == NULL) {
        printf("open file error\n");
        strcpy(msg->data, "open file error");
        send(acceptfd, msg, sizeof(MSG), 0);
        return -1;
    }

    len = strlen(word);
    printf("%s\n, len=%d\n", word, len);

    while(fgets(temp, 512, fp) != NULL) {
        result = strncmp(temp, word, len);

        if(result > 0) {
            continue;
        }
        if(result < 0 || temp[len] != ' ') {
            break;
        }

        p = temp + len;
        while(*p == ' ') {
            p++;
        }

        strcpy(msg->data, p);
        fclose(fp);
        return 1;
    }
    
    fclose(fp);
    return 0;
}

int get_date(char *date) {
    time_t t;
    struct tm *tp;
    time(&t);
    tp = localtime(&t);
    sprintf(date, "%4d-%02d-%02d %02d:%02d:%02d", 
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, 
            tp->tm_hour, tp->tm_min, tp->tm_sec);
    return 0;
}

int do_query(int acceptfd, MSG *msg, sqlite3 *db) {
    char word[64];
    int found = 0;
    char date[128];
    char sql[256];
    char *errmsg;

    strcpy(word, msg->data);
    found = do_searchword(acceptfd, msg, word);

    if(found == 1) {
        get_date(date);
        sprintf(sql, "insert into history values('%s', '%s', '%s')", 
                msg->name, word, date);
        printf("%s\n", sql);
        
        if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
            printf("%s\n", errmsg);
            sqlite3_free(errmsg);
        }
    } else {
        strcpy(msg->data, "Word not found");
    }

    if(send(acceptfd, msg, sizeof(MSG), 0) < 0) {
        perror("fail to send");
        return -1;
    }

    return 0;
}

int do_history(int acceptfd, MSG *msg, sqlite3 *db) {
    printf("Handling history request for: %s\n", msg->name);
    char sql[256];
    char *errmsg;
    char **result;
    int nrow, ncol;
    int i, index;
    char history[1024] = "";

    sprintf(sql, "select word, time from history where name='%s' order by time desc limit 10", msg->name);
    
    if(sqlite3_get_table(db, sql, &result, &nrow, &ncol, &errmsg) == SQLITE_OK) {
        if(nrow == 0) {
            strcpy(msg->data, "No history records found");
        } else {
            index = ncol;
            for(i = 0; i < nrow && i < 5; i++) {
                strcat(history, result[index]);
                strcat(history, " - ");
                strcat(history, result[index + 1]);
                strcat(history, "\n");
                index += ncol;
            }
            strcpy(msg->data, history);
        }
        sqlite3_free_table(result);
    } else {
        sprintf(msg->data, "History error: %s", errmsg);
        printf("History error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    if(send(acceptfd, msg, sizeof(MSG), 0) < 0) {
        perror("fail to send");
        return -1;
    }
    
    return 0;
}
