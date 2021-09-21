#pragma once

#include <vector>
#include <queue>
#include <condition_variable>

#include "NodeVision.Profiling.h"

static NodeVision::Profiling::ProfileSample Lock_Sample("Lock");
#define thread_lock(Protect) \
    NodeVision::Profiling::Profiler::BeginSample(Lock_Sample); \
    std::lock_guard<std::mutex> lock(Protect); \
    NodeVision::Profiling::Profiler::EndSample(Lock_Sample); \

namespace NodeVision::Jobs
{
    using namespace Profiling;

    struct IJob
    {
        virtual void Execute() = 0;
    };

    struct JobHandle
    {
        int Index;
        int Version;
    };

    struct JobData
    {
        JobHandle Handle;
        byte Data[2560];
        std::condition_variable Signal;
        std::vector<JobHandle> Chain;
        int DependencyLeft;
        bool Execute;
    };

    class AutoResetEvent
    {
    public:
        explicit AutoResetEvent(bool initial = false) : flag(initial) {}

        void Set()
        {
            std::lock_guard<std::mutex> _(protect);
            flag = true;
            signal.notify_one();
        }

        void Reset()
        {
            std::lock_guard<std::mutex> _(protect);
            flag = false;
        }

        bool WaitOne()
        {
            std::unique_lock<std::mutex> lk(protect);
            while (!flag) // prevent spurious wakeups from doing harm
                signal.wait(lk);
            flag = false; // waiting resets the flag
            return true;
        }

    private:
        AutoResetEvent(const AutoResetEvent&);
        AutoResetEvent& operator=(const AutoResetEvent&); // non-copyable

    private:
        std::condition_variable signal;
        std::mutex protect;
        bool flag;
    };

    class JobQueue
    {
    public:
        JobQueue()
        {
        }

        JobHandle Enqueue(const std::vector<JobHandle>& dependencies)
        {
            profile_function;
            thread_lock(Protect);

            JobData* jobData;
            if (!FreeJobDataIndices.empty())
            {
                jobData = JobDatas[FreeJobDataIndices.front()];
                jobData->Handle.Version++;
                jobData->DependencyLeft = 0;
                jobData->Execute = false;
                jobData->Chain.clear();
                FreeJobDataIndices.pop();
            }
            else
            {
                jobData = new JobData();
                jobData->Handle.Index = JobDatas.size();
                jobData->Handle.Version = 1;
                jobData->DependencyLeft = 0;
                jobData->Execute = false;
                JobDatas.push_back(jobData);
            }

            //printf("I:%d V:%d Queue Combined\n", jobData->Handle.Index, jobData->Handle.Version);


            for (auto&& dependency : dependencies)
            {
                JobData* dependencyJobData = JobDatas[dependency.Index];
                if (dependencyJobData->Handle.Version == dependency.Version)
                {
                    dependencyJobData->Chain.push_back(jobData->Handle);
                    jobData->DependencyLeft++;
                    //printf("I:%d V:%d Adding dependency I:%d V:%d (%d)\n", jobData->Handle.Index, jobData->Handle.Version,
                    //    dependencyJobData->Handle.Index, dependencyJobData->Handle.Version, jobData->DependencyLeft);
                }
            }

            JobHandle jobHandle = jobData->Handle;

            if (jobData->DependencyLeft == 0)
            {
                //Jobs.push(jobData);
                // We can simulate execution on main thread
                jobData->Handle.Version++;
                FreeJobDataIndices.push(jobData->Handle.Index);
            }

            return jobHandle;
        }

