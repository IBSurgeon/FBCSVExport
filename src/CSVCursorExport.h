#pragma once

#ifndef CSV_CURSOR_H
#define CSV_CURSOR_H

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
#include "sqlda.h"
#include <firebird/Interface.h>
#include <firebird/Message.h>
#include "FBAutoPtr.h"
#include <string>

namespace FBExport
{
    class CSVExportTable
    {
        FB_MESSAGE(InputMsgRecord, Firebird::ThrowStatusWrapper,
            (FB_BIGINT, lowerPP)
            (FB_BIGINT, upperPP)
        );

        Firebird::AutoRelease<Firebird::IAttachment> m_att;
        Firebird::AutoRelease<Firebird::ITransaction> m_tra;
        Firebird::IMaster* m_master = nullptr;
        Firebird::AutoRelease<Firebird::IStatement> m_stmt;
        std::string m_tableName{ "" };
        unsigned int m_sqlDialect = 3;
        bool m_withDbkeyFilter = false;
        Firebird::AutoRelease<Firebird::IMessageMetadata> m_outMetadata;
        Firebird::SQLDAList m_fields;
    public: 
        CSVExportTable(
            Firebird::IAttachment* att,
            Firebird::ITransaction* tra,
            Firebird::IMaster* master
        );

        void prepare(Firebird::ThrowStatusWrapper* status, const std::string& tableName, unsigned int sqlDialect, bool withDbkeyFilter = false);

        void printHeader(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv);

        void printData(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv, int64_t ppNum = 0);
    private:
        void exportResultSet(
            Firebird::ThrowStatusWrapper* status,
            Firebird::IResultSet* rs,
            unsigned char* buffer,
            csv::CSVFile& csv);
    };

} // namespace FBExport

#endif // CSV_CURSOR_H
