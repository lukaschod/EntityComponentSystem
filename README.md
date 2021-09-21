# EntityComponentSystem

## About
This is small implementation of entity component system with C++. The API is heavily inspired by Unity ECS framework, but it does not share any underling implementation. This project is solely done for fun, to see how much it is possible to recreate Unity like ECS framework in C++ with similar API.

## Requiraments
Project was create with Visual Studio 2019 so it is recommended to open with it. However it is only few files, as result it should be trivial to move to any prefer IDEA.

## Features
Systems that are used for executing game logic. In most cases ues entity queries for modifying data.
```
class MySystem : public System
{
public:
    using System::System;
    virtual void OnCreate() {}
    virtual void OnUpdate() {}
};
```

Component type that can be any non generic struct.
```
struct MyComponent
{
    MyComponent(float value) { Value = value; }
    float Value;
};
```

Entity Manager for handling all operations related with entities.
```
virtual void OnCreate()
{
    EntityArchetype archetype = Manager.CreateArchetype({ typeof(MyComponent) });

    Entity entity = Manager.CreateEntity(archetype);
    Manager.SetComponentData(entity, MyComponent(1));

    Manager.AddComponentData(entity, MyComponent2(2));

    printf("%f \n", Manager.GetComponentData<MyComponent2>(entity).Value);

    Manager.DestroyEntity(entity);
}
```

Entity queries for fetching entity component data and executing in for each loop.
```
virtual void OnUpdate()
{
    Entities().ForEach(
        [](Entity entity, cread(MyComponent) myComponent)
        {
            printf("Entity:(%d, %d) MyComponent:%f\n", entity.Index, entity.Version, myComponent.Value);
        }).Run();
        
    Entities().ForEach(
        [](Entity entity, cwrite(MyComponent) myComponent)
        {
            myComponent.Value = 5;
        }).Run();
}
```

Creating world that allows easier way to create systems and execute them.
```
World world; // Create world
world.GetOrCreateSystem<MySystem>(); // Create system
world.Update(); // Execute single frame update
```

## Example Single Threaded
```
void MinimalDemo()
{
    struct MyComponent
    {
        MyComponent(float value) { Value = value; }
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
        }

        virtual void OnUpdate()
        {
            Entities().ForEach(
                [](Entity entity, cread(MyComponent) myComponent)
                {
                    printf("Entity:(%d, %d) MyComponent:%f\n", entity.Index, entity.Version, myComponent.Value);
                }).Run();
        }
    };

    World world; // Create world
    world.GetOrCreateSystem<MySystem>(); // Create system
    world.Update(); // Execute single frame update
}
```
Output:
```
Entity:(0, 0) MyComponent:1.000000
```

## Example Multi Threaded
```
void MinimalDemo()
{
    struct MyComponent
    {
        MyComponent(float value) { Value = value; }
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
```
Output:
```
Entity:(0, 0) MyComponent:1.000000
```
