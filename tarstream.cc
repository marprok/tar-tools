#include <cstring>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include "tarstream.hh"

std::ostream& operator<<(std::ostream& os, const TARHeader& header)
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
    os << "version: " << header.version << '\n';
    os << "uname: " << header.uname << '\n';
    os << "gname: " << header.gname << '\n';
    os << "devmajor: " << header.devmajor << '\n';
    os << "devminor: " << header.devminor << '\n';
    os << "prefix: " << header.prefix << '\n';

    return os;
}

std::ostream& operator<<(std::ostream& os, const TARBlock& block)
{
    for (std::uint32_t i = 0; i < BLOCK_SIZE; ++i)
    {
        if (block.m_data[i])
            os << static_cast<char>(block.m_data[i]);
        else
            os << ".";
    }
    return os;
}

std::uint32_t TARBlock::calculate_checksum() const
{
    std::uint32_t sum = 0;
    const std::uint8_t* chksum = reinterpret_cast<const uint8_t*>(m_header.chksum);
    for (std::uint16_t i = 0; i < BLOCK_SIZE; ++i)
    {
        if (&m_data[i] >= chksum &&
            &m_data[i] < (chksum + sizeof(m_header.chksum)))
            sum+=' ';
        else
            sum += m_data[i];
    }
    return sum;
}

bool TARBlock::is_zero_block() const
{
    std::uint32_t sum = 0;
    for (std::uint16_t i = 0; i < BLOCK_SIZE; ++i)
        sum += m_data[i];

    return sum == 0;
}

std::uint32_t TARHeader::size_in_blocks() const
{
    std::uint32_t bytes = std::stoi(size, nullptr, 8);
    if (!bytes)
        return 0;

    std::uint32_t blocks = bytes/BLOCK_SIZE;
    if (bytes % BLOCK_SIZE)
        blocks++;

    return blocks;
}

std::uint32_t TARHeader::size_in_bytes() const
{
    return std::stoi(size, nullptr, 8);
}

TARStream::TARStream(fs::path file_path, std::uint32_t blocking_factor)
    :m_blocking_factor(blocking_factor),
     m_block_id(0),
     m_record_id(0),
     m_empty(true)
{
    m_file_path = fs::absolute(file_path);
    m_records_in_file = fs::file_size(m_file_path)/(m_blocking_factor*BLOCK_SIZE);
    std::cout << m_records_in_file << " records in file\n";

    m_stream.open(m_file_path, std::ios::in | std::ios::binary);
    if (!m_stream)
        throw std::runtime_error("Could not open file"); // let the caller handle the case
}

Status TARStream::_read_record()
{
    if (!m_record)
        m_record = std::make_unique<std::uint8_t[]>(BLOCK_SIZE*m_blocking_factor);

    if (m_record_id < m_records_in_file)
    {
        m_stream.read(reinterpret_cast<char*>(m_record.get()),
                      BLOCK_SIZE*m_blocking_factor);

        if (!m_stream)
            return Status::TAR_ERROR;
        m_empty = false;
    }else
        return Status::TAR_EOF;

    return Status::TAR_OK;
}

Status TARStream::read_block(TARBlock& raw, bool advance)
{
    if (m_empty)
    {
        Status status = _read_record();
        if (status != Status::TAR_OK)
            return status;
    }

    auto from = m_record.get() + m_block_id*BLOCK_SIZE;
    auto to = from + BLOCK_SIZE;
    std::copy(from, to, raw.m_data);

    if (advance)
        m_block_id++;

    if (m_block_id >= m_blocking_factor)
    {
        m_block_id = 0;
        m_record_id = m_stream.tellg() / (BLOCK_SIZE*m_blocking_factor);
        std::memset(m_record.get(), 0 , BLOCK_SIZE*m_blocking_factor);
        m_empty = true;
    }

    return Status::TAR_OK;
}

Status TARStream::seek_record(std::uint32_t record_id)
{
    m_stream.seekg(record_id*BLOCK_SIZE*m_blocking_factor);
    if (!m_stream)
        return Status::TAR_ERROR;

    m_record_id = record_id;
    if (_read_record() != Status::TAR_OK)
        return Status::TAR_ERROR;

    m_block_id = 0;
    return Status::TAR_OK;
}

Status TARStream::skip_blocks(std::uint32_t count)
{
    if (m_block_id + count < m_blocking_factor)
        m_block_id += count;
    else
    {
        // go to the next record(a.k.a align the records)
        count -= m_blocking_factor - m_block_id;
        m_record_id += count / m_blocking_factor + 1;

        if (seek_record(m_record_id) != Status::TAR_OK)
            return Status::TAR_ERROR;
        m_block_id = count % m_blocking_factor;
    }
    return Status::TAR_OK;
}

