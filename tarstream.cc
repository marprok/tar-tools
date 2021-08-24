#include <cstring>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include "tarstream.hh"

namespace TAR
{
    static_assert(sizeof(Block) == BLOCK_SIZE);

    std::ostream& operator<<(std::ostream& os, const Header& header)
    {
        os << "name: " << header.name << '\n';
        os << "mode: " << header.mode << '\n';
        os << "uid: " << header.uid << '\n';
        os << "gid: " << header.gid << '\n';
        os << "size: " << header.size << '\n';
        os << "mtime: " << header.mtime << '\n';
        os << "checksum: " << header.chksum << '\n';
        os << "typeflag: " << header.typeflag << '\n';
        os << "linkname: " << header.linkname << '\n';
        os << "magic: " << header.magic << '\n';
        os << "version: " << header.version[0] << header.version[1] << '\n';
        os << "uname: " << header.uname << '\n';
        os << "gname: " << header.gname << '\n';
        os << "devmajor: " << header.devmajor << '\n';
        os << "devminor: " << header.devminor << '\n';
        os << "prefix: " << header.prefix << '\n';

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Block& block)
    {
        for (std::uint32_t i = 0; i < BLOCK_SIZE; ++i)
        {
            if (block.as_data[i])
                os << static_cast<char>(block.as_data[i]);
            else
                os << ".";
        }
        return os;
    }

    std::uint32_t Block::calculate_checksum() const
    {
        std::uint32_t sum = 0;
        const std::uint8_t* chksum = reinterpret_cast<const uint8_t*>(as_header.chksum);
        for (std::uint16_t i = 0; i < BLOCK_SIZE; ++i)
        {
            if (&as_data[i] >= chksum &&
                &as_data[i] < (chksum + sizeof(as_header.chksum)))
                sum+=' ';
            else
                sum += as_data[i];
        }
        return sum;
    }

    bool Block::is_zero_block() const
    {
        for (std::uint16_t i = 0; i < BLOCK_SIZE; ++i)
            if (as_data[i])
                return false;

        return true;
    }

    std::uint32_t Header::size_in_blocks() const
    {
        std::uint32_t bytes = std::stoi(size, nullptr, 8);
        if (!bytes)
            return 0;

        std::uint32_t blocks = bytes/BLOCK_SIZE;
        if (bytes % BLOCK_SIZE)
            blocks++;

        return blocks;
    }

    std::uint32_t Header::size_in_bytes() const
    {
        return std::stoi(size, nullptr, 8);
    }

    BlockStream::BlockStream(std::uint32_t blocking_factor)
        :m_blocking_factor(blocking_factor),
         m_block_id(0),
         m_record_id(0)
    {
    }

    std::uint32_t BlockStream::record_id() { return m_record_id; }
    std::uint32_t BlockStream::block_id() { return m_block_id; }
    void BlockStream::set_file_path(const fs::path& file_path) { m_file_path = file_path; }

    InStream::InStream(fs::path file_path, std::uint32_t blocking_factor)
        :BlockStream(blocking_factor),
         m_should_read(true)
    {
        m_file_path = fs::absolute(file_path);
        m_records_in_file = fs::file_size(m_file_path)/(m_blocking_factor*BLOCK_SIZE);
        m_stream.open(m_file_path, std::ios::in | std::ios::binary);
        std::cout << m_records_in_file << " record in file\n";
        if (!m_stream)
            throw std::runtime_error("Could not open file"); // let the caller handle the case
    }

    Status InStream::read_block(Block& raw, bool advance)
    {
        if (m_should_read)
        {
            if (m_record_id < m_records_in_file)
            {
                Status st = _read_record();
                if (st != Status::OK)
                    return st;
            }else
                return Status::END;
        }

        auto from = m_record.get() + m_block_id*BLOCK_SIZE;
        auto to = from + BLOCK_SIZE;
        std::copy(from, to, raw.as_data);

        if (advance)
            m_block_id++;

        if (m_block_id >= m_blocking_factor)
        {
            m_block_id = 0;
            m_record_id = m_stream.tellg() / (BLOCK_SIZE*m_blocking_factor);
            std::memset(m_record.get(), 0 , BLOCK_SIZE*m_blocking_factor);
            m_should_read = true;
        }

        return Status::OK;
    }

    Status InStream::_read_record()
    {
        if (!m_record)
            m_record = std::make_unique<std::uint8_t[]>(BLOCK_SIZE*m_blocking_factor);

        m_stream.read(reinterpret_cast<char*>(m_record.get()),
                      BLOCK_SIZE*m_blocking_factor);

        Status st = Status::OK;
        if (!m_stream)
            st = Status::ERROR;
        else if (m_stream.gcount() != BLOCK_SIZE*m_blocking_factor)
            st = Status::END;
        else
            m_should_read = false;
        return st;
    }

