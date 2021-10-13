#include "stdio.h"
#include "string"
#include "assert.h"
#include "vector"
#include <algorithm>
#include <map>
#include <typeinfo>
#include <typeindex>
#include <stack>
#include <functional>
#include "NodeVision.Profiling.h"
#include "NodeVision.Collections.hpp"
#include "NodeVision.Entities.hpp"
#include "NodeVision.Assets.hpp"
#include "NodeVision.Serialization.hpp"
#include "NodeVision.Jobs.hpp"
#include "NodeVision.Build.hpp"
#include "NodeVision.CommandBuffer.hpp"

using namespace NodeVision::Serialization;

using namespace NodeVision::Entities;
using namespace NodeVision::Collections;
using namespace NodeVision::Profiling;
using namespace NodeVision::Assets;
using namespace NodeVision::Jobs;
using namespace NodeVision::Build;
using namespace NodeVision::CommandBuffer;

void ArchetypeMaskTest()
{
    struct Foo
    {
        Guid MyGuid;
        float MyVariable;
        float MyVariable2;
    };

    struct B
    {
        B(int a, int b) : Value(a), Value2(b) {}
        int Value;
        int Value2;
    };

    auto mask1 = ArchetypeMask({ typeof(Foo), typeof(B) });
    auto mask2 = ArchetypeMask({ typeof(Foo) });
    auto mask3 = ArchetypeMask({ typeof(B) });

    assert(mask1.Contains(mask2));
    assert(mask1.Contains(mask3));

    assert(!mask2.Contains(mask1));
    assert(!mask3.Contains(mask1));
    assert(!mask2.Contains(mask3));
}

void ChunkTest()
{
    struct A
    {
        A(int a) : Value(a) {}
        int Value;
    };

    struct B
    {
        B(int a, int b) : Value(a), Value2(b) {}
        int Value;
        int Value2;
    };

    auto archetype = EntityArchetype({ typeof(A), typeof(B) });

    auto chunk = ArchetypeChunk(archetype, 64);

    int arrayIndex1 = chunk.PushBack();
    chunk.SetComponentData(arrayIndex1, A(5));
    assert(chunk.GetComponentData<A>(arrayIndex1).Value == 5);

    int arrayIndex2 = chunk.PushBack();
    chunk.SetComponentData(arrayIndex2, A(6));
    chunk.SetComponentData(arrayIndex2, B(10, 12));
    assert(chunk.GetComponentData<A>(arrayIndex2).Value == 6);
    assert(chunk.GetComponentData<B>(arrayIndex2).Value == 10);

    chunk.RemoveAtSwapBack(arrayIndex1);

    assert(chunk.GetComponentData<A>(arrayIndex1).Value == 6);
}

void EntityManagerTest()
{
    struct A
    {
        A(int a) : Value(a) {}
        int Value;
    };

    struct B
    {
        B(int a, int b) : Value(a), Value2(b) {}
        int Value;
        int Value2;
    };

    EntityManager entityManager;

    auto archetype = entityManager.CreateArchetype({ typeof(A), typeof(B) });

    Entity entity = entityManager.CreateEntity(archetype);
    entityManager.SetComponentData(entity, A(5));
    assert(entityManager.GetComponentData<A>(entity).Value == 5);

    Entity entity2 = entityManager.CreateEntity(archetype);
    entityManager.SetComponentData(entity2, A(6));
    entityManager.SetComponentData(entity2, B(10, 12));
    assert(entityManager.GetComponentData<A>(entity2).Value == 6);
    assert(entityManager.GetComponentData<B>(entity2).Value == 10);

    entityManager.DestroyEntity(entity);
    assert(entityManager.GetComponentData<A>(entity2).Value == 6);
    assert(entityManager.GetComponentData<B>(entity2).Value == 10);

    auto archetype2 = entityManager.CreateArchetype({ typeof(B) });
    Entity entity3 = entityManager.CreateEntity(archetype2);
    entityManager.SetComponentData(entity3, B(20, 20));
    assert(entityManager.GetComponentData<B>(entity3).Value == 20);
}

