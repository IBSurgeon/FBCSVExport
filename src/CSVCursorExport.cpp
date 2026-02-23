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

#include "CSVCursorExport.h"
#include "guid.h"
#include <cstdarg>
#include <sstream>

using namespace std;


constexpr char WHITESPACE[] = " \n\r\t\f\v";

std::string ltrim(const std::string& s)
{
	size_t start = s.find_first_not_of(WHITESPACE);
	return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string& s)
{
	size_t end = s.find_last_not_of(WHITESPACE);
	return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string& s) {
	return rtrim(ltrim(s));
}

static constexpr int64_t NUMERIC_FACTORS[] = {
					  0, // 0
					 10, // 1
					100, // 2
				   1000, // 3
				  10000, // 4
				 100000, // 5
				1000000, // 6
			   10000000, // 7
			  100000000, // 8
			 1000000000, // 9
			10000000000, // 10
		   100000000000, // 11
		  1000000000000, // 12
		 10000000000000, // 13
		100000000000000, // 14
	   1000000000000000, // 15
	  10000000000000000, // 16
	 100000000000000000, // 17
	1000000000000000000  // 18
};


std::string vformat(const char* zcFormat, ...) 
{

	// initialize use of the variable argument array
	va_list vaArgs;
	va_start(vaArgs, zcFormat);

	// reliably acquire the size
	// from a copy of the variable argument array
	// and a functionally reliable call to mock the formatting
	va_list vaArgsCopy;
	va_copy(vaArgsCopy, vaArgs);
	const int iLen = std::vsnprintf(nullptr, 0, zcFormat, vaArgsCopy);
	va_end(vaArgsCopy);

	// return a formatted string without risking memory mismanagement
	// and without assuming any compiler or platform specific behavior
	std::vector<char> zc(iLen + 1);
	std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
	va_end(vaArgs);
	return std::string(zc.data(), iLen);
}


template <typename T>
std::string getScaledInteger(const T value, short scale)
{
	auto factor = static_cast<T>(NUMERIC_FACTORS[-scale]);
	auto int_value = value / factor;
	auto frac_value = value % factor;
	if (frac_value < 0) frac_value = -frac_value;
	return vformat("%d.%0*d", int_value, -scale, frac_value);
}

std::string getBinaryString(const std::byte* data, size_t length)
{
	std::stringstream ss;
	if (data) {
		for (unsigned int i = 0; i < length; ++i)
			ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(*data++);
	}
	return ss.str();
}

std::string escapeMetaName(const unsigned int sqlDialect, const std::string& name)
{
	if (name == "RDB$DB_KEY")
		return name;
	switch (sqlDialect) {
	case 1:
		return name;
	case 3:
	default:
		return "\"" + name + "\"";
	}
}

std::string buildSqlForTable(const std::string& tableName, const unsigned int sqlDialect, bool withDbkeyFilter)
{
	std::string sql = "SELECT * FROM " + escapeMetaName(sqlDialect, tableName);
	if (withDbkeyFilter) {
		sql += " WHERE RDB$DB_KEY >= MAKE_DBKEY('" + tableName + "', 0, 0, ?)";
		sql += " AND RDB$DB_KEY < MAKE_DBKEY('" + tableName + "', 0, 0, ?)";
	}
	return sql;
}

namespace FBExport 
{
	CSVExportTable::CSVExportTable(
		Firebird::IAttachment* att,
		Firebird::ITransaction* tra,
		Firebird::IMaster* master
	)
		: m_att(att)
		, m_tra(tra)
		, m_master(master)
		, m_stmt{nullptr}
		, m_tableName{""}
		, m_sqlDialect(3)
		, m_withDbkeyFilter(false)
		, m_outMetadata{nullptr}
		, m_fields()
	{
		m_att->addRef();
		m_tra->addRef();
	}

	void CSVExportTable::prepare(Firebird::ThrowStatusWrapper* status, const std::string& tableName, unsigned int sqlDialect, bool withDbkeyFilter)
	{
		// for same table not need repeat prepare
		if (m_tableName == tableName) {
			return;
		}

		m_tableName = tableName;
		m_sqlDialect = sqlDialect;
		m_withDbkeyFilter = withDbkeyFilter;
		std::string sql = buildSqlForTable(tableName, sqlDialect, withDbkeyFilter);

		m_stmt.reset(m_att->prepare(
			status,
			m_tra,
			0,
			sql.c_str(),
			sqlDialect,
			Firebird::IStatement::PREPARE_PREFETCH_METADATA
		));

		m_outMetadata.reset(m_stmt->getOutputMetadata(status));
		Firebird::fillSQLDA(status, m_outMetadata, m_fields);
	}

