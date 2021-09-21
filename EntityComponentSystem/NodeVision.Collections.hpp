#pragma once

#pragma warning(disable:4996)

#include "assert.h"
#include "vector"
#include <string>

namespace NodeVision
{
    namespace Collections
    {
        struct StringSlice
        {
            StringSlice(char* value, int length) : Start(value), End(value + length) {}
            StringSlice(char* start, char* end) : Start(start), End(end) {}

            int Length() const { return End - Start; }

            char* Start;
            char* End;
        };

        template<size_t N>
        struct FixedString
        {
        public:
            FixedString(const char* value)
            {
                assert(strlen(value) <= N);
                strcpy(Data, value);
            }

            FixedString(const char* value, size_t count)
            {
                assert(count <= N);
                memcpy(Data, value, count);
                Data[count] = 0;
            }

            FixedString(std::string value)
            {
                assert(value.size() <= N);
                strcpy(Data, value.c_str());
            }

            FixedString()
            {
                Data[0] = 0;
            }

            bool operator==(const char* value) const
            {
                return strcmp(Data, value) == 0;
            }

            bool operator==(const FixedString& value) const
            {
                return strcmp(Data, value.Data) == 0;
            }

            FixedString& operator +=(const char* value)
            {
                auto length = Length();
                assert(length + strlen(value) <= N);
                strcpy(Data + length, value);
                return *this;
            }

            operator const char*() const { return Data; }

            StringSlice Slice(const char* startSymbol, const char* endSymbol)
            {
                char* valueTextStart = strchr(Data, '#') + 2;
                char* valueTextEnd = strchr(Data, '\n');
                return StringSlice(valueTextStart, valueTextEnd);
            }

            size_t Length() { return strlen(Data); }

        public:
            char Data[N];
        };

        typedef FixedString<256> FixedString256;
        typedef FixedString<128> FixedString128;
        typedef FixedString<64> FixedString64;

        template<class T>
        struct Array
        {
        public:
            Array(int length)
            {
                this->length = length;
                this->data = new T[length];
            }

            void Dispose()
            {
                delete data;
            }

            int Lenght() { return length; }
            bool IsCreated() { return data != nullptr; }

            T& operator [] (int i)
            {
                assert(0 <= i && i < length);
                return data[i];
            }

            template<class N>
            Array<N> Reinterpretate()
            {
                assert((length * sizeof(T)) % (length * sizeof(N)) == 0);
                int newLength = (length * sizeof(T)) / (length * sizeof(N));
                return Array<N>(data, newLength);
            }

        private:
            T* data;
            int length;
        };

        template<class T>
        struct ArraySlice
        {
        public:
            ArraySlice(Array<T> v, int start, int length) :
                data(v.data + start),
                length(length)
            {
            }

            ArraySlice(T* v, int start, int length) :
                data(v + start),
                length(length)
            {
            }

            void Dispose()
            {
                delete data;
            }

            int Length() { return length; }
            bool IsCreated() { return data != nullptr; }

            T& operator [] (int i)
            {
                assert(0 <= i && i < length);
                return data[i];
            }

            bool operator==(const ArraySlice<T>& other) const
            {
                if (length != other.length)
                    return false;
                return memcmp(data, other.data, length) == 0;
            }

            bool operator!=(const ArraySlice<T>& other) const
            {
                if (length == other.length)
                    return false;
                return memcmp(data, other.data, length) != 0;
            }

        public:
            T* data;
            int length;
        };
    }
}