#pragma once

#ifndef SQLDA_H
#define SQLDA_H

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

        short originalType = 0;
        short columnSize = 0;
        short originalScale = 0;
    };

    using SQLDAList = std::vector<SQLDA>;

    void fillSQLDA(CheckStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields);

    void fillSQLDA(ThrowStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields);
    
}

#endif
