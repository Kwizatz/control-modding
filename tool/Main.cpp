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
#include <iostream>
#include <unordered_map>
#include <functional>
#include <memory>

#include <cassert>
#include <cstdint>
#include "Tool.h"
#include "MeshTool.h"

int main ( int argc, char *argv[] )
{
    std::unordered_map<std::string, std::function<std::unique_ptr<ControlModding::Tool>() > > ToolFactories
    {
        { "binfbx", [] { return std::make_unique<ControlModding::MeshTool>(); } },
    };
    try
    {
        int retval = 0;
        if ( argc >= 2 && ToolFactories.find ( argv[1] ) != ToolFactories.end() )
        {
            retval = ToolFactories[argv[1]]()->operator() ( argc, argv );
        }
        else
        {
            std::cout << "Usage: " << argv[0] << " <tool> [-help | ...]" << std::endl;
            std::cout << "Available tools:" << std::endl;
            for(auto& i: ToolFactories)
            {
                std::cout << "  " << i.first << std::endl;
            }
        }
        return retval;
    }
    catch ( std::runtime_error &e )
    {
        std::cout << e.what() << std::endl;
    }
    catch ( ... )
    {
        std::cout << "Error: Unknown Exception caught." << std::endl;
    }
    return -1;
}
