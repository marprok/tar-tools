#ifndef TARSTREAM_HH
#define TARSTREAM_HH

#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

constexpr std::uint32_t BLOCK_SIZE = 512;

struct RawBlock
{
    std::uint8_t m_data[BLOCK_SIZE];

    friend std::ostream& operator<<(std::ostream& os, const RawBlock& block);
};

class TARStream
{
public:
    TARStream(fs::path file_path, std::uint32_t blocking_factor = 20 );
    ~TARStream();

    enum class Status { TAR_OK, TAR_EOF };

    Status read_block(RawBlock& raw);
private:
    fs::path m_file_path;
    std::uint32_t m_blocking_factor;
    std::uint32_t m_block_id;
    std::uint32_t m_record_id;
    std::uint8_t* m_record;
    std::fstream m_stream;
    std::uint32_t m_records_in_file;

    // Private API
    Status _fetch_record();
};

#endif // TARSTREAM_HH
