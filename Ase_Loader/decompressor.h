/*
By Mark Adler - https://github.com/madler/zlib/blob/master/adler32.c

Copyright (C) 1995-2017 Jean-loup Gailly and Mark Adler
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
Jean-loup Gailly        Mark Adler
jloup@gzip.org          madler@alumni.caltech.edu
The data format used by the zlib library is described by RFCs (Request for
Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
(zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
*/

/*
MIT License

Copyright (c) 2020 Artexety

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <iostream>
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdio.h>

#define BASE 65521U
#define NMAX 5552

#define DO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);
#define MOD(a) a %= BASE
#define MOD28(a) a %= BASE
#define MOD63(a) a %= BASE

inline unsigned int adler32_z(unsigned int adler, const unsigned char *buf, unsigned int len) {

	unsigned long sum2;
	unsigned n;

	/* split Adler-32 into component sums */
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* in case user likes doing a byte at a time, keep it fast */
	if (len == 1) {
		adler += buf[0];
		if (adler >= BASE)
			adler -= BASE;
		sum2 += adler;
		if (sum2 >= BASE)
			sum2 -= BASE;
		return adler | (sum2 << 16);
	}

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if (buf == NULL)
		return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	if (len < 16) {
		while (len--) {
			adler += *buf++;
			sum2 += adler;
		}
		if (adler >= BASE)
			adler -= BASE;
		MOD28(sum2);            /* only added so many BASE's */
		return adler | (sum2 << 16);
	}

	/* do length NMAX blocks -- requires just one modulo operation */
	while (len >= NMAX) {
		len -= NMAX;
		n = NMAX / 16;          /* NMAX is divisible by 16 */
		do {
			DO16(buf);          /* 16 sums unrolled */
			buf += 16;
		} while (--n);
		MOD(adler);
		MOD(sum2);
	}

	/* do remaining bytes (less than NMAX, still just one modulo) */
	if (len) {                  /* avoid modulos if none remaining */
		while (len >= 16) {
			len -= 16;
			DO16(buf);
			buf += 16;
		}
		while (len--) {
			adler += *buf++;
			sum2 += adler;
		}
		MOD(adler);
		MOD(sum2);
	}

	/* return recombined sums */
	return adler | (sum2 << 16);
}

#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
#define X64BIT_SHIFTER
#endif /* defined(_M_X64) */

#ifdef X64BIT_SHIFTER
typedef unsigned long long shifter_t;
#else
typedef unsigned int shifter_t;
#endif /* X64BIT_SHIFTER */

struct BitReader {
	/**
	* Initialize bit reader
	*
	* @param in_block pointer to the start of the compressed block
	* @param in_blockend pointer to the end of the compressed block + 1
	*/
	void Init(unsigned char *in_block, unsigned char *in_blockend) {
		this->in_block = in_block;
		this->in_blockend = in_blockend;
		this->in_blockstart = in_block;
	}

	/** Refill 32 bits at a time if the architecture allows it, otherwise do nothing. */
	void Refill32() {

		#ifdef X64BIT_SHIFTER
		if (this->shifter_bit_count <= 32 && (this->in_block + 4) <= this->in_blockend)
		{
			#ifdef defined(_M_X64) || defined(__x86_64__)
			this->shifter_data |= (((shifter_t)(*((unsigned int*)this->in_block))) << this->shifter_bit_count);
			this->shifter_bit_count += 32;
			this->in_block += 4;
			#else
			this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
			this->shifter_bit_count += 8;
			this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
			this->shifter_bit_count += 8;
			this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
			this->shifter_bit_count += 8;
			this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
			this->shifter_bit_count += 8;
			#endif
		}
		#endif /* X64BIT_SHIFTER */
	}

	/**
	* Consume variable bit-length value, after reading it with PeekBits()
	*
	* @param n size of value to consume, in bits
	*/
	void ConsumeBits(const int n) {
		this->shifter_data >>= n;
		this->shifter_bit_count -= n;
	}