    Status InStream::seek_record(std::uint32_t record_id)
    {
        m_stream.seekg(record_id*BLOCK_SIZE*m_blocking_factor);
        if (!m_stream)
            return Status::ERROR;

        m_record_id = record_id;
        if (_read_record() != Status::OK)
            return Status::ERROR;

        m_block_id = 0;
        return Status::OK;
    }

    Status InStream::skip_blocks(std::uint32_t count)
    {
        if (m_block_id + count < m_blocking_factor)
            m_block_id += count;
        else
        {
            // go to the next record(a.k.a align the records)
            count -= m_blocking_factor - m_block_id;
            m_record_id += count / m_blocking_factor + 1;

            if (seek_record(m_record_id) != Status::OK)
                return Status::ERROR;

            m_block_id = count % m_blocking_factor;
        }

        return Status::OK;
    }

    Parser::Parser(InStream &tar_stream)
        :m_stream(tar_stream)
    {
    }

    Status Parser::next_file(File& file)
    {
        Block header_block;
        Status st = m_stream.read_block(header_block);
        if (st != Status::OK)
            return st;

        if (header_block.is_zero_block())
        {
            Block block;
            m_stream.read_block(block, false);
            if (block.is_zero_block())
                return Status::END;
            else
                return Status::ERROR;
        }

        if (!_check_block(header_block))
            return Status::ERROR;

        file.header = header_block.as_header; // deep copy
        file.m_block_id = m_stream.block_id();
        file.m_record_id = m_stream.record_id();

        return Status::OK;
    }

    Data Parser::read_file(File& file)
    {
        m_stream.seek_record(file.m_record_id);
        m_stream.skip_blocks(file.m_block_id);

        return _unpack(file.header);
    }

    Status Parser::list_files(TARList& list)
    {
        m_stream.seek_record(0);
        list.clear();

        while (true)
        {
            File file;
            Status st = next_file(file);
            if (st == Status::END)
                return Status::OK;
            else if (st != Status::OK)
                return st;

            std::uint32_t data_blocks = file.header.size_in_blocks();
            m_stream.skip_blocks(data_blocks);
            list.push_back(file);
        }

        return Status::OK;
    }

    bool Parser::_check_block(Block &block)
    {
        std::uint32_t sum = block.calculate_checksum();
        std::uint32_t header_sum = std::stoi(block.as_header.chksum, nullptr, 8);
        if (sum != header_sum)
        {
            std::cerr << "not matching checksums!\n";
            return false;
        }

        block.as_header.name[sizeof(block.as_header.name)-1] = '\0';
        block.as_header.mode[sizeof(block.as_header.mode)-1] = '\0';
        block.as_header.uid[sizeof(block.as_header.uid)-1] = '\0';
        block.as_header.gid[sizeof(block.as_header.gid)-1] = '\0';
        block.as_header.size[sizeof(block.as_header.size)-1] = '\0';
        block.as_header.mtime[sizeof(block.as_header.mtime)-1] = '\0';
        //block.as_header.chksum[sizeof(block.as_header.chksum)-1] = '\0';
        block.as_header.linkname[sizeof(block.as_header.linkname)-1] = '\0';
        block.as_header.magic[sizeof(block.as_header.magic)-1] = '\0';
        //block.as_header.version[sizeof(block.as_header.version)-1] = '\0';
        block.as_header.uname[sizeof(block.as_header.uname)-1] = '\0';
        block.as_header.gname[sizeof(block.as_header.gname)-1] = '\0';
        block.as_header.devmajor[sizeof(block.as_header.devmajor)-1] = '\0';
        block.as_header.devminor[sizeof(block.as_header.devminor)-1] = '\0';
        block.as_header.prefix[sizeof(block.as_header.prefix)-1] = '\0';

        return true;
    }

    Data Parser::_unpack(const Header& header)
    {
        std::uint32_t data_blocks = header.size_in_blocks();
        std::uint32_t size = header.size_in_bytes();
        Data bytes;
        bytes.reserve(size);

        for (std::uint32_t i = 0; i < data_blocks; ++i)
        {
            Block block;
            if (m_stream.read_block(block) != Status::OK)
                return {}; // should not happen since the blocks must exist

            if (size)
            {
                std::uint32_t bytes_to_copy = BLOCK_SIZE;
                if (size < BLOCK_SIZE)
                    bytes_to_copy = size;

                bytes.insert(bytes.end(), block.as_data, block.as_data + bytes_to_copy);
                size -= bytes_to_copy;
            }
        }

        return bytes;
    }

    OutStream::OutStream(std::uint32_t blocking_factor)
        :BlockStream(blocking_factor)
    {
    }

    Status OutStream::open_output_file(const fs::path& file_path)
    {
        set_file_path(file_path);
        m_stream.open(m_file_path, std::ios::out | std::ios::binary);
        if (!m_stream)
            return Status::ERROR;
        return Status::OK;
    }

