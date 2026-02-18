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
#include <stdexcept>
#include <cstdint>
#include "SkeletonTool.h"

namespace ControlModding
{
    SkeletonTool::SkeletonTool() = default;
    SkeletonTool::~SkeletonTool() = default;
    void SkeletonTool::ProcessArgs ( int argc, char** argv )
    {
        if ( argc < 2 || ( strcmp ( argv[1], "binskeleton" ) != 0 ) )
        {
            std::ostringstream stream;
            stream << "Invalid tool name, expected \"binskeleton\", got " << ( ( argc < 2 ) ? "nothing" : argv[1] ) << std::endl;
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
                    else if ( strncmp ( &argv[i][2], "dump", sizeof ( "dump" ) ) == 0 )
                    {
                        mDump = true;
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
                    default:
                        {
                            std::ostringstream stream;
                            stream << "Unknown Option " << argv[i] << std::endl;
                            throw std::runtime_error(stream.str().c_str());
                        }
                    }
                }
            }
            else if(mInputFile.empty())
            {
                mInputFile = argv[i];
            }
        }
        if ( mInputFile.empty() )
        {
            throw std::runtime_error ( "No Input file provided." );
        }
    }

    // This function is a hash function that takes a string and a byte as input.
    // It initializes a hash value and iterates through each character of the string,
    // updating the hash value based on the character and the byte.
    // The final hash value is returned as a 32-bit unsigned integer.
    
    // The function uses a specific hashing algorithm (FNV-1a) to compute the hash.
    // The algorithm involves XORing the current character with the hash value,
    // multiplying by a prime number, and then shifting the byte left by 5 bits.
    uint32_t FNV1a(const uint8_t *input_buffer, uint8_t salt = 1)
    {
      uint8_t byte{*input_buffer};
      uint32_t hexadecimal{0x811c9dc5};      
      while (byte != '\0')
      {
        input_buffer = input_buffer + 1;
        hexadecimal = ((static_cast<uint32_t>(byte) | static_cast<uint32_t>(static_cast<uint8_t>(salt << 5))) ^ hexadecimal) * 0x1000193;
        byte = *input_buffer;
      }
      return hexadecimal;
    }

    int SkeletonTool::operator() ( int argc, char** argv )
    {
        ProcessArgs ( argc, argv );
        std::ifstream file;
        file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
        try
        {
            file.open ( mInputFile, std::ifstream::in | std::ifstream::binary );
        } 
        catch ( std::ios_base::failure& e )
        {
            throw std::runtime_error ( "Failed to open input file \"" + mInputFile + "\": " + e.what() );
        }

        std::vector<uint8_t> buffer ( ( std::istreambuf_iterator<char> ( file ) ), ( std::istreambuf_iterator<char>() ) );
        file.close();

        if(*reinterpret_cast<uint64_t*>(buffer.data()) != 0x2)
        {
            throw std::runtime_error ( "Invalid input file \"" + mInputFile + "\": Invalid magic number." );
        }

        // Skip header
        uint8_t *pointer{buffer.data() + 0x10};
        SubSectionIndex *bone_data{reinterpret_cast<SubSectionIndex *>(pointer)};
        // Align to next 16-byte boundary from file start to find the bone names section
        SubSectionIndex *bone_names{reinterpret_cast<SubSectionIndex *>(pointer + ((bone_data->mStart + bone_data->mSize + 0xFu) & ~0xFu))};
        std::cout << "Subsection start: " << std::hex << bone_data->mStart << std::endl;
        std::cout << "Subsection size: " << std::hex << bone_data->mSize << std::endl;
        std::cout << "Subsection count: " << std::hex << bone_data->mCount << std::endl;
        std::cout << "Subsection offsets: ";
        for (size_t i = 0; i < bone_data->mCount; ++i)
        {
            std::cout << std::hex << bone_data->mOffsets[i] << " ";
        }
        std::cout << std::endl;
        pointer += bone_data->mStart;
        uint32_t bone_count{*reinterpret_cast<uint32_t*>(pointer)};
        std::cout << "Bone Count: " << std::hex << bone_count << std::endl;

        Transform* transforms{nullptr};
        uint32_t* parent_indices{nullptr};
        uint32_t* bone_ids{nullptr};
        
        std::cout << "Subsection 64bit offsets: ";
        for (size_t i = 0; i < bone_data->mCount; ++i)
        {
            std::cout << std::hex << *reinterpret_cast<uint64_t*>(pointer + bone_data->mOffsets[i]) << " ";
            switch(i)
            {
                case 0:
                    transforms = reinterpret_cast<Transform*> (pointer + *reinterpret_cast<uint64_t*>(pointer + bone_data->mOffsets[i]));
                    break;
                case 1:
                    parent_indices = reinterpret_cast<uint32_t*>(pointer + *reinterpret_cast<uint64_t*>(pointer + bone_data->mOffsets[i]));
                    break;
                case 2:
                    bone_ids = reinterpret_cast<uint32_t*>(pointer + *reinterpret_cast<uint64_t*>(pointer + bone_data->mOffsets[i]));
                    break;
            }
        }
        std::cout << std::endl;

        for(size_t i = 0; i < bone_count; ++i)
        {
            std::cout << "Bone " << std::dec << i << ": ";
            std::cout << "Transform: ";
            std::cout << "Rotation: ";
            std::cout << "X: " << transforms[i].mRotation[0] << " ";
            std::cout << "Y: " << transforms[i].mRotation[1] << " ";
            std::cout << "Z: " << transforms[i].mRotation[2] << " ";
            std::cout << "W: " << transforms[i].mRotation[3] << " ";
            std::cout << "Position: ";
            std::cout << "X: " << transforms[i].mPosition[0] << " ";
            std::cout << "Y: " << transforms[i].mPosition[1] << " ";
            std::cout << "Z: " << transforms[i].mPosition[2] << " ";
            std::cout << "W: " << transforms[i].mPosition[3] << " ";
            std::cout << "Parent Index: " << std::hex << parent_indices[i] << " ";
            std::cout << "Bone ID: " << std::hex << bone_ids[i] << std::endl;
        }

        // Process bone names
        pointer = reinterpret_cast<uint8_t*>(bone_names) + bone_names->mStart;
        std::cout << "Subsection start: " << std::hex << bone_names->mStart << std::endl;
        std::cout << "Subsection size: " << std::hex << bone_names->mSize << std::endl;
        std::cout << "Subsection count: " << std::hex << bone_names->mCount << std::endl;
        std::cout << "Subsection offsets: ";
        for (size_t i = 0; i < bone_names->mCount; ++i)
        {
            std::cout << std::hex << bone_names->mOffsets[i] << " ";
        }
        std::cout << std::endl;

        /* There is two ways to get to the names, either use the 32bit offset array to get to the 64bit value
           or directly use the 64bit start offset + bone count, we'll do both as a check.*/
        NameArray* name_array{nullptr};
        for (size_t i = 0; i < bone_names->mCount; ++i)
        {
            switch(i)
            {
                case 0:
                    name_array = reinterpret_cast<NameArray*> (pointer + bone_names->mOffsets[i]);
                    std::cout << "Offset: " << std::hex << name_array->mOffset << " Count: " << name_array->mCount << " Offsets: ";
                    break;
                default:
                    std::cout << std::hex << *reinterpret_cast<uint64_t*>(pointer + bone_names->mOffsets[i]) << " ";
                    std::cout << reinterpret_cast<char*>(pointer + *reinterpret_cast<uint64_t*>(pointer + bone_names->mOffsets[i])) << " ";
                break;
            }
        }
        std::cout << std::endl;
        pointer = reinterpret_cast<uint8_t*>(name_array) + name_array->mOffset;
        std::cout << "Bone Names: ";
        for (size_t i = 0; i < name_array->mCount; ++i)
        {
            std::cout <<  reinterpret_cast<char*>(name_array) + *( reinterpret_cast<uint64_t*>(pointer) + i ) << " ";
        }
        std::cout << std::endl;
        return 0;
    }
}
