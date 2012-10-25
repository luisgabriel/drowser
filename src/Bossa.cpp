#include "Bossa.h"

#include <WebKit2/WKType.h>
#include <WebKit2/WKString.h>
#include <WebKit2/WKURL.h>
#include <WebKit2/WKPreferences.h>
#include <WebKit2/WKPreferencesPrivate.h>
#include <GL/gl.h>

#include "XlibEventUtils.h"


static const double DOUBLE_CLICK_INTERVAL = 300;

Bossa::Bossa()
    : m_window(new LinuxWindow(this, 1024, 600))
    , m_lastClickTime(0)
    , m_lastClickX(0)
    , m_lastClickY(0)
    , m_lastClickButton(Nix::MouseEvent::NoButton)
    , m_clickCount(0)
{
    m_mainLoop = g_main_loop_new(0, false);
    m_uiContext = WKContextCreate();
    m_uiPageGroup = WKPageGroupCreateWithIdentifier(WKStringCreateWithUTF8CString("Bossa"));
    
    WKPreferencesRef preferences = WKPageGroupGetPreferences(m_uiPageGroup);
    WKPreferencesSetAcceleratedCompositingEnabled(preferences, true);
    WKPreferencesSetFrameFlatteningEnabled(preferences, true);
    WKPreferencesSetDeveloperExtrasEnabled(preferences, true);
    
    m_uiView = Nix::WebView::create(m_uiContext, m_uiPageGroup, this);
    m_uiView->initialize();
    m_uiView->setFocused(true);
    m_uiView->setVisible(true);
    m_uiView->setActive(true);
    m_uiView->setSize(m_window->size().first, 60);
    WKPageLoadURL(m_uiView->pageRef(), WKURLCreateWithUTF8CString("file:///home/hugo/src/bossa/src/ui.html"));

    glViewport(0, 0, m_window->size().first, m_window->size().second);
    m_window->makeCurrent();
}

Bossa::~Bossa()
{
     g_main_loop_unref(m_mainLoop);
     delete m_uiView;
     WKRelease(m_uiContext);
     WKRelease(m_uiPageGroup);
     delete m_window;
}

int Bossa::run()
{
    g_main_loop_run(m_mainLoop);
    return 0;
}

void Bossa::handleExposeEvent()
{
    updateDisplay();
}

void Bossa::updateClickCount(const XButtonPressedEvent& event)
{
    if (m_lastClickX != event.x
        || m_lastClickY != event.y
        || m_lastClickButton != event.button
        || event.time - m_lastClickTime >= DOUBLE_CLICK_INTERVAL)
        m_clickCount = 1;
    else
        ++m_clickCount;

    m_lastClickX = event.x;
    m_lastClickY = event.y;
    m_lastClickButton = convertXEventButtonToNativeMouseButton(event.button);
    m_lastClickTime = event.time;
}

void Bossa::handleWheelEvent(const XButtonPressedEvent& event)
{
    // Same constant we use inside WebView to calculate the ticks. See also WebCore::Scrollbar::pixelsPerLineStep().
    const float pixelsPerStep = 40.0f;

    Nix::WheelEvent ev;
    ev.type = Nix::InputEvent::Wheel;
    ev.modifiers = convertXEventModifiersToNativeModifiers(event.state);
    ev.timestamp = convertXEventTimeToNixTimestamp(event.time);
    ev.x = event.x;
    ev.y = event.y;
    ev.globalX = event.x_root;
    ev.globalY = event.y_root;
    ev.delta = pixelsPerStep * (event.button == 4 ? 1 : -1);
    ev.orientation = event.state & Mod1Mask ? Nix::WheelEvent::Horizontal : Nix::WheelEvent::Vertical;
    m_uiView->sendEvent(ev);
}

void Bossa::handleButtonPressEvent(const XButtonPressedEvent& event)
{
    if (!m_uiView)
        return;

    if (event.button == 4 || event.button == 5) {
        handleWheelEvent(event);
        return;
    }

    updateClickCount(event);

    Nix::MouseEvent ev;
    ev.type = Nix::InputEvent::MouseDown;
    ev.button = convertXEventButtonToNativeMouseButton(event.button);
    ev.x = event.x;
    ev.y = event.y;
    ev.globalX = event.x_root;
    ev.globalY = event.y_root;
    ev.clickCount = m_clickCount;
    ev.modifiers = convertXEventModifiersToNativeModifiers(event.state);
    ev.timestamp = convertXEventTimeToNixTimestamp(event.time);
    
    m_uiView->sendEvent(ev);
}

void Bossa::handleButtonReleaseEvent(const XButtonReleasedEvent& event)
{
    if (!m_uiView || event.button == 4 || event.button == 5)
        return;

    Nix::MouseEvent ev;
    ev.type = Nix::InputEvent::MouseUp;
    ev.button = convertXEventButtonToNativeMouseButton(event.button);
    ev.x = event.x;
    ev.y = event.y;
    ev.globalX = event.x_root;
    ev.globalY = event.y_root;
    ev.clickCount = 0;
    ev.modifiers = convertXEventModifiersToNativeModifiers(event.state);
    ev.timestamp = convertXEventModifiersToNativeModifiers(event.state);
    m_uiView->sendEvent(ev);
}

void Bossa::handlePointerMoveEvent(const XPointerMovedEvent& event)
{
    if (!m_uiView)
        return;

    Nix::MouseEvent ev;
    ev.type = Nix::InputEvent::MouseMove;
    ev.button = Nix::MouseEvent::NoButton;
    ev.x = event.x;
    ev.y = event.y;
    ev.globalX = event.x_root;
    ev.globalY = event.y_root;
    ev.clickCount = 0;
    ev.modifiers = convertXEventModifiersToNativeModifiers(event.state);
    ev.timestamp = convertXEventTimeToNixTimestamp(event.time);
    m_uiView->sendEvent(ev);
}

void Bossa::handleSizeChanged(int width, int height)
{
    if (!m_uiView)
        return;

    m_uiView->setSize(width, height);
}

void Bossa::handleClosed()
{
    g_main_loop_quit(m_mainLoop);
}

void Bossa::viewNeedsDisplay(int, int, int, int)
{
    updateDisplay();
}

void Bossa::updateDisplay()
{
    std::pair<int, int> size = m_window->size();
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_uiView->paintToCurrentGLContext();

    m_window->swapBuffers();
}