    void OutStream::close_output_file()
    {
        set_file_path({});
        m_stream.close();
    }

    Status OutStream::write_block(const Block& block)
    {
        if (!m_record)
        {
            m_record = std::make_unique<std::uint8_t[]>(BLOCK_SIZE*m_blocking_factor);
            std::memset(m_record.get(), 0, BLOCK_SIZE*m_blocking_factor);
        }

        if (m_block_id >= m_blocking_factor)
        {
            if (_flush_record() != Status::OK)
                return Status::ERROR;
            m_block_id = 0;
            m_record_id++;
        }

        std::copy(block.as_data, block.as_data + BLOCK_SIZE,
                  m_record.get() + m_block_id*BLOCK_SIZE);
        m_block_id++;
        return Status::OK;
    }

    Status OutStream::write_blocks(const std::vector<Block>& blocks)
    {
        for (const auto& block : blocks)
        {
            if (write_block(block) != Status::OK)
                return Status::ERROR;
        }

        if (m_block_id != 0)
            _flush_record();

        return Status::OK;
    }

    Status OutStream::_flush_record()
    {
        if (!m_record)
            return Status::ERROR;

        m_stream.write(reinterpret_cast<char*>(m_record.get()),
                      BLOCK_SIZE*m_blocking_factor);

        if (!m_stream)
            return Status::ERROR;
        std::memset(m_record.get(), 0, BLOCK_SIZE*m_blocking_factor);
        return Status::OK;
    }

    Archiver::Archiver(std::uint32_t blocking_factor)
        :m_stream(blocking_factor)
    {

    }

    Status Archiver::archive(const fs::path& thing, const fs::path& dest)
    {
        if (!fs::exists(thing))
        {
            std::cerr << "Entry: " << thing << " does not exist!\n";
            return Status::ERROR;
        }

        if (m_stream.open_output_file(dest) != Status::OK)
            throw std::runtime_error("Could not open file");

        std::vector<Block> blocks;
        for(auto const& temp: std::filesystem::directory_iterator{thing})
        {
            Block header_block;
            _create_header(temp.path(), header_block);
            blocks.push_back(header_block);
            std::cout << header_block.as_header;
            std::cout << "\n\n";
        }
        m_stream.write_blocks(blocks);
        m_stream.close_output_file();
        return Status::OK;
    }

    Status Archiver::_create_header(const fs::path& path, Block& header_block)
    {
        struct stat info;
        Header& header = header_block.as_header;
        if (lstat(path.string().c_str(), &info) < 0)
        {
            std::cerr << "stat for " << path << " failed\n";
            return Status::ERROR;
        }
        std::memset(&header, 0, sizeof(Block));
        std::strncpy(header.name, path.string().c_str(), sizeof(header.name) - 1);
        std::sprintf(header.mode, "%0*o",
                     (int)sizeof(header.mode) - 1,
                     info.st_mode & ~S_IFMT);
        std::sprintf(header.uid, "%0*o",
                     (int)sizeof(header.uid) - 1,
                     info.st_uid);
        std::sprintf(header.gid, "%0*o",
                     (int)sizeof(header.gid) - 1,
                     info.st_gid);
        if (!fs::is_directory(path))
            std::sprintf(header.size, "%0*lo",
                         (int)sizeof(header.size) - 1,
                         info.st_size);
        else
            std::memset(header.size, '0', sizeof(header.size) - 1);

        switch (info.st_mode & S_IFMT)
        {
            case S_IFREG:  header.typeflag = '0';   break;
            case S_IFLNK:  header.typeflag = '1';   break;
            case S_IFCHR:  header.typeflag = '3';  break;
            case S_IFBLK:  header.typeflag = '4';  break;
            case S_IFDIR:  header.typeflag = '5';  break;
            case S_IFIFO:  header.typeflag = '6';  break;
            case S_IFSOCK: header.typeflag = 0;     break;// what is the correct value?
        }

        std::sprintf(header.mtime, "%lo", info.st_mtime);
        std::sprintf(header.magic, "ustar");
        //std::sprintf(header.version, "  ");
        header.version[0] = 0x30;
        header.version[1] = 0x30;

        struct passwd *pw = getpwuid(info.st_uid);
        if (pw)
            std::strncpy(header.uname, pw->pw_name, sizeof(header.uname) - 1);

        struct group  *gr = getgrgid(info.st_gid);
        if (gr)
            std::strncpy(header.gname, gr->gr_name, sizeof(header.gname) - 1);

        std::memset(header.devmajor, '0', sizeof(header.devmajor) - 1);
        std::memset(header.devminor, '0', sizeof(header.devminor) - 1);
        std::sprintf(header.chksum, "%0*o",
                     (int)sizeof(header.chksum) - 2,
                     header_block.calculate_checksum());
        header.chksum[sizeof(header.chksum)-1] = 0x20;
        return Status::OK;
    }
}
