#pragma once

#include <string>
#include <algorithm>

#include "Util.h"
#include "error_code.h"

/*
Reference wrapper
*/
template <typename T>
class Ref
{
public:
	Ref(T& var) : var(var) {}

	T& operator()() const
	{
		return var;
	}

private:
	T& var;
};

/*
Constructs a reference wrapper Ref<T>
*/
template <typename T>
Ref<T> ref(T& var)
{
	return Ref<T>(var);
}

//Helper union for OptRef. Contains either a value or a pointer to a value
template <typename T>
union OptRef_
{
	OptRef_(T val) : val(val) {}
	OptRef_(T* ref) : ref(ref) {}

	T val;
	T* ref;
};

//Wrapper for a value that might be a reference. If it is a reference, the result of operator() is backed by that reference. Otherwise, the value is stored inside the object
template <typename T>
class OptRef
{
public:
	OptRef(T val) : optRef(val), ref(false) {}
	OptRef(Ref<T> var) : optRef(&var()), ref(true) {}

	T& operator()()
	{
		return ref ? *optRef.ref : optRef.val;
	}

private:
	OptRef_<T> optRef;
	bool ref;
};

/*
Base class for devices that provides property mapping utilities to define device properties of various types
Inherits from T, used to linearize class hierarchy. Should be subclass of CDeviceBase
U should be the deriving class
*/
template <typename T, typename U>
class DeviceUtil : public T
{
public:
	/*
	General property accessor to query/set a value
	*/
	struct PropertyAccessor
	{
		virtual std::string QueryParameter(U*);
		virtual void SetParameter(U*, std::string);
	};

	/*
	PropertyAccessor implmentation that uses stored token to query/set parameter via the supplied NamedPropertyAccessor
	*/
	class DefPropertyAccessor : public PropertyAccessor
	{
	public:
		DefPropertyAccessor(std::string);

		std::string QueryParameter(U*);
		void SetParameter(U*, std::string);

	private:
		std::string token;
	};

	/*
	Same as DefPropertyAccessor, but scales the values before setting/after getting them
	The factor can be either a reference to a variable (Ref wrapper) or a value
	*/
	class ScalingPropertyAccessor : public PropertyAccessor
	{
	public:
		ScalingPropertyAccessor(std::string, OptRef<double> = 1);

		std::string QueryParameter(U*);
		void SetParameter(U*, std::string);

	private:
		std::string token;
		OptRef<double> scalingFactor;
	};

	/*
	PropertyAccessor implementation that forwards query/set to stored functors
	Does not set anything if the supplied setter is nullptr
	*/
	class CustomPropertyAccessor : public PropertyAccessor
	{
	public:
		CustomPropertyAccessor(std::string(U::* get)(), void(U::* set)(std::string) = nullptr);

		std::string QueryParameter(U*);
		void SetParameter(U*, std::string);

	private:
		std::string(U::* get)();
		void(U::* set)(std::string);
	};

	/*
	PropertyAccessor implementation that is backed by a variable
	*/
	template <typename V>
	class VariableAccessor : public PropertyAccessor
	{
	public:
		VariableAccessor(V&);

		std::string QueryParameter(U*);
		void SetParameter(U*, std::string);

	private:
		V& var;
	};

	virtual std::string QueryParameter(std::string token) = 0;
	virtual void SetParameter(std::string token, std::string value) = 0;

	/*
	Helper class that constructs and stores the appropriate implementation of PropertyAccessor based on supplied arguments
	*/
	struct PropertyAccessorWrapper
	{
		PropertyAccessorWrapper(PropertyAccessor*);
		PropertyAccessorWrapper(std::string);
		PropertyAccessorWrapper(const char* token);
		template <typename V>
		PropertyAccessorWrapper(Ref<V> var);

		PropertyAccessor* propAcc;
	};

	virtual ~DeviceUtil();
	/*
	Define an integer property with bounds lower and upper. No bounds are enforced if lower == upper
	Returns the id of the defined property
	*/
	long MapNumProperty(PropertyAccessorWrapper propAcc, std::string description, double lower, double upper, MM::PropertyType propType = MM::Integer);

