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

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <firebird/Interface.h>
#include <firebird/Message.h>
#include "FBAutoPtr.h"
#include "CSVFile.h"
#include "CSVCursorExport.h"
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <chrono>

namespace fs = std::filesystem;

enum class OptState { NONE, DATABASE, USERNAME, PASSWORD, CHARSET, DIALECT, OUTPUT_DIR, FILTER, SEPARATOR, PARALLEL };

constexpr char HELP_INFO[] = R"(
Usage CSVExport [out_dir] <options>
General options:
    -h [ --help ]                        Show help
    -o [ --output-dir ] path             Output directory
    -H [ --print-header ]                Print CSV header, default false
    -f [ --table-filter ]                Table filter
    -S [ --column-separator ]            Column separator, default ",". Supported: ",", ";" and "t".
                                         Where "t" is '\t'. 
    -P [ --parallel ]                    Parallel threads, default 1

Database options:
    -d [ --database ] connection_string  Database connection string
    -u [ --username ] user               User name
    -p [ --password ] password           Password
    -c [ --charset ] charset             Character set, default UTF8
    -s [ --sql-dialect ] dialect         SQL dialect, default 3
)";

constexpr auto SQL_RELATIONS_SIMPLE = R"(
SELECT
    R.RDB$RELATION_ID AS RELATION_ID,
    TRIM(R.RDB$RELATION_NAME) AS RELATION_NAME,
    0 AS PAGE_SEQUENCE,
    1 AS PP_CNT
FROM RDB$RELATIONS R
WHERE R.RDB$SYSTEM_FLAG = 0 AND
      R.RDB$RELATION_TYPE = 0 AND
      TRIM(R.RDB$RELATION_NAME) SIMILAR TO CAST(? AS VARCHAR(8191))
ORDER BY R.RDB$RELATION_NAME
)";

constexpr auto SQL_RELATIONS_MULTY = R"(
SELECT
    R.RDB$RELATION_ID AS RELATION_ID,
    TRIM(R.RDB$RELATION_NAME) AS RELATION_NAME,
    P.RDB$PAGE_SEQUENCE AS PAGE_SEQUENCE,
    COUNT(P.RDB$PAGE_SEQUENCE) OVER(PARTITION BY R.RDB$RELATION_NAME) AS PP_CNT
FROM RDB$RELATIONS R
JOIN RDB$PAGES P ON P.RDB$RELATION_ID = R.RDB$RELATION_ID
WHERE R.RDB$SYSTEM_FLAG = 0 AND
      R.RDB$RELATION_TYPE = 0 AND
      P.RDB$PAGE_TYPE = 4 AND
      TRIM(R.RDB$RELATION_NAME) SIMILAR TO CAST(? AS VARCHAR(8191))
ORDER BY R.RDB$RELATION_NAME, P.RDB$PAGE_SEQUENCE
)";

static auto fb_master = Firebird::fb_get_master_interface();

namespace FBExport
{

    FB_MESSAGE(OutputRecord, Firebird::ThrowStatusWrapper,
        (FB_SMALLINT, releation_id)
        (FB_VARCHAR(252), relation_name)
        (FB_INTEGER, page_sequence)
        (FB_BIGINT, pp_cnt)
    );

    struct TableDesc
    {
        TableDesc() = default;
        TableDesc(const OutputRecord& rec)
            : releation_id(rec->releation_id)
            , relation_name(rec->relation_name.str, rec->relation_name.length)
            , page_sequence(rec->page_sequence)
            , pp_cnt(rec->pp_cnt)
        {}

        short releation_id;
        std::string relation_name;
        int32_t page_sequence;
        int64_t pp_cnt;
    };


