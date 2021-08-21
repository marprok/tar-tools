#ifndef TARSTREAM_HH
#define TARSTREAM_HH

#include <fstream>
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>
#include <type_traits>

namespace TAR
{
    enum class Status { OK, END, ERROR };

    namespace fs = std::filesystem;

    typedef std::vector<std::uint8_t> Data;

    constexpr std::uint32_t BLOCK_SIZE = 512;

  /*
   * The header block(POSIX 1003.1-1990)
   */
    struct Header
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
        char prefix[15];

        friend std::ostream& operator<<(std::ostream& os, const Header& header);

        std::uint32_t size_in_blocks() const;
        std::uint32_t size_in_bytes() const;
    };

    struct Block
    {
        union
        {
            std::uint8_t as_data[BLOCK_SIZE];
            Header as_header;
        };

        std::uint32_t calculate_checksum() const;
        bool is_zero_block() const;
        friend std::ostream& operator<<(std::ostream& os, const Block& block);
    };

    struct File
    {
        Header header;
    private:
        std::uint32_t m_block_id;
        std::uint32_t m_record_id;

        friend class Parser;
    };

    typedef std::vector<File> TARList;

    class BlockStream
    {
    public:
        BlockStream(std::uint32_t blocking_factor = 20 );
        virtual ~BlockStream() = default;

        BlockStream(const BlockStream& other) = delete;
        BlockStream& operator=(const BlockStream& other) = delete;

        std::uint32_t record_id();
        std::uint32_t block_id();
        void set_file_path(const fs::path& file_path);

    protected:
        fs::path m_file_path;
        std::uint32_t m_blocking_factor;
        std::uint32_t m_block_id;
        std::uint32_t m_record_id;
        std::unique_ptr<std::uint8_t[]> m_record;
        std::fstream m_stream;
    };

    class IStream : public BlockStream
    {
    public:
        IStream(fs::path file_path, std::uint32_t blocking_factor = 20 );
        ~IStream() = default;

        IStream(const IStream& other) = delete;
        IStream& operator=(const IStream& other) = delete;

        Status read_block(Block& raw, bool advance = true);
        Status seek_record(std::uint32_t record_id);
        Status skip_blocks(std::uint32_t count);

    private:
        std::uint32_t m_records_in_file;
        bool m_should_read;

        Status _read_record();
    };

    class Parser
    {
    public:
        Parser(IStream &tar_stream);
        ~Parser() = default;

        Parser(const Parser& other) = delete;
        Parser& operator=(const Parser& other) = delete;

        Status next_file(File& file);
        Data read_file(File& file);
        Status list_files(TARList& list);
    private:
        bool _check_block(Block& block);
        Data _unpack(const Header& header);

        IStream &m_stream;
    };

    class OutStream : public BlockStream
    {
    public:
        OutStream( std::uint32_t blocking_factor = 20 );
        ~OutStream() = default;

        OutStream(const OutStream& other) = delete;
        OutStream& operator=(const OutStream& other) = delete;

        Status open_output_file(const fs::path& file_path);
        void close_output_file();
        Status write_block(const Block& block);
        Status write_blocks(const std::vector<Block>& blocks);
    private:
        Status _flush_record();
    };

    class Archiver
    {
    public:
        Archiver(std::uint32_t blocking_factor = 20);
        ~Archiver() = default;

        Archiver(const Archiver& other) = delete;
        Archiver& operator=(const Archiver& other) = delete;

        Status archive(const fs::path& thing, const fs::path& dest);
    private:

        Status _create_header(const fs::path& path, Block& header_block);
        OutStream m_stream;
    };
}
#endif // TARSTREAM_HH
