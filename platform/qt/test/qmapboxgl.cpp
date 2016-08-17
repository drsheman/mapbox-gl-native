#include <mbgl/test/util.hpp>
#include <mbgl/util/io.hpp>

#include <QApplication>
#include <QGLWidget>
#include <QMapbox>
#include <QMapboxGL>

// FIXME: Enable on CI when the tests go 100% offline.
#define SKIP_CI \
    if (!qgetenv("CI").isEmpty()) { SUCCEED(); return; }

class QMapboxGLTest : public QObject, public ::testing::Test {
    Q_OBJECT

public:
    QMapboxGLTest() : app(argc, const_cast<char**>(&argv)), map(nullptr, settings) {
        connect(&map, SIGNAL(mapChanged(QMapboxGL::MapChange)),
                this, SLOT(onMapChanged(QMapboxGL::MapChange)));
        connect(&map, SIGNAL(needsRendering()),
                this, SLOT(onNeedsRendering()));

        widget.makeCurrent();
        QMapbox::initializeGLExtensions();

        map.resize(QSize(512, 512));
        map.setCoordinateZoom(QMapbox::Coordinate(60.170448, 24.942046), 14);
    }

    void runUntil(QMapboxGL::MapChange status) {
        changeCallback = [&](QMapboxGL::MapChange change) {
            if (change == status) {
                app.exit();
                changeCallback = nullptr;
            }
        };

        app.exec();
    }

private:
    int argc = 1;
    const char* argv = "mbgl-test";

    QApplication app;
    QGLWidget widget;

protected:
    QMapboxGLSettings settings;
    QMapboxGL map;

    std::function<void(QMapboxGL::MapChange)> changeCallback;

private slots:
    void onMapChanged(QMapboxGL::MapChange change) {
        if (changeCallback) {
            changeCallback(change);
        }
    };

    void onNeedsRendering() {
        map.render();
    };
};

TEST_F(QMapboxGLTest, styleJSON) {
    SKIP_CI;

    QString json = QString::fromStdString(
        mbgl::util::read_file("test/fixtures/resources/style_vector.json"));

    map.setStyleJSON(json);
    ASSERT_EQ(map.styleJSON(), json);
    runUntil(QMapboxGL::MapChangeDidFinishLoadingMap);

    map.setStyleJSON("invalid json");
    runUntil(QMapboxGL::MapChangeDidFailLoadingMap);

    map.setStyleJSON("\"\"");
    runUntil(QMapboxGL::MapChangeDidFailLoadingMap);

    map.setStyleJSON(QString());
    runUntil(QMapboxGL::MapChangeDidFailLoadingMap);
}

TEST_F(QMapboxGLTest, styleURL) {
    SKIP_CI;

    QString url(QMapbox::defaultStyles()[0].first);

    map.setStyleURL(url);
    ASSERT_EQ(map.styleURL(), url);
    runUntil(QMapboxGL::MapChangeDidFinishLoadingMap);

    map.setStyleURL("invalid://url");
    runUntil(QMapboxGL::MapChangeDidFailLoadingMap);

    map.setStyleURL(QString());
    runUntil(QMapboxGL::MapChangeDidFailLoadingMap);
}

#include "qmapboxgl.moc"
