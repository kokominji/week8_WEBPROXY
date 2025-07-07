//클라이언트 연결을 기다리고 연결이 되면 메시지를 받고, 다시 돌려준다

// 실행 ./echoserver 8000

#include "csapp.h"

void echo(int connfd); //인자 : 연결소켓

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    //서버 소켓 열기
    listenfd = Open_listenfd(argv[1]);
    while (1) { //서버는 연결 계속 기다림. 하나의 연결 처리한 뒤에도 다시 대기
        clientlen = sizeof(struct  sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //연결 수락

        //클라이언트 주소->사람이 읽을 수 있는 호스트명/포트로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd); //소켓을 echo함수에 넘겨줌 echo는 클라이언트와 통신(read->write)처리
        Close(connfd);
    }
        exit(0);
}