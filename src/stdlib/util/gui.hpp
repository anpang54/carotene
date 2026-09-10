
#pragma once

/*

    There are 4 separate backends:
      - Windows:               Win32, linked normally
      - macOS, Linux, FreeBSD: GTK 4, with dlopen()
      - Haiku:                 BeAPI, linked normally
      - Web:                   DOM
    
    Maybe Cocoa and Qt later.

*/


// INCLUDES

#include <functional>

#if defined(__EMSCRIPTEN__)
    #define CARO_GUI_WEB

    #include "gui-styles.hpp"

#elif defined(_WIN32)
	#define CARO_GUI_WIN32

	#define WIN32_LEAN_AND_MEAN 
	#define NOMINMAX
	#define TokenType WindowsTokenType
	#include <windows.h>
	#undef TokenType

#elif defined(__HAIKU__)
	#define CARO_GUI_BEAPI
		
	#include <Application.h>
	#include <Window.h>
	#include <StringView.h>
	#include <Button.h>
	#include <LayoutBuilder.h>

#else
    #define CARO_GUI_GTK
	
    #include <dlfcn.h>
	
#endif

#include "../../core/format.hpp"


// block execution with asyncify

#ifdef CARO_GUI_WEB
    EM_ASYNC_JS(int, caroWaitForEvent, (), {
        if(Module.caroEvents.length == 0) await new Promise(resolve => Module.caroWake = resolve);
        return Module.caroEvents.shift();
    });
#endif


namespace CaroGui{


    // WINDOW DESCRIPTION

    enum WidgetType{
        WIDGET_LABEL, WIDGET_BUTTON
    };
    struct Widget{
        WidgetType type;
        string text;
    };

    struct Window{
        string title;
        int width  = 400;
        int height = 250;
        vector<Widget> widgets;
    };

    typedef std::function<bool(size_t widget)> ClickHandler;


    // WEB (acrylic elements)

    #ifdef CARO_GUI_WEB

        // add gui-styles.css and the google fonts it needs
        void addStyles() {
            EM_ASM({

                if(document.getElementById("caro-gui-styles")) return;    // already has the styles

                const link1 = document.createElement("link");
                link1.rel         = "preconnect";
                link1.href        = "https://fonts.googleapis.com";
                document.head.appendChild(link1);

                const link2 = document.createElement("link");
                link2.rel         = "preconnect";
                link2.href        = "https://fonts.gstatic.com";
                link2.crossOrigin = "";
                document.head.appendChild(link2);

                const link3 = document.createElement("link");
                link3.rel         = "stylesheet";
                link3.href        = "https://fonts.googleapis.com/css2?family=Inter&family=Roboto+Mono&family=Schibsted+Grotesk&display=swap";
                document.head.appendChild(link3);

                const style = document.createElement("style");
                style.id          = "caro-gui-styles";
                style.textContent = UTF8ToString($0);
                document.head.appendChild(style);

            }, GUI_STYLES);
        }

        void close() {
            EM_ASM({
                if(!Module.caroContainer) return;
                Module.caroContainer.remove();
                Module.caroContainer = null;
                Module.caroEvents = [];
                Module.caroPush(-1);
            });
        }

        string show(const Window& window, const ClickHandler& onClick) {

            if(EM_ASM_INT({ return Module.caroContainer? 1: 0; })) return "A window is already open.";

            addStyles();

            string rows;
            for(const Widget& widget: window.widgets) {
                rows += widget.type == WIDGET_BUTTON? "2.25em ": "auto ";
            }

            EM_ASM({

                const div = document.createElement("div");
                div.id = "caro-container";
                div.style.gridTemplateRows = UTF8ToString($1);
                document.title = UTF8ToString($0);
                document.body.appendChild(div);
                Module.caroContainer = div;

                // event queue
                Module.caroEvents = [];
                Module.caroPush = (event) => {
                    Module.caroEvents.push(event);
                    if(Module.caroWake) {
                        const wake = Module.caroWake;
                        Module.caroWake = null;
                        wake();
                    }
                };
                
            }, window.title.c_str(), rows.c_str());

            for(size_t i = 0; i < window.widgets.size(); ++i) {

                const Widget& widget = window.widgets[i];

                EM_ASM({

                    let element;
                    switch($0) {

                        case 0:
                            element = document.createElement("span");
                            break;

                        case 1:
                            element = document.createElement("button");
                            element.addEventListener("click", () => Module.caroPush($2));
                            break;

                    }

                    element.innerText = UTF8ToString($1);
                    Module.caroContainer.appendChild(element);

                }, widget.type, widget.text.c_str(), (int)i);

            }

            // block execution until the window gets closed
            while(true) {
                int event = caroWaitForEvent();
                if(event < 0) break;
                if(!onClick(event)) close();
            }

            return "";

        }


