//
// Created by vedam on 4/24/22.
//

#include <iostream>
#include <sstream>
#include <iterator>
#include "ErrorReporter.hpp"

void ErrorReporter::report(std::string file, int line, int char_pos, std::string error)
{
    std::ostringstream oss;
    oss << file << ":" << line << ":" << char_pos << ": " << error;
    m_errors.emplace_back(oss.str());
}

void ErrorReporter::printErrors()
{
    std::copy(m_errors.cbegin(), m_errors.cend(), std::ostream_iterator<std::string>(std::cout, "\n"));
}
