#ifndef COM_FORMAT_H
#define COM_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/**
 * @brief 无 FILE/无堆的有界整数格式化。
 * @return 本应输出的字符数（不含 NUL）；参数或占位符不支持时返回 -1。
 * @note 支持 %%/%c/%s/%d/%i/%u/%x/%X、单个 l、十进制宽度和 0 填充。
 *       语义与 snprintf 一致：capacity > 0 时始终 NUL 结尾，空间不足则截断。
 */
int Com_FormatV(char *buffer, size_t capacity, const char *format, va_list arguments);
int Com_Format(char *buffer, size_t capacity, const char *format, ...);

#endif /* COM_FORMAT_H */
