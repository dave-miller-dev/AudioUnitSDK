//
//  AUInstrumentBase.cpp
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#include "AudioUnitSDK/AUInstrumentBase.h"
#include "AudioUnitSDK/AUMIDIDefs.h"
#include "AudioUnitSDK/SynthEvent.h"

const UInt32 kEventQueueSize = 1024;

AUInstrumentBase::AUInstrumentBase(AudioComponentInstance inInstance, UInt32 numInputs,
	UInt32 numOutputs, UInt32 numGroups, UInt32 numParts)
	: MusicDeviceBase(inInstance, numInputs, numOutputs, numGroups), mAbsoluteSampleFrame(0),
	  mEventQueue(kEventQueueSize), mNumNotes(0), mNumActiveNotes(0), mMaxActiveNotes(0), mNotes(0),
	  mNoteSize(0), mInitNumPartEls(numParts)
{

	mFreeNotes.mState = kNoteState_Free;
	SetWantsRenderThreadID(true);
}


AUInstrumentBase::~AUInstrumentBase()
{
#if DEBUG_PRINT
	printf("delete AUInstrumentBase\n");
#endif
}

std::unique_ptr<ausdk::AUElement> AUInstrumentBase::CreateElement(
	AudioUnitScope inScope, AudioUnitElement element)
{

	switch (inScope) {
	case kAudioUnitScope_Group:
		return std::unique_ptr<ausdk::AUElement>(
			new SynthGroupElement(*this, element, new MidiControls));
	case kAudioUnitScope_Part:
		return std::unique_ptr<ausdk::AUElement>(new SynthPartElement(*this, element));
	}
	return MusicDeviceBase::CreateElement(inScope, element);
}

void AUInstrumentBase::CreateExtendedElements()
{
	Parts().Initialize(this, kAudioUnitScope_Part, mInitNumPartEls);
}

ausdk::AUScope* AUInstrumentBase::GetScopeExtended(AudioUnitScope inScope)
{

	if (inScope == kAudioUnitScope_Part) {
		return &mPartScope;
	}

	return NULL;
}


void AUInstrumentBase::SetNotes(
	UInt32 inNumNotes, UInt32 inMaxActiveNotes, SynthNote* inNotes, UInt32 inNoteDataSize)
{

	mNumNotes = inNumNotes;
	mMaxActiveNotes = inMaxActiveNotes;
	mNoteSize = inNoteDataSize;
	mNotes = inNotes;

	for (UInt32 i = 0; i < mNumNotes; ++i) {
		SynthNote* note = GetNote(i);
		note->Reset();
		mFreeNotes.AddNote(note);
	}
}

UInt32 AUInstrumentBase::CountActiveNotes()
{

	// debugging tool.
	UInt32 sum = 0;

	for (UInt32 i = 0; i < mNumNotes; ++i) {
		SynthNote* note = GetNote(i);

		if (note->GetState() <= kNoteState_Released) {
			sum++;
		}
	}

	return sum;
}

void AUInstrumentBase::AddFreeNote(SynthNote* inNote)
{

	// Fast-released notes are already considered inactive and have already decr'd the active count
	if (inNote->GetState() < kNoteState_FastReleased) {
		DecNumActiveNotes();
	}

	mFreeNotes.AddNote(inNote);
}

OSStatus AUInstrumentBase::Initialize()
{
	/*
	TO DO:
		Currently ValidFormat will check and validate that the num channels is not being
		changed if the AU doesn't support the SupportedNumChannels property - which is correct

		What needs to happen here is that IFF the AU does support this property, (ie, the AU
		can be configured to have different num channels than its original configuration) then
		the state of the AU at Initialization needs to be validated.

		This is work still to be done - see AUEffectBase for the kind of logic that needs to be
	applied here
	*/

	// override to call SetNotes

	mNoteIDCounter = 128; // reset this every time we initialise
	mAbsoluteSampleFrame = 0;
	return noErr;
}

void AUInstrumentBase::Cleanup() { mFreeNotes.Empty(); }