    std::vector<TableDesc> getTablesDesc(
        Firebird::ThrowStatusWrapper* status,
        Firebird::IAttachment* att,
        Firebird::ITransaction* tra,
        unsigned int sqlDialect,
        const std::string& tableIncludeFilter,
        bool singleWorker = true)
    {
        OutputRecord output(status, fb_master);
        output.clear();

        Firebird::AutoRelease<Firebird::IStatement> stmt;

        if (singleWorker) {
            stmt.reset(att->prepare(
                status,
                tra,
                0,
                SQL_RELATIONS_SIMPLE,
                sqlDialect,
                Firebird::IStatement::PREPARE_PREFETCH_METADATA
            ));
        }
        else {
            stmt.reset(att->prepare(
                status,
                tra,
                0,
                SQL_RELATIONS_MULTY,
                sqlDialect,
                Firebird::IStatement::PREPARE_PREFETCH_METADATA
            ));
        }

        // This function is executed once, so here you can allocate memory on the heap without fear of re-allocation.
        Firebird::AutoRelease<Firebird::IMessageMetadata> inputMeta(
            stmt->getInputMetadata(status)
        );
        std::vector<unsigned char> inputData(inputMeta->getMessageLength(status));
        *reinterpret_cast<short*>(inputData.data() + inputMeta->getNullOffset(status, 0)) = false; // nullInd
        *reinterpret_cast<unsigned short*>(inputData.data() + inputMeta->getOffset(status, 0)) = static_cast<unsigned short>(tableIncludeFilter.size());
        tableIncludeFilter.copy(reinterpret_cast<char*>(inputData.data() + inputMeta->getOffset(status, 0) + 2), tableIncludeFilter.size());

        Firebird::AutoRelease<Firebird::IResultSet> rs(stmt->openCursor(
            status,
            tra,
            inputMeta,
            inputData.data(),
            output.getMetadata(),
            0
        ));

        std::vector<TableDesc> tables;
        while (rs->fetchNext(status, output.getData()) == Firebird::IStatus::RESULT_OK)
        {
            tables.emplace_back(output);
        }

        rs->close(status);
        rs.release();

        return tables;
    }

    ISC_INT64 getSnapshotNumber(Firebird::ThrowStatusWrapper* status, Firebird::ITransaction* tra)
    {
        ISC_INT64 ret = 0;
        unsigned char in_buf[] = { fb_info_tra_snapshot_number, isc_info_end };
        unsigned char out_buf[16] = { 0 };

        tra->getInfo(status, sizeof(in_buf), in_buf, sizeof(out_buf), out_buf);

        auto fb_util = fb_master->getUtilInterface();
        Firebird::AutoDispose<Firebird::IXpbBuilder> xpbParser(
            fb_util->getXpbBuilder(status, Firebird::IXpbBuilder::INFO_RESPONSE, out_buf, sizeof(out_buf))
        );

        if (xpbParser->findFirst(status, fb_info_tra_snapshot_number)) {
            ret = xpbParser->getBigInt(status);
        }
        
        return ret;
    }

    class ExportApp final
    {
        fs::path m_outputDir;
        std::string m_filter;
        std::string m_separator{","};
        int m_parallel = 1;
        bool m_printHeader = false;
        // database options
        std::string m_database;
        std::string m_username;
        std::string m_password;
        std::string m_charset{"UTF8"};
        unsigned short m_sqlDialect = 3;
    public:
        int exec(int argc, const char** argv);
    private:
        void printHelp()
        {
            std::cout << HELP_INFO << std::endl;
            std::exit(0);
        }

        int exportData();

        void exportByTableDesc(Firebird::ThrowStatusWrapper* status, FBExport::CSVExportTable& csvExport, const TableDesc& tableDesc);

        void parseArgs(int argc, const char** argv);
    };

    int ExportApp::exec(int argc, const char** argv)
    {
        parseArgs(argc, argv);
        exportData();
        return 0;
    }