	/**
	* Read variable bit-length value
	*
	* @param n size of value in bits (number of bits to read), 0..16
	*
	* @return value, or -1 for failure
	*/
	unsigned int GetBits(const int n) {
		if (this->shifter_bit_count < n) {
			if (this->in_block < this->in_blockend) {
				this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
				this->shifter_bit_count += 8;

				if (this->in_block < this->in_blockend) {
					this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
					this->shifter_bit_count += 8;
				}
			}
			else return -1;
		}

		unsigned int value = this->shifter_data & ((1 << n) - 1);
		this->shifter_data >>= n;
		this->shifter_bit_count -= n;
		return value;
	}

	/**
	* Peek at a 16-bit value in the bitstream (lookahead)
	*
	* @return value
	*/
	unsigned int PeekBits() {
		if (this->shifter_bit_count < 16) {
			if (this->in_block < this->in_blockend) {

				this->shifter_data |= (((shifter_t)(*this->in_block++)) << this->shifter_bit_count);
				if (this->in_block < this->in_blockend)
					this->shifter_data |= (((shifter_t)(*this->in_block++)) << (this->shifter_bit_count + 8));
				this->shifter_bit_count += 16;
			}
		}

		return this->shifter_data & 0xffff;
	}

	/** Re-align bitstream on a byte */
	int ByteAllign() {

		while (this->shifter_bit_count >= 8) {
			this->shifter_bit_count -= 8;
			this->in_block--;
			if (this->in_block < this->in_blockstart) return -1;
		}

		this->shifter_bit_count = 0;
		this->shifter_data = 0;
		return 0;
	}

	void ModifyInBlock(const int v) {
		this->in_block += v;
	}


	int shifter_bit_count = 0;
	shifter_t shifter_data = 0;
	unsigned char *in_block = nullptr;
	unsigned char *in_blockend = nullptr;
	unsigned char *in_blockstart = nullptr;
};

const int kMaxSymbols = 288;
const int kCodeLenSyms = 19;
const int kFastSymbolBits = 10;

struct HuffmanDecoder {
	/**
	* Prepare huffman tables
	*
	* @param rev_symbol_table array of 2 * symbols entries for storing the reverse lookup table
	* @param code_length codeword lengths table
	*
	* @return 0 for success, -1 for failure
	*/
	int PrepareTable(unsigned int *rev_symbol_table, const int read_symbols, const int symbols, unsigned char *code_length) {

		int num_symbols_per_len[16];
		int i;

		if (read_symbols < 0 || read_symbols > kMaxSymbols || symbols < 0 || symbols > kMaxSymbols || read_symbols > symbols)
			return -1;
		this->symbols_ = symbols;


		for (i = 0; i < 16; i++)
			num_symbols_per_len[i] = 0;

		for (i = 0; i < read_symbols; i++) {
			if (code_length[i] >= 16) return -1;
			num_symbols_per_len[code_length[i]]++;
		}

		this->starting_pos_[0] = 0;
		this->num_sorted_ = 0;
		for (i = 1; i < 16; i++) {
			this->starting_pos_[i] = this->num_sorted_;
			this->num_sorted_ += num_symbols_per_len[i];
		}

		for (i = 0; i < symbols; i++)
			rev_symbol_table[i] = -1;

		for (i = 0; i < read_symbols; i++) {
			if (code_length[i])
				rev_symbol_table[this->starting_pos_[code_length[i]]++] = i;
		}

		return 0;
	}

