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

#include "AudioOutputDeviceJack.h"
#include "AudioOutputDeviceFactory.h"

#include <errno.h>

#if HAVE_JACK

namespace LinuxSampler {

    REGISTER_AUDIO_OUTPUT_DRIVER(AudioOutputDeviceJack);

    /* Common parameters for now they'll have to be registered here. */
    REGISTER_AUDIO_OUTPUT_DRIVER_PARAMETER(AudioOutputDeviceJack, ParameterActive);
    REGISTER_AUDIO_OUTPUT_DRIVER_PARAMETER(AudioOutputDeviceJack, ParameterChannels);



// *************** ParameterName ***************
// *

    AudioOutputDeviceJack::AudioChannelJack::ParameterName::ParameterName(AudioChannelJack* pChannel) : AudioChannel::ParameterName(ToString(pChannel->ChannelNr)) {
        this->pChannel = pChannel;
    }

    void AudioOutputDeviceJack::AudioChannelJack::ParameterName::OnSetValue(String s) {
        if (jack_port_set_name(pChannel->hJackPort, s.c_str())) throw AudioOutputException("Failed to rename JACK port");
    }



// *************** ParameterJackBindings ***************
// *

    AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::ParameterJackBindings(AudioChannelJack* pChannel) : DeviceRuntimeParameterStrings(std::vector<String>()) {
        this->pChannel = pChannel;
    }

    String AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::Description() {
        return "Bindings to other JACK clients";
    }

    bool AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::Fix() {
        return false;
    }

    std::vector<String> AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::PossibilitiesAsString() {
        const char** pPortNames = jack_get_ports(pChannel->pDevice->hJackClient, NULL, NULL, JackPortIsInput);
        if (!pPortNames) return std::vector<String>();
        std::vector<String> result;
        for (int i = 0; pPortNames[i]; i++) result.push_back(pPortNames[i]);
        //free(pPortNames); FIXME: pPortNames should be freed here
        return result;
    }

    void AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::OnSetValue(std::vector<String> vS) {
        // TODO: we should remove all existing bindings before we connect new ones here
        String src_name = "LinuxSampler:" + ((DeviceRuntimeParameterString*)pChannel->Parameters["NAME"])->ValueAsString();
        for (int i = 0; i < vS.size(); i++) {
            String dst_name = vS[i];
            int res = jack_connect(pChannel->pDevice->hJackClient, src_name.c_str(), dst_name.c_str());
            if (res == EEXIST) throw AudioOutputException("Jack: Connection to port '" + dst_name + "' already established");
            else if (res)      throw AudioOutputException("Jack: Cannot connect port '" + src_name + "' to port '" + dst_name + "'");
        }
    }

    String AudioOutputDeviceJack::AudioChannelJack::ParameterJackBindings::Name() {
        return "JACK_BINDINGS";
    }



// *************** AudioChannelJack ***************
// *

    AudioOutputDeviceJack::AudioChannelJack::AudioChannelJack(uint ChannelNr, AudioOutputDeviceJack* pDevice) throw (AudioOutputException) : AudioChannel(ChannelNr, CreateJackPort(ChannelNr, pDevice), pDevice->uiMaxSamplesPerCycle) {
        this->pDevice   = pDevice;
        this->ChannelNr = ChannelNr;
        Parameters["NAME"]          = new ParameterName(this);
        Parameters["JACK_BINDINGS"] = new ParameterJackBindings(this);
    }

    AudioOutputDeviceJack::AudioChannelJack::~AudioChannelJack() {
        //TODO: delete JACK port
    }

    float* AudioOutputDeviceJack::AudioChannelJack::CreateJackPort(uint ChannelNr, AudioOutputDeviceJack* pDevice) throw (AudioOutputException) {
        String port_id = ToString(ChannelNr);
        hJackPort = jack_port_register(pDevice->hJackClient, port_id.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!hJackPort) throw AudioOutputException("Jack: Cannot register Jack output port.");
        return (float*) jack_port_get_buffer(hJackPort, pDevice->uiMaxSamplesPerCycle);
    }



// *************** AudioOutputDeviceJack ***************
// *

    /**
     * Open and initialize connection to the JACK system.
     *
     * @param Parameters - optional parameters
     * @throws AudioOutputException  on error
     * @see AcquireChannels()
     */
    AudioOutputDeviceJack::AudioOutputDeviceJack(std::map<String,DeviceCreationParameter*> Parameters) : AudioOutputDevice(Parameters) {
        if ((hJackClient = jack_client_new("LinuxSampler")) == 0)
            throw AudioOutputException("Seems Jack server not running.");

        jack_set_process_callback(hJackClient, __libjack_process_callback, this);
        jack_on_shutdown(hJackClient, __libjack_shutdown_callback, this);
        if (jack_activate(hJackClient))
            throw AudioOutputException("Jack: Cannot activate Jack client.");

        uiMaxSamplesPerCycle = jack_get_buffer_size(hJackClient);

        // create audio channels
        AcquireChannels(((DeviceCreationParameterInt*)Parameters["CHANNELS"])->ValueAsInt());

        // finally activate device if desired
        if (((DeviceCreationParameterBool*)Parameters["ACTIVE"])->ValueAsBool()) Play();
    }

    AudioOutputDeviceJack::~AudioOutputDeviceJack() {
        // destroy jack client
        jack_client_close(hJackClient);
    }

    /**
     * This method should not be called directly! It will be called by
     * libjack to demand transmission of further sample points.
     */
    int AudioOutputDeviceJack::Process(uint Samples) {
        if (csIsPlaying.Pop()) {
            // let all connected engines render 'Samples' sample points
            return RenderAudio(Samples);
        }
        else {
            // playback stop by zeroing output buffer(s) and not calling connected sampler engines to render audio
            return RenderSilence(Samples);
        }
    }

    void AudioOutputDeviceJack::Play() {
        csIsPlaying.PushAndUnlock(true);
    }

    bool AudioOutputDeviceJack::IsPlaying() {
        csIsPlaying.GetUnsafe();
    }

    void AudioOutputDeviceJack::Stop() {
        csIsPlaying.PushAndUnlock(false);
    }

    AudioChannel* AudioOutputDeviceJack::CreateChannel(uint ChannelNr) {
        return new AudioChannelJack(ChannelNr, this);
    }

    uint AudioOutputDeviceJack::MaxSamplesPerCycle() {
        return jack_get_buffer_size(hJackClient);
    }

    uint AudioOutputDeviceJack::SampleRate() {
        return jack_get_sample_rate(hJackClient);
    }

    String AudioOutputDeviceJack::Name() {
        return "JACK";
    }

    String AudioOutputDeviceJack::Driver() {
        return Name();
    }

    String AudioOutputDeviceJack::Description() {
        return "JACK Audio Connection Kit";
    }

    String AudioOutputDeviceJack::Version() {
       String s = "$Revision: 1.13 $";
       return s.substr(11, s.size() - 13); // cut dollar signs, spaces and CVS macro keyword
    }

    // libjack callback functions

    int __libjack_process_callback(jack_nframes_t nframes, void* arg) {
        AudioOutputDeviceJack* pAudioOutputDeviceJack = (AudioOutputDeviceJack*) arg;
        return pAudioOutputDeviceJack->Process(nframes);
    }

    void __libjack_shutdown_callback(void* arg) {
        AudioOutputDeviceJack* pAudioOutputDeviceJack = (AudioOutputDeviceJack*) arg;
        pAudioOutputDeviceJack->Stop();
        fprintf(stderr, "Jack: Jack server shutdown, exiting.\n");
    }

} // namespace LinuxSampler

#endif // HAVE_JACK
