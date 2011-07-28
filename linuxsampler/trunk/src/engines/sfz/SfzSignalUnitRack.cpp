/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2011 Grigor Iliev                                       *
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

#include "SfzSignalUnitRack.h"
#include "Voice.h"

namespace LinuxSampler { namespace sfz {
    
    SfzSignalUnit::SfzSignalUnit(SfzSignalUnitRack* rack): SignalUnit(rack), pVoice(rack->pVoice) {
        
    }
    
    double SfzSignalUnit::GetSampleRate() {
        return pVoice->GetSampleRate() / CONFIG_DEFAULT_SUBFRAGMENT_SIZE;
    }
    
    
    void EGv1Unit::Trigger() {
        ::sfz::Region* const pRegion = pVoice->pRegion;
        // the length of the decay and release curves are dependent on the velocity
        const double velrelease = 1 / pVoice->GetVelocityRelease(pVoice->MIDIVelocity);

        EG.trigger(uint(pRegion->ampeg_start * 10),
                   std::max(0.0, pRegion->ampeg_attack + pRegion->ampeg_vel2attack * velrelease),
                   std::max(0.0, pRegion->ampeg_hold + pRegion->ampeg_vel2hold * velrelease),
                   std::max(0.0, pRegion->ampeg_decay + pRegion->ampeg_vel2decay * velrelease),
                   uint(std::min(std::max(0.0, 10 * (pRegion->ampeg_sustain + pRegion->ampeg_vel2sustain * velrelease)), 1000.0)),
                   std::max(0.0, pRegion->ampeg_release + pRegion->ampeg_vel2release * velrelease),
                   GetSampleRate());
    }
    
    void EGv2Unit::Trigger() {
        EG.trigger(*pEGInfo, GetSampleRate(), pVoice->MIDIVelocity);
    }
    

    void LFOUnit::Increment() {
        if (DelayStage()) return;
        
        SignalUnit::Increment();
        
        Level = lfo.render();
    }
    
    void LFOUnit::Trigger() {
        //reset
        Level = 0;
        
        // set the delay trigger
         uiDelayTrigger = pLfoInfo->delay * GetSampleRate();
    }
    
    void LFOv2Unit::Trigger() {
        LFOUnit::Trigger();
        
        lfo.trigger (
            pLfoInfo->freq,
            start_level_mid,
            1, 0, false, GetSampleRate()
        );
        lfo.update(0);
    }


    EndpointUnit::EndpointUnit(SfzSignalUnitRack* rack): EndpointSignalUnit(rack) {
        
    }
    
    SfzSignalUnitRack* const EndpointUnit::GetRack() {
        return static_cast<SfzSignalUnitRack* const>(pRack);
    }
    
    void EndpointUnit::Trigger() {
        
    }
    
    bool EndpointUnit::Active() {
        if (GetRack()->suVolEG.Active()) return true;
        
        bool b = false;
        for (int i = 0; i < GetRack()->volEGs.size(); i++) {
            if (GetRack()->volEGs[i]->Active()) { b = true; break; }
        }
        
        return b;
    }
    
    float EndpointUnit::GetVolume() {
        float vol = GetRack()->suVolEG.Active() ? GetRack()->suVolEG.GetLevel() : 0;
        
        for (int i = 0; i < GetRack()->volEGs.size(); i++) {
            EGv2Unit* eg = GetRack()->volEGs[i];
            if (!eg->Active()) continue;
            vol += eg->GetLevel() * (eg->pEGInfo->amplitude / 100.0f);
        }
        
        return vol;
    }
    
    float EndpointUnit::GetFilterCutoff() {
        float val = 1;
        
        for (int i = 0; i < GetRack()->filLFOs.size(); i++) {
            LFOv2Unit* lfo = GetRack()->filLFOs[i];
            if (!lfo->Active()) continue;
            
            float f = lfo->GetLevel() * lfo->pLfoInfo->cutoff;
            val *= RTMath::CentsToFreqRatioUnlimited(f);
        }
        
        return val;
    }
    
    float EndpointUnit::GetPitch() {
        return 1;
    }
    
    float EndpointUnit::GetResonance() {
         float val = 0;
        
        for (int i = 0; i < GetRack()->resLFOs.size(); i++) {
            LFOv2Unit* lfo = GetRack()->resLFOs[i];
            if (!lfo->Active()) continue;
            
            val += lfo->GetLevel() * lfo->pLfoInfo->resonance;
        }
        
        return val;
    }
    
