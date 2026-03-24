#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

inline std::string TrimCopy(const std::string& text) {
    std::size_t start = 0;
    while (start < text.size() &&
           std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(start, end - start);
}

inline std::string StripComment(const std::string& text) {
    const std::size_t comment_pos = text.find('#');
    if (comment_pos == std::string::npos) {
        return text;
    }
    return TrimCopy(text.substr(0, comment_pos));
}

inline std::vector<std::string> SplitWhitespace(const std::string& text) {
    std::vector<std::string> tokens;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

inline std::string ToUpperCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

inline bool TryParseInt(const std::string& text, int& value) {
    try {
        std::size_t index = 0;
        value = std::stoi(text, &index);
        return index == text.size();
    } catch (...) {
        return false;
    }
}

inline bool TryParseDouble(const std::string& text, double& value) {
    try {
        std::size_t index = 0;
        value = std::stod(text, &index);
        return index == text.size();
    } catch (...) {
        return false;
    }
}

#endif
