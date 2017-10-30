#include "Util.h"

#if !_MSC_VER >= 1911
int stoi(std::string s)
{
	return atoi(s.c_str());
}
#endif