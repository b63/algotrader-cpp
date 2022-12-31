#ifndef _JSON_H
#define _JSON_H

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>


typedef rapidjson::Document Document;
typedef rapidjson::Value Value;

typedef rapidjson::StringBuffer StringBuffer;
typedef rapidjson::Writer<StringBuffer> StringBufferWriter;



Document from_string(const std::string& json_str);

template <typename Key, typename... Args>
constexpr void add_or_overwrite_member(Document &d, Key&& key, Args&&... args)
{
    const Document::MemberIterator& iterator = d.FindMember(std::forward<Key>(key));
    if (iterator != d.MemberEnd())
        d.RemoveMember(iterator);
    d.AddMember(key, std::forward<Args>(args)...);
}


template <typename T>
    requires std::is_base_of<Value, T>::value
std::string to_string(const T& json)
{
    StringBuffer buffer;
    StringBufferWriter writer(buffer);
    json.Accept(writer);

    return std::string{buffer.GetString()};
}

struct DocumentCreator
{
    Document doc;
    DocumentCreator() : doc{rapidjson::kObjectType}
    {};

    template <typename KeyType, typename StrType>
    DocumentCreator& AddString(KeyType&& key, StrType&& str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(std::forward<StrType>(str), alloc), alloc);
        return *this;
    }

    template <typename KeyType, typename StrType>
    DocumentCreator& AddStringRef(KeyType&& key, StrType&& str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(str.c_str(), str.length(), alloc), alloc);
        return *this;
    }

    inline std::string as_string()
    {
        return to_string(doc);
    }
};



#endif
