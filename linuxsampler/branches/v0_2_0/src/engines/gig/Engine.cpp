/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
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

#include <sstream>
#include "DiskThread.h"
#include "Voice.h"
#include "EGADSR.h"

#include "Engine.h"

#if defined(__APPLE__)
# include <stdlib.h>
#else
# include <malloc.h>
#endif

namespace LinuxSampler { namespace gig {

    InstrumentResourceManager Engine::Instruments;

    Engine::Engine() {
        pRIFF              = NULL;
        pGig               = NULL;
        pInstrument        = NULL;
        pAudioOutputDevice = NULL;
        pDiskThread        = NULL;
        pEventGenerator    = NULL;
        pSysexBuffer       = new RingBuffer<uint8_t>(SYSEX_BUFFER_SIZE, 0);
        pEventQueue        = new RingBuffer<Event>(MAX_EVENTS_PER_FRAGMENT, 0);
        pEventPool         = new Pool<Event>(MAX_EVENTS_PER_FRAGMENT);
        pVoicePool         = new Pool<Voice>(MAX_AUDIO_VOICES);
        pActiveKeys        = new Pool<uint>(128);
        pVoiceStealingQueue = new RTList<Event>(pEventPool);
        pEvents            = new RTList<Event>(pEventPool);
        pCCEvents          = new RTList<Event>(pEventPool);
        for (uint i = 0; i < Event::destination_count; i++) {
            pSynthesisEvents[i] = new RTList<Event>(pEventPool);
        }
        for (uint i = 0; i < 128; i++) {
            pMIDIKeyInfo[i].pActiveVoices  = new RTList<Voice>(pVoicePool);
            pMIDIKeyInfo[i].KeyPressed     = false;
            pMIDIKeyInfo[i].Active         = false;
            pMIDIKeyInfo[i].ReleaseTrigger = false;
            pMIDIKeyInfo[i].pEvents        = new RTList<Event>(pEventPool);
        }
        for (RTList<Voice>::Iterator iterVoice = pVoicePool->allocAppend(); iterVoice == pVoicePool->last(); iterVoice = pVoicePool->allocAppend()) {
            iterVoice->SetEngine(this);
        }
        pVoicePool->clear();

        pSynthesisParameters[0] = NULL; // we allocate when an audio device is connected
        pBasicFilterParameters  = NULL;
        pMainFilterParameters   = NULL;

	InstrumentIdx = -1;
	InstrumentStat = -1;

        AudioDeviceChannelLeft  = -1;
        AudioDeviceChannelRight = -1;

        ResetInternal();
    }

    Engine::~Engine() {
        if (pDiskThread) {
            dmsg(1,("Stopping disk thread..."));
            pDiskThread->StopThread();
            delete pDiskThread;
            dmsg(1,("OK\n"));
        }

        if (pInstrument) Instruments.HandBack(pInstrument, this);

        if (pGig)  delete pGig;
        if (pRIFF) delete pRIFF;
        for (uint i = 0; i < 128; i++) {
            if (pMIDIKeyInfo[i].pActiveVoices) delete pMIDIKeyInfo[i].pActiveVoices;
            if (pMIDIKeyInfo[i].pEvents)       delete pMIDIKeyInfo[i].pEvents;
        }
        for (uint i = 0; i < Event::destination_count; i++) {
            if (pSynthesisEvents[i]) delete pSynthesisEvents[i];
        }
        if (pEvents)     delete pEvents;
        if (pCCEvents)   delete pCCEvents;
        if (pEventQueue) delete pEventQueue;
        if (pEventPool)  delete pEventPool;
	if (pVoicePool) {
		pVoicePool->clear();
		delete pVoicePool;
	}
        if (pActiveKeys) delete pActiveKeys;
        if (pSysexBuffer) delete pSysexBuffer;
        if (pEventGenerator) delete pEventGenerator;
        if (pMainFilterParameters) delete[] pMainFilterParameters;
        if (pBasicFilterParameters) delete[] pBasicFilterParameters;
        if (pSynthesisParameters[0]) free(pSynthesisParameters[0]);
        if (pVoiceStealingQueue) delete pVoiceStealingQueue;
    }

    void Engine::Enable() {
        dmsg(3,("gig::Engine: enabling\n"));
        EngineDisabled.PushAndUnlock(false, 2); // set condition object 'EngineDisabled' to false (wait max. 2s)
        dmsg(3,("gig::Engine: enabled (val=%d)\n", EngineDisabled.GetUnsafe()));
    }

    void Engine::Disable() {
        dmsg(3,("gig::Engine: disabling\n"));
        bool* pWasDisabled = EngineDisabled.PushAndUnlock(true, 2); // wait max. 2s
        if (!pWasDisabled) dmsg(3,("gig::Engine warning: Timeout waiting to disable engine.\n"));
    }

    void Engine::DisableAndLock() {
        dmsg(3,("gig::Engine: disabling\n"));
        bool* pWasDisabled = EngineDisabled.Push(true, 2); // wait max. 2s
        if (!pWasDisabled) dmsg(3,("gig::Engine warning: Timeout waiting to disable engine.\n"));
    }

    /**
     *  Reset all voices and disk thread and clear input event queue and all
     *  control and status variables.
     */
    void Engine::Reset() {
        DisableAndLock();

        //if (pAudioOutputDevice->IsPlaying()) { // if already running
            /*
            // signal audio thread not to enter render part anymore
            SuspensionRequested = true;
            // sleep until wakened by audio thread
            pthread_mutex_lock(&__render_state_mutex);
            pthread_cond_wait(&__render_exit_condition, &__render_state_mutex);
            pthread_mutex_unlock(&__render_state_mutex);
            */
        //}

        //if (wasplaying) pAudioOutputDevice->Stop();

        ResetInternal();

        // signal audio thread to continue with rendering
        //SuspensionRequested = false;
        Enable();
    }

