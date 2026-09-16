#pragma once

#include <string>
#include <memory>
#include <vector>

#include <mysql_connection.h>
#include <mysql_driver.h>

class StorageCrypto;


struct user
{
    int id;
    std::string username;
    std::string normalized;
};

class Database
{
private:
    sql::mysql::MySQL_Driver *driver;
    std::unique_ptr<sql::Connection> con;
    std::unique_ptr<StorageCrypto> crypto;

public:
    Database();
    ~Database();

    bool connect(const std::string &host, const std::string &user, const std::string &password, const std::string &database);

    void disconnect();

    bool InsertUser(const std::string &username, const std::string &normalized);

    std::vector<user> GetUsers();
};
