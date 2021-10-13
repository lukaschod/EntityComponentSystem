#pragma once

#include "stdio.h"
#include "string"
#include "assert.h"
#include "vector"
#include <algorithm>
#include <map>
#include <typeinfo>
#include <typeindex>
#include <stack>
#include <array>
#include <functional>
#include "NodeVision.Core.hpp"
#include "NodeVision.Profiling.h"
#include "NodeVision.Collections.hpp"
#include "NodeVision.Serialization.hpp"
#include "NodeVision.Blob.hpp"
#include "NodeVision.Jobs.hpp"
#include "NodeVision.Entities.ForEach.hpp"

#define cwrite(type) type& avoid_alias
#define cread(type) const type& avoid_alias

namespace NodeVision::Entities
{
    using namespace Collections;
    using namespace Serialization;
    using namespace Blob;
    using namespace Jobs;

    static std::map<Guid, TypeTree> componentTypeTrees;
    static std::map<Guid, int> componentTypeIndices;
    static int typeIndexCounter = 0;

    typedef void (*ComponentDispose)(void*);
    static std::map<Guid, ComponentDispose> componentTypeDisposes;

    struct ComponentType
    {
        bool operator==(const ComponentType& other) const
        {
            return TypeIndex == other.TypeIndex;
        }

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(Guid);
            transfer(Size);
            if (stream.IsRead())
            {
                TypeTree typeTree;
                stream.Transfer("TypeTree", typeTree);

                Dispose = nullptr;

                if (!componentTypeIndices.contains(Guid))
                {
                    TypeIndex = typeIndexCounter++;
                    componentTypeIndices[Guid] = TypeIndex;

                    assert(!componentTypeTrees.contains(Guid));
                    componentTypeTrees[Guid] = typeTree;

                    Dispose = nullptr;
                }
                else
                {
                    TypeIndex = componentTypeIndices[Guid];
                    Dispose = componentTypeDisposes[Guid];
                }
            }
            else
            {
                assert(componentTypeTrees.contains(Guid));
                stream.Transfer("TypeTree", componentTypeTrees[Guid]);
            }
        }

        TypeTree& GetTypeTree() const
        {
            assert(componentTypeTrees.contains(Guid));
            return componentTypeTrees[Guid];
        }

        Guid Guid;
        int TypeIndex;
        int Size;
        ComponentDispose Dispose;
    };

    static std::map<std::type_index, ComponentType> componentTypes;

