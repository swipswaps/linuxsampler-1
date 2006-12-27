/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005, 2006 Christian Schoenebeck                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#include "EngineChannel.h"

namespace LinuxSampler { namespace gig {

    EngineChannel::EngineChannel() {
        pMIDIKeyInfo = new midi_key_info_t[128];
        pEngine      = NULL;
        pInstrument  = NULL;
        pEvents      = NULL; // we allocate when we retrieve the right Engine object
        pEventQueue  = new RingBuffer<Event,false>(CONFIG_MAX_EVENTS_PER_FRAGMENT, 0);
        pActiveKeys  = new Pool<uint>(128);
        for (uint i = 0; i < 128; i++) {
            pMIDIKeyInfo[i].pActiveVoices  = NULL; // we allocate when we retrieve the right Engine object
            pMIDIKeyInfo[i].KeyPressed     = false;
            pMIDIKeyInfo[i].Active         = false;
            pMIDIKeyInfo[i].ReleaseTrigger = false;
            pMIDIKeyInfo[i].pEvents        = NULL; // we allocate when we retrieve the right Engine object
            pMIDIKeyInfo[i].VoiceTheftsQueued = 0;
            pMIDIKeyInfo[i].RoundRobinIndex = 0;
        }
        InstrumentIdx  = -1;
        InstrumentStat = -1;
        pChannelLeft  = NULL;
        pChannelRight = NULL;
        AudioDeviceChannelLeft  = -1;
        AudioDeviceChannelRight = -1;
        pMidiInputPort = NULL;
        midiChannel = midi_chan_all;
        ResetControllers();
        SoloMode       = false;
        PortamentoMode = false;
        PortamentoTime = CONFIG_PORTAMENTO_TIME_DEFAULT;
    }

    EngineChannel::~EngineChannel() {
        DisconnectAudioOutputDevice();
        if (pInstrument) Engine::instruments.HandBack(pInstrument, this);
        if (pEventQueue) delete pEventQueue;
        if (pActiveKeys) delete pActiveKeys;
        if (pMIDIKeyInfo) delete[] pMIDIKeyInfo;
        RemoveAllFxSends();
    }

    /**
     * Implementation of virtual method from abstract EngineChannel interface.
     * This method will periodically be polled (e.g. by the LSCP server) to
     * check if some engine channel parameter has changed since the last
     * StatusChanged() call.
     *
     * This method can also be used to mark the engine channel as changed
     * from outside, e.g. by a MIDI input device. The optional argument
     * \a nNewStatus can be used for this.
     *
     * TODO: This "poll method" is just a lazy solution and might be
     *       replaced in future.
     * @param bNewStatus - (optional, default: false) sets the new status flag
     * @returns true if engine channel status has changed since last
     *          StatusChanged() call
     */
    bool EngineChannel::StatusChanged(bool bNewStatus) {
        bool b = bStatusChanged;
        bStatusChanged = bNewStatus;
        return b;
    }

    void EngineChannel::Reset() {
        if (pEngine) pEngine->DisableAndLock();
        ResetInternal();
        ResetControllers();
        if (pEngine) {
            pEngine->Enable();
            pEngine->Reset();
        }
    }

    /**
     * This method is not thread safe!
     */
    void EngineChannel::ResetInternal() {
        CurrentKeyDimension = 0;

        // reset key info
        for (uint i = 0; i < 128; i++) {
            if (pMIDIKeyInfo[i].pActiveVoices)
                pMIDIKeyInfo[i].pActiveVoices->clear();
            if (pMIDIKeyInfo[i].pEvents)
                pMIDIKeyInfo[i].pEvents->clear();
            pMIDIKeyInfo[i].KeyPressed     = false;
            pMIDIKeyInfo[i].Active         = false;
            pMIDIKeyInfo[i].ReleaseTrigger = false;
            pMIDIKeyInfo[i].itSelf         = Pool<uint>::Iterator();
            pMIDIKeyInfo[i].VoiceTheftsQueued = 0;
        }
        SoloKey       = -1;    // no solo key active yet
        PortamentoPos = -1.0f; // no portamento active yet

        // reset all key groups
        std::map<uint,uint*>::iterator iter = ActiveKeyGroups.begin();
        for (; iter != ActiveKeyGroups.end(); iter++) iter->second = NULL;

        // free all active keys
        pActiveKeys->clear();

        // delete all input events
        pEventQueue->init();

        if (pEngine) pEngine->ResetInternal();

        // status of engine channel has changed, so set notify flag
        bStatusChanged = true;
    }