	/*
	Define a property of specified type and description
	Returns the id of the created property, to be used with SetPropertyNames
	*/
	long MapProperty(PropertyAccessorWrapper propAcc, std::string description, bool readOnly = false, MM::PropertyType propType = MM::String);

	/*
	Define a trigger property that acts as a button. If the value is set to actionName, the property is set to "1" and the value is reset back to "-"
	*/
	long MapTriggerProperty(PropertyAccessorWrapper propAcc, std::string description, vector_of_<std::string> actionName = "Start");

	/*
	Defines labels for the different values of the property with the specified id. If set, the values will be translated using these names before being passed to the PropertyAccessor/User interface
	This means that the device property will expose the names of the values, but internally their indexes are used
	*/
	void SetPropertyNames(long id, std::vector<std::string> names);

	/*
	Common handler for all non-trigger properties. Handles name lookup if applicable
	*/
	int OnProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long data);

	/*
	Handler for trigger properties
	*/
	int OnTrigger(MM::PropertyBase* pProp, MM::ActionType eAct, long data);

private:
	//Property accessor & name of property
	typedef std::pair<PropertyAccessor*, std::string> prop_data;

	std::vector<prop_data> properties_;
	//Names assigned to properties stored by property index
	std::map<long, std::vector<std::string>> valueNames_;
};

template<typename T, typename U>
inline DeviceUtil<T, U>::~DeviceUtil()
{
	for (std::vector<prop_data>::iterator it = properties_.begin(); it != properties_.end(); ++it)
		delete it->first;
}

template<typename T, typename U>
inline long DeviceUtil<T, U>::MapNumProperty(PropertyAccessorWrapper propAcc, std::string description, double lower, double upper, MM::PropertyType propType)
{
	long id = MapProperty(propAcc, description, false, propType);

	SetPropertyLimits(description.c_str(), lower, upper);

	return id;
}

template<typename T, typename U>
inline long DeviceUtil<T, U>::MapProperty(PropertyAccessorWrapper propAcc, std::string description, bool readOnly, MM::PropertyType propType)
{
	std::string val = propAcc.propAcc->QueryParameter((U*)this);

	properties_.push_back(prop_data(propAcc.propAcc, description));
	long id = (long)properties_.size() - 1;
	CreateProperty(description.c_str(), val.c_str(), propType, readOnly, new CPropertyActionEx((U*)this, &DeviceUtil::OnProperty, id));

	return id;
}

template<typename T, typename U>
inline long DeviceUtil<T, U>::MapTriggerProperty(PropertyAccessorWrapper propAcc, std::string description, vector_of_<std::string> actionNames)
{
	properties_.push_back(prop_data(propAcc.propAcc, description));
	long id = (long)properties_.size() - 1;
	//int CreateProperty(name, intial value, "string, integer or float", readOnly, function object called on the property actions)
	CreateProperty(description.c_str(), "-", MM::String, false, new CPropertyActionEx((U*)this, &DeviceUtil::OnTrigger, id));
	actionNames.insert(actionNames.begin(), "-");
	SetPropertyNames(id, actionNames);

	return id;
}

template<typename T, typename U>
inline void DeviceUtil<T, U>::SetPropertyNames(long id, std::vector<std::string> names)
{
	valueNames_[id] = names;
	SetAllowedValues(properties_[id].second.c_str(), names);
}

template<typename T, typename U>
inline int DeviceUtil<T, U>::OnProperty(MM::PropertyBase * pProp, MM::ActionType eAct, long data)
{
	ERRH_START
		std::string val;

	switch (eAct)
	{
	case MM::BeforeGet:
		val = properties_[data].first->QueryParameter((U*)this);

		if (valueNames_.find(data) != valueNames_.end())
		{
			std::vector<std::string> names = valueNames_[data];
			int index = atoi(val.c_str());

			if (names.size() > index)
				val = valueNames_[data][atoi(val.c_str())];
		}
		pProp->Set(val.c_str());
		break;
	case MM::AfterSet:
		pProp->Get(val);

		if (valueNames_.find(data) != valueNames_.end())
		{
			std::vector<std::string> names = valueNames_[data];
			val = to_string(find(names.begin(), names.end(), val) - names.begin());
		}

		properties_[data].first->SetParameter((U*)this, val);
		break;
	}
	ERRH_END
}

