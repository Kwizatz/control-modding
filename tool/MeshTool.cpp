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

#include <fstream>
#include <sstream>
#include <ostream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string_view>
#include <cstring>
#include "BinFBX.h"
#include "MeshTool.h"

namespace ControlModding
{
    MeshTool::MeshTool() = default;
    MeshTool::~MeshTool() = default;
    void MeshTool::ProcessArgs ( int argc, char** argv )
    {
        if ( argc < 2 || ( strcmp ( argv[1], "binfbx" ) != 0 ) )
        {
            std::ostringstream stream;
            stream << "Invalid tool name, expected \"binfbx\", got " << ( ( argc < 2 ) ? "nothing" : argv[1] ) << std::endl;
            throw std::runtime_error ( stream.str().c_str() );
        }
        for ( int i = 2; i < argc; ++i )
        {
            if ( argv[i][0] == '-' )
            {
                if ( argv[i][1] == '-' )
                {
                    if ( strncmp ( &argv[i][2], "in", sizeof ( "in" ) ) == 0 )
                    {
                        i++;
                        mInputFile = argv[i];
                    }
                    else if ( strncmp ( &argv[i][2], "out", sizeof ( "out" ) ) == 0 )
                    {
                        i++;
                        mOutputFile = argv[i];
                    }
                }
                else
                {
                    switch ( argv[i][1] )
                    {
                    case 'i':
                        i++;
                        mInputFile = argv[i];
                        break;
                    case 'o':
                        i++;
                        mOutputFile = argv[i];
                        break;
                    }
                }
            }
            else
            {
                mInputFile = argv[i];
            }
        }
        if ( mInputFile.empty() )
        {
            throw std::runtime_error ( "No Input file provided." );
        }
        else if ( mOutputFile.empty() )
        {
            mOutputFile = mInputFile;
        }
    }

