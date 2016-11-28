/*****************************************************************************
 * Copyright (C) 2016 mg project
 *
 * Authors: MediaGoom <admin@mediagoom.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NONINFRINGEMENT. 
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * For more information, contact us at info@mediagoom.com.
 *****************************************************************************/
#pragma once

#include "MP4DynamicInfo.h"
#include <splitter.h>

#include <aes.h>

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

#define ENC_MISSING_SIZE(s) BLOCK_SIZE - (s % BLOCK_SIZE) 
#define ENC_SIZE(s) s + (ENC_MISSING_SIZE(s))

class HLSRenderer: public IDynamicRenderer
{
	 Cstring _root_m3u;
	 Cstring _body;

	 std::map<int, Cstring> _bit_rate_m3u;
	 std::vector<int> _bit_rate;

	 int _index;

	 Cstring _video_codec;
	 Cstring _audio_codec;

	 bool _has_audio;
	 bool _begin_stream;
	 bool _video_points;

	 int _audio_bit_rate;

	 Cstring _prefix;

	 bool _use_stream_name;

	 unsigned char key[BLOCK_SIZE];
	 bool _encrypted;

	 Cstring _key_file_name;

public:
	HLSRenderer() :
		  _index(-1)
		, _has_audio(false)
		, _audio_bit_rate(96000)
		, _root_m3u(10240)
		, _body(10240)
		, _begin_stream(false)
		, _video_points(false)
		, _prefix(L"video")
		, _use_stream_name(true)
		, _encrypted(false)
		, _key_file_name(L"hls3.key")
	{
	}

	void set_use_stream_name(bool rhs){ _use_stream_name = rhs; }

	void set_prefix(LPCTSTR psz_prefix)
	{
		_prefix = psz_prefix;
	}

	virtual void begin(unsigned __int64 duration)
	{		
		_audio_codec += L"mp4a.40.2";

		_root_m3u += L"#EXTM3U\n";
		_root_m3u += L"#EXT-X-VERSION:3\n";
	}

	virtual void begin_stream(LPCTSTR psz_name, int points, int bitrates, stream_type stype)
	{
		_begin_stream = true;
		_video_points = false;

		if (stream_type::stream_video == stype)
		{
			_video_points = true;

			if (_use_stream_name)
				_prefix = psz_name;
		}

	}

	virtual void add_video_bitrate(int bit_rate, LPCTSTR psz_type, int Width, int Height, BYTE* codec_private_data, int size)
	{
		_index++;

		_ASSERTE(size > 6);

		_video_codec.Clear();
		_video_codec += L"avc1.";
		//_video_codec.append_hex_buffer(codec_private_data + 3, 3);

		_video_codec.append_hex_buffer(codec_private_data + 3, 1);

		unsigned char profile_compatibility = codec_private_data[4];
		              profile_compatibility = 
						                      (profile_compatibility & 0x80) >> 7
										   || (profile_compatibility & 0x40) << 6
										   || (profile_compatibility & 0x20) << 5
										   || (profile_compatibility & 0x10) << 4
										   ;

	    _video_codec.append_hex_buffer(&profile_compatibility, 1);

		_video_codec.append_hex_buffer(codec_private_data + 5, 1);
	    

		_root_m3u += L"#EXT-X-STREAM-INF:BANDWIDTH=";
		_root_m3u += (bit_rate + _audio_bit_rate);
		_root_m3u += L",RESOLUTION=";
		_root_m3u += Width;
		_root_m3u += L"x";
		_root_m3u += Height;
		_root_m3u += L",CODECS=\"";
		_root_m3u += _video_codec; //TODO LOWER CASE?
		_root_m3u += L",";
		_root_m3u += _audio_codec;
		_root_m3u += L"\"\n";

		Cstring file_name = _prefix.clone();
				file_name += L"_";
		        file_name += bit_rate;
				file_name += ".m3u8";

		_bit_rate.push_back(bit_rate);

		_bit_rate_m3u[bit_rate] = file_name;

		_root_m3u += file_name;
		_root_m3u += L"\n";

	}
	virtual void add_audio_bitrate(int bit_rate
		, int AudioTag, int SamplingRate, int Channels, int BitsPerSample, int PacketSize
		, BYTE* codec_privet_data, int size)
	{
		_index++;

		_audio_bit_rate = bit_rate;
		_has_audio = true;
		 

		

		
	}
	virtual void add_point(unsigned __int64 computed_time, unsigned __int64 duration)
	{
		if(!_video_points)
			return;
		
		if(_begin_stream && 0 <= computed_time )
		{
			_begin_stream = false;
			
			_body += L"#EXTM3U\r\n";
			_body += L"#EXT-X-VERSION:3\r\n";
			_body += L"#EXT-X-ALLOW-CACHE:NO\r\n";
			_body += L"#EXT-X-MEDIA-SEQUENCE:0\r\n";
			_body += L"#EXT-X-TARGETDURATION:";
			_body += duration / 10000000UL;
			_body += "\r\n";
			_body += L"#EXT-X-PROGRAM-DATE-TIME:1970-01-01T00:00:00Z\r\n";

			if(_encrypted)
			{
				_body += L"#EXT-X-KEY:METHOD=AES-128,URI=\"";
				_body += _key_file_name;
				_body += L"\"\r\n";
			}

		}

		//duration = duration / 10UL;

		unsigned __int64 TNANO = duration % 1000000UL;

		_body += L"#EXTINF:";
		_body += duration / 10000000UL;
		_body += L".";
		_body += TNANO;
		_body += L",no-desc\r\n";
		_body += _prefix;
		_body += L"_";
		_body += L"$(BITRATE)";
		_body += L"_";
		_body += computed_time;
		_body += L".ts\r\n";
		
	}
    virtual void add_point_info(LPCTSTR psz_path, unsigned __int64 composition, unsigned __int64 computed)
	{
		
	}
	virtual void add_point_info_cross(LPCTSTR psz_path, unsigned __int64 composition, unsigned __int64 computed)
	{
		
	}
	virtual void end_point()
	{
		
	}
	virtual void end_stream()
	{
		
	}
	virtual void end()
	{
		if(!_video_points)
			return;

		_body += L"#EXT-X-ENDLIST\r\n";

	}

