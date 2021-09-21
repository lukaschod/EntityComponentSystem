#pragma once

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

#include "stdio.h"
#include "string"
#include "assert.h"
#include <charconv>
#include "NodeVision.Collections.hpp"

#define transfer(v) stream.Transfer(#v, v);
#define transfername(n, v) stream.Transfer(n, v);

namespace NodeVision::Serialization
{
    using namespace NodeVision::Collections;

    struct GuidText
    {
        char Data[36];
    };

    struct Guid
    {
    public:
        Guid() {}
        Guid(int data0, int data1, int data2, int data3)
        {
            Value[0] = data0;
            Value[1] = data1;
            Value[2] = data2;
            Value[3] = data3;
        }

        bool operator<(const Guid& other) const
        {
            return Value[0] < other.Value[0];
        }

        bool operator>(const Guid& other) const
        {
            return Value[0] > other.Value[0];
        }

        bool operator==(const Guid& other) const
        {
            return memcmp(Value, other.Value, sizeof(int) * 4) == 0;
        }

        bool Valid() const { return Value[0] != 0 && Value[1] != 0 && Value[2] != 0 && Value[3] != 0; }

    public:
        int Value[4];
    };

    class ISerializable;

    struct TypeTree
    {
        enum class Type
        {
            Undefined,
            Structure,
            Array,
            Integer,
            Float,
            Boolean,
        };

        struct Field
        {
            Field(const char* name, Type type) : Name(name), Type(type) {}
            Field() : Name(""), Type(Type::Undefined) {}

            FixedString256 Name;
            Type Type;
        };

        FixedString256 Name;
        std::vector<Field> Fields;
        int Size;
    };

    class TypeTreeStream
    {
    public:
        TypeTreeStream(TypeTree& typeTree) : TypeTree(typeTree)
        {
        }

        bool IsRead() { return false; }

