#include "LabelTools.h"

#include "log.h"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Label I/O helpers
// ---------------------------------------------------------------------------

static char* readLine(FILE* fp, char* buffer, int* len)
{
    int    ch;
    int    i        = 0;
    size_t buff_len = 0;

    buffer          = (char*)malloc(buff_len + 1);
    if (!buffer)
    {
        return NULL;
    }

    while ((ch = fgetc(fp)) != '\n' && ch != EOF)
    {
        buff_len++;
        void* tmp = realloc(buffer, buff_len + 1);
        if (tmp == NULL)
        {
            free(buffer);
            return NULL;
        }
        buffer    = (char*)tmp;

        buffer[i] = (char)ch;
        i++;
    }
    buffer[i] = '\0';

    *len      = buff_len;

    if (ch == EOF && (i == 0 || ferror(fp)))
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static int readLines(const char* fileName, char* lines[], int max_line)
{
    FILE* file = fopen(fileName, "r");
    char* s;
    int   i = 0;
    int   n = 0;

    if (file == NULL)
    {
        LOG_ERROR("Open %s fail!", fileName);
        return -1;
    }

    while ((s = readLine(file, s, &n)) != NULL)
    {
        lines[i++] = s;
        if (i >= max_line)
        {
            break;
        }
    }
    fclose(file);
    return i;
}

LabelTools::LabelTools(const char* label_path)
{
    label_count_ = readLines(label_path, labels_, MAX_LABEL_LEN);
    if (label_count_ < 0)
    {
        LOG_ERROR("Load %s failed!", label_path);
    }
}

LabelTools::~LabelTools()
{
    release();
}

void LabelTools::release()
{
    for (int i = 0; i < label_count_; i++)
    {
        if (labels_[i] != nullptr)
        {
            free(labels_[i]);
            labels_[i] = nullptr;
        }
    }
    label_count_ = 0;
}

// ---------------------------------------------------------------------------
// Label management
// ---------------------------------------------------------------------------

char* LabelTools::get_name(int cls_id) const
{
    if (cls_id >= label_count_ || cls_id < 0)
    {
        return (char*)"null";
    }
    if (labels_[cls_id])
    {
        return labels_[cls_id];
    }
    return (char*)"null";
}