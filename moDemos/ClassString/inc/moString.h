#ifndef __SIMS_STRING_H__
#define __SIMS_STRING_H__

#include <stdio.h>

#define debug(format, ...) printf("[%s, %d--debug] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define error(format, ...) printf("[%s, %d--error] : "format"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

class MoString
{
public:
    MoString();
    MoString(const char *ptr);
    MoString(const int size, const char value);
    MoString(const MoString & other);

    virtual ~MoString();
    
public:
    MoString & operator=(const MoString & other);
    MoString & operator+(MoString & other);
    MoString & operator+(const char * str);

public:
    /*
        return the string in C format;
    */
    const char * c_str();

    /*
        Get the length of this string.
    */
    unsigned int length() const;

    /*
        Find the subString in this string or not
        If exist, return its first appear position;
        Or, return 0-;
    */
    int subString(MoString & subStr);

private:
    /*
        Use Sunday algorithm, to find a pattern from src string.
    */
    int sundaySearch(const char * pSrc, const unsigned int srcLen, 
        const char * pPattern, const unsigned int patternLen, 
        const char * pNext);

    /*
        To Sunday algorithm, a next table is needed, this function generate it.
    */
    int sundayGenNext(char *pNext, const char * pPattern, const unsigned int patternLen);

    
private:
    char *mPtr;
};

#endif
