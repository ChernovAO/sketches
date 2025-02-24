-- Creating user and batabse
CREATE USER test_user WITH PASSWORD 'qwe123qwe';
CREATE DATABASE testdb;
GRANT ALL PRIVILEGES ON DATABASE testdb TO test_user;