	/**
	* Finalize huffman codewords for decoding
	*
	* @param rev_symbol_table array of 2 * symbols entries that contains the reverse lookup table
	*
	* @return 0 for success, -1 for failure
	*/
	int FinalizeTable(unsigned int *rev_symbol_table) {
		const int symbols = this->symbols_;
		unsigned int canonical_code_word = 0;
		unsigned int *rev_code_length_table = rev_symbol_table + symbols;
		int canonical_length = 1;
		int i;

		for (i = 0; i < (1 << kFastSymbolBits); i++) this->fast_symbol_[i] = 0;
		for (i = 0; i < 16; i++)                     this->start_index_[i] = 0;

		i = 0;
		while (i < this->num_sorted_) {

			if (canonical_length >= 16) return -1;
			this->start_index_[canonical_length] = i - canonical_code_word;

			while (i < this->starting_pos_[canonical_length]) {

				if (i >= symbols) return -1;
				rev_code_length_table[i] = canonical_length;

				if (canonical_code_word >= (1U << canonical_length)) return -1;

				if (canonical_length <= kFastSymbolBits) {

					unsigned int rev_word;

					/* Get upside down codeword (branchless method by Eric Biggers) */
					rev_word = ((canonical_code_word & 0x5555) << 1) | ((canonical_code_word & 0xaaaa) >> 1);
					rev_word = ((rev_word & 0x3333) << 2) | ((rev_word & 0xcccc) >> 2);
					rev_word = ((rev_word & 0x0f0f) << 4) | ((rev_word & 0xf0f0) >> 4);
					rev_word = ((rev_word & 0x00ff) << 8) | ((rev_word & 0xff00) >> 8);
					rev_word = rev_word >> (16 - canonical_length);

					int slots = 1 << (kFastSymbolBits - canonical_length);
					while (slots) {
						this->fast_symbol_[rev_word] = (rev_symbol_table[i] & 0xffffff) | (canonical_length << 24);
						rev_word += (1 << canonical_length);
						slots--;
					}
				}

				i++;
				canonical_code_word++;
			}
			canonical_length++;
			canonical_code_word <<= 1;
		}

		while (i < symbols) {
			rev_symbol_table[i] = -1;
			rev_code_length_table[i++] = 0;
		}

		return 0;
	}

	/**
	* Read fixed bit size code lengths
	*
	* @param len_bits number of bits per code length
	* @param read_symbols number of symbols actually read
	* @param symbols number of symbols to build codes for
	* @param code_length output code lengths table
	* @param bit_reader bit reader context
	*
	* @return 0 for success, -1 for failure
	*/
	static int ReadRawLengths(const int len_bits, const int read_symbols, const int symbols, unsigned char *code_length, BitReader *bit_reader) {

		const unsigned char code_len_syms[kCodeLenSyms] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
		int i;

		if (read_symbols < 0 || read_symbols > kMaxSymbols || symbols < 0 || symbols > kMaxSymbols || read_symbols > symbols)
			return -1;

		i = 0;
		while (i < read_symbols) {
			unsigned int length = bit_reader->GetBits(len_bits);
			if (length == -1) return -1;
			code_length[code_len_syms[i++]] = length;
		}

		while (i < symbols) {
			code_length[code_len_syms[i++]] = 0;
		}

		return 0;
	}

	/**
	* Read huffman-encoded code lengths
	*
	* @param tables_rev_symbol_table reverse lookup table for code lengths
	* @param read_symbols number of symbols actually read
	* @param symbols number of symbols to build codes for
	* @param code_length output code lengths table
	* @param bit_reader bit reader context
	*
	* @return 0 for success, -1 for failure
	*/
	int ReadLength(const unsigned int *tables_rev_symbol_table, const int read_symbols, const int symbols, unsigned char *code_length, BitReader *bit_reader) {

		if (read_symbols < 0 || symbols < 0 || read_symbols > symbols) return -1;

		int i = 0;
		unsigned int previous_length = 0;

		while (i < read_symbols) {

			unsigned int length = this->ReadValue(tables_rev_symbol_table, bit_reader);
			if (length == -1) return -1;

			if (length < 16) {
				previous_length = length;
				code_length[i++] = previous_length;
			}
			else {
				unsigned int run_length = 0;

				if (length == 16) {
					int extra_run_length = bit_reader->GetBits(2);
					if (extra_run_length == -1) return -1;
					run_length = 3 + extra_run_length;
				}
				else if (length == 17) {
					int extra_run_length = bit_reader->GetBits(3);
					if (extra_run_length == -1) return -1;
					previous_length = 0;
					run_length = 3 + extra_run_length;
				}
				else if (length == 18) {
					int extra_run_length = bit_reader->GetBits(7);
					if (extra_run_length == -1) return -1;
					previous_length = 0;
					run_length = 11 + extra_run_length;
				}

				while (run_length && i < read_symbols) {
					code_length[i++] = previous_length;
					run_length--;
				}
			}
		}

		while (i < symbols)
			code_length[i++] = 0;

		return 0;
	}

