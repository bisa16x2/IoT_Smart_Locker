#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <mysql.h>

#define DB_HOST "(DB HOST)"
#define DB_USER "(DB USER)"
#define DB_PASS "(DB PASSWORD)"
#define DB_NAME "(DB NAME)"
#define DB_PORT 3306

#define BUF_SIZE 100
#define MAX_CLNT 32
#define ID_SIZE 60
#define ARR_CNT 5

#define LOCKER_ID "LOCKER"

typedef struct {
    int fd;
    char *from;
    char *to;
    char *msg;
    int len;
} MSG_INFO;

typedef struct {
    int index;
    int fd;
    char ip[20];
    char id[ID_SIZE];
    char pw[ID_SIZE];
} CLIENT_INFO;

void *clnt_connection(void *arg);
void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info);
void error_handling(char *msg);
void log_file(char *msgstr);
void getlocaltime(char *buf);
void trim_crlf(char *str);

int register_kiosk_user(const char *locker_no,
                        const char *password);

int check_locker_auth(const char *locker_no,
                      const char *password,
                      char *out_locker_no,
                      size_t out_locker_no_size);

int check_rfid_auth(const char *rfid_tag,
                    char *out_locker_no,
                    size_t out_locker_no_size);

int check_rfid_locker_auth(const char *locker_no,
                           const char *rfid_tag);

void send_rfid_auth_to_locker(CLIENT_INFO *first_client_info,
                              const char *locker_no,
                              const char *rfid_tag,
                              int auth_ok);

void insert_kiosk_auth_log(const char *locker_no,
                           const char *rfid_tag,
                           const char *auth_result);

void send_kiosk_auth_to_locker(CLIENT_INFO *first_client_info,
                               const char *locker_no);

void update_locker_state(const char *locker_no,
                         const char *door_state,
                         const char *lock_state);

void insert_locker_event_log(const char *locker_no,
                             const char *event_type,
                             const char *source,
                             const char *door_state,
                             const char *lock_state,
                             const char *item_state,
                             const char *result);

/*
 * ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
 * 여기서 locker_no = 01만 뽑기 위한 함수
 */
int extract_locker_no_from_alert_payload(const char *payload,
                                         char *out_locker_no,
                                         size_t out_size);

/*
 * ALERT payload 전체 파싱
 *
 * ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
 *
 * index 0 = ALERT
 * index 1 = locker_no
 * index 2 = alert_type
 * index 3 = door_state
 * index 4 = lock_state
 * index 5 = item_state
 */
int parse_alert_payload(const char *payload,
                        char *locker_no,
                        size_t locker_no_size,
                        char *alert_type,
                        size_t alert_type_size,
                        char *door_state,
                        size_t door_state_size,
                        char *lock_state,
                        size_t lock_state_size,
                        char *item_state,
                        size_t item_state_size);

/*
 * alert_log 저장
 */
void insert_alert_log(const char *locker_no,
                      const char *alert_type,
                      const char *door_state,
                      const char *lock_state,
                      const char *item_state,
                      const char *message);

