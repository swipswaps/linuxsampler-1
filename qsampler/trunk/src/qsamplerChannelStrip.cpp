// qsamplerChannelStrip.cpp
//
/****************************************************************************
   Copyright (C) 2004-2007, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2007, Christian Schoenebeck

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qsamplerChannelStrip.h"

#include "qsamplerMainForm.h"

#include <q3dragobject.h>

#include <QUrl>

#include <math.h>

// Channel status/usage usage limit control.
#define QSAMPLER_ERROR_LIMIT	3

namespace QSampler {

ChannelStrip::ChannelStrip(QWidget* parent, Qt::WFlags f) : QWidget(parent, f) {
    ui.setupUi(this);

    // Initialize locals.
    m_pChannel     = NULL;
    m_iDirtyChange = 0;
    m_iErrorCount  = 0;

    // Try to restore normal window positioning.
    adjustSize();

	QObject::connect(ui.ChannelSetupPushButton,
		SIGNAL(clicked()),
		SLOT(channelSetup()));
	QObject::connect(ui.ChannelMutePushButton,
		SIGNAL(toggled(bool)),
		SLOT(channelMute(bool)));
	QObject::connect(ui.ChannelSoloPushButton,
		SIGNAL(toggled(bool)),
		SLOT(channelSolo(bool)));
	QObject::connect(ui.VolumeSlider,
		SIGNAL(valueChanged(int)),
		SLOT(volumeChanged(int)));
	QObject::connect(ui.VolumeSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(volumeChanged(int)));
	QObject::connect(ui.ChannelEditPushButton,
		SIGNAL(clicked()),
		SLOT(channelEdit()));
}

ChannelStrip::~ChannelStrip() {
    // Destroy existing channel descriptor.
    if (m_pChannel)
        delete m_pChannel;
    m_pChannel = NULL;
}


// Drag'n'drop file handler.
bool ChannelStrip::decodeDragFile ( const QMimeSource *pEvent, QString& sInstrumentFile )
{
	if (m_pChannel == NULL)
		return false;
	if (Q3TextDrag::canDecode(pEvent)) {
		QString sText;
		if (Q3TextDrag::decode(pEvent, sText)) {
			QStringList files = QStringList::split('\n', sText);
			for (QStringList::Iterator iter = files.begin(); iter != files.end(); iter++) {
				*iter = QUrl((*iter).stripWhiteSpace().replace(QRegExp("^file:"), QString::null)).path();
				if (qsamplerChannel::isInstrumentFile(*iter)) {
					sInstrumentFile = *iter;
					return true;
				}
			}
		}
	}
	// Fail.
	return false;
}


// Window drag-n-drop event handlers.
void ChannelStrip::dragEnterEvent ( QDragEnterEvent* pDragEnterEvent )
{
	QString sInstrumentFile;
	pDragEnterEvent->accept(decodeDragFile(pDragEnterEvent, sInstrumentFile));
}


void ChannelStrip::dropEvent ( QDropEvent* pDropEvent )
{
	QString sInstrumentFile;

	if (decodeDragFile(pDropEvent, sInstrumentFile)) {
		// Go and set the dropped instrument filename...
		m_pChannel->setInstrument(sInstrumentFile, 0);
		// Open up the channel dialog.
		channelSetup();
	}
}


// Channel strip setup formal initializer.
void ChannelStrip::setup ( qsamplerChannel *pChannel )
{
    // Destroy any previous channel descriptor;
    // (remember that once setup we own it!)
    if (m_pChannel)
        delete m_pChannel;

    // Set the new one...
    m_pChannel = pChannel;

    // Stabilize this around.
    updateChannelInfo();

	// We'll accept drops from now on...
	if (m_pChannel)
		setAcceptDrops(true);
}

// Channel secriptor accessor.
qsamplerChannel *ChannelStrip::channel (void)
{
    return m_pChannel;
}


// Messages view font accessors.
QFont ChannelStrip::displayFont (void)
{
    return ui.EngineNameTextLabel->font();
}

void ChannelStrip::setDisplayFont ( const QFont & font )
{
    ui.EngineNameTextLabel->setFont(font);
    ui.MidiPortChannelTextLabel->setFont(font);
    ui.InstrumentNameTextLabel->setFont(font);
    ui.InstrumentStatusTextLabel->setFont(font);
}


// Channel display background effect.
void ChannelStrip::setDisplayEffect ( bool bDisplayEffect )
{
    QPixmap pm =
        (bDisplayEffect) ?
            QPixmap(":/icons/displaybg1.png") : QPixmap();
    setDisplayBackground(pm);
}


// Update main display background pixmap.
void ChannelStrip::setDisplayBackground ( const QPixmap& pm )
{
    // Set the main origin...
    ui.ChannelInfoFrame->setPaletteBackgroundPixmap(pm);

    // Iterate for every child text label...
    QList<QObject*> list = ui.ChannelInfoFrame->queryList("QLabel");
    for (QList<QObject*>::iterator iter = list.begin(); iter != list.end(); iter++) {
        static_cast<QLabel*>(*iter)->setPaletteBackgroundPixmap(pm);
    }

    // And this standalone too.
    ui.StreamVoiceCountTextLabel->setPaletteBackgroundPixmap(pm);
}


// Maximum volume slider accessors.
void ChannelStrip::setMaxVolume ( int iMaxVolume )
{
    m_iDirtyChange++;
    ui.VolumeSlider->setRange(0, iMaxVolume);
    ui.VolumeSpinBox->setRange(0, iMaxVolume);
    m_iDirtyChange--;
}


// Channel setup dialog slot.
bool ChannelStrip::channelSetup (void)
{
	if (m_pChannel == NULL)
		return false;

	// Invoke the channel setup dialog.
	bool bResult = m_pChannel->channelSetup(this);
	// Notify that this channel has changed.
	if (bResult)
		emit channelChanged(this);

	return bResult;
}


// Channel mute slot.
bool ChannelStrip::channelMute ( bool bMute )
{
	if (m_pChannel == NULL)
		return false;

	// Invoke the channel mute method.
	bool bResult = m_pChannel->setChannelMute(bMute);
	// Notify that this channel has changed.
	if (bResult)
		emit channelChanged(this);

	return bResult;
}


// Channel solo slot.
bool ChannelStrip::channelSolo ( bool bSolo )
{
	if (m_pChannel == NULL)
		return false;

	// Invoke the channel solo method.
	bool bResult = m_pChannel->setChannelSolo(bSolo);
	// Notify that this channel has changed.
	if (bResult)
		emit channelChanged(this);

	return bResult;
}


// Channel edit slot.
void ChannelStrip::channelEdit (void)
{
	if (m_pChannel == NULL)
		return;

	m_pChannel->editChannel();
}


// Channel reset slot.
bool ChannelStrip::channelReset (void)
{
	if (m_pChannel == NULL)
		return false;

	// Invoke the channel reset method.
	bool bResult = m_pChannel->channelReset();
	// Notify that this channel has changed.
	if (bResult)
		emit channelChanged(this);

	return bResult;
}


// Update the channel instrument name.
bool ChannelStrip::updateInstrumentName ( bool bForce )
{
	if (m_pChannel == NULL)
		return false;

	// Do we refresh the actual name?
	if (bForce)
		m_pChannel->updateInstrumentName();

	// Instrument name...
	if (m_pChannel->instrumentName().isEmpty()) {
		if (m_pChannel->instrumentStatus() >= 0)
			ui.InstrumentNameTextLabel->setText(' ' + qsamplerChannel::loadingInstrument());
		else
			ui.InstrumentNameTextLabel->setText(' ' + qsamplerChannel::noInstrumentName());
	} else
		ui.InstrumentNameTextLabel->setText(' ' + m_pChannel->instrumentName());

	return true;
}


// Do the dirty volume change.
bool ChannelStrip::updateChannelVolume (void)
{
    if (m_pChannel == NULL)
        return false;

    // Convert...
#ifdef CONFIG_ROUND
    int iVolume = (int) ::round(100.0 * m_pChannel->volume());
#else
    double fIPart = 0.0;
    double fFPart = ::modf(100.0 * m_pChannel->volume(), &fIPart);
    int iVolume = (int) fIPart;
    if (fFPart >= +0.5)
        iVolume++;
    else
    if (fFPart <= -0.5)
        iVolume--;
#endif

    // And clip...
    if (iVolume < 0)
        iVolume = 0;

    // Flag it here, to avoid infinite recursion.
    m_iDirtyChange++;
    ui.VolumeSlider->setValue(iVolume);
    ui.VolumeSpinBox->setValue(iVolume);
    m_iDirtyChange--;

    return true;
}


// Update whole channel info state.
bool ChannelStrip::updateChannelInfo (void)
{
    if (m_pChannel == NULL)
        return false;

	// Check for error limit/recycle...
	if (m_iErrorCount > QSAMPLER_ERROR_LIMIT)
		return true;

    // Update strip caption.
    QString sText = m_pChannel->channelName();
    setCaption(sText);
    ui.ChannelSetupPushButton->setText(sText);

    // Check if we're up and connected.
	MainForm* pMainForm = MainForm::getInstance();
	if (pMainForm->client() == NULL)
		return false;

    // Read actual channel information.
    m_pChannel->updateChannelInfo();

    // Engine name...
    if (m_pChannel->engineName().isEmpty())
        ui.EngineNameTextLabel->setText(' ' + qsamplerChannel::noEngineName());
    else
        ui.EngineNameTextLabel->setText(' ' + m_pChannel->engineName());

	// Instrument name...
	updateInstrumentName(false);

    // MIDI Port/Channel...
	QString sMidiPortChannel = QString::number(m_pChannel->midiPort()) + " / ";
	if (m_pChannel->midiChannel() == LSCP_MIDI_CHANNEL_ALL)
		sMidiPortChannel += tr("All");
	else
		sMidiPortChannel += QString::number(m_pChannel->midiChannel() + 1);
	ui.MidiPortChannelTextLabel->setText(sMidiPortChannel);

    // Instrument status...
    int iInstrumentStatus = m_pChannel->instrumentStatus();
    if (iInstrumentStatus < 0) {
        ui.InstrumentStatusTextLabel->setPaletteForegroundColor(Qt::red);
        ui.InstrumentStatusTextLabel->setText(tr("ERR%1").arg(iInstrumentStatus));
        m_iErrorCount++;
        return false;
    }
    // All seems normal...
    ui.InstrumentStatusTextLabel->setPaletteForegroundColor(iInstrumentStatus < 100 ? Qt::yellow : Qt::green);
    ui.InstrumentStatusTextLabel->setText(QString::number(iInstrumentStatus) + '%');
    m_iErrorCount = 0;

#ifdef CONFIG_MUTE_SOLO
    // Mute/Solo button state coloring...
    const QColor& rgbNormal = ChannelSetupPushButton->paletteBackgroundColor();
    bool bMute = m_pChannel->channelMute();
    ChannelMutePushButton->setPaletteBackgroundColor(bMute ? Qt::yellow : rgbNormal);
    ChannelMutePushButton->setDown(bMute);
    bool bSolo = m_pChannel->channelSolo();
    ChannelSoloPushButton->setPaletteBackgroundColor(bSolo ? Qt::cyan : rgbNormal);
    ChannelSoloPushButton->setDown(bSolo);
#else
	ui.ChannelMutePushButton->setEnabled(false);
	ui.ChannelSoloPushButton->setEnabled(false);
#endif

    // And update the both GUI volume elements;
    // return success if, and only if, intrument is fully loaded...
    return updateChannelVolume() && (iInstrumentStatus == 100);
}


// Update whole channel usage state.
bool ChannelStrip::updateChannelUsage (void)
{
    if (m_pChannel == NULL)
        return false;

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm->client() == NULL)
		return false;

	// This only makes sense on fully loaded channels...
	if (m_pChannel->instrumentStatus() < 100)
	    return false;

    // Get current channel voice count.
    int iVoiceCount  = ::lscp_get_channel_voice_count(pMainForm->client(), m_pChannel->channelID());
    // Get current stream count.
    int iStreamCount = ::lscp_get_channel_stream_count(pMainForm->client(), m_pChannel->channelID());
    // Get current channel buffer fill usage.
    // As benno has suggested this is the percentage usage
    // of the least filled buffer stream...
    int iStreamUsage = ::lscp_get_channel_stream_usage(pMainForm->client(), m_pChannel->channelID());;

    // Update the GUI elements...
    ui.StreamUsageProgressBar->setValue(iStreamUsage);
    ui.StreamVoiceCountTextLabel->setText(QString("%1 / %2").arg(iStreamCount).arg(iVoiceCount));

    // We're clean.
    return true;
}


// Volume change slot.
void ChannelStrip::volumeChanged ( int iVolume )
{
    if (m_pChannel == NULL)
        return;

    // Avoid recursion.
    if (m_iDirtyChange > 0)
        return;

    // Convert and clip.
    float fVolume = (float) iVolume / 100.0;
    if (fVolume < 0.001)
        fVolume = 0.0;

    // Update the GUI elements.
    if (m_pChannel->setVolume(fVolume)) {
        updateChannelVolume();
        emit channelChanged(this);
    }
}


// Context menu event handler.
void ChannelStrip::contextMenuEvent( QContextMenuEvent *pEvent )
{
    if (m_pChannel == NULL)
        return;

    // We'll just show up the main form's edit menu (thru qsamplerChannel).
    m_pChannel->contextMenuEvent(pEvent);
}


// Error count hackish accessors.
void ChannelStrip::resetErrorCount (void)
{
	m_iErrorCount = 0;
}

} // namespace QSampler


// end of qsamplerChannelStrip.cpp