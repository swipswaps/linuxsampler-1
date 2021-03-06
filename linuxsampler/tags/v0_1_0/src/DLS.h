/***************************************************************************
 *                                                                         *
 *   libgig - C++ cross-platform Gigasampler format file loader library    *
 *                                                                         *
 *   Copyright (C) 2003 by Christian Schoenebeck                           *
 *                         <cuse@users.sourceforge.net>                    *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this library; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#ifndef __DLS_H__
#define __DLS_H__

#include "RIFF.h"

#if WORDS_BIGENDIAN
# define LIST_TYPE_INFO	0x494E464F
# define LIST_TYPE_WVPL	0x7776706C
# define LIST_TYPE_DWPL 0x6477706C  ///< Seen on some files instead of a wvpl list chunk.
# define LIST_TYPE_WAVE	0x77617665
# define LIST_TYPE_LINS	0X6C696E73
# define LIST_TYPE_INS	0X696E7320
# define LIST_TYPE_LRGN	0x6C72676E
# define LIST_TYPE_LART	0x6C617274
# define LIST_TYPE_LAR2	0x6C617232
# define LIST_TYPE_RGN	0x72676E20
# define LIST_TYPE_RGN2	0x72676E32
# define LIST_TYPE_ART1	0x61727431
# define LIST_TYPE_ART2	0x61727432
# define CHUNK_ID_IARL	0x4941524C
# define CHUNK_ID_IART	0x49415254
# define CHUNK_ID_ICMS	0x49434D53
# define CHUNK_ID_ICMT	0x49434D54
# define CHUNK_ID_ICOP	0x49434F50
# define CHUNK_ID_ICRD	0x49435244
# define CHUNK_ID_IENG	0x49454E47
# define CHUNK_ID_IGNR	0x49474E52
# define CHUNK_ID_IKEY	0x494B4559
# define CHUNK_ID_IMED	0x494D4544
# define CHUNK_ID_INAM	0x494E414D
# define CHUNK_ID_IPRD	0x49505244
# define CHUNK_ID_ISBJ	0x4953424A
# define CHUNK_ID_ISFT	0x49534654
# define CHUNK_ID_ISRC	0x49535243
# define CHUNK_ID_ISRF	0x49535246
# define CHUNK_ID_ITCH	0x49544348
# define CHUNK_ID_VERS	0x76657273
# define CHUNK_ID_DLID	0x646C6964
# define CHUNK_ID_FMT	0x666D7420
# define CHUNK_ID_DATA	0x64617461
# define CHUNK_ID_INSH	0x696E7368
# define CHUNK_ID_RGNH	0x72676E68
# define CHUNK_ID_WLNK	0x776C6E6B
# define CHUNK_ID_PTBL	0x7074626C
# define CHUNK_ID_WSMP	0x77736D70
# define CHUNK_ID_COLH	0x636F6C68
#else  // little endian
# define LIST_TYPE_INFO	0x4F464E49
# define LIST_TYPE_WVPL	0x6C707677
# define LIST_TYPE_DWPL 0x6C707764  ///< Seen on some files instead of a wvpl list chunk.
# define LIST_TYPE_WAVE	0x65766177
# define LIST_TYPE_LINS	0X736E696C
# define LIST_TYPE_INS	0X20736E69
# define LIST_TYPE_LRGN	0x6E67726C
# define LIST_TYPE_LART	0x7472616C
# define LIST_TYPE_LAR2	0x3272616C
# define LIST_TYPE_RGN	0x206E6772
# define LIST_TYPE_RGN2	0x326E6772
# define LIST_TYPE_ART1	0x31747261
# define LIST_TYPE_ART2	0x32747261
# define CHUNK_ID_IARL	0x4C524149
# define CHUNK_ID_IART	0x54524149
# define CHUNK_ID_ICMS	0x534D4349
# define CHUNK_ID_ICMT	0x544D4349
# define CHUNK_ID_ICOP	0x504F4349
# define CHUNK_ID_ICRD	0x44524349
# define CHUNK_ID_IENG	0x474E4549
# define CHUNK_ID_IGNR	0x524E4749
# define CHUNK_ID_IKEY	0x59454B49
# define CHUNK_ID_IMED	0x44525049
# define CHUNK_ID_INAM	0x4D414E49
# define CHUNK_ID_IPRD	0x44525049
# define CHUNK_ID_ISBJ	0x4A425349
# define CHUNK_ID_ISFT	0x54465349
# define CHUNK_ID_ISRC	0x43525349
# define CHUNK_ID_ISRF	0x46525349
# define CHUNK_ID_ITCH	0x48435449
# define CHUNK_ID_VERS	0x73726576
# define CHUNK_ID_DLID	0x64696C64
# define CHUNK_ID_FMT	0x20746D66
# define CHUNK_ID_DATA	0x61746164
# define CHUNK_ID_INSH	0x68736E69
# define CHUNK_ID_RGNH	0x686E6772
# define CHUNK_ID_WLNK	0x6B6E6C77
# define CHUNK_ID_PTBL	0x6C627470
# define CHUNK_ID_WSMP	0x706D7377
# define CHUNK_ID_COLH	0x686C6F63
#endif // WORDS_BIGENDIAN

#define WAVE_FORMAT_PCM			0x0001

#define DRUM_TYPE_MASK			0x00000001

#define F_RGN_OPTION_SELFNONEXCLUSIVE	0x0001

#define F_WAVELINK_PHASE_MASTER		0x0001
#define F_WAVELINK_MULTICHANNEL		0x0002

#define F_WSMP_NO_TRUNCATION		0x0001
#define F_WSMP_NO_COMPRESSION		0x0002

#define MIDI_BANK_COARSE(x)		((x & 0x00007F00) >> 8)			// CC0
#define MIDI_BANK_FINE(x)		(x & 0x0000007F)			// CC32
#define MIDI_BANK_MERGE(coarse, fine)	((((uint16_t) coarse) << 7) | fine)	// CC0 + CC32
#define CONN_TRANSFORM_SRC(x)		((x >> 10) & 0x000F)
#define CONN_TRANSFORM_CTL(x)		((x >> 4) & 0x000F)
#define CONN_TRANSFORM_DST(x)		(x & 0x000F)
#define CONN_TRANSFORM_BIPOLAR_SRC(x)	(x & 0x4000)
#define CONN_TRANSFORM_BIPOLAR_CTL(x)	(x & 0x0100)
#define CONN_TRANSFORM_INVERT_SRC(x)	(x & 0x8000)
#define CONN_TRANSFORM_INVERT_CTL(x)	(x & 0x0200)


//TODO: no support for conditional chunks <cdl> yet

/** DLS specific classes and definitions */
namespace DLS {

