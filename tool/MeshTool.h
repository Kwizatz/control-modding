/*
Copyright (C) 2021 Rodrigo Jose Hernandez Cordoba

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
#ifndef CONTROL_TOOL_MESH_H
#define CONTROL_TOOL_MESH_H
#include <string>
#include <stdexcept>
#include "Tool.h"

namespace ControlModding
{
    class MeshTool : public Tool
    {
    public:
        MeshTool();
        ~MeshTool();
        int operator() ( int argc, char** argv ) override;
    private:
        void ProcessArgs ( int argc, char** argv );
        std::string mInputFile;
        std::string mOutputFile;
#ifdef USE_SQLITE
        std::string mSqliteFile;
#endif
    };
}
#endif
