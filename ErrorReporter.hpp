#pragma once

#include <string>
#include <vector>

class ErrorReporter
{
private:
    std::vector<std::string> m_errors;
public:
    ErrorReporter() = default;
    ~ErrorReporter() = default;
    void report(std::string file, int line, int char_pos, std::string error);
    bool containsErrors() { return m_errors.size() > 0; }
    void printErrors();
};
