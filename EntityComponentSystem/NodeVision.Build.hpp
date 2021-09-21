#pragma once

#include <map>
#include "NodeVision.Collections.hpp"
#include "NodeVision.Entities.hpp"
#include "NodeVision.Jobs.hpp"

namespace NodeVision::Build
{
    struct StructuredPath
    {
        StructuredPath() {}

        StructuredPath(const char* directory, const char* name, const char* extension) :
            Directory(directory),
            Name(name),
            Extension(extension)
        {}

        StructuredPath(const char* path)
        {
            auto directoryEnd = strrchr(path, '/');
            auto extensionEnd = strrchr(path, '.');
            assert(directoryEnd != nullptr);
            assert(extensionEnd != nullptr);

            Directory = FixedString256(path, directoryEnd - path);
            Name = FixedString128(directoryEnd + 1, extensionEnd - directoryEnd - 1);
            Extension = FixedString64(extensionEnd + 1);
        }

        StructuredPath(const std::string& path)
        {
            auto directoryEnd = path.find_last_of('/');
            auto extensionEnd = path.find_last_of('.');
            assert(directoryEnd != -1);
            assert(extensionEnd != -1);

            Directory = FixedString256(path.c_str(), directoryEnd);
            Name = FixedString128(path.c_str() + directoryEnd + 1, extensionEnd - directoryEnd - 1);
            Extension = FixedString64(path.c_str() + extensionEnd + 1, path.size() - extensionEnd - 1);
        }

        bool IsDirectoryAny() const { return Directory == "*"; }
        bool IsNameAny() const { return Name == "*"; }
        bool IsExtensionAny() const { return Extension == "*"; }

        FixedString256 Directory;
        FixedString128 Name;
        FixedString64 Extension;
    };

    class BuildStep
    {
    public:
        std::vector<StructuredPath> Inputs;
        StructuredPath Output;
    };

    class BuildGraph
    {
    public:
        struct Node
        {
            Node() : Step(nullptr) {}
            Node(const FixedString256& output) : Step(nullptr), Output(output) {}
            bool Executable() const { return Step != nullptr; }

            std::vector<Node*> Dependencies;
            BuildStep* Step;
            FixedString256 Output;
        };

        template<class T>
        BuildStep* GetOrCreateBuildStep()
        {
            auto step = new T();
            Steps.push_back(step);
            return step;
        }

        bool Build(const std::vector<std::string>& outputs)
        {
            profile_function;
            RootNode = new Node();
            Nodes.push_back(RootNode);

            {
                profile_name(AddOutputs);
                for (auto output : outputs)
                {
                    auto node = new Node(output);
                    Nodes.push_back(node);
                    RootNode->Dependencies.push_back(node);

                    NodesNeedStep.push(node);
                }
            }

            while (!NodesNeedStep.empty())
            {
                auto node = NodesNeedStep.front();
                NodesNeedStep.pop();

                auto step = FindStepForOutput(node->Output);
                if (step == nullptr)
                {
                    continue;
                }

                node->Step = step;
                for (auto input : step->Inputs)
                {
                    auto inputPath = GenerateInput(input, node->Output);

                    auto inputNode = new Node(inputPath);
                    Nodes.push_back(inputNode);
                    node->Dependencies.push_back(inputNode);
                    NodesNeedStep.push(inputNode);
                }
            }

            return true;
        }

        Node* GetRootNode() const { return RootNode; }

    private:
        BuildStep* FindStepForOutput(const FixedString256& output)
        {
            StructuredPath structuredOutput(output);
            for (auto step : Steps)
            {
                bool directoryCheck = step->Output.IsDirectoryAny() ? true : step->Output.Directory == structuredOutput.Directory;
                bool nameCheck = step->Output.IsNameAny() ? true : step->Output.Name == structuredOutput.Name;
                bool extensionCheck = step->Output.IsExtensionAny() ? true : step->Output.Extension == structuredOutput.Extension;
                if (directoryCheck && nameCheck && extensionCheck)
                    return step;
            }
            return nullptr;
        }

        FixedString256 GenerateInput(const StructuredPath& input, const FixedString256& output)
        {
            FixedString256 path;

            StructuredPath structuredOutput(output);

            if (input.IsDirectoryAny())
            {
                path += structuredOutput.Directory;
            }
            else
            {
                path += input.Directory;
            }

            path += "/";

            if (input.IsNameAny())
            {
                path += structuredOutput.Name;
            }
            else
            {
                path += input.Name;
            }

            path += ".";

            if (input.IsExtensionAny())
            {
                path += structuredOutput.Extension;
            }
            else
            {
                path += input.Extension;
            }

            return path;
        }

    private:
        std::vector<BuildStep*> Steps;
        std::queue<Node*> NodesNeedStep;

        Node* RootNode;
        std::vector<Node*> Nodes;
    };

    class BuildGraphExecuter
    {
    public:
        JobHandle Schedule(WorkerManager& workerManager, BuildGraph& graph)
        {
            auto rootNode = graph.GetRootNode();


        }
    };
}