int clnt_cnt = 0;
pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    int sock_option = 1;
    pthread_t t_id[MAX_CLNT] = {0};
    int str_len = 0;
    int i = 0;
    char idpasswd[(ID_SIZE * 2) + 3];
    char *pToken;
    char *pArray[ARR_CNT] = {0};
    char msg[BUF_SIZE];

    FILE *idFd = fopen("idpasswd.txt", "r");
    if(idFd == NULL)
    {
        perror("fopen(\"idpasswd.txt\",\"r\") ");
        exit(1);
    }

    char id[ID_SIZE];
    char pw[ID_SIZE];

    CLIENT_INFO *client_info = (CLIENT_INFO *)calloc(MAX_CLNT, sizeof(CLIENT_INFO));
    if(client_info == NULL)
    {
        perror("calloc()");
        exit(1);
    }

    for(i = 0; i < MAX_CLNT; i++)
    {
        client_info[i].fd = -1;
    }

    i = 0;
    while(1)
    {
        if(i >= MAX_CLNT)
        {
            printf("error client_info full(Max:%d)\n", MAX_CLNT);
            break;
        }

        str_len = fscanf(idFd, "%s %s", id, pw);
        if(str_len <= 0)
            break;

        client_info[i].fd = -1;

        strncpy(client_info[i].id, id, ID_SIZE - 1);
        strncpy(client_info[i].pw, pw, ID_SIZE - 1);

        client_info[i].id[ID_SIZE - 1] = '\0';
        client_info[i].pw[ID_SIZE - 1] = '\0';

        i++;
    }

    fclose(idFd);

    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    fputs("IoT Server Start!!\n", stdout);

    if(pthread_mutex_init(&mutx, NULL))
        error_handling("mutex init error");

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    setsockopt(serv_sock,
               SOL_SOCKET,
               SO_REUSEADDR,
               (void *)&sock_option,
               sizeof(sock_option));

    if(bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while(1)
    {
        clnt_adr_sz = sizeof(clnt_adr);

        clnt_sock = accept(serv_sock,
                           (struct sockaddr *)&clnt_adr,
                           &clnt_adr_sz);

        if(clnt_cnt >= MAX_CLNT)
        {
            printf("socket full\n");
            shutdown(clnt_sock, SHUT_WR);
            continue;
        }
        else if(clnt_sock < 0)
        {
            perror("accept()");
            continue;
        }

        memset(idpasswd, 0x0, sizeof(idpasswd));
        str_len = read(clnt_sock, idpasswd, sizeof(idpasswd) - 1);

        if(str_len > 0)
        {
            idpasswd[str_len] = '\0';

            memset(pArray, 0x0, sizeof(pArray));

            i = 0;
            pToken = strtok(idpasswd, "[:]");
            while(pToken != NULL && i < ARR_CNT)
            {
                pArray[i++] = pToken;
                pToken = strtok(NULL, "[:]");
            }

            if(pArray[0] != NULL)
                trim_crlf(pArray[0]);

            if(pArray[1] != NULL)
                trim_crlf(pArray[1]);

            for(i = 0; i < MAX_CLNT; i++)
            {
                if(pArray[0] != NULL && !strcmp(client_info[i].id, pArray[0]))
                {
                    if(client_info[i].fd != -1)
                    {
                        sprintf(msg, "[%s] Already logged!\n", pArray[0]);
                        write(clnt_sock, msg, strlen(msg));
                        log_file(msg);
                        shutdown(clnt_sock, SHUT_WR);

                        client_info[i].fd = -1;
                        break;
                    }

                    if(pArray[1] != NULL && !strcmp(client_info[i].pw, pArray[1]))
                    {
                        strcpy(client_info[i].ip, inet_ntoa(clnt_adr.sin_addr));

                        pthread_mutex_lock(&mutx);
                        client_info[i].index = i;
                        client_info[i].fd = clnt_sock;
                        clnt_cnt++;
                        pthread_mutex_unlock(&mutx);

                        sprintf(msg,
                                "[%s] New connected! (ip:%s,fd:%d,sockcnt:%d)\n",
                                pArray[0],
                                inet_ntoa(clnt_adr.sin_addr),
                                clnt_sock,
                                clnt_cnt);

                        log_file(msg);
                        write(clnt_sock, msg, strlen(msg));

                        pthread_create(t_id + i,
                                       NULL,
                                       clnt_connection,
                                       (void *)(client_info + i));

                        pthread_detach(t_id[i]);
                        break;
                    }
                }
            }

            if(i == MAX_CLNT)
            {
                sprintf(msg,
                        "[%s] Authentication Error!\n",
                        pArray[0] ? pArray[0] : "NULL");

                write(clnt_sock, msg, strlen(msg));
                log_file(msg);
                shutdown(clnt_sock, SHUT_WR);
            }
        }
        else
        {
            shutdown(clnt_sock, SHUT_WR);
        }
    }

    close(serv_sock);
    free(client_info);

    return 0;
}

