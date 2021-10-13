#pragma once

#include "NodeVision.Entities.hpp"
#include "NodeVision.Jobs.hpp"

using namespace NodeVision::Entities;
using namespace NodeVision::Jobs;

namespace NodeVision::CommandBuffer
{
    class EndSimulationCommandBufferSystem : public System 
    {
    public:
        using System::System;

        EntityCommandBuffer* GetBuffer()
        {
            if (FreeBuffers.empty())
            {
                auto buffer = new EntityCommandBuffer(Manager);
                UsedBuffers.push_back(buffer);
                return buffer;
            }
            else
            {
                auto buffer = UsedBuffers.back();
                UsedBuffers.pop_back();
                return buffer;
            }
        }

        void AddProducer(JobHandle jobHandle)
        {
            Dependencies.push_back(jobHandle);
        }

        virtual void OnUpdate()
        {
            for (auto dependency : Dependencies)
            {
                GetWorkerManager().Complete(dependency);
            }
            Dependencies.clear();

            for (auto buffer : UsedBuffers)
            {
                buffer->Execute();
                FreeBuffers.push_back(buffer);
            }
            UsedBuffers.clear();
        }

        virtual void OnDestroy()
        {
            for (auto buffer : UsedBuffers)
            {
                delete buffer;
            }
        }

    private:
        std::vector<JobHandle> Dependencies;
        std::vector<EntityCommandBuffer*> FreeBuffers;
        std::vector<EntityCommandBuffer*> UsedBuffers;
    };
}