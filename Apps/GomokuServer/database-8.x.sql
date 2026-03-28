create database Gomoku;
use Gomoku;
CREATE TABLE users (
    id INT(11) NOT NULL AUTO_INCREMENT,
    username VARCHAR(255) NOT NULL,
    password VARCHAR(255) NOT NULL,
    PRIMARY KEY (id)
);
UPDATE mysql.user SET host = '%' WHERE user = 'root' AND host = 'localhost';
-- 先创建/修改用户
ALTER USER 'root'@'%' IDENTIFIED BY 'root';
-- 再授权
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;