	void CSVExportTable::printHeader(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv)
	{
		if (!m_stmt) {
			std::string message = "Statement not prepared";
			ISC_STATUS statusVector[] = {isc_arg_gds, isc_random,
			   isc_arg_string, (ISC_STATUS)message.c_str(),
			   isc_arg_end };
			status->setErrors(statusVector);
		}
		for (const auto& field : m_fields) {
			csv << field.field;
		}
		csv << csv::endrow;
	}

	void CSVExportTable::printData(Firebird::ThrowStatusWrapper* status, csv::CSVFile& csv, int64_t ppNum)
	{
		if (!m_stmt) {
			std::string message = "Statement not prepared";
			ISC_STATUS statusVector[] = { isc_arg_gds, isc_random,
			   isc_arg_string, (ISC_STATUS)message.c_str(),
			   isc_arg_end };
			status->setErrors(statusVector);
		}
		std::vector<unsigned char> buffer(m_outMetadata->getMessageLength(status));

		Firebird::AutoRelease<Firebird::IResultSet> rs;
		if (m_withDbkeyFilter) {
			InputMsgRecord input(status, m_master);

			input.clear();
			input->lowerPPNull = false;
			input->lowerPP = ppNum;
			input->upperPPNull = false;
			input->upperPP = ppNum + 1;


			rs.reset(
				m_stmt->openCursor(
					status,
					m_tra,
					input.getMetadata(),
					input.getData(),
					m_outMetadata,
					0)
			);
		}
		else {
			rs.reset(
				m_stmt->openCursor(
					status,
					m_tra,
					nullptr,
					nullptr,
					m_outMetadata,
					0)
			);
		}

		exportResultSet(status, rs, buffer.data(), csv);

		rs->close(status);
		rs.release();
	}