	Cstring & main(){return _root_m3u;}
	int bit_rate_count(){return _bit_rate.size();}

	Cstring bit_rate_name(int idx)
	{
		int bit_rate = _bit_rate[idx];
		return _bit_rate_m3u[bit_rate];
	}

	Cstring bit_rate_body(Cstring bit_rate)
	{
		Cstring body = _body.clone();

		return replace(body, bit_rate, L"$(BITRATE)");

	}
	
	Cstring bit_rate_body(int idx)
	{
		int bit_rate = _bit_rate[idx];
		Cstring sbit;
		        sbit += bit_rate;

		return bit_rate_body(sbit);
	}

	void set_key(unsigned char * pkey)
	{
		::memcpy(&key, pkey, BLOCK_SIZE);
		_encrypted = true;
	}

	const unsigned char * get_key() const {return key;}

	Cstring get_key_file_name() { return _key_file_name;}
	void set_key_file_name(const TCHAR* pszfilename){_key_file_name = pszfilename;}

//#pragma optimize( "", off )

	static HRESULT hls3_encrypt_buffer(unsigned char * pdest
		, unsigned int * pdest_size
		, const unsigned char * pclear
		, unsigned int clear_size
		, int sequence
		, unsigned char * pkey
		)
	{
		_ASSERTE(0 < clear_size);

		if(0 >= clear_size)
			return E_INVALIDARG;

		if(0 == (*pdest_size))
		{
			*pdest_size = ENC_SIZE(clear_size);
			 return S_FALSE;
		}

		_ASSERTE(ENC_SIZE(clear_size) <= (*pdest_size));

		if(ENC_SIZE(clear_size) > (*pdest_size))
		{
			return E_OUTOFMEMORY;
		}

		unsigned int dest_size = ENC_SIZE(clear_size);
		
		int missing = ENC_MISSING_SIZE(clear_size);

		if(1 > missing || missing > 16)
		{
			ALXTHROW_T(_T("INVALID PCKS7"));
		}

//#define OPTERR
#ifdef OPTERR


		Cstring msg = L"hls3_encrypt_buffer:\t";
		        msg += clear_size;
				msg += L"\t";
				msg += dest_size;
				msg += L"\t";
				msg += missing;
				msg += L"\t";
				msg += sequence;


				::OutputDebugString(msg);


#endif

		//pcks7
		_ASSERTE((clear_size + missing) == dest_size);

		unsigned int full_block = (clear_size / BLOCK_SIZE) * BLOCK_SIZE;

		_ASSERTE(0 == (full_block % BLOCK_SIZE));

		unsigned char last[BLOCK_SIZE]; //here we store the last block

		::memset(&last, missing, BLOCK_SIZE);

		::memcpy(&last, pclear + full_block, BLOCK_SIZE - missing);



		unsigned char iv[BLOCK_SIZE];

		::memset(iv, 0, BLOCK_SIZE);

		int sn = sequence;
		
		iv[15] =  sn        & 0x000000FF;
		iv[14] = (sn >> 8)  & 0x000000FF;
		iv[13] = (sn >> 16) & 0x000000FF;
		iv[12] = (sn >> 24) & 0x000000FF;

		aes_encrypt_ctx ctx[1];

		AES_RETURN ret = aes_encrypt_key(pkey, BLOCK_SIZE, ctx);
		_ASSERTE(0 == ret);

		ret = aes_cbc_encrypt(pclear, pdest, full_block, iv, ctx);
		_ASSERTE(0 == ret);

		if(BLOCK_SIZE >= missing)
		{
			_ASSERTE(0 != (clear_size % BLOCK_SIZE));

			ret = aes_cbc_encrypt(last, pdest + full_block, BLOCK_SIZE, iv, ctx);
			_ASSERTE(0 == ret);
		}

		*pdest_size = dest_size;

		return S_OK;
	}


//#pragma optimize( "", on )

};


