#ifndef _HTTP_CONN_H
#define _HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../utils/log.h"


extern int  setnonblocking(int fd);
extern void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
extern void removefd(int epollfd, int fd);
extern void modfd(int epollfd, int fd, int ev, int TRIGMode);


using namespace std;

class http_conn {
public:
  static const int FILENAME_LEN = 200;        // 读取文件名的名称m_real_file大小
  static const int READ_BUFFER_SIZE = 2048;   // 设置缓冲区m_read_buf大小
  static const int WRIET_BUFFER_SIZE = 1024;  //  设置缓冲区m_write_buf大小

  // 主状态机状态
  enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0, /* 正在分析请求行 */
    CHECK_STATE_HEADER,          /* 正在分析头部字段 */
    CHECK_STATE_CONTENT
  };
  // 从状态机状态
  enum LINE_STATUS {
    LINE_OK = 0, /* 读到完整的行 */
    LINE_BAD,    /* 行出错 */
    LINE_OPEN    /* 行数据尚不完整 */
  };
  // HTTP请求结果
  enum HTTP_CODE {
    NO_REQUEST,        /* 请求不完整, 需要继续读取客户数据 */
    GET_REQUEST,       /* 获得了一个完整的客户请求 */
    BAD_REQUEST,       /* 请求有语法错误 */
    INTERNAL_ERROR,    /* 服务器内部错误 */
    NO_RESOURCE,
    FORBIDDEN_REQUEST, /* 对客户请求的资源没有足够的访问权限 */
    FILE_REQUEST,
    CLOSED_CONNECTION  /* 客户已关闭连接 */
  };
  // 请求方法
  enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH };

public:
  http_conn();
  ~http_conn();

public:
  void         init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, int close_log, string user, string passwd,
                    string sqlname);                         // 初始化套接字, 内部调用私有init()
  void         close_conn(bool real_close = true);           // 关闭http连接
  void         process();
  bool         read_once();                                  // 读取浏览器发送的全部数据
  bool         write();                                      // 响应报文写入
  sockaddr_in *get_address();
  void         initmysql_result(connection_pool *connPool);  // 同步线程初始化数据库读取表

private:
  void      init();
  HTTP_CODE process_read();                // 从m_read_buf读取, 处理请求报文
  bool      process_write(HTTP_CODE ret);  // 向m_write_buf写入响应报文

  /* 被process_read()调用, 从状态机 */
  LINE_STATUS parse_line();  // 从状态机读取一行
  HTTP_CODE   parse_request_line(char *text);
  HTTP_CODE   parse_headers(char *text);
  HTTP_CODE   parse_content(char *text);
  HTTP_CODE   do_request();  // 得到完整http请求, 分析目标文件
  /// @brief m_start_line是已经解析的字符, get_line用于将指针向后偏移，指向未处理的字符
  char *get_line();

  /* 被process_write()调用 */
  void unmap();
  bool add_response(const char *format, ...);
  bool add_content(const char *content);
  bool add_status_line(int status, const char *title);
  bool add_headers(int content_length);
  bool add_content_type();
  bool add_content_length(int content_length);
  bool add_linger();
  bool add_blank_line();

public:
  static int m_epollfd;     // 所有socket都注册到同一个epoll内核事件表中, 所有文件符设置为静态
  static int m_user_count;  // 用户数量
  MYSQL     *mysql;         // 该次连接建立的sql连接
  int        timer_flag;
  int        improv;
  int        m_state;  // 当前http连接所处的状态读为0, 写为1

private:
  int         m_sockfd;   // 当前http使用的socket
  sockaddr_in m_address;  // 对方的地址

  /* 请求报文 */
  char m_read_buf[READ_BUFFER_SIZE];  // 存储读取的浏览器发送的请求报文数据
  int  m_read_idx;                    // 读缓冲区中已经读入的数据的最后一个字节的下一个位置
  int  m_checked_idx;                 // m_read_buf中已经解析的字符个数
  int  m_start_line;                  // 当前正在解析的行的起始位置

  /* 响应报文 */
  char m_write_buf[WRIET_BUFFER_SIZE];  // 存储发出的响应报文的数据
  int  m_write_idx;                     // m_write_buff中的长度

  /* 主状态机的状态 */
  CHECK_STATE m_check_state;
  METHOD      m_method;

  /* 解析请求报文(header内容和request-line内容) */
  char  m_real_file[FILENAME_LEN];  // 请求的目标文件的完整路径: doc_root + m_url
  char *m_url;                      // 请求的目标文件的文件名
  char *m_version;                  // Http版本号
  char *m_host;                     // 主机名
  int   m_content_length;           // http请求的消息的长度
  bool  m_linger;                   // 请求是否要保持连接

  char        *m_file_address;      // 服务器上文件位置, 请求的目标文件被mmap到内存中的起始位置
  struct stat  m_file_stat;         // 目标文件的状态
  struct iovec m_iv[2];             // io向量机制iovec, 使用writev执行写操作
  int          m_iv_count;
  int          cgi;                 // 启用post
  char        *m_string;            // 存储请求头
  size_t       bytes_to_send;    // 发送的全部数据的大小(响应头部和文件大小), 动态变量, 也用于记录还有多少需要发送
  size_t       bytes_have_send;  // 已经发送的字节数

  // 该次http连接的属性
  map<string, string> m_users;   // TODO: not useful, use global value, may cause thread unsafe
  char               *doc_root;  // 网站根目录, 文件夹内存放所有html文件, 如: /home/user_name/github/ini_tinywebserver/root
  int                 m_TRIGMode;
  int                 m_close_log;

  char sql_user[100];
  char sql_passwd[100];
  char sql_name[100];
};

#endif
