#include "memforge/image.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace mf {

namespace {
constexpr uint32_t kLimeMagic = 0x4C694D45;  // "EMiL" little-endian
constexpr uint64_t kLimeHeaderSize = 32;

template <typename T>
T load(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof v);
  return v;
}
}  // namespace

const char* format_name(ImageFormat f) {
  switch (f) {
    case ImageFormat::Raw: return "raw";
    case ImageFormat::LiME: return "lime";
    default: return "auto";
  }
}

std::unique_ptr<MemoryImage> MemoryImage::open(const std::string& path, ImageFormat fmt) {
  int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) throw std::system_error(errno, std::generic_category(), "open " + path);
  struct stat st {};
  if (::fstat(fd, &st) != 0) {
    int e = errno;
    ::close(fd);
    throw std::system_error(e, std::generic_category(), "stat " + path);
  }
  std::unique_ptr<MemoryImage> img(new MemoryImage());
  img->path_ = path;
  img->map_len_ = static_cast<uint64_t>(st.st_size);
  if (img->map_len_ > 0) {
    void* p = ::mmap(nullptr, img->map_len_, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
      int e = errno;
      ::close(fd);
      throw std::system_error(e, std::generic_category(), "mmap " + path);
    }
    img->base_ = static_cast<const uint8_t*>(p);
    img->mapped_ = true;
  }
  ::close(fd);  // the mapping keeps the file alive; don't hold fds per image

  if (fmt == ImageFormat::Auto) {
    fmt = (img->map_len_ >= kLimeHeaderSize && load<uint32_t>(img->base_) == kLimeMagic)
              ? ImageFormat::LiME
              : ImageFormat::Raw;
  }
  img->fmt_ = fmt;
  if (fmt == ImageFormat::LiME) {
    img->parse_lime();
  } else if (img->map_len_ > 0) {
    img->ranges_.push_back({0, img->map_len_, 0});
  }
  return img;
}

std::unique_ptr<MemoryImage> MemoryImage::from_buffer(std::vector<uint8_t> buf,
                                                      std::vector<MemRange> ranges,
                                                      std::string name) {
  std::unique_ptr<MemoryImage> img(new MemoryImage());
  img->owned_ = std::move(buf);
  img->base_ = img->owned_.data();
  img->map_len_ = img->owned_.size();
  img->path_ = std::move(name);
  if (ranges.empty()) {
    if (img->map_len_ > 0) ranges.push_back({0, img->map_len_, 0});
  } else {
    for (const auto& r : ranges)
      if (r.file_offset + r.size > img->map_len_)
        throw std::invalid_argument("range exceeds buffer");
    std::sort(ranges.begin(), ranges.end(),
              [](const MemRange& a, const MemRange& b) { return a.phys_start < b.phys_start; });
  }
  img->ranges_ = std::move(ranges);
  return img;
}

MemoryImage::~MemoryImage() {
  if (mapped_ && base_) ::munmap(const_cast<uint8_t*>(base_), map_len_);
}

void MemoryImage::parse_lime() {
  uint64_t off = 0;
  while (off < map_len_) {
    if (map_len_ - off < kLimeHeaderSize)
      throw std::runtime_error(path_ + ": truncated LiME header at file offset " + std::to_string(off));
    const uint8_t* h = base_ + off;
    if (load<uint32_t>(h) != kLimeMagic)
      throw std::runtime_error(path_ + ": bad LiME magic at file offset " + std::to_string(off));
    uint64_t s = load<uint64_t>(h + 8);
    uint64_t e = load<uint64_t>(h + 16);
    if (e < s) throw std::runtime_error(path_ + ": LiME range end before start");
    uint64_t len = e - s + 1;  // LiME end address is inclusive
    uint64_t data_off = off + kLimeHeaderSize;
    if (len > map_len_ - data_off)
      throw std::runtime_error(path_ + ": LiME range runs past end of file");
    ranges_.push_back({s, len, data_off});
    off = data_off + len;
  }
  std::sort(ranges_.begin(), ranges_.end(),
            [](const MemRange& a, const MemRange& b) { return a.phys_start < b.phys_start; });
  for (size_t i = 1; i < ranges_.size(); ++i)
    if (ranges_[i].phys_start < ranges_[i - 1].phys_end())
      throw std::runtime_error(path_ + ": overlapping LiME ranges");
}

uint64_t MemoryImage::total_bytes() const {
  uint64_t t = 0;
  for (const auto& r : ranges_) t += r.size;
  return t;
}

uint64_t MemoryImage::max_phys() const { return ranges_.empty() ? 0 : ranges_.back().phys_end(); }

const uint8_t* MemoryImage::ptr(uint64_t paddr, uint64_t len) const {
  auto it = std::upper_bound(ranges_.begin(), ranges_.end(), paddr,
                             [](uint64_t a, const MemRange& r) { return a < r.phys_start; });
  if (it == ranges_.begin()) return nullptr;
  const MemRange& r = *(it - 1);
  uint64_t rel = paddr - r.phys_start;
  if (rel >= r.size || len > r.size - rel) return nullptr;
  return base_ + r.file_offset + rel;
}

uint64_t MemoryImage::read(uint64_t paddr, void* dst, uint64_t len) const {
  auto* out = static_cast<uint8_t*>(dst);
  std::memset(out, 0, len);
  uint64_t backed = 0;
  for (const auto& r : ranges_) {
    if (r.phys_end() <= paddr || r.phys_start >= paddr + len) continue;
    uint64_t lo = std::max(paddr, r.phys_start);
    uint64_t hi = std::min(paddr + len, r.phys_end());
    std::memcpy(out + (lo - paddr), base_ + r.file_offset + (lo - r.phys_start), hi - lo);
    backed += hi - lo;
  }
  return backed;
}

bool MemoryImage::read_u64(uint64_t paddr, uint64_t& out) const {
  const uint8_t* p = ptr(paddr, 8);
  if (!p) return false;
  out = load<uint64_t>(p);
  return true;
}

void MemoryImage::advise_sequential() const {
  if (mapped_ && base_) ::madvise(const_cast<uint8_t*>(base_), map_len_, MADV_SEQUENTIAL);
}

}  // namespace mf
