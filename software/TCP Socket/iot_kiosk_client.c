/* STM32 Kiosk와 Raspberry Pi TCP server를 연결하는 Bluetooth bridge */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);
int is_bt_forward_message(const char *msg);

char name[NAME_SIZE] = "[Default]";

typedef struct {
    int sockfd;
    int btfd;
    char sendid[NAME_SIZE];
} DEV_FD;

int main(int argc, char *argv[])
{
    DEV_FD dev_fd;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;
    int ret;
    struct sockaddr_rc addr = {0};

    char dest[18] = "(BLUETOOTH MAC)";
    char msg[BUF_SIZE] = {0};

    if(argc != 4)
    {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    dev_fd.sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(dev_fd.sockfd == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(dev_fd.sockfd,
               (struct sockaddr *)&serv_addr,
               sizeof(serv_addr)) == -1)
    {
        error_handling("connect() error");
    }

    /*
     * 서버 로그인 메시지
     * 서버의 strtok 구분자가 "[:]" 이므로
     * [(KIOSK CLIENT ID):(PASSWORD)] 형태로 보내면 ID와 password로 파싱됨
     */
    sprintf(msg, "[%s:(PASSWORD)]", name);
    write(dev_fd.sockfd, msg, strlen(msg));

    dev_fd.btfd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if(dev_fd.btfd == -1)
    {
        perror("bt socket()");
        exit(1);
    }

    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t)1;
    str2ba(dest, &addr.rc_bdaddr);

    ret = connect(dev_fd.btfd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret == -1)
    {
        perror("bt connect()");
        exit(1);
    }

    printf("BT connected to %s\n", dest);
    fflush(stdout);

    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&dev_fd);
    pthread_create(&snd_thread, NULL, send_msg, (void *)&dev_fd);

    pthread_join(snd_thread, &thread_return);

    close(dev_fd.sockfd);
    close(dev_fd.btfd);

    return 0;
}

/*
 * STM32 Bluetooth -> Raspberry Pi Server
 *
 * STM32에서 들어온 문자열:
 * AUTH:01:(PIN)
 * REGISTER:01:(PIN)
 *
 * 위 문자열을 서버로 그대로 전달
 */
void *send_msg(void *arg)
{
    DEV_FD *dev_fd = (DEV_FD *)arg;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char msg[BUF_SIZE] = {0};
    int total = 0;

    FD_ZERO(&initset);
    FD_SET(dev_fd->btfd, &initset);

    while(1)
    {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;

        ret = select(dev_fd->btfd + 1, &newset, NULL, NULL, &tv);

        if(ret < 0)
        {
            perror("select()");
            dev_fd->sockfd = -1;
            return NULL;
        }

        if(ret == 0)
        {
            if(dev_fd->sockfd == -1)
                return NULL;

            continue;
        }

        if(FD_ISSET(dev_fd->btfd, &newset))
        {
            if(total >= BUF_SIZE - 1)
            {
                printf("BT buffer full, clear buffer\n");
                fflush(stdout);

                total = 0;
                memset(msg, 0, sizeof(msg));
                continue;
            }

            ret = read(dev_fd->btfd,
                       msg + total,
                       BUF_SIZE - 1 - total);

            if(ret > 0)
            {
                printf("BT read ret=%d : ", ret);
                fwrite(msg + total, 1, ret, stdout);
                printf("\n");
                fflush(stdout);

                total += ret;
            }
            else if(ret == 0)
            {
                printf("BT disconnected\n");
                fflush(stdout);

                dev_fd->sockfd = -1;
                return NULL;
            }
            else
            {
                perror("bt read()");
                dev_fd->sockfd = -1;
                return NULL;
            }

            /*
             * STM32에서 명령 끝에 '\n'을 붙여서 보내는 구조 기준
             * '\n'이 들어올 때까지 누적하고, 완성된 한 줄만 서버로 전송
             */
            if(total >= BUF_SIZE - 1)
            {
                printf("BT garbage or no newline, clear buffer\n");
                fflush(stdout);

                total = 0;
                memset(msg, 0, sizeof(msg));
                continue;
            }

            if(msg[total - 1] == '\n')
            {
                msg[total] = '\0';
                total = 0;
            }
            else
            {
                continue;
            }

            fputs("ARD:", stdout);
            fputs(msg, stdout);
            fflush(stdout);

            if(write(dev_fd->sockfd, msg, strlen(msg)) <= 0)
            {
                perror("server write()");
                dev_fd->sockfd = -1;
                return NULL;
            }

            memset(msg, 0, sizeof(msg));
        }
    }
}

