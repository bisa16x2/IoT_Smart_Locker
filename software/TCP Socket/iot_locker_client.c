/* Arduino Locker message와 SQL/DB 처리를 연결하는 TCP client */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <mysql.h>

#define BUF_SIZE  256
#define NAME_SIZE 20
#define ARR_CNT   12
#define LOCKER_CLIENT_ID "LOCKER"

void *send_msg(void *arg);
void *recv_msg(void *arg);
void process_db_message(MYSQL *conn, int sock, char *line);
int send_server_msg(int sock, const char *out_msg);
void error_handling(char *msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:(PASSWORD)]", name);
    write(sock, msg, strlen(msg));
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);

    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    if (sock != -1)
        close(sock);
    return 0;
}

int send_server_msg(int sock, const char *out_msg) {
    int written;

    if (out_msg == NULL) return -1;

    printf("send to server: %s", out_msg);
    written = write(sock, out_msg, strlen(out_msg));
    if (written <= 0) {
        perror("write");
    }
    return written;
}

void *send_msg(void *arg) {
    int *sock = (int *)arg;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
    while (1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
        if (FD_ISSET(STDIN_FILENO, &newset)) {
            fgets(msg, BUF_SIZE, stdin);
            if (!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            else if (msg[0] != '[') {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            }
            else
                strcpy(name_msg, msg);
            if (write(*sock, name_msg, strlen(name_msg)) <= 0) {
                *sock = -1;
                return NULL;
            }
        }
        if (ret == 0) {
            if (*sock == -1)
                return NULL;
        }
    }
}

void process_db_message(MYSQL *conn, int sock, char *line) {
    MYSQL_ROW sqlrow;
    int res;
    int i = 0;
    char sql_cmd[512] = {0};
    char *pToken;
    char *pArray[ARR_CNT] = {0};

    if (line == NULL || line[0] == '\0')
        return;

    fputs(line, stdout);
    fputc('\n', stdout);

    pToken = strtok(line, "[:@]");
    while (pToken != NULL) {
        pArray[i] = pToken;
        if (++i >= ARR_CNT)
            break;
        pToken = strtok(NULL, "[:@]");
    }

    if (i < 2)
        return;

    if (!strcmp(pArray[0], "KIOSK") && (i == 2)) {
        const char *locker_no = pArray[1];

        snprintf(sql_cmd, sizeof(sql_cmd),
                 "select replace(replace(replace(upper(rfid_tag), ' ', ''), ':', ''), '-', '') "
                 "from users where locker_no='%s' and rfid_tag is not null limit 1",
                 locker_no);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if ((sqlrow != NULL) && (sqlrow[0] != NULL) && (sqlrow[0][0] != '\0')) {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]KIOSK_AUTH@%s@%s\n", LOCKER_CLIENT_ID, locker_no, sqlrow[0]);
            send_server_msg(sock, sql_cmd);
            printf("kiosk auth accepted: locker_no=%s rfid_tag=%s\n", locker_no, sqlrow[0]);
        }
        else {
            printf("kiosk auth rejected: locker_no=%s rfid_tag not found\n", locker_no);
        }
        mysql_free_result(result);
    }
    else if (!strcmp(pArray[1], "KIOSK") && (i == 3)) {
        const char *locker_no = pArray[2];

        snprintf(sql_cmd, sizeof(sql_cmd),
                 "select replace(replace(replace(upper(rfid_tag), ' ', ''), ':', ''), '-', '') "
                 "from users where locker_no='%s' and rfid_tag is not null limit 1",
                 locker_no);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if ((sqlrow != NULL) && (sqlrow[0] != NULL) && (sqlrow[0][0] != '\0')) {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]KIOSK_AUTH@%s@%s\n", LOCKER_CLIENT_ID, locker_no, sqlrow[0]);
            send_server_msg(sock, sql_cmd);
            printf("kiosk auth accepted: locker_no=%s rfid_tag=%s\n", locker_no, sqlrow[0]);
        }
        else {
            printf("kiosk auth rejected: locker_no=%s rfid_tag not found\n", locker_no);
        }
        mysql_free_result(result);
    }
    else if (!strcmp(pArray[0], "KIOSK_AUTH") && (i == 3)) {
        const char *locker_no = pArray[1];
        const char *rfid_tag = pArray[2];

        snprintf(sql_cmd, sizeof(sql_cmd),
                 "select count(*) from users where locker_no='%s' "
                 "and replace(replace(replace(upper(rfid_tag), ' ', ''), ':', ''), '-', '')=upper('%s')",
                 locker_no, rfid_tag);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if ((sqlrow != NULL) && (atoi(sqlrow[0]) > 0)) {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]KIOSK_AUTH@%s@%s\n", LOCKER_CLIENT_ID, locker_no, rfid_tag);
            send_server_msg(sock, sql_cmd);
            printf("kiosk auth accepted: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        else {
            printf("kiosk auth rejected: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        mysql_free_result(result);
    }
    else if (!strcmp(pArray[1], "KIOSK_AUTH") && (i == 4)) {
        const char *locker_no = pArray[2];
        const char *rfid_tag = pArray[3];

        snprintf(sql_cmd, sizeof(sql_cmd),
                 "select count(*) from users where locker_no='%s' "
                 "and replace(replace(replace(upper(rfid_tag), ' ', ''), ':', ''), '-', '')=upper('%s')",
                 locker_no, rfid_tag);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if ((sqlrow != NULL) && (atoi(sqlrow[0]) > 0)) {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]KIOSK_AUTH@%s@%s\n", LOCKER_CLIENT_ID, locker_no, rfid_tag);
            send_server_msg(sock, sql_cmd);
            printf("kiosk auth accepted: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        else {
            printf("kiosk auth rejected: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        mysql_free_result(result);
    }
    //[LOCKER_SQL]RFID_AUTH@01@(RFID UID)
    else if (!strcmp(pArray[1], "RFID_AUTH") && (i == 4)) {
        const char *locker_no = pArray[2];
        const char *rfid_tag = pArray[3];

        snprintf(sql_cmd, sizeof(sql_cmd),
                 "select count(*) from users where locker_no='%s' "
                 "and replace(replace(replace(upper(rfid_tag), ' ', ''), ':', ''), '-', '')=upper('%s')",
                 locker_no, rfid_tag);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if ((sqlrow != NULL) && (atoi(sqlrow[0]) > 0)) {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]RFID_AUTH@%s@%s\n", LOCKER_CLIENT_ID, locker_no, rfid_tag);
            send_server_msg(sock, sql_cmd);
            printf("rfid auth accepted: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        else {
            snprintf(sql_cmd, sizeof(sql_cmd), "[%s]RFID_DENY@%s@%s\n", LOCKER_CLIENT_ID, locker_no, rfid_tag);
            send_server_msg(sock, sql_cmd);
            printf("rfid auth rejected: locker_no=%s rfid_tag=%s\n", locker_no, rfid_tag);
        }
        mysql_free_result(result);
    }
    else if (!strcmp(pArray[1], "SENSOR") && (i == 3)) {
        printf("sensor response from %s: %s\n", pArray[0], pArray[2]);
    }
    
    //[LOCKER_SQL]LOCKER_STATE@L01@CLOSED@LOCKED@EMPTY
    else if (!strcmp(pArray[1], "LOCKER_STATE") && (i == 6)) {
        snprintf(sql_cmd, sizeof(sql_cmd),
                 "insert into lockers(locker_no,door_state,lock_state,item_state,updated_at) "
                 "values('%s','%s','%s','%s',now()) "
                 "on duplicate key update "
                 "door_state=values(door_state),lock_state=values(lock_state),"
                 "item_state=values(item_state),updated_at=now()",
                 pArray[2], pArray[3], pArray[4], pArray[5]);

        res = mysql_query(conn, sql_cmd);
        if (!res)
            printf("locker updated %lu rows\n", (unsigned long)mysql_affected_rows(conn));
        else
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }
    
    //[LOCKER_SQL]LOCKER_LOG@L01@UNLOCK@RFID_USER@CLOSED@UNLOCKED@EMPTY@UNLOCK
    else if (!strcmp(pArray[1], "LOCKER_LOG") && (i == 9)) {
        snprintf(sql_cmd, sizeof(sql_cmd),
                 "insert into locker_event_log(locker_no,event_type,source,door_state,lock_state,item_state,result) "
                 "values('%s','%s','%s','%s','%s','%s','%s')",
                 pArray[2], pArray[3], pArray[4], pArray[5], pArray[6], pArray[7], pArray[8]);

        res = mysql_query(conn, sql_cmd);
        if (!res)
            printf("locker log inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
        else
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }

    //[LOCKER_SQL]ALERT@L01@FORCED_OPEN@OPEN@LOCKED@EMPTY@FORCED_OPEN
    else if (!strcmp(pArray[1], "ALERT") && (i == 8)) {
        snprintf(sql_cmd, sizeof(sql_cmd),
                 "insert into alert_log(locker_no,alert_type,door_state,lock_state,item_state,message) "
                 "values('%s','%s','%s','%s','%s','%s')",
                 pArray[2], pArray[3], pArray[4], pArray[5], pArray[6], pArray[7]);

        res = mysql_query(conn, sql_cmd);
        if (!res)
            printf("alert inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
        else
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }

    else if (!strcmp(pArray[1], "GETDB") && i == 3) {
        sprintf(sql_cmd, "select value from device where name='%s'", pArray[2]);

        if (mysql_query(conn, sql_cmd)) {
            fprintf(stderr, "%s\n", mysql_error(conn));
            return;
        }
        MYSQL_RES *result = mysql_store_result(conn);
        if (result == NULL) {
            fprintf(stderr, "%s\n", mysql_error(conn));
            return;
        }

        sqlrow = mysql_fetch_row(result);
        if (sqlrow != NULL) {
            sprintf(sql_cmd, "[%s]%s@%s@%s\n", pArray[0], pArray[1], pArray[2], sqlrow[0]);
            send_server_msg(sock, sql_cmd);
        }
        mysql_free_result(result);
    }

    else if (!strcmp(pArray[1], "SETDB") && i >= 4) {
        sprintf(sql_cmd, "update device set value='%s', date=now(), time=now() where name='%s'", pArray[3], pArray[2]);

        res = mysql_query(conn, sql_cmd);
        if (!res) {
            if (i == 4)
                sprintf(sql_cmd, "[%s]%s@%s@%s\n", pArray[0], pArray[1], pArray[2], pArray[3]);
            else if (i == 5)
                sprintf(sql_cmd, "[%s]%s@%s\n", pArray[4], pArray[2], pArray[3]);
            else
                return;

            printf("inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
            send_server_msg(sock, sql_cmd);
        }
        else
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }
}

void *recv_msg(void *arg) {
    MYSQL *conn;
    char *host = "(DB HOST)";
    char *user = "(DB USER)";
    char *pass = "(DB PASSWORD)";
    char *dbname = "(DB NAME)";

    int *sock = (int *)arg;
    char recv_buf[NAME_SIZE + BUF_SIZE + 1];
    char pending[(NAME_SIZE + BUF_SIZE + 1) * 2] = {0};
    size_t pending_len = 0;
    int str_len;

    conn = mysql_init(NULL);

    puts("MYSQL startup");
    if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0))) {
        fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }
    else
        printf("Connection Successful!\n\n");

    while (1) {
        memset(recv_buf, 0x0, sizeof(recv_buf));
        str_len = read(*sock, recv_buf, NAME_SIZE + BUF_SIZE);
        if (str_len <= 0) {
            *sock = -1;
            return NULL;
        }

        if (pending_len + (size_t)str_len >= sizeof(pending)) {
            pending_len = 0;
            memset(pending, 0x0, sizeof(pending));
        }

        memcpy(pending + pending_len, recv_buf, str_len);
        pending_len += (size_t)str_len;
        pending[pending_len] = '\0';

        char *line_start = pending;
        char *newline;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            if ((newline > line_start) && (*(newline - 1) == '\r'))
                *(newline - 1) = '\0';
            process_db_message(conn, *sock, line_start);
            line_start = newline + 1;
        }

        pending_len = strlen(line_start);
        memmove(pending, line_start, pending_len + 1);
    }
    mysql_close(conn);
    return NULL;
}

void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
