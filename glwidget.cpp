/*
 * Copyright (c) 2011 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtGui>
#include <QtOpenGL>
#include <QPalette>
#include <Mlt.h>
#include "glwidget.h"

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT GL_TEXTURE_RECTANGLE_NV
#endif

using namespace Mlt;

GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(parent)
    , Controller()
    , isShowingFrame(false)
    , m_image_width(0)
    , m_image_height(0)
    , m_texture(0)
    , m_display_ratio(4.0/3.0)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

GLWidget::~GLWidget()
{
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(40, 30);
}

QSize GLWidget::sizeHint() const
{
    return QSize(400, 300);
}

void GLWidget::initializeGL()
{
    QPalette palette;
    qglClearColor(palette.color(QPalette::Window));
    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void GLWidget::resizeGL(int width, int height)
{
    double this_aspect = (double) width / height;

    // Special case optimisation to negate odd effect of sample aspect ratio
    // not corresponding exactly with image resolution.
    if ((int) (this_aspect * 1000) == (int) (m_display_ratio * 1000))
    {
        w = width;
        h = height;
    }
    // Use OpenGL to normalise sample aspect ratio
    else if (height * m_display_ratio > width)
    {
        w = width;
        h = width / m_display_ratio;
    }
    else
    {
        w = height * m_display_ratio;
        h = height;
    }
    x = (width - w) / 2;
    y = (height - h) / 2;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    glMatrixMode(GL_MODELVIEW);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLWidget::resizeEvent(QResizeEvent* event)
{
    resizeGL(event->size().width(), event->size().height());
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_texture)
    {
#ifdef Q_WS_MAC
        glClear(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_TEXTURE_RECTANGLE_EXT);
        glBegin(GL_QUADS);
            glTexCoord2i(0, 0);
            glVertex2i  (x, y);
            glTexCoord2i(m_image_width - 1, 0);
            glVertex2i  (x + w - 1, y);
            glTexCoord2i(m_image_width - 1, m_image_height - 1);
            glVertex2i  (x + w - 1, y + h - 1);
            glTexCoord2i(0, m_image_height - 1);
            glVertex2i  (x, y + h - 1);
        glEnd();
        glDisable(GL_TEXTURE_RECTANGLE_EXT);
    }
}

void GLWidget::showFrame(QImage image)
{
    isShowingFrame = true;
    m_image_width = image.width();
    m_image_height = image.height();
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    glPixelStorei  (GL_UNPACK_ROW_LENGTH, m_image_width);
    glGenTextures  (1, &m_texture);
    glBindTexture  (GL_TEXTURE_RECTANGLE_EXT, m_texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D   (GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, m_image_width, m_image_height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    glDraw();
    isShowingFrame = false;
}

int GLWidget::open(const char* url, const char* profile)
{
    int error = Controller::open(url, profile);

    if (!error) {
        // use SDL for audio, OpenGL for video
        m_consumer = new Mlt::Consumer(*m_profile, "sdl_audio");
        if (m_consumer->is_valid()) {
            // Connect the producer to the consumer - tell it to "run" later
            m_consumer->connect(*m_producer);
            // Make an event handler for when a frame's image should be displayed
            m_consumer->listen("consumer-frame-show", this, (mlt_listener) on_frame_show);
            connect(this, SIGNAL(frameReceived(QImage,unsigned)), this, SLOT(showFrame(QImage)));
            isShowingFrame = false;
            m_consumer->start();
            m_display_ratio = m_profile->dar();
        }
        else {
            // Cleanup on error
            error = 2;
            Controller::close();
        }
    }
    return error;
}

// MLT consumer-frame-show event handler
void GLWidget::on_frame_show(mlt_consumer, void* self, mlt_frame frame_ptr)
{
    GLWidget* widget = static_cast<GLWidget*>(self);
    if (!widget->isShowingFrame) {
        Mlt::Frame* frame = new Mlt::Frame(frame_ptr);
        widget->isShowingFrame = true;
        emit widget->frameReceived(widget->getImage(frame), (unsigned) mlt_frame_get_position(frame_ptr));
        delete frame;
    }
}
