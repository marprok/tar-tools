#include <iostream>
#include "tarstream.hh"

int main()
{
        TARStream tar("linux-5.12-rc4.tar");
        TARParser parser(tar);

        auto file = parser.get_next_file();
        if (file.size())
        {
            std::cout << "file " << file[0].m_header.name << " was found!\n"
                      << "File size in blocks including the header: " << file.size() << '\n';
        }
/*
        std::cout << "name: " << block.m_header.name << '\n';
        std::cout << "mode: " << block.m_header.mode << '\n';
        std::cout << "uid: " << block.m_header.uid << '\n';
        std::cout << "gid: " << block.m_header.gid << '\n';
        std::cout << "size: " << block.m_header.size << '\n';
        std::cout << "mtime: " << block.m_header.mtime << '\n';
        std::cout << "checksum: " << block.m_header.chksum << '\n';
        std::cout << "typeflag: " << block.m_header.typeflag << '\n';
        std::cout << "linkname: " << block.m_header.linkname << '\n';
        std::cout << "magic: " << block.m_header.magic << '\n';
        std::cout << "version: " << block.m_header.version << '\n';
        std::cout << "uname: " << block.m_header.uname << '\n';
        std::cout << "gname: " << block.m_header.gname << '\n';
        std::cout << "devmajor: " << block.m_header.devmajor << '\n';
        std::cout << "devminor: " << block.m_header.devminor << '\n';
        std::cout << "prefix: " << block.m_header.prefix << '\n';

        std::cout << "----RAW----\n\n";
*/

}