OSStatus AUInstrumentBase::Reset(AudioUnitScope inScope, AudioUnitElement inElement)
{

	if (inScope == kAudioUnitScope_Global) {

		// kill all notes..
		mFreeNotes.Empty();

		for (UInt32 i = 0; i < mNumNotes; ++i) {

			SynthNote* note = GetNote(i);

			if (note->IsSounding()) {
				note->Kill(0);
			}

			note->ListRemove();
			mFreeNotes.AddNote(note);
		}

		mNumActiveNotes = 0;
		mAbsoluteSampleFrame = 0;

		// empty lists.
		UInt32 numGroups = Groups().GetNumberOfElements();

		for (UInt32 j = 0; j < numGroups; ++j) {
			SynthGroupElement* group = (SynthGroupElement*)Groups().GetElement(j);
			group->Reset();
		}
	}

	return MusicDeviceBase::Reset(inScope, inElement);
}

void AUInstrumentBase::PerformEvents([[maybe_unused]] const AudioTimeStamp& inTimeStamp)
{

	SynthEvent* event;
	SynthGroupElement* group;

	while ((event = mEventQueue.ReadItem()) != NULL) {

		switch (event->GetEventType()) {
		case SynthEvent::kEventType_NoteOn:
			RealTimeStartNote(GetElForGroupID(event->GetGroupID()), event->GetNoteID(),
				event->GetOffsetSampleFrame(), *event->GetParams());
			break;
		case SynthEvent::kEventType_NoteOff:
			RealTimeStopNote(
				event->GetGroupID(), event->GetNoteID(), event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_SustainOn:
			group = GetElForGroupID(event->GetGroupID());
			group->SustainOn(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_SustainOff:
			group = GetElForGroupID(event->GetGroupID());
			group->SustainOff(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_SostenutoOn:
			group = GetElForGroupID(event->GetGroupID());
			group->SostenutoOn(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_SostenutoOff:
			group = GetElForGroupID(event->GetGroupID());
			group->SostenutoOff(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_AllNotesOff:
			group = GetElForGroupID(event->GetGroupID());
			group->AllNotesOff(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_AllSoundOff:
			group = GetElForGroupID(event->GetGroupID());
			group->AllSoundOff(event->GetOffsetSampleFrame());
			break;
		case SynthEvent::kEventType_ResetAllControllers:
			group = GetElForGroupID(event->GetGroupID());
			group->ResetAllControllers(event->GetOffsetSampleFrame());
			break;
		}

		mEventQueue.AdvanceReadPtr();
	}
}


OSStatus AUInstrumentBase::Render([[maybe_unused]] AudioUnitRenderActionFlags& ioActionFlags,
	const AudioTimeStamp& inTimeStamp, UInt32 inNumberFrames)
{
	PerformEvents(inTimeStamp);

	ausdk::AUScope& outputs = Outputs();
	UInt32 numOutputs = outputs.GetNumberOfElements();

	for (UInt32 j = 0; j < numOutputs; ++j) {

		// AUBase::DoRenderBus() only does this for the first output element
		Output(j).PrepareBuffer(inNumberFrames);
		AudioBufferList& bufferList = Output(j).GetBufferList();

		for (UInt32 k = 0; k < bufferList.mNumberBuffers; ++k) {
			memset(bufferList.mBuffers[k].mData, 0, bufferList.mBuffers[k].mDataByteSize);
		}
	}

	UInt32 numGroups = Groups().GetNumberOfElements();

	for (UInt32 j = 0; j < numGroups; ++j) {
		SynthGroupElement* group = (SynthGroupElement*)Groups().GetElement(j);
		OSStatus err = group->Render((SInt64)inTimeStamp.mSampleTime, inNumberFrames, outputs);
		if (err)
			return err;
	}

	mAbsoluteSampleFrame += inNumberFrames;
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    AUInstrumentBase::ValidFormat
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AUInstrumentBase::ValidFormat(AudioUnitScope inScope, AudioUnitElement inElement,
	const AudioStreamBasicDescription& inNewFormat)
{

	// if the AU supports this, then we should just let this go through to the Init call
	if (SupportedNumChannels(NULL)) {
		return MusicDeviceBase::ValidFormat(inScope, inElement, inNewFormat);
	}

	bool isGood = MusicDeviceBase::ValidFormat(inScope, inElement, inNewFormat);

	if (!isGood) {
		return false;
	}

	// if we get to here, then the basic criteria is that the
	// num channels cannot change on an existing bus
	ausdk::AUIOElement& el = IOElement(inScope, inElement);

	return (el.GetStreamFormat().mChannelsPerFrame == inNewFormat.mChannelsPerFrame);
}


bool AUInstrumentBase::StreamFormatWritable(
	[[maybe_unused]] AudioUnitScope scope, [[maybe_unused]] AudioUnitElement element)
{
	return IsInitialized() ? false : true;
}


OSStatus AUInstrumentBase::RealTimeStartNote([[maybe_unused]] SynthGroupElement* inGroup,
	[[maybe_unused]] NoteInstanceID inNoteInstanceID, [[maybe_unused]] UInt32 inOffsetSampleFrame,
	[[maybe_unused]] const MusicDeviceNoteParams& inParams)
{
	return noErr;
}

SynthPartElement* AUInstrumentBase::GetPartElement(AudioUnitElement inPartElement)
{

	ausdk::AUScope& parts = Parts();
	unsigned int numEls = parts.GetNumberOfElements();

	for (unsigned int i = 0; i < numEls; ++i) {

		SynthPartElement* el = reinterpret_cast<SynthPartElement*>(parts.GetElement(i));
		if (el->GetIndex() == inPartElement) {
			return el;
		}
	}
	return NULL;
}

SynthGroupElement* AUInstrumentBase::GetElForGroupID(MusicDeviceGroupID inGroupID)
{

	ausdk::AUScope& groups = Groups();
	unsigned int numEls = groups.GetNumberOfElements();
	SynthGroupElement* unassignedEl = NULL;

	for (unsigned int i = 0; i < numEls; ++i) {

		SynthGroupElement* el = reinterpret_cast<SynthGroupElement*>(groups.GetElement(i));

		if (el->GroupID() == inGroupID) {
			return el;
		}

		if (el->GroupID() == SynthGroupElement::kUnassignedGroup) {
			unassignedEl = el;
			break; // we fill this up from the start of the group scope vector
		}
	}

	if (unassignedEl) {
		unassignedEl->SetGroupID(inGroupID);
		return unassignedEl;
	}

	throw static_cast<OSStatus>(kAudioUnitErr_InvalidElement);
}

OSStatus AUInstrumentBase::RealTimeStopNote(
	MusicDeviceGroupID inGroupID, NoteInstanceID inNoteInstanceID, UInt32 inOffsetSampleFrame)
{

	SynthGroupElement* gp = (inGroupID == kMusicNoteEvent_Unused ? GetElForNoteID(inNoteInstanceID)
																 : GetElForGroupID(inGroupID));

	if (gp) {
		gp->NoteOff(inNoteInstanceID, inOffsetSampleFrame);
	}

	return noErr;
}

SynthGroupElement* AUInstrumentBase::GetElForNoteID(NoteInstanceID inNoteID)
{

	ausdk::AUScope& groups = Groups();
	unsigned int numEls = groups.GetNumberOfElements();

	for (unsigned int i = 0; i < numEls; ++i) {

		SynthGroupElement* el = reinterpret_cast<SynthGroupElement*>(groups.GetElement(i));

		// searches for any note state
		if (el->GetNote(inNoteID) != NULL) {
			return el;
		}
	}

	throw static_cast<OSStatus>(kAudioUnitErr_InvalidElement);
}

OSStatus AUInstrumentBase::StartNote([[maybe_unused]] MusicDeviceInstrumentID inInstrument,
	MusicDeviceGroupID inGroupID, NoteInstanceID* outNoteInstanceID, UInt32 inOffsetSampleFrame,
	const MusicDeviceNoteParams& inParams)
{
	OSStatus err = noErr;
	NoteInstanceID noteID;

	if (outNoteInstanceID) {
		noteID = NextNoteID();
		*outNoteInstanceID = noteID;

	} else {
		noteID = (UInt32)inParams.mPitch;
	}


	if (InRenderThread()) {

		err = RealTimeStartNote(GetElForGroupID(inGroupID), noteID, inOffsetSampleFrame, inParams);

	} else {

		SynthEvent* event = mEventQueue.WriteItem();

		// queue full
		if (!event) {
			return -1;
		}

		event->Set(
			SynthEvent::kEventType_NoteOn, inGroupID, noteID, inOffsetSampleFrame, &inParams);

		mEventQueue.AdvanceWritePtr();
	}

	return err;
}

OSStatus AUInstrumentBase::StopNote(
	MusicDeviceGroupID inGroupID, NoteInstanceID inNoteInstanceID, UInt32 inOffsetSampleFrame)
{

	OSStatus err = noErr;

	if (InRenderThread()) {
		err = RealTimeStopNote(inGroupID, inNoteInstanceID, inOffsetSampleFrame);

	} else {
		SynthEvent* event = mEventQueue.WriteItem();

		// queue full
		if (!event) {
			return -1;
		}

		event->Set(
			SynthEvent::kEventType_NoteOff, inGroupID, inNoteInstanceID, inOffsetSampleFrame, NULL);
		mEventQueue.AdvanceWritePtr();
	}

	return err;
}

OSStatus AUInstrumentBase::SendPedalEvent(
	MusicDeviceGroupID inGroupID, UInt32 inEventType, UInt32 inOffsetSampleFrame)
{

	if (InRenderThread()) {
		SynthGroupElement* group = GetElForGroupID(inGroupID);

		if (!group) {
			return kAudioUnitErr_InvalidElement;
		}

		switch (inEventType) {
		case SynthEvent::kEventType_SustainOn:
			group->SustainOn(inOffsetSampleFrame);
			break;
		case SynthEvent::kEventType_SustainOff:
			group->SustainOff(inOffsetSampleFrame);
			break;
		case SynthEvent::kEventType_SostenutoOn:
			group->SostenutoOn(inOffsetSampleFrame);
			break;
		case SynthEvent::kEventType_SostenutoOff:
			group->SostenutoOff(inOffsetSampleFrame);
			break;
		case SynthEvent::kEventType_AllNotesOff:
			group->AllNotesOff(inOffsetSampleFrame);
			mNumActiveNotes = CountActiveNotes();
			break;
		case SynthEvent::kEventType_AllSoundOff:
			group->AllSoundOff(inOffsetSampleFrame);
			mNumActiveNotes = CountActiveNotes();
			break;
		case SynthEvent::kEventType_ResetAllControllers:
			group->ResetAllControllers(inOffsetSampleFrame);
			break;
		}

	} else {
		SynthEvent* event = mEventQueue.WriteItem();

		// queue full
		if (!event) {
			return -1;
		}

		event->Set(inEventType, inGroupID, 0, 0, NULL);
		mEventQueue.AdvanceWritePtr();
	}

	return noErr;
}

OSStatus AUInstrumentBase::HandleControlChange(
	UInt8 inChannel, UInt8 inController, UInt8 inValue, UInt32 inStartFrame)
{

	SynthGroupElement* gp = GetElForGroupID(inChannel);

	if (gp) {
		gp->ChannelMessage(inController, inValue);

	} else {
		return kAudioUnitErr_InvalidElement;
	}

	switch (inController) {

	case kMidiController_Sustain:
		if (inValue >= 64) {
			SendPedalEvent(inChannel, SynthEvent::kEventType_SustainOn, inStartFrame);
		} else {
			SendPedalEvent(inChannel, SynthEvent::kEventType_SustainOff, inStartFrame);
		}

		break;

	case kMidiController_Sostenuto:
		if (inValue >= 64) {
			SendPedalEvent(inChannel, SynthEvent::kEventType_SostenutoOn, inStartFrame);
		} else {
			SendPedalEvent(inChannel, SynthEvent::kEventType_SostenutoOff, inStartFrame);
		}

		break;

	case kMidiController_OmniModeOff:
	case kMidiController_OmniModeOn:
	case kMidiController_MonoModeOn:
	case kMidiController_MonoModeOff:
		HandleAllSoundOff(inChannel);
		break;
	}

	return noErr;
}

OSStatus AUInstrumentBase::HandlePitchWheel(
	UInt8 inChannel, UInt8 inPitch1, UInt8 inPitch2, [[maybe_unused]] UInt32 inStartFrame)
{

	SynthGroupElement* gp = GetElForGroupID(inChannel);

	if (gp) {
		gp->ChannelMessage(kMidiMessage_PitchWheel, (inPitch2 << 7) | inPitch1);
		return noErr;
	}

	return kAudioUnitErr_InvalidElement;
}


OSStatus AUInstrumentBase::HandleChannelPressure(
	UInt8 inChannel, UInt8 inValue, [[maybe_unused]] UInt32 inStartFrame)
{
	SynthGroupElement* gp = GetElForGroupID(inChannel);

	if (gp) {
		gp->ChannelMessage(kMidiMessage_ChannelPressure, inValue);
		return noErr;
	}

	return kAudioUnitErr_InvalidElement;
}


OSStatus AUInstrumentBase::HandleProgramChange(UInt8 inChannel, UInt8 inValue)
{

	SynthGroupElement* gp = GetElForGroupID(inChannel);

	if (gp) {
		gp->ChannelMessage(kMidiMessage_ProgramChange, inValue);
		return noErr;
	}

	return kAudioUnitErr_InvalidElement;
}


OSStatus AUInstrumentBase::HandlePolyPressure(
	UInt8 inChannel, UInt8 inKey, UInt8 inValue, [[maybe_unused]] UInt32 inStartFrame)
{

	SynthGroupElement* gp = GetElForGroupID(inChannel);

	if (gp) {
		// Combine key and value into single argument.  UGLY!
		gp->ChannelMessage(kMidiMessage_PolyPressure, (inKey << 7) | inValue);
		return noErr;
	}

	return kAudioUnitErr_InvalidElement;
}


OSStatus AUInstrumentBase::HandleResetAllControllers(UInt8 inChannel)
{
	return SendPedalEvent(inChannel, SynthEvent::kEventType_ResetAllControllers, 0);
}


OSStatus AUInstrumentBase::HandleAllNotesOff(UInt8 inChannel)
{
	return SendPedalEvent(inChannel, SynthEvent::kEventType_AllNotesOff, 0);
}


OSStatus AUInstrumentBase::HandleAllSoundOff(UInt8 inChannel)
{
	return SendPedalEvent(inChannel, SynthEvent::kEventType_AllSoundOff, 0);
}

SynthNote* AUInstrumentBase::GetAFreeNote(UInt32 inFrame)
{

	SynthNote* note = mFreeNotes.mHead;

	if (note) {
		mFreeNotes.RemoveNote(note);
		return note;
	}

	return VoiceStealing(inFrame, true);
}

SynthNote* AUInstrumentBase::VoiceStealing(UInt32 inFrame, bool inKillIt)
{

	// free list was empty so we need to kill a note.
	UInt32 startState = inKillIt ? kNoteState_FastReleased : kNoteState_Released;

	for (UInt32 i = startState; i <= startState; --i) {

		UInt32 numGroups = Groups().GetNumberOfElements();

		for (UInt32 j = 0; j < numGroups; ++j) {

			SynthGroupElement* group = (SynthGroupElement*)Groups().GetElement(j);

			if (group->mNoteList[i].NotEmpty()) {

				SynthNote* note = group->mNoteList[i].FindMostQuietNote();

				if (inKillIt) {

					note->Kill(inFrame);
					group->mNoteList[i].RemoveNote(note);

					if (i != kNoteState_FastReleased) {
						DecNumActiveNotes();
					}

					return note;

				} else {

					group->mNoteList[i].RemoveNote(note);
					note->FastRelease(inFrame);
					group->mNoteList[kNoteState_FastReleased].AddNote(note);
					DecNumActiveNotes(); // kNoteState_FastReleased counts as inactive for voice
										 // stealing purposes.
					return NULL;
				}
			}
		}
	}

	return NULL; // It should be impossible to get here. It means there were no notes to kill in any
				 // state.
}
