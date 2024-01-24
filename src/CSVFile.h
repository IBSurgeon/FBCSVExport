#pragma once

#ifndef CSV_FILE_H
#define CSV_FILE_H

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

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace csv
{
	class CSVFile
	{
        std::ofstream fs_;
        bool is_first_;
        const std::string separator_;
        const std::string escape_seq_;
        const std::string special_chars_;
    public:
        CSVFile(const fs::path& filename, const std::string separator = ";");

        ~CSVFile();

        void flush()
        {
            fs_.flush();
        }

        void endrow()
        {
            fs_ << std::endl;
            is_first_ = true;
        }

        CSVFile& operator << (CSVFile& (*val)(CSVFile&))
        {
            return val(*this);
        }

        CSVFile& operator << (const char* val)
        {
            return write(escape(val));
        }

        CSVFile& operator << (const std::string& val)
        {
            return write(escape(val));
        }

        CSVFile& operator << ([[maybe_unused]] std::nullptr_t val)
        {
            return write("");
        }

        template<typename T>
        CSVFile& operator << (const T& val)
        {
            return write(val);
        }

        template<typename T>
        CSVFile& write(const T& val)
        {
            if (!is_first_)
            {
                fs_ << separator_;
            }
            else
            {
                is_first_ = false;
            }
            fs_ << val;
            return *this;
        }
    private:

        std::string escape(const std::string& val);
	};

    CSVFile& endrow(CSVFile& file);
    CSVFile& flush(CSVFile& file);

} // namespace csv

#endif // CSV_FILE_H
