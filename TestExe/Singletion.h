#ifndef Singleton_h__
#define Singleton_h__

#pragma once
#include <Windows.h>
#include <memory>

namespace ISingleton
{
	class CResGuard {
	public:
		CResGuard()  { m_lGrdCnt = 0; InitializeCriticalSection(&m_cs); }
		~CResGuard() { DeleteCriticalSection(&m_cs); }

		// IsGuarded is used for debugging
		BOOL IsGuarded() const { return(m_lGrdCnt > 0); }

	public:
		class CGuard {
		public:
			CGuard(CResGuard& rg) : m_rg(rg) { m_rg.Guard(); };
			~CGuard() { m_rg.Unguard(); }

		private:
			CResGuard& m_rg;
		};

	private:
		void Guard()   { EnterCriticalSection(&m_cs); m_lGrdCnt++; }
		void Unguard() { m_lGrdCnt--; LeaveCriticalSection(&m_cs); }

		// Guard/Unguard can only be accessed by the nested CGuard class.
		friend class CResGuard::CGuard;

	private:
		CRITICAL_SECTION m_cs;
		long m_lGrdCnt;   // # of EnterCriticalSection calls
	};

	template <class T>
	class CSingleton
	{
	public:
		static inline T* Instance();

	private:
		CSingleton(void){}
		~CSingleton(void){}
		CSingleton(const CSingleton&){}
		CSingleton & operator= (const CSingleton &){}

	private:
		static std::auto_ptr<T> _instance;
		static ISingleton::CResGuard _rs;

	};

	template <class T> std::auto_ptr<T> CSingleton<T>::_instance;
	template <class T> ISingleton::CResGuard CSingleton<T>::_rs;

	template <class T> inline T* CSingleton<T>::Instance()
	{
		if (0 == _instance.get())
		{
			ISingleton::CResGuard::CGuard gd(_rs);
			if (0 == _instance.get())
			{
				_instance.reset(new T);
			}
		}

		return _instance.get();
	}

	//Class that will implement the singleton mode, 
	//must use the macro in it's delare file

#define DECLARE_SINGLETON_CLASS( type ) \
		friend class std::auto_ptr<type>;\
		friend class ISingleton::CSingleton<type>;

};

#endif // Singleton_h__