    LinuxSampler::Engine* EngineChannel::GetEngine() {
        return pEngine;
    }

    /**
     * More or less a workaround to set the instrument name, index and load
     * status variable to zero percent immediately, that is without blocking
     * the calling thread. It might be used in future for other preparations
     * as well though.
     *
     * @param FileName   - file name of the Gigasampler instrument file
     * @param Instrument - index of the instrument in the .gig file
     * @see LoadInstrument()
     */
    void EngineChannel::PrepareLoadInstrument(const char* FileName, uint Instrument) {
        InstrumentFile = FileName;
        InstrumentIdx  = Instrument;
        InstrumentStat = 0;
    }

    /**
     * Load an instrument from a .gig file. PrepareLoadInstrument() has to
     * be called first to provide the information which instrument to load.
     * This method will then actually start to load the instrument and block
     * the calling thread until loading was completed.
     *
     * @returns detailed description of the method call result
     * @see PrepareLoadInstrument()
     */
    void EngineChannel::LoadInstrument() {

        if (pEngine) pEngine->DisableAndLock();

        ResetInternal();

        // free old instrument
        if (pInstrument) {
            // give old instrument back to instrument manager
            Engine::instruments.HandBack(pInstrument, this);
        }

        // delete all key groups
        ActiveKeyGroups.clear();

        // request gig instrument from instrument manager
        try {
            InstrumentManager::instrument_id_t instrid;
            instrid.FileName  = InstrumentFile;
            instrid.Index     = InstrumentIdx;
            pInstrument = Engine::instruments.Borrow(instrid, this);
            if (!pInstrument) {
                InstrumentStat = -1;
                dmsg(1,("no instrument loaded!!!\n"));
                exit(EXIT_FAILURE);
            }
        }
        catch (RIFF::Exception e) {
            InstrumentStat = -2;
            String msg = "gig::Engine error: Failed to load instrument, cause: " + e.Message;
            throw Exception(msg);
        }
        catch (InstrumentResourceManagerException e) {
            InstrumentStat = -3;
            String msg = "gig::Engine error: Failed to load instrument, cause: " + e.Message();
            throw Exception(msg);
        }
        catch (...) {
            InstrumentStat = -4;
            throw Exception("gig::Engine error: Failed to load instrument, cause: Unknown exception while trying to parse gig file.");
        }

        // rebuild ActiveKeyGroups map with key groups of current instrument
        for (::gig::Region* pRegion = pInstrument->GetFirstRegion(); pRegion; pRegion = pInstrument->GetNextRegion())
            if (pRegion->KeyGroup) ActiveKeyGroups[pRegion->KeyGroup] = NULL;

        InstrumentIdxName = pInstrument->pInfo->Name;
        InstrumentStat = 100;

        // inform audio driver for the need of two channels
        try {
            if (pEngine && pEngine->pAudioOutputDevice)
                pEngine->pAudioOutputDevice->AcquireChannels(2); // gig Engine only stereo
        }
        catch (AudioOutputException e) {
            String msg = "Audio output device unable to provide 2 audio channels, cause: " + e.Message();
            throw Exception(msg);
        }

        if (pEngine) pEngine->Enable();
    }

    /**
     * Will be called by the InstrumentResourceManager when the instrument
     * we are currently using on this EngineChannel is going to be updated,
     * so we can stop playback before that happens.
     */
    void EngineChannel::ResourceToBeUpdated(::gig::Instrument* pResource, void*& pUpdateArg) {
        dmsg(3,("gig::Engine: Received instrument update message.\n"));
        if (pEngine) pEngine->DisableAndLock();
        ResetInternal();
        this->pInstrument = NULL;
    }

    /**
     * Will be called by the InstrumentResourceManager when the instrument
     * update process was completed, so we can continue with playback.
     */
    void EngineChannel::ResourceUpdated(::gig::Instrument* pOldResource, ::gig::Instrument* pNewResource, void* pUpdateArg) {
        this->pInstrument = pNewResource; //TODO: there are couple of engine parameters we should update here as well if the instrument was updated (see LoadInstrument())
        if (pEngine) pEngine->Enable();
        bStatusChanged = true; // status of engine has changed, so set notify flag
    }