void *clnt_connection(void *arg)
{
    CLIENT_INFO *client_info = (CLIENT_INFO *)arg;
    int str_len = 0;
    int index = client_info->index;
    char msg[BUF_SIZE];
    char to_msg[MAX_CLNT * ID_SIZE + 1];
    int i = 0;
    char *pToken;
    char *pArray[ARR_CNT] = {0};
    char strBuff[BUF_SIZE * 3] = {0};

    MSG_INFO msg_info;
    CLIENT_INFO *first_client_info;

    first_client_info = client_info - index;

    while(1)
    {
        memset(msg, 0x0, sizeof(msg));
        memset(pArray, 0x0, sizeof(pArray));

        str_len = read(client_info->fd, msg, sizeof(msg) - 1);
        if(str_len <= 0)
            break;

        msg[str_len] = '\0';

        i = 0;
        pToken = strtok(msg, "[:]");
        while(pToken != NULL && i < ARR_CNT)
        {
            pArray[i++] = pToken;
            pToken = strtok(NULL, "[:]");
        }

        if(pArray[0] == NULL)
            continue;

        trim_crlf(pArray[0]);

        if(pArray[1] != NULL)
            trim_crlf(pArray[1]);

        /*
         * =========================================================
         * 1. 키오스크 회원가입
         *
         * STM32 -> SERVER
         * REGISTER:<locker_no>:<pin>
         * =========================================================
         */
        if(!strcmp(pArray[0], "REGISTER"))
        {
            char register_result[BUF_SIZE];

            if(pArray[1] == NULL || pArray[2] == NULL)
            {
                snprintf(register_result,
                         sizeof(register_result),
                         "REGISTER_ERROR\n");
            }
            else
            {
                trim_crlf(pArray[1]);
                trim_crlf(pArray[2]);

                if(register_kiosk_user(pArray[1], pArray[2]))
                {
                    snprintf(register_result,
                             sizeof(register_result),
                             "REGISTER_SUCCESS\n");

                    sprintf(strBuff,
                            "REGISTER success : locker:%s\n",
                            pArray[1]);

                    log_file(strBuff);
                }
                else
                {
                    snprintf(register_result,
                             sizeof(register_result),
                             "REGISTER_FAIL\n");

                    sprintf(strBuff,
                            "REGISTER fail : locker:%s\n",
                            pArray[1]);

                    log_file(strBuff);
                }
            }

            write(client_info->fd, register_result, strlen(register_result));
            continue;
        }

        /*
         * =========================================================
         * 2. STM32 키오스크 PIN 인증
         *
         * STM32 -> SERVER
         * AUTH:<locker_no>:<pin>
         *
         * 성공 시:
         * SERVER -> STM32   AUTH_SUCCESS
         * SERVER -> LOCKER  [LOCKER]KIOSK_AUTH@<locker_no>
         * =========================================================
         */
        if(!strcmp(pArray[0], "AUTH"))
        {
            char auth_result[BUF_SIZE];
            char auth_locker_no[ID_SIZE] = {0};

            if(pArray[1] == NULL || pArray[2] == NULL)
            {
                snprintf(auth_result,
                         sizeof(auth_result),
                         "AUTH_ERROR\n");

                insert_kiosk_auth_log(NULL, NULL, "AUTH_ERROR");
            }
            else
            {
                trim_crlf(pArray[1]);
                trim_crlf(pArray[2]);

                if(check_locker_auth(pArray[1],
                                     pArray[2],
                                     auth_locker_no,
                                     sizeof(auth_locker_no)))
                {
                    snprintf(auth_result,
                             sizeof(auth_result),
                             "AUTH_SUCCESS\n");

                    insert_kiosk_auth_log(auth_locker_no,
                                          NULL,
                                          "AUTH_SUCCESS");

                    send_kiosk_auth_to_locker(first_client_info,
                                              auth_locker_no);
                }
                else
                {
                    snprintf(auth_result,
                             sizeof(auth_result),
                             "AUTH_FAIL\n");

                    insert_kiosk_auth_log(pArray[1],
                                          NULL,
                                          "AUTH_FAIL");
                }
            }

            write(client_info->fd, auth_result, strlen(auth_result));

            sprintf(strBuff,
                    "AUTH request : locker:%s result:%s",
                    pArray[1] ? pArray[1] : "NULL",
                    auth_result);

            log_file(strBuff);

            continue;
        }

        /*
         * =========================================================
         * 3. Arduino RFID 카드 인증
         *
         * Arduino -> SERVER
         * RFID_AUTH:<rfid_tag>
         * AUTH_CARD:<rfid_tag>
         * =========================================================
         */
        if(!strcmp(pArray[0], "RFID_AUTH") || !strcmp(pArray[0], "AUTH_CARD"))
        {
            char rfid_tag[64] = {0};
            char locker_no[ID_SIZE] = {0};
            char response[BUF_SIZE];

            if(pArray[1] == NULL)
            {
                insert_kiosk_auth_log(NULL, NULL, "AUTH_FAIL");

                write(client_info->fd,
                      "@RFID_DENY\n",
                      strlen("@RFID_DENY\n"));

                continue;
            }

            trim_crlf(pArray[1]);

            strncpy(rfid_tag, pArray[1], sizeof(rfid_tag) - 1);
            rfid_tag[sizeof(rfid_tag) - 1] = '\0';

            if(check_rfid_auth(rfid_tag,
                               locker_no,
                               sizeof(locker_no)))
            {
                insert_kiosk_auth_log(locker_no,
                                      rfid_tag,
                                      "AUTH_SUCCESS");

                snprintf(response,
                         sizeof(response),
                         "[LOCKER]RFID_AUTH@%s\n",
                         locker_no);

                write(client_info->fd,
                      response,
                      strlen(response));

                sprintf(strBuff,
                        "[RFID AUTH SUCCESS] rfid:%s locker:%s\n",
                        rfid_tag,
                        locker_no);

                log_file(strBuff);
            }
            else
            {
                insert_kiosk_auth_log(NULL,
                                      rfid_tag,
                                      "AUTH_FAIL");

                write(client_info->fd,
                      "@RFID_DENY\n",
                      strlen("@RFID_DENY\n"));

                sprintf(strBuff,
                        "[RFID AUTH FAIL] rfid:%s\n",
                        rfid_tag);

                log_file(strBuff);
            }

            continue;
        }

        /*
         * =========================================================
         * 4. Arduino 문 열림 상태 보고
         *
         * Arduino -> SERVER
         * DOOR_OPEN:<locker_no>
         * =========================================================
         */
        if(!strcmp(pArray[0], "DOOR_OPEN"))
        {
            if(pArray[1] != NULL)
            {
                trim_crlf(pArray[1]);

                update_locker_state(pArray[1],
                                    "OPEN",
                                    "UNLOCKED");

                insert_locker_event_log(pArray[1],
                                        "DOOR_OPEN",
                                        client_info->id,
                                        "OPEN",
                                        "UNLOCKED",
                                        NULL,
                                        "SUCCESS");

                write(client_info->fd,
                      "DOOR_OPEN_OK\n",
                      strlen("DOOR_OPEN_OK\n"));

                sprintf(strBuff,
                        "[DOOR_OPEN] locker:%s source:%s\n",
                        pArray[1],
                        client_info->id);

                log_file(strBuff);
            }

            continue;
        }

        /*
         * =========================================================
         * 5. Arduino 문 닫힘 상태 보고
         *
         * Arduino -> SERVER
         * DOOR_CLOSED:<locker_no>
         * =========================================================
         */
        if(!strcmp(pArray[0], "DOOR_CLOSED"))
        {
            if(pArray[1] != NULL)
            {
                trim_crlf(pArray[1]);

                update_locker_state(pArray[1],
                                    "CLOSED",
                                    "LOCKED");

                insert_locker_event_log(pArray[1],
                                        "DOOR_CLOSED",
                                        client_info->id,
                                        "CLOSED",
                                        "LOCKED",
                                        NULL,
                                        "SUCCESS");

                write(client_info->fd,
                      "DOOR_CLOSED_OK\n",
                      strlen("DOOR_CLOSED_OK\n"));

                sprintf(strBuff,
                        "[DOOR_CLOSED] locker:%s source:%s\n",
                        pArray[1],
                        client_info->id);

                log_file(strBuff);
            }

            continue;
        }

        /*
         * =========================================================
         * 6. Arduino LOCKER_SQL 호환 RFID 인증
         *
         * Arduino -> SERVER
         * [LOCKER_SQL]RFID_AUTH@<locker_no>@<rfid_tag>
         *
         * 성공 시 SERVER -> LOCKER
         * [LOCKER]RFID_AUTH@<locker_no>@<rfid_tag>
         * =========================================================
         */
        if(!strcmp(pArray[0], "LOCKER_SQL") &&
           pArray[1] != NULL &&
           !strncmp(pArray[1], "RFID_AUTH@", 10))
        {
            char auth_msg[BUF_SIZE];
            char *auth_parts[3] = {0};
            char *auth_token;
            int auth_i = 0;
            const char *locker_no;
            const char *rfid_tag;
            int auth_ok;

            strncpy(auth_msg, pArray[1], sizeof(auth_msg) - 1);
            auth_msg[sizeof(auth_msg) - 1] = '\0';
            trim_crlf(auth_msg);

            auth_token = strtok(auth_msg, "@");
            while(auth_token != NULL && auth_i < 3)
            {
                auth_parts[auth_i++] = auth_token;
                auth_token = strtok(NULL, "@");
            }

            locker_no = auth_parts[1];
            rfid_tag = auth_parts[2];

            if(locker_no == NULL || rfid_tag == NULL)
            {
                send_rfid_auth_to_locker(first_client_info,
                                         locker_no ? locker_no : "",
                                         rfid_tag ? rfid_tag : "",
                                         0);
                continue;
            }

            auth_ok = check_rfid_locker_auth(locker_no, rfid_tag);
            insert_kiosk_auth_log(auth_ok ? locker_no : NULL,
                                  rfid_tag,
                                  auth_ok ? "AUTH_SUCCESS" : "AUTH_FAIL");
            send_rfid_auth_to_locker(first_client_info,
                                     locker_no,
                                     rfid_tag,
                                     auth_ok);

            snprintf(strBuff,
                     sizeof(strBuff),
                     "msg : [LOCKER_SQL->LOCKER] RFID_%s@%s@%s\n",
                     auth_ok ? "AUTH" : "DENY",
                     locker_no,
                     rfid_tag);
            log_file(strBuff);
            continue;
        }

        /*
         * =========================================================
         * 6. 기존 일반 메시지 처리 + USER ALERT 자동 라우팅
         *
         * LOCKER 또는 LOCKER_SQL -> SERVER
         *
         * USER:ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
         *
         * 처리:
         * 1. 목적지 USER를 locker_no인 01로 자동 변경
         * 2. 서버 터미널에 [SERVER ALERT] 출력
         * 3. alert_log에 저장
         * 4. 01 클라이언트에게 [LOCKER]ALERT@... 전송
         * =========================================================
         */
        {
            char target_id[ID_SIZE] = {0};

            msg_info.fd = client_info->fd;
            msg_info.from = client_info->id;

            if(pArray[1] == NULL)
                continue;

            /*
             * USER:ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
             */
            if(!strcmp(pArray[0], "USER") &&
               strncmp(pArray[1], "ALERT@", 6) == 0)
            {
                char alert_locker_no[ID_SIZE] = {0};
                char alert_type[64] = {0};
                char door_state[64] = {0};
                char lock_state[64] = {0};
                char item_state[64] = {0};

                if(parse_alert_payload(pArray[1],
                                       alert_locker_no,
                                       sizeof(alert_locker_no),
                                       alert_type,
                                       sizeof(alert_type),
                                       door_state,
                                       sizeof(door_state),
                                       lock_state,
                                       sizeof(lock_state),
                                       item_state,
                                       sizeof(item_state)))
                {
                    strncpy(target_id,
                            alert_locker_no,
                            sizeof(target_id) - 1);

                    target_id[sizeof(target_id) - 1] = '\0';

                    msg_info.to = target_id;

                    /*
                     * 서버 관리자 화면 출력
                     */
                    snprintf(strBuff,
                             sizeof(strBuff),
                             "[SERVER ALERT] locker:%s type:%s door:%s lock:%s item:%s\n",
                             alert_locker_no,
                             alert_type,
                             door_state,
                             lock_state,
                             item_state);

                    log_file(strBuff);

                    /*
                     * alert_log 저장
                     * message는 alert_type과 동일하게 저장
                     */
                    insert_alert_log(alert_locker_no,
                                     alert_type,
                                     door_state,
                                     lock_state,
                                     item_state,
                                     alert_type);
                }
                else
                {
                    msg_info.to = pArray[0];

                    snprintf(strBuff,
                             sizeof(strBuff),
                             "[SERVER ALERT ERROR] invalid alert payload:%s\n",
                             pArray[1]);

                    log_file(strBuff);
                }
            }
            else
            {
                msg_info.to = pArray[0];
            }

            sprintf(to_msg,
                    "[%s]%s",
                    msg_info.from,
                    pArray[1]);

            msg_info.msg = to_msg;
            msg_info.len = strlen(to_msg);

            sprintf(strBuff,
                    "msg : [%s->%s] %s\n",
                    msg_info.from,
                    msg_info.to,
                    pArray[1]);

            log_file(strBuff);

            send_msg(&msg_info, first_client_info);
        }
    }

    close(client_info->fd);

    sprintf(strBuff,
            "Disconnect ID:%s (ip:%s,fd:%d,sockcnt:%d)\n",
            client_info->id,
            client_info->ip,
            client_info->fd,
            clnt_cnt - 1);

    log_file(strBuff);

    pthread_mutex_lock(&mutx);
    clnt_cnt--;
    client_info->fd = -1;
    pthread_mutex_unlock(&mutx);

    return 0;
}

