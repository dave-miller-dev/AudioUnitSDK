//
//  SynthElement.cpp
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#include "AudioUnitSDK/SynthElement.h"
#include "AudioUnitSDK/AUInstrumentBase.h"
#include "AudioUnitSDK/AUMIDIDefs.h"
#include <assert.h>

SynthElement::SynthElement(AUInstrumentBase& audioUnit, UInt32 inElement)
	: AUElement(audioUnit), mIndex(inElement)
{
}

SynthElement::~SynthElement() {}

SynthGroupElement::SynthGroupElement(
	AUInstrumentBase& audioUnit, UInt32 inElement, MIDIControlHandler* inHandler)
	: SynthElement(audioUnit, inElement), mCurrentAbsoluteFrame(-1), mMidiControlHandler(inHandler),
	  mSustainIsOn(false), mSostenutoIsOn(false), mOutputBus(0), mGroupID(kUnassignedGroup)
{

	for (UInt32 i = 0; i < kNumberOfSoundingNoteStates; ++i) {
		mNoteList[i].mState = (SynthNoteState)i;
	}
}

SynthGroupElement::~SynthGroupElement() { delete mMidiControlHandler; }

void SynthGroupElement::SetGroupID(MusicDeviceGroupID inGroup)
{

	// can't re-assign a group once its been assigned
	if (mGroupID != kUnassignedGroup) {
		throw static_cast<OSStatus>(kAudioUnitErr_InvalidElement);
	}

	mGroupID = inGroup;
}

void SynthGroupElement::Reset()
{

	mMidiControlHandler->Reset();

	for (UInt32 i = 0; i < kNumberOfSoundingNoteStates; ++i) {
		mNoteList[i].Empty();
	}
}

SynthPartElement::SynthPartElement(AUInstrumentBase& audioUnit, UInt32 inElement)
	: SynthElement(audioUnit, inElement)
{
}

// Return the SynthNote with the given inNoteID, if found.  If unreleasedOnly is true, only look for
// attacked and sostenutoed notes, otherwise search all states.  Return state of found note via
// outNoteState.

SynthNote* SynthGroupElement::GetNote(
	NoteInstanceID inNoteID, bool unreleasedOnly, UInt32* outNoteState)
{

	const UInt32 lastNoteState =
		unreleasedOnly ? (mSostenutoIsOn ? kNoteState_Sostenutoed : kNoteState_Attacked)
					   : kNoteState_Released;

	SynthNote* note = NULL;

	// Search for notes in each successive state
	for (UInt32 noteState = kNoteState_Attacked; noteState <= lastNoteState; ++noteState) {

		// even if we find nothing
		if (outNoteState) {
			*outNoteState = noteState;
		}

		note = mNoteList[noteState].mHead;

		while (note && note->mNoteID != inNoteID) {
			note = note->mNext;
		}

		if (note) {
			break;
		}
	}

	return note;
}

void SynthGroupElement::NoteOn(SynthNote* note, SynthPartElement* part, NoteInstanceID inNoteID,
	UInt32 inOffsetSampleFrame, const MusicDeviceNoteParams& inParams)
{

	// TODO: CONSIDER FIXING this to not need to initialize mCurrentAbsoluteFrame to -1.
	UInt64 absoluteFrame = (mCurrentAbsoluteFrame == -1)
							   ? inOffsetSampleFrame
							   : (mCurrentAbsoluteFrame + inOffsetSampleFrame);

	if (note->AttackNote(part, this, inNoteID, absoluteFrame, inOffsetSampleFrame, inParams)) {
		mNoteList[kNoteState_Attacked].AddNote(note);
	}
}

void SynthGroupElement::NoteOff(NoteInstanceID inNoteID, UInt32 inFrame)
{

	UInt32 noteState = kNoteState_Attacked;
	SynthNote* note = GetNote(inNoteID, true, &noteState); // asking for unreleased only

	if (note) {

		if (noteState == kNoteState_Attacked) {
			mNoteList[noteState].RemoveNote(note);

			if (mSustainIsOn) {
				mNoteList[kNoteState_ReleasedButSustained].AddNote(note);

			} else {
				note->Release(inFrame);
				mNoteList[kNoteState_Released].AddNote(note);
			}

		} else {
			/* if (noteState == kNoteState_Sostenutoed) */
			mNoteList[kNoteState_Sostenutoed].RemoveNote(note);
			mNoteList[kNoteState_ReleasedButSostenutoed].AddNote(note);
		}
	}
}

void SynthGroupElement::NoteEnded(SynthNote* inNote, [[maybe_unused]] UInt32 inFrame)
{

	if (inNote->IsSounding()) {

		SynthNoteList* list = &mNoteList[inNote->GetState()];
		list->RemoveNote(inNote);
	}

	GetAUInstrument().AddFreeNote(inNote);
}

void SynthGroupElement::NoteFastReleased(SynthNote* inNote)
{

	if (inNote->IsActive()) {
		mNoteList[inNote->GetState()].RemoveNote(inNote);
		GetAUInstrument().DecNumActiveNotes();
		mNoteList[kNoteState_FastReleased].AddNote(inNote);

	} else {
		assert("ASSERT FAILED:  Attempting to fast-release non-active note");
	}
}

