#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stddef.h>

/**
 * 读取整个文件到 malloc 分配的字符串（以 '\0' 结尾）。
 * 成功返回缓冲区（调用者负责 free），失败返回 NULL。
 * out_len 非空时写入实际读取的字节数（不含结尾 '\0'）。
 */
char *file_read_all(const char *path, size_t *out_len);

#endif /* FILE_UTILS_H */