        JobHandle Enqueue(std::initializer_list<JobHandle> dependencies)
        {
            profile_function;
            thread_lock(Protect);

            JobData* jobData;
            if (!FreeJobDataIndices.empty())
            {
                jobData = JobDatas[FreeJobDataIndices.front()];
                jobData->Handle.Version++;
                jobData->DependencyLeft = 0;
                jobData->Execute = false;
                jobData->Chain.clear();
                FreeJobDataIndices.pop();
            }
            else
            {
                jobData = new JobData();
                jobData->Handle.Index = JobDatas.size();
                jobData->Handle.Version = 1;
                jobData->DependencyLeft = 0;
                jobData->Execute = false;
                JobDatas.push_back(jobData);
            }


            for (auto&& dependency : dependencies)
            {
                JobData* dependencyJobData = JobDatas[dependency.Index];
                if (dependencyJobData->Handle.Version == dependency.Version)
                {
                    dependencyJobData->Chain.push_back(jobData->Handle);
                    jobData->DependencyLeft++;
                }
            }

            JobHandle jobHandle = jobData->Handle;

            if (jobData->DependencyLeft == 0)
            {
                //Jobs.push(jobData);
                // We can simulate execution on main thread
                jobData->Handle.Version++;
                FreeJobDataIndices.push(jobData->Handle.Index);
            }

            return jobHandle;
        }

        template<class T>
        JobHandle Enqueue(const T& job)
        {
            profile_function;
            thread_lock(Protect);

            JobData* jobData;
            if (!FreeJobDataIndices.empty())
            {
                jobData = JobDatas[FreeJobDataIndices.front()];
                jobData->Handle.Version++;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                jobData->Chain.clear();
                memcpy(jobData->Data, &job, sizeof(T));
                FreeJobDataIndices.pop();
            }
            else
            {
                jobData = new JobData();
                jobData->Handle.Index = JobDatas.size();
                jobData->Handle.Version = 1;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                assert(sizeof(T) <= 2560);
                memcpy(jobData->Data, &job, sizeof(T));
                JobDatas.push_back(jobData);
            }

            Jobs.push(jobData);

            return jobData->Handle;
        }

        template<class T>
        JobHandle Enqueue(const T& job, std::initializer_list<JobHandle> dependencies)
        {
            profile_function;
            thread_lock(Protect);

            JobData* jobData;
            if (!FreeJobDataIndices.empty())
            {
                jobData = JobDatas[FreeJobDataIndices.front()];
                jobData->Handle.Version++;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                jobData->Chain.clear();
                assert(sizeof(T) <= 2560);
                memcpy(jobData->Data, &job, sizeof(T));
                FreeJobDataIndices.pop();
            }
            else
            {
                jobData = new JobData();
                jobData->Handle.Index = JobDatas.size();
                jobData->Handle.Version = 1;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                assert(sizeof(T) <= 2560);
                memcpy(jobData->Data, &job, sizeof(T));
                JobDatas.push_back(jobData);
            }

            //printf("I:%d V:%d Queue\n", jobData->Handle.Index, jobData->Handle.Version);

            for (auto&& dependency : dependencies)
            {
                JobData* dependencyJobData = JobDatas[dependency.Index];
                if (dependencyJobData->Handle.Version == dependency.Version)
                {
                    dependencyJobData->Chain.push_back(jobData->Handle);
                    jobData->DependencyLeft++;
                    //printf("I:%d V:%d Adding dependency I:%d V:%d (%d)\n", jobData->Handle.Index, jobData->Handle.Version, 
                    //    dependencyJobData->Handle.Index, dependencyJobData->Handle.Version, jobData->DependencyLeft);

                }
            }

            if (jobData->DependencyLeft == 0)
                Jobs.push(jobData);
           // printf("I:%d V:%d Dependency set  %d\n", jobData->Handle.Index, jobData->Handle.Version, jobData->DependencyLeft);

            return jobData->Handle;
        }

