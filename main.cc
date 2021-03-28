#include <iostream>
#include "tarstream.hh"

int main()
{
        TARStream tar("linux-5.12-rc4.tar");
        RawBlock block;
        std::size_t blocks = 0;
        while (tar.read_block(block) == TARStream::Status::TAR_OK)
        {
            blocks++;
            //std::cout << block;
        }
        std::cout << blocks << " in total\n";

}