    /**
     * Will be called by the InstrumentResourceManager on progress changes
     * while loading or realoading an instrument for this EngineChannel.
     *
     * @param fProgress - current progress as value between 0.0 and 1.0
     */
    void EngineChannel::OnResourceProgress(float fProgress) {
        this->InstrumentStat = int(fProgress * 100.0f);
        dmsg(7,("gig::EngineChannel: progress %d%", InstrumentStat));
        bStatusChanged = true; // status of engine has changed, so set notify flag
    }

    void EngineChannel::Connect(AudioOutputDevice* pAudioOut) {
        if (pEngine) {
            if (pEngine->pAudioOutputDevice == pAudioOut) return;
            DisconnectAudioOutputDevice();
        }
        pEngine = Engine::AcquireEngine(this, pAudioOut);
        ResetInternal();
        pEvents = new RTList<Event>(pEngine->pEventPool);
        for (uint i = 0; i < 128; i++) {
            pMIDIKeyInfo[i].pActiveVoices = new RTList<Voice>(pEngine->pVoicePool);
            pMIDIKeyInfo[i].pEvents       = new RTList<Event>(pEngine->pEventPool);
        }
        AudioDeviceChannelLeft  = 0;
        AudioDeviceChannelRight = 1;
        if (fxSends.empty()) { // render directly into the AudioDevice's output buffers
            pChannelLeft  = pAudioOut->Channel(AudioDeviceChannelLeft);
            pChannelRight = pAudioOut->Channel(AudioDeviceChannelRight);
        } else { // use local buffers for rendering and copy later
            // ensure the local buffers have the correct size
            if (pChannelLeft)  delete pChannelLeft;
            if (pChannelRight) delete pChannelRight;
            pChannelLeft  = new AudioChannel(0, pAudioOut->MaxSamplesPerCycle());
            pChannelRight = new AudioChannel(1, pAudioOut->MaxSamplesPerCycle());
        }
        MidiInputPort::AddSysexListener(pEngine);
    }

    void EngineChannel::DisconnectAudioOutputDevice() {
        if (pEngine) { // if clause to prevent disconnect loops
            ResetInternal();
            if (pEvents) {
                delete pEvents;
                pEvents = NULL;
            }
            for (uint i = 0; i < 128; i++) {
                if (pMIDIKeyInfo[i].pActiveVoices) {
                    delete pMIDIKeyInfo[i].pActiveVoices;
                    pMIDIKeyInfo[i].pActiveVoices = NULL;
                }
                if (pMIDIKeyInfo[i].pEvents) {
                    delete pMIDIKeyInfo[i].pEvents;
                    pMIDIKeyInfo[i].pEvents = NULL;
                }
            }
            Engine* oldEngine = pEngine;
            AudioOutputDevice* oldAudioDevice = pEngine->pAudioOutputDevice;
            pEngine = NULL;
            Engine::FreeEngine(this, oldAudioDevice);
            AudioDeviceChannelLeft  = -1;
            AudioDeviceChannelRight = -1;
            if (!fxSends.empty()) { // free the local rendering buffers
                if (pChannelLeft)  delete pChannelLeft;
                if (pChannelRight) delete pChannelRight;
            }
            pChannelLeft  = NULL;
            pChannelRight = NULL;
        }
    }

    AudioOutputDevice* EngineChannel::GetAudioOutputDevice() {
        return (pEngine) ? pEngine->pAudioOutputDevice : NULL;
    }

    void EngineChannel::SetOutputChannel(uint EngineAudioChannel, uint AudioDeviceChannel) {
        if (!pEngine || !pEngine->pAudioOutputDevice) throw AudioOutputException("No audio output device connected yet.");

        AudioChannel* pChannel = pEngine->pAudioOutputDevice->Channel(AudioDeviceChannel);
        if (!pChannel) throw AudioOutputException("Invalid audio output device channel " + ToString(AudioDeviceChannel));
        switch (EngineAudioChannel) {
            case 0: // left output channel
                if (fxSends.empty()) pChannelLeft = pChannel;
                AudioDeviceChannelLeft = AudioDeviceChannel;
                break;
            case 1: // right output channel
                if (fxSends.empty()) pChannelRight = pChannel;
                AudioDeviceChannelRight = AudioDeviceChannel;
                break;
            default:
                throw AudioOutputException("Invalid engine audio channel " + ToString(EngineAudioChannel));
        }
    }

