#include "sqlda.h"
#include <string>

namespace {

    template <typename StatusType>
    void _fillSQLDA(StatusType* status, Firebird::IMessageMetadata* metadata, Firebird::SQLDAList& fields)
    {
        unsigned cnt = metadata->getCount(status);
        fields.reserve(cnt);
        fields.clear();

        for (unsigned i = 0; i < cnt; i++) {
            auto&& sqlda = fields.emplace_back();

            std::string field(metadata->getField(status, i));
            field.copy(sqlda.field, field.size());

            std::string relation(metadata->getRelation(status, i));
            relation.copy(sqlda.relation, relation.size());

            std::string alias(metadata->getAlias(status, i));
            alias.copy(sqlda.alias, alias.size());

            std::string owner(metadata->getOwner(status, i));
            owner.copy(sqlda.owner, owner.size());

            sqlda.type = metadata->getType(status, i);
            sqlda.nullable = metadata->isNullable(status, i);
            sqlda.sub_type = metadata->getSubType(status, i);
            sqlda.length = metadata->getLength(status, i);
            sqlda.charset = metadata->getCharSet(status, i);
            sqlda.scale = metadata->getScale(status, i);

            sqlda.offset = metadata->getOffset(status, i);
            sqlda.nullOffset = metadata->getNullOffset(status, i);
        }
    }
}

namespace Firebird
{

    void fillSQLDA(CheckStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields)
    {
        _fillSQLDA(status, metadata, fields);
    }

    void fillSQLDA(ThrowStatusWrapper* status, IMessageMetadata* metadata, SQLDAList& fields)
    {
        _fillSQLDA(status, metadata, fields);
    }

}
