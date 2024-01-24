/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Simonov Denis
 *  for the open source project "Firebird CSVExport".
 *
 *  Copyright (c) 2023 Simonov Denis <sim-mail@list.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

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

} // namespace csv