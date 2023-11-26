#include "CSVFile.h"
#include <sstream>

using namespace csv;

CSVFile::CSVFile(const fs::path& filename, const std::string separator)
    : fs_(filename) 
    , is_first_(true)
    , separator_(separator)
    , escape_seq_("\"")
    , special_chars_("\"")
{
    fs_.exceptions(std::ios::failbit | std::ios::badbit);
    //fs_.open(filename);
}

CSVFile::~CSVFile()
{
    flush();
    fs_.close();
}

std::string CSVFile::escape(const std::string& val)
{
    std::ostringstream result;
    result << '"';
    std::string::size_type to, from = 0u, len = val.length();
    while (from < len &&
        std::string::npos != (to = val.find_first_of(special_chars_, from)))
    {
        result << val.substr(from, to - from) << escape_seq_ << val[to];
        from = to + 1;
    }
    result << val.substr(from) << '"';
    return result.str();
}

namespace csv
{

    CSVFile& endrow(CSVFile& file)
    {
        file.endrow();
        return file;
    }

    CSVFile& flush(CSVFile& file)
    {
        file.flush();
        return file;
    }

}