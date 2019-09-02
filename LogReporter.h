//
// Created by Ramnarayan Vedam on 2019-09-01.
//

#ifndef OBERONC_LOGREPORTER_H
#define OBERONC_LOGREPORTER_H

#include <string>
#include <vector>

namespace utils
{
  class LogReporter
  {
  public:
    LogReporter();

    ~LogReporter();

    void report_lexer_error(std::string fpath, int line_no, int char_pos, std::string error);

    void report_lexer_warning(std::string fpath, int line_no, int char_pos, std::string warning);

    void report_parser_error(std::string fpath, int line_no, int char_pos, std::string error);

    void report_parser_warning(std::string fpath, int line_no, int char_pos, std::string warning);

  private:
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
  };
}


#endif //OBERONC_LOGREPORTER_H
