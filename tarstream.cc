#include <cstring>
#include <string>
#include <queue>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include "tarstream.hh"

namespace TAR
{
    // Make sure that a block is exactly BLOCK_SIZE bytes
    static_assert(sizeof(Block) == BLOCK_SIZE);

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
        auto end_address = chksum + sizeof(as_header.chksum);

        for (std::uint16_t i = 0; i < BLOCK_SIZE; ++i)
        {
            if (&as_data[i] >= chksum && &as_data[i] < end_address)
                sum += 0x20;
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

        std::uint32_t blocks = bytes / BLOCK_SIZE;
        if (bytes % BLOCK_SIZE) blocks++;

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
        set_file_path(file_path);
        m_records_in_file = fs::file_size(m_file_path)/(m_blocking_factor*BLOCK_SIZE);

        m_stream.open(m_file_path, std::ios::in | std::ios::binary);
        if (!m_stream)
        {
            std::string error_msg("Could not open file ");
            error_msg.append(file_path);
            throw std::runtime_error(error_msg);
        }
    }

    Status InStream::read_block(Block& raw, bool advance)
    {
        if (m_should_read)
        {
            if (m_record_id < m_records_in_file)
            {
                Status st = read_record();
                if (st != Status::OK)
                    return st;
            }else
                return Status::END;
        }

        raw = m_record[m_block_id];

        if (advance)
            m_block_id++;

        if (m_block_id >= m_blocking_factor)
        {
            m_block_id = 0;
            m_record_id = m_stream.tellg() / (BLOCK_SIZE*m_blocking_factor);
            m_should_read = true;
        }

        return Status::OK;
    }

    Status InStream::read_record()
    {
        if (!m_record)
            m_record = std::make_unique<Block[]>(m_blocking_factor);

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
        if (read_record() != Status::OK)
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
        Block block;
        Status st = m_stream.read_block(block);
        if (st != Status::OK)
            return st;

        st = check_block(block);
        if (st != Status::OK)
            return st;

        if (block.as_header.typeflag == 'L')
        {
            auto name_bytes = unpack(block.as_header);
            file.name = std::string(name_bytes.begin(),
                                    name_bytes.end());

            st = m_stream.read_block(block);
            if (st != Status::OK)
                return st;

            st = check_block(block);
            if (st != Status::OK)
                return st;
        }else
        {
            if (block.as_header.name[sizeof(block.as_header.name)-1])
                file.name = std::string(block.as_header.name, 100);
            else
                file.name = std::string(block.as_header.name);
        }

        file.header = block.as_header;
        file.m_block_id = m_stream.block_id();
        file.m_record_id = m_stream.record_id();

        return Status::OK;
    }

    Data Parser::read_file(File& file)
    {
        m_stream.seek_record(file.m_record_id);
        m_stream.skip_blocks(file.m_block_id);

        return unpack(file.header);
    }

    Status Parser::list_files(std::list<File>& list)
    {
        m_stream.seek_record(0);
        list.clear();
        Status st;

        do
        {
            File file;
            st = next_file(file);
            if (st != Status::OK)
                break;

            std::uint32_t data_blocks = file.header.size_in_blocks();
            st = m_stream.skip_blocks(data_blocks);
            if (st != Status::OK)
                break;

            list.push_back(file);
        }while (st == Status::OK);

        if (st == Status::ERROR)
            return st;

        return Status::OK;
    }

    Status Parser::check_block(Block &block)
    {
        if (block.is_zero_block())
        {
            m_stream.read_block(block, false);
            if (block.is_zero_block())
                return Status::END;
            else
                return Status::ERROR;
        }

        std::uint32_t sum = block.calculate_checksum();
        std::uint32_t header_sum = std::stoi(block.as_header.chksum, nullptr, 8);
        if (sum != header_sum)
        {
            std::cerr << "Not matching checksums!\n";
            return Status::ERROR;
        }

        return Status::OK;
    }

