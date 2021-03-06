/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 - 2008 Christian Schoenebeck                       *
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

#ifndef __LS_AUDIOOUTPUTDEVICEJACK_H__
#define __LS_AUDIOOUTPUTDEVICEJACK_H__

#include "../../common/global_private.h"

#if HAVE_JACK

#include <vector>
#include <sstream>
#include <jack/jack.h>

#include "AudioOutputDevice.h"
#include "../../common/ConditionServer.h"

#if HAVE_JACK_MIDI
#include "../midi/MidiInputDeviceJack.h"
#endif

namespace LinuxSampler {

    /** JACK audio output driver
     *
     * Implements audio output to the JACK Audio Connection Kit (JACK).
     */
    class AudioOutputDeviceJack : public AudioOutputDevice {
        public:
            AudioOutputDeviceJack(std::map<String,DeviceCreationParameter*> Parameters);
            virtual ~AudioOutputDeviceJack();

            /**
             * Audio channel implementation for the JACK audio driver.
             */
            class AudioChannelJack : public AudioChannel {
                public:
                    /** Audio Channel Parameter 'NAME'
                     *
                     * Used to assign an arbitrary name to an audio channel.
                     */
                    class ParameterName : public AudioChannel::ParameterName {
                        public:
                            ParameterName(AudioChannelJack* pChannel);
                            virtual void OnSetValue(String s);
                        protected:
                            AudioChannelJack* pChannel;
                    };

                    /** Audio Channel Parameter 'JACK_BINDINGS'
                     *
                     * Used to connect to other JACK clients.
                     */
                    class ParameterJackBindings : public DeviceRuntimeParameterStrings {
                        public:
                            ParameterJackBindings(AudioChannelJack* pChannel);
                            virtual String              Description();
                            virtual bool                Fix();
                            virtual std::vector<String> PossibilitiesAsString();
                            virtual void                OnSetValue(std::vector<String> vS);
                            static String Name();
                        protected:
                            AudioChannelJack*   pChannel;
                            std::vector<String> Bindings;
                    };
                protected:
                    AudioChannelJack(uint ChannelNr, AudioOutputDeviceJack* pDevice) throw (AudioOutputException);
                    ~AudioChannelJack();
                    friend class AudioOutputDeviceJack;
                private:
                    AudioOutputDeviceJack* pDevice;
                    jack_port_t*           hJackPort;
                    uint                   ChannelNr;

                    float* CreateJackPort(uint ChannelNr, AudioOutputDeviceJack* pDevice) throw (AudioOutputException);
            };

            /** Audio Device Parameter 'NAME'
             *
             * Used to assign an arbitrary name to the JACK client of this
             * audio device.
             */
            class ParameterName : public DeviceCreationParameterString {
                public:
                    ParameterName();
                    ParameterName(String s) throw (Exception);
                    virtual String              Description();
                    virtual bool                Fix();
                    virtual bool                Mandatory();
                    virtual std::map<String,DeviceCreationParameter*> DependsAsParameters();
                    virtual std::vector<String> PossibilitiesAsString(std::map<String,String> Parameters);
                    virtual optional<String>    DefaultAsString(std::map<String,String> Parameters);
                    virtual void                OnSetValue(String s) throw (Exception);
                    static String Name();
            };

            // derived abstract methods from class 'AudioOutputDevice'
            virtual void Play();
            virtual bool IsPlaying();
            virtual void Stop();
            virtual uint MaxSamplesPerCycle();
            virtual uint SampleRate();
            virtual AudioChannel* CreateChannel(uint ChannelNr);


            static String Name();

            virtual String Driver();

            static String Description();
            static String Version();

            int Process(uint Samples);  // FIXME: should be private
        protected:
            AudioOutputDeviceJack(String* AutoConnectPortIDs = NULL, uint AutoConnectPorts = 0);
        private:
            ConditionServer csIsPlaying;
            uint            uiMaxSamplesPerCycle;
            jack_client_t*  hJackClient;
    };

    // Callback functions for the libjack API
    int  linuxsampler_libjack_process_callback(jack_nframes_t nframes, void* arg);
    void linuxsampler_libjack_shutdown_callback(void* arg);


    /** JACK client
     *
     * Represents a jack client. This class is shared by
     * AudioOutputDeviceJack and MidiInputDeviceJack. The jack server
     * calls JackClient::Process, which in turn calls
     * AudioOutputDeviceJack::Process and/or
     * MidiInputDeviceJack::Process.
     */
    class JackClient {
        public:
            static JackClient* CreateAudio(String Name);
            static JackClient* CreateMidi(String Name);
            static void ReleaseAudio(String Name);
            static void ReleaseMidi(String Name);
            int Process(uint Samples);
            void Stop();
            void SetAudioOutputDevice(AudioOutputDeviceJack* device);
            #if HAVE_JACK_MIDI
            void SetMidiInputDevice(MidiInputDeviceJack* device);
            #endif

            jack_client_t* hJackClient;

        private:
            static std::map<String, JackClient*> Clients;
            struct config_t {
                AudioOutputDeviceJack* AudioDevice;
                #if HAVE_JACK_MIDI
                MidiInputDeviceJack* MidiDevice;
                #endif
            };
            SynchronizedConfig<config_t> Config;
            SynchronizedConfig<config_t>::Reader ConfigReader;
            bool audio;
            bool midi;

            JackClient(String Name);
            ~JackClient();
    };
}

#endif // HAVE_JACK
#endif // __LS_AUDIOOUTPUTDEVICEJACK_H__