    void ExportApp::parseArgs(int argc, const char** argv)
    {
        if (argc < 2) {
            printHelp();
            exit(0);
        }
        OptState st = OptState::NONE;
        for (int i = 1; i < argc; i++) {
            std::string arg(argv[i]);
            if ((arg.size() == 2) && (arg[0] == '-')) {
                st = OptState::NONE;
                // it's option
                switch (arg[1]) {
                case 'h':
                    printHelp();
                    exit(0);
                    break;
                case 'o':
                    st = OptState::OUTPUT_DIR;
                    break;
                case 'H':
                    m_printHeader = true;
                    break;
                case 'f':
                    st = OptState::FILTER;
                    break;
                case 'S':
                    st = OptState::SEPARATOR;
                    break;
                case 'P':
                    st = OptState::PARALLEL;
                    break;
                case 'd':
                    st = OptState::DATABASE;
                    break;
                case 'u':
                    st = OptState::USERNAME;
                    break;
                case 'p':
                    st = OptState::PASSWORD;
                    break;
                case 'c':
                    st = OptState::CHARSET;
                    break;
                case 's':
                    st = OptState::DIALECT;
                    break;
                default:
                    std::cerr << "Error: unrecognized option '" << arg << "'. See: --help" << std::endl;
                    exit(-1);
                }
            }
            else if ((arg.size() > 2) && (arg[0] == '-') && (arg[1] == '-')) {
                st = OptState::NONE;
                // it's option
                if (arg == "--help") {
                    printHelp();
                    exit(0);
                }
                if (arg == "--output-dir") {
                    st = OptState::OUTPUT_DIR;
                    continue;
                }
                if (arg == "--print-header") {
                    m_printHeader = true;
                    continue;
                }
                if (arg == "--table-filter") {
                    st = OptState::FILTER;
                    continue;
                }
                if (arg == "--column-separator") {
                    st = OptState::SEPARATOR;
                    continue;
                }
                if (arg == "--parallel") {
                    st = OptState::PARALLEL;
                    continue;
                }
                if (arg == "--database") {
                    st = OptState::DATABASE;
                    continue;
                }
                if (arg == "--username") {
                    st = OptState::USERNAME;
                    continue;
                }
                if (arg == "--password") {
                    st = OptState::PASSWORD;
                    continue;
                }
                if (arg == "--charset") {
                    st = OptState::CHARSET;
                    continue;
                }
                if (arg == "--sql-dialect") {
                    st = OptState::DIALECT;
                    continue;
                }
                if (auto pos = arg.find("--output-dir="); pos == 0) {
                    m_outputDir.assign(arg.substr(13));
                    continue;
                }
                if (auto pos = arg.find("--table-filter="); pos == 0) {
                    m_filter.assign(arg.substr(15));
                    continue;
                }
                if (auto pos = arg.find("--column-separator="); pos == 0) {
                    m_separator.assign(arg.substr(19));
                    if (m_separator.length() > 1) {
                        std::cerr << "Error: invalid separator" << std::endl;
                        exit(-1);
                    }
                    else {
                        switch (m_separator[0]) {
                        case ',':
                        case ';':
                            break;
                        case 't':
                            m_separator = "\t";
                            break;
                        default:
                            std::cerr << "Error: invalid separator" << std::endl;
                            exit(-1);
                        }
                    }
                    continue;
                }
                if (auto pos = arg.find("--parallel="); pos == 0) {
                    std::string par = arg.substr(11);
                    m_parallel = std::stoi(par);
                    if (m_parallel < 0) {
                        std::cerr << "Error: parallel must be greater than 0" << std::endl;
                        exit(-1);
                    }
                    continue;
                }
                if (auto pos = arg.find("--database="); pos == 0) {
                    m_database.assign(arg.substr(11));
                    continue;
                }
                if (auto pos = arg.find("--username="); pos == 0) {
                    m_username.assign(arg.substr(11));
                    continue;
                }
                if (auto pos = arg.find("--password="); pos == 0) {
                    m_password.assign(arg.substr(11));
                    continue;
                }
                if (auto pos = arg.find("--charset="); pos == 0) {
                    m_charset.assign(arg.substr(10));
                    continue;
                }
                if (auto pos = arg.find("--sql-dialect="); pos == 0) {
                    std::string sql_dialect = arg.substr(14);
                    m_sqlDialect = static_cast<unsigned short>(std::stoi(sql_dialect));
                    if (m_sqlDialect != 1 && m_sqlDialect != 3) {
                        std::cerr << "Error: sql_dialect must be 1 or 3" << std::endl;
                        exit(-1);
                    }
                    continue;
                }
                std::cerr << "Error: unrecognized option '" << arg << "'. See: --help" << std::endl;
                exit(-1);
            }
            else {
                if (i == 1) {
                    m_outputDir.assign(arg);
                    continue;
                }
                switch (st) {
                case OptState::OUTPUT_DIR:
                    m_outputDir.assign(arg);
                    break;
                case OptState::FILTER:
                    m_filter.assign(arg);
                    break;
                case OptState::SEPARATOR:
                    m_separator.assign(arg);
                    if (m_separator.length() > 1) {
                        std::cerr << "Error: invalid separator" << std::endl;
                        exit(-1);
                    }
                    else {
                        switch (m_separator[0]) {
                        case ',':
                        case ';':
                            break;
                        case 't':
                            m_separator = "\t";
                            break;
                        default:
                            std::cerr << "Error: invalid separator" << std::endl;
                            exit(-1);
                        }
                    }
                    break;
                case OptState::PARALLEL:
                    m_parallel = std::stoi(arg);
                    if (m_parallel < 0) {
                        std::cerr << "Error: parallel must be greater than 0" << std::endl;
                        exit(-1);
                    }
                    break;
                case OptState::DATABASE:
                    m_database.assign(arg);
                    break;
                case OptState::USERNAME:
                    m_username.assign(arg);
                    break;
                case OptState::PASSWORD:
                    m_password.assign(arg);
                    break;
                case OptState::CHARSET:
                    m_charset.assign(arg);
                    break;
                case OptState::DIALECT:
                    m_sqlDialect = static_cast<unsigned short>(std::stoi(arg));
                    if (m_sqlDialect != 1 && m_sqlDialect != 3) {
                        std::cerr << "Error: sql_dialect must be 1 or 3" << std::endl;
                        exit(-1);
                    }
                    break;
                default:
                    continue;
                }
            }
        }
        if (m_outputDir.empty()) {
            std::cerr << "Error: the option '--output-dir' is required but missing" << std::endl;
            exit(-1);
        }
    }