void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info)
{
    int i = 0;

    if(!strcmp(msg_info->to, "ALLMSG"))
    {
        for(i = 0; i < MAX_CLNT; i++)
        {
            if((first_client_info + i)->fd != -1)
            {
                write((first_client_info + i)->fd,
                      msg_info->msg,
                      msg_info->len);
            }
        }
    }
    else if(!strcmp(msg_info->to, "IDLIST"))
    {
        char *idlist = (char *)malloc(ID_SIZE * MAX_CLNT);
        if(idlist == NULL)
            return;

        msg_info->msg[strlen(msg_info->msg) - 1] = '\0';
        strcpy(idlist, msg_info->msg);

        for(i = 0; i < MAX_CLNT; i++)
        {
            if((first_client_info + i)->fd != -1)
            {
                strcat(idlist, (first_client_info + i)->id);
                strcat(idlist, " ");
            }
        }

        strcat(idlist, "\n");
        write(msg_info->fd, idlist, strlen(idlist));
        free(idlist);
    }
    else if(!strcmp(msg_info->to, "GETTIME"))
    {
        sleep(1);
        getlocaltime(msg_info->msg);
        write(msg_info->fd, msg_info->msg, strlen(msg_info->msg));
    }
    else
    {
        for(i = 0; i < MAX_CLNT; i++)
        {
            if((first_client_info + i)->fd != -1)
            {
                if(!strcmp(msg_info->to, (first_client_info + i)->id))
                {
                    write((first_client_info + i)->fd,
                          msg_info->msg,
                          msg_info->len);
                }
            }
        }
    }
}

