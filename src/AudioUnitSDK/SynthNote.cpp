//
//  SynthVoice.cpp
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#include "AudioUnitSDK/SynthNote.h"
#include "AudioUnitSDK/SynthElement.h"

bool SynthNote::AttackNote(SynthPartElement* inPart, SynthGroupElement* inGroup,
	NoteInstanceID inNoteID, UInt64 inAbsoluteSampleFrame, UInt32 inOffsetSampleFrame,
	const MusicDeviceNoteParams& inParams)
{

	mPart = inPart;
	mGroup = inGroup;
	mNoteID = inNoteID;

	mAbsoluteStartFrame = inAbsoluteSampleFrame;
	mRelativeStartFrame = inOffsetSampleFrame;
	mRelativeReleaseFrame = -1;
	mRelativeKillFrame = -1;

	mPitch = inParams.mPitch;
	mVelocity = inParams.mVelocity;

	return Attack(inParams);
}


void SynthNote::Reset()
{
	mPart = 0;
	mGroup = 0;
	mAbsoluteStartFrame = 0;
	mRelativeStartFrame = 0;
	mRelativeReleaseFrame = -1;
	mRelativeKillFrame = -1;
}

void SynthNote::Kill(UInt32 inFrame) { mRelativeKillFrame = inFrame; }

void SynthNote::Release(UInt32 inFrame) { mRelativeReleaseFrame = inFrame; }

void SynthNote::FastRelease(UInt32 inFrame) { mRelativeReleaseFrame = inFrame; }

double SynthNote::TuningA() const { return 440.0; }

double SynthNote::Frequency()
{
	return TuningA() * pow(2.0, (mPitch - 69.0 + GetPitchBend()) / 12);
}

double SynthNote::SampleRate() { return GetAudioUnit().Output(0).GetStreamFormat().mSampleRate; }

ausdk::MusicDeviceBase& SynthNote::GetAudioUnit() const
{
	return (ausdk::MusicDeviceBase&)mGroup->GetAudioUnit();
}

Float32 SynthNote::GetGlobalParameter(AudioUnitParameterID inParamID) const
{
	return mGroup->GetAudioUnit().Globals()->GetParameter(inParamID);
}

void SynthNote::NoteEnded(UInt32 inFrame)
{
	mGroup->NoteEnded(this, inFrame);
	mNoteID = 0xFFFFFFFF;
}

float SynthNote::GetPitchBend() const { return mGroup->GetPitchBend(); }
