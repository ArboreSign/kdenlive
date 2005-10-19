/***************************************************************************
                          trackviewbackgrounddecorator  -  description
                             -------------------
    begin                : May 2005
    copyright            : (C) 2005 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "trackviewaudiobackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"
#include <qimage.h>

//only for testing please remove if video get drawn
#include <kstandarddirs.h>
namespace Gui{
TrackViewAudioBackgroundDecorator::TrackViewAudioBackgroundDecorator(KTimeLine* timeline,
												KdenliveDoc* doc,
												const QColor &selected,
												const QColor &unselected,
												int size) :
									DocTrackDecorator(timeline, doc),
									m_selected(selected),
									m_unselected(unselected),
									m_height(size)
{
}


TrackViewAudioBackgroundDecorator::~TrackViewAudioBackgroundDecorator()
{
}

// virtual
void TrackViewAudioBackgroundDecorator::paintClip(double startX, double endX, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
	int sx = startX; // (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = endX; //(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	//ex -= sx;
	int y=rect.y();
	int h=rect.height();
	
	if (m_height>0){
		y+=(h-m_height);
		h=m_height;
	}
	QColor col = selected ? m_selected : m_unselected;
	double aspect=4.0/3.0;
	int width=(int)(h)*aspect;
	int i=sx;

	QByteArray ba(width);
	
	int channels=2;
	painter.drawRect( sx, y, ex-sx, h);
	painter.fillRect( sx, y, ex-sx, h,col);
	for (;i<ex;i+=width){
		if (i+width<rect.x() || i>rect.x()+rect.width())
			continue;
		int deltaHeight=h/channels;
		for (int countChannel=0;countChannel<channels;countChannel++){
			document()->renderer()->getSoundSamples(document()->URL(),countChannel,1,1.0,&ba);
			drawChannel(countChannel,&ba,i,y+deltaHeight*countChannel,h/channels,ex,painter);	
		}
	}	
}
void TrackViewAudioBackgroundDecorator::drawChannel(int channel,QByteArray *ba,int x,int y,int height,int maxWidth,QPainter& painter)
{
	for (int a=0;a<ba->size();a++){
		int val=abs((*ba)[a])*(height/2)/128;
		
		if (a+x>=maxWidth)
			return;
		painter.drawLine(a+x,y+height/2-val,a+x,y+height/2+val);	
	}
}

};
