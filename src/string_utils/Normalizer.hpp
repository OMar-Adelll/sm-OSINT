#ifndef NORMALIZATION_HPP
#define NORMALIZATION_HPP
#include <bits/stdc++.h>

class Normalizer
{
private:
    std::string initial;

    static std::string Normalization(std::string str)
    {
        std::string normalized;
        normalized.reserve(str.size());
        for (unsigned char raw : str)
        {
            const char character = static_cast<char>(std::tolower(raw));
            if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z'))
            {
                normalized.push_back(character);
            }
            else if (character == '.' || character == '-' || character == '_' || character == '/')
            {
                if (!normalized.empty() && normalized.back() != '.')
                    normalized.push_back('.');
            }
        }
        while (!normalized.empty() && normalized.back() == '.')
            normalized.pop_back();
        return normalized;
    }

    void generator(std::string &s, std::string curr, int idx, std::vector<std::string> &res)
    {
        if (static_cast<std::size_t>(idx) == s.size())
        {
            res.push_back(curr);
            return;
        }

        char c = s[idx];
        if (c >= 'a' && c <= 'z')
        {
            generator(s, curr + c, idx + 1, res);
            generator(s, curr + char(c - 'a' + 'A'), idx + 1, res);
        }
        else if (c == '.')
        {
            for (char r : {'.', '-', '_', '/'})
            {
                generator(s, curr + r, idx + 1, res);
            }
        }
        else if (c == '@')
        {
            for (char r : {'!', '@', '#', '$'})
            {
                generator(s, curr + r, idx + 1, res);
            }
        }
        else if (c == '&')
        {
            for (char r : {'%', '^', '&', '*'})
            {
                generator(s, curr + r, idx + 1, res);
            }
        }
        else
        {
            generator(s, curr + c, idx + 1, res);
        }
    }

    std::vector<std::string> deNormalization(std::string str)
    {
        std::vector<std::string> res;
        generator(str, "", 0, res);
        return res;
    }

public:
    Normalizer() = default;

    Normalizer(std::string initial)
    {
        this->initial = initial;
    };

    // return the normalized string
    std::string Normalized()
    {
        return Normalization(initial);
    }

    static std::string normalize(const std::string &value)
    {
        return Normalization(value);
    }

    // return all possible denormalized strings
    std::vector<std::string> deNormalized()
    {
        return deNormalization(initial);
    }

    ~Normalizer() {}
};

#endif
