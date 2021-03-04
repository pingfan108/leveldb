// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"

namespace leveldb {

static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
  // 把所有申请的block释放
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    // 用户需要的内存足够大, 直接按需申请分配即可
    // Object is more than a quarter of our block size.  Allocate it separately
    // to avoid wasting too much space in leftover bytes.
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // 用户需要的内存比较小, 容易产生碎片; 统一按4k申请, 按需切割后分配
  // We waste the remaining space in the current block.
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* Arena::AllocateAligned(size_t bytes) {
  // 检查机器的位数, 高于64位则以机器位数折算的字节数对齐; 反之, 最低以8字节对齐
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  static_assert((align & (align - 1)) == 0,
                "Pointer size should be a power of 2");
  // a&(n-1), 等价于a%n, 取得当前地址超出对齐部分的字节数
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  // 计算要令当前地址对齐所需填充的字节数
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  // 实际所需的空间需要在用户所申请大小的基础上增加填充的字节数
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // AllocateFallback always returned aligned memory
    result = AllocateFallback(bytes);
  }
  // 验证最终分配的地址是按align字节数对齐的
  assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
  return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_.push_back(result);
  memory_usage_.fetch_add(block_bytes + sizeof(char*),
                          std::memory_order_relaxed);
  return result;
}

}  // namespace leveldb