	/**
	* Decode next symbol
	*
	* @param rev_symbol_table reverse lookup table
	* @param bit_reader bit reader context
	*
	* @return symbol, or -1 for error
	*/
	unsigned int ReadValue(const unsigned int *rev_symbol_table, BitReader *bit_reader) {

		unsigned int stream = bit_reader->PeekBits();
		unsigned int fast_sym_bits = this->fast_symbol_[stream & ((1 << kFastSymbolBits) - 1)];

		if (fast_sym_bits) {
			bit_reader->ConsumeBits(fast_sym_bits >> 24);
			return fast_sym_bits & 0xffffff;
		}

		const unsigned int *rev_code_length_table = rev_symbol_table + this->symbols_;
		unsigned int code_word = 0;
		int bits = 1;

		do	{
			code_word |= (stream & 1);
			unsigned int table_index = this->start_index_[bits] + code_word;

			if (table_index < this->symbols_) {
				if (bits == rev_code_length_table[table_index]) {
					bit_reader->ConsumeBits(bits);
					return rev_symbol_table[table_index];
				}
			}
			code_word <<= 1;
			stream >>= 1;
			bits++;
		}
		while (bits < 16);

		return -1;
	}

	unsigned int fast_symbol_[1 << kFastSymbolBits];
	unsigned int start_index_[16];
	unsigned int symbols_;
	int num_sorted_;
	int starting_pos_[16];
};


#define MATCHLEN_PAIR(__base,__dispbits) ((__base) | ((__dispbits) << 16) | 0x8000)
#define OFFSET_PAIR(__base,__dispbits) ((__base) | ((__dispbits) << 16))

/*-- zlib static and dynamic blocks inflater --*/
const int kCodeLenBits = 3;
const int kLiteralSyms = 288;
const int kEODMarkerSym = 256;
const int kMatchLenSymStart = 257;
const int kMatchLenSyms = 29;
const int kOffsetSyms = 32;
const int kMinMatchSize = 3;

constexpr unsigned int kMatchLenCode[kMatchLenSyms] = {
   MATCHLEN_PAIR(kMinMatchSize + 0, 0), MATCHLEN_PAIR(kMinMatchSize + 1, 0), MATCHLEN_PAIR(kMinMatchSize + 2, 0), MATCHLEN_PAIR(kMinMatchSize + 3, 0), MATCHLEN_PAIR(kMinMatchSize + 4, 0),
   MATCHLEN_PAIR(kMinMatchSize + 5, 0), MATCHLEN_PAIR(kMinMatchSize + 6, 0), MATCHLEN_PAIR(kMinMatchSize + 7, 0), MATCHLEN_PAIR(kMinMatchSize + 8, 1), MATCHLEN_PAIR(kMinMatchSize + 10, 1),
   MATCHLEN_PAIR(kMinMatchSize + 12, 1), MATCHLEN_PAIR(kMinMatchSize + 14, 1), MATCHLEN_PAIR(kMinMatchSize + 16, 2), MATCHLEN_PAIR(kMinMatchSize + 20, 2), MATCHLEN_PAIR(kMinMatchSize + 24, 2),
   MATCHLEN_PAIR(kMinMatchSize + 28, 2), MATCHLEN_PAIR(kMinMatchSize + 32, 3), MATCHLEN_PAIR(kMinMatchSize + 40, 3), MATCHLEN_PAIR(kMinMatchSize + 48, 3), MATCHLEN_PAIR(kMinMatchSize + 56, 3),
   MATCHLEN_PAIR(kMinMatchSize + 64, 4), MATCHLEN_PAIR(kMinMatchSize + 80, 4), MATCHLEN_PAIR(kMinMatchSize + 96, 4), MATCHLEN_PAIR(kMinMatchSize + 112, 4), MATCHLEN_PAIR(kMinMatchSize + 128, 5),
   MATCHLEN_PAIR(kMinMatchSize + 160, 5), MATCHLEN_PAIR(kMinMatchSize + 192, 5), MATCHLEN_PAIR(kMinMatchSize + 224, 5), MATCHLEN_PAIR(kMinMatchSize + 255, 0),
};

