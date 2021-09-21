#pragma once

#include "NodeVision.Serialization.hpp"
#include "NodeVision.Collections.hpp"
#include <map>

namespace NodeVision::Blob
{
    using namespace NodeVision::Serialization;
    using namespace NodeVision::Collections;

    class BlobManager
    {
    public:
        template<class T>
        void CreateBlob(const Guid& guid, const T& data)
        {
            if (Blobs.contains(guid))
            {
                Blob& blob = Blobs[guid];
                blob.Data = new byte[sizeof(T)];
                memcpy(blob.Data, &data, sizeof(T));
                blob.ReferenceCount++;
            }
            else
            {
                Blob blob;
                blob.Data = new byte[sizeof(T)];
                memcpy(blob.Data, &data, sizeof(T));
                blob.ReferenceCount = 0;

                // Create persistent component type
                if constexpr (std::is_base_of<ITest, T>::value)
                {
                    TypeTreeStream stream(blob.TypeTree);
                    blob.TypeTree.Size = sizeof(T);
                    ((T&)data).Transfer(stream);
                }

                Blobs[guid] = blob;
            }
        }

        void CreateBlob(const Guid& guid, const TypeTree& typeTree, byte* data)
        {
            if (Blobs.contains(guid))
            {
                Blob& blob = Blobs[guid];
                blob.Data = data;
                blob.TypeTree = typeTree;
            }
            else
            {
                Blob blob;
                blob.Data = data;
                blob.ReferenceCount = 0;
                blob.TypeTree = typeTree;
                Blobs[guid] = blob;
            }
        }

        void CreateBlob(const Guid& guid)
        {
            if (Blobs.contains(guid))
            {
                Blob& blob = Blobs[guid];
                blob.ReferenceCount++;
            }
            else
            {
                Blob blob;
                blob.Data = nullptr;
                blob.ReferenceCount = 0;
                Blobs[guid] = blob;
            }
        }

        void IncreaseReferenceCount(const Guid& guid)
        {
            assert(Blobs.contains(guid));

            auto& blob = Blobs[guid];
            blob.ReferenceCount++;
            assert(blob.ReferenceCount >= 0);
            //printf("Increase %d\n", blob.ReferenceCount);

        }

        void DecreaseReferenceCount(const Guid& guid)
        {
            assert(Blobs.contains(guid));

            auto& blob = Blobs[guid];
            blob.ReferenceCount--;
            assert(blob.ReferenceCount >= 0);
            //printf("Decrease %d\n", blob.ReferenceCount);

            if (blob.ReferenceCount == 0)
            {
                if (blob.Data != nullptr)
                    delete blob.Data;
                Blobs.erase(guid);
                printf("delete\n");
            }
        }

        template<class T>
        T& GetBlobValue(const Guid& guid)
        {
            assert(Blobs.contains(guid));
            auto& blob = Blobs[guid];
            return *(T*)blob.Data;
        }

        template<class Stream>
        void TransferBlob(Stream& stream, const Guid& guid)
        {
            assert(Blobs.contains(guid));
            auto& blob = Blobs[guid];

            stream.Transfer("TypeTree", blob.TypeTree);
            stream.Transfer("Size", blob.TypeTree.Size);
            stream.Transfer(blob.TypeTree, blob.Data, 1);
        }

        bool IsCreated(const Guid& guid)
        {
            if (!Blobs.contains(guid))
                return false;

            return Blobs[guid].Data != nullptr;
        }

        bool Empty() const { return Blobs.empty(); }
        size_t Count() const { return Blobs.size(); }

    private:
        struct Blob
        {
            TypeTree TypeTree;
            byte* Data;
            int ReferenceCount; // todo atomic
        };

        std::map<Guid, Blob> Blobs;
    };

    static BlobManager* g_BlobManager = nullptr;
    void SetBlobManager(BlobManager* blobManager)
    {
        g_BlobManager = blobManager;
    }
    BlobManager& GetBlobManager()
    {
        assert(g_BlobManager != nullptr);
        return *g_BlobManager;
    }

    static bool g_BlobReferenceState = false;
    struct BlobReferenceScope
    {
        BlobReferenceScope() { g_BlobReferenceState = true; }
        ~BlobReferenceScope() { g_BlobReferenceState = false; }
    };

    template<class T>
    struct BlobReference : IDisposable
    {
        BlobReference() : Guid(0, 0, 0, 0) {}
        /*BlobReference(const Guid& guid) : Guid(guid)
        {
            if (!g_BlobReferenceState || !Guid.Valid())
                return;

            auto& blobManager = GetBlobManager();
            blobManager.CreateBlob(Guid);
        }*/
        BlobReference(const BlobReference& blobReference) : Guid(blobReference.Guid)
        {
            if (!g_BlobReferenceState || !Guid.Valid())
                return;

            auto& blobManager = GetBlobManager();
            blobManager.IncreaseReferenceCount(blobReference.Guid);
        }

        ~BlobReference()
        {
            if (!g_BlobReferenceState || !Guid.Valid())
                return;

            auto& blobManager = GetBlobManager();
            blobManager.DecreaseReferenceCount(Guid);
        }

        T& Value()
        {
            assert(Guid.Valid());
            auto& blobManager = GetBlobManager();
            return blobManager.GetBlobValue<T>(Guid);
        }

        bool IsCreated() const
        {
            if (!Guid.Valid())
                return false;

            auto& blobManager = GetBlobManager();
            return blobManager.IsCreated(Guid);
        }

        void operator=(const BlobReference& blobReference)
        {
            if (!g_BlobReferenceState)
            {
                Guid = blobReference.Guid;
                return;
            }

            auto& blobManager = GetBlobManager();

            if (blobReference.Guid.Valid())
            {
                blobManager.IncreaseReferenceCount(blobReference.Guid);
            }

            if (Guid.Valid())
            {
                blobManager.DecreaseReferenceCount(Guid);
            }

            Guid = blobReference.Guid;
        }

        Guid Guid;
    };

    template<class T>
    class BlobBuilder
    {
    public:
        T& GetRoot() { return Value; }

        BlobReference<T> Build(const Guid& guid)
        {
            auto& blobManager = GetBlobManager();

            blobManager.CreateBlob(guid, Value);

            BlobReference<T> reference;
            reference.Guid = guid;

            return reference;
        }

    private:
        T Value;
    };
}