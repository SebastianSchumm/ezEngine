// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Simple binary output stream
class StreamOut
{
public:
	/// Virtual destructor
	virtual				~StreamOut() = default;

	/// Write a string of bytes to the binary stream
	virtual void		WriteBytes(const void *inData, size_t inNumBytes) = 0;

	/// Returns true if there was an IO failure
	virtual bool		IsFailed() const = 0;

	/// Write a primitive (e.g. float, int, etc.) to the binary stream
	template <class T>
	void				Write(const T &inT)
	{
		WriteBytes(&inT, sizeof(inT));
	}

	/// Write a vector of primitives from the binary stream
	template <class T, class A>
	void				Write(const vector<T, A> &inT)
	{
		typename Array<T>::size_type len = inT.size();
		Write(len);
		if (!IsFailed())
			for (typename Array<T>::size_type i = 0; i < len; ++i)
				Write(inT[i]);
	}

	/// Write a string to the binary stream (writes the number of characters and then the characters)
	template <class Type, class Traits, class Allocator>
	void				Write(const basic_string<Type, Traits, Allocator> &inString)
	{
		typename basic_string<Type, Traits, Allocator>::size_type len = inString.size();
		Write(len);
		if (!IsFailed())
			WriteBytes(inString.data(), len * sizeof(Type));
	}

	/// Write a Vec3 (don't write W)
	void				Write(const Vec3 &inVec)
	{
		WriteBytes(&inVec, 3 * sizeof(float));
	}
};

JPH_NAMESPACE_END
