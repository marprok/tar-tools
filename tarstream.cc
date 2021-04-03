#include <cstring>
#include <string>
#include "tarstream.hh"

std::ostream& operator<<(std::ostream& os, const TARBlock& block)
{
    for (std::uint32_t i = 0; i < BLOCK_SIZE; ++i)
    {
        if (block.m_data[i])
            os << static_cast<char>(block.m_data[i]);
        else
            os << ".";
    }
    os << '\n';
    return os;
}

TARStream::TARStream(fs::path file_path, std::uint32_t blocking_factor)
    :m_blocking_factor(blocking_factor),
     m_block_id(m_blocking_factor),
     m_record_id(0),
     m_record(nullptr)
{
    m_file_path = fs::absolute(file_path);
    m_records_in_file = fs::file_size(m_file_path)/(m_blocking_factor*BLOCK_SIZE);
    std::cout << m_records_in_file << " records in file\n";
    m_stream.open(m_file_path);
    if (!m_stream)
        throw std::runtime_error("Could not open file"); // let the caller handle the case
}

TARStream::~TARStream()
{
    if (m_record)
    {
        delete[] m_record;
        m_record = nullptr;
    }
}

Status TARStream::read_block(TARBlock& raw)
{
    if (m_block_id >= m_blocking_factor)
    {
        if (_fetch_record() == Status::TAR_EOF)
            return Status::TAR_EOF;
        m_block_id = 0;
    }

    std::memcpy(raw.m_data, m_record + m_block_id*BLOCK_SIZE, BLOCK_SIZE);
    m_block_id++;

    return Status::TAR_OK;
}

Status TARStream::_fetch_record()
{
    if (!m_record)
        m_record = new std::uint8_t[BLOCK_SIZE*m_blocking_factor]; // let it fail
    else
        m_record_id++;

    if (m_record_id < m_records_in_file)
    {
        m_stream.read(reinterpret_cast<char*>(m_record), BLOCK_SIZE*m_blocking_factor);
        return Status::TAR_OK;
    }

    return Status::TAR_EOF;
}


TARParser::TARParser(TARStream &tar_stream)
    :m_tar_stream(tar_stream)
{
}

TARFile TARParser::get_next_file()
{
    TARFile file;
    TARBlock header_block;

    m_tar_stream.read_block(header_block);
    std::cout << header_block;

    TARHeader *header = &header_block.m_header;
    _secure_header(header_block.m_header);


    std::uint32_t size = std::stoi(header->size);
    std::cout << "size " << size << '\n';

    std::uint32_t data_blocks = size/BLOCK_SIZE;
    if (size%BLOCK_SIZE)
        data_blocks++;

    file.push_back(header_block);
    for (std::uint32_t i = 0; i < data_blocks; ++i)
    {
        TARBlock block;
        if (m_tar_stream.read_block(block) != Status::TAR_OK)
            return {}; // should not happen since the blocks must exist
        file.push_back(block);
    }
    return file;
}

void TARParser::_secure_header(TARHeader &header)
{
    header.name[sizeof(header.name)-1] = '\0';
    header.mode[sizeof(header.mode)-1] = '\0';
    header.uid[sizeof(header.uid)-1] = '\0';
    header.gid[sizeof(header.gid)-1] = '\0';
    header.size[sizeof(header.size)-1] = '\0';
    header.mtime[sizeof(header.mtime)-1] = '\0';
    header.chksum[sizeof(header.chksum)-1] = '\0';
    header.linkname[sizeof(header.linkname)-1] = '\0';
    header.magic[sizeof(header.magic)-1] = '\0';
    header.version[sizeof(header.version)-1] = '\0';
    header.uname[sizeof(header.uname)-1] = '\0';
    header.gname[sizeof(header.gname)-1] = '\0';
    header.devmajor[sizeof(header.devmajor)-1] = '\0';
    header.devminor[sizeof(header.devminor)-1] = '\0';
    header.prefix[sizeof(header.prefix)-1] = '\0';
}
