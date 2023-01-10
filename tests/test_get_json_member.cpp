#include "json.h"
#include "logger.h"

#include <cassert>
#include <cstring>

int main(int argc, char** argv)
{
    DocumentCreator<Document> dc;
    auto alloc = dc.doc.GetAllocator();
    dc.AddString("key1", "value1");
    dc.AddMember("key2", Value(rapidjson::kObjectType).AddMember("subkey1", Value().SetString("subvalue1", alloc), alloc));

    auto it = get_json_member(dc.doc, "key1");
    assert(it.has_value() && "key1 returned nullopt");
    log("{} = {}", "key1", it.value()->value.GetString());


    it = get_json_member(dc.doc, "key2", "subkey1");
    assert(it.has_value() && "key2.subkey1 returned nullopt");
    log("{} = {}", "key2.subkey1", it.value()->value.GetString());


    it = get_json_member(dc.doc, "key2", "subkey3");
    assert(!it.has_value() && "key2.subkey3 did not return nullopt");

    it = get_json_member(dc.doc, "key2", "subkey3", "dankmemes2050");
    assert(!it.has_value() && "key2.subkey3.dankmemes2050 did not return nullopt");

    auto op_str = get_json_string(dc.doc, "key2", "subkey3", "dankstring2040");
    assert(!op_str.has_value() && "key2.subkey3.dankstring2040 did not return nullopt");

    op_str = get_json_string(dc.doc, "key2", "subkey1");
    assert(op_str.has_value() && "key2.subkey1 returned nullopt");
    log("{} = {}", "key1.subkey1", op_str.value());

    return 0;
}
