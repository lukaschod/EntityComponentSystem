#pragma once

#include "NodeVision.Entities.hpp"
#include "NodeVision.Serialization.hpp"
#include "NodeVision.Blob.hpp"
#include <iostream>
#include <filesystem>

using namespace NodeVision::Entities;
using namespace NodeVision::Serialization;
using namespace NodeVision::Blob;
namespace fs = std::filesystem;

namespace NodeVision::Assets
{
    Guid GetGuid()
    {
        static int counter = 0;
        Guid guid;
        guid.Value[0] = counter++;
        guid.Value[1] = 0;
        guid.Value[2] = 0;
        guid.Value[3] = 0;
        return guid;
    }

    struct Meta
    {
        template<class Stream>
        void Transfer(Stream& stream)
        {
            stream.Transfer("Guid", this->Guid);
        }

        Guid Guid;
    };

    class AssetSystem : public System
    {
    public:
        using System::System;

        virtual void OnUpdate()
        {
            std::string path = ".";
            Recursive(path.c_str());
        }

        void Recursive(const char* path)
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (entry.is_regular_file())
                    File(entry.path().string());
                else
                    Recursive(entry.path().string().c_str());
            }
        }

        Guid GetGuid()
        {
            static int counter = 0;
            Guid guid;
            guid.Value[0] = counter++;
            guid.Value[1] = 0;
            guid.Value[2] = 0;
            guid.Value[3] = 0;
            return guid;
        }

        void File(std::string path)
        {
            int extensionIndex = path.rfind('.');
            if (extensionIndex == -1)
                return;

            auto extension = path.substr(extensionIndex, path.size());
            if (extension != ".asset")
                return;

            auto metaPath = path + ".meta";

            if (fs::exists(metaPath))
                return;

            Meta meta;
            meta.Guid = GetGuid();

            YamlWriteStream2 stream;
            assert(stream.Open(metaPath.c_str()));

            meta.Transfer(stream);

            assert(stream.Close());
        }
    };

    struct Asset
    {
        Guid Guid;
        FixedString256 GlobalPath;
        FixedString256 GlobalMetaPath;
    };

    struct RequestAssetImport
    {
        int PlaceHolder;
    };

    class ImportSystem;

    class AssetCommandBuffer
    {
    public:
        AssetCommandBuffer() : Offset(0), Count(0) {}

        void SaveAsset(const Guid& guid)
        {
            Write(Command::Import);
            Write(guid);
            Count++;
        }

        void LoadAsset(const char* path, const Guid& guid)
        {
            FixedString256 path2 = path;
            Write(Command::Export);
            Write(path2);
            Write(guid);
            Count++;
        }

        void UpdateAsset(const Guid& guid, const TypeTree& typeTree, byte* ptr)
        {
            Write(Command::Create);
            Write(guid);
            Write(typeTree);
            Write(ptr);
            Count++;
        }

        void Execute(ImportSystem& importSystem);

    private:
        enum class Command
        {
            Export,
            Import,
            Create,
        };

        template<class T>
        void Write(const T& data)
        {
            if (Data.size() - Offset <= sizeof(T))
            {
                int newSize = std::max(sizeof(T) * 2, Data.size() * 2);
                Data.resize(newSize);
            }

            memcpy(Data.data() + Offset, (const char*)&data, sizeof(T));
            Offset += sizeof(T);
        }

        template<class T>
        T& Read()
        {
            T* data = ((T*)(Data.data() + Offset));
            Offset += sizeof(T);
            return *data;
        }

        byte* Read(int length)
        {
            byte* data = Data.data() + Offset;
            Offset += length;
            return data;
        }

        std::vector<byte> Data;
        int Offset;
        int Count;
    };

    static AssetCommandBuffer* g_AssetCommandBuffer = nullptr;
    void SetAssetCommandBuffer(AssetCommandBuffer* assetCommandBuffer)
    {
        g_AssetCommandBuffer = assetCommandBuffer;
    }
    AssetCommandBuffer& GetAssetCommandBuffer()
    {
        assert(g_AssetCommandBuffer != nullptr);
        return *g_AssetCommandBuffer;
    }

    class AssetManager
    {
    public:
        template<class T>
        static void SaveAssetAsync(const char* path, const BlobReference<T>& BlobReference)
        {
            auto& commandBuffer = GetAssetCommandBuffer();
            commandBuffer.LoadAsset(path, BlobReference.Guid);
        }
        template<class T>
        static void LoadAssetAsync(const BlobReference<T>& BlobReference)
        {
            auto& commandBuffer = GetAssetCommandBuffer();
            commandBuffer.SaveAsset(BlobReference.Guid);
        }

        template<class T>
        static BlobReference<T> GetAssetReference(const Guid& guid)
        {
            assert(guid.Valid());

            auto& blobManager = GetBlobManager();
            blobManager.CreateBlob(guid);

            BlobReference<T> reference;
            reference.Guid = guid;

            return reference;
        }
    };

    class ImportSystem : public System
    {
    public:
        using System::System;

        virtual void OnCreate()
        {
            SetAssetCommandBuffer(&m_AssetCommandBuffer);
        }

        void SaveAsset(const FixedString256& path, const Guid& guid)
        {
            YamlWriteStream2 stream;
            if (stream.Open(path.Data))
            {
                auto& blobManager = GetBlobManager();
                assert(blobManager.IsCreated(guid));

                blobManager.TransferBlob(stream, guid);

                stream.Close();
            }

            // todo save meta
            /*Meta meta;
            meta.Guid = guid;

            YamlWriteStream2 stream;
            assert(stream.Open(metaPath.c_str()));

            meta.Transfer(stream);

            assert(stream.Close());*/
        }

        void LoadAsset(const Guid& guid)
        {
            auto& blobManager = GetBlobManager();

            if (blobManager.IsCreated(guid))
                return;

            if (m_GuidToEntity.contains(guid))
            {
                Entity entity = m_GuidToEntity[guid];
                Manager.AddComponentData(entity, RequestAssetImport());
                return;
            }
        }

        void UpdateAsset(const Guid& guid, const TypeTree& typeTree, byte* data)
        {
            auto& blobManager = GetBlobManager();
            blobManager.CreateBlob(guid, typeTree, data);

            assert(m_GuidToEntity.contains(guid));
            Entity entity = m_GuidToEntity[guid];
            //Manager.RemoveComponentData(entity, RequestAssetImport());
        }

        virtual void OnUpdate()
        {
            std::string path = "C:\\Users\\Lukas\\Source\\CppPlayground\\Project\\";
            Recursive(path.c_str());

            m_AssetCommandBuffer.Execute(*this);

            Entities()
                .With<RequestAssetImport>()
                .ForEach([](cwrite(Asset) asset)
                    {
                        auto& blobManager = GetBlobManager();

                        YamlReadStream2 stream;
                        if (stream.Open(asset.GlobalPath.Data))
                        {

                            TypeTree typeTree;
                            stream.Transfer("TypeTree", typeTree);

                            int size;
                            stream.Transfer("Size", size);

                            byte* data = new byte[size];
                            stream.Transfer(typeTree, data, 1);

                            stream.Close();

                            auto& assetCommandBuffer = GetAssetCommandBuffer();
                            assetCommandBuffer.UpdateAsset(asset.Guid, typeTree, data);
                        }
                    }).Run();

            m_AssetCommandBuffer.Execute(*this);
        }

        void Recursive(const char* path)
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (entry.is_regular_file())
                    File(entry.path().string());
                else
                    Recursive(entry.path().string().c_str());
            }
        }

        void File(std::string path)
        {
            int extensionIndex = path.rfind('.');
            if (extensionIndex == -1)
                return;

            auto extension = path.substr(extensionIndex, path.size());
            if (extension != ".meta")
                return;

            Meta meta;

            YamlReadStream2 stream;
            if (stream.Open(path.c_str()))
            {
                meta.Transfer(stream);
                stream.Close();
            }
            else
            {
                return;
            }

            if (m_GuidToEntity.contains(meta.Guid))
                return;

            auto archetype = EntityArchetype({ GetComponentType<Entity>(), GetComponentType<Asset>() });
            auto entity = Manager.CreateEntity(archetype);

            Asset asset;
            asset.Guid = meta.Guid;
            asset.GlobalPath = path.substr(0, extensionIndex);
            asset.GlobalMetaPath = path;

            Manager.SetComponentData(entity, asset);

            m_GuidToEntity[meta.Guid] = entity;
        }

    private:
        AssetCommandBuffer m_AssetCommandBuffer;
        std::map<Guid, Entity> m_GuidToEntity;
    };

    void AssetCommandBuffer::Execute(ImportSystem& importSystem)
    {
        Offset = 0;
        while (Count != 0)
        {
            Command command = Read<Command>();

            switch (command)
            {
            case Command::Export:
            {
                auto& path = Read<FixedString256>();
                auto& guid = Read<Guid>();
                importSystem.SaveAsset(path, guid);
                Count--;
                break;
            }

            case Command::Import:
            {
                auto& guid = Read<Guid>();
                importSystem.LoadAsset(guid);
                Count--;
                break;
            }

            case Command::Create:
            {
                auto& guid = Read<Guid>();
                auto& typeTree = Read<TypeTree>();
                auto& ptr = Read<byte*>();
                importSystem.UpdateAsset(guid, typeTree, ptr);
                Count--;
                break;
            }

            default:
                assert(false);
                break;
            }
        }

        Offset = 0;
        assert(Count == 0);
    }

    template<class T>
    struct AssetReference : BlobReference<T>
    {
        bool IsCreated() const
        {
            if (!BlobReference<T>::IsCreated())
            {
                auto& commandBuffer = GetAssetCommandBuffer();
                commandBuffer.SaveAsset(Guid);
                return false;
            }
            return true;
        }
    };
}