    float EndpointUnit::GetPan() {
        float pan = 0;
        
        for (int i = 0; i < GetRack()->panLFOs.size(); i++) {
            LFOv2Unit* lfo = GetRack()->panLFOs[i];
            if (!lfo->Active()) continue;
            
            pan += lfo->GetLevel() * lfo->pLfoInfo->pan;
        }
        
        if(pan < -100) return -100;
        if(pan >  100) return  100;
        
        return pan;
    }
    
    
    SfzSignalUnitRack::SfzSignalUnitRack(Voice* voice)
        : SignalUnitRack(MaxUnitCount), pVoice(voice), suEndpoint(this), suVolEG(this),
        EGs(maxEgCount), volEGs(maxEgCount), pitchEGs(maxEgCount),
        LFOs(maxLfoCount), filLFOs(maxLfoCount), resLFOs(maxLfoCount), panLFOs(maxLfoCount)
    {
        suEndpoint.pVoice = suVolEG.pVoice = voice;
        
        for (int i = 0; i < EGs.capacity(); i++) {
            EGs[i] = new EGv2Unit(this);
            EGs[i]->pVoice = voice;
        }
        
        for (int i = 0; i < LFOs.capacity(); i++) {
            LFOs[i] = new LFOv2Unit(this);
            LFOs[i]->pVoice = voice;
        }
    }
    
    SfzSignalUnitRack::~SfzSignalUnitRack() {
        for (int i = 0; i < EGs.capacity(); i++) {
            delete EGs[i]; EGs[i] = NULL;
        }
        
        for (int i = 0; i < LFOs.capacity(); i++) {
            delete LFOs[i]; LFOs[i] = NULL;
        }
    }
    
    void SfzSignalUnitRack::Trigger() {
        EGs.clear();
        volEGs.clear();
        pitchEGs.clear();
        
        LFOs.clear();
        filLFOs.clear();
        resLFOs.clear();
        panLFOs.clear();
        
        ::sfz::Region* const pRegion = pVoice->pRegion;
        
        for (int i = 0; i < pRegion->eg.size(); i++) {
            if (pRegion->eg[i].node.size() == 0) continue;
            
            if(EGs.size() < EGs.capacity()) {
                EGv2Unit eg(this);
                eg.pEGInfo = &(pRegion->eg[i]);
                EGs.increment()->Copy(eg);
            } else { std::cerr << "Maximum number of EGs reached!" << std::endl; break; }
            
            if (pRegion->eg[i].amplitude > 0) {
                if(volEGs.size() < volEGs.capacity()) volEGs.add(EGs[EGs.size() - 1]);
                else std::cerr << "Maximum number of EGs reached!" << std::endl;
            }
        }
        
        if (pRegion->ampeg_sustain == -1) {
            if (volEGs.size() > 0) pRegion->ampeg_sustain = 0;
            else pRegion->ampeg_sustain = 100;
        }
        
        // LFO
        for (int i = 0; i < pRegion->lfos.size(); i++) {
            if (pRegion->lfos[i].freq == -1) continue; // Not initialized
            
            if(LFOs.size() < LFOs.capacity()) {
                LFOv2Unit lfo(this);
                lfo.pLfoInfo = &(pRegion->lfos[i]);
                LFOs.increment()->Copy(lfo);
            } else { std::cerr << "Maximum number of LFOs reached!" << std::endl; break; }
            
            if (pRegion->lfos[i].cutoff != 0) {
                if(filLFOs.size() < filLFOs.capacity()) filLFOs.add(LFOs[LFOs.size() - 1]);
                else std::cerr << "Maximum number of LFOs reached!" << std::endl;
            }
            
            if (pRegion->lfos[i].resonance != 0) {
                if(resLFOs.size() < resLFOs.capacity()) resLFOs.add(LFOs[LFOs.size() - 1]);
                else std::cerr << "Maximum number of LFOs reached!" << std::endl;
            }
            
            if (pRegion->lfos[i].pan != 0) {
                if(panLFOs.size() < panLFOs.capacity()) panLFOs.add(LFOs[LFOs.size() - 1]);
                else std::cerr << "Maximum number of LFOs reached!" << std::endl;
            }
        }
        
        Units.clear();
        
        Units.add(&suVolEG);
        
        for (int i = 0; i < EGs.size(); i++) {
            Units.add(EGs[i]);
        }
        
        for (int i = 0; i < LFOs.size(); i++) {
            Units.add(LFOs[i]);
        }
        
        Units.add(&suEndpoint);
        
        SignalUnitRack::Trigger();
    }
    
    EndpointSignalUnit* SfzSignalUnitRack::GetEndpointUnit() {
        return &suEndpoint;
    }
    
    void SfzSignalUnitRack::EnterFadeOutStage() {
        suVolEG.EG.enterFadeOutStage();
        
        for (int i = 0; i < volEGs.size(); i++) {
            volEGs[i]->EG.enterFadeOutStage();
        }
    }
    
}} // namespace LinuxSampler::sfz
