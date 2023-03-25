
#include "json.h"
#include "logger.h"


Document from_string(const std::string& json_str)
{
    Document d(rapidjson::Type::kObjectType);
    d.Parse(json_str.data());

    return d;
}


template<>
double get_member_from_str<double>(const Value& doc, const std::string& key)
{
    const char* key_str =  key.c_str();
    if (!doc.HasMember(key_str))
        throw std::runtime_error(std::format("document does not have member {}", key));

    const Value& val = doc[key_str];
    if (!val.IsString())
        throw std::runtime_error(std::format("value for member {} is not type string", key));

    const char* val_str = val.GetString();
    return std::stold(val_str);
}
