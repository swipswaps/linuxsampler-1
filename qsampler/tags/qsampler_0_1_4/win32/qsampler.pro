INCLUDEPATH	+= ../src ../../liblscp

HEADERS	 += ../src/qsamplerAbout.h \
			../src/qsamplerOptions.h \
			../src/qsamplerChannel.h \
			../src/qsamplerMessages.h \
			../src/qsamplerInstrument.h \
			../src/qsamplerInstrumentList.h \
			../src/qsamplerDevice.h

SOURCES	 += ../src/main.cpp \
			../src/qsamplerOptions.cpp \
			../src/qsamplerChannel.cpp \
			../src/qsamplerMessages.cpp \
			../src/qsamplerInstrument.cpp \
			../src/qsamplerInstrumentList.cpp \
			../src/qsamplerDevice.cpp

FORMS	  = ../src/qsamplerMainForm.ui \
			../src/qsamplerChannelStrip.ui \
			../src/qsamplerChannelForm.ui \
			../src/qsamplerOptionsForm.ui \
			../src/qsamplerInstrumentForm.ui \
			../src/qsamplerInstrumentListForm.ui \
			../src/qsamplerDeviceForm.ui

IMAGES	  = ../icons/qsampler.png \
			../icons/qsamplerChannel.png \
			../icons/qsamplerInstrument.png \
			../icons/qsamplerDevice.png \
			../icons/fileNew.png \
			../icons/fileOpen.png \
			../icons/fileSave.png \
			../icons/fileRestart.png \
			../icons/fileReset.png \
			../icons/editAddChannel.png \
			../icons/editRemoveChannel.png \
			../icons/editSetupChannel.png \
			../icons/editResetChannel.png \
			../icons/editResetAllChannels.png \
			../icons/channelsArrange.png \
			../icons/itemGroup.png \
			../icons/itemGroupOpen.png \
			../icons/itemGroupNew.png \
			../icons/itemNew.png \
			../icons/itemFile.png \
			../icons/formEdit.png \
			../icons/formRemove.png \
			../icons/formAccept.png \
			../icons/formReject.png \
			../icons/formRefresh.png \
			../icons/displaybg1.png \
			../icons/deviceCreate.png \
			../icons/deviceDelete.png \
			../icons/midi1.png \
			../icons/midi2.png \
			../icons/audio1.png \
			../icons/audio2.png

TEMPLATE = app
CONFIG  += qt warn_on debug console
LANGUAGE = C++

LIBS    += ../../liblscp/win32/liblscp.lib

