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

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("====== HTTP 연결시작 ======\n");            
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

// 전체 트랜잭션 컨트롤러
// 하나의 HTTP 요청 전체 처리

/*
fd(클라이언트 연결) 들어옴
 → 요청 읽기
 → 요청 파싱 (method, uri)
 → 정적/동적 판단
 → 파일 존재 확인
 → 권한 확인
 → 정적이면 파일 응답
 → 동적이면 프로그램 실행
*/
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("요청 라인 %s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  //GET만 허용
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  // URI 파싱 -> 정적/동적 판단 + filename, cgiargs 분리
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }
  // printf("=== Request ===\n");
  // printf("Method: %s\n", method);
  // printf("URI: %s\n", uri);
  // printf("Filename: %s\n", filename);
  // printf("is_static: %d\n", is_static);

  //정적일 때
  if (is_static)
  { /* Serve static content */
    printf("Serving static file: %s\n", filename);
    printf("====== HTTP 연결완료 ======\n");            

    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }

    // http 헤더 Content-Type, Content-Length 포함해서 body로 전송
    serve_static(fd, filename, sbuf.st_size);
  }
  else
  //동적일 때 - 프로그램 출력이 그대로 HTTP 응답 body가 됨
  { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

//에러 발생 시 HTML 에러 페이지 생성해서 응답 (404 Not Found 등)
//서버가 직접 응답생성
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body + strlen(body), "<body bgcolor=""ffffff"">\r\n");
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
  sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n");

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

//HTTP 요청 헤더들 읽기 (Http 파싱 단계 일부 read -> headers 읽기)
// void read_requesthdrs(rio_t *rp)
// {
//   char buf[MAXLINE];

//   Rio_readlineb(rp, buf, MAXLINE);
//   while (strcmp(buf, "\r\n"))
//   {
//     Rio_readlineb(rp, buf, MAXLINE);
//     printf("%s", buf);
//   }
//   return;

// }
// 요청 라인과 요청헤더를 echo하는 버전
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);// 요청 라인 읽기(GET /index.html HTTP/1.0\r\n) 
  printf("%s", buf);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("요청 헤더 %s", buf);
  }
  return;
}

//URI를 파일 경로와 CGI 인자로 분리 (정적 콘텐츠면 파일 경로, 동적 콘텐츠면 CGI 인자)
//URI → filename + cgiargs
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  { /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else
  { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

//정적 파일 읽어서 HTTP 응답으로 전송 (Content-Type, Content-Length 헤더 포함)
//read(file) → write(socket)
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("\n======= 응답헤더 =======\n%s\n", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}


//파일 확장자 보고 MIME 타입 결정 http 헤더 Content-Type에 들어갈 값 결정
//type/subtype
  void get_filetype(char *filename, char *filetype)
  {
    if (strstr(filename, ".html"))
      strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
      strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
      strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
      strcpy(filetype, "video/mpeg");
    else if (strstr(filename, ".mpeg"))
      strcpy(filetype, "video/mpeg");
    else
      strcpy(filetype, "text/plain");
  }


  //CGI 프로그램 fork/exec해서 실행, 출력 받아서 클라이언트로 전송
  //fork → execve → stdout → socket
  void serve_dynamic(int fd, char *filename, char *cgiargs)
  {
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    printf("====== 완료 ======\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0)
    { /* Child */
      setenv("QUERY_STRING", cgiargs, 1);
      Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
      Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
  }