        template<class T>
        JobHandle Enqueue(const T& job, const std::vector<JobHandle>& dependencies)
        {
            profile_function;
            thread_lock(Protect);

            JobData* jobData;
            if (!FreeJobDataIndices.empty())
            {
                jobData = JobDatas[FreeJobDataIndices.front()];
                jobData->Handle.Version++;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                jobData->Chain.clear();
                assert(sizeof(T) <= 2560);
                memcpy(jobData->Data, &job, sizeof(T));
                FreeJobDataIndices.pop();
            }
            else
            {
                jobData = new JobData();
                jobData->Handle.Index = JobDatas.size();
                jobData->Handle.Version = 1;
                jobData->DependencyLeft = 0;
                jobData->Execute = true;
                assert(sizeof(T) <= 2560);
                memcpy(jobData->Data, &job, sizeof(T));
                JobDatas.push_back(jobData);
            }

            //printf("I:%d V:%d Queue\n", jobData->Handle.Index, jobData->Handle.Version);

            for (auto&& dependency : dependencies)
            {
                JobData* dependencyJobData = JobDatas[dependency.Index];
                if (dependencyJobData->Handle.Version == dependency.Version)
                {
                    dependencyJobData->Chain.push_back(jobData->Handle);
                    jobData->DependencyLeft++;
                    //printf("I:%d V:%d Adding dependency I:%d V:%d (%d)\n", jobData->Handle.Index, jobData->Handle.Version, 
                    //    dependencyJobData->Handle.Index, dependencyJobData->Handle.Version, jobData->DependencyLeft);

                }
            }

            if (jobData->DependencyLeft == 0)
                Jobs.push(jobData);
            // printf("I:%d V:%d Dependency set  %d\n", jobData->Handle.Index, jobData->Handle.Version, jobData->DependencyLeft);

            return jobData->Handle;
        }

        bool Dequeue(JobData*& out)
        {
            profile_function;
            thread_lock(Protect);

            if (Jobs.empty())
                return false;
            out = Jobs.front();
            Jobs.pop();
            return true;
        }

        bool IsEmpty()
        {
            std::lock_guard<std::mutex> lock(Protect);
            return Jobs.empty() && JobDatas.size() == FreeJobDataIndices.size();
        }

        void Complete(const JobHandle& jobHandle)
        {
            profile_function;

            // Early out if it is completed
            if (JobDatas[jobHandle.Index]->Handle.Version != jobHandle.Version)
                return;

            std::unique_lock<std::mutex> lock(Protect);

            JobData* jobData = JobDatas[jobHandle.Index];

            // We have to check again, as it is possible job finished after last check
            if (jobData->Handle.Version != jobHandle.Version)
                return;

            jobData->Signal.wait(lock);
        }

        void SetCompleted(JobData* jobData)
        {
            profile_function;
            thread_lock(Protect);

            //printf("I:%d V:%d Complete\n", jobData->Handle.Index, jobData->Handle.Version);

            jobData->Handle.Version++;

            FreeJobDataIndices.push(jobData->Handle.Index);


            for (auto other : jobData->Chain)
            {
                JobData* chainJobData = JobDatas[other.Index];
                chainJobData->DependencyLeft--;

                //printf("I:%d V:%d Poke I:%d V:%d (%d)\n", jobData->Handle.Index, jobData->Handle.Version, 
                //    chainJobData->Handle.Index, chainJobData->Handle.Version, chainJobData->DependencyLeft);

                assert(chainJobData->DependencyLeft >= 0);

                if (chainJobData->DependencyLeft == 0)
                {
                    Jobs.push(chainJobData);
                }
            }

            jobData->Signal.notify_all();
        }

    private:
        std::mutex Protect;
        std::vector<JobData*> JobDatas;
        std::queue<int> FreeJobDataIndices;

        std::queue<JobData*> Jobs;
    };

    struct WorkerContext
    {
        WorkerContext() : ProfileManager(nullptr) {}
        ProfileManager* ProfileManager;
    };

    class Worker
    {
    public:
        Worker(JobQueue& jobQueue) : 
            JobQueue(jobQueue),
            IsSleeping(false), 
            IsRunning(false), 
            Thread(nullptr) {}

        ~Worker()
        {
            if (Thread != nullptr)
                delete Thread;
        }

