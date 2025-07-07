//텍스트줄을 읽고 echo해주는 함수

//connfd는 echo 함수가 어떤 클라이언트와 통신해야 하는지를 
//정확히 알려주는 유일한 정보

#include "csapp.h"

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t  rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("sever received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}