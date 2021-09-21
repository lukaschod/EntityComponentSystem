#pragma once

#include "assert.h"
#include <chrono>
#include <queue>

#define profile_function \
    static ProfileSample sample(__func__); \
    ProfileScope2 profile(sample);

#define profile_name(Name) \
    static ProfileSample sample_##Name(#Name); \
    ProfileScope2 profile_##Name(sample_##Name);

namespace NodeVision::Profiling
{
    class StopWatch
    {
    private:
        typedef std::chrono::high_resolution_clock clock;
        typedef std::chrono::duration<uint64_t, std::pico> picoseconds;
        typedef std::chrono::duration<uint64_t, std::milli> milliseconds;
        typedef std::chrono::duration<uint64_t, std::micro> microseconds;
        typedef std::chrono::duration<double, typename clock::period> Cycle;

    public:
        StopWatch()
            : isRunning(false)
        {
        }

        inline void Start()
        {
            assert(!isRunning);
            start = end = clock::now();
            isRunning = true;
        }

        inline void Restart()
        {
            start = end = clock::now();
            isRunning = true;
        }

        inline void Stop()
        {
            assert(isRunning);
            end = clock::now();
            isRunning = false;
        }

        inline uint64_t GetElapsedPicoseconds() const
        {
            auto current = isRunning ? clock::now() : clock::now();
            auto ticks_per_iter = Cycle(current - start) / 1;
            return std::chrono::duration_cast<picoseconds>(ticks_per_iter).count();
        }

        inline uint64_t GetElapsedMicroseconds() const
        {
            auto current = isRunning ? clock::now() : clock::now();
            auto ticks_per_iter = Cycle(current - start) / 1;
            return std::chrono::duration_cast<microseconds>(ticks_per_iter).count();
        }

        inline uint64_t GetElapsedMiliseconds() const
        {
            auto current = isRunning ? clock::now() : clock::now();
            auto ticks_per_iter = Cycle(current - start) / 1;
            return std::chrono::duration_cast<milliseconds>(ticks_per_iter).count();
        }

    private:
        std::chrono::high_resolution_clock::time_point start, end;
        bool isRunning;
    };

    struct ProfileSample
    {
        ProfileSample(const char* name) : Name(name), ElapsedPicoseconds(0), CallCount(0) {}
        const char* Name;
        uint64_t ElapsedPicoseconds;
        int CallCount;
    };

    enum class MarkType
    {
        Begin,
        End,
    };

    struct Mark
    {
        ProfileSample* Sample;
        MarkType Type;
        std::chrono::steady_clock::time_point TimePoint;
    };

    class ProfileManager
    {
    public:
        void BeginSample(ProfileSample* profilingSample)
        {
            Mark mark;
            mark.Sample = profilingSample;
            mark.Type = MarkType::Begin;
            mark.TimePoint = std::chrono::high_resolution_clock::now();
            Marks.push_back(mark);
        }

        void EndSample(ProfileSample* profilingSample)
        {
            //assert(Marks.back().Sample == profilingSample);
            Mark mark;
            mark.Sample = profilingSample;
            mark.Type = MarkType::End;
            mark.TimePoint = std::chrono::high_resolution_clock::now();
            Marks.push_back(mark);
        }

        void Clear() { Marks.clear(); }

        const std::vector<Mark>& GetMarks() const { return Marks; }

    private:
        std::vector<Mark> Marks;
    };

    class ProfileHierarchySnapshot
    {
    private:
        typedef std::chrono::high_resolution_clock clock;
        typedef std::chrono::duration<uint64_t, std::pico> picoseconds;
        typedef std::chrono::duration<uint64_t, std::milli> milliseconds;
        typedef std::chrono::duration<uint64_t, std::micro> microseconds;
        typedef std::chrono::duration<double, typename clock::period> Cycle;

    public:
        struct ProfileItem
        {
            ProfileSample* Sample;
            std::chrono::steady_clock::time_point Begin;
            std::chrono::steady_clock::time_point End;
            int indent;
        };

        void Build(ProfileManager& profileManager)
        {
            Indent = 0;

            auto& marks = profileManager.GetMarks();

            auto begin = marks.cbegin();
            auto end = marks.cend();

            while (begin != end)
            {
                CreateItem(begin, end);
                begin++;
            }
        }

        void CreateItem(const std::vector<Mark>::const_iterator begin, const std::vector<Mark>::const_iterator end)
        {
            auto startMark = *begin;

            switch (startMark.Type)
            {
            case MarkType::Begin:
                {
                    auto sample = startMark.Sample;
                    auto target = std::find_if(begin, end, [&](Mark value) { return value.Sample == sample && value.Type == MarkType::End; });

                    assert(target != end);

                    auto endMark = *target;

                    ProfileItem item;
                    item.Sample = sample;
                    item.Begin = startMark.TimePoint;
                    item.End = endMark.TimePoint;
                    item.indent = Indent;
                    Items.push_back(item);

                    Indent++;
                    break;
                }
            case MarkType::End:
                Indent--;
                break;
            default:
                assert(false);
                break;
            }
        }

        void Print()
        {
            auto maxTime = FindMaxTime();
            for (auto& item : Items)
            {
                for (int i = 0; i < item.indent; ++i)
                    printf("  ");

                auto ticks_per_iter = Cycle(item.End - item.Begin) / 1;
                auto time = std::chrono::duration_cast<picoseconds>(ticks_per_iter).count();
                auto timeProcentage = maxTime != 0 ? ((float)time / float(maxTime)) * 100 : 0;

                auto timeMs = (float)time * 1e-9;

                printf("%*s %*.2f %% %*.4f ms\n", -25, item.Sample->Name, 10- item.indent * 2, timeProcentage, 10, timeMs);
            }
            printf("\n");
        }

        const std::vector<ProfileItem>& GetItems() const { return Items; }

    private:
        uint64_t FindMaxTime()
        {
            uint64_t max = 0;
            for (auto& item : Items)
            {
                auto ticks_per_iter = Cycle(item.End - item.Begin) / 1;
                auto ms = std::chrono::duration_cast<picoseconds>(ticks_per_iter).count();

                max = std::max(max, ms);
            }
            return max;
        }

    private:
        std::vector<ProfileItem> Items;
        int Indent;
    };

    static thread_local ProfileManager* g_ProfileManager = nullptr;
    ProfileManager* GetProfileManager() { return g_ProfileManager; }
    void SetProfileManager(ProfileManager* profileManager) { g_ProfileManager = profileManager; }

    class Profiler
    {
    public:
        static void BeginSample(ProfileSample& sample)
        {
            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->BeginSample(&sample);
        }

        static void EndSample(ProfileSample& sample)
        {
            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->EndSample(&sample);
        }
    };

    class ProfileScope
    {
    public:
        ProfileScope(ProfileSample& sample) : Sample(sample)
        {
            Sample.CallCount++;
            StopWatch.Start();

            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->BeginSample(&Sample);
        }

        ~ProfileScope()
        {
            StopWatch.Stop();
            Sample.ElapsedPicoseconds += StopWatch.GetElapsedPicoseconds();

            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->EndSample(&Sample);
        }

        ProfileSample& Sample;
        StopWatch StopWatch;
    };

    class ProfileScope2
    {
    public:
        ProfileScope2(ProfileSample& sample) : Sample(sample)
        {
            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->BeginSample(&Sample);
        }

        ~ProfileScope2()
        {
            auto profileManager = GetProfileManager();
            if (profileManager != nullptr)
                profileManager->EndSample(&Sample);
        }

        ProfileSample& Sample;
    };
}