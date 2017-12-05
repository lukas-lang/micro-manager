#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "boost/numeric/ublas/matrix.hpp"
#include "boost/numeric/ublas/lu.hpp"
#include "boost/numeric/ublas/io.hpp"

#include "errorCode.h"

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

	if (!is)
		throw errorCode(INVALID_INPUT);

	return ret;
}

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
	typedef T type;

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

using namespace boost::numeric::ublas;
using namespace std;

/* Matrix inversion routine.
Uses lu_factorize and lu_substitute in uBLAS to invert a matrix */
template<class T>
bool invertMatrix(const matrix<T>& input, matrix<T>& inverse)
{
	typedef permutation_matrix<std::size_t> pmatrix;

	// create a working copy of the input
	matrix<T> A(input);

	// create a permutation matrix for the LU-factorization
	pmatrix pm(A.size1());

	// perform LU-factorization
	int res = lu_factorize(A, pm);
	if (res != 0)
		return false;

	// create identity matrix of "inverse"
	inverse.assign(identity_matrix<T>(A.size1()));

	// backsubstitute to get the inverse
	lu_substitute(A, pm, inverse);

	return true;
}
