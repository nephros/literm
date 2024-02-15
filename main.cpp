/*
    Copyright 2011-2012 Heikki Holstila <heikki.holstila@gmail.com>

    This work is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This work is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this work.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qplatformdefs.h"

#include <QtGui>
#include <QtQml>
#include <QQuickView>
#include <QDir>
#include <QString>

#include "textrender.h"
#include "utilities.h"
#include "version.h"
#include "keyloader.h"

static void copyFileFromResources(QString from, QString to);

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("literm");

    QGuiApplication app(argc, argv);

    QScreen* sc = app.primaryScreen();
    if(sc){
        sc->setOrientationUpdateMask(Qt::PrimaryOrientation
                                     | Qt::LandscapeOrientation
                                     | Qt::PortraitOrientation
                                     | Qt::InvertedLandscapeOrientation
                                     | Qt::InvertedPortraitOrientation);
    }

    qmlRegisterType<TextRender>("literm", 1, 0, "TextRender");
    qmlRegisterUncreatableType<Util>("literm", 1, 0, "Util", "Util is created by app");
    QQuickView view;

#if defined(DESKTOP_BUILD)
    bool fullscreen = app.arguments().contains("-fullscreen");
#else
    bool fullscreen = !app.arguments().contains("-nofs");
#endif

    QSize screenSize = QGuiApplication::primaryScreen()->size();

    if (fullscreen) {
        view.setWidth(screenSize.width());
        view.setHeight(screenSize.height());
    } else {
        view.setWidth(screenSize.width() / 2);
        view.setHeight(screenSize.height() / 2);
    }

    QString settings_path(QDir::homePath() + "/.config/literm");
    QDir dir;

    if (!dir.exists(settings_path)) {
        // Migrate FingerTerm settings if present
        QString old_settings_path(QDir::homePath() + "/.config/FingerTerm");
        if (dir.exists(old_settings_path)) {
            if (!dir.rename(old_settings_path, settings_path))
                qWarning() << "Could not migrate FingerTerm settings path" << old_settings_path << "to" << settings_path;
        } else if (!dir.mkdir(settings_path))
            qWarning() << "Could not create literm settings path" << settings_path;
    }

    QString settingsFile = settings_path + "/settings.ini";


    Util util(settingsFile);

    QString startupErrorMsg;

    // copy the default config files to the config dir if they don't already exist
    copyFileFromResources(":/data/menu.xml", util.configPath()+"/menu.xml");
    copyFileFromResources(":/data/english.layout", util.configPath()+"/english.layout");
    copyFileFromResources(":/data/finnish.layout", util.configPath()+"/finnish.layout");
    copyFileFromResources(":/data/french.layout", util.configPath()+"/french.layout");
    copyFileFromResources(":/data/german.layout", util.configPath()+"/german.layout");
    copyFileFromResources(":/data/qwertz.layout", util.configPath()+"/qwertz.layout");

    KeyLoader keyLoader;
    keyLoader.setUtil(&util);
    bool ret = keyLoader.loadLayout(util.keyboardLayout());
    if(!ret) {
        // on failure, try to load the default one (english) directly from resources
        startupErrorMsg = "There was an error loading the keyboard layout.<br>\nUsing the default one instead.";
        util.setKeyboardLayout("english");
        ret = keyLoader.loadLayout(":/data/english.layout");
        if(!ret)
            qFatal("failure loading keyboard layout");
    }

    QQmlContext *context = view.rootContext();
    context->setContextProperty( "util", &util );
    context->setContextProperty( "keyLoader", &keyLoader );
    context->setContextProperty( "startupErrorMessage", startupErrorMsg);

    util.setWindow(&view);

    QObject::connect(view.engine(),SIGNAL(quit()),&app,SLOT(quit()));

    // Allow overriding the UX choice
    QString uxChoice;
    if (app.arguments().contains("-mobile"))
        uxChoice = "mobile";
    else if (app.arguments().contains("-desktop"))
        uxChoice = "desktop";

    if (uxChoice.isEmpty()) {
#if defined(MOBILE_BUILD)
        uxChoice = "mobile";
#else
        uxChoice =  "desktop";
#endif
    }

    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qml/" + uxChoice + "/Main.qml"));

    QObject *root = view.rootObject();
    if(!root)
        qFatal("no root object - qml error");

    if (fullscreen) {
        view.showFullScreen();
    } else {
        view.show();
    }

    return app.exec();
}

static void copyFileFromResources(QString from, QString to)
{
    // copy a file from resources to the config dir if it does not exist there
    QFileInfo toFile(to);
    if(!toFile.exists()) {
        QFile newToFile(toFile.absoluteFilePath());
        QResource res(from);
        if (newToFile.open(QIODevice::WriteOnly)) {
            newToFile.write( reinterpret_cast<const char*>(res.data()) );
            newToFile.close();
        } else {
            qWarning() << "Failed to copy default config from resources to" << toFile.filePath();
        }
    }
}