void QueryTest()
{
    struct A
    {
        A(int a) : Value(a) {}
        int Value;
    };

    struct B
    {
        B(int a, int b) : Value(a), Value2(b) {}
        int Value;
        int Value2;
    };

    struct C
    {
        C(int a) : Value(a) {}
        int Value;
    };

    EntityManager entityManager;

    auto archetype = entityManager.CreateArchetype({ typeof(A), typeof(B) });

    Entity entity = entityManager.CreateEntity(archetype);
    entityManager.SetComponentData<A>(entity, A(5));
    entityManager.SetComponentData<B>(entity, B(5, 10));

    {
        Query query(&entityManager);
        query.ForEach(
            [](cread(A) a)
            {
                assert(a.Value == 5);
            }).Run();
        assert(query.Count() == 1);
    }

    auto archetype2 = entityManager.CreateArchetype({ typeof(A) });

    Entity entity1 = entityManager.CreateEntity(archetype2);
    entityManager.SetComponentData(entity1, A(10));
    Entity entity2 = entityManager.CreateEntity(archetype2);
    entityManager.SetComponentData(entity2, A(15));
    Entity entity3 = entityManager.CreateEntity(archetype2);
    entityManager.SetComponentData(entity3, A(20));

    {
        Query query(&entityManager);
        query.ForEach(
            [](cread(A) a)
            {
                static int counter = 1;
                assert(a.Value == 5 * counter++);
            }).Run();
        assert(query.Count() == 4);
    }

    {
        Query query(&entityManager);
        query.ForEach(
            [](cread(A) a, cread(B) b)
            {
                assert(a.Value == 5);
                assert(b.Value == 5);
                assert(b.Value2 == 10);
            }).Run();
        assert(query.Count() == 1);
    }

    entityManager.AddComponentData(entity3, C(10));

    {
        Query query(&entityManager);
        query.ForEach(
            [](cread(A) a, cread(C) c)
            {
                assert(c.Value == 10);
            }).Run();
        assert(query.Count() == 1);
    }
}

void WorldTest()
{
    struct A
    {
        A(int a) : Value(a) {}
        int Value;
    };

    struct B
    {
        B(int a) : Value(a) {}
        int Value;
    };

    struct C
    {
        C(int a) : Value(a) {}
        int Value;
    };

    class ASystem : public System
    {
    public:
        using System::System;

        virtual void OnUpdate()
        {
            auto archetype = Manager.CreateArchetype({ typeof(A), typeof(B) });

            for (int i = 0; i < 1000; ++i)
            {
                Entity entity = Manager.CreateEntity(archetype);
                Manager.SetComponentData(entity, A(5));
            }

            Entities().ForEach(
                [&](Entity entity, cwrite(A) a)
                {
                    for (int i = 0; i < 100000; ++i)
                    {
                        a.Value = a.Value + 5;
                    }

                    //CommandBuffer.AddComponentData(entity, C(20));
                }).Run();

            //CommandBuffer.Execute();

            //assert(Entities().With<A>().Count() == 1000);
            //assert(Entities().With<C>().Count() == 1000);
        }
    };

    World world;
    world.GetOrCreateSystem<ASystem>();
    world.Update();
}

void CommandBufferTest()
{
    struct A
    {
        A(int a) : Value(a) {}
        int Value;
    };

    struct B
    {
        B(int a) : Value(a) {}
        int Value;
    };

    EntityManager manager;
    auto archetype = manager.CreateArchetype({ typeof(A) });

    Entity entity = manager.CreateEntity(archetype);
    manager.SetComponentData(entity, A(5));

    EntityCommandBuffer ecb(manager);

    ecb.AddComponentData(entity, B(20));

    ecb.Execute();

    assert(manager.GetComponentData<B>(entity).Value == 20);
}

struct A : IPersistent<1>
{
    A() {}
    A(int a) : Value(a) {}

    template<class Stream>
    void Transfer(Stream& stream) { transfer(Value); }

    int Value;
};

struct B : IPersistent<3>
{
    B() {}
    B(int a) : Value(a) {}

    template<class Stream>
    void Transfer(Stream& stream) { transfer(Value); }

    int Value;
};

void EntityManagerSerializeTest()
{
    EntityManager manager;

    // Archetype A
    auto archetype = manager.CreateArchetype({ typeof(A) });
    Entity entity = manager.CreateEntity(archetype);
    manager.SetComponentData(entity, A(5));
    Entity entity4 = manager.CreateEntity(archetype);
    manager.SetComponentData(entity4, A(4));
    Entity entity5 = manager.CreateEntity(archetype);
    manager.SetComponentData(entity5, A(5));

    // Archetype B
    auto archetype2 = manager.CreateArchetype({ typeof(B)});
    Entity entity2 = manager.CreateEntity(archetype2);
    manager.SetComponentData(entity2, B(3));

    manager.DestroyEntity(entity4);

    auto stream = YamlWriteStream2();
    if (stream.Open("./Test.yaml"))
    {
        manager.Transfer(stream);
        assert(stream.Close());
    }

    EntityManager manager2;

    YamlReadStream2 readStream;
    if (readStream.Open("./Test.yaml"))
    {
        manager2.Transfer(readStream);
        assert(readStream.Close());
    }

    assert(manager == manager2);
}

struct C : IDisposable
{
    BlobReference<int> Value;
};