    // GTK

    #elifdef CARO_GUI_GTK


        // gtk functions

        #define gtkFunctions(f)\
            \
            /* GTK */\
            f(initCheck,            "gtk_init_check",              int,           (void))\
            f(windowNew,            "gtk_window_new",              void*,         (void))\
            f(windowSetTitle,       "gtk_window_set_title",        void,          (void*, const char*))\
            f(windowSetDefaultSize, "gtk_window_set_default_size", void,          (void*, int, int))\
            f(windowSetChild,       "gtk_window_set_child",        void,          (void*, void*))\
            f(windowPresent,        "gtk_window_present",          void,          (void*))\
            f(windowDestroy,        "gtk_window_destroy",          void,          (void*))\
            f(labelNew,             "gtk_label_new",               void*,         (const char*))\
            f(buttonNewWithLabel,   "gtk_button_new_with_label",   void*,         (const char*))\
            f(boxNew,               "gtk_box_new",                 void*,         (int, int))\
            f(boxAppend,            "gtk_box_append",              void,          (void*, void*))\
            f(widgetSetHalign,      "gtk_widget_set_halign",       void,          (void*, int))\
            f(widgetSetValign,      "gtk_widget_set_valign",       void,          (void*, int))\
            \
            /* GObject */\
            f(signalConnectData,    "g_signal_connect_data",       unsigned long, (void*, const char*, void(*)(), void*, void*, int))\
            \
            /* GLib */\
            f(mainContextIteration, "g_main_context_iteration",    int,           (void*, int))


        // struct

        struct GtkLibrary{

            void* gtk = nullptr;

            #define gtkField(name, symbol, ret, args) ret (*name) args;
            gtkFunctions(gtkField)
            #undef gtkField

            template<typename T>
            bool load(T& function, const char* name) {
                function = (T)dlsym(gtk, name);
                return function != nullptr;
            }

            GtkLibrary() {

                // this one dlopen() also loads the 2 dependencies
                gtk = dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_LOCAL);
                if(!gtk) return;

                #define gtkLoad(name, symbol, ret, args) && load(name, symbol)
                bool complete = true gtkFunctions(gtkLoad);
                #undef gtkLoad