    Data Parser::unpack(const Header& header)
    {
        std::uint32_t data_blocks = header.size_in_blocks();
        std::uint32_t size = header.size_in_bytes();
        Data bytes;
        bytes.reserve(size);

        for (std::uint32_t i = 0; i < data_blocks; ++i)
        {
            Block block;
            if (m_stream.read_block(block) != Status::OK)
                return {};

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
        if (m_block_id != 0)
            flush_record();

        set_file_path({});
        m_stream.close();
    }

    Status OutStream::write_block(const Block& block)
    {
        if (!m_record)
            m_record = std::make_unique<Block[]>(m_blocking_factor);

        if (m_block_id >= m_blocking_factor)
        {
            if (flush_record() != Status::OK)
                return Status::ERROR;

            m_record_id++;
        }

        m_record[m_block_id] = block;
        m_block_id++;

        return Status::OK;
    }

    Status OutStream::write_blocks(const std::vector<Block>& blocks)
    {
        for (const auto& block : blocks)
            if (write_block(block) != Status::OK)
                return Status::ERROR;

        return Status::OK;
    }

    Status OutStream::flush_record()
    {
        if (!m_record)
            return Status::ERROR;

        if (m_block_id < m_blocking_factor)
        {
            auto padding = m_blocking_factor - m_block_id;
            std::memset(m_record.get() + m_block_id, 0, BLOCK_SIZE*padding);
        }

        m_stream.write(reinterpret_cast<char*>(m_record.get()),
                      BLOCK_SIZE*m_blocking_factor);

        if (!m_stream)
            return Status::ERROR;

        m_block_id = 0;

        return Status::OK;
    }

    Archiver::Archiver(std::uint32_t blocking_factor)
        :m_stream(blocking_factor)
    {

    }

    Status Archiver::archive(const fs::path& src, const fs::path& dest)
    {
        if (!fs::exists(src))
        {
            std::cerr << src << " does not exist!\n";
            return Status::ERROR;
        }

        if (m_stream.open_output_file(dest) != Status::OK)
        {
            std::string error_msg("Could not create file ");
            error_msg.append(dest);
            throw std::runtime_error(error_msg);
        }

        std::queue<fs::path> to_be_visited;
        to_be_visited.push(src);

        while (!to_be_visited.empty())
        {
            const auto& thing = to_be_visited.front();
            Block header_block;
            create_header(thing, header_block);
            std::vector<Block> blocks;
            // Handle the case of too long names
            if (thing.string().size() > 100)
                create_long_name_blocks(thing.string(), blocks, header_block.as_header);

            if (fs::is_directory(thing))
            {
                for(auto const& entry: std::filesystem::directory_iterator{thing})
                    to_be_visited.push(entry.path());
                blocks.push_back(header_block);
            }
            else
            {
                blocks.push_back(header_block);
                if (pack(thing, blocks) != Status::OK)
                {
                    std::cerr << "Cound not read " << thing << '\n';
                    return Status::ERROR;
                }
            }

            m_stream.write_blocks(blocks);
            to_be_visited.pop();
        }

        // Write two zero blocks to indicate the end of the archive
        Block zeros;
        std::memset(&zeros, 0, sizeof(Block));
        m_stream.write_block(zeros);
        m_stream.write_block(zeros);
        m_stream.close_output_file();

        return Status::OK;
    }

    Status Archiver::create_header(const fs::path& path, Block& header_block)
    {
        struct stat info;
        Header& header = header_block.as_header;

        if (lstat(path.string().c_str(), &info) < 0)
        {
            std::cerr << "stat for " << path << " failed\n";
            return Status::ERROR;
        }

        std::memset(&header, 0, sizeof(Block));
        std::string name = path.string();
        if (name.size() >= 100)
            std::memcpy(header.name, path.string().c_str(), sizeof(header.name));
        else
            std::strncpy(header.name, path.string().c_str(), sizeof(header.name) - 1);

        std::sprintf(header.mode, "%0*o",
                     static_cast<int>(sizeof(header.mode)) - 1,
                     static_cast<std::uint16_t>(info.st_mode & ~S_IFMT));
        std::sprintf(header.uid, "%0*o",
                     static_cast<int>(sizeof(header.uid)) - 1,
                     info.st_uid);
        std::sprintf(header.gid, "%0*o",
                     static_cast<int>(sizeof(header.gid)) - 1,
                     info.st_gid);
        if (!fs::is_directory(path))
            std::sprintf(header.size, "%0*lo",
                         static_cast<int>(sizeof(header.size)) - 1,
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
            case S_IFSOCK: header.typeflag = 0;     break; // what is the correct value?
        }

        std::sprintf(header.mtime, "%lo", info.st_mtime);
        std::sprintf(header.magic, "ustar");

        header.version[0] = 0x20;
        header.version[1] = 0x20;

        struct passwd *pw = getpwuid(info.st_uid);
        if (pw)
            std::strncpy(header.uname, pw->pw_name, sizeof(header.uname) - 1);

        struct group  *gr = getgrgid(info.st_gid);
        if (gr)
            std::strncpy(header.gname, gr->gr_name, sizeof(header.gname) - 1);

        std::memset(header.devmajor, '0', sizeof(header.devmajor) - 1);
        std::memset(header.devminor, '0', sizeof(header.devminor) - 1);
        std::sprintf(header.chksum, "%0*o",
                     static_cast<int>(sizeof(header.chksum)) - 2,
                     header_block.calculate_checksum());
        header.chksum[sizeof(header.chksum)-1] = 0x20;

        return Status::OK;
    }

    void Archiver::create_long_name_blocks(const std::string& name,
                                           std::vector<Block>& blocks,
                                           const Header& real_header)
    {
        Block fake_block;
        std::memset(&fake_block, 0, sizeof(Block));
        Header& fake_header = fake_block.as_header;

        std::strncpy(fake_header.name, "././@LongName", sizeof(fake_header.name) - 1);
        std::strncpy(fake_header.mode, real_header.mode, sizeof(fake_header.mode));
        std::memset(fake_header.uid, '0', sizeof(fake_header.uid)-1);
        std::memset(fake_header.gid, '0', sizeof(fake_header.gid)-1);
        std::sprintf(fake_header.size, "%0*lo",
                     static_cast<int>(sizeof(fake_header.size)) - 1,
                     name.size());
        std::memset(fake_header.mtime, '0', sizeof(fake_header.mtime)-1);
        fake_header.typeflag = 'L';
        std::sprintf(fake_header.magic, "ustar");
        fake_header.version[0] = 0x20;
        fake_header.version[1] = 0x20;
        std::strncpy(fake_header.uname, "root", sizeof(fake_header.uname) - 1);
        std::strncpy(fake_header.gname, "root", sizeof(fake_header.gname) - 1);
        std::sprintf(fake_header.chksum, "%0*o",
                     static_cast<int>(sizeof(fake_header.chksum)) - 2,
                     fake_block.calculate_checksum());
        fake_header.chksum[sizeof(fake_header.chksum)-1] = 0x20;

        blocks.push_back(fake_block);

        std::size_t size = name.size();
        std::size_t nBlocks = size/BLOCK_SIZE;
        if (size % BLOCK_SIZE) nBlocks++;

        const char* pName = name.c_str();
        for (std::size_t i = 0; i < nBlocks; ++i)
        {
            Block block;
            if (i != nBlocks - 1)
                std::memcpy(&block, pName + i*BLOCK_SIZE, BLOCK_SIZE);
            else
            {
                std::memset(&block, 0, sizeof(Block));
                std::memcpy(&block, pName + i*BLOCK_SIZE, size % BLOCK_SIZE);
            }
            blocks.push_back(block);
        }
    }

    Status Archiver::pack(const fs::path& path, std::vector<Block>& blocks)
    {
        std::fstream in(path, std::ios::in | std::ios::binary);
        std::size_t total_bytes = fs::file_size(path);
        std::size_t total_blocks = total_bytes/BLOCK_SIZE;

        if (total_bytes%BLOCK_SIZE) total_blocks++;
        blocks.reserve(blocks.capacity() + total_blocks);

        std::array<Block, 10> cache;
        std::size_t blocks_read = 0;
        while (total_blocks > 0)
        {
            std::memset(cache.data(), 0, cache.size()*sizeof(Block));
            in.read(reinterpret_cast<char*>(cache.data()), cache.size()*sizeof(Block));
            if (total_blocks > cache.size())
                blocks_read = cache.size();
            else
                blocks_read = total_blocks;
            total_blocks -= blocks_read;

            if (total_blocks > 0 && !in)
                return Status::ERROR;

            blocks.insert(blocks.end(), cache.begin(), cache.begin() + blocks_read);
        }

        return Status::OK;
    }
}