void BlobReferenceTest()
{
    EntityManager manager;

    auto archetype = manager.CreateArchetype({ typeof(C) });

    BlobManager blobManager;
    SetBlobManager(&blobManager);
    {
        Entity entity = manager.CreateEntity(archetype);

        assert(GetBlobManager().Empty());

        C ca;
        BlobBuilder<int> builder;
        auto& c = builder.GetRoot();
        c = 10;
        ca.Value = builder.Build(Guid(1, 2, 3, 4));
        manager.SetComponentData(entity, ca);
        assert(GetBlobManager().Count() == 1);

        Entity entity2 = manager.CreateEntity(archetype);

        C ba;
        ba.Value = ca.Value;
        manager.SetComponentData(entity, ba);
        assert(GetBlobManager().Count() == 1);

        manager.SetComponentData(entity2, ba);

        assert(manager.GetComponentData<C>(entity).Value.Guid == Guid(1, 2, 3, 4));
        manager.DestroyEntity(entity);
        assert(GetBlobManager().Count() == 1);
        manager.DestroyEntity(entity2);
    }

    assert(GetBlobManager().Empty());
}

void JobsTest()
{
    WorkerManager workerManager;
    workerManager.Start(2);

    struct SlowAddJob : IJob
    {
        SlowAddJob(int* a, int* b, int* result) : A(a), B(b), Result(result) {}
        virtual void Execute()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            *Result = *A + *B;
        }
        int* A;
        int* B;
        int* Result;
    };

    struct SlowMuldJob : IJob
    {
        SlowMuldJob(int* a, int* b, int* result) : A(a), B(b), Result(result) {}
        virtual void Execute()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            *Result = *A * *B;
        }
        int* A;
        int* B;
        int* Result;
    };

    // Lets do with jobs (5+2) * (6+3)

    int _5 = 5;
    int _2 = 2;
    int _6 = 6;
    int _3 = 3;

    int _5_2;
    int _6_3;
    int result;

    auto jobHandle0 = workerManager.Schedule(SlowAddJob(&_5, &_2, &_5_2));
    auto jobHandle1 = workerManager.Schedule(SlowAddJob(&_6, &_3, &_6_3));
    auto waitAll = workerManager.Combine(jobHandle0, jobHandle1);
    auto jobHandle2 = workerManager.Schedule(SlowMuldJob(&_5_2, &_6_3, &result), waitAll);

    workerManager.Complete(jobHandle2);

    workerManager.Stop();

    assert(result == (5 + 2) * (6 + 3));
}

void JobifiedEntityCommandBufferTest()
{
    struct MyComponent
    {
        MyComponent(float value) { Value = value; }
        float Value;
    };

    struct MyComponent2
    {
        MyComponent2(float value) { Value = value; }
        float Value;
    };

    class MySystem : public System
    {
    public:
        using System::System;

        virtual void OnCreate()
        {
            EcbSystem = &World->GetOrCreateSystem<EndSimulationCommandBufferSystem>();
            EntityArchetype archetype = Manager.CreateArchetype({ typeof(MyComponent) });
            Entity entity = Manager.CreateEntity(archetype);
        }

        virtual void OnUpdate()
        {
            auto ecb = EcbSystem->GetBuffer();
            auto dependency = Entities().ForEach(
                [=](Entity entity, cwrite(MyComponent) myComponent)
                {
                    ecb->AddComponentData(entity, MyComponent2(4));
                    //printf("Entity:(%d, %d) MyComponent:%f\n", entity.Index, entity.Version, myComponent.Value);
                }).Schedule();
            EcbSystem->AddProducer(dependency);
        }

        EndSimulationCommandBufferSystem* EcbSystem;
    };

    class MySystem2 : public System
    {
    public:
        using System::System;

        virtual void OnUpdate()
        {
            Entities().ForEach(
                [](Entity entity, cread(MyComponent2) myComponent)
                {
                    assert(myComponent.Value == 4);
                }).Schedule();
        }
    };

    WorkerManager workerManager; // Create multi threads
    workerManager.Start({ WorkerContext() });
    World world(&workerManager); // Create world
    world.GetOrCreateSystem<MySystem>(); // Create system
    world.GetOrCreateSystem<MySystem2>(); // Create system
    world.Update(); // Execute single frame update
    workerManager.Stop();
}

