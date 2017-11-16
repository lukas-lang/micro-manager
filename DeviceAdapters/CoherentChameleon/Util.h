///////////////////////////////////////////////////////////////////////////////
// FILE:          Util.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   A device adapter for the Coherent Chameleon Ultra laser
//
// AUTHOR:        Lukas Lang
//
// COPYRIGHT:     2017 Lukas Lang
// LICENSE:       Licensed under the Apache License, Version 2.0 (the "License");
//                you may not use this file except in compliance with the License.
//                You may obtain a copy of the License at
//                
//                http://www.apache.org/licenses/LICENSE-2.0
//                
//                Unless required by applicable law or agreed to in writing, software
//                distributed under the License is distributed on an "AS IS" BASIS,
//                WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//                See the License for the specific language governing permissions and
//                limitations under the License.

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>

template <typename T>
std::string to_string(const T& expr)
{
	std::ostringstream os;
	os << expr;
	return os.str();
}

template <typename T>
T from_string(const std::string& str)
{
	std::istringstream is(str);
	T ret;
	is >> ret;
	return ret;
}

template <typename T>
struct fixtype
{
	typedef T type;
};

template <size_t n>
struct fixtype<char[n]>
{
	typedef std::string type;
};

template <size_t n>
struct fixtype<const char[n]>
{
	typedef std::string type;
};

template <>
inline std::string to_string<std::string>(const std::string& expr)
{
	return expr;
}

template <>
inline std::string from_string<std::string>(const std::string& str)
{
	return str;
}

template <typename T>
struct vector_of_ : public std::vector<T>
{
	vector_of_() {}

	template <typename U>
	vector_of_(const U& u)
	{
		(*this)(u);
	}

	vector_of_& operator()(const T& t)
	{
		this->push_back(t);
		return *this;
	}
};

template <typename T>
vector_of_<typename fixtype<T>::type> vector_of(const T& t)
{
	return t;
}

template <typename K, typename V>
struct map_of_ : public std::map<K, V>
{
	map_of_<K, V>& operator()(const K& k, const V& v)
	{
		(*this)[k] = v;
		return *this;
	}
};

template <typename K, typename V>
map_of_<typename fixtype<K>::type, typename fixtype<V>::type> map_of(const K& k, const V& v)
{
	return map_of_<typename fixtype<K>::type, typename fixtype<V>::type>()(k, v);
}

#if !_MSC_VER >= 1911
int stoi(std::string s);
#endif
