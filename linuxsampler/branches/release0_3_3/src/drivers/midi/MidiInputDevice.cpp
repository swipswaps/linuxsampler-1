/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 Christian Schoenebeck                              *
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

#include "MidiInputDevice.h"

namespace LinuxSampler {

// *************** ParameterActive ***************
// *

    MidiInputDevice::ParameterActive::ParameterActive() : DeviceCreationParameterBool() {
        InitWithDefault();
    }

    MidiInputDevice::ParameterActive::ParameterActive(String active) : DeviceCreationParameterBool(active) {
    }

    String MidiInputDevice::ParameterActive::Description() {
        return "Enable / disable device";
    }

    bool MidiInputDevice::ParameterActive::Fix() {
        return false;
    }

    bool MidiInputDevice::ParameterActive::Mandatory() {
        return false;
    }

    std::map<String,DeviceCreationParameter*> MidiInputDevice::ParameterActive::DependsAsParameters() {
        return std::map<String,DeviceCreationParameter*>();
    }

    optional<bool> MidiInputDevice::ParameterActive::DefaultAsBool(std::map<String,String> Parameters) {
        return true;
    }

    void MidiInputDevice::ParameterActive::OnSetValue(bool b) throw (LinuxSamplerException) {
        if (b) ((MidiInputDevice*)pDevice)->Listen();
        else ((MidiInputDevice*)pDevice)->StopListen();
    }

    String MidiInputDevice::ParameterActive::Name() {
        return "ACTIVE";
    }



// *************** ParameterPorts ***************
// *

    MidiInputDevice::ParameterPorts::ParameterPorts() : DeviceCreationParameterInt() {
        InitWithDefault();
    }

    MidiInputDevice::ParameterPorts::ParameterPorts(String val) : DeviceCreationParameterInt(val) {
    }

    String MidiInputDevice::ParameterPorts::Description() {
        return "Number of ports";
    }

    bool MidiInputDevice::ParameterPorts::Fix() {
        return false;
    }

    bool MidiInputDevice::ParameterPorts::Mandatory() {
        return false;
    }

    std::map<String,DeviceCreationParameter*> MidiInputDevice::ParameterPorts::DependsAsParameters() {
        return std::map<String,DeviceCreationParameter*>();
    }

    optional<int> MidiInputDevice::ParameterPorts::DefaultAsInt(std::map<String,String> Parameters) {
        return 1;
    }

    optional<int> MidiInputDevice::ParameterPorts::RangeMinAsInt(std::map<String,String> Parameters) {
        return optional<int>::nothing;
    }

    optional<int> MidiInputDevice::ParameterPorts::RangeMaxAsInt(std::map<String,String> Parameters) {
        return optional<int>::nothing;
    }

    std::vector<int> MidiInputDevice::ParameterPorts::PossibilitiesAsInt(std::map<String,String> Parameters) {
        return std::vector<int>();
    }

    void MidiInputDevice::ParameterPorts::OnSetValue(int i) throw (LinuxSamplerException) {
        ((MidiInputDevice*)pDevice)->AcquirePorts(i);
    }

    String MidiInputDevice::ParameterPorts::Name() {
        return "PORTS";
    }



// *************** MidiInputDevice ***************
// *

    MidiInputDevice::MidiInputDevice(std::map<String,DeviceCreationParameter*> DriverParameters, void* pSampler) {
        this->Parameters = DriverParameters;
        this->pSampler   = pSampler;
    }

    MidiInputDevice::~MidiInputDevice() {
	    std::map<String,DeviceCreationParameter*>::iterator iter = Parameters.begin();
	    while (iter != Parameters.end()) {
		    Parameters.erase(iter);
		    delete iter->second;
		    iter++;
	    }
    }

    MidiInputPort* MidiInputDevice::GetPort(uint iPort) throw (MidiInputException) {
        if (iPort >= Ports.size()) throw MidiInputException("There is no port " + ToString(iPort));
        return Ports[iPort];
    }

    std::map<String,DeviceCreationParameter*> MidiInputDevice::DeviceParameters() {
	    return Parameters;
    }

    void MidiInputDevice::AcquirePorts(uint newPorts) {
	 int diff = this->Ports.size() - newPorts;
	 if (!diff)
		 return; //Number of ports matches already, nothing to do.

	 while (diff != 0) {
		 if (diff > 0) {	//We've got too many ports, remove one
			 std::map<int,MidiInputPort*>::iterator portsIter = Ports.end();
			 --portsIter;
			 Ports.erase(portsIter);
			 delete portsIter->second;
			 diff--;
		 }
		 if (diff < 0) {       //We don't have enough ports, create one
			 MidiInputPort* midiPort = this->CreateMidiPort();
			 Ports[midiPort->portNumber] = midiPort;
			 diff++;
		 }
	 }
    }

} // namespace LinuxSampler
