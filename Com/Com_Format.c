#include "Com_Format.h"

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

typedef struct
{
    char *buffer;
    size_t capacity;
    size_t length;
    bool failed;
} Com_FormatWriterTypeDef;

enum
{
    COM_FORMAT_MAX_FIELD_WIDTH = 384u,
    COM_FORMAT_MAX_STRING_LENGTH = 384u
};

static void Com_Format_PutChar(Com_FormatWriterTypeDef *writer, char value)
{
    if (writer->length == SIZE_MAX)
    {
        writer->failed = true;
        return;
    }
    if ((writer->capacity > 0u) && (writer->length < (writer->capacity - 1u)))
    {
        writer->buffer[writer->length] = value;
    }
    writer->length++;
}

static void Com_Format_PutPadding(Com_FormatWriterTypeDef *writer, char value, uint32_t count)
{
    while (count > 0u)
    {
        Com_Format_PutChar(writer, value);
        count--;
    }
}

static void Com_Format_PutUnsigned(Com_FormatWriterTypeDef *writer,
                                   uint32_t value,
                                   uint8_t base,
                                   bool uppercase,
                                   bool negative,
                                   uint32_t width,
                                   bool zero_padding)
{
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *digits_table = uppercase ? upper_digits : lower_digits;
    char digits[32];
    uint32_t digit_count = 0u;
    uint32_t content_width;
    uint32_t padding;

    do
    {
        digits[digit_count] = digits_table[value % base];
        digit_count++;
        value /= base;
    } while ((value != 0u) && (digit_count < sizeof(digits)));

    content_width = digit_count + (negative ? 1u : 0u);
    padding = (width > content_width) ? (width - content_width) : 0u;
    if (!zero_padding)
    {
        Com_Format_PutPadding(writer, ' ', padding);
    }
    if (negative)
    {
        Com_Format_PutChar(writer, '-');
    }
    if (zero_padding)
    {
        Com_Format_PutPadding(writer, '0', padding);
    }
    while (digit_count > 0u)
    {
        digit_count--;
        Com_Format_PutChar(writer, digits[digit_count]);
    }
}

static void Com_Format_PutString(Com_FormatWriterTypeDef *writer, const char *text, uint32_t width)
{
    const char *safe_text = (text != NULL) ? text : "(null)";
    uint32_t text_length = 0u;

    while ((safe_text[text_length] != '\0') && (text_length < COM_FORMAT_MAX_STRING_LENGTH))
    {
        text_length++;
    }
    if (safe_text[text_length] != '\0')
    {
        writer->failed = true;
        return;
    }
    if (width > text_length)
    {
        Com_Format_PutPadding(writer, ' ', width - text_length);
    }
    for (uint32_t index = 0u; index < text_length; index++)
    {
        Com_Format_PutChar(writer, safe_text[index]);
    }
}

static uint32_t Com_Format_ReadWidth(const char **cursor, bool *valid)
{
    uint32_t width = 0u;

    while ((**cursor >= '0') && (**cursor <= '9'))
    {
        uint32_t digit = (uint32_t)(**cursor - '0');

        if (width > ((UINT32_MAX - digit) / 10u))
        {
            *valid = false;
            return 0u;
        }
        width = (width * 10u) + digit;
        (*cursor)++;
    }
    return width;
}

int Com_FormatV(char *buffer, size_t capacity, const char *format, va_list arguments)
{
    Com_FormatWriterTypeDef writer = {
        .buffer = buffer,
        .capacity = capacity,
        .length = 0u,
        .failed = false,
    };
    const char *cursor = format;

    if (((buffer == NULL) && (capacity > 0u)) || (format == NULL))
    {
        return -1;
    }

    while ((*cursor != '\0') && !writer.failed)
    {
        bool zero_padding = false;
        bool long_argument = false;
        bool valid = true;
        uint32_t width;
        char specifier;

        if (*cursor != '%')
        {
            Com_Format_PutChar(&writer, *cursor);
            cursor++;
            continue;
        }
        cursor++;
        if (*cursor == '%')
        {
            Com_Format_PutChar(&writer, '%');
            cursor++;
            continue;
        }
        if (*cursor == '0')
        {
            zero_padding = true;
            cursor++;
        }
        width = Com_Format_ReadWidth(&cursor, &valid);
        if (width > COM_FORMAT_MAX_FIELD_WIDTH)
        {
            valid = false;
        }
        if (*cursor == 'l')
        {
            long_argument = true;
            cursor++;
            if (*cursor == 'l')
            {
                valid = false;
            }
        }
        specifier = *cursor;
        if ((specifier == '\0') || !valid)
        {
            writer.failed = true;
            break;
        }
        cursor++;

        switch (specifier)
        {
            case 'd':
            case 'i':
            {
                int32_t signed_value = long_argument ? (int32_t)va_arg(arguments, long)
                                                     : (int32_t)va_arg(arguments, int);
                bool negative = signed_value < 0;
                uint32_t magnitude =
                    negative ? (uint32_t)(-(signed_value + 1)) + 1u : (uint32_t)signed_value;

                Com_Format_PutUnsigned(
                    &writer, magnitude, 10u, false, negative, width, zero_padding);
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            {
                uint32_t value = long_argument ? (uint32_t)va_arg(arguments, unsigned long)
                                               : (uint32_t)va_arg(arguments, unsigned int);
                uint8_t base = (specifier == 'u') ? 10u : 16u;

                Com_Format_PutUnsigned(
                    &writer, value, base, specifier == 'X', false, width, zero_padding);
                break;
            }
            case 'c':
                if (long_argument)
                {
                    writer.failed = true;
                }
                else
                {
                    Com_Format_PutChar(&writer, (char)va_arg(arguments, int));
                }
                break;
            case 's':
                if (long_argument || zero_padding)
                {
                    writer.failed = true;
                }
                else
                {
                    Com_Format_PutString(&writer, va_arg(arguments, const char *), width);
                }
                break;
            default:
                writer.failed = true;
                break;
        }
    }

    if (capacity > 0u)
    {
        size_t terminator = (writer.length < capacity) ? writer.length : (capacity - 1u);

        buffer[terminator] = '\0';
    }
    if (writer.failed || (writer.length > (size_t)INT_MAX))
    {
        return -1;
    }
    return (int)writer.length;
}

int Com_Format(char *buffer, size_t capacity, const char *format, ...)
{
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = Com_FormatV(buffer, capacity, format, arguments);
    va_end(arguments);
    return length;
}
