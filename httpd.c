/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/stat.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

void *accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void *accept_request(void *arg)
{
    int client = *(int*)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    /**************************************************************
    struct stat 是一个用于存储文件状态信息的结构体
    下面是 struct stat 结构体的一些常见成员变量：

    st_dev: 文件的设备号。
    st_ino: 文件的inode编号。
    st_mode: 文件
    ************************************************************/
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf));

    /*********************************************************************
    i = 0; j = 0;：这两行代码将变量 i 和 j 初始化为 0。i 用于追踪当前解析的位置，j 用于记录方法字符串的长度。

    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))：这是一个 while 循环，它的循环条件是：
    当前字符不是空格且未超出 method 数组的容量。这个条件用于确保只解析方法字符串的有效部分，并防止数组越界。

    method[i] = buf[i];：在循环中，将 buf 中的字符逐个复制到 method 中。

    i++;：每次复制完一个字符后，i 增加 1，以便在下一次循环中处理下一个字符。

    j = i;：在循环结束后，将 j 更新为当前 i 的值，以记录方法字符串的长度。

    method[i] = '\0';：在方法字符串的末尾添加空字符 \0，以表示字符串的结束。

    这段代码的作用是从接收到的数据中解析出 HTTP 请求中的方法，并将其存储在 method 字符数组中
    ISspace 的问题，我认为它可能是一个宏或者函数，用于判断一个字符是否为空
    **************************************************************************/
    i = 0; j = 0;
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';

    /*****************************************************************
    这段代码是用来检查 HTTP 请求中的方法是否是 GET 或 POST。让我解释一下：

    strcasecmp() 是一个字符串比较函数，用于忽略大小写地比较两个字符串是否相等。
    如果两个字符串相等，返回值为 0；否则返回一个非零值。

    这段代码通过 strcasecmp() 函数比较 method 字符串与 "GET" 和 "POST" 字符串是否相等。
    如果都不相等，说明 HTTP 请求的方法既不是 GET 也不是 POST，那么就调用 unimplemented(client) 函数，然后返回。
    这表示服务器不支持该请求方法，通常会返回一个 "501 Not Implemented" 的 HTTP 响应状态码。

    如果方法是 GET 或 POST，就会继续执行后面的代码，处理相应的请求。
    ************************************************************************/
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return NULL;
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    //这段代码的目的是跳过字符串 buf 中开头的空白字符（例如空格、制表符等），直到遇到第一个非空白字符为止
    while (ISspace(buf[j]) && (j < numchars))
        j++;
 
    //将字符串 buf 中连续的非空白字符复制到另一个字符串 url 中
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)
    {
        /********************************************************************
        这个指针通常用于在处理 HTTP 请求时存储查询字符串（query string）的内容。

        查询字符串是在 URL 中包含的可选部分，通常用于向服务器传递参数。它通常以问号 ? 开头，
        后面跟着以键值对形式表示的参数，不同参数之间使用 & 分隔。

        例如，在以下 URL 中：

        http://example.com/search?q=keyword&page=1
        查询字符串为 ?q=keyword&page=1，其中包含了两个参数 q 和 page。
        ********************************************************************/
        query_string = url;

        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        
        /***************************************************************
        如果查询字符串的第一个字符是问号 ?，则表示这是一个标准的查询字符串格式。
        当检测到查询字符串以问号开头时，将 cgi 标志设为 1，表示需要执行 CGI 处理。
        将查询字符串的第一个字符设置为字符串结束符 \0，以将查询字符串截断为请求行部分，
        这样请求行部分和查询字符串部分就分开了。
        将 query_string 指针向后移动一位，指向查询字符串的实际内容。
        总之，这段代码的作用是检测 HTTP 请求中是否包含查询字符串，并将其与请求行分离开来，以便进一步处理。
        ****************************************************************************/
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    /*********************************************************************
    这段代码使用 sprintf 函数将一个字符串格式化后存储到目标字符串 path 中。让我解释一下这段代码的功能和参数：

    sprintf 函数是 C 语言中用于格式化字符串的函数，其原型为 int sprintf(char *str, const char *format, ...)。
    它类似于 printf 函数，但是将格式化后的字符串写入到指定的字符数组中，而不是输出到标准输出流。
    path 是一个字符数组，用于存储格式化后的字符串，通常用于表示文件路径。
    "htdocs%s" 是格式化字符串，其中 %s 是格式说明符，表示后续参数将被格式化为字符串并插入到该位置。
    这个格式化字符串的含义是在 htdocs 目录下拼接上一个字符串，这个字符串由后续的 url 参数提供。
    url 是一个字符串，表示请求的 URL 或者资源路径。
    因此，这段代码的作用是将指定的 url 路径与 htdocs 目录拼接起来，生成完整的文件路径，并存储到 path 字符数组中。
    这样，就可以使用 path 变量来引用 htdocs 目录下的相应资源。

    htdocs 目录通常是在 Web 服务器中使用的一个特定目录，用于存放网站的相关文件，
    包括 HTML、CSS、JavaScript、图像、视频等资源文件。这个目录在一些 Web 服务器软件中是默认的站点根目录，
    例如 Apache HTTP Server。
    ************************************************************************/
    sprintf(path, "htdocs%s", url);

    /********************************************************************
    这段代码片段是在检查路径字符串 path 的末尾是否以斜杠结尾，如果是的话，则将字符串 "index.html" 追加到路径的末尾。
    这段代码的目的是确保路径字符串 path 指向的是一个文件而不是一个目录。
    如果路径是一个目录，那么就将路径补全为该目录下的 index.html 文件，以便后续的处理可以正常进行。
    *************************************************************************/
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    /********************************************************************
    这段代码片段是在检查路径字符串 path 对应的文件是否存在。让我来解释一下代码的逻辑：

    stat(path, &st) 函数用于获取文件的状态信息，并将其存储在结构体 st 中。如果该函数返回 -1，说明获取文件状态信息失败，
    即文件不存在或出现了其他错误。
    如果文件不存在，进入 if 语句块内部执行以下操作：
    通过 get_line 函数读取并丢弃客户端发送的请求头部信息，直到读取到一个空行为止，空行通常表示请求头部信息的结束。
    调用 not_found(client) 函数，向客户端发送 404 Not Found 响应，表示请求的文件未找到。
    ********************************************************************/
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }

    else
    {
        /*************************************************************
        这段代码片段是在检查文件的类型是否为目录，并且如果是目录的话，将路径字符串 path 追加 "/index.html"。

        st.st_mode & S_IFMT 用来获取文件的类型。在这里，S_IFMT 是一个宏，用于获取文件类型掩码。
        通过 & 操作符与 st.st_mode 结合使用，可以提取文件类型的部分。
        S_IFDIR 是一个宏，表示目录类型。
        所以 (st.st_mode & S_IFMT) == S_IFDIR 这个条件判断就是在检查文件是否是一个目录。
        *****************************************************************/
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");

        /*************************************************************
        这段代码片段是在检查文件的权限是否具有可执行权限，并且如果文件具有可执行权限，则将变量 cgi 设置为 1。

        st.st_mode & S_IXUSR、st.st_mode & S_IXGRP、st.st_mode & S_IXOTH 
        分别用于检查文件的用户（owner）、组（group）和其他用户的可执行权限。
        S_IXUSR、S_IXGRP、S_IXOTH 是宏，表示用户、组和其他用户的可执行权限位。
        如果文件的任何一组权限位具有可执行权限，则 cgi 变量被设置为 1。
        在这里，cgi 变量可能被用作一个标志，用于指示该文件是否是一个可执行的 CGI 程序。
        这段代码的作用是在检查文件是否具有可执行权限，并将其标记为一个 CGI 程序。
        **********************************************************************/
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )
            cgi = 1;

        if (!cgi)
            serve_file(client, path);//调用 cat 把服务器文件返回给浏览器
        else
            execute_cgi(client, path, method, query_string);
    }

    close(client);//在网络编程中，关闭套接字文件描述符通常表示一个连接的结束，即服务器不再与客户端进行通信
    return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);//使用 fgets 函数从已打开的文件中读取一行数据，并将其存储在缓冲区 buf 中
    
    /**************************************************************
    循环读取文件中的每一行数据，并将其发送给客户端，直到文件末尾。
    这通常用于在网络编程中，将文件的内容发送给客户端，比如 Web 服务器向浏览器发送网页内容。
    ************************************************************/
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);//将缓冲区 buf 中的数据发送给客户端
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    char buf[1024];//声明了一个名为 buf 的字符数组，用于存储从标准输入读取的数据。
    int cgi_output[2];//声明了两个整型数组，用于在父进程和子进程之间传递数据的管道。
    int cgi_input[2];
    /*
    在使用 fork() 函数创建新进程时，pid 变量通常用于存储 fork() 函数的返回值，
    以判断当前代码是在父进程还是子进程中执行。在父进程中，pid 的值将是子进程的进程ID，
    而在子进程中，pid 的值将是0。这样就可以通过 pid 的值来执行不同的逻辑，从而实现父子进程之间的区分和交互。
    */
    pid_t pid;//声明了一个变量 pid，用于存储进程ID。

    int status;//声明了一个整型变量 status，用于存储进程的状态信息。
    int i;
    char c;//用于临时存储读取的字符。
    int numchars = 1;//初始化一个变量 numchars，用于统计读取的字符数，并将其初始化为1。
    int content_length = -1;// 初始化一个变量 content_length，用于存储从HTTP请求头中读取的内容长度，默认值为-1

    buf[0] = 'A'; buf[1] = '\0';//buf 就被初始化为一个只包含一个字符 'A' 的字符串。

    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        /*
        读取客户端发送的 HTTP 请求头信息，并从中提取出内容长度（Content-Length）。
        */
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            /*
            如果当前行包含 Content-Length，则将其后面的数字部分转换为整数，并存储到 content_length 变量中。
            这里使用 atoi 函数将字符串转换为整数，并且从 buf 数组的第17个字符开始解析，因为第16个字符是冒号。
            */
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));

            numchars = get_line(client, buf, sizeof(buf));
        }

        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }
    else/*HEAD or other*/
    {
    }

    /*
    同时创建两个管道，一个用于从子进程向父进程传输数据，另一个用于从父进程向子进程传输数据
    调用了 pipe 函数，它创建了一个管道，并将管道的读写端分别存储在 cgi_output 数组中。
    cgi_output[0] 是管道的读端，cgi_output[1] 是管道的写端。
    */
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    /*
    调用了 fork() 函数，它会创建一个新的子进程。在父进程中，fork() 函数返回新创建的子进程的进程ID；在子进程中，返回值为0。
    因此，通过 (pid = fork()) 的赋值操作，可以判断当前是在父进程还是子进程中，并获取子进程的进程ID。
    调用 fork() 函数的进程称为父进程
    */
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    //使用 sprintf 函数将字符串 "HTTP/1.0 200 OK\r\n" 格式化并存储到缓冲区 buf 中。
    //这个字符串是一个 HTTP 响应的状态行，表示请求成功
    //这行代码的作用是将 HTTP 响应状态行格式化并存储到 buf 缓冲区中，以便后续将其发送给客户端。
    /*
    "HTTP/1.0 200 OK\r\n" 是一个包含 HTTP 响应状态行的字符串。其中：
    HTTP/1.0 表示使用的 HTTP 协议版本。
    200 表示状态码，表示请求成功。
    OK 是状态码对应的文本描述。
    \r\n 是回车和换行符，表示换行。
    */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    /************************************************
    根据 pid 的值来判断是父进程还是子进程执行接下来的代码。具体地说：

    如果 pid 大于 0，则说明当前代码是在父进程中执行。pid 的值就是新创建的子进程的 PID。
    如果 pid 等于 0，则说明当前代码是在子进程中执行。因为子进程的 fork() 返回值是 0。
    如果 pid 小于 0，则说明 fork() 调用失败，没有创建成功子进程。

    使得父进程和子进程能够在 fork() 调用之后执行不同的代码路径，以实现并行处理或者其他相关的任务分配。
    **********************************************************/
    if (pid == 0)  /* child: CGI script 子进程中*/
    {
        char meth_env[255];//声明了三个字符数组，用于存储环境变量的字符串。
        char query_env[255];
        char length_env[255];
        //将子进程的标准输出重定向到 cgi_output 管道的写入端，这样子进程的输出将被发送到父进程。
        dup2(cgi_output[1], STDOUT);
        ////将子进程的标准输入重定向到 cgi_input 管道的读取端，这样父进程发送给子进程的输入将被子进程接收。
        dup2(cgi_input[0], STDIN);
        //分别关闭子进程中用于读取的 cgi_output 管道的读取端和用于写入的 cgi_input 管道的写入端，因为子进程不需要使用它们。
        close(cgi_output[0]);
        close(cgi_input[1]);
        //根据请求方法（GET 或 POST）设置 REQUEST_METHOD 环境变量。
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        //将 REQUEST_METHOD 环境变量设置为子进程的环境变量之一。
        putenv(meth_env);

        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            //根据请求正文的长度设置 CONTENT_LENGTH 环境变量。
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        //用于执行 CGI 脚本。path 是 CGI 脚本的路径，execl 函数会用指定的程序替换当前进程，并执行该程序。
        execl(path, NULL);
        exit(0);//退出子进程

    } else {    /* parent */
        //关闭与子进程通信的管道的写端口，因为父进程不再需要向子进程写入数据。
        close(cgi_output[1]);
        //关闭与子进程通信的管道的读端口，因为父进程不会从子进程读取数据。
        close(cgi_input[0]);

        /*************************************
         * 如果请求方法是 POST，进入循环，该循环会读取来自客户端的请求数据，并将其写入到与子进程通信的管道中。
        **********************************************/
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                /*
                这行代码使用 recv() 函数从客户端接收数据。让我逐个解释参数的含义：

                client：这是与客户端连接的套接字描述符，用于接收客户端发送的数据。
                &c：这是一个字符型变量的地址，用于存储接收到的数据。recv() 函数将从客户端接收到的数据存储到这个变量中。
                1：这是要接收的数据的最大长度，这里是 1 字节，表示一次接收一个字节的数据。
                0：这是一个标志位，用于控制接收操作的行为。在这里，参数为 0 表示没有特殊的标志位。
                综上，这行代码的作用是从客户端接收一个字节的数据，并将其存储到变量 c 中。
                */
                recv(client, &c, 1, 0);
                /*
                这行代码使用 write() 函数将数据写入到与子进程通信的管道中。让我逐个解释参数的含义：

                cgi_input[1]：这是与子进程通信的管道的写端口，即父进程通过这个文件描述符向子进程发送数据。
                &c：这是一个字符型变量的地址，表示要发送的数据。在这里，c 存储了从客户端接收到的一个字节的数据。
                1：这是要发送的数据的长度，这里是 1 字节，表示一次发送一个字节的数据。
                综上，这行代码的作用是将从客户端接收到的一个字节的数据写入到与子进程通信的管道中，以便子进程能够读取到这些数据。
                */
                write(cgi_input[1], &c, 1);
            }
        //通过循环从与子进程通信的管道中读取数据，直到读取到文件结尾（EOF）。
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);//将从子进程读取到的数据发送给客户端。

        close(cgi_output[0]);//关闭与子进程通信的管道的读端口，因为父进程已经读取完所有来自子进程的数据。
        close(cgi_input[1]);//关闭与子进程通信的管道的写端口，因为父进程不再需要向子进程写入数据。
        /*
        等待子进程结束。父进程在这里会阻塞，直到子进程执行完毕。
        waitpid() 函数的第一个参数是子进程的 PID，第二个参数是一个整型指针，用于存储子进程的退出状态，
        最后一个参数为 0 表示等待任何子进程结束。
        */
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
/*
这段代码是一个函数，用于从套接字 sock 中读取一行数据，并将其存储在字符数组 buf 中，最多读取 size - 1 个字符。

i 用于跟踪当前已经读取的字符数，初始化为 0。
c 是用于存储每次从套接字中读取的字符的变量，初始化为空字符 \0。
n 用于存储 recv() 函数的返回值，表示读取到的字符数。
在一个 while 循环中，不断尝试从套接字中读取字符，直到读取到换行符 \n 或者已经读取了 size - 1 个字符为止。

读取一个字符到 c 中，使用 recv() 函数。
recv() 函数是用于从套接字接收数据的系统调用（函数）。它通常在网络编程中使用，用于从一个已连接的套接字（socket）接收数据

ssize_t recv(int sockfd, void *buf, size_t len, int flags);
sockfd 参数是一个文件描述符，用于标识一个已连接的套接字，通过 socket() 函数创建。
buf 参数是一个指向存储接收数据的缓冲区的指针。
len 参数是要接收的数据的最大长度（以字节为单位）。
flags 参数是一组标志，可以影响接收操作的行为。
recv() 函数会阻塞程序，直到有数据可读取或者发生错误。如果成功接收数据，它将返回实际接收到的字节数；如果发生错误，返回 -1，
并设置 errno 来指示具体的错误类型。
在网络编程中，通常会循环调用 recv() 函数来接收完整的数据，因为一次调用可能只能接收到部分数据。

如果读取到的字符是回车符 \r，则尝试读取下一个字符，判断是否为换行符 \n。
如果是换行符，则将其从输入流中移除；如果不是换行符，则将 c 设置为换行符 \n。
将读取到的字符存储在 buf 中，并将 i 增加 1。
如果读取到的字符数大于 0，则继续读取下一个字符；否则，将 c 设置为换行符 \n，结束循环。
将字符串的结束符 \0 添加到 buf 的末尾，以确保字符串的正确结束。
返回读取的字符数（即字符串的长度）。
这个函数的作用是从套接字中读取一行数据，通常在解析 HTTP 请求报文时会用到。
*/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);//当 recv 函数使用参数 0 调用时，它会从输入队列中删除已接收的数据。
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                /******************************************************
                这行代码的作用是从套接字 sock 中接收一个字节的数据，并查看但不移出接收缓冲区中的数据，
                接收到的数据存储在变量 c 中，而不影响缓冲区中的数据。这种操作通常用于查看接收缓冲区中的数据，
                而不会真正地将其移出缓冲区，以便稍后的处理。
                ********************************************************/
                n = recv(sock, &c, 1, MSG_PEEK);

                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file 
 
 用于发送 HTTP 响应头部信息给客户端。它首先发送一个状态行（"HTTP/1.0 200 OK\r\n"），表明请求成功；
 然后发送服务器信息（可能是预先定义好的常量 SERVER_STRING）；
 接着发送 Content-Type 头部，指示响应的内容类型为 text/html；最后发送一个空行，表示头部信息结束。*/
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);//如果 buf 中之前有其他信息，那么在执行 strcpy 后，
    //这些信息就会被 SERVER_STRING 的内容所取代，而不会保留原有的信息。
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;//创建了一个指向文件的指针
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");//这行代码的作用是打开一个文件，如果成功打开文件，则返回一个指向该文件的指针，
    //如果打开文件失败，则返回 NULL。在后续的代码中，你可以使用 resource 这个文件指针来进行读取文件内容的操作。
    //记得在不再需要文件时，要使用 fclose(resource) 关闭文件，以释放系统资源。

    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);//把 HTTP 响应的头部写到套接字发送到客户端
        cat(client, resource);// 读取服务器上某个文件写到 socket 套接字发送到客户端
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);//创建一个套接字的调用
    /*
    socket: 这是一个系统调用（函数），用于创建一个新的套接字。它接受三个参数：
    PF_INET: 这是地址族（Address Family）的标识符，表示使用 IPv4 地址族。
    SOCK_STREAM: 这是套接字类型，表示创建一个面向连接的 TCP 套接字，用于可靠的、双向的、基于字节流的通信。
    0: 这是协议的标识符，通常为 0 表示让系统根据前两个参数自动选择合适的协议。在这种情况下，会自动选择 TCP 协议。
    */
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    /*
    将指针 &name 指向的内存区域，也就是 name 的大小范围内的所有内容都设置为 0。

    这种操作通常用于初始化数据结构或者清空某个变量所占用的内存，确保它们处于一个已知的初始状态，
    以免出现未初始化或者脏数据的情况。例如，在使用结构体时，可以通过 memset 将其所有字段初始化为零值。
    */
    name.sin_family = AF_INET;
    /*
    name: 通常是一个 sockaddr_in 类型的变量，用于存储 IPv4 地址信息。
    sin_family 是该结构体中的一个字段，表示地址族。

    AF_INET: 是一个常量，代表 IPv4 地址族。
    */
    name.sin_port = htons(*port);
    /*
    name: 这是一个 sockaddr_in 类型的结构体变量，用于存储 IP 地址和端口信息。

    sin_port: 是 sockaddr_in 结构体中用于存储端口号的字段。

    htons(): 是一个函数，用于将一个16位主机字节序的整数转换为网络字节序（大端字节序）。
    在网络编程中，端口号需要以网络字节序的形式传输。

    *port: 这里假设 port 是一个指向端口号的指针，通过 *port 取得指针所指向的端口号的值。

    这行代码的作用是将指针 port 所指向的端口号转换为网络字节序，然后赋值给 name 结构体的 sin_port 字段，
    以便在套接字操作中使用。这样做是为了确保在网络传输中正确地表示端口号。
    */
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    /*
    name: 这是一个 sockaddr_in 类型的结构体变量，用于存储 IP 地址和端口信息。

   sin_addr: 是 sockaddr_in 结构体中用于存储 IP 地址的字段。

   s_addr: 是 sockaddr_in 结构体中的一个子字段，用于存储 32 位的 IP 地址。

   htonl(): 是一个函数，用于将一个32位主机字节序的整数转换为网络字节序（大端字节序）。
   在网络编程中，IP 地址需要以网络字节序的形式传输。

   INADDR_ANY: 是一个常量，表示任意 IP 地址，通常用于服务器端绑定到所有网络接口上。

   这行代码的作用是将 INADDR_ANY 常量表示的任意 IP 地址转换为网络字节序，
   然后赋值给 name 结构体的 sin_addr.s_addr 字段，以便在套接字操作中使用。
   这样做是为了告诉套接字在哪个 IP 地址上监听连接，而 INADDR_ANY 表示在所有可用的网络接口上监听
    */
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
    /*
    setsockopt 是一个用于设置套接字选项的函数。在这个语句中，各个参数的含义如下：

    httpd: 这是一个套接字描述符，用于指定要对哪个套接字进行操作。

    SOL_SOCKET: 这是一个常量，表示在套接字级别设置选项。这个常量用于指定套接字的通用选项。

    SO_REUSEADDR: 这是一个套接字选项，允许在套接字绑定时重新使用地址。
    启用这个选项的作用是在套接字关闭后，即使本地端口仍处于 TIME_WAIT 状态，仍然可以在该端口上绑定新套接字。
    这在服务程序（如 HTTP 服务器）需要在短时间内重启时非常有用。

    &on: 这是一个指向整型变量的指针，用于传递选项的值。在这里，on 表示启用 SO_REUSEADDR 选项。

    sizeof(on): 表示选项值的大小。在这个案例中，由于 on 是整型变量，因此其大小是 sizeof(int)。

    这行代码的作用是设置指定套接字 httpd 使其允许地址重用。
    启用这个选项可以帮助避免因端口仍处于 TIME_WAIT 状态而导致的绑定失败，从而使服务器能够快速重启，
    改善服务的可用性和部署灵活性。
    */  
    {  
        error_die("setsockopt failed");
    }
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    /*
    这行代码使用 bind 函数将套接字 httpd 绑定到指定的网络地址结构 name，并指定了 name 的大小。

    具体而言，参数说明如下：

    httpd: 这是要绑定的套接字描述符。

    (struct sockaddr *)&name: 这是一个指向要绑定的地址结构的指针，通常是 struct sockaddr 类型的。
    地址结构包含了套接字要绑定到的 IP 地址和端口号等信息。
    需要注意的是，因为 bind 函数的参数类型是 struct sockaddr *，所以在传递时需要进行类型转换。

    sizeof(name): 这是指定了地址结构的大小，以确保 bind 函数正确地读取了地址结构的信息。

    bind 函数的作用是将一个本地地址（IP 地址和端口号）绑定到套接字上，从而使套接字与指定的地址相关联。
    在网络编程中，通常在服务器端使用 bind 函数来指定服务器的 IP 地址和端口号，以便监听客户端的连接请求。
    */
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port 
    如果端口号为0，则 bind 函数将自动分配一个未被使用的端口。
    这在服务器端的监听套接字中非常有用，因为它允许操作系统为服务器选择一个可用的端口，从而避免端口冲突问题。*/
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
        /*
        getsockname 函数获取与套接字 httpd 关联的本地地址信息，并将该信息存储在指定的地址结构 name 中。

        具体而言，参数说明如下：

        httpd: 这是要获取本地地址信息的套接字描述符。

        (struct sockaddr *)&name: 这是一个指向用于存储地址信息的地址结构的指针。
        通常是 struct sockaddr 类型的。需要注意的是，因为 getsockname 函数的参数类型是 struct sockaddr *，
        所以在传递时需要进行类型转换。

        &namelen: 这是一个指向存储地址结构大小的变量的指针。
        在调用 getsockname 函数之前，你需要将这个变量设置为地址结构的大小，以便 getsockname 函数可以正确地填充地址信息。调用后，这个变量会被更新为实际存储的地址结构的大小。

        getsockname 函数用于获取与指定套接字关联的本地地址信息，这对于在服务器端获取服务器的 IP 地址和端口号非常有用。
        通常在服务器创建套接字并绑定到地址后，可以使用 getsockname 函数来获取绑定的实际地址信息。
        */
            error_die("getsockname");
        *port = ntohs(name.sin_port);
        /*
        sockaddr_in 结构体中的端口号从网络字节序（Big-Endian）转换为主机字节序（Host-Endian）。

        具体来说：
        name.sin_port：这是 sockaddr_in 结构体中存储的端口号，它以网络字节序的形式存储，即 Big-Endian。

        ntohs() 函数：这是一个网络字节序转换函数，用于将 16 位无符号整数从网络字节序转换为主机字节序。
        在这里，ntohs() 函数将 name.sin_port 中存储的端口号从网络字节序转换为主机字节序。

        *port：这是一个指向要存储端口号的变量的指针。通过将转换后的端口号存储在 *port 变量中，
        可以在主机程序中方便地使用端口号。

        总之，这行代码的作用是将 sockaddr_in 结构体中存储的端口号从网络字节序转换为主机字节序，
        并将结果存储在指定的变量中。
        */
    }
    if (listen(httpd, 5) < 0)
    /*listen() 函数来设置套接字 httpd 监听连接请求，并指定了待处理连接队列的长度为 5。
    如果 listen() 函数执行失败，它会返回一个负数，表示出现了错误。*/
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;//当创建套接字失败时，server_sock 保持为 -1，表示套接字创建失败
    u_short port = 8080;//u_short 是无符号短整型数据类型的简写，通常用于存储 16 位无符号整数，
    //范围从 0 到 65,535。这个数据类型常常用于网络编程中的端口号，因为端口号在 TCP/IP 协议中也是一个 16 位无符号整数。

    int client_sock = -1;//当创建套接字失败时，client_sock 保持为 -1，表示套接字创建失败
    struct sockaddr_in client_name; //struct sockaddr_in 是用于存储 IPv4 地址信息的结构体类型，
                                   //定义在 <netinet/in.h> 头文件中
    socklen_t  client_name_len = sizeof(client_name);//用于存储客户端地址结构体的大小
    pthread_t newthread;//这是一种用于标识 POSIX 线程的类型。
                         //在 POSIX 标准中，线程是通过 pthread_t 类型进行引用和管理的

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        /*
        accept() 函数接受客户端的连接请求，并创建一个新的套接字 client_sock 用于与客户端进行通信。

        具体来说：

        server_sock：这是服务器套接字，用于监听连接请求的套接字描述符。

        (struct sockaddr *)&client_name：这是一个指向 sockaddr 结构体的指针，用于存储客户端的地址信息。
        accept() 函数会将客户端的地址信息填充到这个结构体中。

        &client_name_len：这是一个指向 socklen_t 类型的变量的指针，用于指定 client_name 结构体的长度。
        在调用 accept() 函数之前，需要将 client_name_len 设置为 client_name 结构体的大小，
        以便 accept() 函数可以正确地填充客户端的地址信息。

        如果 accept() 函数执行成功，它会返回一个新的套接字描述符 client_sock，通过这个套接字可以与客户端进行通信。
        如果 accept() 函数执行失败，它会返回一个负数，表示出现了错误。
        */
        if (client_sock == -1)
            error_die("accept");
        accept_request(&client_sock);
        if (pthread_create(&newthread , NULL, accept_request, (void *)&client_sock) != 0)
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}