    template<class T>
    uint8_t* PrintArray(uint8_t* aIndex, std::string_view aName, uint32_t* aCount = nullptr)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        std::cout << aName << " ("<< count << ")";
        for(uint32_t i = 0;i<count;++i)
        {
            std::cout << "\t" << reinterpret_cast<T*>(aIndex)[0];
            aIndex+=sizeof(T);
        }
        std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArray<char>(uint8_t* aIndex, std::string_view aName, uint32_t* aCount)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        std::cout << aName << " ("<< count << ") ";
        for(uint32_t i = 0;i<count;++i)                        
        {
            if(aIndex[0]!=0)
            {
                std::cout << aIndex[0];
            }
            aIndex+=1;
        }
        std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArray<uint8_t>(uint8_t* aIndex, std::string_view aName, uint32_t* aCount)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        std::cout << aName << " ("<< count << ")";
        for(uint32_t i = 0;i<count;++i)
        {
            std::cout << " " << static_cast<uint32_t>(aIndex[0]);
            aIndex+=1;
        }
        std::cout << std::endl;
        return aIndex;
    }

    template<class T>
    uint8_t* PrintArrayCount(uint8_t* aIndex, std::string_view aName, uint32_t aCount)
    {
        std::cout << aName << " ("<< aCount << ")";
        for(uint32_t i = 0;i<aCount;++i)
        {
            std::cout << "\t" << reinterpret_cast<T*>(aIndex)[0];
            aIndex+=sizeof(T);
        }
        std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArrayCount<uint8_t>(uint8_t* aIndex, std::string_view aName, uint32_t aCount)
    {
        std::cout << aName << " ("<< aCount << ")";
        for(uint32_t i = 0;i<aCount;++i)
        {
            std::cout << " " << static_cast<uint32_t>(aIndex[0]);
            aIndex+=1;
        }
        std::cout << std::endl;
        return aIndex;
    }

    template<class T>
    uint8_t* PrintSingle(uint8_t* aIndex, std::string_view aName, T* aOutput = nullptr)
    {
        std::cout << aName << "\t" << reinterpret_cast<T*>(aIndex)[0] << std::endl;
        if(aOutput){*aOutput = reinterpret_cast<T*>(aIndex)[0];}
        aIndex+=sizeof(T);
        return aIndex;
    }

    int MeshTool::operator() ( int argc, char** argv )
    {
        ProcessArgs ( argc, argv );
        std::ifstream file;
        file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
        file.open ( mInputFile, std::ifstream::in | std::ifstream::binary );
        std::vector<uint8_t> buffer ( ( std::istreambuf_iterator<char> ( file ) ), ( std::istreambuf_iterator<char>() ) );
        file.close();
        uint8_t* index = buffer.data();
        Header* binfbx = reinterpret_cast<Header*>(buffer.data());
        if(binfbx->Magick!=BinFBXMagick)
        {
            std::cout << "Not a BinFBX file " << binfbx->Magick << std::endl;
            return -1;
        }        

        index = PrintSingle<uint32_t>(index,"Magick");
        index = PrintSingle<uint32_t>(index,"Attribute Buffer Size");
        index = PrintSingle<uint32_t>(index,"Vertex Buffer Size");
        index = PrintSingle<uint32_t>(index,"Index Count");
        index = PrintSingle<uint32_t>(index,"Index Size");
        
        // Move Past the Buffers
        index += binfbx->AttributeBufferSize + binfbx->VertexBufferSize + (binfbx->IndexCount * binfbx->IndexSize);

        uint32_t JointCount = *reinterpret_cast<uint32_t*>(index);
        std::cout << "Joint Count " << JointCount << std::endl;
        index += sizeof(uint32_t);

        for(uint32_t i = 0; i < JointCount; ++i)
        {
            index = PrintArray<char>(index,"Joint Name");
            index = PrintArrayCount<float>(index,"Matrix 1",4);
            index = PrintArrayCount<float>(index,"Matrix 2",4);
            index = PrintArrayCount<float>(index,"Matrix 3",4);
            index = PrintArrayCount<float>(index,"Matrix 4",4);
            index = PrintSingle<uint32_t>(index,"Parent Index");
        }


        std::cout << "Location " << std::hex << static_cast<size_t>(index - buffer.data()) << std::endl; std::cout << std::dec;

        index = PrintArrayCount<float>(index,"Up Vector",3);  // Up Vector
        index = PrintArray<float>(index,"Unknown Variable Array");
        index = PrintArrayCount<float>(index,"Unknown Fixed Array",11);
        index = PrintSingle<uint32_t>(index, "Model Count");
        uint32_t material_count;
        index = PrintSingle<uint32_t>(index, "Material Count", &material_count);


        //---- Materials
        for(uint32_t i = 0; i < material_count;++i )
        {
            std::cout << "Location " << std::hex << static_cast<size_t>(index - buffer.data()) << std::endl; std::cout << std::dec;
            index = PrintSingle<uint32_t>(index,"Unknown Uint32");
            index = PrintArrayCount<uint8_t>(index,"Unknown Fixed Array",8);

            index = PrintArray<char>(index,"Material Name");
            index = PrintArray<char>(index,"Material Type");
            index = PrintArray<char>(index,"Material Path");

            index = PrintArrayCount<uint32_t>(index,"Unknown Fixed Array",6);

            uint32_t UniformCount;
            index = PrintSingle<uint32_t>(index,"Uniform Count",&UniformCount);

            for(uint32_t j=0;j<UniformCount;++j)
            {
                index = PrintArray<char>(index,"Uniform Variable Name");
                uint32_t UniformType;
                index = PrintSingle<uint32_t>(index,"Uniform Variable Type",&UniformType);
            
                switch(UniformType)
                {
                case Float:
                    index = PrintSingle<float>(index,"Float");
                    break;
                case Range:
                    index = PrintArrayCount<float>(index,"Range",2);
                    break;
                case Color:
                    index = PrintArrayCount<float>(index,"Color",4);
                    break;
                case Vector:
                    index = PrintArrayCount<float>(index,"Vec3",3);
                    break;
                case TextureMap:
                    index = PrintArray<char>(index,"TextureMap");
                    break;
                case TextureSampler:
                    break;
                case Boolean:
                    index = PrintSingle<uint32_t>(index,"Boolean");
                    break;
                }
            }
        }

        uint32_t material_map{};
        index = PrintArray<uint32_t>(index,"Material Map",&material_map);
        uint32_t alternate_material_count{};
        index = PrintSingle<uint32_t>(index,"Alternate Material Count", &alternate_material_count);

        for(uint32_t i = 0; i<alternate_material_count;++i)
        {
            index =  PrintArray<char>(index,"Alt Material");
            index =  PrintArrayCount<uint32_t>(index,"Material Map",material_map);
        }

        index = PrintArray<uint32_t>(index,"Material Map");

        uint32_t submesh_count1;
        index = PrintSingle<uint32_t>(index,"SubMesh Count 1",&submesh_count1);
        for(uint32_t i = 0; i<submesh_count1;++i)
        {
            index = PrintSingle<uint32_t>(index,std::string("LOD ")+std::to_string(i+1));
            index = PrintSingle<uint32_t>(index,"Vertex Count");
            index = PrintSingle<uint32_t>(index,"Triangle Count");
            index = PrintSingle<uint32_t>(index,"Vertex Attribute Offset");
            index = PrintSingle<uint32_t>(index,"Vertex Buffer Offset");
            index = PrintSingle<uint32_t>(index,"Index Buffer Offset");

            std::cout << std::hex;
            index = PrintArrayCount<uint32_t>(index,"Unknown",12);
            std::cout << std::dec;

            uint16_t attribute_count;
            index = PrintSingle<uint16_t>(index,"Attribute Count", &attribute_count);
            uint16_t attribute_size;
            index = PrintSingle<uint16_t>(index,"Attribute Size", &attribute_size);
        
            std::cout << std::hex;
            PrintArrayCount<uint32_t>(index,"ParamBlockHashes",attribute_count);
            index = PrintArrayCount<uint16_t>(index,"ParamBlockHashes",attribute_count*attribute_size);
            std::cout << std::dec;

            index = PrintSingle<uint32_t>(index,"Unknown");
            index = PrintSingle<int16_t>(index,"Unknown");
            index = PrintSingle<float>(index,"Unknown");
        }

        uint32_t submesh_count2;
        index = PrintSingle<uint32_t>(index,"SubMesh Count 2",&submesh_count2);
        for(uint32_t i = 0; i< submesh_count2;++i)
        {
            index = PrintSingle<uint32_t>(index,std::string("LOD ")+std::to_string(i+1));
            index = PrintSingle<uint32_t>(index,"Vertex Count");
            index = PrintSingle<uint32_t>(index,"Triangle Count");
            index = PrintSingle<uint32_t>(index,"Vertex Attribute Offset");
            index = PrintSingle<uint32_t>(index,"Vertex Buffer Offset");
            index = PrintSingle<uint32_t>(index,"Index Buffer Offset");

            index = PrintArrayCount<uint32_t>(index,"Unknown",12);

            uint16_t table_var_count;
            index = PrintSingle<uint16_t>(index,"Table Var Count",&table_var_count);
            index = PrintSingle<uint16_t>(index,"Unknown padding maybe?");
        
            uint32_t ParamBlockHashes;
            index = PrintArrayCount<uint32_t>(index,"ParamBlockHashes",table_var_count);

            index = PrintSingle<uint32_t>(index,"Unknown");
            index = PrintSingle<int16_t>(index,"Unknown");
            index = PrintSingle<float>(index,"Unknown");
        }
        std::cout << "File Size " << buffer.size() << " Index Location " << static_cast<size_t>(index - buffer.data()) << std::endl;

        return 0;
    }
}