                if(!complete) {
                    dlclose(gtk);
                    gtk = nullptr;
                }

            }

        };


        // load

        const GtkLibrary& gtk() {
            static GtkLibrary library;
            return library;
        }

        bool started() {
            static bool ok = gtk().initCheck() != 0;
            return ok;
        }


        // callbacks

        void* currentWindow = nullptr;

        struct Click{
            size_t widget;
            const ClickHandler* onClick;
        };

        void onDestroy(void*, void* closed) {
            *(bool*)closed = true;
            currentWindow = nullptr;
        }
        void onClicked(void*, void* data) {
            Click* click = (Click*)data;
            if(!(*click->onClick)(click->widget) && currentWindow) gtk().windowDestroy(currentWindow);
        }


        // show

        string show(const Window& window, const ClickHandler& onClick) {

            if(!gtk().gtk) return "The gui module requires GTK 4. Therefore, please install it.";
            if(!started()) return "Couldn't open a window, is there a display?";
            if(currentWindow) return "A window is already open.";

            bool closed = false;
            vector<Click> clicks(window.widgets.size());

            // add widgets
            void* box = gtk().boxNew(1, 25);
            for(size_t i = 0; i < window.widgets.size(); ++i) {
                const Widget& widget = window.widgets[i];

                switch(widget.type) {

                    case WIDGET_LABEL:
                        gtk().boxAppend(box, gtk().labelNew(widget.text.c_str()));
                        break;

                    case WIDGET_BUTTON:
                        void* button = gtk().buttonNewWithLabel(widget.text.c_str());
                        gtk().widgetSetHalign  (button, 3);
                        clicks[i] = {i, &onClick};
                        gtk().signalConnectData(button, "clicked", (void(*)())onClicked, &clicks[i], nullptr, 0);
                        gtk().boxAppend        (box, button);
                        break;

                }
                
            }
            gtk().widgetSetHalign(box, 3);
            gtk().widgetSetValign(box, 3);

            // make window
            void* gtkWindow = gtk().windowNew();
            gtk().windowSetTitle      (gtkWindow, window.title.c_str());
            gtk().windowSetDefaultSize(gtkWindow, window.width, window.height);
            gtk().windowSetChild      (gtkWindow, box);
            gtk().signalConnectData   (gtkWindow, "destroy", (void(*)())onDestroy, &closed, nullptr, 0);
            gtk().windowPresent       (gtkWindow);
            currentWindow = gtkWindow;

            // block execution until the window gets closed
            while(!closed) gtk().mainContextIteration(nullptr, true);

            // without this, the window might not close properly
            while(gtk().mainContextIteration(nullptr, false));

            return "";

        }

        void close() {
            if(currentWindow) gtk().windowDestroy(currentWindow);
        }


	// WIN32
	
	#elifdef CARO_GUI_WIN32
	
        // constants
        const int firstId      = 100;
        const int buttonWidth  = 120;
        const int buttonHeight = 28;
        const int spacing      = 25;     // between widgets

        // the window being shown
        HWND                currentWindow = nullptr;
        const Window*       currentLayout = nullptr;
        const ClickHandler* currentClick  = nullptr;

        // without this, you'd get the windows 3.1 font
        HFONT guiFont() {
            static HFONT font = []{
                NONCLIENTMETRICSW metrics = {sizeof(metrics)};
                if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
                    return CreateFontIndirectW(&metrics.lfMessageFont);
                }
                return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }();
            return font;
        }

        // utf-8 to utf-16
        std::wstring wide(const string& text) {
            int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            std::wstring result(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
            result.pop_back();
            return result;
        }

        // callback
        LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

            switch(message) {

                case WM_CREATE: {

                    for(size_t i = 0; i < currentLayout->widgets.size(); ++i) {

                        const Widget& widget = currentLayout->widgets[i];

                        HWND control;
                        
                        switch(widget.type) {
                            
                            case WIDGET_LABEL:
                                CreateWindowExW(
                                    0,
                                    L"STATIC",
                                    wide(widget.text).c_str(),
                                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                            case WIDGET_BUTTON:
                                CreateWindowExW(
                                    0,
                                    L"BUTTON",
                                    wide(widget.text).c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                        }

                        SendMessageW(control, WM_SETFONT, (WPARAM)guiFont(), true);

                    }

                    return 0;

                }

                case WM_SIZE: {

                    int width  = LOWORD(lParam);
                    int height = HIWORD(lParam);
                    size_t count = currentLayout->widgets.size();

                    // measure every widget
                    vector<int> heights(count);
                    for(size_t i = 0; i < count; ++i) {

                        if(currentLayout->widgets[i].type == WIDGET_BUTTON) {
                            heights[i] = buttonHeight;
                            continue;
                        }

                        HWND label = GetDlgItem(hwnd, firstId + i);
                        std::wstring text = wide(currentLayout->widgets[i].text);

                        HDC     dc  = GetDC(label);
                        HGDIOBJ old = SelectObject(dc, guiFont());
                        RECT    box = {0, 0, width, 0};
                        DrawTextW(dc, text.c_str(), -1, &box, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
                        SelectObject(dc, old);
                        ReleaseDC(label, dc);

                        heights[i] = box.bottom;

                    }

                    // stack them in the middle
                    int stack = 0;
                    for(int h: heights) stack += h;
                    if(count > 0) stack += spacing * (int)(count - 1);

                    int top = (height - stack) / 2;
                    for(size_t i = 0; i < count; ++i) {
                        HWND control = GetDlgItem(hwnd, firstId + i);
                        if(currentLayout->widgets[i].type == WIDGET_BUTTON) {
                            MoveWindow(control, (width - buttonWidth) / 2, top, buttonWidth, buttonHeight, true);
                        } else {
                            MoveWindow(control, 0, top, width, heights[i], true);
                        }
                        top += heights[i] + spacing;
                    }

                    return 0;

                }

                case WM_COMMAND: {
                    int id = LOWORD(wParam) - firstId;
                    if(id >= 0 && id < (int)currentLayout->widgets.size() && currentLayout->widgets[id].type == WIDGET_BUTTON) {
                        if(!(*currentClick)(id) && currentWindow) DestroyWindow(currentWindow);
                        return 0;
                    }
                    break;
                }

                case WM_DESTROY:
                    currentWindow = nullptr;
                    PostQuitMessage(0);
                    return 0;

            }

            return DefWindowProcW(hwnd, message, wParam, lParam);

        }
        
		string show(const Window& window, const ClickHandler& onClick) {

            if(currentWindow) return "A window is already open.";

            // make class
            const wchar_t CLASS_NAME[]  = L"Carotene window class";
            HINSTANCE hInstance = GetModuleHandleW(nullptr);
            WNDCLASSW wc = {};
            wc.lpfnWndProc   = windowProc;
            wc.hInstance     = hInstance;
            wc.lpszClassName = CLASS_NAME;
            wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);    // without this the client area is never erased

            // register class
            static bool registered = false;
            if(!registered) {
                if(!RegisterClassW(&wc)) return "Couldn't register the window class.";
                registered = true;
            }

            currentLayout = &window;
            currentClick  = &onClick;

            // make window
            HWND hwnd = CreateWindowExW(

                0,                             // Optional window styles.
                CLASS_NAME,                    // Window class
                wide(window.title).c_str(),    // Window text
                WS_OVERLAPPEDWINDOW,           // Window style

                CW_USEDEFAULT, CW_USEDEFAULT, window.width, window.height,    // Size and position

                nullptr,                       // Parent window    
                nullptr,                       // Menu
                hInstance,                     // Instance handle
                nullptr                        // Additional application data
                
            );
            if(hwnd == NULL) return "Couldn't make window.";
            currentWindow = hwnd;

            // show window
            ShowWindow(hwnd, SW_SHOWNORMAL);

            // block execution
            MSG message;
            while(GetMessageW(&message, nullptr, 0, 0) > 0) {
                if(IsDialogMessageW(hwnd, &message)) continue;    // so buttons can be pressed with the keyboard too
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            return "";

            // based on https://learn.microsoft.com/en-us/windows/win32/learnwin32/creating-a-window
            
        }
		
        void close() {
            if(currentWindow) DestroyWindow(currentWindow);
        }

	
	// BEAPI
	
	#elifdef CARO_GUI_BEAPI
	
        const ClickHandler* currentClick = nullptr;

		class CaroWindow: public BWindow{
	
			public:
			
				CaroWindow(const Window& window):

                    // make window
					BWindow(
						BRect(0, 0, window.width, window.height),    // gets centered later
						window.title.c_str(),
						B_TITLED_WINDOW,
						B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS
					)

                    // add widgets
					{

						BLayoutBuilder::Group<> builder(this, B_VERTICAL, B_USE_DEFAULT_SPACING);

						builder.SetInsets(B_USE_WINDOW_INSETS).AddGlue();

						for(size_t i = 0; i < window.widgets.size(); ++i) {

							const Widget& widget = window.widgets[i];
							BView* view;

							switch(widget.type) {

                                case WIDGET_LABEL:
                                    view = new BStringView("label", widget.text.c_str());
                                    break;
                                
                                case WIDGET_BUTTON:
                                    BMessage* message = new BMessage('clik');
                                    message->AddInt32("widget", (int32)i);
                                    view = new BButton("button", widget.text.c_str(), message);
                                    break;

							}

							builder.AddGroup(B_HORIZONTAL)
								.AddGlue()
								.Add(view)
								.AddGlue()
							.End();

						}

						builder.AddGlue();
                            // the glues make everything centered

						Layout(true);
						CenterOnScreen();
					}
				
				void MessageReceived(BMessage* message) override{
					switch (message->what) {
						case 'clik':
							if(!(*currentClick)(message->GetInt32("widget", 0))) be_app->PostMessage(B_QUIT_REQUESTED);
							break;
						default:
							BWindow::MessageReceived(message);
					}
				}
				
		};

		string show(const Window& window, const ClickHandler& onClick) {
            if(be_app) return "A window is already open.";
			BApplication app("application/x-vnd.Carotene-Window");
            currentClick = &onClick;
			(new CaroWindow(window))->Show();
			app.Run();
			return "";
		}
		
        void close() {
            if(be_app) be_app->PostMessage(B_QUIT_REQUESTED);
        }

        // the haiku convention is to use 4 char chars for ints, which seems kinda funny but we'll follow it

        
    #endif


}
