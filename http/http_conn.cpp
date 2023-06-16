#include "http_conn.h"

// http响应的状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker              m_lock;
map<string, string> users;


http_conn::http_conn() {}

http_conn::~http_conn() {}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, int close_log, string user, string passwd, string sqlname) {
  m_sockfd = sockfd;
  m_address = addr;

  // BUG: DEBUG use, the flowing two lines
  int reuse = 1;
  setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  addfd(m_epollfd, sockfd, true, TRIGMode);

  doc_root = root;
  m_TRIGMode = TRIGMode;
  m_close_log = close_log;

  strcpy(sql_user, user.c_str());
  strcpy(sql_passwd, passwd.c_str());
  strcpy(sql_name, sqlname.c_str());

  m_user_count++;
  init();
}

// 关闭连接, 客户量减一
void http_conn::close_conn(bool real_close) {
  if (real_close && (m_sockfd != -1)) {
    printf("close %d\n", m_sockfd);
    removefd(m_epollfd, m_sockfd);
    m_sockfd = -1;
    m_user_count--;
  }
}

void http_conn::process() {
  http_conn::HTTP_CODE read_ret_state = process_read();
  if (read_ret_state == HTTP_CODE::NO_REQUEST) {
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    return;
  }

  bool write_ret_stete = process_write(read_ret_state);
  if (write_ret_stete == false) {
    close_conn(true);
  }
  modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

// 读到无数据可读或对方关闭连接
bool http_conn::read_once() {
  if (m_read_idx >= READ_BUFFER_SIZE)
    return false;

  int bytes_read = 0;
  // LT模式
  if (m_TRIGMode == 0) {
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    if (bytes_read <= 0) {
      return false;
    }
    m_read_idx += bytes_read;
    return true;
  }
  // 非阻塞ET模式, 需要一次性将数据读完
  else if (m_TRIGMode == 1) {
    while (true) {
      // 从m_sockfd接收数据, 存储在m_read_buf缓冲区
      bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
      if (bytes_read == -1) {  // recv() error
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          break;
        return false;
      } else if (bytes_read == 0) {  // Connection closed
        return false;
      }
      // 更新已经读取的字节数
      m_read_idx += bytes_read;
    }
  }
  return true;
}

// http_conn::write()使用m_iv写入套接字
bool http_conn::write() {

  int offset = 0;

  if (bytes_to_send == 0) {
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    init();
    return true;
  }

  while (1) {
    int len = writev(m_sockfd, m_iv, m_iv_count);
    if (len > 0) {
      bytes_have_send += len;
      offset = bytes_have_send - m_write_idx;  // 偏移ivoec pointer
    }

    if (len <= -1) {
      // 发送缓冲区已满, 等待下一轮
      if (errno == EAGAIN) {
        if (bytes_have_send >= m_iv[0].iov_len)  // 第一个iovec响应报文头部数据
        {
          m_iv[0].iov_len = 0;
          m_iv[1].iov_base = m_file_address + offset;
          m_iv[1].iov_len = bytes_to_send;
        } else {  // 继续发送头部信息
          m_iv[0].iov_base = m_write_buf + bytes_to_send;
          m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
        return true;
      } else {  // 发送失败
        unmap();
        return false;
      }
    }

    // 更新已发送的字节数
    bytes_to_send -= len;

    // 已经全部发送完毕
    if (bytes_to_send <= 0) {
      unmap();
      modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
      if (m_linger) {
        init();
        return true;
      } else {
        return false;
      }
    }
  }

  return false;
}

sockaddr_in *http_conn::get_address() { return &m_address; }

// 初始化数据库连接(用于本次http连接)用户名和密码
void http_conn::initmysql_result(connection_pool *connPool) {
  // 从连接池获得连接
  mysql = nullptr;
  connectionRAII connRAII(&mysql, connPool);

  string query_exp = "SELECT username, passwd FROM user";
  // if (mysql_query(mysql, query_exp.c_str()) == 0)
  //   LOG_ERROR("SELECT error: %s\n", mysql_error(mysql));
  if (mysql_query(mysql, query_exp.c_str()) != 0)
    LOG_ERROR("SELECT error: %s\n", mysql_error(mysql));

  MYSQL_RES *ret = mysql_store_result(mysql);

  int num_fileds = mysql_num_fields(ret);
  if (num_fileds < 0) {
    LOG_ERROR("file:%s line:%d mysql_num_fields() error: %s\n", __FILE__, __LINE__, mysql_error(mysql));
    assert(num_fileds < 0);
  }

  MYSQL_FIELD *fileds = mysql_fetch_fields(ret);
  if (fileds == nullptr) {
    LOG_ERROR("file:%s line:%d mysql_fetch_fields() error: %s\n", __FILE__, __LINE__, mysql_error(mysql));
    assert(num_fileds < 0);
  }

  while (MYSQL_ROW row = mysql_fetch_row(ret))
    users[row[0]] = row[1];
}

void http_conn::init() {
  // public
  mysql = nullptr;
  timer_flag = 0;
  improv = 0;
  m_state = 0;

  // private
  memset(m_read_buf, '\0', READ_BUFFER_SIZE);
  m_read_idx = 0;
  m_checked_idx = 0;
  m_start_line = 0;

  memset(m_write_buf, '\0', WRIET_BUFFER_SIZE);
  m_write_idx = 0;

  m_check_state = CHECK_STATE_REQUESTLINE;
  m_method = GET;

  memset(m_real_file, '\0', FILENAME_LEN);
  m_url = 0;
  m_version = 0;
  m_host = 0;
  m_content_length = 0;
  m_linger = false;

  m_file_address = 0;
  m_iv_count = 0;
  cgi = 0;
  bytes_to_send = 0;
  bytes_have_send = 0;
}

http_conn::HTTP_CODE http_conn::process_read() {
  LINE_STATUS line_status = LINE_OK;
  HTTP_CODE   ret = BAD_REQUEST;
  char       *text;

  auto legal_stete = [&line_status, &ret, this]() -> bool {
    bool content = this->m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK;
    // CONTENT解析时, 不使用parse_line函数
    if (content)
      return true;
    bool second = (line_status = parse_line()) == LINE_OK;
    return content || second;
  };

  while (legal_stete() == true) {
    text = get_line();
    m_start_line = m_checked_idx;
    LOG_INFO("got 1 http line %s", text);

    switch (m_check_state) {

    case CHECK_STATE_REQUESTLINE: {  // 解析http请求行
      ret = parse_request_line(text);
      if (ret == BAD_REQUEST)
        return BAD_REQUEST;
      break;
    }

    case CHECK_STATE_HEADER: {
      ret = parse_headers(text);
      if (ret == BAD_REQUEST)
        return BAD_REQUEST;
      if (ret == GET_REQUEST)  // 处理完整请求
        return do_request();
      break;
    }

    case CHECK_STATE_CONTENT: {
      ret = parse_content(text);
      if (ret == GET_REQUEST)  // 处理完整请求
        return do_request();
      line_status = LINE_OPEN;  // 解析完消息体后, 完成报文全部解析, 此时仍位于CHECK_STATE_CONTENT, 需要跳出循环, 将line_status改为LINE_OPEN
      break;
    }

    default: {
      return INTERNAL_ERROR;
      break;
    }
    };
  }
  return NO_REQUEST;
}

bool http_conn::process_write(HTTP_CODE ret) {
  switch (ret) {

  case BAD_REQUEST: {
    add_status_line(400, error_400_title);
    add_headers(strlen(error_400_form));
    if (!add_content(error_400_form))
      return false;
    break;
  }

  case NO_RESOURCE: {
    add_status_line(404, error_404_title);
    add_headers(strlen(error_404_form));
    if (!add_content(error_404_form))
      return false;
    break;
  }

  case FORBIDDEN_REQUEST: {
    add_status_line(403, error_403_title);
    add_headers(strlen(error_403_form));
    if (!add_content(error_403_form))
      return false;
    break;
  }

  case FILE_REQUEST: {
    add_status_line(200, ok_200_title);
    if (m_file_stat.st_size != 0) {
      add_headers(m_file_stat.st_size);
      m_iv[0].iov_base = m_write_buf;     // iovec[0]指向响应报文的缓冲区, 长度为m_write_idx;
      m_iv[0].iov_len = m_write_idx;
      m_iv[1].iov_base = m_file_address;  // iovec[1]指向mmap返回的文件指针
      m_iv[1].iov_len = m_file_stat.st_size;

      bytes_to_send = m_write_idx + m_file_stat.st_size;
      m_iv_count = 2;
      return true;
    } else {  // 请求的资源为0
      const char *ok_string = "<html><body></body></html>";
      add_headers(strlen(ok_string));
      if (!add_content(ok_string))
        return false;
    }
    break;
  }

  case INTERNAL_ERROR: {
    add_status_line(500, error_500_title);
    add_headers(strlen(error_500_form));
    if (!add_content(error_500_form))
      return false;
    break;
  }

  default:
    return false;
  }

  // 其余主状态机状态只申请一个iovec, 为响应报文缓冲区
  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  bytes_to_send = m_write_idx;

  return true;
}

// 从状态机，用于分析出一行内容
http_conn::LINE_STATUS http_conn::parse_line() {

  char tmp;
  for (; m_checked_idx < m_read_idx; m_checked_idx++) {
    tmp = m_read_buf[m_checked_idx];
    if (tmp == '\r') {                      // 可能读取到完整的行
      if (m_checked_idx + 1 == m_read_idx)  // 下一个字节到达buffer结尾, 接收不完整, 需要继续读
        return LINE_OPEN;
      else if (m_read_buf[m_checked_idx + 1] == '\n') {  // 从状态机已经将每一行的末尾\r\n符号改为\0\0，以便于主状态机直接取出对应字符串进行处理。
        m_read_buf[m_checked_idx++] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    // 读到\n, 也可能读取到完整行, \n可能是上一次接收到\r后不完整
    else if (tmp == '\n') {
      // 前一个字符是\r，则接收完整
      if ((m_checked_idx > 1) && m_read_buf[m_checked_idx - 1] == '\r') {
        m_read_buf[m_checked_idx - 1] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  // 未找到\r\n继续接收
  return LINE_OPEN;
}

// 解析http请求行, 获得请求方法, 目标url和http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {

  /* 切片请求方法 */
  m_url = strpbrk(text, " \t");  // finds the first location of any character in one string, in another string
  if (!m_url)
    return BAD_REQUEST;
  // 将该位置改为\0，用于将前面数据取出(方便切片)
  *m_url++ = '\0';
  char *method = text;
  if (strcasecmp(method, "GET") == 0)
    m_method = GET;
  else if (strcasecmp(method, "POST") == 0) {
    m_method = POST;
    cgi = 1;
  } else
    return BAD_REQUEST;
  // 跳过紧跟着的所有的空格和\t
  m_url += strspn(m_url, " \t");

  /* 切片version */
  char *version = m_url;
  version = strpbrk(m_url, " \t");
  if (!version)
    return BAD_REQUEST;
  *version++ = '\0';
  version += strspn(version, " \t");
  // 仅支持http/1.1
  if (strcasecmp(version, "HTTP/1.1") != 0)
    return BAD_REQUEST;
  else {
    m_version = new char[200];
    memcpy(m_version, version, strlen(version));
  }

  /* 切片url */
  // 例如: GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
  if (strncasecmp(m_url, "http://", 7) == 0) {
    m_url += 7;
    m_url = strchr(m_url, '/');
  }
  if (strncasecmp(m_url, "https://", 8) == 0) {
    m_url += 8;
    m_url = strchr(m_url, '/');
  }
  // 例如: GET /pub/WWW/TheProject.html HTTP/1.1
  if (!m_url || m_url[0] != '/')
    return BAD_REQUEST;

  // 请求为 "/"
  if (strlen(m_url) == 1)
    strcat(m_url, "judge.html");

  m_check_state = CHECK_STATE_HEADER;  // 主状态机转移至处理请求头
  return NO_REQUEST;
}

// 解析http请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
  // 空行, 解析完毕
  if (text[0] == '\0') {
    if (m_content_length != 0) {  // http有消息体
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST;
  }
  // 处理Connection字段, 获得m_linger
  else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0)  // 长连接设置linger
      m_linger = true;
  }
  // 处理Content-length, 获得m_content_length, POST请求的内容
  else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");
    m_content_length = atol(text);
  }
  // 处理Host, 获得m_host`
  else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    m_host = text;
  } else {
    LOG_ERROR("oop ! unknow header %s \n", text);
  }
  return NO_REQUEST;
}

// 专用于POST请求, 处理消息体
http_conn::HTTP_CODE http_conn::parse_content(char *text) {
  // 检测buffer是否读入了消息体
  if (m_read_idx >= (m_content_length + m_checked_idx)) {
    text[m_content_length] = '\0';
    m_string = text;  // POST中该部分为密码
    return GET_REQUEST;
  }

  return NO_REQUEST;
}

char *http_conn::get_line() { return m_read_buf + m_start_line; }

// 采用内存映射, 将文件映射至m_file_address处
// TODO: 分析请求的url
http_conn::HTTP_CODE http_conn::do_request() {
  strcpy(m_real_file, doc_root);
  int   len = strlen(doc_root);  // 网站根目录长度
  char *p = strrchr(m_url, '/');
  LOG_INFO("request for m_url: %s\n", m_url);

  /* 0-注册, 1-登录, 2-CGI登录校验, 3-CGI注册校验
     5-图片请求, 6-视频请求, 7-关注请求
  */
  char request_flag = *(p + 1);

  // 处理post请求(登录请求), CGISQL
  if (cgi == 1 && (request_flag == '2' || request_flag == '3')) {

    // TODO: figure out what this code below do
    char *m_real_url = (char *)malloc(sizeof(char) * 200);
    strcpy(m_real_url, "/");
    strcat(m_real_url, m_url + 2);
    strncpy(m_real_file + len, m_real_url, FILENAME_LEN - len + 1);
    free(m_real_url);

    // 获取密码和用户, m_string = "user=xxx&password=xxx.."
    string user, passwd;
    int    i = 5;
    while (m_string[i] != '&')
      user.push_back(m_string[i++]);
    i += 10;
    while (m_string[i] != '\0')
      passwd.push_back(m_string[i++]);

    // TODO: 注册校验
    if (request_flag == '3') {
      string query = "INSERT INTO user(username, passwd) VALUES('" + user + "', '" + passwd + "')";

      // FIXME: if ret error, but we still have user and passwd, so we just go to th 549, this is error, need to test
      if (users.find(user) == users.end()) {  // 表中没有重复的用户名
        m_lock.lock();
        int ret = mysql_query(mysql, query.c_str());

        if (!ret) {  // 执行成功返回0
          users.insert({user, passwd});
          strcpy(m_url, "/log.html");
          m_lock.unlock();
        } else {
          strcpy(m_url, "/registerError.html");
          m_lock.unlock();
        }
      } else {  // 表中有重复的用户名
        strcpy(m_url, "/registerError.html");
      }
    }

    // 登录校验
    if (request_flag == '2') {
      // 优先使用users表
      if (users.find(user) != users.end()) {
        if (users[user] == passwd)
          strcpy(m_url, "/welcome.html");
        else
          strcpy(m_url, "/logError.html");
      }

      // 其次使用数据库
      if (users.find(user) == users.end()) {
        string query = "INSERT INTO user(username, passwd) VALUES('" + user + "', '" + passwd + "')";
        m_lock.lock();
        int ret = mysql_query(mysql, query.c_str());

        if (!ret) {  // 执行成功返回0
          users.insert({user, passwd});
          strcpy(m_url, "/welcome.html");
          m_lock.unlock();
        } else {
          strcpy(m_url, "/logError.html");
          m_lock.unlock();
        }
      }
    }
  }

  char *m_url_real = (char *)malloc(sizeof(char) * 200);

  switch (request_flag) {
  case '0': {
    strcpy(m_url_real, "/register.html");
    memcpy(m_real_file + len, m_url_real, strlen(m_url_real));
    break;
  }


  case '1': {
    strcpy(m_url_real, "/log.html");
    memcpy(m_real_file + len, m_url_real, strlen(m_url_real));
    break;
  }

  case '5': {
    strcpy(m_url_real, "/picture.html");
    memcpy(m_real_file + len, m_url_real, strlen(m_url_real));
    break;
  }

  case '6': {
    strcpy(m_url_real, "/video.html");
    memcpy(m_real_file + len, m_url_real, strlen(m_url_real));
    break;
  }

  case '7': {
    strcpy(m_url_real, "/fan.html");
    memcpy(m_real_file + len, m_url_real, strlen(m_url_real));
    break;
  }

  default:
    // 请求其他资源, 直接将m_url附加到后面
    memcpy(m_real_file + len, m_url, strlen(m_url));
    break;
  }
  free(m_url_real);

  // 访问不存在
  if (stat(m_real_file, &m_file_stat) < 0)
    return NO_RESOURCE;
  // 无权限
  if (!(m_file_stat.st_mode & S_IROTH))
    return FORBIDDEN_REQUEST;
  // 非文件
  if (S_ISDIR(m_file_stat.st_mode))
    return BAD_REQUEST;

  // 创建匿名映射
  int fd = open(m_real_file, O_RDONLY);
  m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return FILE_REQUEST;
}

void http_conn::unmap() {
  if (m_file_address) {
    munmap(m_file_address, m_file_stat.st_size);
    m_file_address = 0;
  }
}

// 向缓冲区写入待发送数据
bool http_conn::add_response(const char *format, ...) {
  if (m_write_idx >= WRIET_BUFFER_SIZE)
    return false;

  va_list arg_list;
  va_start(arg_list, format);
  int len = vsnprintf(m_write_buf + m_write_idx, WRIET_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

  if (len >= (WRIET_BUFFER_SIZE - 1 - m_write_idx))
    return false;

  m_write_idx += len;
  va_end(arg_list);

  LOG_INFO("request:%s", m_write_buf);
  return true;
}

bool http_conn::add_content(const char *content) { return add_response("%s", content); }

bool http_conn::add_status_line(int status, const char *title) { return add_response("%s %d %s\r\n", "HTTP/1.1", status, title); }

bool http_conn::add_headers(int content_length) { return add_content_length(content_length) && add_linger() && add_blank_line(); }

bool http_conn::add_content_type() { return add_response("Content-Type:%s\r\n", "text/html"); }

bool http_conn::add_content_length(int content_length) { return add_response("Content-Length: %d\r\n", content_length); }

bool http_conn::add_linger() { return add_response("Connection: %s\r\n", m_linger ? "keep-alive" : "close"); }

bool http_conn::add_blank_line() { return add_response("\r\n"); }

int http_conn::m_epollfd = -1;

int http_conn::m_user_count = 0;