std::uint32_t TARStream::record_id() { return m_record_id; }
std::uint32_t TARStream::block_id() { return m_block_id; }

TARParser::TARParser(TARStream &tar_stream)
    :m_tar(tar_stream)
{
}

Status TARParser::next_file(TARFile& file)
{
    TARBlock header_block;
    Status status = m_tar.read_block(header_block);
    if (status != Status::TAR_OK)
        return status;

    if (header_block.is_zero_block())
    {
        TARBlock block;
        m_tar.read_block(block, false);
        if (block.is_zero_block())
            return Status::TAR_EOF;
        else
            return Status::TAR_ERROR;
    }

    if (!_check_block(header_block))
        return Status::TAR_ERROR;

    file.header = header_block.m_header; // deep copy
    file.m_block_id = m_tar.block_id();
    file.m_record_id = m_tar.record_id();

    return Status::TAR_OK;
}

TARData TARParser::read_file(TARFile& file)
{
    m_tar.seek_record(file.m_record_id);
    m_tar.skip_blocks(file.m_block_id);

   return _unpack(file.header);
}

Status TARParser::list_files(TARList& list)
{
    m_tar.seek_record(0);
    list.clear();
    while (1)
    {
        TARFile file;
        Status status = next_file(file);
        if (status == Status::TAR_EOF)
            return Status::TAR_OK;
        else if (status != Status::TAR_OK)
            return status;

        std::uint32_t data_blocks = file.header.size_in_blocks();
        m_tar.skip_blocks(data_blocks);
        list.push_back(file);
    }

    return Status::TAR_OK;
}

#if 0
TARExtended TARParser::parse_extended(const TARFile &file)
{
    if (file.header.typeflag != 'x' && file.header.typeflag != 'g')
        return {};

    std::string data(file.data.begin(), file.data.end());
    std::istringstream ss(data);
    std::unordered_map<std::string, std::string> extended;
    for (std::string line; std::getline(ss, line);)
    {
        auto space_pos = line.find(" ");
        if (space_pos != std::string::npos)
        {
            auto size = line.substr(0, space_pos);
            auto equals_pos = line.find("=");
            if (equals_pos != std::string::npos)
            {
                auto elem = line.substr(space_pos+1, equals_pos - space_pos - 1);
                auto value = line.substr(equals_pos+1);
                extended[elem] = value;
            }
        }
    }
    return extended;
}
#endif
bool TARParser::_check_block(TARBlock &block)
{
    std::uint32_t sum = block.calculate_checksum();
    std::uint32_t header_sum = std::stoi(block.m_header.chksum, nullptr, 8);

    if (sum != header_sum)
        return false;

    block.m_header.name[sizeof(block.m_header.name)-1] = '\0';
    block.m_header.mode[sizeof(block.m_header.mode)-1] = '\0';
    block.m_header.uid[sizeof(block.m_header.uid)-1] = '\0';
    block.m_header.gid[sizeof(block.m_header.gid)-1] = '\0';
    block.m_header.size[sizeof(block.m_header.size)-1] = '\0';
    block.m_header.mtime[sizeof(block.m_header.mtime)-1] = '\0';
    block.m_header.chksum[sizeof(block.m_header.chksum)-1] = '\0';
    block.m_header.linkname[sizeof(block.m_header.linkname)-1] = '\0';
    block.m_header.magic[sizeof(block.m_header.magic)-1] = '\0';
    block.m_header.version[sizeof(block.m_header.version)-1] = '\0';
    block.m_header.uname[sizeof(block.m_header.uname)-1] = '\0';
    block.m_header.gname[sizeof(block.m_header.gname)-1] = '\0';
    block.m_header.devmajor[sizeof(block.m_header.devmajor)-1] = '\0';
    block.m_header.devminor[sizeof(block.m_header.devminor)-1] = '\0';
    block.m_header.prefix[sizeof(block.m_header.prefix)-1] = '\0';

    return true;
}

TARData TARParser::_unpack(const TARHeader& header)
{
    std::uint32_t data_blocks = header.size_in_blocks();
    std::uint32_t size = header.size_in_bytes();
    TARData bytes;
    bytes.reserve(size);

    for (std::uint32_t i = 0; i < data_blocks; ++i)
    {
        TARBlock block;
        if (m_tar.read_block(block) != Status::TAR_OK)
            return {}; // should not happen since the blocks must exist

        if (size)
        {
            std::uint32_t bytes_to_copy = BLOCK_SIZE;
            if (size < BLOCK_SIZE)
                bytes_to_copy = size;

            bytes.insert(bytes.end(), block.m_data, block.m_data + bytes_to_copy);
            size -= bytes_to_copy;
        }
    }

    return bytes;
}

