#ifndef TARSTREAM_HH
#define TARSTREAM_HH

#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <tuple>

namespace fs = std::filesystem;

constexpr std::uint32_t BLOCK_SIZE = 512;

/*
 * The header block(POSIX 1003.1-1990)
 */
struct TARHeader
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
};

struct TARBlock
{
    union
    {
        std::uint8_t m_data[BLOCK_SIZE];
        TARHeader m_header;
    };

    friend std::ostream& operator<<(std::ostream& os, const TARBlock& block);
};

typedef std::vector<TARBlock> TARFile;

enum class Status { TAR_OK, TAR_EOF };

class TARStream
{
public:
    TARStream(fs::path file_path, std::uint32_t blocking_factor = 20 );
    ~TARStream();

    TARStream(const TARStream& other) = delete;
    TARStream& operator=(const TARStream& other) = delete;

    Status read_block(TARBlock& raw);
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

class TARParser
{
public:
    TARParser(TARStream &tar_stream);

    TARFile get_next_file();
private:
    TARStream &m_tar_stream;
};

#endif // TARSTREAM_HH
