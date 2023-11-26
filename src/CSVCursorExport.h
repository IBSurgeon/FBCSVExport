#pragma once

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

        void prepare(Firebird::ThrowStatusWrapper* status, const std::string& tableName, const unsigned int sqlDialect, bool withDbkeyFilter = false);

        void printHeader(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv);

        void printData(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv, int64_t ppNum = 0);
    private:
        void exportResultSet(
            Firebird::ThrowStatusWrapper* status,
            Firebird::IResultSet* rs,
            unsigned char* buffer,
            csv::CSVFile& csv);
    };

}
