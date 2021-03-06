/*
 gYAxis Implementation
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*/

#include <math.h>
#include <QDebug>
#include "gYAxis.h"
#include "SleepLib/profiles.h"

gYSpacer::gYSpacer(int spacer)
    :Layer(NoChannel)
{
    Q_UNUSED(spacer)
}

gXGrid::gXGrid(QColor col)
    :Layer(NoChannel)
{
    Q_UNUSED(col)

    m_major_color=QColor(180,180,180,64);
    //m_major_color=QColor(180,180,180,92);
    m_minor_color=QColor(230,230,230,64);
    m_show_major_lines=true;
    m_show_minor_lines=true;
}
gXGrid::~gXGrid()
{
}
void gXGrid::paint(gGraph & w,int left,int top, int width, int height)
{
    gVertexBuffer * stippled, * lines;

    int x,y;

    EventDataType miny=w.min_y;
    EventDataType maxy=w.max_y;

    if (miny<0) { // even it up if it's starts negative
        miny=-MAX(fabs(miny),fabs(maxy));
    }

    w.roundY(miny,maxy);

    //EventDataType dy=maxy-miny;

    if (height<0) return;

    static QString fd="0";
    GetTextExtent(fd,x,y);

    double max_yticks=round(height / (y+14.0)); // plus spacing between lines
    //double yt=1/max_yticks;

    double mxy=MAX(fabs(maxy),fabs(miny));
    double mny=miny;
    if (miny<0) {
        mny=-mxy;
    }
    double rxy=mxy-mny;

    int myt;
    bool fnd=false;
    for (myt=max_yticks;myt>=1;myt--) {
        float v=rxy/float(myt);
        if (float(v)==int(v)) {
            fnd=true;
            break;
        }
    }
    if (fnd) max_yticks=myt;
    else {
        max_yticks=2;
    }
    double yt=1/max_yticks;

    double ymult=height/rxy;

    double min_ytick=rxy*yt;

    float ty,h;

    if (min_ytick<=0) {
        qDebug() << "min_ytick error in gXGrid::paint() in" << w.title();
        return;
    }
    if (min_ytick>=1000000) {
        min_ytick=100;
    }


    stippled=w.backlines();
    lines=w.backlines();
    for (double i=miny; i<=maxy+min_ytick-0.00001; i+=min_ytick) {
        ty=(i - miny) * ymult;
        h=top+height-ty;
        if (m_show_major_lines && (i > miny)) {
            stippled->add(left,h,left+width,h,m_major_color.rgba());
        }
        double z=(min_ytick/4)*ymult;
        double g=h;
        for (int i=0;i<3;i++) {
            g+=z;
            if (g>top+height) break;
            //if (vertcnt>=maxverts) {
              //  qWarning() << "vertarray bounds exceeded in gYAxis for " << w.title() << "graph" << "MinY =" <<miny << "MaxY =" << maxy << "min_ytick=" <<min_ytick;
//                break;
  //          }
            if (m_show_minor_lines) {// && (i > miny)) {
                stippled->add(left,g,left+width,g,m_minor_color.rgba());
            }
            if (stippled->full()) {
                break;
            }
        }
        if (lines->full() || stippled->full()) {
            qWarning() << "vertarray bounds exceeded in gYAxis for " << w.title() << "graph" << "MinY =" <<miny << "MaxY =" << maxy << "min_ytick=" <<min_ytick;
            break;
        }
    }
}



gYAxis::gYAxis(QColor col)
:Layer(NoChannel)
{
    m_line_color=col;
    m_text_color=col;

    m_yaxis_scale=1;
}
gYAxis::~gYAxis()
{
}
void gYAxis::paint(gGraph & w,int left,int top, int width, int height)
{
    if (height<0) return;
    if (height>2000) return;
    int x,y;
    int labelW=0;

    EventDataType miny=w.min_y;
    EventDataType maxy=w.max_y;

    if (miny<0) { // even it up if it's starts negative
        miny=-MAX(fabs(miny),fabs(maxy));
    }

    w.roundY(miny,maxy);

    EventDataType dy=maxy-miny;

    static QString fd="0";
    GetTextExtent(fd,x,y);

    double max_yticks=round(height / (y+14.0)); // plus spacing between lines

    double mxy=MAX(fabs(maxy),fabs(miny));
    double mny=miny;
    if (miny<0) {
        mny=-mxy;
    }

    double rxy=mxy-mny;

    int myt;
    bool fnd=false;
    for (myt=max_yticks;myt>2;myt--) {
        float v=rxy/float(myt);
        if (v==int(v)) {
            fnd=true;
            break;
        }
    }
    if (fnd) max_yticks=myt;
    double yt=1/max_yticks;

    double ymult=height/rxy;

    double min_ytick=rxy*yt;


    //if (dy>5) {
    //    min_ytick=round(min_ytick);
    //} else {

    //}

    float ty,h;

    if (min_ytick<=0) {
        qDebug() << "min_ytick error in gYAxis::paint() in" << w.title();
        return;
    }
    if (min_ytick>=1000000) {
        min_ytick=100;
    }
    lines=w.backlines();

    GLuint line_color=m_line_color.rgba();
    for (double i=miny; i<=maxy+min_ytick-0.00001; i+=min_ytick) {
        ty=(i - miny) * ymult;
        if (dy<5) {
            fd=Format(i*m_yaxis_scale,2);
        } else {
            fd=Format(i*m_yaxis_scale,1);
        }

        GetTextExtent(fd,x,y); // performance bottleneck..

        if (x>labelW) labelW=x;
        h=top+height-ty;
        if (h<top) continue;
        w.renderText(fd,left+width-8-x,(h+(y/2.0)),0,m_text_color);

        lines->add(left+width-4,h,left+width,h,line_color);

        double z=(min_ytick/4)*ymult;
        double g=h;
        for (int i=0;i<3;i++) {
            g+=z;
            if (g>top+height) break;
            lines->add(left+width-3,g,left+width,g,line_color);
            if (lines->full()) {
                qWarning() << "vertarray bounds exceeded in gYAxis for " << w.title() << "graph" << "MinY =" <<miny << "MaxY =" << maxy << "min_ytick=" <<min_ytick;
                break;
            }
        }
        if (lines->full()) {
            qWarning() << "vertarray bounds exceeded in gYAxis for " << w.title() << "graph" << "MinY =" <<miny << "MaxY =" << maxy << "min_ytick=" <<min_ytick;
            break;
        }
    }
}
const QString gYAxis::Format(EventDataType v, int dp) {
   return QString::number(v,'f',dp);
}

bool gYAxis::mouseMoveEvent(QMouseEvent * event)
{
    Q_UNUSED(event)
    //int x=event->x();
    //int y=event->y();
    //qDebug() << "Hover at " << x << y;
    return false;
}


const QString gYAxisTime::Format(EventDataType v, int dp)
{
    int h=int(v) % 24;
    int m=int(v*60) % 60;
    int s=int(v*3600) % 60;

    char pm[3]={"am"};

    if (show_12hr) {

        h>=12 ? pm[0]='p' : pm[0]='a';
        h %= 12;
        if (h==0) h=12;
    } else {
        pm[0]=0;
    }
    if (dp>2) return QString().sprintf("%02i:%02i:%02i%s",h,m,s,pm);
    return QString().sprintf("%i:%02i%s",h,m,pm);
}

const QString gYAxisWeight::Format(EventDataType v, int dp)
{
    Q_UNUSED(dp)
    return weightString(v,m_unitsystem);
}
