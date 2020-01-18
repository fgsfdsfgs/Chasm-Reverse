#pragma once
#include <memory>

#include "../vfs.hpp"

namespace PanzerChasm
{

namespace Sound
{

class ISoundData
{
public:
	virtual ~ISoundData(){}

public:
	enum class DataType
	{
		Signed8,
		Unsigned8,
		Signed16,
		Unsigned16,
	};

public:
	inline unsigned int GetDataSize() const
	{
		return
			sample_count_ * channel_count_ *
			( ( data_type_ == DataType::Signed8 || data_type_ == DataType::Unsigned8 ) ? 1u : 2u );
	}

public:
	unsigned int frequency_; // samples per seconds
	DataType data_type_;

	const void* data_;
	unsigned int sample_count_;
	unsigned int channel_count_;
};

typedef std::unique_ptr<ISoundData> ISoundDataConstPtr;


ISoundDataConstPtr LoadSound( const char* file_path, Vfs& vfs );
ISoundDataConstPtr LoadRawMonsterSound( const Vfs::FileContent& raw_sound_data );
ISoundDataConstPtr LoadCdTrack( unsigned int track );

} // namespace Sound

} // namespace PanzerChasm
