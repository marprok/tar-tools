#include <iostream>
#include "tarstream.hh"

int main()
{
    TARStream tar("linux-5.12-rc4.tar");
    TARParser parser(tar);
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
    auto files = parser.list_files();
    std::cout << files.size() << " files found\n";
    for (auto& file : files)
        std::cout << file.header.name << '\n';
}