/*
 * STM32 키오스크 인증 성공 시 LOCKER에게 문 열림 명령 전송
 */
void send_rfid_auth_to_locker(CLIENT_INFO *first_client_info,
                              const char *locker_no,
                              const char *rfid_tag,
                              int auth_ok)
{
    int i;
    char cmd[BUF_SIZE];

    if(first_client_info == NULL || locker_no == NULL || rfid_tag == NULL)
        return;

    if(auth_ok)
    {
        snprintf(cmd,
                 sizeof(cmd),
                 "[LOCKER]RFID_AUTH@%s@%s\n",
                 locker_no,
                 rfid_tag);
    }
    else
    {
        snprintf(cmd,
                 sizeof(cmd),
                 "[LOCKER]RFID_DENY@%s@%s\n",
                 locker_no,
                 rfid_tag);
    }

    for(i = 0; i < MAX_CLNT; i++)
    {
        if((first_client_info + i)->fd != -1 &&
           !strcmp((first_client_info + i)->id, LOCKER_ID))
        {
            write((first_client_info + i)->fd,
                  cmd,
                  strlen(cmd));

            printf("[SERVER -> LOCKER] %s", cmd);
            return;
        }
    }

    printf("[SERVER] LOCKER is not connected. cmd:%s", cmd);
}

void send_kiosk_auth_to_locker(CLIENT_INFO *first_client_info,
                               const char *locker_no)
{
    int i;
    char cmd[BUF_SIZE];

    if(first_client_info == NULL || locker_no == NULL)
        return;

    snprintf(cmd,
             sizeof(cmd),
             "[LOCKER]KIOSK_AUTH@%s\n",
             locker_no);

    for(i = 0; i < MAX_CLNT; i++)
    {
        if((first_client_info + i)->fd != -1 &&
           !strcmp((first_client_info + i)->id, LOCKER_ID))
        {
            write((first_client_info + i)->fd,
                  cmd,
                  strlen(cmd));

            printf("[SERVER -> LOCKER] %s", cmd);
            return;
        }
    }

    printf("[SERVER] LOCKER is not connected. cmd:%s", cmd);
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void log_file(char *msgstr)
{
    fputs(msgstr, stdout);
}

void getlocaltime(char *buf)
{
    struct tm *t;
    time_t tt;
    char wday[7][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    tt = time(NULL);
    if(errno == EFAULT)
        perror("time()");

    t = localtime(&tt);

    sprintf(buf,
            "[GETTIME]%02d.%02d.%02d %02d:%02d:%02d %s",
            t->tm_year + 1900 - 2000,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec,
            wday[t->tm_wday]);
}

void trim_crlf(char *str)
{
    if(str == NULL)
        return;

    str[strcspn(str, "\r\n")] = '\0';
}

/*
 * =========================================================
 * 회원가입 DB 저장
 *
 * REGISTER:<locker_no>:<pin>
 * users.locker_no 저장
 * users.pin_hash = SHA2(pin, 256)
 * 기존 rfid_tag는 유지
 * =========================================================
 */
int register_kiosk_user(const char *locker_no,
                        const char *password)
{
    MYSQL *conn;

    char esc_locker_no[64];
    char esc_password[128];
    char query[512];

    int result = 0;

    if(locker_no == NULL || password == NULL)
        return 0;

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return 0;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no,
                             strlen(locker_no));

    mysql_real_escape_string(conn,
                             esc_password,
                             password,
                             strlen(password));

    snprintf(query,
             sizeof(query),
             "INSERT INTO users (locker_no, pin_hash) "
             "VALUES ('%s', SHA2('%s', 256)) "
             "ON DUPLICATE KEY UPDATE "
             "pin_hash = SHA2('%s', 256)",
             esc_locker_no,
             esc_password,
             esc_password);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] register failed: %s\n", mysql_error(conn));
        result = 0;
    }
    else
    {
        result = 1;
    }

    mysql_close(conn);

    return result;
}

