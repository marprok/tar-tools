#include "tarstream.hh"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: parser input.tar\n";
        return 1;
    }

    TAR::InStream in(argv[1]);
    TAR::Parser parser(in);
    std::list<TAR::File> files;
    parser.list_files(files);

    for (const auto& file : files)
        std::cout << file.name << '\n';
    return 0;
}
