#include <string>

#include "webserver.h"
#include "config/config.h"

int main(int argc, char *argv[]) {

  string sql_database_name = "mydb";
  string user = "rockstar";
  string passwd = "123456";


  Config cfg;
  // cfg.parse_arg(argc, argv);

  WebServer server;
  server.init(cfg.PORT, sql_database_name, user, passwd, cfg.sql_num, cfg.LOGWrite, cfg.close_log, cfg.actor_model, cfg.thread_num, cfg.OPT_LINGER,
              cfg.TRIGMode, cfg.LISTENTrigmode, cfg.CONNTrigmode);


  // 先初始化数据库连接池, http服务连接池需要用到数据库连接池
  server.sql_pool();

  server.thread_pool();

  server.log_write();

  server.event_listen();

  server.event_loop();

  return 0;
}