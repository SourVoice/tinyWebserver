#ifndef _SQL_CONNECTION_POOL_H
#define _SQL_CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <list>

#include "../locker/locker.h"

using namespace std;

class connection_pool {
public:
  friend class connectionRAII;

  MYSQL *GetConnection();                 // 获得数据库连接
  bool   ReleaseConnection(MYSQL *conn);  // 释放连接
  int    GetFreeConn();                   // 获得连接
  void   DestoryPool();                   // 销毁所有连接池

  static connection_pool *GetInstance();

  void init(string url, string user, string passwd, string databasename, int port, int maxConn, int close_log);

private:
  connection_pool();
  ~connection_pool();

  int           m_MaxConn;
  int           m_curConn;   // 当前已用连接数量
  int           m_freeConn;  // 可用连接数量
  locker        lock;
  list<MYSQL *> connList;
  sem           reserve;

public:
  string m_url;
  string m_port;
  string m_user;
  string m_passwd;
  string m_database;
  int    m_close_log;
};

class connectionRAII {
public:
  connectionRAII(MYSQL **Sqlconn, connection_pool *conn_pool);
  ~connectionRAII();

private:
  MYSQL           *connRAII;
  connection_pool *connRAII_pool;
};

#endif
