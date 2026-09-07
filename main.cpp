#include "database/database.hpp"
#include "src/Trie/Trie.hpp"
#include "src/probalistic system/UsernameGenerator.hpp"
#include "src/probalistic system/UsernameProbability.hpp"
#include "src/string_utils/Normalizer.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
std::string environmentOr(const char *name, const char *fallback = nullptr)
{
    const char *value = std::getenv(name);
    if (value != nullptr && *value != '\0')
        return value;
    return fallback == nullptr ? "" : fallback;
}
int parseLimit(const char *value)
{
    try { return std::max(1, std::stoi(value)); }
    catch (const std::exception &) { return 10; }
}

void printUsage(const char *program)
{
    std::cout << "Usage:\n"
              << "  " << program << " --add <username>\n"
              << "  " << program << " --query <username> [limit]\n"
              << "  " << program << " --stats\n";
}
}

int main(int argc, char *argv[])
{
    const std::string host = environmentOr("SM_OSINT_DB_HOST", "tcp://127.0.0.1:3306");
    const std::string dbUser = environmentOr("SM_OSINT_DB_USER", "osint");
    const std::string password = environmentOr("SM_OSINT_DB_PASSWORD");
    const std::string database = environmentOr("SM_OSINT_DB_NAME", "osint");
    if (password.empty())
    {
        std::cerr << "SM_OSINT_DB_PASSWORD is required\n";
        return 1;
    }

    Database db;
    if (!db.connect(host, dbUser, password, database))
    {
        std::cout << "Connection faild\n";
        return 1;
    }

    if (argc == 3 && std::string(argv[1]) == "--add")
    {
        const std::string normalized = Normalizer::normalize(argv[2]);
        if (normalized.empty())
        {
            std::cerr << "Username has no supported characters\n";
            return 1;
        }
        if (!db.InsertUser(argv[2], normalized))
            return 1;
        std::cout << "Stored encrypted username.\n";
        return 0;
    }

    const std::vector<user> users = db.GetUsers();
    TRIE trie;
    UsernameProbability probability;
    for (const user &record : users)
    {
        trie.insert(record.normalized);
        probability.feed(record.normalized);
    }

    if (argc == 2 && std::string(argv[1]) == "--stats")
    {
        std::cout << "Encrypted records loaded: " << users.size() << '\n';
        std::cout << "Probability samples: " << probability.samples() << '\n';
        return 0;
    }

    if (argc < 3 || std::string(argv[1]) != "--query")
    {
        printUsage(argv[0]);
        return 1;
    }

    const std::string seed = Normalizer::normalize(argv[2]);
    if (seed.empty())
    {
        std::cerr << "Username has no supported characters\n";
        return 1;
    }
    const int limit = argc >= 4 ? parseLimit(argv[3]) : 10;

    std::cout << "Prefix matches for " << seed << ":\n";
    for (const std::string &match : trie.suggest(seed, limit))
        std::cout << "  " << match << '\n';

    UsernameGenerator generator;
    std::cout << "Ranked candidate variations:\n";
    for (const UsernameGenerator::Candidate &candidate : generator.generateRanked(seed, &probability, limit))
        std::cout << "  " << candidate.username << " (" << candidate.score << ")\n";

    return 0;
}