        void Start(WorkerContext context) 
        {
            profile_function;

            assert(!IsRunning);
            if (Thread != nullptr)
                delete Thread;
            IsRunning = true;
            Thread = new std::thread(&Worker::Run, this, context);
        }

        void Stop()
        {
            profile_function;

            assert(IsRunning);
            IsRunning = false;
            Wakeup();
            Thread->join();
        }

        void Wakeup()
        {
            profile_function;

            Event.Set();
            IsSleeping = false;
        }

        bool Idle() const { return IsSleeping; }

        ProfileManager* ProfileManager;

    private:
        void Run(WorkerContext context)
        {
            SetProfileManager(context.ProfileManager);

            profile_function;
            JobData* jobData;

            while (IsRunning)
            {
                if (!JobQueue.Dequeue(jobData))
                {
                    Sleep();
                    continue;
                }

                if (jobData->Execute)
                {
                    IJob& job = *(IJob*)jobData->Data;
                    job.Execute();
                }

                JobQueue.SetCompleted(jobData);
            }
        }

        void Sleep()
        {
            profile_function;
            IsSleeping = true;
            Event.WaitOne();
        }

    private:
        JobQueue& JobQueue;
        AutoResetEvent Event;
        bool IsSleeping;
        bool IsRunning;
        std::thread* Thread;
    };

    class WorkerManager
    {
    public:
        WorkerManager() : IsRunning(false)
        {}

        ~WorkerManager()
        {
            for (auto worker : Workers)
            {
                delete worker;
            }
        }

        template<class Job>
        JobHandle Schedule(const Job& job)
        {
            auto jobHandle = JobQueue.Enqueue(job);
            WakeupWorker();
            return jobHandle;
        }

        template<class Job, typename... JobHandles>
        JobHandle Schedule(const Job& job, JobHandles... dependencies)
        {
            auto jobHandle = JobQueue.Enqueue(job, { dependencies... });
            WakeupWorker();
            return jobHandle;
        }

        template<class Job>
        JobHandle Schedule(const Job& job, const std::vector<JobHandle>& dependencies)
        {
            auto jobHandle = JobQueue.Enqueue(job, dependencies);
            WakeupWorker();
            return jobHandle;
        }

        void Complete(const JobHandle& jobHandle)
        {
            JobQueue.Complete(jobHandle);
        }

        template<typename... JobHandles>
        JobHandle Combine(JobHandles... dependencies)
        {
            auto jobHandle = JobQueue.Enqueue({ dependencies... });
            WakeupWorker();
            return jobHandle;
        }

        JobHandle Combine(const std::vector<JobHandle> dependencies)
        {
            auto jobHandle = JobQueue.Enqueue(dependencies);
            WakeupWorker();
            return jobHandle;
        }

        void Start(int workerCount)
        {
            profile_function;

            assert(!IsRunning);
            IsRunning = true;
            for (int i = 0; i < workerCount; ++i)
            {
                auto worker = new Worker(JobQueue);
                worker->Start(WorkerContext());
                Workers.push_back(worker);
            }
        }

        void Start(std::initializer_list<WorkerContext> contexts)
        {
            profile_function;

            assert(!IsRunning);
            IsRunning = true;
            for (auto context : contexts)
            {
                auto worker = new Worker(JobQueue);
                worker->Start(context);
                Workers.push_back(worker);
            }
        }

        void Wait()
        {
            profile_function;
            while (!JobQueue.IsEmpty());
        }

        void Stop()
        {
            profile_function;

            assert(IsRunning);
            IsRunning = false;

            while (!JobQueue.IsEmpty());

            for (auto worker : Workers)
            {
                worker->Stop();
            }
        }

        int GetWorkerCount() const { return Workers.size(); }

    private:
        void WakeupWorker()
        {
            profile_function;

            for (auto worker : Workers)
            {
                if (worker->Idle())
                {
                    worker->Wakeup();
                    break;
                }
            }
        }

    private:
        JobQueue JobQueue;
        bool IsRunning;
        std::vector<Worker*> Workers;
    };
}