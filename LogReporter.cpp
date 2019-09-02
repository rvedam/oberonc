//
// Created by Ramnarayan Vedam on 2019-09-01.
//

#include <sstream>
#include "LogReporter.h"

namespace utils
{
  void LogReporter::report_lexer_error(std::string fpath, int line_no, int char_pos, std::string error)
  {
    std::ostringstream oss;
    oss << fpath << ":" << line_no << ":" << char_pos << ": ERROR: " << error;
    m_errors.emplace_back(oss.str());
  }

  void LogReporter::report_parser_error(std::string fpath, int line_no, int char_pos, std::string error)
  {
    std::ostringstream oss;
    oss << fpath << ":" << line_no << ":" << char_pos << ": ERROR: " << error;
    m_errors.emplace_back(oss.str());
  }

}