    void ExportApp::exportByTableDesc(Firebird::ThrowStatusWrapper* status, FBExport::CSVExportTable& csvExport, const TableDesc& tableDesc)
    {
        // If the number of PP pages is greater than 1, then it is a large table.To extract data from it, 
        // a SQL query is built with a division into RDB$DB_KEY ranges.
        bool withDbKeyFilter = tableDesc.pp_cnt > 1;
        csvExport.prepare(status, tableDesc.relation_name, m_sqlDialect, withDbKeyFilter);
        std::string fileName = tableDesc.relation_name + ".csv";
        // If this is not the first part of the page, then the file is temporary; the extension ".partN" is added to it.
        if (tableDesc.page_sequence > 0) {
            fileName += ".part_" + std::to_string(tableDesc.page_sequence);
        }
        csv::CSVFile csv(m_outputDir / fileName);
        if (tableDesc.page_sequence == 0 && m_printHeader) {
            csvExport.printHeader(status, csv);
        }
        csvExport.printData(status, csv, tableDesc.page_sequence);
    }

    int ExportApp::exportData()
    {
        auto fbUtil = fb_master->getUtilInterface();

        try
        {
            auto start = std::chrono::steady_clock::now();

            Firebird::ThrowStatusWrapper status(fb_master->getStatus());

            Firebird::AutoDispose<Firebird::IXpbBuilder> dpbBuilder(fbUtil->getXpbBuilder(&status, Firebird::IXpbBuilder::DPB, nullptr, 0));
            dpbBuilder->insertString(&status, isc_dpb_user_name, m_username.c_str());
            dpbBuilder->insertString(&status, isc_dpb_password, m_password.c_str());
            dpbBuilder->insertString(&status, isc_dpb_lc_ctype, m_charset.c_str());

            const auto dpb = dpbBuilder->getBuffer(&status);
            const auto dbpLength = dpbBuilder->getBufferLength(&status);

            Firebird::AutoRelease<Firebird::IProvider> provider(fb_master->getDispatcher());

            Firebird::AutoRelease<Firebird::IAttachment> att(
                provider->attachDatabase(
                    &status,
                    m_database.c_str(),
                    dbpLength,
                    dpb
                )
            );

            Firebird::AutoDispose<Firebird::IXpbBuilder> tpbBuilder(fbUtil->getXpbBuilder(&status, Firebird::IXpbBuilder::TPB, nullptr, 0));
            tpbBuilder->insertTag(&status, isc_tpb_concurrency);

            Firebird::AutoRelease<Firebird::ITransaction> tra(
                att->startTransaction(
                    &status,
                    tpbBuilder->getBufferLength(&status),
                    tpbBuilder->getBuffer(&status)
                )
            );

            auto tables = getTablesDesc(&status, att, tra, m_sqlDialect, m_filter, m_parallel == 1);

            if (m_parallel == 1) {
                auto start_p = std::chrono::steady_clock::now();
                FBExport::CSVExportTable csvExport(att, tra, fb_master);
                for (const auto& tableDesc : tables) {
                    csvExport.prepare(&status, tableDesc.relation_name, m_sqlDialect, false);
                    const std::string fileName = tableDesc.relation_name + ".csv";
                    csv::CSVFile csv(m_outputDir / fileName);
                    if (m_printHeader) {
                        csvExport.printHeader(&status, csv);
                    }
                    csvExport.printData(&status, csv);
                }
                auto end_p = std::chrono::steady_clock::now();
                std::cout << "Elapsed time in milliseconds parallel_part: "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end_p - start_p).count()
                    << " ms" << std::endl;
            }
            else {
                auto start_p = std::chrono::steady_clock::now();
                const auto workerCount = m_parallel - 1;

                // get snapshot number
                auto snapshotNumber = getSnapshotNumber(&status, tra);
                std::exception_ptr exceptionPointer = nullptr;

                std::mutex m;
                std::atomic<size_t> counter = 0;

                std::vector<std::thread> thread_pool;
                thread_pool.reserve(workerCount);
                // worker threads
                for (int i = 0; i < workerCount; i++) {
                    Firebird::AutoRelease<Firebird::IAttachment> workerAtt(
                        provider->attachDatabase(
                            &status,
                            m_database.c_str(),
                            dbpLength,
                            dpb
                        )
                    );
                    Firebird::AutoDispose<Firebird::IXpbBuilder> tpbWorkerBuilder(fbUtil->getXpbBuilder(&status, Firebird::IXpbBuilder::TPB, nullptr, 0));
                    tpbWorkerBuilder->insertTag(&status, isc_tpb_concurrency);
                    tpbWorkerBuilder->insertBigInt(&status, isc_tpb_at_snapshot_number, snapshotNumber);

                    Firebird::AutoRelease<Firebird::ITransaction> workerTra(
                        workerAtt->startTransaction(
                            &status,
                            tpbWorkerBuilder->getBufferLength(&status),
                            tpbWorkerBuilder->getBuffer(&status)
                        )
                    );

                    std::thread t([att = std::move(workerAtt), tra = std::move(workerTra), 
                                   this, &m, &tables, &counter, &exceptionPointer]() mutable {
                        Firebird::ThrowStatusWrapper status(fb_master->getStatus());

                        try {
                            FBExport::CSVExportTable csvExport(att, tra, fb_master);
                            while (true) {
                                size_t localCounter = counter++;
                                if (localCounter >= tables.size())
                                    break;
                                const auto& tableDesc = tables[localCounter];
                                exportByTableDesc(&status, csvExport, tableDesc);
                            }
                            if (tra) {
                                tra->commit(&status);
                                tra.release();
                            }

                            if (att) {
                                att->detach(&status);
                                att.release();
                            }
                        }
                        catch (...) {
                            std::unique_lock<std::mutex> lock(m);
                            exceptionPointer = std::current_exception();
                        }
                        });
                    thread_pool.push_back(std::move(t));
                }

                // export in main threads
                FBExport::CSVExportTable csvExport(att, tra, fb_master);
                while (true) {
                    size_t localCounter = counter++;
                    if (localCounter >= tables.size())
                        break;
                    const auto& tableDesc = tables[localCounter];
                    exportByTableDesc(&status, csvExport, tableDesc);
                }


                for (auto& th : thread_pool) {
                    th.join();
                }
                if (exceptionPointer) {
                    std::rethrow_exception(exceptionPointer);
                }

                auto end_p = std::chrono::steady_clock::now();
                std::cout << "Elapsed time in milliseconds parallel_part: "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end_p - start_p).count()
                    << " ms" << std::endl;

                // For each large table, the CSV files are merged into one (main) file.
                for (size_t i = 0; i < tables.size(); i++) {
                    const auto& tableDesc = tables[i];
                    if (tableDesc.pp_cnt > 1) {
                        std::string fileName = tableDesc.relation_name + ".csv";
                        std::ofstream ofile(m_outputDir / fileName, std::ios::out | std::ios::app);
                        i++;
                        for (int64_t j = 1; j < tableDesc.pp_cnt; j++, i++) {
                            std::string partFileName = fileName + ".part_" + std::to_string(j);
                            auto partFilePath = m_outputDir / partFileName;
                            std::ifstream ifile(partFilePath, std::ios::in);
                            ofile << ifile.rdbuf();
                            ifile.close();
                            fs::remove(partFilePath);
                        }
                        ofile.close();
                    }
                }
            }

            if (tra) {
                tra->commit(&status);
                tra.release();
            }

            if (att) {
                att->detach(&status);
                att.release();
            }

            auto end = std::chrono::steady_clock::now();

            std::cout << "Elapsed time in milliseconds: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                << " ms" << std::endl;

        }
        catch (const Firebird::FbException& e) {
            char buffer[2048];
            fbUtil->formatStatus(buffer, static_cast<unsigned int>(std::size(buffer)), e.getStatus());
            std::cerr << "Error: " << buffer << std::endl;
            return 1;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    }
} // namespace FBExport

using namespace FBExport;

int main(int argc, const char* argv[])
{
    ExportApp app;
    return app.exec(argc, argv);
}
