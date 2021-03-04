// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace leveldb {

class Arena {
 public:
  Arena();

  // 禁用拷贝构造函数和赋值运算符 ==> 单例
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  ~Arena();

  // Return a pointer to a newly allocated memory block of "bytes" bytes.
  char* Allocate(size_t bytes);

  // Allocate memory with the normal alignment guarantees provided by malloc.
  char* AllocateAligned(size_t bytes);

  // Returns an estimate of the total memory usage of data allocated
  // by the arena.
  size_t MemoryUsage() const {
    return memory_usage_.load(std::memory_order_relaxed);
  }

 private:
  char* AllocateFallback(size_t bytes);        // 分配失败处理
  char* AllocateNewBlock(size_t block_bytes);  // 分配新的block

  // Allocation state(当前block的分配状态)
  char* alloc_ptr_;               // 指向当前block未分配空间首字节
  size_t alloc_bytes_remaining_;  // 当前block剩余未分配字节数

  // Array of new[] allocated memory blocks
  std::vector<char*> blocks_;     // 所有已申请(且都分配了部分)的block

  // Total memory usage of the arena.
  //
  // TODO(costan): This member is accessed via atomics, but the others are
  //               accessed without any locking. Is this OK?
  std::atomic<size_t> memory_usage_;
};

inline char* Arena::Allocate(size_t bytes) {
  // The semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  assert(bytes > 0);
  // 如果当前block还有剩余空间, 直接从当前block分配
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  // 转分配失败处理
  return AllocateFallback(bytes);
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