template<typename T, typename U>
inline int DeviceUtil<T, U>::OnTrigger(MM::PropertyBase * pProp, MM::ActionType eAct, long data)
{
	ERRH_START
		std::string val;

	switch (eAct)
	{
	case MM::BeforeGet:
		pProp->Set("-");
		break;
	case MM::AfterSet:
		pProp->Get(val);

		if (val != "-")
		{
			std::vector<std::string> names = valueNames_[data];
			int index = find(names.begin(), names.end(), val) - names.begin();
			properties_[data].first->SetParameter((U*)this, to_string(index));
		}
		break;
	}
	ERRH_END
}

template<typename T, typename U>
inline std::string DeviceUtil<T, U>::PropertyAccessor::QueryParameter(U *)
{
	return "0";
}

template<typename T, typename U>
inline void DeviceUtil<T, U>::PropertyAccessor::SetParameter(U *, std::string)
{
}

template <typename T, typename U>
inline DeviceUtil<T, U>::DefPropertyAccessor::DefPropertyAccessor(std::string token) : token(token) {}

template <typename T, typename U>
inline std::string DeviceUtil<T, U>::DefPropertyAccessor::QueryParameter(U* propAcc)
{
	return propAcc->QueryParameter(token);
}

template <typename T, typename U>
inline void DeviceUtil<T, U>::DefPropertyAccessor::SetParameter(U* propAcc, std::string value)
{
	propAcc->SetParameter(token, value);
}

template <typename T, typename U>
inline DeviceUtil<T, U>::ScalingPropertyAccessor::ScalingPropertyAccessor(std::string token, OptRef<double> scalingFactor) : token(token), scalingFactor(scalingFactor) {}

template <typename T, typename U>
inline std::string DeviceUtil<T, U>::ScalingPropertyAccessor::QueryParameter(U* propAcc)
{
	return to_string(from_string<double>(propAcc->QueryParameter(token)) * scalingFactor());
}

template <typename T, typename U>
inline void DeviceUtil<T, U>::ScalingPropertyAccessor::SetParameter(U* propAcc, std::string value)
{
	propAcc->SetParameter(token, to_string(from_string<double>(value) / scalingFactor()));
}

template <typename T, typename U>
inline DeviceUtil<T, U>::CustomPropertyAccessor::CustomPropertyAccessor(std::string(U::* get)(), void(U::* set)(std::string)) : get(get), set(set) {}

template <typename T, typename U>
inline std::string DeviceUtil<T, U>::CustomPropertyAccessor::QueryParameter(U* inst)
{
	return (*inst.*get)();
}

template <typename T, typename U>
inline void DeviceUtil<T, U>::CustomPropertyAccessor::SetParameter(U* inst, std::string value)
{
	if (set != nullptr)
		(*inst.*set)(value);
}

template <typename T, typename U>
inline DeviceUtil<T, U>::PropertyAccessorWrapper::PropertyAccessorWrapper(PropertyAccessor* propAcc) : propAcc(propAcc) {}

template <typename T, typename U>
inline DeviceUtil<T, U>::PropertyAccessorWrapper::PropertyAccessorWrapper(std::string token) : propAcc(new DefPropertyAccessor(token)) {}

template <typename T, typename U>
inline DeviceUtil<T, U>::PropertyAccessorWrapper::PropertyAccessorWrapper(const char * token) : propAcc(new DefPropertyAccessor(token)) {}

template <typename T, typename U> template <typename V>
inline DeviceUtil<T, U>::PropertyAccessorWrapper::PropertyAccessorWrapper(Ref<V> ref) : propAcc(new VariableAccessor<V>(ref())) {}

template <typename T, typename U> template <typename V>
inline DeviceUtil<T, U>::VariableAccessor<V>::VariableAccessor(V& var) : var(var) {}

template <typename T, typename U> template <typename V>
inline std::string DeviceUtil<T, U>::VariableAccessor<V>::QueryParameter(U*)
{
	return to_string(var);
}

template <typename T, typename U> template <typename V>
inline void DeviceUtil<T, U>::VariableAccessor<V>::SetParameter(U*, std::string val)
{
	var = from_string<V>(val);
}