/*
 * =========================================================
 * STM32 PIN 인증
 *
 * AUTH:<locker_no>:<pin>
 * =========================================================
 */
int check_locker_auth(const char *locker_no,
                      const char *password,
                      char *out_locker_no,
                      size_t out_locker_no_size)
{
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    char esc_locker_no[64];
    char esc_password[128];
    char query[512];

    int auth_ok = 0;

    if(locker_no == NULL || password == NULL)
        return 0;

    if(out_locker_no != NULL && out_locker_no_size > 0)
        out_locker_no[0] = '\0';

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return 0;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no,
                             strlen(locker_no));

    mysql_real_escape_string(conn,
                             esc_password,
                             password,
                             strlen(password));

    snprintf(query,
             sizeof(query),
             "SELECT locker_no "
             "FROM users "
             "WHERE locker_no = '%s' "
             "AND pin_hash = SHA2('%s', 256) "
             "LIMIT 1",
             esc_locker_no,
             esc_password);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] PIN auth query failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    res = mysql_store_result(conn);
    if(res == NULL)
    {
        printf("[DB] store result failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    row = mysql_fetch_row(res);

    if(row != NULL && row[0] != NULL)
    {
        auth_ok = 1;

        if(out_locker_no != NULL && out_locker_no_size > 0)
        {
            strncpy(out_locker_no, row[0], out_locker_no_size - 1);
            out_locker_no[out_locker_no_size - 1] = '\0';
        }
    }

    mysql_free_result(res);
    mysql_close(conn);

    return auth_ok;
}

/*
 * =========================================================
 * RFID 인증
 *
 * RFID_AUTH:<rfid_tag>
 * AUTH_CARD:<rfid_tag>
 * =========================================================
 */
int check_rfid_auth(const char *rfid_tag,
                    char *out_locker_no,
                    size_t out_locker_no_size)
{
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    char esc_rfid_tag[128];
    char query[512];

    int auth_ok = 0;

    if(rfid_tag == NULL)
        return 0;

    if(out_locker_no != NULL && out_locker_no_size > 0)
        out_locker_no[0] = '\0';

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return 0;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    mysql_real_escape_string(conn,
                             esc_rfid_tag,
                             rfid_tag,
                             strlen(rfid_tag));

    snprintf(query,
             sizeof(query),
             "SELECT locker_no "
             "FROM users "
             "WHERE rfid_tag = '%s' "
             "LIMIT 1",
             esc_rfid_tag);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] RFID auth query failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    res = mysql_store_result(conn);
    if(res == NULL)
    {
        printf("[DB] store result failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    row = mysql_fetch_row(res);

    if(row != NULL && row[0] != NULL)
    {
        auth_ok = 1;

        if(out_locker_no != NULL && out_locker_no_size > 0)
        {
            strncpy(out_locker_no, row[0], out_locker_no_size - 1);
            out_locker_no[out_locker_no_size - 1] = '\0';
        }
    }

    mysql_free_result(res);
    mysql_close(conn);

    return auth_ok;
}

int check_rfid_locker_auth(const char *locker_no,
                           const char *rfid_tag)
{
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    char esc_locker_no[64];
    char esc_rfid_tag[128];
    char query[512];
    int auth_ok = 0;

    if(locker_no == NULL || rfid_tag == NULL)
        return 0;

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return 0;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no,
                             strlen(locker_no));

    mysql_real_escape_string(conn,
                             esc_rfid_tag,
                             rfid_tag,
                             strlen(rfid_tag));

    snprintf(query,
             sizeof(query),
             "SELECT locker_no "
             "FROM users "
             "WHERE locker_no = '%s' "
             "AND REPLACE(REPLACE(REPLACE(UPPER(rfid_tag), ' ', ''), ':', ''), '-', '') = UPPER('%s') "
             "LIMIT 1",
             esc_locker_no,
             esc_rfid_tag);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] RFID locker auth query failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    res = mysql_store_result(conn);
    if(res == NULL)
    {
        printf("[DB] store result failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    row = mysql_fetch_row(res);
    if(row != NULL && row[0] != NULL)
        auth_ok = 1;

    mysql_free_result(res);
    mysql_close(conn);

    return auth_ok;
}

/*
 * =========================================================
 * kiosk_auth_log 저장
 * =========================================================
 */
void insert_kiosk_auth_log(const char *locker_no,
                           const char *rfid_tag,
                           const char *auth_result)
{
    MYSQL *conn;

    char esc_locker_no[64];
    char esc_rfid_tag[128];
    char esc_auth_result[32];

    char locker_no_value[80];
    char rfid_tag_value[160];

    char query[512];

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    if(locker_no != NULL && locker_no[0] != '\0')
    {
        mysql_real_escape_string(conn,
                                 esc_locker_no,
                                 locker_no,
                                 strlen(locker_no));

        snprintf(locker_no_value,
                 sizeof(locker_no_value),
                 "'%s'",
                 esc_locker_no);
    }
    else
    {
        snprintf(locker_no_value,
                 sizeof(locker_no_value),
                 "NULL");
    }

    if(rfid_tag != NULL && rfid_tag[0] != '\0')
    {
        mysql_real_escape_string(conn,
                                 esc_rfid_tag,
                                 rfid_tag,
                                 strlen(rfid_tag));

        snprintf(rfid_tag_value,
                 sizeof(rfid_tag_value),
                 "'%s'",
                 esc_rfid_tag);
    }
    else
    {
        snprintf(rfid_tag_value,
                 sizeof(rfid_tag_value),
                 "NULL");
    }

    mysql_real_escape_string(conn,
                             esc_auth_result,
                             auth_result ? auth_result : "",
                             strlen(auth_result ? auth_result : ""));

    snprintf(query,
             sizeof(query),
             "INSERT INTO kiosk_auth_log "
             "(locker_no, rfid_tag, auth_result) "
             "VALUES (%s, %s, '%s')",
             locker_no_value,
             rfid_tag_value,
             esc_auth_result);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] kiosk_auth_log insert failed: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
}

/*
 * =========================================================
 * lockers 상태 업데이트
 * =========================================================
 */
void update_locker_state(const char *locker_no,
                         const char *door_state,
                         const char *lock_state)
{
    MYSQL *conn;

    char esc_locker_no[64];
    char esc_door_state[64];
    char esc_lock_state[64];

    char query[512];

    if(locker_no == NULL || door_state == NULL || lock_state == NULL)
        return;

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no,
                             strlen(locker_no));

    mysql_real_escape_string(conn,
                             esc_door_state,
                             door_state,
                             strlen(door_state));

    mysql_real_escape_string(conn,
                             esc_lock_state,
                             lock_state,
                             strlen(lock_state));

    snprintf(query,
             sizeof(query),
             "UPDATE lockers "
             "SET door_state = '%s', "
             "lock_state = '%s', "
             "updated_at = CURRENT_TIMESTAMP "
             "WHERE locker_no = '%s'",
             esc_door_state,
             esc_lock_state,
             esc_locker_no);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] lockers update failed: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
}

