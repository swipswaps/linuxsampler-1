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

#ifndef __LS_ENGINE_H__
#define __LS_ENGINE_H__

#include "../common/global.h"

namespace LinuxSampler {

    /** @brief LinuxSampler Sampler Engine Interface
     *
     * Abstract base interface class for all LinuxSampler engines which
     * defines all mandatory methods which have to be implemented by all
     * sampler engine implementations.
     */
    class Engine {
        public:

            /////////////////////////////////////////////////////////////////
            // abstract methods
            //     (these have to be implemented by the descendant)

            virtual int    RenderAudio(uint Samples) = 0;
            virtual void   SendSysex(void* pData, uint Size) = 0;
            virtual void   Reset() = 0;
            virtual void   Enable() = 0;
            virtual void   Disable() = 0;
            virtual uint   VoiceCount() = 0;
            virtual uint   VoiceCountMax() = 0;
            virtual bool   DiskStreamSupported() = 0;
            virtual uint   DiskStreamCount() = 0;
            virtual uint   DiskStreamCountMax() = 0;
            virtual String DiskStreamBufferFillBytes() = 0;
            virtual String DiskStreamBufferFillPercentage() = 0;
            virtual String Description() = 0;
            virtual String Version() = 0;
            virtual String EngineName() = 0;

        protected:
            virtual ~Engine() {}; // MUST only be destroyed by EngineFactory
            friend class EngineFactory;
    };

} // namespace LinuxSampler

#endif // __LS_ENGINE_H__