constexpr unsigned int kOffsetCode[kOffsetSyms] = {
   OFFSET_PAIR(1, 0), OFFSET_PAIR(2, 0), OFFSET_PAIR(3, 0), OFFSET_PAIR(4, 0), OFFSET_PAIR(5, 1), OFFSET_PAIR(7, 1), OFFSET_PAIR(9, 2), OFFSET_PAIR(13, 2), OFFSET_PAIR(17, 3), OFFSET_PAIR(25, 3),
   OFFSET_PAIR(33, 4), OFFSET_PAIR(49, 4), OFFSET_PAIR(65, 5), OFFSET_PAIR(97, 5), OFFSET_PAIR(129, 6), OFFSET_PAIR(193, 6), OFFSET_PAIR(257, 7), OFFSET_PAIR(385, 7), OFFSET_PAIR(513, 8), OFFSET_PAIR(769, 8),
   OFFSET_PAIR(1025, 9), OFFSET_PAIR(1537, 9), OFFSET_PAIR(2049, 10), OFFSET_PAIR(3073, 10), OFFSET_PAIR(4097, 11), OFFSET_PAIR(6145, 11), OFFSET_PAIR(8193, 12), OFFSET_PAIR(12289, 12), OFFSET_PAIR(16385, 13), OFFSET_PAIR(24577, 13),
};

inline unsigned int CopyStored(BitReader *bit_reader, unsigned char *out, unsigned int out_offset, unsigned int block_size_max) {

	if (bit_reader->ByteAllign() < 0 || bit_reader->in_block + 4 > bit_reader->in_blockend)
		return -1;

	unsigned short stored_length = ((unsigned short)bit_reader->in_block[0]) | (((unsigned short)bit_reader->in_block[0]) << 8);
	bit_reader->ModifyInBlock(2);

	unsigned short neg_stored_length = ((unsigned short)bit_reader->in_block[0]) | (((unsigned short)bit_reader->in_block[1]) << 8);
	bit_reader->ModifyInBlock(2);

	if (stored_length != ((~neg_stored_length) & 0xffff) || stored_length > block_size_max)
		return -1;

	std::memcpy(out + out_offset, bit_reader->in_block, stored_length);
	bit_reader->ModifyInBlock(stored_length);

	return (unsigned int)stored_length;
}

