
#pragma once

/*

    There are 3 separate backends:
      - Windows:               Win32, linked normally
      - macOS, Linux, FreeBSD: GTK 4, with dlopen()
      - Haiku:                 BeAPI, linked normally
    
    Maybe Cocoa and Qt later.

*/


// INCLUDES

#if defined(_WIN32)
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


namespace CaroGui{


    // BACKENDS


    // gtk

    #ifdef CARO_GUI_GTK


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

        void onDestroy(void*, void* closed)  { *(bool*)closed = true;     }
        void onClicked(void*, void* window)  { gtk().windowDestroy(window); }


        // test

        string test() {

            if(!gtk().gtk) return "The gui module requires GTK 4. Therefore, please install it.";
            if(!started()) return "Couldn't open a window, is there a display?";

            bool closed = false;

            // add button
            void* button = gtk().buttonNewWithLabel("Self-destruct");
            gtk().widgetSetHalign(button, 3);

            // add box and label
            void* box = gtk().boxNew(1, 25);
            gtk().boxAppend      (box, gtk().labelNew("Hello! This is a test window rendered using GTK 4. Pretty cool!"));
            gtk().boxAppend      (box, button);
            gtk().widgetSetHalign(box, 3);
            gtk().widgetSetValign(box, 3);

            // make window
            void* window = gtk().windowNew();
            gtk().windowSetTitle      (window, "Carotene test window");
            gtk().windowSetDefaultSize(window, 400, 250);
            gtk().windowSetChild      (window, box);
            gtk().signalConnectData   (window, "destroy", (void(*)())onDestroy, &closed, nullptr, 0);
            gtk().signalConnectData   (button, "clicked", (void(*)())onClicked, window,  nullptr, 0);
            gtk().windowPresent       (window);

            // block execution until the window gets closed
            while(!closed) gtk().mainContextIteration(nullptr, true);

            // without this, the window might not close properly
            while(gtk().mainContextIteration(nullptr, false));

            return "";

        }


	// win32
	
	#elifdef CARO_GUI_WIN32
	
        // constants
        const int labelId  = 100;
        const int buttonId = 101;
        const int buttonWidth  = 120;
        const int buttonHeight = 28;
        const int spacing      = 25;    // between the label and the button

        // without this, you get the windows 3.1 font
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

        // callback
        LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

            switch(message) {

                case WM_CREATE: {

                    HWND label = CreateWindowExW(
                        0,
                        L"STATIC",
                        L"Hello! This is a test window rendered using Win32. Pretty NOT cool, this API is a mess, I can see why Microsoft is constantly trying to replace it!",
                        WS_CHILD | WS_VISIBLE | SS_CENTER,
                        0, 0, 0, 0,
                        hwnd,
                        (HMENU)labelId,
                        (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                        nullptr
                    );
                    SendMessageW(label, WM_SETFONT, (WPARAM)guiFont(), true);

                    HWND button = CreateWindowExW(
                        0,
                        L"BUTTON",
                        L"Self-destruct",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        0, 0, 0, 0,
                        hwnd,
                        (HMENU)buttonId,
                        (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                        nullptr
                    );
                    SendMessageW(button, WM_SETFONT, (WPARAM)guiFont(), true);

                    return 0;

                }

                case WM_SIZE: {

                    HWND label  = GetDlgItem(hwnd, labelId);
                    HWND button = GetDlgItem(hwnd, buttonId);
                    int  width  = LOWORD(lParam);
                    int  height = HIWORD(lParam);

                    wchar_t text[256];
                    GetWindowTextW(label, text, 256);

                    HDC     dc  = GetDC(label);
                    HGDIOBJ old = SelectObject(dc, guiFont());
                    RECT    box = {0, 0, width, 0};
                    DrawTextW(dc, text, -1, &box, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
                    SelectObject(dc, old);
                    ReleaseDC(label, dc);

                    int stack = box.bottom + spacing + buttonHeight;
                    int top   = (height - stack) / 2;

                    MoveWindow(label,  0,                          top,                          width,       box.bottom,   true);
                    MoveWindow(button, (width - buttonWidth) / 2,  top + box.bottom + spacing,   buttonWidth, buttonHeight, true);

                    return 0;

                }

                case WM_COMMAND:
                    if(LOWORD(wParam) == buttonId) {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    break;

                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;

            }

            return DefWindowProcW(hwnd, message, wParam, lParam);

        }
        
		string test() {

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

            // make window
            HWND hwnd = CreateWindowExW(

                0,                                         // Optional window styles.
                CLASS_NAME,                                // Window class
                L"Carotene test window",                   // Window text
                WS_OVERLAPPEDWINDOW,                       // Window style

                CW_USEDEFAULT, CW_USEDEFAULT, 450, 250,    // Size and position

                nullptr,                                   // Parent window    
                nullptr,                                   // Menu
                hInstance,                                 // Instance handle
                nullptr                                    // Additional application data
                
            );
            if(hwnd == NULL) return "Couldn't make window.";

            // show window
            ShowWindow(hwnd, SW_SHOWNORMAL);

            // block execution
            MSG message;
            while(GetMessageW(&message, nullptr, 0, 0) > 0) {
                if(IsDialogMessageW(hwnd, &message)) continue;    // so the button can be pressed with the keyboard too
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            return "";

            // based on https://learn.microsoft.com/en-us/windows/win32/learnwin32/creating-a-window
            
        }
		
	
	// beapi
	
	#elifdef CARO_GUI_BEAPI
	
		class CaroWindow: public BWindow{
	
			public:
			
				CaroWindow():
					BWindow(
						BRect(0, 0, 400, 250),    // gets centered later
						"Carotene test window",
						B_TITLED_WINDOW,
						B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS
					)
					{
						BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
							.SetInsets(B_USE_WINDOW_INSETS)
							.AddGlue()
							.AddGroup(B_HORIZONTAL)
								.AddGlue()
								.Add(new BStringView("label", "Hello! This is a test window rendered using BeAPI. Pretty cool!"))
								.AddGlue()
							.End()
							.AddGroup(B_HORIZONTAL)
								.AddGlue()
								.Add(new BButton("button", "Self-destruct", new BMessage('sfdt')))
								.AddGlue()
							.End()
							.AddGlue();
								// the glues make everything centered
						Layout(true);
						CenterOnScreen();
					}
				
				void MessageReceived(BMessage* message) override{
					switch (message->what) {
						case 'sfdt':
							be_app->PostMessage(B_QUIT_REQUESTED);
							break;
						default:
							BWindow::MessageReceived(message);
					}
				}
				
		};

		string test() {
			BApplication app("application/x-vnd.Carotene-WindowTest");
			(new CaroWindow())->Show();
			app.Run();
			return "";
		}
		
    #endif


}
