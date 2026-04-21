#include "../csapp.h"

void echo(int connfd);

int main(int argc, char **argv){
    //.echoserver 8080 argv: 명령줄 인자 문자열 배열

    //socket file descriptor 2개선언 (listnefd 듣는 용도의 소켓(대기용), connfd(통신용) 실제클라이언트와 통신하는 용도의 소켓)
    //listenfd는 서버시작시 단 한번 생성, connfd는 매번 새롭게 생성된다.
    int listenfd, connfd;

    // accept()는 클라이언트 주소를 채워줄 때, 주소 구조체의 길이를 인자로 받는다. 클라이언트 주소 구조체의 길이를 저장하는 변수
    // unsigned int clientlen
    socklen_t clientlen;

    //클라이언트 주소를 저장할 구조체 
    struct sockaddr_storage clientaddr;

    //클라이언트의 호스트 이름과 포트 번호를 문자열로 저장할 배열
    char client_hostname[MAXLINE], client_port[MAXLINE];

    //명령행 인자 개수 확인
    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // 서버가 요청을 받을 리스닝 소켓 생성
    // .echoserver 8080 -> argv[1] = 8080

    //getaddrinfo (socket, bind, listen))
    // argv[1]포트에서 기다리는 서버 소켓 생성, 리턴값은 listenfd에 대기용 소켓 fd들어감
    listenfd = Open_listenfd(argv[1]);

    //무한반복
    while(1){
        // clientaddr 구조체의 크기를 clientlen에 넣음
        // 클라이언트 주소를 담은 버퍼의 크기 알려줘야함.
        clientlen = sizeof(struct sockaddr_storage);

        // listenfd에서 클라이언트의 연결 요청이 오면 수락 후, 
        // connfd에 통신용 소켓 생성, clientaddr에 클라이언트 주소 저장, clientlen에 주소 길이 저장
        connfd = Accept (listenfd, (SA *)&clientaddr, &clientlen);
        // Accept = 새 연결을 만들고, 그 연결을 가리키는 fd(정수)를 반환한다.
        // 동시에 클라이언트 주소 addr와 길이 addlrlen을 채워줌.

        // 클라이언트 주소 구조체를 사람이 읽을 수 있는 문자열로 바꾼다. 
        // clientaddr는 구조체라 출력하기 불편
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        // 누가 접속했는지 출력
        printf("Connected to (호스트: %s, 클라이언트 포트: %s)\n", client_hostname, client_port);

        // 클라이언트가 보낸 데이터를 읽고, 그대로 다시 돌려보냄
        echo(connfd);

        //현재 클라이언트와의 연결 종료
        Close(connfd);
    }
    exit(0);
}

void echo(int connfd){
    //읽은 바이트 수 저장
    size_t n;
    //클라이언트가 보낸 데이터를 잠깐 담아둘 버퍼로 한 줄을 읽어서 저장함.
    char buf[MAXLINE];
    //
    rio_t rio;

    // Rio_readinitb - rio구조체를 connfd에 연결해서 읽기 준비상태로 초기화
    // 내부에서는 해당 fd에서 읽고, 버퍼에 남아있는데이터는 없고, 읽을 위치를 버퍼 시작으로 설정
    Rio_readinitb(&rio, connfd);


    // 내부에서 read()를 여러번 호출해서 \n이 나올 때까지 데이터를 모음.
    // 그래서 write 한번에 read 여러번이 될 수 있음.
    //사용자가 종료하면 0이 리턴됨. 0이 리턴될 때까지 반복
    // n == 0 이되면 네트워크 종료
    //"안녕하세요\n\0 여기서 0은 문자열로 쓰기위해 함수에서 0넣어줌"
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        printf("server received %d bytes\n", (int)n);
        // buf에 있는 n바이트 데이터를 connfd(클라이언트소켓)로 전송하는 함수.
        Rio_writen(connfd, buf, n);
    }
}

// typedef struct {
//     int rio_fd; 읽을 대상(소켓 파일 디스크립터)
//     int rio_cnt; 현재 버퍼에 남아있는 '읽지 않은 바이트 수'
//     char *rio_bufptr; 다음에 읽을 위치 (버퍼 내부 포인터)
//     char rio_buf[RIO_BUFSIZE]; 실제 데이터가 들어있는 버퍼(메모리 공간)
// } rio_t;

//rio_buf = "hello\nworld\n"
//rio_cnt = 12
//rio_bufptr -> 'h'


/*기본 read()는 문제있음.
* 한번에 원하는 만큼 읽히지 않을 수 있고,
* 네트워크는 데이터가 쪼개져서 도착함.
* 줄단위 \n로 읽기 어려움.
* 그래서 robust I/O 패키지 사용
* rio_readlineb()는 줄단위로 읽어줌.
* rio_readinitb()는 rio_t 구조체 초기화, rio_readlineb()는 rio_t 구조체를 이용해서 줄단위로 읽음.
* Rio_writen()는 robust하게 원하는 바이트 수만큼 써줌.

* 한번에 크게 읽어두고 조금씩 꺼내쓰는 구조 필요
*read() 하면 시스템콜 → 느림
*/


/*
* write()는 한번에 원하는 만큼 써지지 않을 수 있음.
* Rio_writen()는 robust하게 원하는 바이트 수만큼 써줌.
*/