inline unsigned int DecompressBlock(BitReader *bit_reader, int dynamic_block, unsigned char *out, unsigned int out_offset, unsigned int block_size_max) {

	HuffmanDecoder literals_decoder;
	HuffmanDecoder offset_decoder;
	unsigned int literals_rev_sym_table[kLiteralSyms * 2];
	unsigned int offset_rev_sym_table[kLiteralSyms * 2];
	int i;

	if (dynamic_block) {

		HuffmanDecoder tables_decoder;
		unsigned char code_length[kLiteralSyms + kOffsetSyms];
		unsigned int tables_rev_sym_table[kCodeLenSyms * 2];

		unsigned int literal_syms = bit_reader->GetBits(5);
		if (literal_syms == -1) return -1;
		literal_syms += 257;
		if (literal_syms > kLiteralSyms) return -1;

		unsigned int offset_syms = bit_reader->GetBits(5);
		if (offset_syms == -1) return -1;
		offset_syms += 1;
		if (offset_syms > kOffsetSyms) return -1;

		unsigned int code_len_syms = bit_reader->GetBits(4);
		if (code_len_syms == -1) return -1;
		code_len_syms += 4;
		if (code_len_syms > kCodeLenSyms) return -1;

		if (HuffmanDecoder::ReadRawLengths(kCodeLenBits, code_len_syms, kCodeLenSyms, code_length, bit_reader) < 0
		|| tables_decoder.PrepareTable(tables_rev_sym_table, kCodeLenSyms, kCodeLenSyms, code_length) < 0
		|| tables_decoder.FinalizeTable(tables_rev_sym_table) < 0
		|| tables_decoder.ReadLength(tables_rev_sym_table, literal_syms + offset_syms, kLiteralSyms + kOffsetSyms, code_length, bit_reader) < 0
		|| literals_decoder.PrepareTable(literals_rev_sym_table, literal_syms, kLiteralSyms, code_length) < 0
		|| offset_decoder.PrepareTable(offset_rev_sym_table, offset_syms, kOffsetSyms, code_length + literal_syms) < 0)
			return -1;
	}
	else {
		unsigned char fixed_literal_code_len[kLiteralSyms];
		unsigned char fixed_offset_code_len[kOffsetSyms];

		for (i = 0; i < 144; i++)         fixed_literal_code_len[i] = 8;
		for (; i < 256; i++)              fixed_literal_code_len[i] = 9;
		for (; i < 280; i++)              fixed_literal_code_len[i] = 7;
		for (; i < kLiteralSyms; i++)     fixed_literal_code_len[i] = 8;
		for (i = 0; i < kOffsetSyms; i++) fixed_offset_code_len[i]  = 5;

		if (literals_decoder.PrepareTable(literals_rev_sym_table, kLiteralSyms, kLiteralSyms, fixed_literal_code_len) < 0
		|| offset_decoder.PrepareTable(offset_rev_sym_table, kOffsetSyms, kOffsetSyms, fixed_offset_code_len) < 0)
			return -1;
	}

	for (i = 0; i < kOffsetSyms; i++) {
		unsigned int n = offset_rev_sym_table[i];
		if (n < kOffsetSyms) {
			offset_rev_sym_table[i] = kOffsetCode[n];
		}
	}

	for (i = 0; i < kLiteralSyms; i++) {
		unsigned int n = literals_rev_sym_table[i];
		if (n >= kMatchLenSymStart && n < kLiteralSyms) {
			literals_rev_sym_table[i] = kMatchLenCode[n - kMatchLenSymStart];
		}
	}

	if (literals_decoder.FinalizeTable(literals_rev_sym_table) < 0
	|| offset_decoder.FinalizeTable(offset_rev_sym_table) < 0)
		return -1;

	unsigned char *current_out = out + out_offset;
	const unsigned char *out_end = current_out + block_size_max;
	const unsigned char *out_fast_end = out_end - 15;

	while (1)
	{
		bit_reader->Refill32();

		unsigned int literals_code_word = literals_decoder.ReadValue(literals_rev_sym_table, bit_reader);
		if (literals_code_word < 256) {

			if (current_out < out_end)
				*current_out++ = literals_code_word;
			else
				return -1;
		}
		else {
			if (literals_code_word == kEODMarkerSym) break;
			if (literals_code_word == -1) return -1;

			unsigned int match_length = bit_reader->GetBits((literals_code_word >> 16) & 15);
			if (match_length == -1) return -1;

			match_length += (literals_code_word & 0x7fff);

			unsigned int offset_code_word = offset_decoder.ReadValue(offset_rev_sym_table, bit_reader);
			if (offset_code_word == -1) return -1;

			unsigned int match_offset = bit_reader->GetBits((offset_code_word >> 16) & 15);
			if (match_offset == -1) return -1;

			match_offset += (offset_code_word & 0x7fff);

			const unsigned char *src = current_out - match_offset;
			if (src >= out) {
				if (match_offset >= 16 && (current_out + match_length) <= out_fast_end) {
					const unsigned char *copy_src = src;
					unsigned char *copy_dst = current_out;
					const unsigned char *copy_end_dst = current_out + match_length;

					do {
						std::memcpy(copy_dst, copy_src, 16);
						copy_src += 16;
						copy_dst += 16;
					}
					while (copy_dst < copy_end_dst);

					current_out += match_length;
				}
				else {

					if ((current_out + match_length) > out_end) return -1;

					while
						(match_length--) {
						*current_out++ = *src++;
					}
				}
			}
			else return -1;
		}
	}

	return (unsigned int)(current_out - (out + out_offset));
}

