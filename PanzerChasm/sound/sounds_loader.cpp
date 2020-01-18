#include <cstring>

#include <SDL_audio.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "../assert.hpp"
#include "../log.hpp"
#include "../vfs.hpp"

#include "sounds_loader.hpp"

namespace PanzerChasm
{

namespace Sound
{

namespace
{

const char* ExtractExtension( const char* const file_path )
{
	unsigned int pos= std::strlen( file_path );

	while( pos > 0u && file_path[ pos ] != '.' )
		pos--;

	return file_path + pos + 1u;
}

class RawPCMSoundData final : public ISoundData
{
public:
	RawPCMSoundData( Vfs::FileContent data )
	{
		pcm_data_= std::move( data );

		frequency_= 11025u;
		data_type_= DataType::Unsigned8;
		data_= pcm_data_.data();
		sample_count_= pcm_data_.size();
	}

	virtual ~RawPCMSoundData() override {}

private:
	Vfs::FileContent pcm_data_;
};

class RawMonsterSoundData final : public ISoundData
{
public:
	RawMonsterSoundData( Vfs::FileContent data )
	{
		pcm_data_= std::move( data );

		frequency_= 11025u;
		data_type_= DataType::Signed8;
		data_= pcm_data_.data();
		sample_count_= pcm_data_.size();
		channel_count_= 1;
	}

	virtual ~RawMonsterSoundData() override {}

private:
	Vfs::FileContent pcm_data_;
};

// TODO: actually make this streaming
class VorbisSoundData final : public ISoundData
{
public:
	VorbisSoundData( std::FILE* f )
	{
		frequency_= 22050;
		data_type_= DataType::Unsigned8;
		data_= nullptr;
		sample_count_= 0u;
		channel_count_= 0u;

		if( ov_open( f, &vf_, nullptr, 0 ) < 0 )
		{
			Log::Warning( "Could not open OGG/Vorbis file" );
			return;
		}

		vorbis_info* vi = ov_info( &vf_, -1 );
		if( vi == nullptr )
		{
			Log::Warning( "Invalid OGG/Vorbis file" );
			return;
		}

		frequency_= vi->rate;
		channel_count_= vi->channels;
		sample_count_= ov_pcm_total( &vf_, -1 );
		data_type_= DataType::Signed16;

		const auto total_size= sample_count_ * channel_count_;
		Sint16* buf= new Sint16[ total_size ];
		std::memset( buf, 0, total_size * 2 );

		Sint16* out= buf;
		while( out < buf + total_size )
		{
			int bs;
			auto ret= ov_read( &vf_, (char*)out, 4096, 0, 2, 1, &bs );
			if( ret <= 0 )
				break;
			else
				out+= ret / 2;
		}

		ov_clear( &vf_ );
		data_= (const void*)buf;
	}

	virtual ~VorbisSoundData() override
	{
		if( data_ != nullptr )
			delete (Sint16*)data_;
		data_= nullptr;
	}

private:
	OggVorbis_File vf_;
};

class WavSoundData final : public ISoundData
{
public:
	WavSoundData( const Vfs::FileContent& data )
	{
		bool ok= false;

		SDL_AudioSpec spec;
		spec.freq= 0;
		spec.format= 0;
		spec.channels= 0;
		Uint32 buffer_length_butes;

		SDL_RWops* const rw_ops=
			SDL_RWFromConstMem( data.data(), data.size() );

		if( rw_ops != nullptr )
		{
			const SDL_AudioSpec* const result=
				SDL_LoadWAV_RW(
					rw_ops, 1, // 1 - means free rw_ops
					&spec,
					&wav_buffer_, &buffer_length_butes );

			ok= result != nullptr;
			PC_ASSERT( result == nullptr || result == &spec );
		}

		const unsigned int bits= SDL_AUDIO_BITSIZE( spec.format );
		const bool is_signed= SDL_AUDIO_ISSIGNED( spec.format ) != 0;

		if( !( bits == 16u || bits == 8u ) )
			ok= false;
		if( SDL_AUDIO_ISFLOAT( spec.format ) || spec.channels != 1u )
			ok= false;

		if( ok )
		{
			frequency_= spec.freq;
			data_type_=
				bits == 8u
					? ( is_signed ? DataType::Signed8  : DataType::Unsigned8  )
					: ( is_signed ? DataType::Signed16 : DataType::Unsigned16 );
			data_= wav_buffer_;
			sample_count_= buffer_length_butes / ( bits / 8u );
			channel_count_= 1;
		}
		else
		{
			// Make dummy
			wav_buffer_= nullptr;
			frequency_= 22050;
			data_type_= DataType::Unsigned8;
			data_= nullptr;
			sample_count_= 0u;
			channel_count_= 0u;
		}
	}

	virtual ~WavSoundData() override
	{
		if( wav_buffer_ != nullptr )
			SDL_FreeWAV( wav_buffer_ );
	}

private:
	Uint8* wav_buffer_= nullptr;
};

} // namespace

ISoundDataConstPtr LoadSound( const char* file_path, Vfs& vfs )
{
	Vfs::FileContent file_content= vfs.ReadFile( file_path );
	if( file_content.empty() )
	{
		Log::Warning( "Can not load \"", file_path, "\"" );
		return nullptr;
	}

	const char* const extension= ExtractExtension( file_path );

	if( std::strcmp( extension, "WAV" ) == 0 ||
		std::strcmp( extension, "wav" ) == 0 )
	{
		return ISoundDataConstPtr( new WavSoundData( file_content ) );
	}
	else // *.SFX, *.PCM, *.RAW files
	{
		return ISoundDataConstPtr( new RawPCMSoundData( std::move( file_content ) ) );
	}

	return nullptr;
}

ISoundDataConstPtr LoadRawMonsterSound( const Vfs::FileContent& raw_sound_data )
{
	if( raw_sound_data.empty() )
		return nullptr;

	return ISoundDataConstPtr( new RawMonsterSoundData( raw_sound_data ) );
}

ISoundDataConstPtr LoadCdTrack( unsigned int track )
{
	static const char* c_cdtrack_dir= "music";

	if( track < 1 )
	{
		Log::Warning( "Incorrect CD track number: ", track );
		return nullptr;
	}

	char trackpath[ 256u ];
	std::snprintf( trackpath, sizeof( trackpath ), "%s/track%02u.ogg", c_cdtrack_dir, track );

	std::FILE* f= std::fopen( trackpath, "rb" );
	if( !f )
	{
		Log::Warning( "Can not open CD track file \"", trackpath, "\"" );
		return nullptr;
	}

	auto ret= ISoundDataConstPtr( new VorbisSoundData( f ) );
	std::fclose( f );
	return ret;
}

} // namespace Sound

} // namespace PanzerChasm
