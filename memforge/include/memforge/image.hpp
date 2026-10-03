// Physical memory image access: raw dumps and LiME captures, memory-mapped.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mf {

enum class ImageFormat { Auto, Raw, LiME };

const char* format_name(ImageFormat f);

// A contiguous run of physical memory backed by bytes in the image file.
struct MemRange {
  uint64_t phys_start = 0;
  uint64_t size = 0;
  uint64_t file_offset = 0;
  uint64_t phys_end() const { return phys_start + size; }
};

// Read-only view of a physical memory capture. The file is mmap()ed once, so
// any number of threads can read it concurrently without locking or copying.
class MemoryImage {
 public:
  static std::unique_ptr<MemoryImage> open(const std::string& path,
                                           ImageFormat fmt = ImageFormat::Auto);
  // In-memory image (used by tests and the synthetic generator). When ranges
  // is empty the whole buffer is one range starting at physical address 0.
  static std::unique_ptr<MemoryImage> from_buffer(std::vector<uint8_t> buf,
                                                  std::vector<MemRange> ranges = {},
                                                  std::string name = "<memory>");
  ~MemoryImage();
  MemoryImage(const MemoryImage&) = delete;
  MemoryImage& operator=(const MemoryImage&) = delete;

  const std::string& path() const { return path_; }
  ImageFormat format() const { return fmt_; }
  const std::vector<MemRange>& ranges() const { return ranges_; }
  uint64_t file_size() const { return map_len_; }
  uint64_t total_bytes() const;  // bytes of physical memory present
  uint64_t max_phys() const;     // one past the highest backed physical address

  // Pointer to [paddr, paddr+len) when it lies inside a single range, else nullptr.
  const uint8_t* ptr(uint64_t paddr, uint64_t len) const;
  const uint8_t* range_data(const MemRange& r) const { return base_ + r.file_offset; }
  // Copies physical memory into dst, zero-filling holes. Returns bytes backed.
  uint64_t read(uint64_t paddr, void* dst, uint64_t len) const;
  bool read_u64(uint64_t paddr, uint64_t& out) const;

  // Hint the kernel that the image will be streamed (MADV_SEQUENTIAL).
  void advise_sequential() const;

 private:
  MemoryImage() = default;
  void parse_lime();

  std::string path_;
  ImageFormat fmt_ = ImageFormat::Raw;
  const uint8_t* base_ = nullptr;
  uint64_t map_len_ = 0;
  bool mapped_ = false;
  std::vector<uint8_t> owned_;
  std::vector<MemRange> ranges_;
};

}  // namespace mf