/*
 * Raspberry Pi Server -> STM32 Bluetooth
 *
 * 전달:
 * AUTH_SUCCESS
 * AUTH_FAIL
 * AUTH_ERROR
 * REGISTER_SUCCESS
 * REGISTER_FAIL
 * REGISTER_ERROR
 * [SERVER]ALERT@...
 *
 * ALERT 메시지는 클라이언트 터미널에 알림으로 출력하고,
 * 필요하면 HC-06을 통해 STM32에도 전달한다.
 */
void *recv_msg(void *arg)
{
    DEV_FD *dev_fd = (DEV_FD *)arg;
    char name_msg[NAME_SIZE + BUF_SIZE + 1];
    int str_len;
    int bt_wr;

    while(1)
    {
        memset(name_msg, 0x0, sizeof(name_msg));

        str_len = read(dev_fd->sockfd,
                       name_msg,
                       NAME_SIZE + BUF_SIZE);

        if(str_len <= 0)
        {
            dev_fd->sockfd = -1;
            return NULL;
        }

        name_msg[str_len] = '\0';

        printf("SRV:%s%s", name_msg, strchr(name_msg, '\n') ? "" : "\n");
        fflush(stdout);

        /*
         * 서버에서 특정 클라이언트 CGH_BLT에게 보낸 알림 확인용 출력
         *
         * 예:
         * SRV:[SERVER]ALERT@FORCED_OPEN@01@OPEN@LOCKED@EXIST
         */
        if(strstr(name_msg, "ALERT@") != NULL)
        {
            printf("[CLIENT ALERT RECEIVED] %s%s",
                   name_msg,
                   strchr(name_msg, '\n') ? "" : "\n");
            fflush(stdout);
        }

        /*
         * STM32 LCD/FSM에서 처리해야 하는 서버 응답만 Bluetooth로 전달
         * ALERT도 필요하면 STM32로 전달한다.
         */
        if(!is_bt_forward_message(name_msg))
        {
            printf("Skip BT write: %s%s",
                   name_msg,
                   strchr(name_msg, '\n') ? "" : "\n");
            fflush(stdout);
            continue;
        }

        /*
         * STM32는 '\n' 기준으로 한 줄 응답을 읽으므로
         * 서버 응답에 개행이 없다면 붙여준다.
         */
        if(strchr(name_msg, '\n') == NULL)
        {
            strncat(name_msg,
                    "\n",
                    sizeof(name_msg) - strlen(name_msg) - 1);
        }

        bt_wr = write(dev_fd->btfd,
                      name_msg,
                      strlen(name_msg));

        printf("BT write ret=%d : %s", bt_wr, name_msg);
        fflush(stdout);
    }
}

/*
 * 서버 응답 중 STM32 또는 알림 표시용으로 넘길 메시지 필터
 */
int is_bt_forward_message(const char *msg)
{
    if(msg == NULL)
        return 0;

    if(strncmp(msg, "AUTH_SUCCESS", 12) == 0)
        return 1;

    if(strncmp(msg, "AUTH_FAIL", 9) == 0)
        return 1;

    if(strncmp(msg, "AUTH_ERROR", 10) == 0)
        return 1;

    if(strncmp(msg, "REGISTER_SUCCESS", 16) == 0)
        return 1;

    if(strncmp(msg, "REGISTER_FAIL", 13) == 0)
        return 1;

    if(strncmp(msg, "REGISTER_ERROR", 14) == 0)
        return 1;

    /*
     * 서버에서 온 알림:
     * [SERVER]ALERT@FORCED_OPEN@01@OPEN@LOCKED@EXIST
     */
    if(strstr(msg, "ALERT@") != NULL)
        return 1;

    return 0;
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
