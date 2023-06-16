// 建立库
// ensure the server is running 
// $(shell)>sudo systemctl start mysql.service
// $(shell)> sudo mysql -u root -p 
// root 密码 123456
// 添加新用户
// $(mysql)> create user 'rockstar'@'localhost' identified by '123456';
create database mydb;

// 创建user表
USE mydb;
CREATE TABLE user(
    username char(50) NULL,
    passwd char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, passwd) VALUES('name', 'passwd');