//
//  SynthNoteList.hpp
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#ifndef SynthNoteList_h
#define SynthNoteList_h

#include "AudioUnitSDK/SynthNote.h"

class SynthNoteList {

public:
	SynthNoteList() : mState(kNoteState_Unset), mHead(0), mTail(0) {}

	bool NotEmpty() const { return mHead != NULL; }
	bool IsEmpty() const { return mHead == NULL; }
	void Empty()
	{
		//        SanityCheck();
		mHead = mTail = NULL;
	}

	UInt32 Length() const
	{

		//        SanityCheck();

		UInt32 length = 0;

		for (SynthNote* note = mHead; note; note = note->mNext) {
			length++;
		}

		return length;
	};

	void AddNote(SynthNote* inNote)
	{

		//        printf("AddNote(inNote=%p) to state: %u\n", inNote, mState);
		//        SanityCheck();
		inNote->SetState(mState);
		inNote->mNext = mHead;
		inNote->mPrev = NULL;

		if (mHead) {
			mHead->mPrev = inNote;
			mHead = inNote;
		} else {
			mHead = mTail = inNote;
		}
	}

	void RemoveNote(SynthNote* inNote)
	{

		if (inNote->mPrev) {
			inNote->mPrev->mNext = inNote->mNext;
		} else {
			mHead = inNote->mNext;
		}

		if (inNote->mNext) {
			inNote->mNext->mPrev = inNote->mPrev;
		} else {
			mTail = inNote->mPrev;
		}

		inNote->mPrev = 0;
		inNote->mNext = 0;
	}

	void TransferAllFrom(SynthNoteList* inNoteList, UInt32 inFrame)
	{

		if (!inNoteList->mTail) {
			return;
		}

		if (mState == kNoteState_Released) {

			for (SynthNote* note = inNoteList->mHead; note; note = note->mNext) {
				note->Release(inFrame);
				note->SetState(mState);
			}

		} else {

			for (SynthNote* note = inNoteList->mHead; note; note = note->mNext) {
				note->SetState(mState);
			}
		}

		inNoteList->mTail->mNext = mHead;

		if (mHead) {
			mHead->mPrev = inNoteList->mTail;

		} else {
			mTail = inNoteList->mTail;
		}

		mHead = inNoteList->mHead;

		inNoteList->mHead = NULL;
		inNoteList->mTail = NULL;
	}

	SynthNote* FindOldestNote()
	{

		UInt64 minStartFrame = -1;
		SynthNote* oldestNote = NULL;

		for (SynthNote* note = mHead; note; note = note->mNext) {

			if (note->mAbsoluteStartFrame < minStartFrame) {
				oldestNote = note;
				minStartFrame = note->mAbsoluteStartFrame;
			}
		}

		return oldestNote;
	}

	SynthNote* FindMostQuietNote()
	{

		Float32 minAmplitude = 1e9f;
		UInt64 minStartFrame = -1;
		SynthNote* mostQuietNote = NULL;

		for (SynthNote* note = mHead; note; note = note->mNext) {
			Float32 amp = note->Amplitude();

			if (amp < minAmplitude) {
				mostQuietNote = note;
				minAmplitude = amp;
				minStartFrame = note->mAbsoluteStartFrame;

			} else if (amp == minAmplitude && note->mAbsoluteStartFrame < minStartFrame) {
				// use earliest start time as a tie breaker
				mostQuietNote = note;
				minStartFrame = note->mAbsoluteStartFrame;
			}
		}

		return mostQuietNote;
	}

	void SanityCheck() const;

	SynthNoteState mState;
	SynthNote* mHead;
	SynthNote* mTail;
};

#endif /* SynthNoteList_hpp */
