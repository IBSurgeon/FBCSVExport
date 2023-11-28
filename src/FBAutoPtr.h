#pragma once
#ifndef FB_AUTO_PTR_H
#define FB_AUTO_PTR_H

/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

namespace Firebird
{
	template <typename What>
	class SimpleDelete
	{
	public:
		static void clear(What* ptr)
		{
			static_assert(sizeof(What) > 0, "can't delete pointer to incomplete type");
			delete ptr;
		}
	};

	template <typename What>
	class ArrayDelete
	{
	public:
		static void clear(What* ptr)
		{
			static_assert(sizeof(What) > 0, "can't delete pointer to incomplete type");
			delete[] ptr;
		}
	};


	template <typename T>
	class SimpleRelease
	{
	public:
		static void clear(T* ptr)
		{
			if (ptr)
			{
				ptr->release();
			}
		}
	};


	template <typename T>
	class SimpleDispose
	{
	public:
		static void clear(T* ptr)
		{
			if (ptr)
			{
				ptr->dispose();
			}
		}
	};


	template <typename Where, template <typename W> class Clear = SimpleDelete >
	class AutoPtr
	{
	private:
		Where* ptr;
	public:
		AutoPtr(Where* v = nullptr)
			: ptr(v)
		{}

		AutoPtr(AutoPtr&& v) noexcept
			: ptr(v.ptr)
		{
			v.ptr = nullptr;
		}

		~AutoPtr()
		{
			Clear<Where>::clear(ptr);
		}

		AutoPtr& operator= (Where* v)
		{
			Clear<Where>::clear(ptr);
			ptr = v;
			return *this;
		}

		AutoPtr& operator= (AutoPtr&& r) noexcept
		{
			if (this != &r)
			{
				ptr = r.ptr;
				r.ptr = nullptr;
			}

			return *this;
		}

		Where* get() const
		{
			return ptr;
		}

		operator Where* () const
		{
			return ptr;
		}

		bool operator !() const
		{
			return !ptr;
		}

		bool hasData() const
		{
			return ptr != nullptr;
		}

		Where* operator->() const
		{
			return ptr;
		}

		Where* release() noexcept
		{
			Where* tmp = ptr;
			ptr = nullptr;
			return tmp;
		}

		void reset(Where* v = nullptr)
		{
			if (v != ptr)
			{
				Clear<Where>::clear(ptr);
				ptr = v;
			}
		}

	private:
		AutoPtr(const AutoPtr&) = delete;
		void operator=(const AutoPtr&) = delete;
	};


	template <typename Where>
	class AutoDispose : public AutoPtr<Where, SimpleDispose>
	{
	public:
		AutoDispose(Where* v = nullptr)
			: AutoPtr<Where, SimpleDispose>(v)
		{ }
	};


	template <typename Where>
	class AutoRelease : public AutoPtr<Where, SimpleRelease>
	{
	public:
		AutoRelease(Where* v = nullptr)
			: AutoPtr<Where, SimpleRelease>(v)
		{ }
	};

	template <typename Where>
	class AutoDelete : public AutoPtr<Where, SimpleDelete>
	{
	public:
		AutoDelete(Where* v = nullptr)
			: AutoPtr<Where, SimpleDelete>(v)
		{ }
	};

	template <typename Where>
	class AutoDeleteArray : public AutoPtr<Where, ArrayDelete>
	{
	public:
		AutoDeleteArray(Where* v = nullptr)
			: AutoPtr<Where, ArrayDelete>(v)
		{ }
	};
}

#endif	// FB_AUTO_PTR_H
