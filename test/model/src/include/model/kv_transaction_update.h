/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MODEL_KV_TRANSACTION_UPDATE_H
#define MODEL_KV_TRANSACTION_UPDATE_H

#include <list>
#include <memory>
#include <string>

#include "model/data_value.h"

namespace model {

class kv_update;

/*
 * kv_transaction_update --
 *     An update operation within a transaction.
 */
class kv_transaction_update {

public:
    /*
     * kv_transaction_update::kv_transaction_update --
     *     Create a new instance of the update.
     */
    inline kv_transaction_update(
      const char *table_name, const data_value &key, std::shared_ptr<kv_update> &update)
        : _table_name(table_name), _key(key), _update(update)
    {
    }

    /*
     * kv_transaction_update::key --
     *     Get the key associated with the update. Note that this returns a copy of the key.
     */
    inline data_value
    key() const noexcept
    {
        return _key;
    }

    /*
     * kv_transaction_update::table_name --
     *     Get the name of the table associated with the update.
     */
    inline const std::string &
    table_name() const noexcept
    {
        return _table_name;
    }

    /*
     * kv_transaction_update::update --
     *     Get the update.
     */
    inline std::shared_ptr<kv_update>
    update() noexcept
    {
        return _update;
    }

private:
    /* Remember the table name, as storing a pointer to the table itself here is somewhat tricky. */
    std::string _table_name;

    data_value _key;
    std::shared_ptr<kv_update> _update;
};

} /* namespace model */
#endif