void Demo()
{
    struct Position
    {
        Position(float x, float y) : X(x), Y(y) {}
        float X;
        float Y;
    };

    struct MoveMe
    {
        MoveMe(float speed) : Speed(speed) {}
        float Speed;
    };

    class MySystem : public System
    {
    public:
        using System::System;

        virtual void OnCreate()
        {
            EntityArchetype archetype = Manager.CreateArchetype({ typeof(MoveMe), typeof(Position), typeof(B) });

            Entity entity = Manager.CreateEntity(archetype);
            Manager.SetComponentData(entity, Position(1, 1));
            Manager.SetComponentData(entity, MoveMe(1));

            Entity entity2 = Manager.CreateEntity(archetype);
            Manager.SetComponentData(entity2, Position(0, 0));
            Manager.SetComponentData(entity2, MoveMe(2));

            EntityArchetype archetype2 = Manager.CreateArchetype({ typeof(Position) });

            Entity entity3 = Manager.CreateEntity(archetype2);
            Manager.SetComponentData(entity3, Position(2, 2));
        }

        virtual void OnUpdate()
        {
            // Print current position
            Entities().ForEach(
                [](Entity entity, cread(Position) position)
                {
                    printf("Entity:(%d, %d) Position:(%f, %f)\n", entity.Index, entity.Version, position.X, position.Y);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }).Schedule();

            // Move entities by speed
            Entities().ForEach(
                [](Entity entity, cread(MoveMe) move, cwrite(Position) position)
                {
                    printf("Entity:(%d, %d) Move Position By:%f\n", entity.Index, entity.Version, move.Speed);
                    position.X += move.Speed;
                    position.Y += move.Speed;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }).Schedule();

             // Print changed position
             Entities().ForEach(
                [](Entity entity, cread(Position) position)
                {
                    printf("Entity:(%d, %d) Position:(%f, %f)\n", entity.Index, entity.Version, position.X, position.Y);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }).Schedule();
        }
    };

#define PROFILER_ENABLED 0

    WorkerManager workerManager;
    WorkerContext workerContext0;
    WorkerContext workerContext1;

#if PROFILER_ENABLED
    // Set profiler for main thread
    ProfileManager profileManager;
    SetProfileManager(&profileManager);

    // Set profiler for worker thread 0
    ProfileManager ProfileManager0;
    workerContext0.ProfileManager = &ProfileManager0;

    // Set profiler for worker thread 1
    ProfileManager ProfileManager1;
    workerContext1.ProfileManager = &ProfileManager1;
#endif

    {
        // Start worker threads
        workerManager.Start({ workerContext0, workerContext1 });

        World world(&workerManager); // Create world
        world.GetOrCreateSystem<MySystem>(); // Create system
        world.Update(); // Execute single frame update

        // Stop worker threads
        workerManager.Stop();
    }

    // Print profile to console
#if PROFILER_ENABLED
    ProfileHierarchySnapshot snapshot;
    snapshot.Build(profileManager);
    printf("Main Thread:\n");
    snapshot.Print();

    ProfileHierarchySnapshot snapshot0;
    snapshot0.Build(ProfileManager0);
    printf("Worker Thread 0:\n");
    snapshot0.Print();

    ProfileHierarchySnapshot snapshot1;
    snapshot1.Build(ProfileManager1);
    printf("Worker Thread 1:\n");
    snapshot1.Print();
#endif
}

void MinimalDemo()
{
    struct MyComponent
    {
        MyComponent(float value) { Value = value; }
        float Value;
    };

    struct MyComponent2
    {
        MyComponent2(float value) { Value = value; }
        float Value;
    };

    class MySystem : public System
    {
    public:
        using System::System;

        virtual void OnCreate()
        {
            EntityArchetype archetype = Manager.CreateArchetype({ typeof(MyComponent) });

            Entity entity = Manager.CreateEntity(archetype);
            Manager.SetComponentData(entity, MyComponent(1));

            Manager.AddComponentData(entity, MyComponent2(2));

            printf("%f \n", Manager.GetComponentData<MyComponent2>(entity).Value);

            Manager.DestroyEntity(entity);
        }

        virtual void OnUpdate()
        {
            Entities().ForEach(
                [](Entity entity, cread(MyComponent) myComponent)
                {
                    printf("Entity:(%d, %d) MyComponent:%f\n", entity.Index, entity.Version, myComponent.Value);
                }).Schedule();
        }
    };

    WorkerManager workerManager; // Create multi threads
    workerManager.Start({ WorkerContext() });
    World world(&workerManager); // Create world
    world.GetOrCreateSystem<MySystem>(); // Create system
    world.Update(); // Execute single frame update
    workerManager.Stop();
}

#define run_test(Name) \
    printf("Running Test " #Name ":\n"); \
    ##Name (); \
    printf("PASSED!\n");

int main()
{
    // Run tests to check against regressions
    run_test(ArchetypeMaskTest);
    run_test(ChunkTest);
    run_test(EntityManagerTest);
    run_test(QueryTest);
    run_test(WorldTest);
    run_test(CommandBufferTest);
    run_test(BlobReferenceTest);
    run_test(JobsTest);
    run_test(EntityManagerSerializeTest);
    run_test(JobifiedEntityCommandBufferTest);

    // Run small demo
    Demo();
    MinimalDemo();

    return 0;
}