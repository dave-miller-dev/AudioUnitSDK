//
//  MIDIControlHandler.h
//  Synthesizer
//
//  Created by David Miller on 10/6/2023.
//

#ifndef MIDIControlHandler_h
#define MIDIControlHandler_h

#include "AUMIDIDefs.h"
#include "AudioUnitSDK/AUMidiUtility.h"

/// Abstract interface base class for classes which handle all incoming MIDI data
class MIDIControlHandler {

public:
	virtual ~MIDIControlHandler() {}
	virtual void Reset() = 0; //! Restore all state to defaults
	virtual bool SetProgramChange(UInt16 inProgram) = 0;
	virtual bool SetPitchWheel(UInt16 inValue) = 0;
	virtual bool SetChannelPressure(UInt8 inValue) = 0;
	virtual bool SetPolyPressure(UInt8 inKey, UInt8 inValue) = 0;
	virtual bool SetController(UInt8 inControllerNumber, UInt8 inValue) = 0;
	virtual bool SetSysex(void* inSysexMsg) = 0;

	virtual float GetPitchBend() const = 0;

	/*! Default controller values.  These represent MSB values unless indicated in the name */
	enum {
		kDefault_Midpoint = 0x40, //! Used for all center-null-point controllers
		kDefault_Volume = 100,
		kDefault_Pan = kDefault_Midpoint,
		kDefault_ModWheel = 0,
		kDefault_Pitch = kDefault_Midpoint,
		kDefault_Expression = 0x7f,
		kDefault_ChannelPressure = 0,
		kDefault_ReverbSend = 40,
		kDefault_ChorusSend = 0,

		kDefault_RPN_LSB = 0x7f,
		kDefault_RPN_MSB = 0x7f,
		kDefault_PitchBendRange = 2,
		kDefault_FineTuning = kDefault_Midpoint,
		kDefault_CoarseTuning = kDefault_Midpoint,
		kDefault_ModDepthRange = 0,
		kDefault_ModDepthRangeLSB = kDefault_Midpoint
	};
};


class MidiControls : public MIDIControlHandler {

	enum { kMaxControls = 128 };

public:
	MidiControls() { Reset(); };

	virtual ~MidiControls() {}

	virtual void Reset()
	{
		memset(mControls, 0, sizeof(mControls));
		memset(mPolyPressure, 0, sizeof(mPolyPressure));
		mMonoPressure = 0;
		mProgramChange = 0;
		mPitchBend = 0;
		mActiveRPN = 0;
		mActiveNRPN = 0;
		mActiveRPValue = 0;
		mActiveNRPValue = 0;
		mControls[kMidiController_Pan] = 64;
		mControls[kMidiController_Expression] = 127;
		mPitchBendDepth = 24 << 7;
		mFPitchBendDepth = 24.0f;
		mFPitchBend = 0.0f;
	};

	virtual bool SetProgramChange(UInt16 inProgram)
	{
		mProgramChange = inProgram;
		return true;
	}

	virtual bool SetPitchWheel(UInt16 inValue)
	{
		mPitchBend = inValue;
		mFPitchBend = (float)(((SInt16)mPitchBend - 8192) / 8192.);
		return true;
	}

	virtual bool SetChannelPressure(UInt8 inValue)
	{
		mMonoPressure = inValue;
		return true;
	}

	virtual bool SetPolyPressure(UInt8 inKey, UInt8 inValue)
	{
		mPolyPressure[inKey] = inValue;
		return true;
	}

	virtual bool SetController(UInt8 inControllerNumber, UInt8 inValue)
	{

		if (inControllerNumber < kMaxControls) {
			mControls[inControllerNumber] = inValue;
			return true;
		}

		return false;
	}

	virtual bool SetSysex([[maybe_unused]] void* inSysexMsg) { return false; }

	virtual float GetPitchBend() const { return mFPitchBend * mFPitchBendDepth; }

	SInt16 GetHiResControl(UInt32 inIndex) const
	{
		return ((mControls[inIndex] & 127) << 7) | (mControls[inIndex + 32] & 127);
	}

	float GetControl(UInt32 inIndex) const
	{

		if (inIndex < 32) {
			return (float)(mControls[inIndex] + (mControls[inIndex + 32] / 127.));
		} else {
			return (float)mControls[inIndex];
		}
	}


private:
	UInt8 mControls[128];
	UInt8 mPolyPressure[128];
	UInt8 mMonoPressure;
	UInt8 mProgramChange;
	UInt16 mPitchBend;
	UInt16 mActiveRPN;
	UInt16 mActiveNRPN;
	UInt16 mActiveRPValue;
	UInt16 mActiveNRPValue;

	UInt16 mPitchBendDepth;
	float mFPitchBendDepth;
	float mFPitchBend;

	void SetHiResControl(UInt32 inIndex, UInt8 inMSB, UInt8 inLSB)
	{
		mControls[inIndex] = inMSB;
		mControls[inIndex + 32] = inLSB;
	}
};

#endif /* MIDIControlHandler_h */