    /**
     *  Reset all voices and disk thread and clear input event queue and all
     *  control and status variables. This method is not thread safe!
     */
    void Engine::ResetInternal() {
        Pitch               = 0;
        SustainPedal        = false;
        ActiveVoiceCount    = 0;
        ActiveVoiceCountMax = 0;
        GlobalVolume        = 1.0;
        CurrentKeyDimension = 0;

        // reset voice stealing parameters
        itLastStolenVoice = RTList<Voice>::Iterator();
        iuiLastStolenKey  = RTList<uint>::Iterator();
        pVoiceStealingQueue->clear();

        // reset to normal chromatic scale (means equal temper)
        memset(&ScaleTuning[0], 0x00, 12);

        // set all MIDI controller values to zero
        memset(ControllerTable, 0x00, 128);

        // reset key info
        for (uint i = 0; i < 128; i++) {
            pMIDIKeyInfo[i].pActiveVoices->clear();
            pMIDIKeyInfo[i].pEvents->clear();
            pMIDIKeyInfo[i].KeyPressed     = false;
            pMIDIKeyInfo[i].Active         = false;
            pMIDIKeyInfo[i].ReleaseTrigger = false;
            pMIDIKeyInfo[i].itSelf         = Pool<uint>::Iterator();
        }

        // reset all key groups
        map<uint,uint*>::iterator iter = ActiveKeyGroups.begin();
        for (; iter != ActiveKeyGroups.end(); iter++) iter->second = NULL;

        // reset all voices
        for (RTList<Voice>::Iterator iterVoice = pVoicePool->allocAppend(); iterVoice == pVoicePool->last(); iterVoice = pVoicePool->allocAppend()) {
            iterVoice->Reset();
        }
        pVoicePool->clear();

        // free all active keys
        pActiveKeys->clear();

        // reset disk thread
        if (pDiskThread) pDiskThread->Reset();

        // delete all input events
        pEventQueue->init();
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
    void Engine::PrepareLoadInstrument(const char* FileName, uint Instrument) {
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
    void Engine::LoadInstrument() {

        DisableAndLock();

        ResetInternal(); // reset engine

        // free old instrument
        if (pInstrument) {
            // give old instrument back to instrument manager
            Instruments.HandBack(pInstrument, this);
        }

        // delete all key groups
        ActiveKeyGroups.clear();

        // request gig instrument from instrument manager
        try {
            instrument_id_t instrid;
            instrid.FileName    = InstrumentFile;
            instrid.iInstrument = InstrumentIdx;
            pInstrument = Instruments.Borrow(instrid, this);
            if (!pInstrument) {
                InstrumentStat = -1;
                dmsg(1,("no instrument loaded!!!\n"));
                exit(EXIT_FAILURE);
            }
        }
        catch (RIFF::Exception e) {
            InstrumentStat = -2;
            String msg = "gig::Engine error: Failed to load instrument, cause: " + e.Message;
            throw LinuxSamplerException(msg);
        }
        catch (InstrumentResourceManagerException e) {
            InstrumentStat = -3;
            String msg = "gig::Engine error: Failed to load instrument, cause: " + e.Message();
            throw LinuxSamplerException(msg);
        }
        catch (...) {
            InstrumentStat = -4;
            throw LinuxSamplerException("gig::Engine error: Failed to load instrument, cause: Unknown exception while trying to parse gig file.");
        }

        // rebuild ActiveKeyGroups map with key groups of current instrument
        for (::gig::Region* pRegion = pInstrument->GetFirstRegion(); pRegion; pRegion = pInstrument->GetNextRegion())
            if (pRegion->KeyGroup) ActiveKeyGroups[pRegion->KeyGroup] = NULL;

	InstrumentIdxName = pInstrument->pInfo->Name;
	InstrumentStat = 100;

        // inform audio driver for the need of two channels
        try {
            if (pAudioOutputDevice) pAudioOutputDevice->AcquireChannels(2); // gig Engine only stereo
        }
        catch (AudioOutputException e) {
            String msg = "Audio output device unable to provide 2 audio channels, cause: " + e.Message();
            throw LinuxSamplerException(msg);
        }

        Enable();
    }

    /**
     * Will be called by the InstrumentResourceManager when the instrument
     * we are currently using in this engine is going to be updated, so we
     * can stop playback before that happens.
     */
    void Engine::ResourceToBeUpdated(::gig::Instrument* pResource, void*& pUpdateArg) {
        dmsg(3,("gig::Engine: Received instrument update message.\n"));
        DisableAndLock();
        ResetInternal();
        this->pInstrument = NULL;
    }

    /**
     * Will be called by the InstrumentResourceManager when the instrument
     * update process was completed, so we can continue with playback.
     */
    void Engine::ResourceUpdated(::gig::Instrument* pOldResource, ::gig::Instrument* pNewResource, void* pUpdateArg) {
        this->pInstrument = pNewResource; //TODO: there are couple of engine parameters we should update here as well if the instrument was updated (see LoadInstrument())
        Enable();
    }

    void Engine::Connect(AudioOutputDevice* pAudioOut) {
        pAudioOutputDevice = pAudioOut;

        ResetInternal();

        // inform audio driver for the need of two channels
        try {
            pAudioOutputDevice->AcquireChannels(2); // gig engine only stereo
        }
        catch (AudioOutputException e) {
            String msg = "Audio output device unable to provide 2 audio channels, cause: " + e.Message();
            throw LinuxSamplerException(msg);
        }

        this->AudioDeviceChannelLeft  = 0;
        this->AudioDeviceChannelRight = 1;
        this->pOutputLeft             = pAudioOutputDevice->Channel(0)->Buffer();
        this->pOutputRight            = pAudioOutputDevice->Channel(1)->Buffer();
        this->MaxSamplesPerCycle      = pAudioOutputDevice->MaxSamplesPerCycle();
        this->SampleRate              = pAudioOutputDevice->SampleRate();

        // FIXME: audio drivers with varying fragment sizes might be a problem here
        MaxFadeOutPos = MaxSamplesPerCycle - int(double(SampleRate) * EG_MIN_RELEASE_TIME) - 1;
        if (MaxFadeOutPos < 0)
            throw LinuxSamplerException("EG_MIN_RELEASE_TIME in EGADSR.h to big for current audio fragment size / sampling rate!");

        // (re)create disk thread
        if (this->pDiskThread) {
            dmsg(1,("Stopping disk thread..."));
            this->pDiskThread->StopThread();
            delete this->pDiskThread;
            dmsg(1,("OK\n"));
        }
        this->pDiskThread = new DiskThread(((pAudioOut->MaxSamplesPerCycle() << MAX_PITCH) << 1) + 6); //FIXME: assuming stereo
        if (!pDiskThread) {
            dmsg(0,("gig::Engine  new diskthread = NULL\n"));
            exit(EXIT_FAILURE);
        }

        for (RTList<Voice>::Iterator iterVoice = pVoicePool->allocAppend(); iterVoice == pVoicePool->last(); iterVoice = pVoicePool->allocAppend()) {
            iterVoice->pDiskThread = this->pDiskThread;
            dmsg(3,("d"));
        }
        pVoicePool->clear();

        // (re)create event generator
        if (pEventGenerator) delete pEventGenerator;
        pEventGenerator = new EventGenerator(pAudioOut->SampleRate());

        // (re)allocate synthesis parameter matrix
        if (pSynthesisParameters[0]) free(pSynthesisParameters[0]);

        #if defined(__APPLE__)
        pSynthesisParameters[0] = (float *) malloc(Event::destination_count * sizeof(float) * pAudioOut->MaxSamplesPerCycle());
        #else
        pSynthesisParameters[0] = (float *) memalign(16,(Event::destination_count * sizeof(float) * pAudioOut->MaxSamplesPerCycle()));
        #endif
        for (int dst = 1; dst < Event::destination_count; dst++)
            pSynthesisParameters[dst] = pSynthesisParameters[dst - 1] + pAudioOut->MaxSamplesPerCycle();

        // (re)allocate biquad filter parameter sequence
        if (pBasicFilterParameters) delete[] pBasicFilterParameters;
        if (pMainFilterParameters)  delete[] pMainFilterParameters;
        pBasicFilterParameters = new biquad_param_t[pAudioOut->MaxSamplesPerCycle()];
        pMainFilterParameters  = new biquad_param_t[pAudioOut->MaxSamplesPerCycle()];

        dmsg(1,("Starting disk thread..."));
        pDiskThread->StartThread();
        dmsg(1,("OK\n"));

        for (RTList<Voice>::Iterator iterVoice = pVoicePool->allocAppend(); iterVoice == pVoicePool->last(); iterVoice = pVoicePool->allocAppend()) {
            if (!iterVoice->pDiskThread) {
                dmsg(0,("Engine -> voice::trigger: !pDiskThread\n"));
                exit(EXIT_FAILURE);
            }
        }
    }

    void Engine::DisconnectAudioOutputDevice() {
        if (pAudioOutputDevice) { // if clause to prevent disconnect loops
            AudioOutputDevice* olddevice = pAudioOutputDevice;
            pAudioOutputDevice = NULL;
            olddevice->Disconnect(this);
            AudioDeviceChannelLeft  = -1;
            AudioDeviceChannelRight = -1;
        }
    }

    /**
     *  Let this engine proceed to render the given amount of sample points. The
     *  calculated audio data of all voices of this engine will be placed into
     *  the engine's audio sum buffer which has to be copied and eventually be
     *  converted to the appropriate value range by the audio output class (e.g.
     *  AlsaIO or JackIO) right after.
     *
     *  @param Samples - number of sample points to be rendered
     *  @returns       0 on success
     */
    int Engine::RenderAudio(uint Samples) {
        dmsg(5,("RenderAudio(Samples=%d)\n", Samples));

        // return if no instrument loaded or engine disabled
        if (EngineDisabled.Pop()) {
            dmsg(5,("gig::Engine: engine disabled (val=%d)\n",EngineDisabled.GetUnsafe()));
            return 0;
        }
        if (!pInstrument) {
            dmsg(5,("gig::Engine: no instrument loaded\n"));
            return 0;
        }


        // update time of start and end of this audio fragment (as events' time stamps relate to this)
        pEventGenerator->UpdateFragmentTime(Samples);


        // empty the event lists for the new fragment
        pEvents->clear();
        pCCEvents->clear();
        for (uint i = 0; i < Event::destination_count; i++) {
            pSynthesisEvents[i]->clear();
        }
        {
            RTList<uint>::Iterator iuiKey = pActiveKeys->first();
            RTList<uint>::Iterator end    = pActiveKeys->end();
            for(; iuiKey != end; ++iuiKey) {
                pMIDIKeyInfo[*iuiKey].pEvents->clear(); // free all events on the key
            }
        }


        // get all events from the input event queue which belong to the current fragment
        {
            RingBuffer<Event>::NonVolatileReader eventQueueReader = pEventQueue->get_non_volatile_reader();
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


        // process events
        {
            RTList<Event>::Iterator itEvent = pEvents->first();
            RTList<Event>::Iterator end     = pEvents->end();
            for (; itEvent != end; ++itEvent) {
                switch (itEvent->Type) {
                    case Event::type_note_on:
                        dmsg(5,("Engine: Note on received\n"));
                        ProcessNoteOn(itEvent);
                        break;
                    case Event::type_note_off:
                        dmsg(5,("Engine: Note off received\n"));
                        ProcessNoteOff(itEvent);
                        break;
                    case Event::type_control_change:
                        dmsg(5,("Engine: MIDI CC received\n"));
                        ProcessControlChange(itEvent);
                        break;
                    case Event::type_pitchbend:
                        dmsg(5,("Engine: Pitchbend received\n"));
                        ProcessPitchbend(itEvent);
                        break;
                    case Event::type_sysex:
                        dmsg(5,("Engine: Sysex received\n"));
                        ProcessSysex(itEvent);
                        break;
                }
            }
        }


        int active_voices = 0;

        // render audio from all active voices
        {
            RTList<uint>::Iterator iuiKey = pActiveKeys->first();
            RTList<uint>::Iterator end    = pActiveKeys->end();
            while (iuiKey != end) { // iterate through all active keys
                midi_key_info_t* pKey = &pMIDIKeyInfo[*iuiKey];
                ++iuiKey;

                RTList<Voice>::Iterator itVoice     = pKey->pActiveVoices->first();
                RTList<Voice>::Iterator itVoicesEnd = pKey->pActiveVoices->end();
                for (; itVoice != itVoicesEnd; ++itVoice) { // iterate through all voices on this key
                    // now render current voice
                    itVoice->Render(Samples);
                    if (itVoice->IsActive()) active_voices++; // still active
                    else { // voice reached end, is now inactive
                        FreeVoice(itVoice); // remove voice from the list of active voices
                    }
                }
            }
        }


        // now render all postponed voices from voice stealing
        {
            RTList<Event>::Iterator itVoiceStealEvent = pVoiceStealingQueue->first();
            RTList<Event>::Iterator end               = pVoiceStealingQueue->end();
            for (; itVoiceStealEvent != end; ++itVoiceStealEvent) {
                Pool<Voice>::Iterator itNewVoice = LaunchVoice(itVoiceStealEvent, itVoiceStealEvent->Param.Note.Layer, itVoiceStealEvent->Param.Note.ReleaseTrigger, false);
                if (itNewVoice) {
                    for (; itNewVoice; itNewVoice = itNewVoice->itChildVoice) {
                        itNewVoice->Render(Samples);
                        if (itNewVoice->IsActive()) active_voices++; // still active
                        else { // voice reached end, is now inactive
                            FreeVoice(itNewVoice); // remove voice from the list of active voices
                        }
                    }
                }
                else dmsg(1,("gig::Engine: ERROR, voice stealing didn't work out!\n"));
            }
        }
        // reset voice stealing for the new fragment
        pVoiceStealingQueue->clear();
        itLastStolenVoice = RTList<Voice>::Iterator();
        iuiLastStolenKey  = RTList<uint>::Iterator();


        // free all keys which have no active voices left
        {
            RTList<uint>::Iterator iuiKey = pActiveKeys->first();
            RTList<uint>::Iterator end    = pActiveKeys->end();
            while (iuiKey != end) { // iterate through all active keys
                midi_key_info_t* pKey = &pMIDIKeyInfo[*iuiKey];
                ++iuiKey;
                if (pKey->pActiveVoices->isEmpty()) FreeKey(pKey);
                #if DEVMODE
                else { // FIXME: should be removed before the final release (purpose: just a sanity check for debugging)
                    RTList<Voice>::Iterator itVoice     = pKey->pActiveVoices->first();
                    RTList<Voice>::Iterator itVoicesEnd = pKey->pActiveVoices->end();
                    for (; itVoice != itVoicesEnd; ++itVoice) { // iterate through all voices on this key
                        if (itVoice->itKillEvent) {
                            dmsg(1,("gig::Engine: ERROR, killed voice survived !!!\n"));
                        }
                    }
                }
                #endif // DEVMODE
            }
        }


        // write that to the disk thread class so that it can print it
        // on the console for debugging purposes
        ActiveVoiceCount = active_voices;
        if (ActiveVoiceCount > ActiveVoiceCountMax) ActiveVoiceCountMax = ActiveVoiceCount;


        return 0;
    }

    /**
     *  Will be called by the MIDIIn Thread to let the audio thread trigger a new
     *  voice for the given key.
     *
     *  @param Key      - MIDI key number of the triggered key
     *  @param Velocity - MIDI velocity value of the triggered key
     */
    void Engine::SendNoteOn(uint8_t Key, uint8_t Velocity) {
        Event event               = pEventGenerator->CreateEvent();
        event.Type                = Event::type_note_on;
        event.Param.Note.Key      = Key;
        event.Param.Note.Velocity = Velocity;
        if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
        else dmsg(1,("Engine: Input event queue full!"));
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to release
     *  voice(s) on the given key.
     *
     *  @param Key      - MIDI key number of the released key
     *  @param Velocity - MIDI release velocity value of the released key
     */
    void Engine::SendNoteOff(uint8_t Key, uint8_t Velocity) {
        Event event               = pEventGenerator->CreateEvent();
        event.Type                = Event::type_note_off;
        event.Param.Note.Key      = Key;
        event.Param.Note.Velocity = Velocity;
        if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
        else dmsg(1,("Engine: Input event queue full!"));
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread to change
     *  the pitch value for all voices.
     *
     *  @param Pitch - MIDI pitch value (-8192 ... +8191)
     */
    void Engine::SendPitchbend(int Pitch) {
        Event event             = pEventGenerator->CreateEvent();
        event.Type              = Event::type_pitchbend;
        event.Param.Pitch.Pitch = Pitch;
        if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
        else dmsg(1,("Engine: Input event queue full!"));
    }

    /**
     *  Will be called by the MIDIIn Thread to signal the audio thread that a
     *  continuous controller value has changed.
     *
     *  @param Controller - MIDI controller number of the occured control change
     *  @param Value      - value of the control change
     */
    void Engine::SendControlChange(uint8_t Controller, uint8_t Value) {
        Event event               = pEventGenerator->CreateEvent();
        event.Type                = Event::type_control_change;
        event.Param.CC.Controller = Controller;
        event.Param.CC.Value      = Value;
        if (this->pEventQueue->write_space() > 0) this->pEventQueue->push(&event);
        else dmsg(1,("Engine: Input event queue full!"));
    }

    /**
     *  Will be called by the MIDI input device whenever a MIDI system
     *  exclusive message has arrived.
     *
     *  @param pData - pointer to sysex data
     *  @param Size  - lenght of sysex data (in bytes)
     */
    void Engine::SendSysex(void* pData, uint Size) {
        Event event             = pEventGenerator->CreateEvent();
        event.Type              = Event::type_sysex;
        event.Param.Sysex.Size  = Size;
        if (pEventQueue->write_space() > 0) {
            if (pSysexBuffer->write_space() >= Size) {
                // copy sysex data to input buffer
                uint toWrite = Size;
                uint8_t* pPos = (uint8_t*) pData;
                while (toWrite) {
                    const uint writeNow = RTMath::Min(toWrite, pSysexBuffer->write_space_to_end());
                    pSysexBuffer->write(pPos, writeNow);
                    toWrite -= writeNow;
                    pPos    += writeNow;

                }
                // finally place sysex event into input event queue
                pEventQueue->push(&event);
            }
            else dmsg(1,("Engine: Sysex message too large (%d byte) for input buffer (%d byte)!",Size,SYSEX_BUFFER_SIZE));
        }
        else dmsg(1,("Engine: Input event queue full!"));
    }

    /**
     *  Assigns and triggers a new voice for the respective MIDI key.
     *
     *  @param itNoteOnEvent - key, velocity and time stamp of the event
     */
    void Engine::ProcessNoteOn(Pool<Event>::Iterator& itNoteOnEvent) {

        const int key = itNoteOnEvent->Param.Note.Key;

        // Change key dimension value if key is in keyswitching area
        if (key >= pInstrument->DimensionKeyRange.low && key <= pInstrument->DimensionKeyRange.high)
            CurrentKeyDimension = ((key - pInstrument->DimensionKeyRange.low) * 128) /
                (pInstrument->DimensionKeyRange.high - pInstrument->DimensionKeyRange.low + 1);

        midi_key_info_t* pKey = &pMIDIKeyInfo[key];

        pKey->KeyPressed = true; // the MIDI key was now pressed down

        // cancel release process of voices on this key if needed
        if (pKey->Active && !SustainPedal) {
            RTList<Event>::Iterator itCancelReleaseEvent = pKey->pEvents->allocAppend();
            if (itCancelReleaseEvent) {
                *itCancelReleaseEvent = *itNoteOnEvent;                  // copy event
                itCancelReleaseEvent->Type = Event::type_cancel_release; // transform event type
            }
            else dmsg(1,("Event pool emtpy!\n"));
        }

        // move note on event to the key's own event list
        RTList<Event>::Iterator itNoteOnEventOnKeyList = itNoteOnEvent.moveToEndOf(pKey->pEvents);

        // allocate and trigger a new voice for the key
        LaunchVoice(itNoteOnEventOnKeyList, 0, false, true);
    }

    /**
     *  Releases the voices on the given key if sustain pedal is not pressed.
     *  If sustain is pressed, the release of the note will be postponed until
     *  sustain pedal will be released or voice turned inactive by itself (e.g.
     *  due to completion of sample playback).
     *
     *  @param itNoteOffEvent - key, velocity and time stamp of the event
     */
    void Engine::ProcessNoteOff(Pool<Event>::Iterator& itNoteOffEvent) {
        midi_key_info_t* pKey = &pMIDIKeyInfo[itNoteOffEvent->Param.Note.Key];

        pKey->KeyPressed = false; // the MIDI key was now released

        // release voices on this key if needed
        if (pKey->Active && !SustainPedal) {
            itNoteOffEvent->Type = Event::type_release; // transform event type
        }

        // move event to the key's own event list
        RTList<Event>::Iterator itNoteOffEventOnKeyList = itNoteOffEvent.moveToEndOf(pKey->pEvents);

        // spawn release triggered voice(s) if needed
        if (pKey->ReleaseTrigger) {
            LaunchVoice(itNoteOffEventOnKeyList, 0, true, false); //FIXME: for the moment we don't perform voice stealing for release triggered samples
            pKey->ReleaseTrigger = false;
        }
    }

    /**
     *  Moves pitchbend event from the general (input) event list to the pitch
     *  event list.
     *
     *  @param itPitchbendEvent - absolute pitch value and time stamp of the event
     */
    void Engine::ProcessPitchbend(Pool<Event>::Iterator& itPitchbendEvent) {
        this->Pitch = itPitchbendEvent->Param.Pitch.Pitch; // store current pitch value
        itPitchbendEvent.moveToEndOf(pSynthesisEvents[Event::destination_vco]);
    }

    /**
     *  Allocates and triggers a new voice. This method will usually be
     *  called by the ProcessNoteOn() method and by the voices itself
     *  (e.g. to spawn further voices on the same key for layered sounds).
     *
     *  @param itNoteOnEvent       - key, velocity and time stamp of the event
     *  @param iLayer              - layer index for the new voice (optional - only
     *                               in case of layered sounds of course)
     *  @param ReleaseTriggerVoice - if new voice is a release triggered voice
     *                               (optional, default = false)
     *  @param VoiceStealing       - if voice stealing should be performed
     *                               when there is no free voice
     *                               (optional, default = true)
     *  @returns pointer to new voice or NULL if there was no free voice or
     *           if the voice wasn't triggered (for example when no region is
     *           defined for the given key).
     */
    Pool<Voice>::Iterator Engine::LaunchVoice(Pool<Event>::Iterator& itNoteOnEvent, int iLayer, bool ReleaseTriggerVoice, bool VoiceStealing) {
        midi_key_info_t* pKey = &pMIDIKeyInfo[itNoteOnEvent->Param.Note.Key];

        // allocate a new voice for the key
        Pool<Voice>::Iterator itNewVoice = pKey->pActiveVoices->allocAppend();
        if (itNewVoice) {
            // launch the new voice
            if (itNewVoice->Trigger(itNoteOnEvent, this->Pitch, this->pInstrument, iLayer, ReleaseTriggerVoice, VoiceStealing) < 0) {
                dmsg(4,("Voice not triggered\n"));
                pKey->pActiveVoices->free(itNewVoice);
            }
            else { // on success
                uint** ppKeyGroup = NULL;
                if (itNewVoice->KeyGroup) { // if this voice / key belongs to a key group
                    ppKeyGroup = &ActiveKeyGroups[itNewVoice->KeyGroup];
                    if (*ppKeyGroup) { // if there's already an active key in that key group
                        midi_key_info_t* pOtherKey = &pMIDIKeyInfo[**ppKeyGroup];
                        // kill all voices on the (other) key
                        RTList<Voice>::Iterator itVoiceToBeKilled = pOtherKey->pActiveVoices->first();
                        RTList<Voice>::Iterator end               = pOtherKey->pActiveVoices->end();
                        for (; itVoiceToBeKilled != end; ++itVoiceToBeKilled) {
                            if (itVoiceToBeKilled->Type != Voice::type_release_trigger) itVoiceToBeKilled->Kill(itNoteOnEvent);
                        }
                    }
                }
                if (!pKey->Active) { // mark as active key
                    pKey->Active = true;
                    pKey->itSelf = pActiveKeys->allocAppend();
                    *pKey->itSelf = itNoteOnEvent->Param.Note.Key;
                }
                if (itNewVoice->KeyGroup) {
                    *ppKeyGroup = &*pKey->itSelf; // put key as the (new) active key to its key group
                }
                if (itNewVoice->Type == Voice::type_release_trigger_required) pKey->ReleaseTrigger = true; // mark key for the need of release triggered voice(s)
                return itNewVoice; // success
            }
        }
        else if (VoiceStealing) {
            // first, get total amount of required voices (dependant on amount of layers)
            ::gig::Region* pRegion = pInstrument->GetRegion(itNoteOnEvent->Param.Note.Key);
            if (!pRegion) return Pool<Voice>::Iterator(); // nothing defined for this MIDI key, so no voice needed
            int voicesRequired = pRegion->Layers;

            // now steal the (remaining) amount of voices
            for (int i = iLayer; i < voicesRequired; i++)
                StealVoice(itNoteOnEvent);

            // put note-on event into voice-stealing queue, so it will be reprocessed after killed voice died
            RTList<Event>::Iterator itStealEvent = pVoiceStealingQueue->allocAppend();
            if (itStealEvent) {
                *itStealEvent = *itNoteOnEvent; // copy event
                itStealEvent->Param.Note.Layer = iLayer;
                itStealEvent->Param.Note.ReleaseTrigger = ReleaseTriggerVoice;
            }
            else dmsg(1,("Voice stealing queue full!\n"));
        }

        return Pool<Voice>::Iterator(); // no free voice or error
    }

    /**
     *  Will be called by LaunchVoice() method in case there are no free
     *  voices left. This method will select and kill one old voice for
     *  voice stealing and postpone the note-on event until the selected
     *  voice actually died.
     *
     *  @param itNoteOnEvent - key, velocity and time stamp of the event
     */
    void Engine::StealVoice(Pool<Event>::Iterator& itNoteOnEvent) {
        if (!pEventPool->poolIsEmpty()) {

            RTList<uint>::Iterator  iuiOldestKey;
            RTList<Voice>::Iterator itOldestVoice;

            // Select one voice for voice stealing
            switch (VOICE_STEAL_ALGORITHM) {

                // try to pick the oldest voice on the key where the new
                // voice should be spawned, if there is no voice on that
                // key, or no voice left to kill there, then procceed with
                // 'oldestkey' algorithm
                case voice_steal_algo_keymask: {
                    midi_key_info_t* pOldestKey = &pMIDIKeyInfo[itNoteOnEvent->Param.Note.Key];
                    if (itLastStolenVoice) {
                        itOldestVoice = itLastStolenVoice;
                        ++itOldestVoice;
                    }
                    else { // no voice stolen in this audio fragment cycle yet
                        itOldestVoice = pOldestKey->pActiveVoices->first();
                    }
                    if (itOldestVoice) {
                        iuiOldestKey = pOldestKey->itSelf;
                        break; // selection succeeded
                    }
                } // no break - intentional !

                // try to pick the oldest voice on the oldest active key
                // (caution: must stay after 'keymask' algorithm !)
                case voice_steal_algo_oldestkey: {
                    if (itLastStolenVoice) {
                        midi_key_info_t* pOldestKey = &pMIDIKeyInfo[*iuiLastStolenKey];
                        itOldestVoice = itLastStolenVoice;
                        ++itOldestVoice;
                        if (!itOldestVoice) {
                            iuiOldestKey = iuiLastStolenKey;
                            ++iuiOldestKey;
                            if (iuiOldestKey) {
                                midi_key_info_t* pOldestKey = &pMIDIKeyInfo[*iuiOldestKey];
                                itOldestVoice = pOldestKey->pActiveVoices->first();
                            }
                            else {
                                dmsg(1,("gig::Engine: Warning, too less voices, even for voice stealing! - Better recompile with higher MAX_AUDIO_VOICES.\n"));
                                return;
                            }
                        }
                        else iuiOldestKey = iuiLastStolenKey;
                    }
                    else { // no voice stolen in this audio fragment cycle yet
                        iuiOldestKey = pActiveKeys->first();
                        midi_key_info_t* pOldestKey = &pMIDIKeyInfo[*iuiOldestKey];
                        itOldestVoice = pOldestKey->pActiveVoices->first();
                    }
                    break;
                }

                // don't steal anything
                case voice_steal_algo_none:
                default: {
                    dmsg(1,("No free voice (voice stealing disabled)!\n"));
                    return;
                }
            }

            //FIXME: can be removed, just a sanity check for debugging
            if (!itOldestVoice->IsActive()) dmsg(1,("gig::Engine: ERROR, tried to steal a voice which was not active !!!\n"));

            // now kill the selected voice
            itOldestVoice->Kill(itNoteOnEvent);
            // remember which voice on which key we stole, so we can simply proceed for the next voice stealing
            this->itLastStolenVoice = itOldestVoice;
            this->iuiLastStolenKey = iuiOldestKey;
        }
        else dmsg(1,("Event pool emtpy!\n"));
    }

    /**
     *  Removes the given voice from the MIDI key's list of active voices.
     *  This method will be called when a voice went inactive, e.g. because
     *  it finished to playback its sample, finished its release stage or
     *  just was killed.
     *
     *  @param itVoice - points to the voice to be freed
     */
    void Engine::FreeVoice(Pool<Voice>::Iterator& itVoice) {
        if (itVoice) {
            midi_key_info_t* pKey = &pMIDIKeyInfo[itVoice->MIDIKey];

            uint keygroup = itVoice->KeyGroup;

            // free the voice object
            pVoicePool->free(itVoice);

            // if no other voices left and member of a key group, remove from key group
            if (pKey->pActiveVoices->isEmpty() && keygroup) {
                uint** ppKeyGroup = &ActiveKeyGroups[keygroup];
                if (*ppKeyGroup == &*pKey->itSelf) *ppKeyGroup = NULL; // remove key from key group
            }
        }
        else std::cerr << "Couldn't release voice! (!itVoice)\n" << std::flush;
    }

    /**
     *  Called when there's no more voice left on a key, this call will
     *  update the key info respectively.
     *
     *  @param pKey - key which is now inactive
     */
    void Engine::FreeKey(midi_key_info_t* pKey) {
        if (pKey->pActiveVoices->isEmpty()) {
            pKey->Active = false;
            pActiveKeys->free(pKey->itSelf); // remove key from list of active keys
            pKey->itSelf = RTList<uint>::Iterator();
            pKey->ReleaseTrigger = false;
            pKey->pEvents->clear();
            dmsg(3,("Key has no more voices now\n"));
        }
        else dmsg(1,("gig::Engine: Oops, tried to free a key which contains voices.\n"));
    }

    /**
     *  Reacts on supported control change commands (e.g. pitch bend wheel,
     *  modulation wheel, aftertouch).
     *
     *  @param itControlChangeEvent - controller, value and time stamp of the event
     */
    void Engine::ProcessControlChange(Pool<Event>::Iterator& itControlChangeEvent) {
        dmsg(4,("Engine::ContinuousController cc=%d v=%d\n", itControlChangeEvent->Param.CC.Controller, itControlChangeEvent->Param.CC.Value));

        switch (itControlChangeEvent->Param.CC.Controller) {
            case 64: {
                if (itControlChangeEvent->Param.CC.Value >= 64 && !SustainPedal) {
                    dmsg(4,("PEDAL DOWN\n"));
                    SustainPedal = true;

                    // cancel release process of voices if necessary
                    RTList<uint>::Iterator iuiKey = pActiveKeys->first();
                    if (iuiKey) {
                        itControlChangeEvent->Type = Event::type_cancel_release; // transform event type
                        while (iuiKey) {
                            midi_key_info_t* pKey = &pMIDIKeyInfo[*iuiKey];
                            ++iuiKey;
                            if (!pKey->KeyPressed) {
                                RTList<Event>::Iterator itNewEvent = pKey->pEvents->allocAppend();
                                if (itNewEvent) *itNewEvent = *itControlChangeEvent; // copy event to the key's own event list
                                else dmsg(1,("Event pool emtpy!\n"));
                            }
                        }
                    }
                }
                if (itControlChangeEvent->Param.CC.Value < 64 && SustainPedal) {
                    dmsg(4,("PEDAL UP\n"));
                    SustainPedal = false;

                    // release voices if their respective key is not pressed
                    RTList<uint>::Iterator iuiKey = pActiveKeys->first();
                    if (iuiKey) {
                        itControlChangeEvent->Type = Event::type_release; // transform event type
                        while (iuiKey) {
                            midi_key_info_t* pKey = &pMIDIKeyInfo[*iuiKey];
                            ++iuiKey;
                            if (!pKey->KeyPressed) {
                                RTList<Event>::Iterator itNewEvent = pKey->pEvents->allocAppend();
                                if (itNewEvent) *itNewEvent = *itControlChangeEvent; // copy event to the key's own event list
                                else dmsg(1,("Event pool emtpy!\n"));
                            }
                        }
                    }
                }
                break;
            }
        }

        // update controller value in the engine's controller table
        ControllerTable[itControlChangeEvent->Param.CC.Controller] = itControlChangeEvent->Param.CC.Value;

        // move event from the unsorted event list to the control change event list
        itControlChangeEvent.moveToEndOf(pCCEvents);
    }

    /**
     *  Reacts on MIDI system exclusive messages.
     *
     *  @param itSysexEvent - sysex data size and time stamp of the sysex event
     */
    void Engine::ProcessSysex(Pool<Event>::Iterator& itSysexEvent) {
        RingBuffer<uint8_t>::NonVolatileReader reader = pSysexBuffer->get_non_volatile_reader();

        uint8_t exclusive_status, id;
        if (!reader.pop(&exclusive_status)) goto free_sysex_data;
        if (!reader.pop(&id))               goto free_sysex_data;
        if (exclusive_status != 0xF0)       goto free_sysex_data;

        switch (id) {
            case 0x41: { // Roland
                uint8_t device_id, model_id, cmd_id;
                if (!reader.pop(&device_id)) goto free_sysex_data;
                if (!reader.pop(&model_id))  goto free_sysex_data;
                if (!reader.pop(&cmd_id))    goto free_sysex_data;
                if (model_id != 0x42 /*GS*/) goto free_sysex_data;
                if (cmd_id != 0x12 /*DT1*/)  goto free_sysex_data;

                // command address
                uint8_t addr[3]; // 2 byte addr MSB, followed by 1 byte addr LSB)
                const RingBuffer<uint8_t>::NonVolatileReader checksum_reader = reader; // so we can calculate the check sum later
                if (reader.read(&addr[0], 3) != 3) goto free_sysex_data;
                if (addr[0] == 0x40 && addr[1] == 0x00) { // System Parameters
                }
                else if (addr[0] == 0x40 && addr[1] == 0x01) { // Common Parameters
                }
                else if (addr[0] == 0x40 && (addr[1] & 0xf0) == 0x10) { // Part Parameters (1)
                    switch (addr[3]) {
                        case 0x40: { // scale tuning
                            uint8_t scale_tunes[12]; // detuning of all 12 semitones of an octave
                            if (reader.read(&scale_tunes[0], 12) != 12) goto free_sysex_data;
                            uint8_t checksum;
                            if (!reader.pop(&checksum))                      goto free_sysex_data;
                            if (GSCheckSum(checksum_reader, 12) != checksum) goto free_sysex_data;
                            for (int i = 0; i < 12; i++) scale_tunes[i] -= 64;
                            AdjustScale((int8_t*) scale_tunes);
                            break;
                        }
                    }
                }
                else if (addr[0] == 0x40 && (addr[1] & 0xf0) == 0x20) { // Part Parameters (2)
                }
                else if (addr[0] == 0x41) { // Drum Setup Parameters
                }
                break;
            }
        }

        free_sysex_data: // finally free sysex data
        pSysexBuffer->increment_read_ptr(itSysexEvent->Param.Sysex.Size);
    }

    /**
     * Calculates the Roland GS sysex check sum.
     *
     * @param AddrReader - reader which currently points to the first GS
     *                     command address byte of the GS sysex message in
     *                     question
     * @param DataSize   - size of the GS message data (in bytes)
     */
    uint8_t Engine::GSCheckSum(const RingBuffer<uint8_t>::NonVolatileReader AddrReader, uint DataSize) {
        RingBuffer<uint8_t>::NonVolatileReader reader = AddrReader;
        uint bytes = 3 /*addr*/ + DataSize;
        uint8_t addr_and_data[bytes];
        reader.read(&addr_and_data[0], bytes);
        uint8_t sum = 0;
        for (uint i = 0; i < bytes; i++) sum += addr_and_data[i];
        return 128 - sum % 128;
    }

    /**
     * Allows to tune each of the twelve semitones of an octave.
     *
     * @param ScaleTunes - detuning of all twelve semitones (in cents)
     */
    void Engine::AdjustScale(int8_t ScaleTunes[12]) {
        memcpy(&this->ScaleTuning[0], &ScaleTunes[0], 12); //TODO: currently not sample accurate
    }

    /**
     * Initialize the parameter sequence for the modulation destination given by
     * by 'dst' with the constant value given by val.
     */
    void Engine::ResetSynthesisParameters(Event::destination_t dst, float val) {
        int maxsamples = pAudioOutputDevice->MaxSamplesPerCycle();
        float* m = &pSynthesisParameters[dst][0];
        for (int i = 0; i < maxsamples; i += 4) {
           m[i]   = val;
           m[i+1] = val;
           m[i+2] = val;
           m[i+3] = val;
        }
    }

    float Engine::Volume() {
        return GlobalVolume;
    }

    void Engine::Volume(float f) {
        GlobalVolume = f;
    }

    uint Engine::Channels() {
        return 2;
    }

    void Engine::SetOutputChannel(uint EngineAudioChannel, uint AudioDeviceChannel) {
        AudioChannel* pChannel = pAudioOutputDevice->Channel(AudioDeviceChannel);
        if (!pChannel) throw AudioOutputException("Invalid audio output device channel " + ToString(AudioDeviceChannel));
        switch (EngineAudioChannel) {
            case 0: // left output channel
                pOutputLeft = pChannel->Buffer();
                AudioDeviceChannelLeft = AudioDeviceChannel;
                break;
            case 1: // right output channel
                pOutputRight = pChannel->Buffer();
                AudioDeviceChannelRight = AudioDeviceChannel;
                break;
            default:
                throw AudioOutputException("Invalid engine audio channel " + ToString(EngineAudioChannel));
        }
    }

    int Engine::OutputChannel(uint EngineAudioChannel) {
        switch (EngineAudioChannel) {
            case 0: // left channel
                return AudioDeviceChannelLeft;
            case 1: // right channel
                return AudioDeviceChannelRight;
            default:
                throw AudioOutputException("Invalid engine audio channel " + ToString(EngineAudioChannel));
        }
    }

    uint Engine::VoiceCount() {
        return ActiveVoiceCount;
    }

    uint Engine::VoiceCountMax() {
        return ActiveVoiceCountMax;
    }

    bool Engine::DiskStreamSupported() {
        return true;
    }

    uint Engine::DiskStreamCount() {
        return (pDiskThread) ? pDiskThread->ActiveStreamCount : 0;
    }

    uint Engine::DiskStreamCountMax() {
        return (pDiskThread) ? pDiskThread->ActiveStreamCountMax : 0;
    }

    String Engine::DiskStreamBufferFillBytes() {
        return pDiskThread->GetBufferFillBytes();
    }

    String Engine::DiskStreamBufferFillPercentage() {
        return pDiskThread->GetBufferFillPercentage();
    }

    String Engine::EngineName() {
        return "GigEngine";
    }

    String Engine::InstrumentFileName() {
        return InstrumentFile;
    }

    String Engine::InstrumentName() {
        return InstrumentIdxName;
    }

    int Engine::InstrumentIndex() {
        return InstrumentIdx;
    }

    int Engine::InstrumentStatus() {
        return InstrumentStat;
    }

    String Engine::Description() {
        return "Gigasampler Engine";
    }

    String Engine::Version() {
        String s = "$Revision: 1.25 $";
        return s.substr(11, s.size() - 13); // cut dollar signs, spaces and CVS macro keyword
    }

}} // namespace LinuxSampler::gig
