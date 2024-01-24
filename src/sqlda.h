#pragma once

#ifndef SQLDA_H
#define SQLDA_H

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

#include "firebird/Interface.h"
#include <vector>

namespace Firebird
{

    struct SQLDA {
        char field[63 * 4]{ "" };
        char relation[63 * 4]{ "" };
        char owner[63 * 4]{ "" };
        char alias[63 * 4]{ "" };
        unsigned type = 0;
        bool nullable = false;
        int sub_type = 0;
        unsigned length = 0;
        int scale = 0;
        unsigned charset = 0;
        unsigned offset = 0;
        unsigned nullOffset = 0;
    };

    using SQLDAList = std::vector<SQLDA>;

    void fillSQLDA(CheckStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields);

    void fillSQLDA(ThrowStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields);
    
} // namespace Firebird

#endif // SQLDA_H