/*
 * =========================================================
 * locker_event_log 저장
 * =========================================================
 */
void insert_locker_event_log(const char *locker_no,
                             const char *event_type,
                             const char *source,
                             const char *door_state,
                             const char *lock_state,
                             const char *item_state,
                             const char *result)
{
    MYSQL *conn;

    char esc_locker_no[64];
    char esc_event_type[64];
    char esc_source[64];
    char esc_door_state[64];
    char esc_lock_state[64];
    char esc_item_state[64];
    char esc_result[64];

    char item_state_value[96];

    char query[1024];

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no ? locker_no : "",
                             strlen(locker_no ? locker_no : ""));

    mysql_real_escape_string(conn,
                             esc_event_type,
                             event_type ? event_type : "",
                             strlen(event_type ? event_type : ""));

    mysql_real_escape_string(conn,
                             esc_source,
                             source ? source : "",
                             strlen(source ? source : ""));

    mysql_real_escape_string(conn,
                             esc_door_state,
                             door_state ? door_state : "",
                             strlen(door_state ? door_state : ""));

    mysql_real_escape_string(conn,
                             esc_lock_state,
                             lock_state ? lock_state : "",
                             strlen(lock_state ? lock_state : ""));

    mysql_real_escape_string(conn,
                             esc_result,
                             result ? result : "",
                             strlen(result ? result : ""));

    if(item_state != NULL && item_state[0] != '\0')
    {
        mysql_real_escape_string(conn,
                                 esc_item_state,
                                 item_state,
                                 strlen(item_state));

        snprintf(item_state_value,
                 sizeof(item_state_value),
                 "'%s'",
                 esc_item_state);
    }
    else
    {
        snprintf(item_state_value,
                 sizeof(item_state_value),
                 "NULL");
    }

    snprintf(query,
             sizeof(query),
             "INSERT INTO locker_event_log "
             "(locker_no, event_type, source, door_state, lock_state, item_state, result) "
             "VALUES ('%s', '%s', '%s', '%s', '%s', %s, '%s')",
             esc_locker_no,
             esc_event_type,
             esc_source,
             esc_door_state,
             esc_lock_state,
             item_state_value,
             esc_result);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] locker_event_log insert failed: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
}

/*
 * ALERT payload에서 locker_no만 추출
 *
 * ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
 * index 1 = 01
 */