    typedef std::string String;

    /** Quadtuple version number ("major.minor.release.build"). */
    struct version_t {
        uint16_t minor;
        uint16_t major;
        uint16_t build;
        uint16_t release;
    };

    /** Every subject of an DLS file and the file itself can have an unique, computer generated ID. */
    struct dlsid_t {
        uint32_t ulData1;
        uint16_t usData2;
        uint16_t usData3;
        uint8_t  abData[8];
    };

    /** Connection Sources */
    typedef enum {
        // Modulator Sources
        conn_src_none            = 0x0000,
        conn_src_lfo             = 0x0001,
        conn_src_keyonvelocity   = 0x0002,
        conn_src_keynumber       = 0x0003,
        conn_src_eg1             = 0x0004,
        conn_src_eg2             = 0x0005,
        conn_src_pitchwheel      = 0x0006,
        conn_src_polypressure    = 0x0007,
        conn_src_channelpressure = 0x0008,
        conn_src_vibrato         = 0x0009,
        // MIDI Controller Sources
        conn_src_cc1             = 0x0081,
        conn_src_cc7             = 0x0087,
        conn_src_cc10            = 0x008A,
        conn_src_cc11            = 0x008B,
        conn_src_cc91            = 0x00DB,
        conn_src_cc93            = 0x00DD,
        // Registered Parameter Numbers
        conn_src_rpn0            = 0x0100,
        conn_src_rpn1            = 0x0101,
        conn_src_rpn2            = 0x0102
    } conn_src_t;

