/*
Copyright (C) 2021,2022 Rodrigo Jose Hernandez Cordoba

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
#include <cmath>
#include <bitset>

#include "BinFBX.h"
#include "MeshTool.h"
namespace ControlModding
{
    bool bPrint = true;
    MeshTool::MeshTool() = default;
    MeshTool::~MeshTool() = default;
    enum PrintType
    {
        None,
        Binary,
        Hexadecimal,
        Decimal,
    };

    std::ostream& operator<<(std::ostream& os, const AttributeInfo& aAttributeInfo){
        os << "\nBufferLocation " << static_cast<uint32_t>(aAttributeInfo.Index) << "\nType ";

        switch(aAttributeInfo.Type)
        {
        case 0x2:
            os << "R32G32B32_FLOAT";
            break;
        case 0x4:
            os << "B8G8R8A8_UNORM";
            break;
        case 0x7:
         os << "R16G16_SINT";
         break;
        case 0x8:
         os << "R16G16B16A16_SINT";
         break;
        case 0xd:
         os << "R16G16B16A16_UINT";
         break;
        case 0x5:
         os << "R8G8B8A8_UINT";
         break;
        default:
            os << "Unknown ("<< static_cast<uint32_t>(aAttributeInfo.Type) <<")";
        }

        os << "\nSemantic ";
        switch(aAttributeInfo.Usage)
        {
        case 0x0:
            os << "Position";
            break;
        case 0x1:
            os << "Normal";
            break;
        case 0x2:
            os << "TexCoord";
            break;
        case 0x3:
            os << "Tangent";
            break;
        case 0x5:
            os << "Index";
            break;
        case 0x6:
            os << "Weight";
            break;
        default:
            os << "Unknown ("<< static_cast<uint32_t>(aAttributeInfo.Usage) <<")";
            break;
        }
        os << "\nZero " << static_cast<uint32_t>(aAttributeInfo.Zero);
        return os;
    }

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
                    else if ( strncmp ( &argv[i][2], "remove", sizeof ( "remove" ) ) == 0 )
                    {

                        MeshReference mesh_reference{};
                        if(i + 2 < argc)
                        {
                            errno = 0;
                            mesh_reference.mGroup = std::strtoul(argv[i + 1], nullptr, 10);
                            if(errno!=0)
                            {
                                std::ostringstream stream;
                                stream << "Invalid group number, expected unsigned integer, got " << argv[i + 1] << std::endl;
                                throw std::runtime_error ( stream.str().c_str() );
                            }
                            errno = 0;
                            mesh_reference.mIndex = std::strtoul(argv[i + 2], nullptr, 10);
                            if(errno!=0)
                            {
                                std::ostringstream stream;
                                stream << "Invalid index number, expected unsigned integer, got " << argv[i + 2] << std::endl;
                                throw std::runtime_error ( stream.str().c_str() );
                            }
                        }
                        else
                        {
                            throw std::runtime_error ( "Remove argument missing, expected \"remove <mesh group> <mesh index>\"" );
                        }
                        i+=2;
                        mRemove.push_back(mesh_reference);
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
#if 0
        else if ( mOutputFile.empty() )
        {
            mOutputFile = mInputFile;
        }
#endif
    }

    template<class T>
    uint8_t* PrintArray(uint8_t* aIndex, std::string_view aName, uint32_t* aCount = nullptr)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        if(bPrint) std::cout << aName << " ("<< count << ")";
        for(uint32_t i = 0;i<count;++i)
        {
            if(bPrint) std::cout << "\t" << reinterpret_cast<T*>(aIndex)[0];
            aIndex+=sizeof(T);
        }
        if(bPrint) std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArray<char>(uint8_t* aIndex, std::string_view aName, uint32_t* aCount)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        if(bPrint) std::cout << aName << " ("<< count << ") ";
        for(uint32_t i = 0;i<count;++i)                        
        {
            if(aIndex[0]!=0)
            {
                if(bPrint)  std::cout << aIndex[0];
            }
            aIndex+=1;
        }
        if(bPrint) std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArray<uint8_t>(uint8_t* aIndex, std::string_view aName, uint32_t* aCount)
    {
        uint32_t count = reinterpret_cast<uint32_t*>(aIndex)[0];
        if(aCount){*aCount = count;}
        aIndex+=sizeof(uint32_t);
        if(bPrint)  std::cout << aName << " ("<< count << ")";
        for(uint32_t i = 0;i<count;++i)
        {
            if(bPrint) std::cout << " " << static_cast<uint32_t>(aIndex[0]);
            aIndex+=1;
        }
        if(bPrint) std::cout << std::endl;
        return aIndex;
    }

    template<class T>
    uint8_t* PrintArrayCount(uint8_t* aIndex, std::string_view aName, uint32_t aCount, PrintType aPrintType = Decimal)
    {
        if(aPrintType!=None) std::cout << aName << " ("<< aCount << ")";
        for(uint32_t i = 0;i<aCount;++i)
        {
            if(aPrintType!=None)
            {
                switch(aPrintType)
                {
                case Binary:
                    std::cout << "\t" << std::bitset<sizeof(T)*8>(reinterpret_cast<T*>(aIndex)[0]);
                    break;
                case Decimal:
                    std::cout << "\t" << std::dec << reinterpret_cast<T*>(aIndex)[0];
                    break;
                case Hexadecimal:
                    std::cout << "\t" << std::hex << reinterpret_cast<T*>(aIndex)[0];
                    std::cout << std::dec;
                    break;
                }
            }
            aIndex+=sizeof(T);
        }
        if(aPrintType!=None) std::cout << std::endl;
        return aIndex;
    }

    template<>
    uint8_t* PrintArrayCount<uint8_t>(uint8_t* aIndex, std::string_view aName, uint32_t aCount, PrintType aPrintType)
    {
        if(aPrintType!=None) std::cout << aName << " ("<< aCount << ")";
        for(uint32_t i = 0;i<aCount;++i)
        {
            if(aPrintType!=None)
            {
                switch(aPrintType)
                {
                case Binary:
                    std::cout << "\t" << std::bitset<8>(static_cast<uint32_t>(aIndex[0]));
                    break;
                case Decimal:
                    std::cout << " " << std::dec << static_cast<uint32_t>(aIndex[0]);
                    break;
                case Hexadecimal:
                    std::cout << " " << std::hex << static_cast<uint32_t>(aIndex[0]);
                    std::cout << std::dec;
                    break;
                }
            }
            aIndex+=1;
        }
        if(aPrintType!=None)  std::cout << std::endl;
        return aIndex;
    }

    template<class T>
    uint8_t* PrintSingle(uint8_t* aIndex, std::string_view aName, T* aOutput = nullptr)
    {
        if(bPrint) std::cout << aName << "\t" << reinterpret_cast<T*>(aIndex)[0] << std::endl;
        if(aOutput){*aOutput = reinterpret_cast<T*>(aIndex)[0];}
        aIndex+=sizeof(T);
        return aIndex;
    }

    template<>
    uint8_t* PrintSingle<uint8_t>(uint8_t* aIndex, std::string_view aName, uint8_t* aOutput)
    {
        if(bPrint) std::cout << aName << "\t" << static_cast<size_t>(*aIndex) << std::endl;
        if(aOutput){*aOutput = *aIndex;}
        aIndex+=sizeof(uint8_t);
        return aIndex;
    }


    uint8_t* PrintMesh(uint8_t* aIndex, std::string_view aName)
    {
            aIndex = PrintSingle<uint32_t>(aIndex, aName);
            aIndex = PrintSingle<uint32_t>(aIndex,"Vertex Count");
            aIndex = PrintSingle<uint32_t>(aIndex,"Triangle Count");
            aIndex = PrintSingle<uint32_t>(aIndex,"Vertex Attribute Offset");
            aIndex = PrintSingle<uint32_t>(aIndex,"Vertex Buffer Offset");
            aIndex = PrintSingle<uint32_t>(aIndex,"Index Buffer Offset");

            aIndex = PrintSingle<int32_t>(aIndex,"Unknown Int");
            aIndex = PrintArrayCount<int32_t>(aIndex,"Bounding Sphere", 4);
            aIndex = PrintArrayCount<int32_t>(aIndex,"Bounding Box", 6);

            aIndex = PrintSingle<int32_t>(aIndex,"Vertex Format Int32 (Unknown)");
            uint8_t vertex_format_count{};
            aIndex = PrintSingle<uint8_t>(aIndex,"Vertex Format Count (byte)", &vertex_format_count);
            aIndex = PrintArrayCount<AttributeInfo>(aIndex,"Vertex Format", vertex_format_count);
            aIndex = PrintSingle<int32_t>(aIndex,"Unknown Int");
            aIndex = PrintSingle<float>(aIndex,"Unknown Float");
            aIndex = PrintSingle<uint8_t>(aIndex,"Unknown Byte as Bool");
            aIndex = PrintSingle<float>(aIndex,"Unknown Float");

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

        BinFBX binfbx{buffer};

        // Remove Meshes
        for(auto& mesh_reference : mRemove)
        {
            binfbx.RemoveMesh(mesh_reference.mGroup, mesh_reference.mIndex);
        }

        if(!mOutputFile.empty())
        {
            binfbx.Write(mOutputFile);
        }
        else
        {
            binfbx.Dump();
        }
        return 0;
    }
}
