#include <iostream>
#include <cstring>
#include "tarstream.hh"

int main()
{
    //#define TEST_OUT
    #ifndef TEST_OUT
    //TAR::IStream tar("linux-5.12-rc4.tar");
    TAR::IStream tar("dir1.tar");
    TAR::Parser parser(tar);
/*
    for (int i = 0; i < 40; ++i)
    {
        auto file = parser.get_next_file();
        std::cout << "file " << file.header.name << " was found!\n";
        auto data = parser.read_file(file);
        std::cout << "size: " << data.size();
        std::cout << file.header << '\n';
        if (file.header.typeflag == 'x' ||
            file.header.typeflag == 'g')
        {
            std::cout << "EXTENDED TAR FORMAT\n";
        }
        std::cout << "\n\n\n\n\n\n";
    }
*/
    TAR::TARList files;
    auto st = parser.list_files(files);
    if (st != TAR::Status::OK)
    {
        std::cerr << "Error listing files\n";
        return 1;
    }

    std::cout << files.size() << " files found\n";
    for (auto& file : files)
    {
        std::cout << file.header << '\n';
        if (!strcmp("linux-5.12-rc4/virt/kvm/coalesced_mmio.h", file.header.name))
        {
            auto data = parser.read_file(file);
            for (char c : data)
                std::cout << c;
            std::cout << '\n';
            break;
        }
    }
    #else
    TAR::Archiver arc;
    arc.archive("dir1", "test");
    #endif
}