    /** Connection Destinations */
    typedef enum {
        // Generic Destinations
        conn_dst_none             = 0x0000,
        conn_dst_gain             = 0x0001,
        conn_dst_reserved         = 0x0002,
        conn_dst_pitch            = 0x0003,
        conn_dst_pan              = 0x0004,
        conn_dst_keynumber        = 0x0005,
        // Channel Output Destinations
        conn_dst_left             = 0x0010,
        conn_dst_right            = 0x0011,
        conn_dst_center           = 0x0012,
        conn_dst_lfe_channel      = 0x0013,
        conn_dst_leftrear         = 0x0014,
        conn_dst_rightrear        = 0x0015,
        conn_dst_chorus           = 0x0080,
        conn_dst_reverb           = 0x0081,
        // Modulator LFO Destinations
        conn_dst_lfo_frequency    = 0x0104,
        conn_dst_lfo_startdelay   = 0x0105,
        // Vibrato LFO Destinations
        conn_dst_vib_frequency    = 0x0114,
        conn_dst_vib_startdelay   = 0x0115,
        // EG Destinations
        conn_dst_eg1_attacktime   = 0x0206,
        conn_dst_eg1_decaytime    = 0x0207,
        conn_dst_eg1_reserved     = 0x0208,
        conn_dst_eg1_releasetime  = 0x0209,
        conn_dst_eg1_sustainlevel = 0x020A,
        conn_dst_eg1_delaytime    = 0x020B,
        conn_dst_eg1_holdtime     = 0x020C,
        conn_dst_eg1_shutdowntime = 0x020D,
        conn_dst_eg2_attacktime   = 0x030A,
        conn_dst_eg2_decaytime    = 0x030B,
        conn_dst_eg2_reserved     = 0x030C,
        conn_dst_eg2_releasetime  = 0x030D,
        conn_dst_eg2_sustainlevel = 0x030E,
        conn_dst_eg2_delaytime    = 0x030F,
        conn_dst_eg2_holdtime     = 0x0310,
        // Filter Destinations
        conn_dst_filter_cutoff    = 0x0500,
        conn_dst_filter_q         = 0x0501
    } conn_dst_t;

    /** Connection Transforms */
    typedef enum {
        conn_trn_none    = 0x0000,
        conn_trn_concave = 0x0001,
        conn_trn_convex  = 0x0002,
        conn_trn_switch  = 0x0003
    } conn_trn_t;

    /** Lower and upper limit of a range. */
    struct range_t {
        uint16_t low;  ///< Low value of range.
        uint16_t high; ///< High value of range.
    };

    /** Defines Sample Loop Points. */
    struct sample_loop_t {
        uint32_t Size;
        uint32_t LoopType;
        uint32_t LoopStart;
        uint32_t LoopLength;
    };

    // just symbol prototyping
    class File;
    class Instrument;
    class Region;
    class Sample;

    /** Defines a connection within the synthesis model. */
    class Connection {
        public:
            conn_src_t Source;
            conn_trn_t SourceTransform;
            bool       SourceInvert;
            bool       SourceBipolar;
            conn_src_t Control;
            conn_trn_t ControlTransform;
            bool       ControlInvert;
            bool       ControlBipolar;
            conn_dst_t Destination;
            conn_trn_t DestinationTransform;
            uint32_t   Scale;
        protected:
            struct conn_block_t {
                uint16_t source;
                uint16_t control;
                uint16_t destination;
                uint16_t transform;
                uint32_t scale;
            };
            Connection() {};
            void Init(conn_block_t* Header);
            virtual ~Connection() {};
            friend class Articulation;
    };

    /** Provides access to the defined connections used for the synthesis model. */
    class Articulation {
        public:
            Connection*  pConnections; ///< Points to the beginning of a <i>Connection</i> array.
            uint32_t     Connections;  ///< Reflects the number of Connections.
            Articulation(RIFF::List* artList);
           ~Articulation();
    };

    /** Abstract base class for classes that provide articulation information (thus for <i>Instrument</i> and <i>Region</i> class). */
    class Articulator {
        public:
            Articulator(RIFF::List* ParentList);
            Articulation* GetFirstArticulation();
            Articulation* GetNextArticulation();
        protected:
            typedef std::list<Articulation*> ArticulationList;
            RIFF::List*                 pParentList;
            ArticulationList*           pArticulations;
            ArticulationList::iterator  ArticulationsIterator;

            void LoadArticulations();
           ~Articulator();
    };