    int EngineChannel::OutputChannel(uint EngineAudioChannel) {
        switch (EngineAudioChannel) {
            case 0: // left channel
                return AudioDeviceChannelLeft;
            case 1: // right channel
                return AudioDeviceChannelRight;
            default:
                throw AudioOutputException("Invalid engine audio channel " + ToString(EngineAudioChannel));
        }
    }

    void EngineChannel::Connect(MidiInputPort* pMidiPort, midi_chan_t MidiChannel) {
        if (!pMidiPort || pMidiPort == this->pMidiInputPort) return;
        DisconnectMidiInputPort();
        this->pMidiInputPort = pMidiPort;
        this->midiChannel    = MidiChannel;
        pMidiPort->Connect(this, MidiChannel);
    }

    void EngineChannel::DisconnectMidiInputPort() {
        MidiInputPort* pOldPort = this->pMidiInputPort;
        this->pMidiInputPort = NULL;
        if (pOldPort) pOldPort->Disconnect(this);
    }

    MidiInputPort* EngineChannel::GetMidiInputPort() {
        return pMidiInputPort;
    }

    midi_chan_t EngineChannel::MidiChannel() {
        return midiChannel;
    }

    FxSend* EngineChannel::AddFxSend(uint8_t MidiCtrl, String Name) throw (Exception) {
        if (pEngine) pEngine->DisableAndLock();
        FxSend* pFxSend = new FxSend(this, MidiCtrl, Name);
        if (fxSends.empty()) {
            if (pEngine && pEngine->pAudioOutputDevice) {
                AudioOutputDevice* pDevice = pEngine->pAudioOutputDevice;
                // create local render buffers
                pChannelLeft  = new AudioChannel(0, pDevice->MaxSamplesPerCycle());
                pChannelRight = new AudioChannel(1, pDevice->MaxSamplesPerCycle());
            } else {
                // postpone local render buffer creation until audio device is assigned
                pChannelLeft  = NULL;
                pChannelRight = NULL;
            }
        }
        fxSends.push_back(pFxSend);
        if (pEngine) pEngine->Enable();
        return pFxSend;
    }

    FxSend* EngineChannel::GetFxSend(uint FxSendIndex) {
        return (FxSendIndex < fxSends.size()) ? fxSends[FxSendIndex] : NULL;
    }

    uint EngineChannel::GetFxSendCount() {
        return fxSends.size();
    }

    void EngineChannel::RemoveFxSend(FxSend* pFxSend) {
        if (pEngine) pEngine->DisableAndLock();
        for (
            std::vector<FxSend*>::iterator iter = fxSends.begin();
            iter != fxSends.end(); iter++
        ) {
            if (*iter == pFxSend) {
                delete pFxSend;
                fxSends.erase(iter);
                if (fxSends.empty()) {
                    // destroy local render buffers
                    if (pChannelLeft)  delete pChannelLeft;
                    if (pChannelRight) delete pChannelRight;
                    // fallback to render directly into AudioOutputDevice's buffers
                    if (pEngine && pEngine->pAudioOutputDevice) {
                        pChannelLeft  = pEngine->pAudioOutputDevice->Channel(AudioDeviceChannelLeft);
                        pChannelRight = pEngine->pAudioOutputDevice->Channel(AudioDeviceChannelRight);
                    } else { // we update the pointers later
                        pChannelLeft  = NULL;
                        pChannelRight = NULL;
                    }
                }
                break;
            }
        }
        if (pEngine) pEngine->Enable();
    }

