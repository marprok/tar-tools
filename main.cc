#include <iostream>
#include "tarstream.hh"

int main()
{
    TARStream tar("linux-5.12-rc4.tar");
    TARParser parser(tar);

    for (auto i : {1,2,3,4})
    {
        auto file = parser.get_next_file();
        std::cout << "file " << file.header.name << " was found!\n"
                  << "File size : " << file.data.size() << '\n';
        std::cout << file.header << '\n';
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
