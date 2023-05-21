#include "sql_connection_pool.h"

// 从数据库连接池中返回可用连接
MYSQL *connection_pool::GetConnection() {
  MYSQL *conn = nullptr;
  if (connList.size() == 0)
    return nullptr;

  // 信号量减一
  reserve.wait();

  lock.lock();
  conn = connList.front();
  connList.pop_front();

  m_freeConn--;
  m_curConn++;
  lock.unlock();

  return conn;
}

// 释放连接
bool connection_pool::ReleaseConnection(MYSQL *conn) {
  if (conn == nullptr)
    return false;

  lock.lock();
  connList.push_back(conn);

  m_freeConn++;
  m_curConn--;

  lock.unlock();

  reserve.post();  // 通知可用
  return conn;
}

int connection_pool::GetFreeConn() { return m_freeConn; }

// 释放连接池
void connection_pool::DestoryPool() {
  lock.lock();
  if (connList.size() > 0) {
    list<MYSQL *>::iterator it;
    for (it = connList.begin(); it != connList.end(); it++) {
      MYSQL *conn = *it;
      mysql_close(conn);
    }
    m_curConn = 0;
    m_freeConn = 0;

    connList.clear();
    lock.unlock();
  } else {
    lock.unlock();
  }
}

// 单例模式
connection_pool *connection_pool::GetInstance() {
  static connection_pool connPool;
  return &connPool;
}

void connection_pool::init(string url, string user, string passwd, string databasename, int port, int maxConn, int close_log) {
  this->m_url = url;
  this->m_user = user;
  this->m_passwd = passwd;
  this->m_database = databasename;
  this->m_port = to_string(port);
  this->m_MaxConn = maxConn;
  this->m_close_log = close_log;

  for (int i = 0; i < m_MaxConn; i++) {
    MYSQL *conn = nullptr;
    conn = mysql_init(conn);
    if (!conn) {
      cerr << "error mysql_init() in connection_pool::init()\n" << mysql_error(conn);
      exit(1);
    }
    conn = mysql_real_connect(conn, m_url.c_str(), m_user.c_str(), m_passwd.c_str(), m_database.c_str(), port, NULL, 0);
    if (!conn) {
      cerr << "error mysql_real_connect() in connection_pool::init()" << mysql_error(conn);
      exit(1);
    }
    connList.push_back(conn);
    m_freeConn++;
  }

  reserve = m_freeConn;  // 信号量初始化为最大连接次数
  this->m_MaxConn = m_freeConn;
}

connection_pool::connection_pool() {
  this->m_curConn = 0;
  this->m_freeConn = 0;
}

connection_pool::~connection_pool() { DestoryPool(); }

connectionRAII::connectionRAII(MYSQL **Sqlconn, connection_pool *conn_pool) {
  *Sqlconn = conn_pool->GetConnection();
  connRAII = *Sqlconn;

  connRAII_pool = conn_pool;
}

connectionRAII::~connectionRAII() { connRAII_pool->ReleaseConnection(connRAII); }

// TODO: remove this test code
// int main() {
//   MYSQL *con = nullptr;
//   connection_pool *connpool = connection_pool::GetInstance();
//   connectionRAII obj(&con, connpool);
//   return 0;
// }