/*
Copyright (C) 2016 Apple Inc. All Rights Reserved.
See LICENSE.txt for this sample’s licensing information

Abstract:
Part of Core Audio AUInstrument Base Classes
*/

#include <libkern/OSAtomic.h>

template <class ITEM>
class LockFreeFIFOWithFree {

private:
	LockFreeFIFOWithFree(); // private, unimplemented.

public:
	LockFreeFIFOWithFree(UInt32 inMaxSize) : mReadIndex(0), mWriteIndex(0), mFreeIndex(0)
	{
		// assert(IsPowerOfTwo(inMaxSize));
		mItems = new ITEM[inMaxSize];
		mMask = inMaxSize - 1;
	}

	~LockFreeFIFOWithFree() { delete[] mItems; }


	void Reset()
	{
		FreeItems();
		mReadIndex = 0;
		mWriteIndex = 0;
		mFreeIndex = 0;
	}

	ITEM* WriteItem()
	{

		// printf("WriteItem %d %d\n", mReadIndex, mWriteIndex);
		FreeItems(); // free items on the write thread.
		int32_t nextWriteIndex = (mWriteIndex + 1) & mMask;

		if (nextWriteIndex == mFreeIndex) {
			return NULL;
		}

		return &mItems[mWriteIndex];
	}

	ITEM* ReadItem()
	{
		// printf("ReadItem %d %d\n", mReadIndex, mWriteIndex);
		if (mReadIndex == mWriteIndex) {
			return NULL;
		}

		return &mItems[mReadIndex];
	}

	void AdvanceWritePtr()
	{
		auto expected = mWriteIndex.load();
		mWriteIndex.compare_exchange_strong(expected, (mWriteIndex + 1) & mMask,
			std::memory_order_relaxed, std::memory_order_relaxed);
	}

	void AdvanceReadPtr()
	{
		auto expected = mReadIndex.load();
		mReadIndex.compare_exchange_strong(expected, (mReadIndex + 1) & mMask,
			std::memory_order_relaxed, std::memory_order_relaxed);
	}

private:
	ITEM* FreeItem()
	{
		if (mFreeIndex == mReadIndex)
			return NULL;
		return &mItems[mFreeIndex];
	}
	void AdvanceFreePtr()
	{
		auto expected = mFreeIndex.load();
		mFreeIndex.compare_exchange_strong(expected, (mFreeIndex + 1) & mMask,
			std::memory_order_relaxed, std::memory_order_relaxed);
	}

	void FreeItems()
	{
		ITEM* item;
		while ((item = FreeItem()) != NULL) {
			item->Free();
			AdvanceFreePtr();
		}
	}

	std::atomic<int32_t> mReadIndex, mWriteIndex, mFreeIndex;
	int32_t mMask;
	ITEM* mItems;
};


// Same as above but no free.

template <class ITEM>
class LockFreeFIFO {
	LockFreeFIFO(); // private, unimplemented.
public:
	LockFreeFIFO(UInt32 inMaxSize) : mReadIndex(0), mWriteIndex(0)
	{
		// assert(IsPowerOfTwo(inMaxSize));
		mItems = new ITEM[inMaxSize];
		mMask = inMaxSize - 1;
	}

	~LockFreeFIFO() { delete[] mItems; }

	void Reset()
	{
		mReadIndex = 0;
		mWriteIndex = 0;
	}

	ITEM* WriteItem()
	{
		int32_t nextWriteIndex = (mWriteIndex + 1) & mMask;
		if (nextWriteIndex == mReadIndex)
			return NULL;
		return &mItems[mWriteIndex];
	}

	ITEM* ReadItem()
	{
		if (mReadIndex == mWriteIndex)
			return NULL;
		return &mItems[mReadIndex];
	}

	// the CompareAndSwap will always succeed. We use CompareAndSwap because it calls the PowerPC
	// sync instruction, plus any processor bug workarounds for various CPUs.
	void AdvanceWritePtr()
	{
		OSAtomicCompareAndSwap32(mWriteIndex, (mWriteIndex + 1) & mMask, &mWriteIndex);
	}
	void AdvanceReadPtr()
	{
		OSAtomicCompareAndSwap32(mReadIndex, (mReadIndex + 1) & mMask, &mReadIndex);
	}

private:
	volatile int32_t mReadIndex, mWriteIndex;
	int32_t mMask;
	ITEM* mItems;
};
