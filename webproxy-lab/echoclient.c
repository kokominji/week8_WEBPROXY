//클라이언트가 서버에 문자열을 보내면, 서버는 그 문자열을 다시 돌려줌

// 실행 telnet localhost 8000

#include "csapp.h"

int main (int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1]; //호스트 이름
    port = argv[2]; //포트, 서비스네임

    //서버 연결
    //Open_clientfd 동작 getaddrinto -> socket() -> connect()
    clientfd = Open_clientfd(host, port); //hostname과 port로 TCP연결
    Rio_readinitb(&rio, clientfd);

    //서버로 보냄
    while (Fgets(buf, MAXLINE, stdin) != NULL) { //한줄씩 읽음
        Rio_writen (clientfd, buf, strlen (buf)); //서버로 전송
        Rio_readlineb(&rio, buf, MAXLINE); //서버 응답 받음
        Fputs(buf,stdout); //화면 출력
    }
    //연결 종료
    Close(clientfd);
    exit(0);
}