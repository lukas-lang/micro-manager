#include "errorCode.h"

errorCode::errorCode(int code, std::string msg) : code(code), msg(msg) {}

void errorCode::ThrowErr(int code, MM::Device* source) throw (errorCode)
{
	if (code != DEVICE_OK)
		if (source)
		{
			char msg[MM::MaxStrLength];
			source->GetErrorText(code, msg);
			throw errorCode(code, msg);
		}
		else
			throw errorCode(code);
}