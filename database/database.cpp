#include "database.hpp"
#include "storage_crypto.hpp"
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cstdlib>

Database::Database()
{
    driver = nullptr;
}

Database::~Database() = default;

bool Database::connect(const std::string &host, const std::string &user, const std::string &password, const std::string &database)
{
    try
    {
        const char *key = std::getenv("SM_OSINT_ENCRYPTION_KEY");
        if (key == nullptr)
        {
            std::cerr << "SM_OSINT_ENCRYPTION_KEY is required to access encrypted username data\n";
            return false;
        }
        crypto = std::make_unique<StorageCrypto>(key);
        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(
            driver->connect(host, user, password));
        con->setSchema(database);

        return true;
    }
    catch (sql::SQLException &e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Database encryption setup failed: " << e.what() << '\n';
        crypto.reset();
        return false;
    }
}

void Database::disconnect()
{
    con.reset();
    crypto.reset();
}

bool Database::InsertUser(const std::string &username, const std::string &normalized)
{
    try
    {
        if (!con || !crypto)
            return false;
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO usernames (username_ciphertext,normalized_ciphertext,normalized_lookup)"
                "VALUES (?,?,?)"));

        stmt->setString(1, crypto->encrypt(username));
        stmt->setString(2, crypto->encrypt(normalized));
        stmt->setString(3, crypto->lookupDigest(normalized));

        stmt->execute();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "InsertUser failed: " << e.what() << '\n';

        return false;
    }
}

std::vector<user> Database::GetUsers()
{
    try
    {
        std::vector<user> users;

        if (!con || !crypto)
            return {};
        std::unique_ptr<sql::Statement> stmt(
            con->createStatement());

        std::unique_ptr<sql::ResultSet> res(
            stmt->executeQuery(
                "SELECT id, username_ciphertext, normalized_ciphertext FROM usernames"));

        while (res->next())
        {
            user cur_user;

            cur_user.id = res->getInt64("id");
            cur_user.username = crypto->decrypt(res->getString("username_ciphertext"));
            cur_user.normalized = crypto->decrypt(res->getString("normalized_ciphertext"));

            users.push_back(cur_user);
        }

        return users;
    }
    catch (const std::exception &e)
    {
        std::cerr << "GetUsers failed: " << e.what() << '\n';

        return {};
    }
}
