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
#include <cmath>
#include <bitset>
#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

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
#ifdef USE_SQLITE
                    else if ( strncmp ( &argv[i][2], "sqlite", sizeof ( "sqlite" ) ) == 0 )
                    {
                        i++;
                        mSqliteFile = argv[i];
                    }
#endif
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
#ifdef USE_SQLITE
                    case 's':
                        i++;
                        mSqliteFile = argv[i];
                        break;
#endif
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
        if(!mOutputFile.empty())
        {
            binfbx.Write(mOutputFile);
        }
        else
        {
            binfbx.Dump();
        }
        return 0;
#if 0
        uint8_t* index = buffer.data();
        Header* header = reinterpret_cast<Header*>(buffer.data());
        if(header->Magick!=BinFBXMagick)
        {
            std::cout << "Not a BinFBX file " << header->Magick << std::endl;
            return -1;
        }        
#if 0
#ifdef USE_SQLITE
        sqlite3 *db;
        if(!mSqliteFile.empty())
        {
            sqlite3_stmt *res;
            
            int rc = sqlite3_open(mSqliteFile.c_str(), &db);
            
            if (rc != SQLITE_OK) 
            {
                
                std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_close(db);                
                return 1;
            }
        }
#endif
#endif

        index = PrintSingle<uint32_t>(index,"Magick");
        index = PrintSingle<uint32_t>(index,"Attribute Buffer Size");
        index = PrintSingle<uint32_t>(index,"Vertex Buffer Size");
        index = PrintSingle<uint32_t>(index,"Index Count");
        index = PrintSingle<uint32_t>(index,"Index Size");
        
        // Move Past the Buffers
        index += header->VertexBufferSizes[0] + header->VertexBufferSizes[1] + (header->IndexCount * header->IndexSize);

        uint32_t JointCount = *reinterpret_cast<uint32_t*>(index);
        std::cout << "Joint Count " << JointCount << std::endl;
        index += sizeof(uint32_t);
#if 0
#ifdef USE_SQLITE
        //std::cout << "Working on " << mInputFile << std::endl;
        char *errmsg{nullptr};
        int rc = sqlite3_exec(
        db,
        R"(CREATE TABLE IF NOT EXISTS general (
            file_path TEXT PRIMARY KEY,
            attribute_buffer_size INTEGER NOT NULL,
            vertex_buffer_size INTEGER NOT NULL,
            index_count INTEGER NOT NULL,
            index_size INTEGER NOT NULL,
            joint_count INTEGER NOT NULL
        );)",
        nullptr,//int (*callback)(void*,int,char**,char**),  /* Callback function */
        nullptr,                                             /* 1st argument to callback */
        &errmsg);

        if(errmsg != nullptr)
        {
            std::cerr << "Table creation Failed: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }

        std::stringstream insert;
        insert << "INSERT OR REPLACE INTO general (file_path, attribute_buffer_size, vertex_buffer_size, index_count, index_size, joint_count) values " <<
        "(\"" << mInputFile << "\"," << binfbx->AttributeBufferSize << "," << binfbx->VertexBufferSize << "," << binfbx->IndexCount << "," << binfbx->IndexSize << ","<< JointCount <<");";

        rc = sqlite3_exec(
        db,
        insert.str().c_str(),
        nullptr,
        nullptr,
        &errmsg);

        if(errmsg != nullptr)
        {
            std::cerr << "Insert Failed: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }

        sqlite3_close(db);
        return 0;
#endif
#endif
        for(uint32_t i = 0; i < JointCount; ++i)
        {
            index = PrintArray<char>(index,"Joint Index " + std::to_string(i) + " Name");
            index = PrintArrayCount<float>(index,"Matrix 1",3);
            index = PrintArrayCount<float>(index,"Matrix 2",3);
            index = PrintArrayCount<float>(index,"Matrix 3",3);
            index = PrintArrayCount<float>(index,"Matrix 4",3);
            index = PrintArrayCount<float>(index,"Envelope Vector",3);
            index = PrintSingle<float>(index,"Envelope Radius");
            index = PrintSingle<int32_t>(index,"Parent Index");
        }

        index = PrintArrayCount<int32_t>(index,"Unknown Ints", 2);
        index = PrintSingle<float>(index, "Unknown Float");
        index = PrintArray<float>(index,"Unknown Depth Buffer");
        index = PrintSingle<float>(index, "Unknown Float");
        index = PrintArrayCount<float>(index,"Unknown Vec3", 3);
        index = PrintSingle<float>(index, "Unknown Float");
        index = PrintArrayCount<float>(index,"Unknown Vec3", 3);
        index = PrintArrayCount<float>(index,"Unknown Vec3", 3);

        index = PrintSingle<uint32_t>(index, "LOD Count");

        uint32_t material_count;
        index = PrintSingle<uint32_t>(index, "Material Count", &material_count);

        //---- Materials
        for(uint32_t i = 0; i < material_count;++i )
        {
            index = PrintSingle<uint32_t>(index,"Material File Magick Always (7)");
            index = PrintArrayCount<uint8_t>(index,"Material Global ID",8);
            std::cout << std::dec;

            index = PrintArray<char>(index,"Material Name");
            index = PrintArray<char>(index,"Material Type");
            index = PrintArray<char>(index,"Material Path");

            index = PrintArrayCount<uint32_t>(index,"Unknown Array of 6", 6);

            //bPrint = false;
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
            bPrint = true;
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

        index = PrintArray<uint32_t>(index, "Unknown Int Array");

        uint32_t submesh_count1;
        index = PrintSingle<uint32_t>(index,"SubMesh Count 1",&submesh_count1);
        std::cout<< "---------------------------------------------------------------------------" << std::endl;
        for(uint32_t i = 0; i<submesh_count1;++i)
        {
            index = PrintMesh(index,std::string("SM 1 Mesh ")+std::to_string(i)+std::string(" LOD"));
            std::cout<< "---------------------------------------------------------------------------" << std::endl;
        }

        std::cout << std::endl;

        uint32_t submesh_count2;
        index = PrintSingle<uint32_t>(index,"SubMesh Count 2",&submesh_count2);
        std::cout<< "---------------------------------------------------------------------------" << std::endl;
        for(uint32_t i = 0; i< submesh_count2;++i)
        {
            index = PrintMesh(index,std::string("SM 2 Mesh ")+std::to_string(i)+std::string(" LOD"));
            std::cout<< "---------------------------------------------------------------------------" << std::endl;
        }

        index = PrintArray<uint32_t>(index, "Unknown Array, usually zero");
        index = PrintSingle<float>(index,"Unknown Float");
        index = PrintArray<float>(index, "Unknown Array of floats");
        return 0;
#endif
    }
}