bool SynthGroupElement::ChannelMessage(UInt16 controllerID, UInt16 inValue)
{
	bool handled = true;

	// Sustain and sostenuto are "pedal events", and are handled during render cycle
	if (controllerID <= kMidiController_RPN_MSB && controllerID != kMidiController_Sustain &&
		controllerID != kMidiController_Sostenuto)
		handled = mMidiControlHandler->SetController(controllerID, UInt8(inValue));

	else {

		switch (controllerID) {
		case kMidiMessage_ProgramChange:
			handled = mMidiControlHandler->SetProgramChange(inValue);
			break;
		case kMidiMessage_PitchWheel:
			handled = mMidiControlHandler->SetPitchWheel(inValue);
			break;
		case kMidiMessage_ChannelPressure:
			handled = mMidiControlHandler->SetChannelPressure(UInt8(inValue));
			break;
		case kMidiMessage_PolyPressure: {
			UInt8 inKey = inValue >> 7;
			UInt8 val = inValue & 0x7f;
			handled = mMidiControlHandler->SetPolyPressure(inKey, val);
			break;
		}
		default:
			handled = false;
			break;
		}
	}

	return handled;
}

void SynthGroupElement::SostenutoOn(UInt32 inFrame)
{

	if (!mSostenutoIsOn) {
		mMidiControlHandler->SetController(kMidiController_Sostenuto, 127);
		mSostenutoIsOn = true;
		mNoteList[kNoteState_Sostenutoed].TransferAllFrom(&mNoteList[kNoteState_Attacked], inFrame);
	}
}

void SynthGroupElement::SostenutoOff(UInt32 inFrame)
{

	if (mSostenutoIsOn) {
		mMidiControlHandler->SetController(kMidiController_Sostenuto, 0);
		mSostenutoIsOn = false;
		mNoteList[kNoteState_Attacked].TransferAllFrom(&mNoteList[kNoteState_Sostenutoed], inFrame);

		if (mSustainIsOn) {
			mNoteList[kNoteState_ReleasedButSustained].TransferAllFrom(
				&mNoteList[kNoteState_ReleasedButSostenutoed], inFrame);

		} else {
			mNoteList[kNoteState_Released].TransferAllFrom(
				&mNoteList[kNoteState_ReleasedButSostenutoed], inFrame);
		}
	}
}


void SynthGroupElement::SustainOn([[maybe_unused]] UInt32 inFrame)
{

	if (!mSustainIsOn) {
		mMidiControlHandler->SetController(kMidiController_Sustain, 127);
		mSustainIsOn = true;
	}
}

void SynthGroupElement::SustainOff(UInt32 inFrame)
{
	if (mSustainIsOn) {

		mMidiControlHandler->SetController(kMidiController_Sustain, 0);
		mSustainIsOn = false;

		mNoteList[kNoteState_Released].TransferAllFrom(
			&mNoteList[kNoteState_ReleasedButSustained], inFrame);
	}
}

void SynthGroupElement::AllNotesOff(UInt32 inFrame)
{

	SynthNote* note;

	for (UInt32 i = 0; i <= kNoteState_Sostenutoed; ++i) {
		UInt32 newState =
			(i == kNoteState_Attacked) ? kNoteState_Released : kNoteState_ReleasedButSostenutoed;
		note = mNoteList[i].mHead;

		while (note) {
			SynthNote* nextNote = note->mNext;

			mNoteList[i].RemoveNote(note);
			note->Release(inFrame);
			mNoteList[newState].AddNote(note);

			note = nextNote;
		}
	}
}

void SynthGroupElement::AllSoundOff(UInt32 inFrame)
{

	SynthNote* note;
	for (UInt32 i = 0; i < kNumberOfActiveNoteStates; ++i) {
		note = mNoteList[i].mHead;

		while (note) {
			SynthNote* nextNote = note->mNext;

			mNoteList[i].RemoveNote(note);
			note->FastRelease(inFrame);
			mNoteList[kNoteState_FastReleased].AddNote(note);
			GetAUInstrument().DecNumActiveNotes();
			note = nextNote;
		}
	}
}

void SynthGroupElement::ResetAllControllers([[maybe_unused]] UInt32 inFrame)
{
	mMidiControlHandler->Reset();
}

OSStatus SynthGroupElement::Render(
	SInt64 inAbsoluteSampleFrame, UInt32 inNumberFrames, ausdk::AUScope& outputs)
{

	// Avoid duplicate calls at same sample offset
	if (inAbsoluteSampleFrame != mCurrentAbsoluteFrame) {

		mCurrentAbsoluteFrame = inAbsoluteSampleFrame;
		AudioBufferList* buffArray[16];
		UInt32 numOutputs = outputs.GetNumberOfElements();

		for (UInt32 outBus = 0; outBus < numOutputs && outBus < 16; ++outBus) {
			buffArray[outBus] = &GetAudioUnit().Output(outBus).GetBufferList();
		}

		for (UInt32 i = 0; i < kNumberOfSoundingNoteStates; ++i) {
			SynthNote* note = mNoteList[i].mHead;

			while (note) {

				SynthNote* nextNote = note->mNext;
				OSStatus err =
					note->Render(inAbsoluteSampleFrame, inNumberFrames, buffArray, numOutputs);

				if (err) {
					return err;
				}

				note = nextNote;
			}
		}
	}
	return noErr;
}
