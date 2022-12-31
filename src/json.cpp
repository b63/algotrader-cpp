
#include "json.h"
#include "logger.h"


Document from_string(const std::string& json_str)
{
    Document d(rapidjson::Type::kObjectType);
    d.Parse(json_str.data());

    return d;
}


