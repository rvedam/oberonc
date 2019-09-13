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
    /**
     * default constructor
     */
    LogReporter() = default;

    /**
     * default destructor
     */
    ~LogReporter() = default;

    /**
     *
     * @param fpath
     * @param line_no
     * @param char_pos
     * @param error
     */
    void report_lexer_error(std::string fpath, int line_no, int char_pos, std::string error);

    /**
     *
     * @param fpath
     * @param line_no
     * @param char_pos
     * @param error
     */
    void report_parser_error(std::string fpath, int line_no, int char_pos, std::string error);

    /**
     *
     * @param fpath
     * @param line_no
     * @param char_pos
     * @param warning
     */
    void report_parser_warning(std::string fpath, int line_no, int char_pos, std::string warning);

  private:
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
  };
}


#endif //OBERONC_LOGREPORTER_H
