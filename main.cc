#include <iostream>
#include "tarstream.hh"

void print_heaader(const TARHeader& header)
{
    std::cout << "name: " << header.name << '\n';
    std::cout << "mode: " << header.mode << '\n';
    std::cout << "uid: " << header.uid << '\n';
    std::cout << "gid: " << header.gid << '\n';
    std::cout << "size: " << header.size << '\n';
    std::cout << "mtime: " << header.mtime << '\n';
    std::cout << "checksum: " << header.chksum << '\n';
    std::cout << "typeflag: " << header.typeflag << '\n';
    std::cout << "linkname: " << header.linkname << '\n';
    std::cout << "magic: " << header.magic << '\n';
    std::cout << "version: " << header.version << '\n';
    std::cout << "uname: " << header.uname << '\n';
    std::cout << "gname: " << header.gname << '\n';
    std::cout << "devmajor: " << header.devmajor << '\n';
    std::cout << "devminor: " << header.devminor << '\n';
    std::cout << "prefix: " << header.prefix << '\n';
}

int main()
{
    TARStream tar("linux-5.12-rc4.tar");
    TARParser parser(tar);

    for (auto i : {1,2,3,4})
    {
        auto file = parser.get_next_file();
        std::cout << "file " << file.header.name << " was found!\n"
                  << "File size : " << file.data.size() << '\n';
        print_heaader(file.header);
        if (file.header.typeflag == 'x' ||
            file.header.typeflag == 'g')
        {
            std::cout << "EXTENDED TAR FORMAT\n";
            auto extended = parser.parse_extended(file);
            for (auto pair : extended)
                std::cout << pair.first << ":" << pair.second << '\n';
        }
        std::cout << "\n\n\n\n\n\n";
    }
}