    /** Optional information for DLS files, instruments, samples, etc. */
    class Info {
        public:
            String Name;             ///< <INAM-ck>. Stores the title of the subject of the file, such as, Seattle From Above.
            String ArchivalLocation; ///< <IARL-ck>. Indicates where the subject of the file is stored.
            String CreationDate;     ///< <ICRD-ck>. Specifies the date the subject of the file was created. List dates in yyyy-mm-dd format.
            String Comments;         ///< <ICMT-ck>. Provides general comments about the file or the subject of the file. Sentences might end with semicolon.
            String Product;          ///< <IPRD-ck>. Specifies the name of the title the file was originally intended for, such as World Ruler V.
            String Copyright;        ///< <ICOP-ck>. Records the copyright information for the file.
            String Artists;          ///< <IART-ck>. Lists the artist of the original subject of the file.
            String Genre;            ///< <IGNR-ck>. Descirbes the original work, such as, Jazz, Classic, Rock, Techno, Rave, etc.
            String Keywords;         ///< <IKEY-ck>. Provides a list of keywords that refer to the file or subject of the file. Keywords are separated with semicolon and blank, e.g., FX; death; murder.
            String Engineer;         ///< <IENG-ck>. Stores the name of the engineer who worked on the file. Multiple engineer names are separated by semicolon and blank, e.g, Smith, John; Adams, Joe.
            String Technician;       ///< <ITCH-ck>. Identifies the technician who sampled the subject file.
            String Software;         ///< <ISFT-ck>. Identifies the name of the sofware package used to create the file.
            String Medium;           ///< <IMED-ck>. Describes the original subject of the file, such as, record, CD, and so forth.
            String Source;           ///< <ISRC-ck>. Identifies the name of the person or organization who supplied the original subject of the file.
            String SourceForm;       ///< <ISRF-ck>. Identifies the original form of the material that was digitized, such as record, sampling CD, TV sound track. This is not neccessarily the same as <i>Medium</i>.
            String Commissioned;     ///< <ICMS-ck>. Lists the name of the person or organization that commissioned the subject of the file, e.g., Pope Julian II.

            Info(RIFF::List* list);
        private:
            inline void LoadString(uint32_t ChunkID, RIFF::List* lstINFO, String& s) {
                RIFF::Chunk* ck = lstINFO->GetSubChunk(ChunkID);
                if (ck) {
                    // TODO: no check for ZSTR terminated strings yet
                    s = (char*) ck->LoadChunkData();
                    ck->ReleaseChunkData();
                }
            }
    };

    /** Abstract base class which encapsulates data structures which all DLS resources are able to provide. */
    class Resource {
        public:
            Info*    pInfo;        ///< Points (in any case) to an <i>Info</i> object, providing additional, optional infos and comments.
            dlsid_t* pDLSID;       ///< Points to a <i>dlsid_t</i> structure if the file provided a DLS ID else is <i>NULL</i>.

            Resource* GetParent() { return pParent; };
        protected:
            Resource* pParent;

            Resource(Resource* Parent, RIFF::List* lstResource);
           ~Resource();
    };

    /** Abstract base class which provides mandatory informations about sample players in general. */
    class Sampler {
        public:
            uint8_t        UnityNote;
            int16_t        FineTune;
            int32_t        Gain;
            bool           NoSampleDepthTruncation;
            bool           NoSampleCompression;
            uint32_t       SampleLoops;              ///< Reflects the number of sample loops.
            sample_loop_t* pSampleLoops;             ///< Points to the beginning of a sample loop array, or is NULL if there are no loops defined.
        protected:
            uint32_t       SamplerOptions;
            Sampler(RIFF::List* ParentList);
           ~Sampler();
    };

    /** Encapsulates sample waves used for playback. */
    class Sample : public Resource {
        public:
            uint16_t      FormatTag;             ///< Format ID of the waveform data (should be WAVE_FORMAT_PCM for DLS1 compliant files).
            uint16_t      Channels;              ///< Number of channels represented in the waveform data, e.g. 1 for mono, 2 for stereo ().
            uint32_t      SamplesPerSecond;      ///< Sampling rate at which each channel should be played.
            uint32_t      AverageBytesPerSecond; ///< The average number of bytes per second at which the waveform data should be transferred (Playback software can estimate the buffer size using this value).
            uint16_t      BlockAlign;            ///< The block alignment (in bytes) of the waveform data. Playback software needs to process a multiple of <i>BlockAlign</i> bytes of data at a time, so the value of <i>BlockAlign</i> can be used for buffer alignment.
            uint16_t      BitDepth;              ///< Size of each sample per channel (only if known sample data format is used, 0 otherwise).
            unsigned long SamplesTotal;          ///< Reflects total number of samples (only if known sample data format is used, 0 otherwise).
            uint          FrameSize;             ///< Reflects the size (in bytes) of one single sample (only if known sample data format is used, 0 otherwise).

            void*         LoadSampleData();              ///< Load sample data into RAM. Returns a pointer to the data in RAM on success, <i>NULL</i> otherwise.
            void          ReleaseSampleData();           ///< Release the samples once you used them if you don't want to be bothered to.
            unsigned long SetPos(unsigned long SampleCount, RIFF::stream_whence_t Whence = RIFF::stream_start);
            unsigned long Read(void* pBuffer, unsigned long SampleCount);
        protected:
            RIFF::Chunk*  pCkData;
            RIFF::Chunk*  pCkFormat;
            unsigned long ulWavePoolOffset;  // needed for comparison with the wave pool link table, thus the link to instruments

