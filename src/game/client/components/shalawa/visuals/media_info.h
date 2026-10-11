#ifndef SHALAWA_MEDIA_INFO_H
#define SHALAWA_MEDIA_INFO_H

struct SMediaInfo
{
    char title[256];
    char artist[256];
    bool playing;
    bool valid;
};

bool MediaInfo_Query(SMediaInfo *info);

#endif