        void Transfer(const char* name, float& value)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Float));
        }
        void Transfer(const char* name, int& value)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Integer));
        }
        void Transfer(const char* name, bool& value)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Boolean));
        }
        void Transfer(const char* name, ISerializable& value)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Structure));
        }
        void Transfer(const char* name, int* value, int length)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Array));
        }
        void Transfer(const char* name, ISerializable* value, int stride, int length)
        {
            TypeTree.Fields.push_back(TypeTree::Field(name, TypeTree::Type::Array));
        }

        TypeTree& TypeTree;
    };

    class YamlWriteStream2
    {
    public:
        bool Open(const char* path)
        {
            file = fopen(path, "w");
            indent = 0;
            return file != 0;
        }

        bool Close()
        {
            return fclose(file) == 0;
        }

        bool IsRead() { return false; }

        void Transfer(const char* name, float& value)
        {
            Indent();
            auto valueToString = std::to_string(value);
            fprintf(file, "%s: %s\n", name, valueToString.c_str());
        }

        void Transfer(const char* name, int& value)
        {
            Indent();
            auto valueToString = std::to_string(value);
            fprintf(file, "%s: %s\n", name, valueToString.c_str());
        }

        void Transfer(const char* name, bool& value)
        {
            int temp = value;
            Transfer(name, temp);
        }

        void Transfer(const char* name, FixedString256& value)
        {
            Indent();
            fprintf(file, "%s: %s\n", name, value.Data);
        }

        void Transfer(const char* name, Guid& value)
        {
            Indent();
            auto valueToString0 = std::to_string(value.Value[0]);
            auto valueToString1 = std::to_string(value.Value[1]);
            auto valueToString2 = std::to_string(value.Value[2]);
            auto valueToString3 = std::to_string(value.Value[3]);
            fprintf(file, "%s: %s %s %s %s\n", name, valueToString0.c_str(), valueToString1.c_str(), valueToString2.c_str(), valueToString3.c_str());
        }

        void Transfer(const char* name, TypeTree& typeTree)
        {
            Indent();

            auto& fields = typeTree.Fields;

            auto valueToString = std::to_string(fields.size());

            fprintf(file, "%s: \# %s\n", name, valueToString.c_str());
            indent++;
            for (int i = 0; i < fields.size(); ++i)
            {
                isArray = true;

                switch (fields[i].Type)
                {
                case TypeTree::Type::Integer:
                    Transfer("Name", fields[i].Name);
                    Indent();
                    fprintf(file, "Type: Integer\n");
                    break;
                case TypeTree::Type::Float:
                    Transfer("Name", fields[i].Name);
                    Indent();
                    fprintf(file, "Type: Float\n");
                    break;
                case TypeTree::Type::Boolean:
                    Transfer("Name", fields[i].Name);
                    Indent();
                    fprintf(file, "Type: Boolean\n");
                    break;
                default:
                    assert(false);
                    break;
                }
            }
            indent--;
        }

        void Transfer(const TypeTree& typeTree, byte* data, int length)
        {
            Indent();

            auto& fields = typeTree.Fields;
            auto ptr = (char*)data;

            const char* name = "NoName";
            const char* shortName = strchr(typeTree.Name.Data, ' ');
            if (shortName != 0)
                name = shortName + 1;
            shortName = strrchr(typeTree.Name.Data, ':');
            if (shortName != 0)
                name = shortName + 1;

            fprintf(file, "%s:\n", name);
            indent++;
            for (int j = 0; j < length; ++j)
            {
                isArray = true;
                byte* fieldPtr = ptr;
                for (int i = 0; i < fields.size(); ++i)
                {
                    switch (fields[i].Type)
                    {
                    case TypeTree::Type::Integer:
                        Transfer(fields[i].Name.Data, *((int*)fieldPtr));
                        fieldPtr += sizeof(int);
                        break;
                    case TypeTree::Type::Float:
                        Transfer(fields[i].Name.Data, *((float*)fieldPtr));
                        fieldPtr += sizeof(float);
                        break;
                    case TypeTree::Type::Boolean:
                        Transfer(fields[i].Name.Data, *((int*)fieldPtr));
                        fieldPtr += sizeof(int);
                        break;
                    default:
                        assert(false);
                        break;
                    }
                }
                ptr += typeTree.Size;
            }
            indent--;
        }

        void Transfer(const char* name, std::stack<int>& value)
        {
            Indent();

            int* ptr = value.empty() ? nullptr : &value.top();

            auto valueToString = std::to_string(value.size());

            fprintf(file, "%s: \# %s\n", name, valueToString.c_str());
            indent++;
            for (int i = 0; i < value.size(); ++i)
            {
                valueToString = std::to_string(ptr[i]);
                Indent();
                fprintf(file, "- %s\n", valueToString.c_str());
            }
            indent--;
        }

        void Transfer(const char* name, std::vector<int>& value)
        {
            Indent();

            fprintf(file, "%s: [", name);
            for (int i = 0; i < value.size(); ++i)
            {
                auto valueToString = std::to_string(value[i]);
                fprintf(file, "%s, ", valueToString.c_str());
            }
            fprintf(file, "]\n");
        }

        template<class T>
        void Transfer(const char* name, std::vector<T>& value)
        {
            Indent();

            auto valueToString = std::to_string(value.size());

            fprintf(file, "%s: \# %s\n", name, valueToString.c_str());
            indent++;
            for (int i = 0; i < value.size(); ++i)
            {
                isArray = true;
                value[i].Transfer(*this);
            }
            indent--;
        }

        template<class T>
        void Transfer(const char* name, T& value)
        {
            Indent();
            fprintf(file, "%s:\n", name);
            indent++;
            value.Transfer(*this);
            indent--;
        }

    private:
        void Indent()
        {
            for (int i = 0; i < indent; ++i)
            {
                if (isArray && i == indent - 1)
                {
                    fwrite("- ", 2, 1, file);
                    isArray = false;
                }
                else
                    fwrite("  ", 2, 1, file);
            }
        }

    private:
        FILE* file;
        int indent;
        bool isArray;
    };

    class YamlReadStream2
    {
    public:
        bool Open(const char* path)
        {
            auto result = fopen_s(&file, path, "r");
            return result == 0;
        }

        bool Close()
        {
            return fclose(file) == 0;
        }

        bool IsRead() { return true; }

        void Transfer(const char* name, float& value)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            const char* valueTextStart = strchr(buffer, ':') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, value);
        }

        void Transfer(const char* name, int& value)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            const char* valueTextStart = strchr(buffer, ':') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, value);
        }

        void Transfer(const char* name, bool& value)
        {
            int temp;
            Transfer(name, temp);
            value = temp;
        }

        void Transfer(const char* name, FixedString256& value)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            const char* valueTextStart = strchr(buffer, ':') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');

            memcpy(value.Data, valueTextStart, valueTextEnd - valueTextStart);
            value.Data[valueTextEnd - valueTextStart] = 0;
        }

        void Transfer(const char* name, Guid& value)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            const char* valueTextStart = strchr(buffer, ':') + 2;
            const char* valueTextEnd = strchr(valueTextStart, ' ');
            std::from_chars(valueTextStart, valueTextEnd, value.Value[0]);

            valueTextStart = valueTextEnd + 1;
            valueTextEnd = strchr(valueTextStart, ' ');
            std::from_chars(valueTextStart, valueTextEnd, value.Value[1]);

            valueTextStart = valueTextEnd + 1;
            valueTextEnd = strchr(valueTextStart, ' ');
            std::from_chars(valueTextStart, valueTextEnd, value.Value[2]);

            valueTextStart = valueTextEnd + 1;
            valueTextEnd = strchr(valueTextStart, '\n');
            std::from_chars(valueTextStart, valueTextEnd, value.Value[3]);
        }

        void Transfer(const char* name, TypeTree& typeTree)
        {
            char buffer[256];

            int size;
            fgets(buffer, 256, file);
            char* valueTextStart = strchr(buffer, '#') + 2;
            char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, size);

            auto& fields = typeTree.Fields;

            fields.resize(size);

            for (int i = 0; i < fields.size(); ++i)
            {
                Transfer("Name", fields[i].Name);

                FixedString256 type;
                Transfer("Type", type);

                if (type == "Integer")
                {
                    fields[i].Type = TypeTree::Type::Integer;
                }

                if (type == "Float")
                {
                    fields[i].Type = TypeTree::Type::Float;
                }

                if (type == "Boolean")
                {
                    fields[i].Type = TypeTree::Type::Boolean;
                }
            }
        }

        void Transfer(const TypeTree& typeTree, byte* data, int length)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            auto& fields = typeTree.Fields;
            byte* ptr = (byte*)data;

            for (int j = 0; j < length; ++j)
            {
                for (int i = 0; i < fields.size(); ++i)
                {
                    switch (fields[i].Type)
                    {
                    case TypeTree::Type::Integer:
                        Transfer("", *(int*)ptr);
                        ptr += sizeof(int);
                        break;
                    case TypeTree::Type::Float:
                        Transfer("", *(float*)ptr);
                        ptr += sizeof(float);
                        break;
                    case TypeTree::Type::Boolean:
                        Transfer("", *(int*)ptr);
                        ptr += sizeof(int);
                        break;
                    default:
                        assert(false);
                        break;
                    }
                }
                //ptr += typeTree.Size;
            }
        }

        void Transfer(const char* name, std::stack<int>& value)
        {
            //char buffer[256];
            //fgets(buffer, 256, file);

            char buffer[256];
            fgets(buffer, 256, file);

            int size;
            const char* valueTextStart = strchr(buffer, '#') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, size);

            valueTextStart = valueTextEnd;
            for (int i = 0; i < size; ++i)
            {
                fgets(buffer, 256, file);

                valueTextStart = strchr(buffer, '-') + 2;
                valueTextEnd = strchr(valueTextStart, '\n');
                int v;
                std::from_chars(valueTextStart, valueTextEnd, v);
                value.push(v);
            }
            fprintf(file, "]\n");
        }

        void Transfer(const char* name, std::vector<int>& value)
        {
            char buffer[256];
            fgets(buffer, 256, file);

            int size;
            const char* valueTextStart = strchr(buffer, '#') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, size);

            value.resize(size);

            valueTextStart = strchr(buffer, '[') + 2;
            for (int i = 0; i < value.size(); ++i)
            {
                valueTextEnd = strchr(valueTextStart, ' ');
                if (valueTextEnd == nullptr)

                std::from_chars(valueTextStart, valueTextEnd, value[0]);
                valueTextStart = valueTextEnd;
            }
            fprintf(file, "]\n");
        }

        template<class T>
        void Transfer(const char* name, std::vector<T>& value)
        {
            char buffer[256];

            int size;
            fgets(buffer, 256, file);
            const char* valueTextStart = strchr(buffer, '#') + 2;
            const char* valueTextEnd = strchr(buffer, '\n');
            std::from_chars(valueTextStart, valueTextEnd, size);

            value.resize(size);

            for (int i = 0; i < value.size(); ++i)
            {
                value[i].Transfer(*this);
            }
        }

        template<class T>
        void Transfer(const char* name, T& value)
        {
            char buffer[256];

            // skip name
            fgets(buffer, 256, file);

            value.Transfer(*this);
        }

    private:
        FILE* file;
    };

    struct ITest
    {

    };

    template<int G>
    struct IPersistent : ITest
    {
        inline static Guid Id = Guid(G, G, G, G);
    };

    struct IDisposable {};
}