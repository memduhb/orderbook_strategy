#pragma once
#include <cstdint>

namespace endian 
{
	/**
     * Reads a 16-bit big-endian value from memory
     * @param p Pointer to the byte array (must have at least 2 bytes)
     * @return The 16-bit value in host byte order
     */
	inline uint16_t read_u16_be(const char* p) noexcept
	{    
		return (static_cast<uint8_t>(p[0]) << 8) | 
		(static_cast<uint8_t>(p[1]));
	}

	/**
     * Reads a 32-bit big-endian value from memory
     * @param p Pointer to the byte array (must have at least 4 bytes)
     * @return The 32-bit value in host byte order
     */
	inline uint32_t read_u32_be(const char* p) noexcept
	{
		return 	(static_cast<uint8_t>(p[0]) << 24) |
				(static_cast<uint8_t>(p[1]) << 16) | 
				(static_cast<uint8_t>(p[2]) << 8) |  
				(static_cast<uint8_t>(p[3]));
	}

	/**
     * Reads a 64-bit big-endian value from memory
     * @param p Pointer to the byte array (must have at least 8 bytes)
     * @return The 64-bit value in host byte order
     */
	inline uint64_t read_u64_be(const char*p) noexcept
	{
		return (static_cast<uint64_t>(static_cast<uint8_t>(p[0])) << 56) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[1])) << 48) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[2])) << 40) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[3])) << 32) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[4])) << 24) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[5])) << 16) |
			   (static_cast<uint64_t>(static_cast<uint8_t>(p[6])) << 8) |
			   static_cast<uint64_t>(static_cast<uint8_t>(p[7]));
	}
}

