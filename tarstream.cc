#include <cstring>
#include "tarstream.hh"

std::ostream& operator<<(std::ostream& os, const RawBlock& block)
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

TARStream::Status TARStream::read_block(RawBlock& raw)
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

TARStream::Status TARStream::_fetch_record()
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
