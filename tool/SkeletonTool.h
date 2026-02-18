/*
Copyright (C) 2025,2026 Rodrigo Jose Hernandez Cordoba

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef CONTROL_TOOL_SKELETON_H
#define CONTROL_TOOL_SKELETON_H
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include "Tool.h"

namespace ControlModding
{
    class SkeletonTool : public Tool
    {
    public:
        SkeletonTool();
        ~SkeletonTool();
        int operator() ( int argc, char** argv ) override;
    private:
        void ProcessArgs ( int argc, char** argv );
        struct SubSectionIndex
        {
            uint32_t mStart{};
            uint32_t mSize{};
            uint32_t mCount{};
            uint32_t mOffsets[0];
        };
        struct Transform
        {
            float mRotation[4];
            float mPosition[4];
        };
        struct NameArray
        {
            uint64_t mOffset{};
            uint64_t mCount{};
        };
        std::string mInputFile;
        int64_t mA{0};
        int64_t mB{0};
        bool mDump{false};
    };
}
#endif
