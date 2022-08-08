#pragma once

#include <cstdint>
#include <string>

namespace utils{
	
// ------------------ Операции шифрования и кодирования ------------------
uint32_t crc32_wiki_inv(uint32_t initial, const uint8_t *block, uint64_t size);
uint32_t crc32_wiki_inv(uint32_t initial, const char *block, uint64_t size);

std::unique_ptr<uint8_t[]> base64_encode(const uint8_t *src, size_t len, size_t *out_len);
std::unique_ptr<uint8_t[]> base64_encode(const char *src, size_t len, size_t *out_len);
std::string base64_encode(const uint8_t *src, size_t len);
std::string base64_encode(const char *src, size_t len);

std::unique_ptr<uint8_t[]> base64_decode(const uint8_t *src, size_t len, size_t *out_len);
std::unique_ptr<uint8_t[]> base64_decode(const char *src, size_t len, size_t *out_len);

void file_base64_encode(const std::string &in_fname, const std::string &out_fname);

std::string file_md5(const std::string &file_path);
std::string md5sum(const char *in, size_t len);


} // namespace utils