#pragma once
#ifndef RKDETECTORLABELTOOLS_H
#define RKDETECTORLABELTOOLS_H

#define MAX_LABEL_LEN 128

namespace rkdet {

class LabelTools
{
public:
    void init(const char* label_path);
    ~LabelTools();
    void  release();
    char* get_name(int cls_id) const;

private:
    // label
    char* labels_[MAX_LABEL_LEN]{};
    int   label_count_{};
};

} // namespace rkdet

#endif //RKDETECTORLABELTOOLS_H