	void CSVExportTable::exportResultSet(
			Firebird::ThrowStatusWrapper* status,
			Firebird::IResultSet* rs,
			unsigned char* buffer,
			csv::CSVFile& csv)
	{
		auto fbUtil = m_master->getUtilInterface();
		auto i128 = fbUtil->getInt128(status);
		auto df16 = fbUtil->getDecFloat16(status);
		auto df34 = fbUtil->getDecFloat34(status);

		while (rs->fetchNext(status, buffer) == Firebird::IStatus::RESULT_OK)
		{
			for (const auto& field : m_fields) {
				short nullFlag = *reinterpret_cast<short*>(buffer + field.nullOffset);
				if (nullFlag) {
					csv << nullptr;
					continue;
				}
				unsigned char* valuePtr = reinterpret_cast<unsigned char*>(buffer + field.offset);
				switch (field.type) {
				case SQL_BOOLEAN:
				{
					auto value = *reinterpret_cast<unsigned char*>(valuePtr);
					csv << (value ? "1" : "0");
					break;
				}
				case SQL_TEXT:
				{
					if (field.charset == 1) {
						if (field.length == 16) {
							// special case BINARY(16) as GUID
							auto guid = reinterpret_cast<Firebird::Guid*>(valuePtr);
							char guidBuff[Firebird::GUID_BUFF_SIZE + 1] = { 0 };
							Firebird::GuidToString(guidBuff, guid);
							csv.write(guidBuff);
						}
						else {
							// BINARY(N)
							std::byte* b = reinterpret_cast<std::byte*>(valuePtr);
							csv << rtrim(getBinaryString(b, field.length));
						}
					}
					else {
						// CHAR(N)
						std::string s(reinterpret_cast<char*>(valuePtr), field.length);
						csv << rtrim(s);
					}
					break;
				}
				case SQL_VARYING:
				{
					if (field.charset == 1) {
						// VARBINARY(N)
						auto len = *reinterpret_cast<unsigned short*>(valuePtr);
						std::byte* b = reinterpret_cast<std::byte*>(valuePtr + 2);
						csv << getBinaryString(b, len);
					}
					else {
						// VARCHAR(N)
						auto len = *reinterpret_cast<unsigned short*>(valuePtr);
						std::string s(reinterpret_cast<char*>(valuePtr + 2), len);
						csv << s;
					}
					break;
				}
				case SQL_SHORT:
				{
					auto value = *reinterpret_cast<short*>(valuePtr);
					if (field.scale == 0) {
						csv << value;
					}
					else {
						csv.write(getScaledInteger(value, static_cast<short>(field.scale)));
					}
					break;
				}
				case SQL_LONG:
				{
					auto value = *reinterpret_cast<int*>(valuePtr);
					if (field.scale == 0) {
						csv << value;
					}
					else {
						csv.write(getScaledInteger(value, static_cast<short>(field.scale)));
					}
					break;
				}
				case SQL_INT64:
				{
					auto value = *reinterpret_cast<int64_t*>(valuePtr);
					if (field.scale == 0) {

						csv << value;
					}
					else {
						csv.write(getScaledInteger(value, static_cast<short>(field.scale)));
					}
					break;
				}
				case SQL_INT128:
				{
					auto i128Ptr = reinterpret_cast<FB_I128_t*>(valuePtr);
					std::string s;
					s.reserve(Firebird::IInt128::STRING_SIZE);
					i128->toString(status, i128Ptr, field.scale, Firebird::IInt128::STRING_SIZE, s.data());
					csv.write(s);
					break;
				}
				case SQL_FLOAT:
				{
					auto value = *reinterpret_cast<float*>(valuePtr);
					csv << value;
					break;
				}
				case SQL_D_FLOAT:
				case SQL_DOUBLE:
				{
					auto value = *reinterpret_cast<double*>(valuePtr);
					csv << value;
					break;
				}
				case SQL_TYPE_DATE:
				{
					auto value = *reinterpret_cast<ISC_DATE*>(valuePtr);
					unsigned int year, month, day;
					fbUtil->decodeDate(value, &year, &month, &day);
					auto s = vformat("%d-%02d-%02d", year, month, day);
					csv.write(s);
					break;
				}
				case SQL_TYPE_TIME:
				{
					auto value = *reinterpret_cast<ISC_TIME*>(valuePtr);
					unsigned int hours, minutes, seconds, fractions;
					fbUtil->decodeTime(value, &hours, &minutes, &seconds, &fractions);
					auto s = vformat("%02d:%02d:%02d.%04d", hours, minutes, seconds, fractions);
					csv.write(s);
					break;
				}
				case SQL_TIMESTAMP:
				{
					auto value = *reinterpret_cast<ISC_TIMESTAMP*>(valuePtr);
					unsigned int year, month, day;
					fbUtil->decodeDate(value.timestamp_date, &year, &month, &day);
					unsigned int hours, minutes, seconds, fractions;
					fbUtil->decodeTime(value.timestamp_time, &hours, &minutes, &seconds, &fractions);
					auto s = vformat("%d-%02d-%02d %02d:%02d:%02d.%04d", year, month, day, hours, minutes, seconds, fractions);
					csv.write(s);
					break;
				}
				case SQL_DEC16:
				{
					auto decValuePtr = reinterpret_cast<FB_DEC16_t*>(valuePtr);
					char decBuffer[Firebird::IDecFloat16::STRING_SIZE + 1] = { 0 };
					df16->toString(status, decValuePtr, Firebird::IDecFloat16::STRING_SIZE, decBuffer);
					csv.write(&decBuffer[0]);
					break;
				}
				case SQL_DEC34:
				{
					auto decValuePtr = reinterpret_cast<FB_DEC34_t*>(valuePtr);
					char decBuffer[Firebird::IDecFloat34::STRING_SIZE + 1] = { 0 };
					df34->toString(status, decValuePtr, Firebird::IDecFloat34::STRING_SIZE, decBuffer);
					csv.write(&decBuffer[0]);
					break;
				}
				case SQL_TIMESTAMP_TZ:
				{
					auto tsPtr = reinterpret_cast<ISC_TIMESTAMP_TZ*>(valuePtr);
					unsigned int year, month, day;
					unsigned int hours, minutes, seconds, fractions;
					char tsBuffer[32] = { 0 };
					fbUtil->decodeTimeStampTz(
						status, tsPtr,
						&year, &month, &day,
						&hours, &minutes, &seconds, &fractions,
					    static_cast<unsigned int>(std::size(tsBuffer)), 
						tsBuffer
					);
					auto s = vformat("%d-%02d-%02d %02d:%02d:%02d.%04d %s", year, month, day, hours, minutes, seconds, fractions, tsBuffer);
					csv.write(s);
					break;
				}
				case SQL_TIME_TZ:
				{
					auto tsPtr = reinterpret_cast<ISC_TIME_TZ*>(valuePtr);
					unsigned int hours, minutes, seconds, fractions;
					char tsBuffer[32] = { 0 };
					fbUtil->decodeTimeTz(
						status, tsPtr,
						&hours, &minutes, &seconds, &fractions,
						static_cast<unsigned int>(std::size(tsBuffer)),
						tsBuffer
					);
					auto s = vformat("%02d:%02d:%02d.%04d %s", hours, minutes, seconds, fractions, tsBuffer);
					csv.write(s);
					break;
				}
				case SQL_BLOB:
				{
					// can not support export blob data, 
					csv << nullptr;
					break;
				}
				case SQL_ARRAY:
				{
					// can not support export array data, 
					csv << nullptr;
					break;
				}
				}
			}
			csv << csv::endrow;
		}
	}
} // namespace FBExport