#define typeof(Type) GetComponentType<Type>()

    template<class T>
    static ComponentType GetComponentType()
    {
        const type_info& type = typeid(T);

        if (!componentTypes.contains(type))
        {
            Guid guid;

            // Check if persistent component type is created
            if constexpr (std::is_base_of<ITest, T>::value)
            {
                guid = T::Id;

                if (componentTypeIndices.contains(guid))
                {
                    assert(componentTypeTrees.contains(guid));
                    auto componentType = ComponentType();
                    componentType.TypeIndex = componentTypeIndices[guid];
                    componentType.Size = componentTypeTrees[guid].Size;
                    componentType.Guid = guid;
                    componentType.Dispose = componentTypeDisposes[guid];
                    componentTypes[type] = componentType;
                    return componentType;
                }
            }

            // Create component type
            auto componentType = ComponentType();
            componentType.TypeIndex = typeIndexCounter++;
            componentType.Size = sizeof(T);
            componentType.Guid = guid;

            // Create persistent component type
            if constexpr (std::is_base_of<ITest, T>::value)
            {
                TypeTree typeTree;
                typeTree.Name = type.name();

                TypeTreeStream stream(typeTree);
                T dummy;
                dummy.Transfer(stream);

                typeTree.Size = sizeof(T);
                componentTypeTrees[guid] = typeTree;
                componentTypeIndices[guid] = componentType.TypeIndex;
                componentTypeDisposes[guid] = componentType.Dispose;
            }

            // Add dispose
            if constexpr (std::is_base_of<IDisposable, T>::value)
            {
                componentType.Dispose = [](void* ptr) { ((T*)ptr)->~T(); };
            }
            else
            {
                componentType.Dispose = nullptr;
            }

            componentTypes[type] = componentType;
        }

        return componentTypes[type];
    }

    struct ArchetypeMask2
    {
        ArchetypeMask2() {}

        ArchetypeMask2(const std::vector<ComponentType>& componentTypes)
        {
            for (auto& componentType : componentTypes)
            {
                Enable(componentType);
            }
        }

        void Enable(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            if (index >= Bits.size())
            {
                Bits.resize(std::max(index * 2, 8), 0);
            }

            Bits[index] |= mask;
        }

        void Disable(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            if (index >= Bits.size())
            {
                Bits.resize(std::max(index * 2, 8), 0);
            }

            Bits[index] &= ~mask;
        }

        bool Contains(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            if (index >= Bits.size())
            {
                Bits.resize(std::max(index * 2, 8), 0);
            }

            return (Bits[index] & mask) != 0;
        }

        bool Contains(const ArchetypeMask2& archetypeMask) const
        {
            if (archetypeMask.Bits.size() > Bits.size())
                return false;

            for (int i = 0; i < archetypeMask.Bits.size(); ++i)
            {
                if ((Bits[i] & archetypeMask.Bits[i]) != archetypeMask.Bits[i])
                    return false;
            }
            return true;
        }

        bool operator==(const ArchetypeMask2& other) const
        {
            if (other.Bits.size() != Bits.size())
                return false;
            if (memcmp(Bits.data(), other.Bits.data(), Bits.size() * sizeof(int)) != 0)
                return false;
            return true;
        }

        void operator=(const ArchetypeMask2& other)
        {
            if (Bits.size() < other.Bits.size())
                Bits.resize(other.Bits.size());

             memcpy(Bits.data(), other.Bits.data(), other.Bits.size() * sizeof(int));

             // If current is bigger we have to set it to zero
             if (Bits.size() > other.Bits.size())
                memset(Bits.data() + Bits.size(), 0, (other.Bits.size() - Bits.size()) * sizeof(int));
        }

        std::vector<int> Bits;
    };

    template<int N>
    struct ArchetypeFixedMask
    {
        ArchetypeFixedMask()
        { 
            Bits.fill(0);
        }

        ArchetypeFixedMask(const std::vector<ComponentType>& componentTypes)
        {
            Bits.fill(0);
            for (auto& componentType : componentTypes)
            {
                Enable(componentType);
            }
        }

        void Enable(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            Bits[index] |= mask;
        }

        void Disable(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            Bits[index] &= ~mask;
        }

        bool Contains(const ComponentType& componentType)
        {
            int index = componentType.TypeIndex / 32;
            int mask = 1 << (componentType.TypeIndex % 32);

            return (Bits[index] & mask) != 0;
        }

        bool Contains(const ArchetypeFixedMask& archetypeMask) const
        {
            for (int i = 0; i < N; ++i)
            {
                if ((Bits[i] & archetypeMask.Bits[i]) != archetypeMask.Bits[i])
                    return false;
            }
            return true;
        }

        bool operator==(const ArchetypeFixedMask& other) const
        {
            return memcmp(Bits.data(), other.Bits.data(), N * sizeof(int)) == 0;
        }

        void operator=(const ArchetypeFixedMask& other)
        {
            memcpy(Bits.data(), other.Bits.data(), sizeof(int) * N);
        }

        std::array<int, N> Bits;
    };

    typedef ArchetypeFixedMask<64> ArchetypeMask;

    struct Entity : IPersistent<2>
    {
        Entity() : Index(0), Version(0) {}
        Entity(int index, int version) : Index(index), Version(version) {}

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(Index);
            transfer(Version);
        }

        int Index;
        int Version;
    };

    struct EntityArchetype
    {
        EntityArchetype() : Size(0) {}
        EntityArchetype(const std::vector<ComponentType>& componentTypes, bool expermental = false) :
            Mask(componentTypes),
            ComponentTypes(componentTypes),
            Size(0)
        {
            for (auto& componentType : componentTypes)
            {
                Size += componentType.Size;
            }
            if (expermental)
            {
                Size += sizeof(Entity);
            }
            Expermetal = expermental;
        }

        EntityArchetype(std::initializer_list<ComponentType> componentTypes, bool expermental = false) :
            Mask(componentTypes),
            ComponentTypes(componentTypes),
            Size(0)
        {
            for (auto& componentType : componentTypes)
            {
                Size += componentType.Size;
            }
            if (expermental)
            {
                Size += sizeof(Entity);
            }
            Expermetal = expermental;
        }

        bool operator==(const EntityArchetype& other) const
        {
            return Mask == other.Mask && Size == other.Size;
        }

        bool Contains(const ComponentType& componentType)
        {
            return Mask.Contains(componentType);
        }

        int GetOffset(const ComponentType& componentType) const
        {
            int offset = 0;
            if (Expermetal)
                offset = sizeof(Entity);
            for (const auto& item : ComponentTypes)
            {
                if (item.TypeIndex == componentType.TypeIndex)
                    return offset;
                offset += item.Size;
            }
            assert(false);
            return -1;
        }

        int GetIndex(const ComponentType& componentType) const
        {
            int offset = 0;
            for (const auto& item : ComponentTypes)
            {
                if (item.TypeIndex == componentType.TypeIndex)
                    return offset;
                offset++;
            }
            assert(false);
            return -1;
        }

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(ComponentTypes);
            transfer(Size);
            transfer(Expermetal);
            if (stream.IsRead())
            {
                Mask = ArchetypeMask(ComponentTypes);
            }
        }

        std::vector<ComponentType> ComponentTypes;
        ArchetypeMask Mask;
        int Size;
        bool Expermetal;
    };

    template<class T>
    struct ComponentArraySlice : public ArraySlice<T>
    {
        ComponentArraySlice() : ArraySlice<T>(nullptr, 0, 0) {}
        ComponentArraySlice(T* v, int start, int length, JobHandle* jobHandle, JobHandle* readHandle) :
            ArraySlice<T>(v, start, length),
            Handle(jobHandle),
            ReadOHandle(readHandle)
        {
        }

        JobHandle* Handle;
        JobHandle* ReadOHandle;
    };

    class ArchetypeChunk
    {
    public:
        ArchetypeChunk() : Count(0), Capacity(0) {}
        ArchetypeChunk(const EntityArchetype& archetype, int size) :
            Archetype(archetype),
            Count(0)
        {
            Capacity = size / Archetype.Size;
            Data.resize(size);
            for (auto& componentType : archetype.ComponentTypes)
            {
                JobHandle jobHandle;
                jobHandle.Index = 0;
                jobHandle.Version = 0;
                ComponentJobHandles.push_back(jobHandle);
                ComponentReadHandles.push_back(jobHandle);
            }
        }

        int PushBack()
        {
            assert(!IsFull());
            return Count++;
        }

        void RemoveAtSwapBack(int arrayIndex)
        {
            BlobReferenceScope blobReferenceScope;
            int archetypeOffset = 0;
            if (Archetype.Expermetal)
                archetypeOffset = sizeof(Entity);
            for (auto& componentType : Archetype.ComponentTypes)
            {
                int typeSize = componentType.Size;
                int chunkOffset = Capacity * archetypeOffset;

                char* destination = Data.data() + chunkOffset + (arrayIndex * typeSize);
                char* source = Data.data() + chunkOffset + ((Count - 1) * typeSize);

                if (componentType.Dispose != nullptr)
                {
                    componentType.Dispose(destination);
                }

                memcpy((void*)destination, (void*)source, typeSize);

                archetypeOffset += typeSize;
            }

            Count--;
        }

        ArraySlice<byte> GetComponents(const ComponentType& componentType) const
        {
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            return ArraySlice<byte>((byte*)Data.data() + chunkOffset, 0, Count);
        }

        template<class C>
        ArraySlice<C> GetComponents()
        {
            auto componentType = GetComponentType<C>();

            assert(Archetype.Contains(componentType));
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            return ArraySlice<C>((C*)(Data.data() + chunkOffset), 0, Count);
        }

        ArraySlice<Entity> GetEntities()
        {
            int archetypeOffset = 0;
            int chunkOffset = Capacity * archetypeOffset;

            return ArraySlice<Entity>((Entity*)(Data.data() + chunkOffset), 0, Count);
        }

        ComponentArraySlice<Entity> GetEntitiesForJob()
        {
            int archetypeOffset = 0;
            int chunkOffset = Capacity * archetypeOffset;

            return ComponentArraySlice<Entity>((Entity*)(Data.data() + chunkOffset), 0, Count, nullptr, nullptr);
        }

        template<class C>
        ComponentArraySlice<C> GetComponentsForJob()
        {
            profile_function;

            auto componentType = GetComponentType<C>();

            assert(Archetype.Contains(componentType));
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            int archetypeComponentIndex = Archetype.GetIndex(componentType);

            return ComponentArraySlice<C>((C*)(Data.data() + chunkOffset), 0, Count, 
                &ComponentJobHandles[archetypeComponentIndex],
                &ComponentReadHandles[archetypeComponentIndex]);
        }

        template<class C>
        ComponentArraySlice<const C& __restrict> GetComponentsForJobRead()
        {
            profile_function;

            auto componentType = GetComponentType<C>();

            assert(Archetype.Contains(componentType));
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            int archetypeComponentIndex = Archetype.GetIndex(componentType);

            return ComponentArraySlice<C>((C*)(Data.data() + chunkOffset), 0, Count,
                &ComponentJobHandles[archetypeComponentIndex],
                &ComponentReadHandles[archetypeComponentIndex]);
        }

        template<class C>
        ComponentArraySlice<C& __restrict> GetComponentsForJobRead()
        {
            profile_function;

            auto componentType = GetComponentType<C>();

            assert(Archetype.Contains(componentType));
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            int archetypeComponentIndex = Archetype.GetIndex(componentType);

            return ComponentArraySlice<C>((C*)(Data.data() + chunkOffset), 0, Count,
                &ComponentJobHandles[archetypeComponentIndex],
                &ComponentReadHandles[archetypeComponentIndex]);
        }

        byte* GetComponentData(const ComponentType& componentType, int arrayIndex)
        {
            assert(Archetype.Contains(componentType));
            int archetypeOffset = Archetype.GetOffset(componentType);
            int chunkOffset = Capacity * archetypeOffset;

            byte* component = ((Data.data() + chunkOffset) + (arrayIndex * componentType.Size));

            return component;
        }

        template<class T>
        T& GetComponentData(int arrayIndex)
        {
            auto componentType = GetComponentType<T>();
            return *(T*)GetComponentData(componentType, arrayIndex);
        }

        template<class T>
        void SetComponentData(int arrayIndex, const T& data)
        {
            /*auto componentType = GetComponentType<T>();
            byte* dst = GetComponentData(componentType, arrayIndex);
            memcpy(dst, (byte*)&data, sizeof(T));*/
            BlobReferenceScope blobReferenceScope;
            GetComponentData<T>(arrayIndex) = data;
        }

        void SetComponentData(const ComponentType& componentType, int arrayIndex, byte* data)
        {
            byte* dst = GetComponentData(componentType, arrayIndex);
            memcpy(dst, data, componentType.Size);
        }

        bool IsFull() { return Count == Capacity; }

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(Archetype);
            transfer(Count);
            transfer(Capacity);
            if (stream.IsRead())
            {
                Data.resize(Capacity * Archetype.Size);
            }
            for (auto& componentType : Archetype.ComponentTypes)
            {
                ArraySlice<byte> slice = GetComponents(componentType);

                auto& typeTree = componentType.GetTypeTree();
                stream.Transfer(typeTree, slice.data, Count);
            }
        }

        bool operator==(const ArchetypeChunk& other) const
        {
            if (Archetype != other.Archetype)
                return false;

            for (auto& componentType : Archetype.ComponentTypes)
            {
                ArraySlice<byte> slice = GetComponents(componentType);
                ArraySlice<byte> sliceOther = other.GetComponents(componentType);

                if (slice != sliceOther)
                    return false;
            }
            return Count == other.Count && Capacity == other.Capacity;
        }

        bool operator!=(const ArchetypeChunk& other) const { return !(operator==(other)); }

        EntityArchetype Archetype;
        std::vector<char> Data;
        std::vector<JobHandle> ComponentJobHandles;
        std::vector<JobHandle> ComponentReadHandles;
        int Count;
        int Capacity;
    };

    struct IComponent {};

    struct EntityIndexer
    {
        struct Instance
        {
            template<class Stream>
            void Transfer(Stream& stream)
            {
                transfer(ChunkIndex);
                transfer(ArrayIndex);
                transfer(Version);
            }

            int ChunkIndex;
            int ArrayIndex;
            int Version;
        };

        Entity CreateEntity(int chunkIndex, int arrayIndex)
        {
            // Try to get from free
            if (!Free.empty())
            {
                int entityIndex = Free.top();
                Free.pop();

                Instance& instance = Allocated[entityIndex];
                instance.ChunkIndex = chunkIndex;
                instance.ArrayIndex = arrayIndex;
                instance.Version++;
                return Entity(entityIndex, instance.Version);
            }
            else
            {
                // Get new
                int entityIndex = Allocated.size();
                Instance instance;
                instance.ChunkIndex = chunkIndex;
                instance.ArrayIndex = arrayIndex;
                instance.Version = 0;

                Allocated.push_back(instance);

                return Entity(entityIndex, instance.Version);
            }
        }

        bool IsValid(Entity entity)
        {
            return entity.Version == Allocated[entity.Index].Version;
        }

        void DestroyEntity(Entity entity)
        {
            Instance& instance = Allocated[entity.Index];
            if (instance.Version == entity.Version)
            {
                instance.Version++;
                Free.push(entity.Index);
            }
        }

        int GetChunkIndex(Entity entity)
        {
            assert(IsValid(entity));
            return Allocated[entity.Index].ChunkIndex;
        }

        int GetArrayIndex(Entity entity)
        {
            assert(IsValid(entity));
            return Allocated[entity.Index].ArrayIndex;
        }

        void SetChunkIndex(Entity entity, int chunkIndex)
        {
            assert(IsValid(entity));
            Allocated[entity.Index].ChunkIndex = chunkIndex;
        }

        void SetArrayIndex(Entity entity, int arrayIndex)
        {
            assert(IsValid(entity));
            Allocated[entity.Index].ArrayIndex = arrayIndex;
        }

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(Allocated);
            transfer(Free);
        }

        bool operator==(const EntityIndexer& other) const
        {
            if (Allocated.size() != other.Allocated.size())
                return false;
            if (memcmp(Allocated.data(), other.Allocated.data(), Allocated.size() * sizeof(Instance)) != 0)
                return false;

            if (Free.size() != other.Free.size())
                return false;
            if (Free.size() != 0 && memcmp((int*)&Free.top(), (int*)&other.Free.top(), Free.size() * sizeof(int)) != 0)
                return false;

            return true;
        }

        bool operator!=(const EntityIndexer& other) const { return !(operator==(other)); }

        std::vector<Instance> Allocated;
        std::stack<int> Free;
    };

    class EntityCommandBuffer;

    class EntityManager
    {
    public:
        ~EntityManager()
        {
        }

        bool operator==(const EntityManager& other) const
        {
            if (Chunks.size() != other.Chunks.size())
                return false;
            for (int i = 0; i < Chunks.size(); ++i)
            {
                if (Chunks[i] != other.Chunks[i])
                    return false;
            }
            return Indexer == other.Indexer;
        }

        bool operator!=(const EntityManager& other) const { return !(operator==(other)); }

        EntityArchetype CreateArchetype(std::initializer_list<ComponentType> components)
        {
            profile_function;

            return EntityArchetype(components, true);
        }

        Entity CreateEntity(const EntityArchetype& archetype)
        {
            //profile_function;

            int chunkIndex = GetOrCreateChunk(archetype);
            auto& chunk = Chunks[chunkIndex];

            int arrayIndex = chunk.PushBack();
            Entity entity = Indexer.CreateEntity(chunkIndex, arrayIndex);

            if (chunk.Archetype.Expermetal)
            {
                chunk.GetEntities()[arrayIndex] = entity;
            }
            else
            {
                SetComponentData(entity, entity);
            }

            return entity;
        }

        void DestroyEntity(Entity entity)
        {
            profile_function;

            if (!Indexer.IsValid(entity))
                return;

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            auto& chunk = Chunks[chunkIndex];

            // Update entity at swapback
            int swapBackArrayIndex = chunk.Count - 1;
            if (arrayIndex != swapBackArrayIndex)
            {
                Entity swapBackEntity;// = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                if (chunk.Archetype.Expermetal)
                {
                    swapBackEntity = chunk.GetEntities()[swapBackArrayIndex];
                }
                else
                {
                    swapBackEntity = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                }
                Indexer.SetArrayIndex(swapBackEntity, arrayIndex);
            }
            chunk.RemoveAtSwapBack(arrayIndex);

            Indexer.DestroyEntity(entity);
        }

        template<class T>
        void AddComponentData(Entity entity, const T& data)
        {
            auto newComponentType = GetComponentType<T>();
            AddComponentData(entity, newComponentType, (byte*)&data);
        }

        void AddComponentData(Entity entity, const ComponentType& componentType, byte* data)
        {
            profile_function;

            if (!Indexer.IsValid(entity))
                return;

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            int newChunkIndex = GetOrCreateChunkWithComponent(Chunks[chunkIndex].Archetype, componentType);

            auto& newChunk = Chunks[newChunkIndex];
            int newArrayIndex = newChunk.PushBack();

            auto& chunk = Chunks[chunkIndex];

            for (const auto& componentType : chunk.Archetype.ComponentTypes)
            {
                newChunk.SetComponentData(componentType, newArrayIndex, chunk.GetComponentData(componentType, arrayIndex));
            }

            // Update entity at swapback
            int swapBackArrayIndex = chunk.Count - 1;
            if (arrayIndex != swapBackArrayIndex)
            {
                Entity swapBackEntity;// = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                if (chunk.Archetype.Expermetal)
                {
                    swapBackEntity = chunk.GetEntities()[swapBackArrayIndex];
                }
                else
                {
                    swapBackEntity = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                }
                Indexer.SetArrayIndex(swapBackEntity, arrayIndex);
            }
            chunk.RemoveAtSwapBack(arrayIndex);

            Indexer.SetChunkIndex(entity, newChunkIndex);
            Indexer.SetArrayIndex(entity, newArrayIndex);

            newChunk.SetComponentData(componentType, newArrayIndex, data);
        }

        template<class T>
        void RemoveComponent(Entity entity)
        {
            auto newComponentType = GetComponentType<T>();
            RemoveComponent(entity, newComponentType);
        }

        void RemoveComponent(Entity entity, const ComponentType& componentType)
        {
            profile_function;

            if (!Indexer.IsValid(entity))
                return;

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            int newChunkIndex = GetOrCreateChunkWithoutComponent(Chunks[chunkIndex].Archetype, componentType);

            auto& newChunk = Chunks[newChunkIndex];
            int newArrayIndex = newChunk.PushBack();

            auto& chunk = Chunks[chunkIndex];

            for (const auto& componentType : newChunk.Archetype.ComponentTypes)
            {
                newChunk.SetComponentData(componentType, newArrayIndex, chunk.GetComponentData(componentType, arrayIndex));
            }

            // Update entity at swapback
            int swapBackArrayIndex = chunk.Count - 1;
            if (arrayIndex != swapBackArrayIndex)
            {
                Entity swapBackEntity;// = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                if (chunk.Archetype.Expermetal)
                {
                    swapBackEntity = chunk.GetEntities()[swapBackArrayIndex];
                }
                else
                {
                    swapBackEntity = chunk.GetComponentData<Entity>(swapBackArrayIndex);
                }
                Indexer.SetArrayIndex(swapBackEntity, arrayIndex);
            }
            chunk.RemoveAtSwapBack(arrayIndex);

            Indexer.SetChunkIndex(entity, newChunkIndex);
            Indexer.SetArrayIndex(entity, newArrayIndex);
        }

        template<class T>
        void SetComponentData(Entity entity, const T& data)
        {
            if (!Indexer.IsValid(entity))
                return;

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            auto& chunk = Chunks[chunkIndex];

            chunk.SetComponentData<T>(arrayIndex, data);
        }

        template<class T>
        T& GetComponentData(Entity entity)
        {
            assert(Indexer.IsValid(entity));

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            auto& chunk = Chunks[chunkIndex];

            return chunk.GetComponentData<T>(arrayIndex);
        }

        byte* GetComponentData(const ComponentType& componentType, Entity entity)
        {
            assert(Indexer.IsValid(entity));

            int chunkIndex = Indexer.GetChunkIndex(entity);
            int arrayIndex = Indexer.GetArrayIndex(entity);

            auto& chunk = Chunks[chunkIndex];

            return chunk.GetComponentData(componentType, arrayIndex);
        }

        void GetChunks(const ArchetypeMask& includeMask, std::vector<ArchetypeChunk*>& result)
        {
            profile_function;

            for (auto& chunk : Chunks)
            {
                const auto& mask = chunk.Archetype.Mask;
                if (mask.Contains(includeMask))
                    result.push_back(&chunk);
            }
        }

        template<class Stream>
        void Transfer(Stream& stream)
        {
            transfer(Indexer);
            transfer(Chunks);
        }

    private:
        int GetOrCreateChunk(const EntityArchetype& archetype)
        {
            //profile_function;

            for (int i = 0; i < Chunks.size(); ++i)
            {
                if (Chunks[i].Archetype == archetype)
                    return i;
            }

            int chunkIndex = Chunks.size();
            Chunks.push_back(ArchetypeChunk(archetype, 1 << 16));
            return chunkIndex;
        }

        int GetChunk(const ArchetypeMask& mask)
        {
            for (int i = 0; i < Chunks.size(); ++i)
            {
                if (Chunks[i].Archetype.Mask == mask)
                    return i;
            }

            return -1;
        }

        int GetOrCreateChunkWithComponent(const EntityArchetype& archetype, const ComponentType& componentType)
        {
            //profile_function;

            static ArchetypeMask tempMask;
            tempMask = archetype.Mask;
            tempMask.Enable(componentType);

            for (int i = 0; i < Chunks.size(); ++i)
            {
                if (Chunks[i].Archetype.Mask == tempMask)
                    return i;
            }

            std::vector<ComponentType> componentTypes = archetype.ComponentTypes;
            componentTypes.push_back(componentType);

            auto newArchetype = EntityArchetype(componentTypes);

            int chunkIndex = Chunks.size();
            Chunks.push_back(ArchetypeChunk(newArchetype, 1 << 16));
            return chunkIndex;
        }

        int GetOrCreateChunkWithoutComponent(const EntityArchetype& archetype, const ComponentType& componentType)
        {
            static ArchetypeMask tempMask;
            tempMask = archetype.Mask;
            tempMask.Enable(componentType);

            for (int i = 0; i < Chunks.size(); ++i)
            {
                if (Chunks[i].Archetype.Mask == tempMask)
                    return i;
            }

            std::vector<ComponentType> componentTypes = archetype.ComponentTypes;
            componentTypes.push_back(componentType);

            auto newArchetype = EntityArchetype(componentTypes);

            int chunkIndex = Chunks.size();
            Chunks.push_back(ArchetypeChunk(newArchetype, 1 << 16));
            return chunkIndex;
        }

        EntityIndexer Indexer;
        std::vector<ArchetypeChunk> Chunks;
    };

    class EntityCommandBuffer
    {
    public:
        EntityCommandBuffer(EntityManager& manager) : Manager(manager), Offset(0), Count(0) {}

        template<class C>
        void AddComponentData(Entity entity, const C& data)
        {
            auto componentType = GetComponentType<C>();
            Write(entity);
            Write(componentType);
            Write(data);
            Write(sizeof(C));
            Write(Command::AddComponentData);
            Count++;
        }

        void AddEntityToArray(std::map<Guid, Entity> value, const Guid& guid, Entity entity)
        {

        }

        void CreateEntity(const EntityArchetype& archetype, const byte* data)
        {

        }

        void Execute()
        {
            while (Count != 0)
            {
                Command command = Read<Command>();

                switch (command)
                {
                case Command::AddComponentData:
                {
                    auto size = Read<size_t>();
                    auto data = Read(size);
                    auto componentType = Read<ComponentType>();
                    auto entity = Read<Entity>();
                    Manager.AddComponentData(entity, componentType, data);
                    Count--;
                    break;
                }

                default:
                    assert(false);
                    break;
                }
            }

            assert(Offset == 0);
        }

    private:
        enum class Command
        {
            AddComponentData,
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
            Offset -= sizeof(T);
            T* data = ((T*)(Data.data() + Offset));
            return *data;
        }

        byte* Read(int length)
        {
            Offset -= length;
            byte* data = Data.data() + Offset;
            return data;
        }

        EntityManager& Manager;
        std::vector<byte> Data;
        int Offset;
        int Count;
    };

    template<typename TF>
    struct ForEachLambdaJob : IJob
    {
        ForEachLambdaJob(EntityManager* manager, WorkerManager* workerManager, ArchetypeMask& includeMask, TF func) :
            Manager(manager),
            WorkerManager(workerManager),
            IncludeMask(includeMask),
            Func(func)
        {
        }

        void Run()
        {
            profile_function;

            static_assert(lambda_traits<TF>::arg_count <= 3,
                "ForEach supports 3 arguments maximum");

            if constexpr (lambda_traits<TF>::arg_count >= 1 && !std::is_same<Entity, lambda_traits<TF>::arg0_raw_type>::value)
            {
                static_assert(lambda_traits<TF>::arg0_cwrite_type::value || lambda_traits<TF>::arg0_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg0_type>());
            }
            if constexpr (lambda_traits<TF>::arg_count >= 2)
            {
                static_assert(lambda_traits<TF>::arg1_cwrite_type::value || lambda_traits<TF>::arg1_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg1_type>());
            }
            if constexpr (lambda_traits<TF>::arg_count >= 3)
            {
                static_assert(lambda_traits<TF>::arg2_cwrite_type::value || lambda_traits<TF>::arg2_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg2_type>());
            }

            static std::vector<ArchetypeChunk*> chunks;
            {
                profile_name(GetChunk);
                chunks.clear();
                Manager->GetChunks(IncludeMask, chunks);
                Count = chunks.size();
            }

            assert(lambda_traits<TF>::arg_count * Count <= 50);

            int count = 0;
            for (auto chunk : chunks)
            {
                if constexpr (lambda_traits<TF>::arg_count >= 1)
                {
                    if constexpr (std::is_same<Entity, lambda_traits<TF>::arg0_raw_type>::value)
                    {
                        auto entities = (ComponentArraySlice<byte>&) chunk->GetEntitiesForJob();
                        ComponentArrays[count++] = entities;
                    }
                    else if constexpr (lambda_traits<TF>::arg0_cwrite_type::value)
                    {
                        auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg0_type >();
                        ComponentArrays[count++] = components;
                        if (WorkerManager != nullptr)
                        {
                            WorkerManager->Complete(*components.Handle);
                            WorkerManager->Complete(*components.ReadOHandle);
                        }
                    }
                    else
                    {
                        auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg0_type >();
                        ComponentArrays[count++] = components;
                        if (WorkerManager != nullptr)
                            WorkerManager->Complete(*components.Handle);
                    }
                }
                if constexpr (lambda_traits<TF>::arg_count >= 2)
                {
                    auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg1_type >();
                    ComponentArrays[count++] = components;
                    if constexpr (lambda_traits<TF>::arg1_cwrite_type::value)
                    {
                        if (WorkerManager != nullptr)
                        {
                            WorkerManager->Complete(*components.Handle);
                            WorkerManager->Complete(*components.ReadOHandle);
                        }
                    }
                    else
                    {
                        if (WorkerManager != nullptr)
                            WorkerManager->Complete(*components.Handle);
                    }
                }
                if constexpr (lambda_traits<TF>::arg_count >= 3)
                {
                    auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg2_type >();
                    ComponentArrays[count++] = components;
                    if constexpr (lambda_traits<TF>::arg2_cwrite_type::value)
                    {
                        if (WorkerManager != nullptr)
                        {
                            WorkerManager->Complete(*components.Handle);
                            WorkerManager->Complete(*components.ReadOHandle);
                        }
                    }
                    else
                    {
                        if (WorkerManager != nullptr)
                            WorkerManager->Complete(*components.Handle);
                    }
                }
            }

            Execute();
        }

        JobHandle Schedule()
        {
            profile_function;
            assert(WorkerManager != nullptr);

            static_assert(lambda_traits<TF>::arg_count <= 3,
                "ForEach supports 3 arguments maximum");

            if constexpr (lambda_traits<TF>::arg_count >= 1 && !std::is_same<Entity, lambda_traits<TF>::arg0_raw_type>::value)
            {
                static_assert(lambda_traits<TF>::arg0_cwrite_type::value || lambda_traits<TF>::arg0_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg0_type>());
            }
            if constexpr (lambda_traits<TF>::arg_count >= 2)
            {
                static_assert(lambda_traits<TF>::arg1_cwrite_type::value || lambda_traits<TF>::arg1_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg1_type>());
            }
            if constexpr (lambda_traits<TF>::arg_count >= 3)
            {
                static_assert(lambda_traits<TF>::arg2_cwrite_type::value || lambda_traits<TF>::arg2_cread_type::value,
                    "For each requires type to be declared with cwrite(Type) or cread(Type)");
                IncludeMask.Enable(GetComponentType<lambda_traits<TF>::arg2_type>());
            }

            static std::vector<ArchetypeChunk*> chunks;
            {
                profile_name(GetChunk);
                chunks.clear();
                Manager->GetChunks(IncludeMask, chunks);
                Count = chunks.size();
            }

            assert(lambda_traits<TF>::arg_count * Count <= 50);

            static std::vector<JobHandle> dependencies;
            {
                profile_name(DependencyClear);
                dependencies.clear();
            }

            int count = 0;
            for (auto chunk : chunks)
            {
                if constexpr (lambda_traits<TF>::arg_count >= 1)
                {
                    if constexpr (std::is_same<Entity, lambda_traits<TF>::arg0_raw_type>::value)
                    {
                        auto entities = (ComponentArraySlice<byte>&) chunk->GetEntitiesForJob();
                        ComponentArrays[count++] = entities;
                    }
                    else if constexpr (lambda_traits<TF>::arg0_cwrite_type::value)
                    {
                        auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg0_type >();
                        ComponentArrays[count++] = components;
                        dependencies.push_back(*components.Handle);
                        dependencies.push_back(*components.ReadOHandle);
                    }
                    else
                    {
                        auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg0_type >();
                        ComponentArrays[count++] = components;
                        dependencies.push_back(*components.Handle);
                    }
                }
                if constexpr (lambda_traits<TF>::arg_count >= 2)
                {
                    auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg1_type >();
                    ComponentArrays[count++] = components;
                    if constexpr (lambda_traits<TF>::arg1_cwrite_type::value)
                    {
                        dependencies.push_back(*components.Handle);
                        dependencies.push_back(*components.ReadOHandle);
                    }
                    else
                    {
                        dependencies.push_back(*components.Handle);
                    }
                }
                if constexpr (lambda_traits<TF>::arg_count >= 3)
                {
                    auto components = (ComponentArraySlice<byte>&) chunk->GetComponentsForJob< lambda_traits<TF>::arg2_type >();
                    ComponentArrays[count++] = components;
                    if constexpr (lambda_traits<TF>::arg2_cwrite_type::value)
                    {
                        dependencies.push_back(*components.Handle);
                        dependencies.push_back(*components.ReadOHandle);
                    }
                    else
                    {
                        dependencies.push_back(*components.Handle);
                    }
                }
            }

            JobHandle handle = WorkerManager->Schedule(*this, dependencies);

            count = 0;
            for (int i = 0; i < Count; ++i)
            {
                if constexpr (lambda_traits<TF>::arg_count >= 1)
                {
                    if constexpr (lambda_traits<TF>::arg0_cwrite_type::value)
                    {
                        *ComponentArrays[count].Handle = handle;
                    }
                    else if constexpr (lambda_traits<TF>::arg0_cread_type::value)
                    {
                        *ComponentArrays[count].ReadOHandle = handle;
                    }
                    count++;
                }
                if constexpr (lambda_traits<TF>::arg_count >= 2)
                {
                    if constexpr (lambda_traits<TF>::arg1_cwrite_type::value)
                    {
                        *ComponentArrays[count].Handle = handle;
                    }
                    else
                    {
                        *ComponentArrays[count].ReadOHandle = handle;
                    }
                    count++;
                }
                if constexpr (lambda_traits<TF>::arg_count >= 3)
                {
                    if constexpr (lambda_traits<TF>::arg2_cwrite_type::value)
                    {
                        *ComponentArrays[count].Handle = handle;
                    }
                    else
                    {
                        *ComponentArrays[count].ReadOHandle = handle;
                    }
                    count++;
                }
            }

            return handle;
        }

    private:

        virtual void Execute()
        {
            profile_function;

            int count = 0;
            for (int i = 0; i < Count; ++i)
            {
                if constexpr (lambda_traits<TF>::arg_count >= 3)
                {
                    auto c0 = *((ComponentArraySlice< lambda_traits<TF>::arg0_type >*) & ComponentArrays[count++]);
                    auto c1 = *((ComponentArraySlice< lambda_traits<TF>::arg1_type >*) & ComponentArrays[count++]);
                    auto c2 = *((ComponentArraySlice< lambda_traits<TF>::arg2_type >*) & ComponentArrays[count++]);
                    profile_name(ForEach3);
                    for (int i = 0; i < c0.Length(); ++i)
                        Func(c0[i], c1[i], c2[i]);
                }
                else if constexpr (lambda_traits<TF>::arg_count >= 2)
                {
                    auto c0 = *((ComponentArraySlice< lambda_traits<TF>::arg0_type >*) & ComponentArrays[count++]);
                    auto c1 = *((ComponentArraySlice< lambda_traits<TF>::arg1_type >*) & ComponentArrays[count++]);
                    profile_name(ForEach2);
                    for (int i = 0; i < c0.Length(); ++i)
                        Func(c0[i], c1[i]);
                }
                else if constexpr (lambda_traits<TF>::arg_count >= 1)
                {
                    auto c0 = *((ComponentArraySlice< lambda_traits<TF>::arg0_type >*)&ComponentArrays[count++]);
                    profile_name(ForEach1);
                    for (int i = 0; i < c0.Length(); ++i)
                        Func(c0[i]);
                }
            }
        }

        EntityManager* Manager;
        ArchetypeMask& IncludeMask;

        WorkerManager* WorkerManager;
        TF Func;
        int Count;

        ComponentArraySlice<byte> ComponentArrays[50];
    };

    struct Query
    {
        Query(EntityManager* manager) : Manager(manager), WorkerManager(nullptr)
        {
        }
        Query(EntityManager* manager, WorkerManager* workerManager) : Manager(manager), WorkerManager(workerManager)
        {
        }

        template<typename TF>
        ForEachLambdaJob<TF> ForEach(TF&& func)
        {
            profile_function;
            return ForEachLambdaJob<TF>(Manager, WorkerManager, IncludeMask, func);
        }

        int Count()
        {
            int count = 0;

            std::vector<ArchetypeChunk*> chunks;
            Manager->GetChunks(IncludeMask, chunks);

            for (auto chunk : chunks)
            {
                count += chunk->Count;
            }

            return count;
        }

        template<class T>
        Query& With()
        {
            auto componentType = GetComponentType<T>();
            IncludeMask.Enable(componentType);
            return *this;
        }

        template<class T>
        Query& Without()
        {
            auto componentType = GetComponentType<T>();
            ExcludeMask.Enable(componentType);
            return *this;
        }

        EntityManager* Manager;
        WorkerManager* WorkerManager;
        ArchetypeMask IncludeMask;
        ArchetypeMask ExcludeMask;
    };

    class World;

    class System
    {
    public:
        System(World* world, EntityManager& manager, WorkerManager* workerManager) :
            World(world),
            Manager(manager), 
            WorkerManager(workerManager)
        {}

    public:
        virtual void OnCreate() {}
        virtual void OnUpdate() = 0;
        virtual void OnDestroy() {}

    protected:
        Query Entities() const { return Query(&Manager, WorkerManager); }
        WorkerManager& GetWorkerManager() const { return *WorkerManager; }

    protected:
        World* World;
        EntityManager& Manager;
        WorkerManager* WorkerManager;
    };

    class World
    {
    public:
        World() :
            Worker(nullptr)
        {
            Manager = new EntityManager();
            m_BlobManager = new BlobManager();
        }

        World(WorkerManager* workerManager) :
            Worker(workerManager)
        {
            Manager = new EntityManager();
            m_BlobManager = new BlobManager();
        }

        ~World()
        {
            for (auto system : Systems)
            {
                system->OnDestroy();
            }

            delete Manager;
            delete m_BlobManager;
        }

        void Update()
        {
            profile_function;

            SetBlobManager(m_BlobManager);

            for (auto system : Systems)
            {
                system->OnUpdate();
            }

            SetBlobManager(nullptr);
        }

        template<class S>
        S& GetOrCreateSystem()
        {
            profile_function;
            for (auto system : Systems)
            {
                if (typeid(system) == typeid(S))
                    return *((S*)system);
            }

            auto system = new S(this, *Manager, Worker);
            Systems.push_back(system);

            SetBlobManager(m_BlobManager);

            profile_name(OnCreate);
            system->OnCreate();

            return *system;
        }

        EntityManager& GetManager() const { return *Manager; }

    private:
        EntityManager* Manager;
        BlobManager* m_BlobManager;
        WorkerManager* Worker;

        std::vector<System*> Systems;
    };
}