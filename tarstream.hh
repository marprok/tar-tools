#ifndef TARSTREAM_HH
#define TARSTREAM_HH

#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <unordered_map>

enum class Status { TAR_OK, TAR_EOF, TAR_ERROR };

namespace fs = std::filesystem;
typedef std::unordered_map<std::string, std::string> TARExtended;
typedef std::vector<std::uint8_t> TARData;

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

    friend std::ostream& operator<<(std::ostream& os, const TARHeader& header);

    std::uint32_t size_in_blocks() const;
    std::uint32_t size_in_bytes() const;
};

struct TARBlock
{
    union
    {
        std::uint8_t m_data[BLOCK_SIZE];
        TARHeader m_header;
    };

    std::uint32_t calculate_checksum() const;
    bool is_zero_block() const;
    friend std::ostream& operator<<(std::ostream& os, const TARBlock& block);
};

struct TARFile
{
    TARHeader header;
private:
    std::uint32_t m_block_id;
    std::uint32_t m_record_id;

    friend class TARParser;
};

typedef std::vector<TARFile> TARList;

class TARStream
{
public:
    TARStream(fs::path file_path, std::uint32_t blocking_factor = 20 );
    ~TARStream() = default;

    TARStream(const TARStream& other) = delete;
    TARStream& operator=(const TARStream& other) = delete;

    Status read_block(TARBlock& raw);
    Status seek_record(std::uint32_t record_id);
    Status skip_blocks(std::uint32_t count);

    std::uint32_t record_id();
    std::uint32_t block_id();
private:
    fs::path m_file_path;
    std::uint32_t m_blocking_factor;
    std::uint32_t m_block_id;
    std::uint32_t m_record_id;
    std::vector<std::uint8_t> m_record;
    std::fstream m_stream;
    std::uint32_t m_records_in_file;

    // Private API
    Status _read_record();
};

class TARParser
{
public:
    TARParser(TARStream &tar_stream);
    Status next_file(TARFile& file);
    TARData read_file(TARFile& file);
    TARExtended parse_extended(const TARFile &file);
    Status list_files(TARList& list);
private:
    bool _check_block(TARBlock& block);
    TARData _unpack(const TARHeader& header);

    TARStream &m_tar;
};

#endif // TARSTREAM_HH
