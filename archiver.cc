#include "tarstream.hh"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: archiver input_directory [output_name]\n";
        return 1;
    }

    TAR::Archiver archiver;
    if (argc == 3)
    {
        std::string dest(argv[2]);
        auto        tar_extension = dest.find(".tar");
        if (tar_extension == std::string::npos)
            dest += ".tar";

        if (archiver.archive(argv[1], dest) != TAR::Status::OK)
        {
            std::cerr << "Error: could not archive!\n";
            return 1;
        }
    }
    else
    {
        std::string dest(argv[1]);
        dest += ".tar";
        if (archiver.archive(argv[1], dest) != TAR::Status::OK)
        {
            std::cerr << "Error: could not archive!\n";
            return 1;
        }
    }

    return 0;
}
