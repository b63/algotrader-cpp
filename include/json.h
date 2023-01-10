#ifndef _JSON_H
#define _JSON_H

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>

#include <typeinfo>
#include <optional>


typedef rapidjson::Document Document;
typedef rapidjson::Value Value;

typedef rapidjson::StringBuffer StringBuffer;
typedef rapidjson::Writer<StringBuffer> StringBufferWriter;



Document from_string(const std::string& json_str);

template<typename T, typename... Keys>
std::optional<Value::ConstMemberIterator> get_json_member(const T& doc, Keys&&... keys)
{
    if (!doc.IsObject())
        return std::nullopt;

    Value::ConstMemberIterator val_it;
    bool first = true;

    for (decltype(auto) key : {keys...})
    {
        if (first)
        {
            first = false;
            val_it = doc.FindMember(key);
            if (val_it == doc.MemberEnd())
                return std::nullopt;
            continue;
        }

        if (!val_it->value.IsObject())
            return std::nullopt;

        Value::ConstMemberIterator new_val = val_it->value.FindMember(key);
        if (new_val == val_it->value.MemberEnd())
        { return std::nullopt; }

        val_it = new_val;
    }

    return first ? std::nullopt : std::optional<Value::ConstMemberIterator>(val_it);
}

template<typename T, typename... Keys>
std::optional<std::string> get_json_string(const T& doc, Keys&&... keys)
{
    if (!doc.IsObject())
        return std::nullopt;

    Value::ConstMemberIterator val_it;
    bool first = true;

    for (decltype(auto) key : {keys...})
    {
        if (first)
        {
            first = false;
            val_it = doc.FindMember(key);
            if (val_it == doc.MemberEnd())
                return std::nullopt;
            continue;
        }

        if (!val_it->value.IsObject())
            return std::nullopt;

        Value::ConstMemberIterator new_val = val_it->value.FindMember(key);
        if (new_val == val_it->value.MemberEnd())
        { return std::nullopt; }

        val_it = new_val;
    }

    if (!first && val_it->value.IsString())
        return std::optional<std::string>(std::in_place, val_it->value.GetString());

    return std::nullopt;
}


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

template <typename T>
    requires std::same_as<T, rapidjson::Document> || std::same_as<T, Value>
struct DocumentCreator{};

template <>
struct DocumentCreator<Document>
{
    Document doc;
    DocumentCreator() : doc{rapidjson::kObjectType}
    {};

    template <typename KeyType>
    DocumentCreator& AddString(KeyType&& key, const char* str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(str, alloc), alloc);
        return *this;
    }

    template <typename KeyType>
    DocumentCreator& AddString(KeyType&& key, const std::string& str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(str.c_str(), str.length(), alloc), alloc);
        return *this;
    }

    template <typename KeyType, typename ValType>
    DocumentCreator& AddMember(KeyType&& key, ValType&& val)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(key, val, alloc);
        return *this;
    }


    inline std::string as_string()
    {
        return to_string(doc);
    }

    void write_string(std::string& dst)
    {
        StringBuffer buffer;
        StringBufferWriter writer(buffer);
        doc.Accept(writer);

        dst = std::string(buffer.GetString());
    }
};


template <>
struct DocumentCreator<Value>
{
    Value value;
    Document& doc;
    DocumentCreator(Document& doc, rapidjson::Type type = rapidjson::kObjectType) : value{type}, doc{doc}
    {};

    template <typename KeyType>
    DocumentCreator& AddString(KeyType&& key, const char* str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(str, alloc), alloc);
        return *this;
    }

    template <typename KeyType>
    DocumentCreator& AddString(KeyType&& key, const std::string& str)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), Value().SetString(str.c_str(), str.length(), alloc), alloc);
        return *this;
    }

    template <typename KeyType, typename ValType>
    DocumentCreator& AddMember(KeyType&& key, ValType&& val)
    {
        static auto& alloc = doc.GetAllocator();
        doc.AddMember(std::forward<KeyType>(key), std::forward<KeyType>(val), alloc);
        return *this;
    }
};


#endif
