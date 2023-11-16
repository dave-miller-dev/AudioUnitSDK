//
//  SynthElement.hpp
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#ifndef SynthElement_h
#define SynthElement_h

#include "AudioUnitSDK.h"
#include "MIDIControlHandler.h"
#include "SynthNote.h"
#include "SynthNoteList.h"
#include <AudioToolBox/AudioUnit.h>

class AUInstrumentBase;

class SynthElement : public ausdk::AUElement {

public:
	SynthElement(AUInstrumentBase& audioUnit, UInt32 inElement);

	virtual ~SynthElement();

	UInt32 GetIndex() const { return mIndex; }

	AUInstrumentBase& GetAUInstrument() { return (AUInstrumentBase&)GetAudioUnit(); }

private:
	UInt32 mIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SynthKeyZone {
	UInt8 mLoNote;
	UInt8 mHiNote;
	UInt8 mLoVelocity;
	UInt8 mHiVelocity;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////

const UInt32 kUnlimitedPolyphony = 0xFFFFFFFF;

class SynthPartElement : public SynthElement {

public:
	SynthPartElement(AUInstrumentBase& audioUnit, UInt32 inElement);

	UInt32 GetGroupIndex() const { return mGroupIndex; }

	bool InRange(Float32 inNote, Float32 inVelocity);

	UInt32 GetMaxPolyphony() const { return mMaxPolyphony; }

	void SetMaxPolyphony(UInt32 inMaxPolyphony) { mMaxPolyphony = inMaxPolyphony; }

private:
	UInt32 mGroupIndex;
	UInt32 mPatchIndex;
	UInt32 mMaxPolyphony;
	SynthKeyZone mKeyZone;
};


class SynthGroupElement : public SynthElement {

public:
	enum { kUnassignedGroup = 0xFFFFFFFF };

	SynthGroupElement(AUInstrumentBase& audioUnit, UInt32 inElement, MIDIControlHandler* inHandler);

	virtual ~SynthGroupElement();

	virtual void NoteOn(SynthNote* note, SynthPartElement* part, NoteInstanceID inNoteID,
		UInt32 inOffsetSampleFrame, const MusicDeviceNoteParams& inParams);

	virtual void NoteOff(NoteInstanceID inNoteID, UInt32 inOffsetSampleFrame);

	void SustainOn(UInt32 inFrame);
	void SustainOff(UInt32 inFrame);
	void SostenutoOn(UInt32 inFrame);
	void SostenutoOff(UInt32 inFrame);

	void NoteEnded(SynthNote* inNote, UInt32 inFrame);

	void NoteFastReleased(SynthNote* inNote);

	virtual bool ChannelMessage(UInt16 controlID, UInt16 controlValue);

	virtual void AllNotesOff(UInt32 inFrame);

	virtual void AllSoundOff(UInt32 inFrame);

	void ResetAllControllers(UInt32 inFrame);

	SynthNote* GetNote(
		NoteInstanceID inNoteID, bool unreleasedOnly = false, UInt32* outNoteState = NULL);

	void Reset();

	virtual OSStatus Render(
		SInt64 inAbsoluteSampleFrame, UInt32 inNumberFrames, ausdk::AUScope& outputs);

	float GetPitchBend() const { return mMidiControlHandler->GetPitchBend(); }

	SInt64 GetCurrentAbsoluteFrame() const { return mCurrentAbsoluteFrame; }

	MusicDeviceGroupID GroupID() const { return mGroupID; }

	virtual void SetGroupID(MusicDeviceGroupID inGroup);

	MIDIControlHandler* GetMIDIControlHandler() const { return mMidiControlHandler; }

protected:
	SInt64 mCurrentAbsoluteFrame;
	SynthNoteList mNoteList[kNumberOfSoundingNoteStates];
	MIDIControlHandler* mMidiControlHandler;

private:
	friend class AUInstrumentBase;

	bool mSustainIsOn;
	bool mSostenutoIsOn;
	UInt32 mOutputBus;
	MusicDeviceGroupID mGroupID;
};


#endif /* SynthElement_hpp */
