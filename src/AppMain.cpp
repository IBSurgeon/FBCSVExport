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
#include <exception>
#include <stdexcept>
#include <chrono>

namespace fs = std::filesystem;

enum class OptState { NONE, DATABASE, USERNAME, PASSWORD, CHARSET, DIALECT, OUTPUT_DIR, FILTER, SEPARATOR, PARALLEL };

constexpr char HELP_INFO[] = R"(
Usage fb_repl_print [file_path] <options>
General options:
    -h [ --help ]                        Show help
    -o [ --output-dir ] path             Output directory                                        
    -H [ --print-header ]                Print CSV header, default true
    -f [ --table-filter ]                Table filter
    -S [ --column-separator ]            Column separator, default ","
    -P [ --parallel ]                    Parallel threads, default 1

Database options:
    -d [ --database ] connection_string  Database connection string
    -u [ --username ] user               User name
    -p [ --password ] password           Password
    -c [ --charset ] ch                  Character set, default UTF8
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

static auto master = Firebird::fb_get_master_interface();

namespace FBExport
{

    FB_MESSAGE(InputRecord, Firebird::ThrowStatusWrapper,
        (FB_VARCHAR(4096 * 4), relation_name)
    );

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
        OutputRecord output(status, master);
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
        *reinterpret_cast<short*>(inputData.data() + inputMeta->getOffset(status, 0)) = static_cast<short>(tableIncludeFilter.size());
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

        unsigned char* p = out_buf, * e = out_buf + sizeof(out_buf);
        while (p < e)
        {
            short len = 0;
            switch (*p++)
            {
            case isc_info_error:
            case isc_info_end:
                p = e;
                break;

            case fb_info_tra_snapshot_number:
                len = static_cast<short>(isc_vax_integer(reinterpret_cast<char*>(p), 2));
                p += 2;
                ret = isc_portable_integer(p, len);
                p += len;
                break;
            }
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
                    m_sqlDialect = std::stoi(sql_dialect);
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
                    m_sqlDialect = std::stoi(arg);
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

    int ExportApp::exportData()
    {
        auto fbUtil = master->getUtilInterface();

        try
        {
            auto start = std::chrono::steady_clock::now();

            Firebird::ThrowStatusWrapper status(master->getStatus());

            Firebird::AutoDispose<Firebird::IXpbBuilder> dpbBuilder(fbUtil->getXpbBuilder(&status, Firebird::IXpbBuilder::DPB, nullptr, 0));
            dpbBuilder->insertString(&status, isc_dpb_user_name, m_username.c_str());
            dpbBuilder->insertString(&status, isc_dpb_password, m_password.c_str());
            dpbBuilder->insertString(&status, isc_dpb_lc_ctype, m_charset.c_str());

            const auto dpb = dpbBuilder->getBuffer(&status);
            const auto dbpLength = dpbBuilder->getBufferLength(&status);

            Firebird::AutoRelease<Firebird::IProvider> provider(master->getDispatcher());

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
                FBExport::CSVExportTable csvExport(att, tra, master);
                for (const auto& tableDesc : tables) {
                    csvExport.prepare(&status, tableDesc.relation_name, m_sqlDialect, false);
                    const std::string fileName = tableDesc.relation_name + ".csv";
                    csv::CSVFile csv(m_outputDir / fileName);
                    csvExport.printHeader(&status, csv);
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

                    std::thread t([att = std::move(workerAtt), tra = std::move(workerTra), this, &tables, &counter, &exceptionPointer]() mutable {
                        Firebird::ThrowStatusWrapper status(master->getStatus());

                        try {
                            FBExport::CSVExportTable csvExport(att, tra, master);
                            while (true) {
                                size_t localCounter = counter++;
                                if (localCounter >= tables.size())
                                    break;
                                const auto& tableDesc = tables[localCounter];
                                csvExport.prepare(&status, tableDesc.relation_name, m_sqlDialect, tableDesc.pp_cnt > 1);
                                std::string fileName = tableDesc.relation_name + ".csv";
                                if (tableDesc.page_sequence > 0) {
                                    fileName += ".part_" + std::to_string(tableDesc.page_sequence);
                                }
                                csv::CSVFile csv(m_outputDir / fileName);
                                if (tableDesc.page_sequence == 0) {
                                    csvExport.printHeader(&status, csv);
                                }
                                csvExport.printData(&status, csv, tableDesc.page_sequence);
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
                            exceptionPointer = std::current_exception();
                        }
                        });
                    thread_pool.push_back(std::move(t));
                }

                // export in main threads
                FBExport::CSVExportTable csvExport(att, tra, master);
                Firebird::ThrowStatusWrapper status(master->getStatus());
                while (true) {
                    size_t localCounter = counter++;
                    if (localCounter >= tables.size())
                        break;
                    const auto& tableDesc = tables[localCounter];
                    csvExport.prepare(&status, tableDesc.relation_name, m_sqlDialect, tableDesc.pp_cnt > 1);
                    std::string fileName = tableDesc.relation_name + ".csv";
                    if (tableDesc.page_sequence > 0) {
                        fileName += ".part_" + std::to_string(tableDesc.page_sequence);
                    }
                    csv::CSVFile csv(m_outputDir / fileName);
                    if (tableDesc.page_sequence == 0) {
                        csvExport.printHeader(&status, csv);
                    }
                    csvExport.printData(&status, csv, tableDesc.page_sequence);
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

                // merge part csv files into main csv file
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
}

using namespace FBExport;

int main(int argc, const char* argv[])
{
    ExportApp app;
    return app.exec(argc, argv);
}