    /**
     *  Will be called by the MIDIIn Thread to let the audio thread trigger a new
     *  voice for the given key. This method is meant for real time rendering,
     *  that is an event will immediately be created with the current system
     *  time as time stamp.
     *
     *  @param Key      - MIDI key number of the triggered key
     *  @param Velocity - MIDI velocity value of the triggered key
     */
    void EngineChannel::SendNoteOn(uint8_t Key, uint8_t Velocity) {
        if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent();
            event.Type                = Event::type_note_on;
            event.Param.Note.Key      = Key;
            event.Param.Note.Velocity = Velocity;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to let the audio thread trigger a new
     *  voice for the given key. This method is meant for offline rendering
     *  and / or for cases where the exact position of the event in the current
     *  audio fragment is already known.
     *
     *  @param Key         - MIDI key number of the triggered key
     *  @param Velocity    - MIDI velocity value of the triggered key
     *  @param FragmentPos - sample point position in the current audio
     *                       fragment to which this event belongs to
     */
    void EngineChannel::SendNoteOn(uint8_t Key, uint8_t Velocity, int32_t FragmentPos) {
        if (FragmentPos < 0) {
            dmsg(1,("EngineChannel::SendNoteOn(): negative FragmentPos! Seems MIDI driver is buggy!"));
        }
        else if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent(FragmentPos);
            event.Type                = Event::type_note_on;
            event.Param.Note.Key      = Key;
            event.Param.Note.Velocity = Velocity;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to release
     *  voice(s) on the given key. This method is meant for real time rendering,
     *  that is an event will immediately be created with the current system
     *  time as time stamp.
     *
     *  @param Key      - MIDI key number of the released key
     *  @param Velocity - MIDI release velocity value of the released key
     */
    void EngineChannel::SendNoteOff(uint8_t Key, uint8_t Velocity) {
        if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent();
            event.Type                = Event::type_note_off;
            event.Param.Note.Key      = Key;
            event.Param.Note.Velocity = Velocity;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to release
     *  voice(s) on the given key. This method is meant for offline rendering
     *  and / or for cases where the exact position of the event in the current
     *  audio fragment is already known.
     *
     *  @param Key         - MIDI key number of the released key
     *  @param Velocity    - MIDI release velocity value of the released key
     *  @param FragmentPos - sample point position in the current audio
     *                       fragment to which this event belongs to
     */
    void EngineChannel::SendNoteOff(uint8_t Key, uint8_t Velocity, int32_t FragmentPos) {
        if (FragmentPos < 0) {
            dmsg(1,("EngineChannel::SendNoteOff(): negative FragmentPos! Seems MIDI driver is buggy!"));
        }
        else if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent(FragmentPos);
            event.Type                = Event::type_note_off;
            event.Param.Note.Key      = Key;
            event.Param.Note.Velocity = Velocity;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to change
     *  the pitch value for all voices. This method is meant for real time
     *  rendering, that is an event will immediately be created with the
     *  current system time as time stamp.
     *
     *  @param Pitch - MIDI pitch value (-8192 ... +8191)
     */
    void EngineChannel::SendPitchbend(int Pitch) {
        if (pEngine) {
            Event event             = pEngine->pEventGenerator->CreateEvent();
            event.Type              = Event::type_pitchbend;
            event.Param.Pitch.Pitch = Pitch;
            event.pEngineChannel    = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to change
     *  the pitch value for all voices. This method is meant for offline
     *  rendering and / or for cases where the exact position of the event in
     *  the current audio fragment is already known.
     *
     *  @param Pitch       - MIDI pitch value (-8192 ... +8191)
     *  @param FragmentPos - sample point position in the current audio
     *                       fragment to which this event belongs to
     */
    void EngineChannel::SendPitchbend(int Pitch, int32_t FragmentPos) {
        if (FragmentPos < 0) {
            dmsg(1,("EngineChannel::SendPitchBend(): negative FragmentPos! Seems MIDI driver is buggy!"));
        }
        else if (pEngine) {
            Event event             = pEngine->pEventGenerator->CreateEvent(FragmentPos);
            event.Type              = Event::type_pitchbend;
            event.Param.Pitch.Pitch = Pitch;
            event.pEngineChannel    = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread that a
     *  continuous controller value has changed. This method is meant for real
     *  time rendering, that is an event will immediately be created with the
     *  current system time as time stamp.
     *
     *  @param Controller - MIDI controller number of the occured control change
     *  @param Value      - value of the control change
     */
    void EngineChannel::SendControlChange(uint8_t Controller, uint8_t Value) {
        if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent();
            event.Type                = Event::type_control_change;
            event.Param.CC.Controller = Controller;
            event.Param.CC.Value      = Value;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread that a
     *  continuous controller value has changed. This method is meant for
     *  offline rendering and / or for cases where the exact position of the
     *  event in the current audio fragment is already known.
     *
     *  @param Controller  - MIDI controller number of the occured control change
     *  @param Value       - value of the control change
     *  @param FragmentPos - sample point position in the current audio
     *                       fragment to which this event belongs to
     */
    void EngineChannel::SendControlChange(uint8_t Controller, uint8_t Value, int32_t FragmentPos) {
        if (FragmentPos < 0) {
            dmsg(1,("EngineChannel::SendControlChange(): negative FragmentPos! Seems MIDI driver is buggy!"));
        }
        else if (pEngine) {
            Event event               = pEngine->pEventGenerator->CreateEvent(FragmentPos);
            event.Type                = Event::type_control_change;
            event.Param.CC.Controller = Controller;
            event.Param.CC.Value      = Value;
            event.pEngineChannel      = this;
            if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
            else dmsg(1,("EngineChannel: Input event queue full!"));
        }
    }

    void EngineChannel::ClearEventLists() {
        pEvents->clear();
        // empty MIDI key specific event lists
        {
            RTList<uint>::Iterator iuiKey = pActiveKeys->first();
            RTList<uint>::Iterator end    = pActiveKeys->end();
            for(; iuiKey != end; ++iuiKey) {
                pMIDIKeyInfo[*iuiKey].pEvents->clear(); // free all events on the key
            }
        }
    }

    void EngineChannel::ResetControllers() {
        Pitch          = 0;
        SustainPedal   = false;
        SostenutoPedal = false;
        GlobalVolume   = CONFIG_GLOBAL_ATTENUATION;
        MidiVolume     = 1.0;
        GlobalPanLeft  = 1.0f;
        GlobalPanRight = 1.0f;
        // set all MIDI controller values to zero
        memset(ControllerTable, 0x00, 129);
    }

    /**
     * Copy all events from the engine channel's input event queue buffer to
     * the internal event list. This will be done at the beginning of each
     * audio cycle (that is each RenderAudio() call) to distinguish all
     * events which have to be processed in the current audio cycle. Each
     * EngineChannel has it's own input event queue for the common channel
     * specific events (like NoteOn, NoteOff and ControlChange events).
     * Beside that, the engine also has a input event queue for global
     * events (usually SysEx messages).
     *
     * @param Samples - number of sample points to be processed in the
     *                  current audio cycle
     */
    void EngineChannel::ImportEvents(uint Samples) {
        RingBuffer<Event,false>::NonVolatileReader eventQueueReader = pEventQueue->get_non_volatile_reader();
        Event* pEvent;
        while (true) {
            // get next event from input event queue
            if (!(pEvent = eventQueueReader.pop())) break;
            // if younger event reached, ignore that and all subsequent ones for now
            if (pEvent->FragmentPos() >= Samples) {
                eventQueueReader--;
                dmsg(2,("Younger Event, pos=%d ,Samples=%d!\n",pEvent->FragmentPos(),Samples));
                pEvent->ResetFragmentPos();
                break;
            }
            // copy event to internal event list
            if (pEvents->poolIsEmpty()) {
                dmsg(1,("Event pool emtpy!\n"));
                break;
            }
            *pEvents->allocAppend() = *pEvent;
        }
        eventQueueReader.free(); // free all copied events from input queue
    }

    void EngineChannel::RemoveAllFxSends() {
        if (pEngine) pEngine->DisableAndLock();
        if (!fxSends.empty()) { // free local render buffers
            if (pChannelLeft) {
                delete pChannelLeft;
                if (pEngine && pEngine->pAudioOutputDevice) {
                    // fallback to render directly to the AudioOutputDevice's buffer
                    pChannelLeft = pEngine->pAudioOutputDevice->Channel(AudioDeviceChannelLeft);
                } else pChannelLeft = NULL;
            }
            if (pChannelRight) {
                delete pChannelRight;
                if (pEngine && pEngine->pAudioOutputDevice) {
                    // fallback to render directly to the AudioOutputDevice's buffer
                    pChannelRight = pEngine->pAudioOutputDevice->Channel(AudioDeviceChannelRight);
                } else pChannelRight = NULL; 
            }
        }
        for (int i = 0; i < fxSends.size(); i++) delete fxSends[i];
        fxSends.clear();
        if (pEngine) pEngine->Enable();
    }

    float EngineChannel::Volume() {
        return GlobalVolume;
    }

    void EngineChannel::Volume(float f) {
        GlobalVolume = f;
        bStatusChanged = true; // status of engine channel has changed, so set notify flag
    }

    uint EngineChannel::Channels() {
        return 2;
    }

    String EngineChannel::InstrumentFileName() {
        return InstrumentFile;
    }

    String EngineChannel::InstrumentName() {
        return InstrumentIdxName;
    }

    int EngineChannel::InstrumentIndex() {
        return InstrumentIdx;
    }

    int EngineChannel::InstrumentStatus() {
        return InstrumentStat;
    }

    String EngineChannel::EngineName() {
        return LS_GIG_ENGINE_NAME;
    }

}} // namespace LinuxSampler::gig