            Sample(File* pFile, RIFF::List* waveList, unsigned long WavePoolOffset);
            friend class File;
            friend class Region; // Region has to compare the wave pool offset to get its sample
    };

    /** Defines <i>Region</i> information of an <i>Instrument</i>. */
    class Region : public Resource, public Articulator, public Sampler {
        public:
            range_t     KeyRange;
            range_t     VelocityRange;
            uint16_t    KeyGroup;
            uint16_t    Layer;
            bool        SelfNonExclusive;
            bool        PhaseMaster;
            uint16_t    PhaseGroup;
            bool        MultiChannel;
            uint32_t    Channel;

            Sample*     GetSample();
        protected:
            RIFF::List* pCkRegion;
            uint32_t    WavePoolTableIndex; // index in the wave pool table to the sample wave this region is linked to
            Sample*     pSample;            // every region refers to exactly one sample

            Region(Instrument* pInstrument, RIFF::List* rgnList);
           ~Region();
            friend class Instrument;
    };

    /** Provides all neccessary information for the synthesis of a DLS <i>Instrument</i>. */
    class Instrument : public Resource, public Articulator {
        public:
            bool     IsDrum;         ///< Indicates if the <i>Instrument</i> is a drum type, as they differ in the synthesis model of DLS from melodic instruments.
            uint16_t MIDIBank;       ///< Reflects combination of <i>MIDIBankCoarse</i> and <i>MIDIBankFine</i> (bank 1 - bank 16384).
            uint8_t  MIDIBankCoarse; ///< Reflects the MIDI Bank number for MIDI Control Change 0 (bank 1 - 128).
            uint8_t  MIDIBankFine;   ///< Reflects the MIDI Bank number for MIDI Control Change 32 (bank 1 - 128).
            uint32_t MIDIProgram;    ///< Specifies the MIDI Program Change Number this Instrument should be assigned to.
            uint32_t Regions;        ///< Reflects the number of <i>Region</i> defintions this Instrument has.

            Region*  GetFirstRegion();
            Region*  GetNextRegion();
        protected:
            typedef std::list<Region*> RegionList;
            struct midi_locale_t {
                uint32_t bank;
                uint32_t instrument;
            };

            RIFF::List*          pCkInstrument;
            RegionList*          pRegions;
            RegionList::iterator RegionsIterator;

            Instrument(File* pFile, RIFF::List* insList);
            void LoadRegions();
           ~Instrument();
            friend class File;
    };

    /** Parses DLS Level 1 and 2 compliant files and provides abstract access to the data. */
    class File : public Resource {
        public:
            version_t* pVersion;              ///< Points to a <i>version_t</i> structure if the file provided a version number else is set to <i>NULL</i>.
            uint32_t   Instruments;           ///< Reflects the number of available <i>Instrument</i> objects.

            File(RIFF::File* pRIFF);
            Sample*     GetFirstSample();     ///< Returns a pointer to the first <i>Sample</i> object of the file, <i>NULL</i> otherwise.
            Sample*     GetNextSample();      ///< Returns a pointer to the next <i>Sample</i> object of the file, <i>NULL</i> otherwise.
            Instrument* GetFirstInstrument(); ///< Returns a pointer to the first <i>Instrument</i> object of the file, <i>NULL</i> otherwise.
            Instrument* GetNextInstrument();  ///< Returns a pointer to the next <i>Instrument</i> object of the file, <i>NULL</i> otherwise.
           ~File();
        protected:
            typedef std::list<Sample*>     SampleList;
            typedef std::list<Instrument*> InstrumentList;

            RIFF::File*              pRIFF;
            SampleList*              pSamples;
            SampleList::iterator     SamplesIterator;
            InstrumentList*          pInstruments;
            InstrumentList::iterator InstrumentsIterator;
            uint32_t                 WavePoolCount;
            uint32_t*                pWavePoolTable;

            void LoadSamples();
            void LoadInstruments();
            friend class Region; // Region has to look in the wave pool table to get its sample
    };

    /** Will be thrown whenever a DLS specific error occurs while trying to access a DLS File. */
    class Exception : public RIFF::Exception {
        public:
            Exception(String Message);
            void PrintMessage();
    };

} // namespace DLS

#endif // __DLS_H__