int extract_locker_no_from_alert_payload(const char *payload,
                                         char *out_locker_no,
                                         size_t out_size)
{
    char temp[BUF_SIZE * 2];
    char *token;
    char *saveptr = NULL;
    int index = 0;

    if(payload == NULL || out_locker_no == NULL || out_size == 0)
        return 0;

    out_locker_no[0] = '\0';

    strncpy(temp, payload, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    token = strtok_r(temp, "@", &saveptr);

    while(token != NULL)
    {
        if(index == 1)
        {
            strncpy(out_locker_no, token, out_size - 1);
            out_locker_no[out_size - 1] = '\0';
            return 1;
        }

        index++;
        token = strtok_r(NULL, "@", &saveptr);
    }

    return 0;
}

/*
 * ALERT payload 전체 파싱
 */
int parse_alert_payload(const char *payload,
                        char *locker_no,
                        size_t locker_no_size,
                        char *alert_type,
                        size_t alert_type_size,
                        char *door_state,
                        size_t door_state_size,
                        char *lock_state,
                        size_t lock_state_size,
                        char *item_state,
                        size_t item_state_size)
{
    char temp[BUF_SIZE * 2];
    char *token;
    char *saveptr = NULL;
    int index = 0;

    if(payload == NULL ||
       locker_no == NULL ||
       alert_type == NULL ||
       door_state == NULL ||
       lock_state == NULL ||
       item_state == NULL)
    {
        return 0;
    }

    locker_no[0] = '\0';
    alert_type[0] = '\0';
    door_state[0] = '\0';
    lock_state[0] = '\0';
    item_state[0] = '\0';

    strncpy(temp, payload, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    token = strtok_r(temp, "@", &saveptr);

    while(token != NULL)
    {
        /*
         * payload 예:
         * ALERT@01@ITEM_CHANGED_WHILE_LOCKED@OPEN@LOCKED@EMPTY
         *
         * index 0 = ALERT
         * index 1 = locker_no
         * index 2 = alert_type
         * index 3 = door_state
         * index 4 = lock_state
         * index 5 = item_state
         */
        if(index == 1)
        {
            strncpy(locker_no, token, locker_no_size - 1);
            locker_no[locker_no_size - 1] = '\0';
        }
        else if(index == 2)
        {
            strncpy(alert_type, token, alert_type_size - 1);
            alert_type[alert_type_size - 1] = '\0';
        }
        else if(index == 3)
        {
            strncpy(door_state, token, door_state_size - 1);
            door_state[door_state_size - 1] = '\0';
        }
        else if(index == 4)
        {
            strncpy(lock_state, token, lock_state_size - 1);
            lock_state[lock_state_size - 1] = '\0';
        }
        else if(index == 5)
        {
            strncpy(item_state, token, item_state_size - 1);
            item_state[item_state_size - 1] = '\0';
        }

        index++;
        token = strtok_r(NULL, "@", &saveptr);
    }

    if(locker_no[0] == '\0' ||
       alert_type[0] == '\0' ||
       door_state[0] == '\0' ||
       lock_state[0] == '\0' ||
       item_state[0] == '\0')
    {
        return 0;
    }

    return 1;
}

/*
 * alert_log 저장
 *
 * alert_log 구조:
 * id
 * locker_no
 * alert_type
 * door_state
 * lock_state
 * item_state
 * message
 * is_checked
 * created_at
 */
void insert_alert_log(const char *locker_no,
                      const char *alert_type,
                      const char *door_state,
                      const char *lock_state,
                      const char *item_state,
                      const char *message)
{
    MYSQL *conn;

    char esc_locker_no[64];
    char esc_alert_type[128];
    char esc_door_state[64];
    char esc_lock_state[64];
    char esc_item_state[64];
    char esc_message[256];

    char query[1024];

    if(locker_no == NULL ||
       alert_type == NULL ||
       door_state == NULL ||
       lock_state == NULL ||
       item_state == NULL ||
       message == NULL)
    {
        return;
    }

    conn = mysql_init(NULL);
    if(conn == NULL)
    {
        printf("[DB] mysql_init failed\n");
        return;
    }

    if(mysql_real_connect(conn,
                          DB_HOST,
                          DB_USER,
                          DB_PASS,
                          DB_NAME,
                          DB_PORT,
                          NULL,
                          0) == NULL)
    {
        printf("[DB] connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    mysql_real_escape_string(conn,
                             esc_locker_no,
                             locker_no,
                             strlen(locker_no));

    mysql_real_escape_string(conn,
                             esc_alert_type,
                             alert_type,
                             strlen(alert_type));

    mysql_real_escape_string(conn,
                             esc_door_state,
                             door_state,
                             strlen(door_state));

    mysql_real_escape_string(conn,
                             esc_lock_state,
                             lock_state,
                             strlen(lock_state));

    mysql_real_escape_string(conn,
                             esc_item_state,
                             item_state,
                             strlen(item_state));

    mysql_real_escape_string(conn,
                             esc_message,
                             message,
                             strlen(message));

    snprintf(query,
             sizeof(query),
             "INSERT INTO alert_log "
             "(locker_no, alert_type, door_state, lock_state, item_state, message, is_checked) "
             "VALUES "
             "('%s', '%s', '%s', '%s', '%s', '%s', 0)",
             esc_locker_no,
             esc_alert_type,
             esc_door_state,
             esc_lock_state,
             esc_item_state,
             esc_message);

    if(mysql_query(conn, query) != 0)
    {
        printf("[DB] alert_log insert failed: %s\n", mysql_error(conn));
    }
    else
    {
        printf("[DB] alert_log inserted : locker:%s type:%s\n",
               locker_no,
               alert_type);
    }

    mysql_close(conn);
}
