#pragma once

#include <string>
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

template <typename T>
struct vector_of_ : public std::vector<T>
{
	vector_of_<T>& operator()(const T& t)
	{
		this->push_back(t);
		return *this;
	}
}; 

template <typename T>
vector_of_<typename fixtype<T>::type> vector_of(const T& t)
{
	return vector_of_<typename fixtype<T>::type>()(t);
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