/**
 * Inflate zlib data
 *
 * @param compressed_data pointer to start of zlib data
 * @param compressed_data_size size of zlib data, in bytes
 * @param out pointer to start of decompression buffer
 * @param out_size_max maximum size of decompression buffer, in bytes
 * @param checksum defines if the decompressor should use a specific checksum
 *
 * @return number of bytes decompressed, or -1 in case of an error
 */
inline unsigned int Decompressor_Feed(const void *compressed_data, unsigned int compressed_data_size, unsigned char *out, unsigned int out_size_max, bool checksum) {

	unsigned char *current_compressed_data = (unsigned char *)compressed_data;
	unsigned char *end_compressed_data = current_compressed_data + compressed_data_size;
	unsigned int final_block;
	unsigned int current_out_offset;
	unsigned long check_sum = 0;

	BitReader bit_reader;

	if ((current_compressed_data + 2) > end_compressed_data) return -1;

	unsigned char CMF = current_compressed_data[0];
	unsigned char FLG = current_compressed_data[1];
	unsigned short check = FLG | (((unsigned short)CMF) << 8);

	if ((CMF >> 4) <= 7 && (check % 31) == 0) {
		current_compressed_data += 2;
		if (FLG & 0x20) {
			if ((current_compressed_data + 4) > end_compressed_data) return -1;
			current_compressed_data += 4;
		}
	}

	if (checksum) check_sum = adler32_z(0, nullptr, 0);

	bit_reader.Init(current_compressed_data, end_compressed_data);
	current_out_offset = 0;

	do {
		unsigned int block_type;
		unsigned int block_result;

		final_block = bit_reader.GetBits(1);
		block_type = bit_reader.GetBits(2);

		switch (block_type) {
		case 0:
			block_result = CopyStored(&bit_reader, out, current_out_offset, out_size_max - current_out_offset);
			break;

		case 1:
			block_result = DecompressBlock(&bit_reader, 0, out, current_out_offset, out_size_max - current_out_offset);
			break;

		case 2:
			block_result = DecompressBlock(&bit_reader, 1, out, current_out_offset, out_size_max - current_out_offset);
			break;

		case 3:
			return -1;
		}

		if (block_result == -1) return -1;

		if (checksum) {
			check_sum = adler32_z(check_sum, out + current_out_offset, block_result);
		}

		current_out_offset += block_result;
	}
	while (!final_block);

	bit_reader.ByteAllign();
	current_compressed_data = bit_reader.in_block;

	if (checksum) {
		unsigned int stored_check_sum;

		if ((current_compressed_data + 4) > end_compressed_data) return -1;

		stored_check_sum  = ((unsigned int)current_compressed_data[0]) << 24;
		stored_check_sum |= ((unsigned int)current_compressed_data[1]) << 16;
		stored_check_sum |= ((unsigned int)current_compressed_data[2]) << 8;
		stored_check_sum |= ((unsigned int)current_compressed_data[3]);

		if (stored_check_sum != check_sum) return -1;

		current_compressed_data += 4;
	}

	return current_out_offset;
}
