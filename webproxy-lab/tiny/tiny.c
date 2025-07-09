/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

//1. tiny main - 서버 실행 -> 무한 루프 -> 요청 수락
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  //잘못된 인자가 들어왔을 경우 오류메세지 출력 후 종료
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  //포트열기
  listenfd = Open_listenfd(argv[1]);
  //내부에서 open_listenfd호출. 소켓생성->바인드->리슨
//   int Open_listenfd(char *port) 
// {
//     int rc;

//     if ((rc = open_listenfd(port)) < 0)
// 	unix_error("Open_listenfd error");//실패시 강제 종료
//     return rc; 성공시 리스닝 소켓 디스크립터 리턴
// }
  while (1)
  {
    clientlen = sizeof(clientaddr);

    //connfd는 클라이언트와 통신할 소켓
    //요청이 들어오면 Acccept이 connfd소켓생성 listenfd는 리슨 상태유지
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd); //클라이언트 요청읽고 정적/동적 구분해서 처리
    Close(connfd); 
  }
}

//2. doit - 클라이언트의 HTTP 요청 하나를 처리
void doit(int fd)
{
  int is_static; //정적 컨텐츠(1) 동적 컨텐츠(0)인지
  struct stat sbuf; //파일 크기 저장
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  //요청 라인 & 헤더 읽기
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  //메소드가 GET인지 확인. GET이 아니라면 오류메세지 출력, GET이라면 헤더만 읽고 버림
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); 

  //URI 파싱 확인
  is_static = parse_uri(uri, filename, cgiargs);

  //파일있는지 확인
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  //정적/동적 처리 구분
  if (is_static)
  { 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); //파일을 클라이언트에 전송
  }
  else
  { 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); //CGI실행 + 결과 돌려줌
  }
}

//3. clienterror - 서버에서 클라이언트로 에러 응답보내기
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body,"<html><title>TinyError</title>");
  sprintf(body,"%s<bodybgcolor=""ffffff"">\r\n",body);
  sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
  sprintf(body,"%s<p>%s:%s\r\n",body,longmsg,cause);
  sprintf(body,"%s<hr><em>TheTinyWebserver</em>\r\n",body);

  sprintf(buf,"HTTP/1.0%s%s\r\n",errnum,shortmsg);
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Content-type:text/html\r\n");
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Content-length:%d\r\n\r\n",(int)strlen(body));
  Rio_writen(fd,buf,strlen(buf));
  Rio_writen(fd,body,strlen(body));
}

//4. read_requesthdrs - HTTP 헤더 읽기.읽기만 하고 버림
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE]; //한 줄씩 읽은 헤더 내용을 임시 저장

    Rio_readlineb(rp, buf, MAXLINE);

    //빈 줄이 나올때까지 계속 읽음
    while (strcmp(buf, "\r\n")) { //같지않으면 참 -> buf가 "\r\n"과 같지 않으면 반복.
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

//5. parse_uri - 클라이언트가 요청한 URL분석해서 정적/동적 구분
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) { //cgi-bin이 없다면 정적
        strcpy(cgiargs, ""); //쿼리 파라미터 없음
        strcpy(filename, ".");
        strcat(filename, uri); //파일 경로연결
        if (uri[strlen(uri) - 1] == '/') 
            strcat(filename, "home.html");
        return 1; //1리턴 : 정적 컨텐츠라는 뜻
    } else {

        //동적 컨텐츠 처리
        ptr = index(uri, '?'); 
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0'; //?를 NULL문자로 바꿔서 파일 경로만 남김
        } else {
              strcpy(cgiargs, ""); //?없다면 쿼리 파라미터 없음
        }
          strcpy(filename, ".");
          strcat(filename, uri);
          return 0; //0리턴 : 동적 컨텐츠(CGI)라는 뜻
    }
}

// //6. serve_static - 정적 파일을 클라이언트에 보내줌. MIME 타입 결정
// void serve_static(int fd, char *filename, int filesize)
// {
//     int srcfd; //디스크에서 파일읽을 때 파일 디스크립터
//     char *srcp, filetype[MAXLINE], buf[MAXBUF];

//     get_filetype(filename, filetype); //MIME 타입 결정
//     sprintf(buf, "HTTP/1.0 200 OK\r\n");
//     sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
//     sprintf(buf, "%sConnection: close\r\n", buf);
//     sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
//     sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

//     Rio_writen(fd, buf, strlen(buf)); //응답 헤더 클라이언트에 전송
//     printf("Response headers:\n");
//     printf("%s", buf);

//     srcfd = Open(filename, O_RDONLY, 0);
//     srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
//     Close(srcfd);
//     Rio_writen(fd, srcp, filesize); //매핑된 내용을 소켓으로 보냄
//     Munmap(srcp, filesize);
// }

// //숙제 문제 11.9 : malloc,rio_readn,rio_writen 사용
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *filebuf; //파일 내용을 담을 메모리 버퍼
    ssize_t bytes_read; 
    char filetype[MAXLINE], buf[MAXBUF];

    // MIME 타입 결정
    get_filetype(filename, filetype);

    //HTTP 응답 헤더 만들기
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //HTTP/1.0 응답 헤더 생성 시작
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); //서버 정보 헤더 추가
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

    // 완성된 HTTP 응답 헤더를 클라이언트로 전송
    Rio_writen(fd, buf, strlen(buf));

    srcfd = Open(filename, O_RDONLY, 0); //요청한 정적 파일을 디스크에서 읽기
    filebuf = (char *)Malloc(filesize); //파일 크기 만큼 메모리 버퍼 할당
    bytes_read = Rio_readn(srcfd, filebuf, filesize); //디스크 파일 내용을 filebuf로 읽어오기 

    //읽어온 파일 내용을 클라이언트로 전송
    Rio_writen(fd, filebuf, bytes_read);

    Free(filebuf); //메모리 버퍼 해제
    Close(srcfd); //디스크 파일 닫기
}

//7. get_filetype - 파일 이름보고 Content-Type 결정
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

//8. serve_dynamic - 동적컨텐츠(CGI) 실행
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf)); 
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    //fork로 자식프로세스 생성
    if (Fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1); // CGI환경변수 설정

        // 표준 출력을 클라이언트 소켓으로 리디렉션:
        // CGI 프로그램의 printf 결과가 클라이언트에게 직접 전송되도록 함
        Dup2(fd, STDOUT_FILENO);

        // CGI 프로그램 실행
        // environ: 현재 환경 변수 목록
        Execve(filename, emptylist, environ); 
    }
    Wait(NULL); // 부모 프로세스는 자식이 종료되기를 기다림 -> 자식 프로세스가 좀비가 되는 것을 방지
}